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
#include <sys/types.h>
#include <sys/stat.h>
#include "runc.h"
#include "../config.h"
#include "helpers/helpers.h"
#include "namespaces/user/user.h"
#include "namespaces/mount/mount.h"
#include "seccomp/seccomp_config.h"
#include "namespaces/cgroup/cgroup.h"
#include "capabilities/capabilities.h"
#include "namespaces/network/network.h"


int child_fn(void *args_par)
{
    struct clone_args *args = (struct clone_args *) args_par;
    char ch;
    
    if (args->has_userns) {
	    /* We are the consumer*/
	    close(args->sync_uid_gid_map_fd[1]);

	    /* We read EOF when the child close the its write end of the tip */
	    if (read(args->sync_uid_gid_map_fd[0], &ch, 1) != 0) {
		    fprintf(stderr, "Failure in child: read from pipe returned != 0\n");
		    exit(EXIT_FAILURE);
	    }
	    close(args->sync_uid_gid_map_fd[0]);
           
        /* UID 0 maps to UID 1000 outside. Ensure that the exec process
         * will run as UID 0 in order to drop its privileges */
        if (setresgid(0,0,0) == -1)
            printErr("setgid");
        if (setresuid(0,0,0) == -1)
            printErr("setuid");

        fprintf(stderr,"=> setuid and seguid done\n");	  
    }

    /* setting new hostname */
    set_container_hostname();

    /* Be sure umount events are not propagated to the host. */
    if(mount("","/","", MS_SLAVE | MS_REC, "") == -1)
	    printErr("mount failed");

   /* Make parent mount private to make sure following bind mount does
    * not propagate in other namespaces. Also it will help with kernel
    * check pass in pivot_root. (IS_SHARED(new_mnt->mnt_parent))
    * link1) https://bugzilla.redhat.com/show_bug.cgi?id=1361043 
    * link2) check prepareRoot() function in runC/rootfs_linux_go */
    if (mount("", "/", "", MS_PRIVATE, "") == 1)
	    printErr("mount-MS_PRIVATE");
 
   /* Ensure that 'new_root' is a mount point. 
    * By default, when a directory is bind mounted, only that directory is
    * mounted; if there are any submounts under the directory tree, they
    * are not bind mounted.  If the MS_REC flag is also specified, then a
    * recursive bind mount operation is performed: all submounts under the
    * source subtree (other than unbindable mounts) are also bind mounted
    * at the corresponding location in the target subtree.*/
    if (mount(FILE_SYSTEM_PATH,FILE_SYSTEM_PATH,
            "bind",
            MS_BIND | MS_REC, "") == -1)
	    printErr("mount-MS_BIND");

   /* Actually it is needed to mount everything we need before unmounting
    * the old root. This is because it is not allowed to mount a fs in an
    * user namespace if it is not already present in the current mount
    * namespace. 
    * When we clone we receive a copy of the mount points from our parent
    * but if we detach the old root we lost them, being not able to mount
    * anything.
    */
    prepare_rootfs(args->has_userns);

    /* mounting the new container file system */
    perform_pivot_root(args->has_userns);

    prepare_dev_fd();

   /* The root user inside the container must have less privileges than
    * the real host root, so drop some capablities */
    drop_caps();

    /* disallowing system calls using seccomp */
    sys_filter();
      
    if (execvp(args->command[0], args->command) != 0)
        printErr("command exec failed");

    /* we should never reach here! */
abort:
    exit(EXIT_FAILURE);
}

void runc(struct runc_args *runc_arguments)
{

    pid_t child_pid;
    void *child_stack;
    char *uid_map = NULL;
    char *gid_map = NULL;
    struct clone_args args;

    args.command = runc_arguments->child_entrypoint;
    args.command_size = runc_arguments->child_entrypoint_size;
    args.has_userns = runc_arguments->has_userns;

    print_running_infos(&args);

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
    if (runc_arguments->has_userns && (pipe(args.sync_uid_gid_map_fd) == -1)) 
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
    *       https://medium.com/@gggauravgandhi/
    *       uid-user-identifier-and-gid-group-identifier-in-linux-121ea68bf510
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
                CLONE_NEWNS  	|
                CLONE_NEWUTS 	|
                CLONE_NEWIPC 	|
                CLONE_NEWPID 	|
                CLONE_NEWNET;
    
    /* CLONE_NEWGROUP if required */
    if (runc_arguments->resources) {
        clone_flags |= CLONE_NEWCGROUP;
        args.resources = runc_arguments->resources;

        /* apply resource limitations */
        apply_cgroups(args.resources);
    }

    /* CLONE_NEWUSER if required */
    if (runc_arguments->has_userns)
	    clone_flags |= CLONE_NEWUSER;

    child_pid = clone(child_fn, child_stack + STACK_SIZE,
                        clone_flags | SIGCHLD, &args);

    if (child_pid < 0) {
        free_cgroup_resources();
        printErr("Unable to create child process");
    }
   
    /* Set up the network for the child. */
    prepare_netns(child_pid);

    /* We force a mapping of 0 1000 1, this means that in the child namespace there will
     * be only UID 0. 
     * Any call to setuid different from 0 fails because we does not specify
     * any other UID in the child namespace.
     * 
     * Update the UID and GUI maps in the child (see user.h).
     *    
     * 1. The /proc/PID/uid_map file is owned by the user ID that created the
     *    namespace, and is writeable only by that user. 
     * 2. After the creation of a new user namespace, the uid_map file of one of
     *    the processes in the namespace may be written to once to define the
     *    mapping of user IDs in the new user namespace. An attempt to write
     *    more than once to a uid_map file in a user namespace fails with the
     *    error EPERM. Similar rules apply for gid_map files.
     */

    if (args.has_userns) {
	    fprintf(stderr,"=> uid and gid mapping ...");

	    /* We are the producer*/
	    close(args.sync_uid_gid_map_fd[0]);
	    
	    map_uid_gid(child_pid); 

	    fprintf(stderr," done.\n");

	    /* Notify child that the mapping is done. */
	    close(args.sync_uid_gid_map_fd[1]);	
    }
 
    if (waitpid(child_pid, NULL, 0) == -1)
        printErr("waitpid");

    /* removing the cgroup folder associated with the child process */
    free_cgroup_resources();
    fprintf(stdout, "\nContainer process terminated.\n");
}

void set_container_hostname()
{
    char *hostname = strdup(HOSTNAME);
    int ret = sethostname(hostname, sizeof(hostname)+1);
    
    if (ret < 0)
        printErr("hostname");
}

/* prints the processID anche the entrypoint command of the container */
void print_running_infos(struct clone_args *args)
{
    char buf[COMAND_MAX_SIZE];
    char entrypoint[COMAND_MAX_SIZE];

    fprintf(stdout, "ProcessID: %ld\n", (long) getpid());

    snprintf(buf, COMAND_MAX_SIZE, "Running [ ");
    for (int i = 0; i < args->command_size;) {
        strcat(buf, args->command[i++]);
        strcat(buf, " ");
    }

    fprintf(stdout, "%s]\n", buf);
}