#include <stdio.h>
#include <stdlib.h>

#define MEMORY "1073741824"     // memory limit to 1GB in userspace
#define SHARES "256"            // cpu shares
#define PIDS "64"               // max pids for the containered process
#define WEIGHT "10"             // the default weight on all io devices. a per device rule can be defined. It defines a throttling/upper IO rate limit on devices.
#define FD_COUNT 64

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

/* initialize the struct cgroup_args */
void init_resources(bool cgroup_flag, bool pids_flag, bool memory_flag,
			bool weight_flag, bool cpu_shares_flag,
			long max_pids, long memory_limit, long max_weight,
			long cpu_shares, struct cgroup_args **cgroup_arguments);