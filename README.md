[![Build Status](https://dev.azure.com/davideg94/davideg_94/_apis/build/status/DavideAG.Understanding-containers?branchName=master)](https://dev.azure.com/davideg94/davideg_94/_build/latest?definitionId=1&branchName=master)
[![CodeFactor](https://www.codefactor.io/repository/github/davideag/understanding-containers/badge)](https://www.codefactor.io/repository/github/davideag/understanding-containers)
# Understanding containers

Nowadays light virtualization is a weapon used by many.
[Microservice based architecture](https://ieeexplore.ieee.org/abstract/document/7943959)
is increasingly used and containers are the backbone of it.

Software like [Docker](https://www.docker.com/) allow us to manage containers
in a simple way but: how are they made?
What features are really necessary for a process to be a container manager?
It's time to get your hands dirty and make our homemade container.

The questions we're gonna try to resolve are:

 - How container engine really works?
 - How are containers created by "facilitators" like Docker?

First of all we have to start with some tools that we will use.
Below is a brief summary of the tools (present in Linux) that we
will use to build a container:

- [Namespaces](#namespaces)
- [Pivot_root](#Pivot_root)
- [Cgroups](#Cgroups)

## [Namespaces](https://en.wikipedia.org/wiki/Linux_namespaces)
Namespaces are a feature of the Linux kernel that partitions kernel resources such
that one set of processes sees one set of resources while another set of processes
sees a different set of resources.
So, thanks by feature, we can limit what the "process can see". We will set up
namespaces using Linux kernel syscalls.
The [namespaces](http://man7.org/linux/man-pages/man7/namespaces.7.html) man page
tells us there are 3 system calls that make up the API:
- [clone](http://man7.org/linux/man-pages/man2/clone.2.html)
- [setns](http://man7.org/linux/man-pages/man2/setns.2.html)
- [unshare](http://man7.org/linux/man-pages/man2/unshare.2.html)

## [Pivot_root](http://man7.org/linux/man-pages/man2/pivot_root.2.html)
pivot_root is a Linux API that changes the root mount in the mount namespace of the
calling process.

## Cgroups
Cgroups (control groups) is a Linux kernel feature that limits, accounts
for, and isolates the resource usage (CPU, memory, disk I/O, network, etc...)
of a collection of processes. Cgroups support has not yet been implemented in this
repository. Work is still in progress

## Dependencies
In order to compile this tool is necessary to install the following libraries:
```bash
~$  sudo apt install libcap-dev seccomp-dev -y
```

## Usage
To create your homemade container you will need to compile the source code in
the `"src"` directory but first you have to use netsetgo_install.sh inside the
tools folder. I personally recommend using [cmake](https://cmake.org/)
to do this. Move to the `Understanding-containers` directory and then run the
following commands

```bash
~$  cd tools && sudo ./netsetgo_install.sh && cd ..
~$  mkdir build && cd build
~$  cmake .. && make -j $(getconf _NPROCESSORS_ONLN)
```

Now in the build folder you'll have the executable ready to run!
Here the help of the tool:
```bash
~$ sudo ./MyDocker -h
Usage: sudo ./MyDocker <options> <entrypoint>

<options> should be:
	- a	run all namespaces
	- c	cgrops used to limit resources.
		This command must be chained with at least one of:
		- M <memory_limit> 				[1-4294967296]	default: 1073741824 (1GB)
		- C <percentage_of_cpu_shares> 			[1-100]			default: 25
		- P <max_pids> 					[10-32768]		default: 64
		- I <io_weighht> 				[10-1000]		default: 10
```
Feel the thrill of your new container now by running. An example of a command can be:

```bash
~$  sudo ./MyDocker -ac -C 50 -I 20 -P 333 /bin/bash
```

In this case the following cgroup resource limits are applied:

<center>

| Resource | Applied value |
|---|---|
| memory_limit | 1GB |
| cpu_shares | 50 |
| max_pids | 333 |
| io_weight | 20 |

</center>

Now you'll be running bash inside your container.
You can, for example, control the processes that are active inside it and
notice how these are different from those of the host machine.

When you want you can finish your container killing the process of his bash `exit`

## Tree of the directors of this repository
The folders in this repository are:

	├── root_fs
	├── src
	│ 	├── helpers
	│ 	└── namespaces
	│ 	    ├── mount
	│ 	    ├── network
	│ 	    └── user
	└── tools

 - root_fs	[the root filesystem where your container will run]
 - [src](https://github.com/DavideAG/Understanding-containers/tree/master/src)	[the source folder]
 - [helpers](https://github.com/DavideAG/Understanding-containers/tree/master/src/helpers)	[helpers files]
 - [namespaces](https://github.com/DavideAG/Understanding-containers/tree/master/src/namespaces) [support for various namespaces]
 - [mount](https://github.com/DavideAG/Understanding-containers/tree/master/src/namespaces/mount)	[mount namespace reference folder]
 - [network](https://github.com/DavideAG/Understanding-containers/tree/master/src/namespaces/network)	[network namespace reference folder]
 - [user](https://github.com/DavideAG/Understanding-containers/tree/master/src/namespaces/user)	[user namespace reference folder]
 - [tools](https://github.com/DavideAG/Understanding-containers/tree/master/tools)	[tools folders]

Each folder (except root_fs) contains a more detailed instruction file called
README.md
the tools folder contains a binary file and scripts that will allow your
namespace to connect to the internet. to support this you need a CNI, one
of the scripts provided will allow you to use [`Polycube`](https://github.com/polycube-network/polycube) to do this, so
you can also take advantage of ebpf technology!

## Let's get started!
If you want to better understand how namespaces work then you need to explore
the `src` folder. Discover the README.md files inside the various folders,
they will guide you in understanding the realization of the `containers`.
I suggest you also take a look at the source files because they are full of
useful comments that will help you understand how virtualization works.

## Suggestions, corrections and feedback

Please report any issues, corrections or ideas on [GitHub](https://github.com/DavideAG/Understanding-containers/issues)

## Pointers
- [Creating Containers](http://crosbymichael.com/creating-containers-part-1.html) of [Michael Crosby](https://github.com/crosbymichael)
- [container](https://github.com/hoanhan101/container) of [Hoanh An](https://github.com/hoanhan101)
- [Containers From Scratch](https://www.youtube.com/watch?time_continue=9&v=8fi7uSYlOdc&feature=emb_logo) of [Liz Rice](https://www.lizrice.com/)
- [nsroot](https://github.com/uit-no/nsroot) & [nsroot-paper](https://arxiv.org/ftp/arxiv/papers/1609/1609.03750.pdf) of [Inge Alexander Raknes, Bjørn Fjukstad, Lars Ailo Bongo - UiT The Arctic University of Norway](https://en.uit.no/startsida)
- [Container Specification - runc](https://github.com/opencontainers/runc/blob/4932620b6237ed2a91aa5b5ca8cca6a73c10311b/libcontainer/SPEC.md)
- [Linux Namespaces](https://medium.com/@teddyking/linux-namespaces-850489d3ccf) - [GitHub repository](https://github.com/teddyking/ns-process) of [Ed king](https://github.com/teddyking)
- [Docker and Go: why did we decide to write Docker in Go?](https://www.slideshare.net/jpetazzo/docker-and-go-why-did-we-decide-to-write-docker-in-go) of [Google Developers Group meetup at Google West Campus 2 (Michael Crosby)](https://github.com/crosbymichael)
- [USER_NAMESPACES](http://man7.org/linux/man-pages/man7/user_namespaces.7.html) of [Linux Programmer's Manual](http://man7.org/index.html)
- [What UID and GID are](https://geek-university.com/linux/uid-user-identifier-gid-group-identifier/)
- [pivot_root new documentation](https://lwn.net/Articles/800381/) of [LWN.net](https://lwn.net/)
- [Introducing Linux Network Namespaces (04-09-2013)](https://blog.scottlowe.org/2013/09/04/introducing-linux-network-namespaces/)
- [Virtual Ethernet Device](http://man7.org/linux/man-pages/man4/veth.4.html) of [Linux Programmer's Manual](http://man7.org/index.html)
- [A deep dive into Linux namespaces](http://ifeanyi.co/posts/linux-namespaces-part-1/#pivot-root)
- [netsetgo](https://github.com/teddyking/netsetgo) of [Ed King](https://github.com/teddyking)
- [ns-process](https://github.com/teddyking/ns-process) of [Ed King](https://github.com/teddyking)
- [Linux containers in 500 lines of code](https://blog.lizzie.io/linux-containers-in-500-loc.html) of [Lizzie Dixon](https://github.com/startling)
