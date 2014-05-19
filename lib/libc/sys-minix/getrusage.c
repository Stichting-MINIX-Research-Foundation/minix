#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

int getrusage(int who, struct rusage *r_usage)
{
	int rc;
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_pm_rusage.who = who;
	m.m_lc_pm_rusage.addr = (vir_bytes)r_usage;

	if (r_usage == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN) {
		errno = EINVAL;
		return -1;
	}

	memset(r_usage, 0, sizeof(struct rusage));
	if ((rc = _syscall(PM_PROC_NR, PM_GETRUSAGE, &m)) < 0)
		return rc;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_rusage.addr = (vir_bytes)r_usage;
	if ((rc = _syscall(VFS_PROC_NR, VFS_GETRUSAGE, &m)) < 0)
		return rc;

	memset(&m, 0, sizeof(m));
	m.m_lc_vm_rusage.addr = (vir_bytes)r_usage;
	return _syscall(VM_PROC_NR, VM_GETRUSAGE, &m);
}
