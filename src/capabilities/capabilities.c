#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include "capabilities.h"
#include "../helpers/helpers.h"

int drop_caps()
{
	fprintf(stderr, "=> dropping capabilities...");
    int drop_caps[] = {
        CAP_NET_BROADCAST,
        CAP_SYS_MODULE,
        CAP_SYS_RAWIO,
        CAP_SYS_PACCT,
        CAP_SYS_ADMIN,
        CAP_SYS_NICE,
        CAP_SYS_RESOURCE,
        CAP_SYS_TIME,
        CAP_SYS_TTY_CONFIG,
        CAP_AUDIT_CONTROL,
        CAP_MAC_OVERRIDE,
        CAP_MAC_ADMIN,
        CAP_NET_ADMIN,
        CAP_SYSLOG,
        CAP_DAC_READ_SEARCH,
        CAP_LINUX_IMMUTABLE,
        CAP_IPC_LOCK,
        CAP_IPC_OWNER,
        CAP_SYS_PTRACE,
        CAP_SYS_BOOT,
        CAP_LEASE,
        CAP_WAKE_ALARM,
        CAP_BLOCK_SUSPEND,
        CAP_MKNOD,
        CAP_AUDIT_READ,
        CAP_AUDIT_WRITE,
        CAP_FSETID,
        CAP_SETFCAP
	};

	size_t num_caps = sizeof(drop_caps) / sizeof(*drop_caps);
	fprintf(stderr, "bounding...");
	
    for (size_t i = 0; i < num_caps; i++) {
		if (prctl(PR_CAPBSET_DROP, drop_caps[i], 0, 0, 0)) {
			printErr("prctl");
		}
	}

	fprintf(stderr, "inheritable...");
    cap_t caps = NULL;

	if (!(caps = cap_get_proc())
	    || cap_set_flag(caps, CAP_INHERITABLE, num_caps, drop_caps, CAP_CLEAR)
	    || cap_set_proc(caps)) {
		if (caps) cap_free(caps);
        printErr("inheritable");
    }
	cap_free(caps);
	fprintf(stderr, "done.\n");
}
