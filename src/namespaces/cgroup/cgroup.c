#include <string.h>
#include "cgroup.h"
#include "../../helpers/helpers.h"

/* initialize the struct cgroup_args */
void init_resources(bool cgroup_flag,
			bool pids_flag,
			bool memory_flag,
			bool weight_flag,
			bool cpu_shares_flag,
			long max_pids,
			long memory_limit,
			long max_weight,
			long cpu_shares,
			struct cgroup_args **cgroup_arguments)
{
    char buf[MAX_BUF_SIZE];

    if (!cgroup_flag) {
        goto abort;
    }

    *cgroup_arguments = (struct cgroup_args *) malloc(sizeof(struct cgroup_args));
    
    (*cgroup_arguments)->has_max_pids = pids_flag;
    (*cgroup_arguments)->has_io_weight = weight_flag;
    (*cgroup_arguments)->has_memory_limit = memory_flag;
    (*cgroup_arguments)->has_cpu_shares = cpu_shares_flag;

    buf[0] = '\0';
    if (pids_flag) {
        sprintf(buf, "%ld", max_pids);
    } else {
        strcpy(buf, PIDS);
    }
    (*cgroup_arguments)->max_pids = strdup(buf);

    buf[0] = '\0';
    if (weight_flag) {
        sprintf(buf, "%ld", max_weight);
    } else {
        strcpy(buf, WEIGHT);
    }
    (*cgroup_arguments)->io_weight = strdup(buf);

    buf[0] = '\0';
    if (memory_flag) {
        sprintf(buf, "%ld", memory_limit);
    } else {
        strcpy(buf, MEMORY);
    }
    (*cgroup_arguments)->memory_limit = strdup(buf);

    buf[0] = '\0';
    if (cpu_shares_flag) {
        sprintf(buf, "%ld", cpu_shares * 1024 / 100);
    } else {
        strcpy(buf, SHARES);
    }
    (*cgroup_arguments)->cpu_shares = strdup(buf);
    
    return;

abort:
    *cgroup_arguments = NULL;
}