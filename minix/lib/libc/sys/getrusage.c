#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

int
getrusage(int who, struct rusage * r_usage)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_pm_rusage.who = who;
	m.m_lc_pm_rusage.addr = (vir_bytes)r_usage;

	return _syscall(PM_PROC_NR, PM_GETRUSAGE, &m);
}
