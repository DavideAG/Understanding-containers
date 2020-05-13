#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "mount.h"
#include "../../helpers/helpers.h"
#include "../../../config.h"

/*
* Oh no glibc for pivot_root! We will use syscall.
* http://man7.org/linux/man-pages/man2/pivot_root.2.html
* 'newroot' (the first argument) is the path to the desired
* new root filesystem and 'putold' is a path to a directory
* in which to move the current root.
* The process of preparing a newroot filesystem can be quite
* a detailed and complex one. Take for example Docker’s
* layered filesystem approach in which many filesystem “layers”
* are joined together to present a single coherent root.
* We’re going to do something much simpler, which is to to assume
* that a suitable root filesystem has already been prepared for use. */
static int
pivot_root(const char *new_root, const char *put_old)
{
    return syscall(SYS_pivot_root, new_root, put_old);
}

void mount_fs() {
    char path[PATH_MAX];
    const char *new_root = FILE_SYSTEM_PATH;
    const char *put_old = "pivot_root";

    /* This is the correct way to do pivot_root
     * https://lwn.net/Articles/800381/
     * This is the same process proposed in Docker runc
     * https://github.com/opencontainers/runc/blob/4932620b6237ed2a91aa5b5ca8cca6a73c10311b/libcontainer/SPEC.md#filesystem */

    /* Ensure that 'new_root' and its parent mount don't have
     * shared propagation (which would cause pivot_root() to
     * return an error), and prevent propagation of mount
     * events to the initial mount namespace */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == 1)
        printErr("mount-MS_PRIVATE");

    /* Ensure that 'new_root' is a mount point */
    if (mount(new_root, new_root, NULL, MS_BIND, NULL) == -1)
        printErr("mount-MS_BIND");

    /* Create directory to which old root will be pivoted */
    snprintf(path, sizeof(path), "%s/%s", new_root, put_old);
    if (mkdir(path, 0777) && errno != EEXIST)
        printErr("mkdir at mount_fs");

    /* And pivot root filesystem */
    if (pivot_root(new_root, path) == -1)
        printErr("pivot_root");
    syscall(SYS_pivot_root, FILE_SYSTEM_PATH, put_old);

    /* Switch the current working working directory to "/" */
    if (chdir("/") == -1)
        printErr("chdir at mount_fs");

    /* mounting /proc in order to support commands like `ps` */
    mount_proc();

    /* Unmount old root and remove mount point */
    if (umount2(put_old, MNT_DETACH) == -1)
        perror("umount2 at mount_fs");
    if (rmdir(put_old) == -1)   
        perror("rmdir at mount_fs");
}


void mount_proc() {
    /*
    * When we execute `ps`, we are able to see the process IDs. It will
    * look inside the directory called `/proc` (ls /proc) to get process
    * informations. From the host machine point of view, we can see all
    * the processes running on it.
    *
    * `/proc` isn't just a regular file system but a pseudo file system.
    * It does not contain real files but rather runtime system information
    * (e.g. system memory, devices mounted, hardware configuration, etc).
    * It is a mechanism the kernel uses to communicate information about
    * running processes from the kernel space into user spaces. From the
    * user space, it looks like a normal file system.
    * For example, informations about the current self process is stored
    * inside /proc/self. You can find the name of the process using
    * [ls -l /proc/self/exe].
    * Using a sequence of [ls -l /proc/self] you can see that your self
    * process has a new processID because /proc/self changes at the start
    * of a new process.
    *
    * Because we change the root inside the container, we don't currently
    * have this `/procs` file system available. Therefore, we need to
    * mount it otherwise the container process will see the same `/proc`
    * directory of the host.
    */

    /*
     * As proposed by docker runc spec (see the link in "Pointers" section
     * of README.md)
     * 
     * MS_NOEXEC -> do not allow programs to be executed from this
     *              filesystem
     * MS_NOSUID -> Do not honor set-user-ID and set-group-ID bits or file
     *              capabilities when executing programs from this filesystem.
     * MS_NODEV  -> Do not allow access to devices (special files) on this
     *              filesystem.
     *
     * for more informations about mount flags:
     *      http://man7.org/linux/man-pages/man2/mount.2.html
     */
    

    unsigned long mountflags = MS_NOEXEC | MS_NOSUID | MS_NODEV;

    if (mkdir("/proc", 0755) && errno != EEXIST) 
        printErr("mkdir when mounting proc"); 
    
    if (mount("proc", "/proc", "proc", mountflags, NULL)) 
        printErr("mount proc"); 
}