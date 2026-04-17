# OS Jackfruit – Mini Container Runtime

## Overview

This project implements a lightweight container runtime in C along with a custom Linux kernel module for monitoring and enforcing memory limits. It demonstrates key operating system concepts such as process isolation, kernel-user communication, memory management, and CPU scheduling.

The runtime creates containers using Linux namespaces, while the kernel module tracks memory usage and enforces soft and hard limits. The system also includes experiments to study CPU scheduling using different nice values.

---

## Features

- Process isolation using Linux namespaces:
  - PID namespace  
  - UTS namespace  
  - Mount namespace  

- User-space and kernel-space communication using ioctl  
- Memory monitoring using Resident Set Size (RSS)  
- Soft limit enforcement with warning messages  
- Hard limit enforcement with process termination  
- CPU scheduling experiment using nice values  
- Proper cleanup with no zombie processes  

---

## Project Structure
OS--Jackfruit/

├── boilerplate/

│ ├── engine.c

│ ├── monitor.c

│ ├── monitor_ioctl.h

│ ├── memory_hog.c

│ ├── cpu_hog.c

│ ├── io_pulse.c

│ ├── rootfs-alpha/

│ ├── rootfs-beta/

│ └── rootfs-base/

└── README.md


---

## Build Instructions

Run the following commands inside the `boilerplate` directory:

```bash
make clean
make
```

## Running the System (Demonstration)

This section shows how the container runtime and kernel module are executed along with expected outputs.

---

### 1. Load Kernel Module

```bash
sudo insmod monitor.ko
```
This loads the kernel module and creates the device:
/dev/container_monitor

### 2. Start Supervisor
```bash
sudo ./engine supervisor ./rootfs-base
```
### 3. Start a Container
```bash
sudo ./engine start alpha ./rootfs-alpha /memory_hog
```
### 4. List Running Containers
``` bash
sudo ./engine ps
```
### 5. View Logs
``` bash
dmesg -w
```
### 6. Stop a Container
``` bash
sudo ./engine stop alpha
```
### 7. Scheduling Experiment
``` bash
sudo ./engine start c1 ./rootfs-alpha /cpu_hog --nice 10
sudo ./engine start c2 ./rootfs-beta /cpu_hog --nice -5
ps -eo pid,ni,%cpu,comm | grep cpu_hog
```
### 8. Cleanup and Verification
``` bash
ps aux | grep defunct

ps aux | grep defunct
```
## Contributors
Jyeshta J

Manapriya K
