#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/sched/signal.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include "monitor_ioctl.h"
#define DEVICE_NAME "container_monitor"
MODULE_LICENSE("GPL");
struct monitor_entry {
pid_t pid; char
container_id[32];
unsigned long soft_limit;
unsigned long hard_limit;
int soft_triggered; struct
list_head list;
};
static LIST_HEAD(monitor_list);static
DEFINE_MUTEX(monitor_lock);
static dev_t dev_num;static
struct cdev c_dev; static
struct class *cl;
/* ===== WORKQUEUE ===== */
static struct workqueue_struct *monitor_wq; static
struct delayed_work monitor_work;
/* ===== RSS ===== */ static
long get_rss_bytes(pid_t pid)
{
struct task_struct *task;
struct mm_struct *mm; long
rss_pages = 0;
rcu_read_lock();
task = pid_task(find_vpid(pid), PIDTYPE_PID);
if (!task) { rcu_read_unlock(); return -1;
}
get_task_struct(task);
rcu_read_unlock();
mm = get_task_mm(task);
if (mm) {
rss_pages = get_mm_rss(mm);
mmput(mm);
}
put_task_struct(task);
return rss_pages * PAGE_SIZE;
}
/* ===== LOGGING ===== */
static void log_soft(const char *id, pid_t pid,
unsigned long limit, long rss)
{
printk(KERN_WARNING "[monitor] SOFT %s pid=%d rss=%ld limit=%lu\n",
id, pid, rss, limit);
}
static void kill_proc(const char *id, pid_t pid,
unsigned long limit, long rss)
{
struct task_struct *task;
rcu_read_lock();
task = pid_task(find_vpid(pid), PIDTYPE_PID);
if (task)
send_sig(SIGKILL, task, 1);
rcu_read_unlock();
printk(KERN_WARNING "[monitor] HARD %s pid=%d rss=%ld limit=%lu\n",
id, pid, rss, limit);
}
/* ===== MONITOR LOOP ===== */
static void monitor_fn(struct work_struct *work)
{
struct monitor_entry *entry, *tmp;
mutex_lock(&monitor_lock);
list_for_each_entry_safe(entry, tmp, &monitor_list, list) {
long rss = get_rss_bytes(entry->pid);
if (rss < 0) {
list_del(&entry->list);
kfree(entry);
continue;
}
if (rss > entry->soft_limit && !entry->soft_triggered) {
log_soft(entry->container_id, entry->pid, entry-
>soft_limit, rss);
entry->soft_triggered = 1;
}
if (rss > entry->hard_limit) {
kill_proc(entry->container_id, entry->pid,
entry->hard_limit, rss); list_del(&entry-
>list); kfree(entry);
}
}
mutex_unlock(&monitor_lock);
queue_delayed_work(monitor_wq, &monitor_work, msecs_to_jiffies(1000)); }
/* ===== IOCTL ===== */
static long monitor_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
struct monitor_request req;
struct monitor_entry *entry, *tmp;
if (copy_from_user(&req, (void user *)arg, sizeof(req)))
return -EFAULT;
if (cmd == MONITOR_REGISTER) {
if (req.soft_limit_bytes > req.hard_limit_bytes)
return -EINVAL;
entry = kmalloc(sizeof(*entry), GFP_KERNEL); if
(!entry)
return -ENOMEM;
entry->pid = req.pid;
strncpy(entry->container_id, req.container_id, 31);
entry->container_id[31] = '\0'; entry-
>soft_limit = req.soft_limit_bytes; entry-
>hard_limit = req.hard_limit_bytes; entry-
>soft_triggered = 0;
mutex_lock(&monitor_lock); list_add(&entry-
>list, &monitor_list);
mutex_unlock(&monitor_lock);
printk(KERN_INFO "[monitor] registered %s pid=%d\n", entry-
>container_id, entry->pid);
return
0;
}
if (cmd == MONITOR_UNREGISTER) {
mutex_lock(&monitor_lock);
list_for_each_entry_safe(entry, tmp, &monitor_list, list) {
if (entry->pid == req.pid) { list_del(&entry->list);
kfree(entry); mutex_unlock(&monitor_lock);
return 0;
}
}
mutex_unlock(&monitor_lock);
return -ENOENT;
}
return -EINVAL;
}
/* ===== FILE OPS ===== */
static struct file_operations fops = {
.owner = THIS_MODULE,
.unlocked_ioctl = monitor_ioctl,
};
/* ===== INIT ===== */ static
int init monitor_init(void)
{
if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
return -1;
cl = class_create(DEVICE_NAME); // NEW API (kernel 6.x)
device_create(cl, NULL, dev_num, NULL, DEVICE_NAME);
cdev_init(&c_dev, &fops);
cdev_add(&c_dev, dev_num, 1);
monitor_wq = create_singlethread_workqueue("monitor_wq");
INIT_DELAYED_WORK(&monitor_work, monitor_fn);
queue_delayed_work(monitor_wq, &monitor_work, msecs_to_jiffies(1000));
printk(KERN_INFO "[monitor] loaded\n");
return 0;
}
/* ===== EXIT ===== */
static void exit monitor_exit(void)
{
struct monitor_entry *entry, *tmp;
cancel_delayed_work_sync(&monitor_work);
destroy_workqueue(monitor_wq);
mutex_lock(&monitor_lock);
list_for_each_entry_safe(entry, tmp, &monitor_list, list) {
list_del(&entry->list); kfree(entry);
}
mutex_unlock(&monitor_lock);
cdev_del(&c_dev);
device_destroy(cl, dev_num);
class_destroy(cl);
unregister_chrdev_region(dev_num, 1);
printk(KERN_INFO "[monitor] unloaded\n");
}
module_init(monitor_init); module_exit(monitor_exit);
