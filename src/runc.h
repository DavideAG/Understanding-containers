#define STACK_SIZE (1024 * 1024)
#define COMAND_MAX_SIZE 200

/* called when "run" command is done.
   This function will run a new container*/
void runc(int n_values, char *command_input[]);

/* used to boot up the container process */
int bootstrap_container(void *buf);

/* set your new hostname */
void set_container_hostname();