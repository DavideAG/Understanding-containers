#define N_PARAMS 2

/* commands */
#define RUN 1
#define RUN_COMMAND "run"
#define CHILD 2
#define CHILD_COMMAND "child"
#define COMMAND_NOT_FOUND -1

#define printErr(msg)   do { \
                            fprintf(stdout, "[ERROR] - %s\n", msg); \
                            exit(EXIT_FAILURE); \
                        } while (0)


/* print the usage of the tool */
void printUsage();

/* dispatch che input command */
int parser(char *command);