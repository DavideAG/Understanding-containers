# Understanding-containers

This repository is for a personal study about containers.
I will guide you through a simple implementation in C.

Main questions are:

 - How do they really work?
 - How are containers created by "facilitators" like Docker?

Below is a brief summary of the tools (present in Linux) that we will use to build a container:

- [Namespaces](#namespaces)
- [Chroot](#chroot)
- [Cgroups](#cgroups)

## Namespaces
Namespaces are a feature of the Linux kernel that partitions kernel resources such that one set of processes sees one set of resources while another set of processes sees a different set of resources.
So, thanks by feature, we can limit what the "process can see". We will set up namespaces using Linux kernel syscalls.

## Chroot
Chroot is an operation that changes the apparent root directory for the current running process and its cildren. The modified environment is called a **chroot jail**.

## Cgroups
Cgroups (control groups) is a Linux kernel feature that limits, accounts for, and isolates the resource usage (CPU, memory, disk I/O, network, etc...) of a collection of processes.
