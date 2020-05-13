#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include "network.h"
#include "../../helpers/helpers.h"

void start_network(pid_t child_pid) {
    char buffer[MAX_BUF_SIZE];
    char *command = "/usr/local/bin/netsetgo -pid";
    snprintf(buffer, sizeof(buffer), "%s %d", command, child_pid);
    //fprintf(stdout,"buffer: %s", buffer);
    system(buffer);
}