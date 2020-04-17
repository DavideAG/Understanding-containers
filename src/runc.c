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


int child_fn(void *buf) {
    /* here we can customize the container process */
    
    /* setting new hostname */
    char *hostname = strdup("container");
    sethostname(hostname, sizeof(hostname)+1);
    
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
    
    pid = clone(child_fn, child_stack_top, CLONE_NEWUTS | SIGCHLD, (void *) buffer);
    if (pid < 0)
        printErr("Unable to create child process");
    
    if (waitpid(pid, NULL, 0) == -1)
        printErr("waitpid");

    fprintf(stdout, "Container process terminated.\n");

    /* then free the memory */
    free(child_stack_bottom);
    free(buffer);
}