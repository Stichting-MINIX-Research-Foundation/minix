#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>
#include <sys/resource.h>

int getrusage(int who, struct rusage *r_usage)
{
	int rc;
	message m;
	m.RU_WHO = who;
	m.RU_RUSAGE_ADDR = r_usage;

	memset(r_usage, 0, sizeof(struct rusage));
	if ((rc = _syscall(PM_PROC_NR, GETRUSAGE, &m)) < 0)
		return rc;
	m.m1_p1 = r_usage;
	if ((rc = _syscall(VFS_PROC_NR, GETRUSAGE, &m)) < 0)
		return rc;
	m.m1_p1 = r_usage;
	return _syscall(VM_PROC_NR, VM_GETRUSAGE, &m);
}
