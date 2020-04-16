#define _GNU_SOURCE
#define STACK_SIZE (1024 * 1024)

/* callend when "run" command is done.
   This function will run a new container*/
void runc(int n_values, char *command_input[]);

/* child function handler */
void child_fn(int n_values, char *command_input[]);

/* used to concatenate a command */
char *concat_command(char *first, char *second);