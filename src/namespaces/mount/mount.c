#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include<linux/limits.h>

#include "mount.h"
#include "../../helpers/helpers.h"
#include "../../../config.h"

#define DEFAULT_DEVS 7
#define DEFAULT_FS 6

struct device {
	
	const char *path;
	int flags;
	int major;
	int minor;
	uid_t uid;
	gid_t gid;

};

struct filesystem {
	const char *path;
	const char* type;
	int flags;
	const char* data;
};

struct device default_devs[] = {{ "/dev/null", S_IFCHR | 0666, 1, 3, 0, 0 },
                                { "/dev/zero", S_IFCHR | 0666, 1, 5, 0, 0 },
                                { "/dev/full", S_IFCHR | 0666, 1, 7, 0, 0 },
                                { "/dev/tty", S_IFCHR | 0666, 5, 0, 0, 0 },
                                { "/dev/random", S_IFCHR | 0666, 1, 8, 0, 0 },
                                { "/dev/urandom", S_IFCHR | 0666, 1, 9, 0, 0 },
				{ "/dev/console", S_IFCHR | 0666, 5, 1, 0, 0}
                               };



struct filesystem default_fs[] = {{"/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL},
				  {"/dev", "tmpfs", MS_NOEXEC | MS_STRICTATIME, "mode=755"},
				  {"/dev/shm", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV, "mode=1777,size=65536k"},
				  {"/dev/mqueue", "mqueue", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL},
				  {"/dev/pts", "devpts", MS_NOEXEC | MS_NOSUID, "newinstance,ptmxmode=0666,mode=620"}, //TODO gid=5?
				  {"/sys", "sysfs", MS_NOEXEC | MS_NOSUID | MS_NODEV | MS_RDONLY, NULL}
				 };


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

	int i;
	char* base_path;
	char* target_fs;
	FILE* fp;	 
	char* target_dev_path;

	base_path = get_rootfs();
        if(base_path == NULL){
		fprintf(stderr,"=> cannot retrieve root_fs path.\n");
		exit(EXIT_FAILURE);
	}

        target_fs = base_path;
	for(i=0;i<DEFAULT_FS;i++){
		if(strcat(target_fs,default_fs[i].path) == NULL){
			fprintf(stderr,"=> strcat failed.\n");
			exit(EXIT_FAILURE);
		}
		if(mkdir(target_fs, 0755) && errno != EEXIST){
			fprintf(stderr,"=> mkdir %s failed.\n",default_fs[i].path);
			exit(EXIT_FAILURE);
		}
		if(mount("none",target_fs,default_fs[i].type,default_fs[i].flags,default_fs[i].data) == -1){
			fprintf(stderr,"=> mount %s failed.\n",default_fs[i].path);
			exit(EXIT_FAILURE);
		}
		target_fs[strlen(target_fs)-strlen(default_fs[i].path)]='\0';		
	}

  target_dev_path  = base_path;
  
  if(!has_userns){
	  for(i=0;i<DEFAULT_DEVS-1;i++){
		  if(strcat(target_dev_path,default_devs[i].path) == NULL){
			  fprintf(stderr,"=> strcat() failed.\n");
			  exit(EXIT_FAILURE);
		  }
		  if(mknod(target_dev_path, default_devs[i].flags, makedev(default_devs[i].major,default_devs[i].minor)) == -1){
			  fprintf(stderr,"=> mknod %s failed.\n",default_devs[i].path);
			  exit(EXIT_FAILURE);
		  }
		  target_dev_path[strlen(target_dev_path)-strlen(default_devs[i].path)]='\0';
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
		  /* We setup console later */
		  for(i=0;i<DEFAULT_DEVS-1;i++){
			  if(strcat(target_dev_path,default_devs[i].path) == NULL){
				  fprintf(stderr,"=> strcat() failed.\n");
				  exit(EXIT_FAILURE);
			  }

			  fp = fopen(target_dev_path,"a");

			  if(fp == NULL){
				  fprintf(stderr,"=> %s device creation failed\n",default_devs[i].path);
				  exit(EXIT_FAILURE);
			  }else{
				  if(mount(default_devs[i].path,target_dev_path,"bind",MS_BIND | MS_PRIVATE,"uid=0,gid=0,mode=0666") == -1){
					  fprintf(stderr,"=>%s mount failed\n",default_devs[i].path);
					  exit(EXIT_FAILURE);
				  }
			  }
			  fclose(fp);
			  target_dev_path[strlen(target_dev_path)-strlen(default_devs[i].path)]='\0';
		  }
  }
  
  /* SETUP CONSOLE
   *
   * The use of a pseudo TTY is optional within a container and it should 
   * support both. If a pseudo is provided to the container /dev/console 
   * will need to be setup by binding the console in /dev/ after it has 
   * been populated and mounted in tmpfs.
   */

  /* We assume that both stderr,stdin and stdout are linked to the same pty. */
  char* current_pts = ttyname(0);

  if(strcat(target_dev_path,default_devs[i].path) == NULL){
	  fprintf(stderr,"=> strcat() failed.\n");
	  exit(EXIT_FAILURE);
  }
  /* create target */
  fp = fopen(target_dev_path,"a");

  if(fp == NULL){
	  fprintf(stderr,"=> console creation failed\n");
	  exit(EXIT_FAILURE);
  }
  if(mount(current_pts,target_dev_path,"bind",MS_BIND | MS_PRIVATE,"uid=0,gid=0,mode=0600")==-1){
	  fprintf(stderr,"=> console bind failed\n");
	  exit(EXIT_FAILURE);
  }
  fclose(fp);

  if(!prepare_dev_fd()){
	  fprintf(stderr,"=> /dev/fd symlinks done.\n");
  }else{
	fprintf(stderr,"=> /dev/fd symlinks failed.\n");
	exit(EXIT_FAILURE);
  }
}

int prepare_dev_fd(){
	//TODO: error checks.
	/*symlink("..root_fs/proc/self/fd","..root_fs/dev/fd"); 
	symlink("..root_fs/proc/self/fd/0","..root_fs/dev/stdin"); 
	symlink("..root_fs/proc/self/fd/1","..root_fs/dev/stdout"); 
	symlink("..root_fs/proc/self/fd/2","..root_fs/dev/stderr");*/		
}

char* get_rootfs(){
	  static char root_fs[PATH_MAX];
	  if(getcwd(root_fs,PATH_MAX) ==  NULL){
		  fprintf(stderr,"=> getcwd() failed.\n");
		  return NULL;
	  }
	  if(memmove(&root_fs[strlen(root_fs)-strlen("build")],
		     "root_fs",sizeof("root_fs")) == NULL ){

		  fprintf(stderr,"=> memmove() failed.\n");
		  return NULL;
	  }
	  return root_fs;
}
