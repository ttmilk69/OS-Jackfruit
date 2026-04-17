#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "monitor_ioctl.h"
#define STACK_SIZE (1024 * 1024)
#define SOCKET_PATH "/tmp/mini_runtime.sock"
typedefstruct { char
id[32]; char
rootfs[256]; char
command[256];
} request_t;
typedef struct {
char id[32]; pid_t
pid;
} container_t;
container_t containers[20]; int
container_count = 0;
char stack[STACK_SIZE];
int child_fn(void *arg) { request_t
*req = (request_t *)arg;
sethostname("container", 9);
mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
if (chroot(req->rootfs) != 0) {
perror("chroot"); return 1;
}
chdir("/");
mkdir("/proc", 0555); mount("proc",
"/proc", "proc", 0, NULL); if (strstr(req-
>command, "cpu_hog")) { nice(-5); //
high priority
}
execlp(req->command, req->command, NULL);
perror("exec"); return
1;
}
void run_supervisor() { int
server_fd, client_fd; struct
sockaddr_un addr;
int monitor_fd = open("/dev/container_monitor", O_RDWR);
if (monitor_fd < 0) { perror("monitor open");
}
unlink(SOCKET_PATH);
server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
addr.sun_family = AF_UNIX; strcpy(addr.sun_path,
SOCKET_PATH);
bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)); listen(server_fd,
5);
printf("[supervisor] running...\n");
while (1) {
client_fd = accept(server_fd, NULL, NULL);
char buffer[256] = {0};
read(client_fd, buffer,sizeof(buffer));
/* START */
if (strncmp(buffer, "start", 5) == 0) {
request_t req;
sscanf(buffer, "start %s %s %s", req.id, req.rootfs, req.command);
pid_t pid = clone(child_fn,
stack + STACK_SIZE,
CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD,
&req);
containers[container_count].pid = pid;
strcpy(containers[container_count].id, req.id); container_count++;
/* REGISTER WITH MONITOR */
struct monitor_request mreq;
memset(&mreq, 0,sizeof(mreq));
mreq.pid = pid;
mreq.soft_limit_bytes = 40 * 1024 * 1024;
mreq.hard_limit_bytes = 64 * 1024 * 1024;
strncpy(mreq.container_id, req.id, 31);
if (monitor_fd >= 0) {
ioctl(monitor_fd, MONITOR_REGISTER, &mreq);
}
write(client_fd, "started", 7);
}
/* PS */
else if (strncmp(buffer, "ps", 2) == 0) {
char out[512] = "";
for (int i = 0; i < container_count; i++) {
char line[128];
int status;
pid_t result = waitpid(containers[i].pid, &status, WNOHANG);
if (result == 0) {
snprintf(line, sizeof(line), "%s : PID %d (running)\n",
containers[i].id, containers[i].pid);
} else {
snprintf(line, sizeof(line), "%s : PID %d (killed)\n",
containers[i].id, containers[i].pid);
}
strcat(out, line);
}
write(client_fd, out, strlen(out));
}
/* STOP */
else if (strncmp(buffer, "stop", 4) == 0) {
char id[32]; sscanf(buffer, "stop %s",
id);
for (int i = 0; i < container_count; i++) {
if (strcmp(containers[i].id, id) == 0) {
kill(containers[i].pid, SIGTERM);
struct monitor_request mreq;
memset(&mreq, 0, sizeof(mreq)); mreq.pid
= containers[i].pid;
strncpy(mreq.container_id, id, 31);
if (monitor_fd >= 0) {
ioctl(monitor_fd, MONITOR_UNREGISTER, &mreq);
}
}
}
write(client_fd, "stopped", 7);
}
close(client_fd);
}
}
void send_command(int argc, char *argv[]) {
int sock; struct sockaddr_un addr;
sock = socket(AF_UNIX, SOCK_STREAM, 0);
addr.sun_family = AF_UNIX; strcpy(addr.sun_path,
SOCKET_PATH);
connect(sock, (struct sockaddr *)&addr, sizeof(addr));
char cmd[256] = ""; for (int
i = 1; i < argc; i++) {
strcat(cmd, argv[i]);
strcat(cmd, " ");
}
write(sock, cmd, strlen(cmd));
char buffer[512] = {0}; read(sock,
buffer, sizeof(buffer));
printf("%s\n", buffer);
close(sock);
}
int main(int argc, char *argv[]) {
if (argc < 2) { printf("Usage:\n"); printf("
engine supervisor <rootfs>\n"); printf(" engine
start <id> <rootfs> <cmd>\n"); printf(" engine
ps\n"); printf(" engine stop <id>\n"); return 1;
}
if (strcmp(argv[1], "supervisor") == 0) {
run_supervisor(); } else {
send_command(argc, argv);
}
return 0;
}
