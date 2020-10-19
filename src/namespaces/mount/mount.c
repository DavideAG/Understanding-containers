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
#define DEFAULT_SYMLINKS 5

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

struct symlink {
	const char* path;
	const char* target;
};


struct device default_devs[] = {{ "/dev/null", S_IFCHR | 0666, 1, 3, 0, 0 },
                                { "/dev/zero", S_IFCHR | 0666, 1, 5, 0, 0 },
                                { "/dev/full", S_IFCHR | 0666, 1, 7, 0, 0 },
                                { "/dev/tty", S_IFCHR | 0666, 5, 0, 0, 0 },
                                { "/dev/random", S_IFCHR | 0666, 1, 8, 0, 0 },
                                { "/dev/urandom", S_IFCHR | 0666, 1, 9, 0, 0 },
								{ "/dev/console", S_IFCHR | 0666, 5, 1, 0, 0}
                               };



struct filesystem default_fs[] = {
		{"/proc", "proc", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL},
		{"/dev", "tmpfs", MS_NOEXEC | MS_STRICTATIME, "mode=755"},
		{"/dev/shm", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV, "mode=1777,size=65536k"},
		{"/dev/mqueue", "mqueue", MS_NOEXEC | MS_NOSUID | MS_NODEV, NULL},
		{"/dev/pts", "devpts", MS_NOEXEC | MS_NOSUID, "newinstance,ptmxmode=0666,mode=620"}, //TODO gid=5?
		{"/sys", "sysfs", MS_NOEXEC | MS_NOSUID | MS_NODEV | MS_RDONLY, NULL}
		};

struct symlink default_symlinks[] = {
		{"/proc/self/fd", "/dev/fd"},
		{"/proc/self/fd/0", "/dev/stdin"},
		{"/proc/self/fd/1", "/dev/stdout"},
		{"/proc/self/fd/2", "/dev/stderr"},
		{"/dev/pts/ptmx", "/dev/ptmx"}
		};

static int
pivot_root(const char *new_root, const char *put_old)
{
    return syscall(SYS_pivot_root, new_root, put_old);
}

/* SEE func (l *linuxStandardInit) Init() on runc/libcontainer for clarification
 *
 * While the documentation may claim otherwise, pivot_root(".", ".") is
 * actually valid.
 * What this results in is / being the new root but /proc/self/cwd being
 * the old root (after pivot_root is called).
 * Since we can play around with the cwd with pivot_root this allows us 
 * to pivot without creating directories in the rootfs. 
 * 
 * After pivot_root(".",".") call:
 *
 * 1.By this point, both the CWD and root dir of the calling process are
 *   in newroot (and so do not keep newroot busy, and thus don't prevent
 *   the unmount).
 * 2.After the pivot_root() operation, there are two mount points
 *   stacked at "/": oldroot and newroot, with oldroot a child mount
 *   stacked on top of newroot (I did some experiments to verify that this
 *   is so, by examination of /proc/self/mountinfo).
 * 3.The umount(".") operation unmounts the topmost mount from the pair
 *   of mounts stacked at "/".
 *
 * This succeeds because the pivot_root()  call  stacks  the old root mount
 * point (old_root) on top of the new root mount point at /.  At that point,
 * the calling  process's  root  directory  and current  working  directory
 * refer  to  the  new  root mount point (new_root).  During the subsequent 
 * umount()  call,  resolution  of "."   starts  with  new_root  and then 
 * moves up the list of mounts stacked at /, with the result that old_root
 * is unmounted.
 * Currently our "." is oldroot (according to the current kernel code).
 * However, purely for safety, we will fchdir(oldroot) since there isn't
 * really any guarantee from the kernel what /proc/self/cwd will be after a
 * pivot_root(2).
 */
void perform_pivot_root(int has_userns)
{
  int oldroot = open("/", O_DIRECTORY | O_RDONLY,0);
  if(oldroot == -1)
    printErr("open oldroot");

  int newroot = open(FILE_SYSTEM_PATH, O_DIRECTORY | O_RDONLY, 0);
  if(newroot == -1)
    printErr("open newroot");

  /* Change to the new root so that the pivot_root actually acts on it. */
  if(fchdir(newroot)==-1)
    printErr("fchdir newroot");

  if(pivot_root(".",".")==-1)
    printErr("pivot_root");

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
    if (base_path == NULL) {
		fprintf(stderr,"=> cannot retrieve root_fs path.\n");
		exit(EXIT_FAILURE);
	}

    target_fs = base_path;
	for (i=0; i<DEFAULT_FS; i++) {
		if (strcat(target_fs,default_fs[i].path) == NULL) {
			fprintf(stderr,"=> strcat failed.\n");
			exit(EXIT_FAILURE);
		}
		if (mkdir(target_fs, 0755) && errno != EEXIST) {
			fprintf(stderr,"=> mkdir %s failed.\n",default_fs[i].path);
			exit(EXIT_FAILURE);
		}
		if (mount("none",target_fs,default_fs[i].type,default_fs[i].flags,default_fs[i].data) == -1) {
			fprintf(stderr,"=> mount %s failed.\n",default_fs[i].path);
			exit(EXIT_FAILURE);
		}
		target_fs[strlen(target_fs)-strlen(default_fs[i].path)]='\0';		
	}

  target_dev_path  = base_path;
  
	if (!has_userns) {
		for (i=0;i<DEFAULT_DEVS-1;i++) {
			if (strcat(target_dev_path,default_devs[i].path) == NULL) {
				fprintf(stderr,"=> strcat() failed.\n");
				exit(EXIT_FAILURE);
			}
			if (mknod(target_dev_path, default_devs[i].flags, makedev(default_devs[i].major,default_devs[i].minor)) == -1) {
				fprintf(stderr,"=> mknod %s failed.\n",default_devs[i].path);
				exit(EXIT_FAILURE);
			}
			target_dev_path[strlen(target_dev_path)-strlen(default_devs[i].path)]='\0';
		}
	} else {
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
  

  /* We assume that both stderr,stdin and stdout are linked to the same pty. */
  char* current_pts = ttyname(0);

  if (strcat(target_dev_path,default_devs[i].path) == NULL) {
	  fprintf(stderr,"=> strcat() failed.\n");
	  exit(EXIT_FAILURE);
  }
  /* create target */
  fp = fopen(target_dev_path,"a");

  if (fp == NULL) {
	  fprintf(stderr,"=> console creation failed\n");
	  exit(EXIT_FAILURE);
  }
  if (mount(current_pts,target_dev_path,"bind",MS_BIND | MS_PRIVATE,"uid=0,gid=0,mode=0600")==-1) {
	  fprintf(stderr,"=> console bind failed\n");
	  exit(EXIT_FAILURE);
  }
  fclose(fp);
}

int prepare_dev_fd() {
	int i;
	for (i=0;i<DEFAULT_SYMLINKS;i++) {
		if (symlink(default_symlinks[i].path,default_symlinks[i].target) == -1) {
			fprintf(stderr,"=> Symlink %s failed.\n",default_symlinks[i].target);
			exit(EXIT_FAILURE);
		}
	}
}

char* get_rootfs() {
	static char root_fs[PATH_MAX];
	if (getcwd(root_fs,PATH_MAX) ==  NULL) {
		fprintf(stderr,"=> getcwd() failed.\n");
		return NULL;
	}
	if (memmove(&root_fs[strlen(root_fs)-strlen("build")],
			"root_fs",sizeof("root_fs")) == NULL ) {

		fprintf(stderr,"=> memmove() failed.\n");
		return NULL;
	}
	return root_fs;
}
