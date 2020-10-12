/* cgroup args structure */
struct cgroup_args {
    long memory_limit;
    long cpu_shares_percentage;
    long max_pids;
    long io_weight;
};

// --------------------------------------------------------

#define MEMORY "1073741824"     // memory limit to 1GB in userspace
#define SHARES "256"            // cpu shares
#define PIDS "64"               // max pids for the containered process
#define WEIGHT "10"             // the default weight on all io devices. a per device rule can be defined. It defines a throttling/upper IO rate limit on devices.
#define FD_COUNT 64

struct cgrp_control {
	char control[256];
	struct cgrp_setting {
		char name[256];
		char value[256];
	} **settings;
};
struct cgrp_setting add_to_tasks = {
	.name = "tasks",
	.value = "0"
};

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