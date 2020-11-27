#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "runc.h"
#include "helpers/helpers.h"
#include "namespaces/cgroup/cgroup.h"


int main(int argc, char *argv[])
{
	int option = 0;
	bool empty = false;
	bool runall = false;
	bool pids_flag = false;
	bool has_userns = false;
	bool cgroup_flag = false;
	bool memory_flag = false;
	bool weight_flag = false;
	bool cpu_shares_flag = false;
	long max_pids = 0;
	long max_weight = 0;
	long cpu_shares = 0;
	long memory_limit = 0;
	char **child_entrypoint;
	struct runc_args *runc_arguments = NULL;
	struct cgroup_args *cgroup_arguments = NULL;

	while ((option = getopt(argc, argv, "hacUM:C:P:I:")) != -1) {
		switch(option) {
			case 'h':
				debug_print("case help\n");
				goto usage;
			
			case 'a':
				debug_print("case all\n");
				runall = true;
				break;

			case 'U':
				has_userns = true;
				break;

			case 'c':
				debug_print("case cgroup\n");
				cgroup_flag = true;
				break;

			case 'M':
				debug_print("case memory limit\n");
				empty = (bool) !strcmp(optarg, "");

				if (cgroup_flag && !empty)
				{
					memory_limit = strtol(optarg, NULL, 10);
					empty != empty;

					if (memory_limit < 1 ||
						memory_limit > MAX_MEMORY_ALLOCABLE) {
						printErr("memory_limit value out of range");
						goto abort;
					}

				} else {
					printErr("-M must be chained with -c and the argument "
					"cannot be empty. \nCommand");
					goto abort;
				}
				memory_flag = true;
				break;

			case 'C':
				debug_print("case cpu shares\n");
				empty = (bool) !strcmp(optarg, "");

				if (cgroup_flag && !empty)
				{
					cpu_shares = strtol(optarg, NULL, 10);
					empty != empty;

					if (cpu_shares < 1 || cpu_shares > MAX_CPU_SHARES) {
						printErr("cpu_shares value out of range");
						goto abort;
					}

				} else {
					printErr("-C must be chained with -c and the argument "
					"cannot be empty. \nCommand");
					goto abort;
				}
				cpu_shares_flag = true;
				break;

			case 'P':
				debug_print("case pids\n");
				empty = (bool) !strcmp(optarg, "");

				if (cgroup_flag && !empty)
				{
					max_pids = strtol(optarg, NULL, 10);
					empty != empty;

					if (max_pids < MIN_PIDS || max_pids > MAX_PIDS) {
						printErr("max_pids value out of range");
						goto abort;
					}

				} else {
					printErr("-C must be chained with -c and the argument "
					"cannot be empty. \nCommand");
					goto abort;
				}
				pids_flag = true;
				break;

			case 'I':
				debug_print("case io weight\n");
				empty = (bool) !strcmp(optarg, "");

				if (cgroup_flag && !empty)
				{
					max_weight = strtol(optarg, NULL, 10);
					empty != empty;

					if (max_weight < MIN_WEIGHT || max_weight > MAX_WEIGHT) {
						printErr("io_weight value out of range");
						goto abort;
					}

				} else {
					printErr("-C must be chained with -c and the argument "
					"cannot be empty. \nCommand");
					goto abort;
				}
				weight_flag = true;
				break;

				// add other cases here

			default:
				printf("Command not found.\n\n");
				goto usage;	
		}
	}

	get_child_entrypoint(optind, argv, argc, &child_entrypoint);

	init_resources(cgroup_flag, pids_flag, memory_flag, weight_flag,
			cpu_shares_flag, max_pids, memory_limit, max_weight,
			cpu_shares, &cgroup_arguments);

	runc_arguments = (struct runc_args *) malloc( sizeof(struct runc_args) );
	runc_arguments->child_entrypoint = child_entrypoint;
	runc_arguments->child_entrypoint_size = (size_t) argc - optind;
	runc_arguments->resources = cgroup_arguments;

	// privileged or unprivileged container
	runc_arguments->has_userns = has_userns;

	if (runall) {
		runc(runc_arguments);
	} else {
		fprintf(stderr, "-a flag must be used in order to create "
		"your new container");
		goto abort;
	}
		
	// Now it's time to free them. Bye!
	if (runc_arguments->resources) {
		free(runc_arguments->resources->max_pids);
		free(runc_arguments->resources->io_weight);
		free(runc_arguments->resources->cpu_shares);
		free(runc_arguments->resources->memory_limit);
		free(runc_arguments->resources);
	}
	
	for (int i = 0; i < argc - optind; ++i) {
		free(child_entrypoint[i]);
	}
	free(child_entrypoint);

	exit(EXIT_SUCCESS);


usage:
	printf("Usage: sudo ./MyDocker <options> <entrypoint>\n");
	printf("\n");
	printf("<options> should be:\n");
	printf("\t- a\trun all namespaces without the user namespace\n");
	printf("\t- U\trun a user namespace using unprivileged container\n");
	printf("\t- c\tcgrops used to limit resources.\n\t\tThis command must "
	"be chained with at least one of:\n");
	printf("\t\t- M <memory_limit> \t\t[1-4294967296]\t"
	"default: 1073741824 (1GB)\n");
	printf("\t\t- C <percentage_of_cpu_shares> \t[1-100]\t\tdefault: 25\n");
	printf("\t\t- P <max_pids> \t\t\t[10-32768]\tdefault: 64\n");
	printf("\t\t- I <io_weighht> \t\t[10-1000]\tdefault: 10\n");
	exit(EXIT_FAILURE);

abort:
	printf("\n\n[ABORT]\n");
	exit(EXIT_FAILURE);
}
