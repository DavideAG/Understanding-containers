/**
 * From mount_namespaces(7)
 * http://man7.org/linux/man-pages/man7/mount_namespaces.7.html
 * 
 * A new mount namespace is created using either clone(2) or unshare(2)
 * with the CLONE_NEWNS flag. When a new mount namespace is created,
 * its mount point list is initialized as follows:
 * 
 *  *   If the namespace is created using clone(2), the mount point list
 *      of the child's namespace is a copy of the mount point list in the
 *      parent's namespace.
 * 
 *  *   If the namespace is created using unshare(2), the mount point list
 *      of the new namespace is a copy of the mount point list in the
 *      caller's previous mount namespace.
 * 
 * -----------------------------------------------------------------------
 * So we need some way of clearing the host's mounts from the new Mount
 * namespace in order to keep them secure. What we need is pivot_root.
 * pivot_root allows you to set a new root filesystem for the calling
 * process. For exampel we can change what '/' is.
 * This is what allows, for example, to run a CentOS container inside
 * Ubuntu host machine.
 * pivot_root must be called from within the new Mount namespace,
 * otherwise we'll end up changing the host's '/' which is not the
 * intention! And we want all this to happen before the namespaced shell
 * starts so that the requested root filesystem is ready for when it does.
 */

/* mounting the container file system -> ubuntu-fs */
void perform_pivot_root();

void prepare_rootfs();

/* /proc fs mount */
int mount_proc();

/* /sys fs mount */
int mount_sysfs();

/* /dev mount */
int mount_dev();

/* /dev/pts mount */
int mount_dev_pts();

/* /dev/shm mount */
int mount_dev_shm();

/*/dev/mqueue mount */
int mount_dev_mqueue();

int create_devices();

int prepare_dev_fd();
