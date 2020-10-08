#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "runc.h"
#include "helpers/helpers.h"


int main(int argc, char *argv[]) {

	int option = 0;

    	if (argc < N_PARAMS)
		printUsage();

	while ((option = getopt(argc, argv, "anu")) != -1) {
		switch(option) {
			case 'a':
				debug_print("case all");
				runc(argc, argv);
				break;
			case 'n':
				debug_print("case network");
				break;
			case 'u':
				debug_print("case user");
				break;
	
				// add here other cases

			default:
				goto usage;	
		}
	}

	exit(EXIT_SUCCESS);

usage:
	printf("usage case");

}
