# Understanding-containers

Nowadays light virtualization is a weapon used by many. Microservice based architecture is increasingly used and containers are the backbone of it.

Software like docker allow us to manage containers in a simple way but: how are they made? What features are really necessary for a process to be a container manager? It's time to get your hands dirty and make our homemade container.

The questions we're gonna try to resolve are:

 - How container engine really works?
 - How are containers created by "facilitators" like Docker?

First of all we have to start with some tools that we will use. Below is a brief summary of the tools (present in Linux) that we will use to build a container:

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

## Usage
To create your homemade container you will need to compile the source code in the `"src"` directory. I personally recommend using [cmake](https://cmake.org/) to do this. Move to the `Understanding-containers` directory and then run
```bash
~$  mkdir build && cd build && cmake .. && make -j $(getconf _NPROCESSORS_ONLN)
```
Now in the build folder you'll have the executable ready to run! Feel the thrill of your new container now by running
```bash
~$  sudo .MyDocker run /bin/bash
```
Now you can see your container directly from the inside. You can, for example, control the processes that are active inside it and notice how these are different from those of the host machine
```bash
~$  ps fxa
```
When you want you can finish your container killing the process of his bash `exit`

## Suggestions, corrections and feedback

Please report any issues, corrections or ideas on [GitHub](https://github.com/DavideAG/Understanding-containers/issues)

## Pointers
- [Creating Containers](http://crosbymichael.com/creating-containers-part-1.html) of [Michael Crosby](https://github.com/crosbymichael)
- [container](https://github.com/hoanhan101/container) of [Hoanh An](https://github.com/hoanhan101)
- [Containers From Scratch](https://www.youtube.com/watch?time_continue=9&v=8fi7uSYlOdc&feature=emb_logo) of [Liz Rice](https://www.lizrice.com/)
- [nsroot](https://github.com/uit-no/nsroot) & [nsroot-paper](https://arxiv.org/ftp/arxiv/papers/1609/1609.03750.pdf) of [Inge Alexander Raknes, Bjørn Fjukstad, Lars Ailo Bongo - UiT The Arctic University of Norway](https://en.uit.no/startsida)
- [Container Specification - runc](https://github.com/opencontainers/runc/blob/4932620b6237ed2a91aa5b5ca8cca6a73c10311b/libcontainer/SPEC.md)
