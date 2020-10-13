#include <stdio.h>
#include <stdlib.h>

#define BUFF_LEN	256
#define FD_COUNT	64
#define MEMORY		"1073741824"     // memory limit to 1GB in userspace
#define SHARES		"256"            // cpu shares
#define PIDS		"64"             // max pids for the containered process
#define WEIGHT		"10"             // the default weight on all io devices.
									 // A per device rule can be defined.
									 // It defines a throttling/upper IO rate
									 //limit on devices.

typedef enum {false, true} bool;

/* This structure contains all information about the
 * resources limitations that will be applied in the
 * cgrop namespace of the process.
 * Metrics should be reported as string. */
struct cgroup_args {
	bool has_max_pids;			/* max_pids flag */
	bool has_io_weight;			/* io_weight flag */
	bool has_memory_limit;		/* memory_limit flag */
	bool has_cpu_shares;		/* max_pids flag */
    char *max_pids;				/* max pids limit */
    char *io_weight;			/* io default weight limit */
	char *memory_limit;			/* memory limit. Kernel and user space both */
    char *cpu_shares;			/* cpu shares chunks value */
};

/* This structure represents the file that is written inside
 * /sys/fs/cgroup/<cgrp_control.control>/mydocker/ 
 * the name of the file is cgrp_setting.name and the value that is written
 * inside is store inside cgrp_setting.value */
struct cgrp_setting {
	char *name;
	char *value;
};

/* Represents the directory that will be created in /sys/fs/cgroup/
 * each controller is dedicated for a certain type of resource, for example
 * the maximum userspace memory that can be allocated by the process. */
struct cgrp_control {
	char *control;
	struct cgrp_setting **settings;
};

/*
struct cgrp_control *cgrps[] = {
	& (struct cgrp_control) {
		.control = "memory",
		.settings = (struct cgrp_setting *[]) {
			& (struct cgrp_setting) {
				.name = "memory.limit_in_bytes",
				.value = MEMORY
			},
			& (struct cgrp_setting) {
				.name = "memory.kmem.limit_in_bytes",
				.value = MEMORY
			},
			&add_to_tasks,
			NULL
		}
	},
	& (struct cgrp_control) {
		.control = "cpu",
		.settings = (struct cgrp_setting *[]) {
			& (struct cgrp_setting) {
				.name = "cpu.shares",
				.value = SHARES
			},
			&add_to_tasks,
			NULL
		}
	},
	& (struct cgrp_control) {
		.control = "pids",
		.settings = (struct cgrp_setting *[]) {
			& (struct cgrp_setting) {
				.name = "pids.max",
				.value = PIDS
			},
			&add_to_tasks,
			NULL
		}
	},
	& (struct cgrp_control) {
		.control = "blkio",
		.settings = (struct cgrp_setting *[]) {
			& (struct cgrp_setting) {
				.name = "blkio.weight",
				.value = PIDS
			},
			&add_to_tasks,
			NULL
		}
	},
	NULL
};
*/

/* initialize the struct cgroup_args */
void init_resources(bool cgroup_flag, bool pids_flag, bool memory_flag,
			bool weight_flag, bool cpu_shares_flag,
			long max_pids, long memory_limit, long max_weight,
			long cpu_shares, struct cgroup_args **cgroup_arguments);

/* apply a specific resource configuration for the containered process */
void apply_cgroups(struct cgroup_args *cgroup_arguments);