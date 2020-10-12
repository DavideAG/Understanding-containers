#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "runc.h"
#include "helpers/helpers.h"


int main(int argc, char *argv[]) {

	int option = 0;
	const char *errstr;
	char child_entrypoint[MAX_BUF_SIZE];
	bool empty = false;
	bool runall = false;
	bool pids_flag = false;
	bool cgroup_flag = false;
	bool memory_flag = false;
	bool weight_flag = false;
	bool cpu_shares_flag = false;
	long max_pids = 0;
	long max_weight = 0;
	long cpu_shares = 0;
	long memory_limit = 0;

	while ((option = getopt(argc, argv, "acM:C:P:I:")) != -1) {
		switch(option) {
			case 'a':
				debug_print("case all\n");
				runall = true;
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
				break;
			case 'C':
				debug_print("case cpu shares\n");
				empty = (bool) !strcmp(optarg, "");
				if (cgroup_flag && !empty)
				{
					cpu_shares = strtol(optarg, NULL, 10);
					if (cpu_shares < 1 || cpu_shares > MAX_CPU_SHARES) {
						printErr("cpu_shares value out of range");
						goto abort;
					}
					empty != empty;
				} else {
					printErr("-C must be chained with -c and the argument "
					"cannot be empty. \nCommand");
					goto abort;
				}
				break;
			case 'P':
				debug_print("case pids\n");
				empty = (bool) !strcmp(optarg, "");
				if (cgroup_flag && !empty)
				{
					max_pids = strtol(optarg, NULL, 10);
					if (max_pids < MIN_PIDS || max_pids > MAX_PIDS) {
						printErr("max_pids value out of range");
						goto abort;
					}
					empty != empty;
				} else {
					printErr("-C must be chained with -c and the argument "
					"cannot be empty. \nCommand");
					goto abort;
				}
				break;
			case 'I':
				debug_print("case io weight\n");
				empty = (bool) !strcmp(optarg, "");
				if (cgroup_flag && !empty)
				{
					max_weight = strtol(optarg, NULL, 10);
					if (max_weight < MIN_WEIGHT || max_weight > MAX_WEIGHT) {
						printErr("io_weight value out of range");
						goto abort;
					}
					empty != empty;
				} else {
					printErr("-C must be chained with -c and the argument "
					"cannot be empty. \nCommand");
					goto abort;
				}
				break;
			/* TODO:
			case 'n':
				debug_print("case network");
				break;
			case 'u':
				debug_print("case user");
				break;*/

				// add here other cases

			default:
				goto usage;	
		}
	}

	get_child_entrypoint(optind, argv, argc, child_entrypoint);
	debug_print("child entrypoint = ");
	debug_print(child_entrypoint);
	debug_print("\n");


	// TODO: remember to pass memorylimit, cpushares etc.. as struct is better
	if (runall)
		runc(argc, argv);
	else
	{
		fprintf(stderr, "-a flag must be used in order to create "
		"your new container");
		goto abort;
	}
		

	exit(EXIT_SUCCESS);

usage:
	printf("Usage: MyDocker <options> <entrypoint>\n");
    printf("\n");
    printf("<options> should be:\n");
    printf("\t- a\trun all namespaces\n");
	printf("\t- c\tcgrops used to limit resources.\n\t\tThis command must can "
	"be chained with at least one of:\n");
	printf("\t\t- M <memory_limit> \t\t[1-4294967296]\t"
	"default: 1073741824 (1GB)\n");
	printf("\t\t- C <percentage_of_cpu_shares> \t[1-100]\t\tdefault: 25\n");
	printf("\t\t- P <max_pids> \t\t\t[10-32768]\tdefault: 64\n");
	printf("\t\t- I <io_weighht> \t\t[10-1000]\tdefault: 10\n");
	printf(" ");
	exit(EXIT_FAILURE);

abort:
	printf("\n\n[ABORT]\n");
	exit(EXIT_FAILURE);
}
