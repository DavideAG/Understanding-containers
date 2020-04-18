#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "helpers.h"

void printUsage() {
    printf("Usage: MyDocker <cmd> <params>\n");
    printf("\n");
    printf("<cmd> should be:\n");
    printf("\t- run\n");
    printf("\t- child\n");

    exit(EXIT_FAILURE);
}

int parser(char *command) {
    if (strcmp(command, RUN_COMMAND) == 0)
    {
        return RUN;
    } else if (strcmp(command, CHILD_COMMAND) == 0)
    {
        return CHILD;
    }
    
    return COMMAND_NOT_FOUND;
}

void printRunningInfos(struct clone_args *args) {
    fprintf(stdout, "ProcessID: %ld\n", (long) getpid());
    printf("Running [ ");
    for (int i = 0; i < args->n_args;) { printf("%s ", args->argv[i++]); }
    printf("]\n");
}