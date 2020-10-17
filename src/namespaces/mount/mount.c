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
#include <sys/types.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
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

/* SEE func (l *linuxStandardInit) Init() on runc/libcontainer for clarification. */
void perform_pivot_root(int has_userns){


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

 
  /* Ensure that 'new_root' is a mount point 
   * By default, when a directory is bind mounted, only that directory is
   * mounted; if there are any submounts under the directory tree, they
   * are not bind mounted.  If the MS_REC flag is also specified, then a
   * recursive bind mount operation is performed: all submounts under the
   * source subtree (other than unbindable mounts) are also bind mounted
   * at the corresponding location in the target subtree.*/
  if (mount(FILE_SYSTEM_PATH,FILE_SYSTEM_PATH, "bind", MS_BIND | MS_REC, "") == -1)
      printErr("mount-MS_BIND");

  prepare_rootfs(has_userns);
 /* While the documentation may claim otherwise, pivot_root(".", ".") is
  * actually valid. What this results in is / being the new root but
  * /proc/self/cwd being the old root. Since we can play around with the cwd
  * with pivot_root this allows us to pivot without creating directories in
  * the rootfs. Shout-outs to the LXC developers for giving us this idea.
  * Instead of using a temporary directory for the pivot_root put-old, use "." both
  * for new-root and old-root.  Then fchdir into the old root temporarily in
  * order to unmount the old-root, and finallychdir back into our '/'. 
  */

  int oldroot = open("/",O_DIRECTORY | O_RDONLY,0);
  if(oldroot == -1)
    printErr("open oldroot");

  int newroot = open(FILE_SYSTEM_PATH,O_DIRECTORY | O_RDONLY,0);
  if(newroot == -1)
    printErr("open newroot");

  /* Change to the new root so that the pivot_root actually acts on it. */
  if(fchdir(newroot)==-1)
    printErr("fchdir newroot");

  if(pivot_root(".",".")==-1)
    printErr("pivot_root");

  /* 1.By this point, both the CWD and root dir of the calling process are
   *   in newroot (and so do not keep newroot busy, and thus don't prevent
   *   the unmount).
   * 2.After the pivot_root() operation, there are two mount points
   *   stacked at "/": oldroot and newroot, with oldroot a child mount
   *   stacked on top of newroot (I did some experiments to verify that this
   *   is so, by examination of /proc/self/mountinfo).
   * 3.The umount(".") operation unmounts the topmost mount from the pair
   *   of mounts stacked at "/".
   *
   *
   *  new_root and put_old may be the same  directory.   In  particular,
   *  the following sequence allows a pivot-root operation without need‐
   *  ing to create and remove a temporary directory put_old:
   *
   *      chdir(new_root);
   *      pivot_root(".", ".");
   *      umount2(".", MNT_DETACH);
   *
   *  This sequence succeeds because the pivot_root()  call  stacks  the
   *  old root mount point (old_root) on top of the new root mount point
   *  at /.  At that point, the calling  process's  root  directory  and
   *  current  working  directory  refer  to  the  new  root mount point
   *  (new_root).  During the subsequent umount()  call,  resolution  of
   *  "."   starts  with  new_root  and then moves up the list of mounts
   *  stacked at /, with the result that old_root is unmounted.
   *
   *
   * Currently our "." is oldroot (according to the current kernel code).
   * However, purely for safety, we will fchdir(oldroot) since there isn't
   * really any guarantee from the kernel what /proc/self/cwd will be after a
   * pivot_root(2).
   */

  if(fchdir(oldroot)==-1)
    printErr("fchdir oldroot");

  /* Make oldroot rslave to make sure our unmounts don't propagate to the
   * host (and thus bork the machine). We don't use rprivate because this is
   * known to cause issues due to races where we still have a reference to a
   * mount while a process in the host namespace are trying to operate on
   * something they think has no mounts (devicemapper in particular).
   */
  if(mount("", ".", "", MS_SLAVE|MS_REC, "")==-1)
    printErr("mount oldroot as slave");


  /* Actually it is needed to mount everything we need before unmounting
   * the old root. This is because it is not allowed to mount a fs in an
   * user namespace if it is not already present in the current mount
   * namespace. 
   * When we clone we receive a copy of the mount points from our parent
   * but if we detach the old root we lost them, being not able to mount
   * anything.
   */

  /* Preform the unmount. MNT_DETACH allows us to unmount /proc/self/cwd.
   * Here we lost all the mounts point that we received from our parent when
   * clone was called.
   * We will see only the mount points of the container that has been prepared
   * by prepare_rootfs(). */
  if(umount2(".", MNT_DETACH)==-1)
    printErr("unmount oldroot");

  close(newroot);
  close(oldroot);

  /* Switch back to our shiny new root. */
  chdir("/");

}

void prepare_rootfs(int has_userns){

  /* Set up the proc VFS */
  if(!mount_proc()){
	  fprintf(stderr,"=> procfs mount done.\n");
  }else{
	  fprintf(stderr,"=> procfs mount failed.\n");
	  exit(EXIT_FAILURE);
  }

  /* Set up the sysfs */
  if(!mount_sysfs()){
	  fprintf(stderr,"=> sysfs mount done.\n");
  }else{
	  fprintf(stderr,"=> sysfs mount failed.\n");
	  exit(EXIT_FAILURE);
  }
 
  /* Set up the /dev fs */
  if(!mount_dev()){
	  fprintf(stderr,"=> /dev mount done.\n");
  }else{
	  fprintf(stderr,"=> /dev mount failed.\n");
	  exit(EXIT_FAILURE);
  }

  /* Set up the /dev/pts fs */
  if(!mount_dev_pts()){
	  fprintf(stderr,"=> /dev/pts mount done.\n");
  }else{
	  fprintf(stderr,"=> /dev/pts mount failed.\n");
	  exit(EXIT_FAILURE);
  } 

  if(!mount_dev_shm()){
	  fprintf(stderr,"=> /dev/shm mount done.\n");
  }else{
	  fprintf(stderr,"=> /dev/shm mount failed.\n");
	  exit(EXIT_FAILURE);
  }

  if(!mount_dev_mqueue()){
	  fprintf(stderr,"=> /dev/queue mount done.\n");
  }else{
	  fprintf(stderr,"=> /dev/queue mount failed.\n");
	  exit(EXIT_FAILURE);
  } 

  /* Only priviliged container can create devices.
   * TODO: how to manage unprivileged container devices? 
   */
  if(!has_userns){ 
	  if(!create_devices())
		  fprintf(stderr,"=> devices creation done.\n");
	  else{
	          fprintf(stderr,"=> devices creation failed.\n");
	          exit(EXIT_FAILURE);
	  }
  }else{
         /* Set up devices for unpivileged container.
          *
	  * One notable restriction is the inability to use the mknod command. 
          * Permission is denied for device creation within the container when 
          * run by the root user.
	  * Why?
	  * Unprivileged containers have all capabilities, including CAP_MKNOD, it
          * just so happens that the kernel check for mknod will not allow root in
          * an unprivileged container to run mknod, no matter its capabilities.
	  *
	  * //TODO: possible solutions
          * 1 Because you are unprivileged, you cannot create /dev/random. All you
	  *   can do is to bind mount it from the host. So it gets the same uid/gid/perms
	  *   as on the host. 
	  *   You can set it up that way yourself by becoming root on the host, mknod'ing
	  *   the device in the container's /dev, and chowning it to your container root uid.
	  *   ==> https://lxc-users.linuxcontainers.narkive.com/5MX4xx6N/
	  *   	  dev-random-problem-with-unprivileged-minimal-containers
	  *   
	  *   The device can be created just as a dummy file.
	  *   ==> https://unix.stackexchange.com/questions/538594/
	  *   	  how-to-make-dev-inside-linux-namespaces
	  */


	  char* default_devs[6] = { "/dev/null",
		  		    "/dev/random",
				    "/dev/urandom",
				    "/dev/full",
			            "/dev/tty",
				    "/dev/zero"
	  			  };
	  for(int i=0;i<6;i++){
		 
		  FILE* fp;
		  char abs_dev_path[100] = "/home/gabriele/Desktop/Understanding-containers/root_fs";
		  
		  strcat(abs_dev_path,default_devs[i]);
		
		  fp = fopen(abs_dev_path,"a");
		  
		  if(fp == NULL){
			  fprintf(stderr,"=> %s device creation failed\n",default_devs[i]);
			  exit(EXIT_FAILURE);
		 }else{
			  if(mount(default_devs[i],abs_dev_path,"bind",MS_BIND,"mode=0666") == -1){
				  fprintf(stderr,"=>%s mount failed\n",default_devs[i]);
				  exit(EXIT_FAILURE);
			  }
		  }
		  
		  fclose(fp);
	  }

	  FILE* fp;

	  /* We assume that both stderr,stdin and stdout are linked to the same pty. */
	  char* current_pts = ttyname(0);

	  fp = fopen("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/console","a");

	  if(fp == NULL){
		  fprintf(stderr,"=> console creation failed\n");
		  exit(EXIT_FAILURE);
	  }


	  if(mount(current_pts,"/home/gabriele/Desktop/Understanding-containers/root_fs/dev/console","bind",MS_BIND,"uid=0,gid=0,mode=0600")==-1){
		  fprintf(stderr,"=> console bind failed\n");
		  exit(EXIT_FAILURE);
	  }
	  
	  
  }
 

  if(!prepare_dev_fd()){
	  fprintf(stderr,"=> /dev/fd symlinks done.\n");
  }else{
	fprintf(stderr,"=> /dev/fd symlinks failed.\n");
	exit(EXIT_FAILURE);
  }

}

int mount_proc() {
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
    

    unsigned long mountflags = MS_NOSUID | MS_NOEXEC | MS_NODEV;

    if (mkdir("/home/gabriele/Desktop/Understanding-containers/root_fs/proc", 0755) && errno != EEXIST)
	    return -1;
   
    /* The proc filesystem is not associated with a special device, and when 
     * mounting it, an arbitrary keyword, such as proc can be used instead of
     * a device specification. This represents the first argument (source) of
     * mount(). */  
    if (mount("proc", "/home/gabriele/Desktop/Understanding-containers/root_fs/proc", "proc", mountflags, NULL) == -1)
	   return -1;
    
    return 0; 
}

int mount_sysfs(){

	unsigned long mountflags = MS_NOEXEC | MS_NOSUID | MS_NODEV | MS_RDONLY;

	if(mkdir("/home/gabriele/Desktop/Understanding-containers/root_fs/sys", 0755) && errno != EEXIST)
		return -1;

	if(mount("sysfs","/home/gabriele/Desktop/Understanding-containers/root_fs/sys","sysfs",mountflags,NULL) == -1)
		return -1;
        
	return 0;
}

int mount_dev(){

	unsigned long mountflags = MS_NOEXEC | MS_STRICTATIME;

	if(mkdir("/home/gabriele/Desktop/Understanding-containers/root_fs/dev", 0755) && errno != EEXIST)
		return -1;

	if(mount("dev","/home/gabriele/Desktop/Understanding-containers/root_fs/dev","tmpfs",mountflags,"mode=755,size=65536k") == -1)
		return -1;
        
	return 0;

}

int mount_dev_pts(){

	/* The shell is actually within a pty but the pty itself is outside the container,
	 * so commands like 'tty' thinks we are not within a tty. However, if you directly 
	 * call the isatty() function, it will return true. The solution would be to create 
	 * the pty within the container instead of outside.
	 */

	unsigned long mountflags = MS_NOEXEC | MS_NOSUID;

	if(mkdir("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/pts", 0755) && errno != EEXIST)
		return -1;

	//TODO: gid=5
	if(mount("devpts","/home/gabriele/Desktop/Understanding-containers/root_fs/dev/pts","devpts",mountflags,"newinstance,ptmxmode=066,mode=620") == -1)
		return -1;
        
	return 0;
}

int mount_dev_shm(){

	unsigned long mountflags = MS_NOEXEC | MS_NOSUID | MS_NODEV;

	if(mkdir("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/shm", 0755) && errno != EEXIST)
		return -1;

	//TODO: gid=5
	if(mount("shm","/home/gabriele/Desktop/Understanding-containers/root_fs/dev/shm","tmpfs",mountflags,"mode=1777,size=65536k") == -1)
		return -1;
        
	return 0;
}

int mount_dev_mqueue(){
	
	unsigned long mountflags = MS_NOEXEC | MS_NOSUID | MS_NODEV;

	if(mkdir("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/mqueue", 0755) && errno != EEXIST)
		return -1;

	if(mount("mqueue","/home/gabriele/Desktop/Understanding-containers/root_fs/dev/mqueue","mqueue",mountflags,NULL) == -1)
		return -1;
        
	return 0;
}

int create_devices(){
        
	unsigned long mknodflags = S_IFCHR | 0666 ;
	
	/* If the file type is S_IFCHR or S_IFBLK, then dev specifies the major
         * and minor numbers of the newly created device special file
         * (makedev(3) may be useful to build the value for dev); otherwise it
         *  is ignored.
	 */	
 	if(mknod("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/null", mknodflags, makedev(1,3)) == -1){
		fprintf(stderr,"=> /dev/null failed.\n");
		return -1;
	}
	if(mknod("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/zero", mknodflags, makedev(1,5)) == -1){
		fprintf(stderr,"=> /dev/zero failed.\n");
		return -1;
	}

	if(mknod("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/full", mknodflags, makedev(1,7)) == -1){
		fprintf(stderr,"=> /dev/full failed.\n");
		return -1;
	}
	if(mknod("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/tty",mknodflags, makedev(4,1)) == -1){
		fprintf(stderr,"=> /dev/tty failed.\n");
		return -1;
	}

	if(mknod("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/random",mknodflags, makedev(1,8)) == -1){
		fprintf(stderr,"=> /dev/random failed.\n");
		return -1;
	}

	if(mknod("/home/gabriele/Desktop/Understanding-containers/root_fs/dev/urandom",mknodflags, makedev(1,9)) == -1){
		fprintf(stderr,"=> /dev/urandom failed.\n");
		return -1;
	}

	return 0;

}

int prepare_dev_fd(){
	
	//TODO: error checks.
	/*symlink("..root_fs/proc/self/fd","..root_fs/dev/fd"); 
	symlink("..root_fs/proc/self/fd/0","..root_fs/dev/stdin"); 
	symlink("..root_fs/proc/self/fd/1","..root_fs/dev/stdout"); 
	symlink("..root_fs/proc/self/fd/2","..root_fs/dev/stderr");*/		
}
