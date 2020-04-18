#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include "helpers.h"
#include "runc.h"
#include "../config.h"


int bootstrap_container(void *buf) {
    /* here we can customize the container process */
    fprintf(stdout, "ProcessID: %ld\n", (long) getpid());

    /* setting new hostname */
    set_container_hostname();
    
    /* mounting /proc in order to support ps */
    mount_proc();


    system((char *)buf);
    return 0;
}

void runc(int n_values, char *command_input[]) {
    void *child_stack_bottom;
    void *child_stack_top;
    int pid;
    char *buffer = (char *) calloc(COMAND_MAX_SIZE, sizeof(char));

    if (buffer == NULL)
        printErr("buffer allocation");

    fprintf(stdout, "ProcessID: %ld\n", (long) getpid());
    fprintf(stdout, "Running [ ");
    for (int i = 1; i < n_values-1;) {
        fprintf(stdout, "%s ", command_input[++i]);
        strcat(buffer, command_input[i]);
        strcat(buffer, " ");
    }
    fprintf(stdout, "]\n");

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
    CLONE_NEWUTS | CLONE_NEWPID | SIGCHLD, (void *) buffer);
    if (pid < 0)
        printErr("Unable to create child process");
    
    if (waitpid(pid, NULL, 0) == -1)
        printErr("waitpid");

    fprintf(stdout, "\nContainer process terminated.\n");

    /* then free the memory */
    free(child_stack_bottom);
    free(buffer);
}

void set_container_hostname() {
    char *hostname = strdup(HOSTNAME);
    int ret = sethostname(hostname, sizeof(hostname)+1);
    if (ret < 0)
        printErr("hostname");
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

}