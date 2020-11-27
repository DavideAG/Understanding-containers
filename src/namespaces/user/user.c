#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include "../../helpers/helpers.h"
#include "user.h"

void map_uid_gid(pid_t child_pid) {
    char *uid_map = NULL;
    char *gid_map = NULL;
    char map_path[PATH_MAX];
    char map_buf[MAX_BUF_SIZE];

    snprintf(map_path, PATH_MAX, "/proc/%ld/uid_map", (long) child_pid);
    snprintf(map_buf, MAX_BUF_SIZE, "0 100000 65536");
    uid_map = map_buf;
    update_map(uid_map, map_path);

    proc_setgroups_write(child_pid, "deny");
    snprintf(map_path, PATH_MAX, "/proc/%ld/gid_map",(long) child_pid);
    snprintf(map_buf, MAX_BUF_SIZE, "0 100000 65536");
    gid_map = map_buf;
    update_map(gid_map, map_path);
}


void update_map(char *mapping, char *map_file) {
    int fd, j;
    size_t map_len;     /* Length of 'mapping' */

    /* Replace commas in mapping string with newlines */

    map_len = strlen(mapping);
    for (j = 0; j < map_len; j++)
        if (mapping[j] == ',')
            mapping[j] = '\n';

    fd = open(map_file, O_RDWR);
    if (fd == -1) {
        printErr(map_file);
    }

    if (write(fd, mapping, map_len) != map_len) {
        printErr(map_file);
    }

    close(fd);
}


void proc_setgroups_write(pid_t child_pid, char *str) {
    char setgroups_path[PATH_MAX];
    int fd;

    snprintf(setgroups_path, PATH_MAX, "/proc/%ld/setgroups",
            (long) child_pid);

    fd = open(setgroups_path, O_RDWR);
    if (fd == -1) {

        /* We may be on a system that doesn't support
            /proc/PID/setgroups. In that case, the file won't exist,
            and the system won't impose the restrictions that Linux 3.19
            added. That's fine: we don't need to do anything in order
            to permit 'gid_map' to be updated.

            However, if the error from open() was something other than
            the ENOENT error that is expected for that case,  let the
            user know. */

        if (errno != ENOENT)
            fprintf(stderr, "ERROR: open %s: %s\n", setgroups_path,
                strerror(errno));
        return;
    }

    if (write(fd, str, strlen(str)) == -1)
        fprintf(stderr, "ERROR: write %s: %s\n", setgroups_path,
            strerror(errno));

    close(fd);
}