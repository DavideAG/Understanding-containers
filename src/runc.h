#define STACK_SIZE (1024 * 1024)
#define COMAND_MAX_SIZE 200

/* callend when "run" command is done.
   This function will run a new container*/
void runc(int n_values, char *command_input[]);

/* child function handler */
int child_fn(void *buf);

/* used to concatenate a command */
char *concat_command(char *first, char *second);