#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <string.h>
#include <errno.h>
#include "helpers.h"
#include "runc.h"
#include "../config.h"


int bootstrap_container(void *args_par) {

    struct clone_args *args = (struct clone_args *)args_par;

    /* here we can customize the container process */
    fprintf(stdout, "ProcessID: %ld\n", (long) getpid());

    /* setting new hostname */
    set_container_hostname();
    
    /* mounting the new container file system */
    mount_fs();

    /* mounting /proc in order to support commands like `ps` */
    mount_proc();

    if (execvp(args->argv[0], args->argv) != 0)
        printErr("command exec failed");
    
    /* we should never reach here! */
    exit(EXIT_FAILURE);
}

void runc(int n_values, char *command_input[]) {
    void *child_stack_bottom;
    void *child_stack_top;
    int pid;
    struct clone_args args;

    /* why 2? just skipping <prog_name> <cmd> */
    args.argv = &command_input[2];
    args.n_args = n_values - 2;

    printRunningInfos(&args);

    /* child stack allocation */
    child_stack_bottom = malloc(STACK_SIZE);
    if (child_stack_bottom == NULL)
        printErr("child stack allocation");

    /* stack will grow from top to bottom */
    child_stack_top = child_stack_bottom + STACK_SIZE;
    
    printf("Booting up your container...\n\n");

    /* Here we can specify the namespace we want by using the appropriate flags
     * - CLONE_NEWUTS:  UTS namespace, where UTS stands for Unix Timestamp System.
	 * - CLONE_NEWPID:  Process IDs namespace.
	 * - CLONE_NEWNS:   Mount namespace to make the mount point only visible
	 *                  inside the container.
     */
    pid = clone(bootstrap_container, child_stack_top,
    CLONE_NEWUTS | CLONE_NEWPID | SIGCHLD, &args);
    if (pid < 0)
        printErr("Unable to create child process");
    
    if (waitpid(pid, NULL, 0) == -1)
        printErr("waitpid");

    fprintf(stdout, "\nContainer process terminated.\n");

    umount_proc();

    /* then free the memory */
    free(child_stack_bottom);
    
}

void set_container_hostname() {
    char *hostname = strdup(HOSTNAME);
    int ret = sethostname(hostname, sizeof(hostname)+1);
    if (ret < 0)
        printErr("hostname");
}

void mount_fs() {
    if (chroot(FILE_SYSTEM_PATH) != 0)
        printErr("chroot to the new file system");
    if (chdir("/") < 0)
        printErr("chdir to the root (/)");
}


void mount_proc() {
    /*
     * When we execute `ps`, we are able to see the process IDs. It will look inside
	 * the directory called `/proc` (ls /proc) to get process informations. From the
     * host machine point of view, we can see all the processes running on it.
	 *
	 * `/proc` isn't just a regular file system but a pseudo file system. It does not
	 * contain real files but rather runtime system information (e.g. system
	 * memory, devices mounted, hardware configuration, etc). It is a mechanism the
	 * kernel uses to communicate information about running processes from the kernel
	 * space into user spaces. From the user space, it looks like a normal file
	 * system.
     * For example, informations about the current self process is stored inside
     * /proc/self. You can find the name of the process using [ls -l /proc/self/exe].
     * Using a sequence of [ls -l /proc/self] you can see that your self process has
     * a new processID because /proc/self changes at the start of a new process.
	 *
	 * Because we change the root inside the container, we don't currently have this
	 * `/procs` file system available. Therefore, we need to mount it otherwise the
     * container process will see the same `/proc` directory of the host.
     */

    if (mount("proc", "/proc", "proc", 0, "") < 0)
        printErr("mounting /proc");
}

void umount_proc() {
    char *buffer = strdup(FILE_SYSTEM_PATH);
    strcat(buffer, "/proc");
    if (umount(buffer) != 0)
        printErr("unmounting /proc");
    fprintf(stdout, "/proc correctly umounted\n");
}