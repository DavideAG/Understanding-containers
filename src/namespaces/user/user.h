/*
 * UID and GID mapping. This operation its necessary to avoid
 * the user regression. In order to setup the user namespace
 * properly, we also need to map UserID and GroupID.
 * 
 * Some informations about User namespace:
 *      - The User namespace provides isolation of UIDs and GIDs
 *      - There can be multiple, distinct User namespaces in use
 *        on the same host at any given time
 *      - Every Linux process runs in one of these User namespaces
 *      - User namespaces allow for the UID of a process in User
 *        namespace 1 to be different to the UID for the same
 *        process in User namespace 2
 *      - UID/GID mapping provides a mechanism for mapping IDs
 *        between two separate User namespaces
 * 
 *              User namespace 1                                    UID/GID for UserNs 1
 *      +-------------------------------+                     +--------------------------------+
 *      |  Process A, UID=0, GID=0      |                     |username         UID     GID    |
 *      +-------------------------------+                     |root             0       0      |
 *      |  Process B, UID=1000, GID=1000|                     |non-root-user    1000    1000   |
 *      +-------------------------------+                     +--------------------^-------^---+
 *      |  Process C, UID=1000, GID=1000|                                          |       |
 *      +-----|-------------------------+                                          |       |
 *            |                                                                    |       |
 *            |                                                                    |       |
 *          clone()                                                                |       |
 *       CLONE_NEWUSER                                                             |       |
 *            |             User namespace 2                  +--------------------|-------|---+
 *            |      +-------------------------------+        |username         UID|    GID|   |
 *            +----> |  Process D, UID=0, GID=0      |        |root             0--+    0--+   |
 *                   +-------------------------------+        +--------------------------------+
 * 
 * This scheme shows namespaces, 1 and 2, with their corresponding
 * UID and GID tables. Note that process C, running as "non-root-user"
 * is able to spawn Process D, which is running as root.
 * 
 * Process D only has root privileges within the context of User
 * namespace 2. From the perspective of processes in User namespace 1,
 * process D is running as non-root-user, and as such, doesn’t have
 * those all-important root privileges.
 * 
 * The /proc/[pid]/uid_map and /proc/[pid]/gid_map files (available
 * since Linux 3.5) expose the mappings for user and group IDs inside
 * the user namespace for the process pid.  These files can be read
 * to view the mappings in a user namespace and written to (once)
 * define the mappings.
 * 
 * for more details see:
 * http://man7.org/linux/man-pages/man7/user_namespaces.7.html
 */
void map_uid_gid(pid_t child_pid);


/*
 * Update the mapping file 'map_file', with the value provided in
 * 'mapping', a string that defines a UID or GID mapping. A UID or
 * GID mapping consists of one or more newline-delimited records
 * of the form:
 * 
 *   ID_inside-ns    ID-outside-ns   length
 * 
 * Requiring the user to supply a string that contains newlines is
 * of course inconvenient for command-line use. Thus, we permit the
 * use of commas to delimit records in this string, and replace them
 * with newlines before writing the string to the file
 */
void update_map(char *mapping, char *map_file);


/*
 * Linux 3.19 made a change in the handling of setgroups(2) and the
 * 'gid_map' file to address a security issue. The issue allowed
 * *unprivileged* users to employ user namespaces in order to drop
 * The upshot of the 3.19 changes is that in order to update the
 * 'gid_maps' file, use of the setgroups() system call in this
 * user namespace must first be disabled by writing "deny" to one of
 * the /proc/PID/setgroups files for this namespace.  That is the
 * purpose of the following function.
 */
void proc_setgroups_write(pid_t child_pid, char *str);