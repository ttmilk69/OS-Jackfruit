#ifndef MONITOR_IOCTL_H
#define MONITOR_IOCTL_H
#include <linux/ioctl.h>
#define MONITOR_REGISTER _IOW('a','a', struct monitor_request) #define
MONITOR_UNREGISTER _IOW('a','b', struct monitor_request)
struct monitor_request {
int pid;
unsigned long soft_limit_bytes;
unsigned long hard_limit_bytes; char
container_id[32];
};
#endif
