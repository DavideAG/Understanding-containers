#include <errno.h>
#define N_PARAMS 2
#define BUFF_SIZE 300
#define MAP_BUF_SIZE 100


/* commands */
#define RUN 1
#define RUN_COMMAND "run"
#define CHILD 2
#define CHILD_COMMAND "child"
#define COMMAND_NOT_FOUND -1

#define printErr(msg)   do { \
                            fprintf(stdout, "[ERROR] - %s failed (errno: %d): %s\n", msg, errno, strerror(errno)); \
                            exit(EXIT_FAILURE); \
                        } while (0)

/* clone args structure */
struct clone_args {
   unsigned int n_args;
   char **argv;
   int pipe_fd[2];  /* Pipe used to synchronize parent and child */
};

/* print the usage of the tool */
void printUsage();

/* dispatch che input command */
int parser(char *command);

/* print the container pid and command name */
void printRunningInfos(struct clone_args *args);