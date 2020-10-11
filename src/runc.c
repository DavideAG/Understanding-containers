#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mount.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "./helpers/helpers.h"
#include "runc.h"
#include "namespaces/user/user.h"
#include "namespaces/mount/mount.h"
#include "namespaces/network/network.h"
#include "../config.h"

       
int child_fn(void *args_par) {

    struct clone_args *args = (struct clone_args *)args_par;
    char ch;

    /* Wait until the parent has updated the UID and GID mappings.
    See the comment in main(). We wait for end of file on a
    pipe that will be closed by the parent process once it has
    updated the mappings. */

    close(args->pipe_fd[1]);    /* Close our descriptor for the write
                                   dend of the pipe so that we see EOF
                                   when parent closes its descriptor.
			        */
    if (read(args->pipe_fd[0], &ch, 1) != 0) {
        fprintf(stderr,
                "Failure in child: read from pipe returned != 0\n");
        exit(EXIT_FAILURE);
    }

    close(args->pipe_fd[0]);

    /* here we can customize the container process */
    fprintf(stdout, "ProcessID: %ld\n", (long) getpid());

    /* setting new hostname */
    set_container_hostname();

    /* mounting the new container file system */
    mount_fs();

    
    /* UID 0 maps to UID 1000 outside. Ensure that the exec process
     * will run as UID 0 in order to drop its privileges. */
    if (setgid(0) == -1)
        printErr("Failed to setgid: %m\n");
    if (setuid(0) == -1)
        printErr("Failed to setuid: %m\n");

    if (execvp(args->argv[0], args->argv) != 0)
        printErr("command exec failed");
    
    /* we should never reach here! */
    exit(EXIT_FAILURE);
}

void runc(int n_values, char *command_input[]) {
    void *child_stack;
    pid_t child_pid;
    struct clone_args args;
    char *uid_map = NULL;
    char *gid_map = NULL;


    /* why 2? just skipping <prog_name> <cmd> */
    args.argv = &command_input[2];
    args.n_args = n_values - 2;

    printRunningInfos(&args);

    /* child stack allocation */
    child_stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);

    if (child_stack == MAP_FAILED)
        printErr("child stack allocation");
    
    printf("Booting up your container...\n\n");

    /*  We use a pipe to synchronize the parent and child, in order to 
        ensure that the parent sets the UID and GID maps before the child 
        calls execve(). This ensures that the child maintains its 
        capabilities during the execve() in the common case where we 
        want to map the child's effective user ID to 0 in the new user 
        namespace. Without this synchronization, the child would lose 
        its capabilities if it performed an execve() with nonzero 
        user IDs (see the capabilities(7) man page for details of the 
        transformation of a process's capabilities during execve()). */
    if (pipe(args.pipe_fd) == -1) 
        printErr("pipe");

    /* 
    * Here we can specify the namespace we want by using the appropriate
    * flags
    * - CLONE_NEWNS:   Mount namespace to make the mount point only
    *                  visible inside the container.
    *                  (but we have to take off by the host's mounts and
    *                  rootfs)
    * - CLONE_NEWUTS:  UTS namespace, where UTS stands for Unix Timestamp
    *                  System.
    * - CLONE_NEWIPC:  Create a process in a new IPC namespace
    * - CLONE_NEWPID:  Process IDs namespace. (we need to mount /proc !)
    * - CLONE_NEWNET:  Create a process in a new Network namespace
    *                  (we have to setup interfaces inside the namespace)
    * - CLONE_NEWUSER: User namespace. (we have to provide a UID/GID mapping)
    *                  about UID and GUI:
    * https://medium.com/@gggauravgandhi/uid-user-identifier-and-gid-group-identifier-in-linux-121ea68bf510
    * 
    * The execution context of the cloned process is, in part, defined
    * by the flags passed in.
    * 
    * When requesting a new User namespace alongside other namespaces,
    * the User namespace will be created first. User namespaces can be
    * created without root permissions, which means we can now drop the
    * sudo and run our program as a non-root user!
    */
    int clone_flags =
                CLONE_NEWNS  |
                CLONE_NEWUTS |
                CLONE_NEWIPC |
                CLONE_NEWPID |
                CLONE_NEWNET |
                CLONE_NEWUSER|
                SIGCHLD;
                /*TODO: CLONE_NEWCGROUP 
		 *      CLONE_NEWTIME
		 */

    child_pid = clone(child_fn, child_stack + STACK_SIZE, clone_flags, &args);
    if (child_pid < 0)
        printErr("Unable to create child process");

     prepare_netns(child_pid);
      
    /*    Update the UID and GUI maps in the child (see user.h).
     *    
     * 1. The /proc/PID/uid_map file is owned by the user ID that created the namespace,
     *    and is writeable only by that user. 
     * 2. After the creation of a new user namespace, the uid_map file of one of the processes
     *    in the namespace may be written to once to define the mapping of user IDs in the new
     *    user namespace. An attempt to write more than once to a uid_map file in a user namespace
     *    fails with the error EPERM. Similar rules apply for gid_map files.
     *    */
    
    char *cmd;

    map_uid_gid(child_pid); 
 
    /* Close the write end of the pipe, to signal to the child that we
     * have updated the UID/GID maps and that we updated the network
     * configuration */
    //close(args.pipe_fd[1]);

    close(args.pipe_fd[1]);

    if (waitpid(child_pid, NULL, 0) == -1)
        printErr("waitpid");

    fprintf(stdout, "\nContainer process terminated.\n");
    
}


void set_container_hostname() {
    char *hostname = strdup(HOSTNAME);
    int ret = sethostname(hostname, sizeof(hostname)+1);
    if (ret < 0)
        printErr("hostname");
}
