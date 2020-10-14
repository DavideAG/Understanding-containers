#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cgroup.h"
#include "../../helpers/helpers.h"
#include "../../../config.h"

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

struct cgrp_control *alloc_ctr_memory(char *memory_limit)
{
        struct cgrp_setting *memory_ker = NULL;
        struct cgrp_setting *memory_usr = NULL;
        struct cgrp_control *ctr_memory = NULL;
        size_t n_settings = 0;

        /* creating the two settings structures */
        memory_usr =
            (struct cgrp_setting *) malloc(sizeof(struct cgrp_setting));
        
        if (!memory_usr) {
            printErr("alloc_ctr_memory malloc");
        }

        memory_usr->name = strdup("memory.limit_in_bytes");
        memory_usr->value = memory_limit;
        ++n_settings;

        memory_ker =
            (struct cgrp_setting *) malloc(sizeof(struct cgrp_setting));

        if (!memory_ker) {
            printErr("alloc_ctr_memory malloc");
        }

        memory_ker->name = strdup("memory.kmem.limit_in_bytes");
        memory_ker->value = memory_limit;
        ++n_settings;

        /* now the controller */
        ctr_memory = (struct cgrp_control *) malloc(sizeof(struct cgrp_control));

        if (!ctr_memory) {
            printErr("alloc_ctr_memory malloc");
        }

        ctr_memory->control = strdup("memory");

        if (!ctr_memory->control) {
            printErr("alloc_ctr_memory malloc");
        }

        ctr_memory->settings =
            (struct cgrp_setting **) malloc(2 * sizeof(struct cgrp_setting *));
        
        if (!ctr_memory->settings) {
            printErr("alloc_ctr_memory malloc");
        }

        /* pushing the two setting structures */
        ctr_memory->settings[0] = memory_usr;
        ctr_memory->settings[1] = memory_ker;
        ctr_memory->n_settings = n_settings;

        return ctr_memory;
}

struct cgrp_control *alloc_ctr_cpu(char *cpu_shares)
{
    struct cgrp_control *ctr_cpu = NULL;
    struct cgrp_setting *cpu = NULL;
    size_t n_settings = 0;

    cpu = (struct cgrp_setting *) malloc(sizeof(struct cgrp_setting));

    if (!cpu) {
        printErr("alloc_ctr_cpu malloc");
    }

    cpu->name = strdup("cpu.shares");
    cpu->value = cpu_shares;
    ++n_settings;

    ctr_cpu = (struct cgrp_control *) malloc(sizeof(struct cgrp_control));

    if (!ctr_cpu) {
        printErr("alloc_ctr_cpu malloc");
    }

    ctr_cpu->settings = (struct cgrp_setting **)
        malloc(sizeof(struct cgrp_setting *));
    
    if (!ctr_cpu->settings) {
        printErr("alloc_ctr_cpu malloc");
    }

    ctr_cpu->control = strdup("cpu");
    ctr_cpu->settings[0] = cpu;
    ctr_cpu->n_settings = n_settings;

    return ctr_cpu;
}

struct cgrp_control *alloc_ctr_pids(char *max_pids)
{
    struct cgrp_control *ctr_pids = NULL;
    struct cgrp_setting *pids = NULL;
    size_t n_settings = 0;

    pids = (struct cgrp_setting *) malloc(sizeof(struct cgrp_setting));

    if (!pids) {
        printErr("alloc_ctr_pids malloc");
    }

    pids->name = strdup("pids.max");
    pids->value = max_pids;
    ++n_settings;

    ctr_pids = (struct cgrp_control *) malloc(sizeof(struct cgrp_control));

    if (!ctr_pids) {
        printErr("alloc_ctr_pids malloc");
    }

    ctr_pids->settings = (struct cgrp_setting **)
        malloc(sizeof(struct cgrp_setting *));
    
    if (!ctr_pids->settings) {
        printErr("alloc_ctr_pids malloc");
    }

    ctr_pids->control = strdup("pids");
    ctr_pids->settings[0] = pids;
    ctr_pids->n_settings = n_settings;

    return ctr_pids;
}

struct cgrp_control *alloc_ctr_blkio(char *io_weight)
{
    struct cgrp_control *ctr_blkio = NULL;
    struct cgrp_setting *blkio = NULL;
    size_t n_settings = 0;

    blkio = (struct cgrp_setting *) malloc(sizeof(struct cgrp_setting));

    if (!blkio) {
        printErr("alloc_ctr_blkio malloc");
    }

    blkio->name = strdup("blkio.weight");
    blkio->value = io_weight;
    ++n_settings;

    ctr_blkio = (struct cgrp_control *) malloc(sizeof(struct cgrp_control));

    if (!ctr_blkio) {
        printErr("alloc_ctr_blkio malloc");
    }

    ctr_blkio->settings = (struct cgrp_setting **)
        malloc(sizeof(struct cgrp_setting *));
    
    if (!ctr_blkio->settings) {
        printErr("alloc_ctr_blkio malloc");
    }

    ctr_blkio->control = strdup("blkio");
    ctr_blkio->settings[0] = blkio;
    ctr_blkio->n_settings = n_settings;

    return ctr_blkio;
}


/* create a new cgroup controller */
struct cgrp_control **setup_cgrp_controller(
    struct cgroup_args *cgroup_arguments,
    size_t *n_controller)
{
    struct cgrp_control **controller = NULL;
    struct cgrp_control *ctr_memory = NULL;
    struct cgrp_control *ctr_blkio = NULL;
    struct cgrp_control *ctr_pids = NULL;
    struct cgrp_control *ctr_cpu = NULL;
    int n_controllers = 0, k = 0;

    if (cgroup_arguments->has_memory_limit) {
        ++n_controllers;
        ctr_memory = alloc_ctr_memory(cgroup_arguments->memory_limit);
    }

    if (cgroup_arguments->has_cpu_shares) {
        ++n_controllers;
        ctr_cpu = alloc_ctr_cpu(cgroup_arguments->cpu_shares);
    }

    if (cgroup_arguments->has_max_pids) {
        ++n_controllers;
        ctr_pids = alloc_ctr_pids(cgroup_arguments->max_pids);
    }

    if (cgroup_arguments->has_io_weight) {
        ++n_controllers;
        ctr_blkio = alloc_ctr_blkio(cgroup_arguments->io_weight);
    }

    controller = (struct cgrp_control **) 
        malloc(n_controllers * sizeof(struct cgrp_control *));

    if (!controller) {
        printErr("controller failed allocation");
    }

    if (ctr_memory) {
        controller[k++] = ctr_memory;
    }

    if (ctr_cpu) {
        controller[k++] = ctr_cpu;
    }

    if (ctr_pids) {
        controller[k++] = ctr_pids;
    }

    if (ctr_blkio) {
        controller[k++] = ctr_blkio;
    }
    
    *n_controller = n_controllers;
    return controller;
}

void write_writing_process_task(char dir[BUFF_LEN])
{
    char file_name[BUFF_LEN];
    char *value = NULL;
    int fd = 0;

    if (snprintf(file_name, sizeof(file_name), "%s/tasks", dir) == -1) {
        printErr("snprintf at write_writing_process_task");
    }

    if ((fd = open(file_name, O_WRONLY)) == -1) {
        printErr("open at write_writing_process_task");
    }

    value = strdup("0");

    if (write(fd, value, strlen(value)) == -1) {
        close(fd);
        free(value);
        printErr("writing the cgroup tasks file");
    }

    close(fd);
    free(value);
}

/* For each controller a new directory under
 * /sys/fs/cgroup/<cgrp_control.control>/container/ is created,
 * here a new file containing the resource limitation represented
 * by cgrp_setting is created and the associated value is written. */
void setting_cgroups(struct cgrp_control **controller, size_t n_controller)
{
    int i = 0, j = 0;
    struct cgrp_control **cgrp;
    struct cgrp_setting **setting;
    fprintf(stderr, "=> setting cgroups...");

    for (i = 0, cgrp = controller; i < n_controller; cgrp++, i++) {
        char dir[BUFF_LEN] = { 0 };

        if (snprintf(dir, sizeof(dir), "/sys/fs/cgroup/%s/" HOSTNAME,
                (*cgrp)->control) == -1) {
            printErr("snprintf at setting_cgroups");
        }
        
        if (mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR)) {
            printErr("mkdir at setting_cgroups");
        }

        for (j = 0, setting = (*cgrp)->settings;
                j < (*cgrp)->n_settings;
                setting++, j++) {
            char path[BUFF_LEN] = {0};
            int fd = 0;
            
            if (snprintf(path, sizeof(path), "%s/%s", dir,
                    (*setting)->name) == -1) {
                printErr("snprintf at setting_cgroups");
            }

            if ((fd = open(path, O_WRONLY)) == -1) {
                printErr("open at setting_cgroups");
            }

            if (write(fd, (*setting)->value, strlen((*setting)->value)) == -1) {
                close(fd);
                printErr("writing the cgroup file");
            }
            printf("\nopened: %s\nwrote: %s\n", path, (*setting)->value);
            close(fd);
        }

        write_writing_process_task(dir);
    }
    fprintf(stderr, "done.\n");
}

void apply_cgroups(struct cgroup_args *cgroup_arguments)
{
    struct cgrp_control **controller = NULL;
    size_t n_controller = 0;

    controller = setup_cgrp_controller(cgroup_arguments, &n_controller);

    // The hostname will be 'mydocker', actually it is static.
    setting_cgroups(controller, n_controller);

    //TODO: remember to free the controller and related setting structure
}