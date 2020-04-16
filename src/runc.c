#define _GNU_SOURCE
#include <linux/sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "helpers.h"
#include "runc.h"

void runc(int n_values, char *command_input[]) {
    fprintf(stdout, "Running [ ");
    for (int i = 1; i < n_values-1;) {
        fprintf(stdout, "%s ", command_input[++i]);
    }
    fprintf(stdout, "]\n");

    /* child stack allocation */
    void *child_stack_bottom  = malloc(STACK_SIZE);
    if (!child_stack_bottom)
        printErr("child stack allocation");

    /* stack will grow from top to bottom */
    void *child_stack_top = child_stack_bottom + STACK_SIZE;
    
    int pid;
        fprintf(stdout, "prima di clone\n");
    pid = clone(child_fn, child_stack_top, CLONE_NEWUTS | SIGCHLD, n_values, command_input);
        fprintf(stdout, "dopo di clone\n");
    if (pid < 0)
        printErr("Unable to create child process");
    
    int status;
    if (wait(&status) == -1)
        printErr("wait");

}

void child_fn(int n_values, char *command_input[]) {
    /* Okay guys, we can do this better, I know! Now it just has to work. */
    char buffer[200];
    for (int i = 1; i < n_values-2; i++)
        strcat(buffer ,concat_command(command_input[i], command_input[i+1]));

    fprintf(stdout, "comando: %s", buffer);
    system(buffer);
}

char *concat_command(char *first, char *second) {
    char buffer[100];
    char *new_command;
    strcpy(buffer, first);
    strcat(buffer, " ");
    return strcat(buffer, second);
}