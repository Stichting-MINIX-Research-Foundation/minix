#include "syslib.h"
#include <unistd.h>
#include <string.h>

int
getprocnr(pid_t pid, endpoint_t *proc_e)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.m_lsys_pm_getprocnr.pid = pid;

	if ((r = _taskcall(PM_PROC_NR, PM_GETPROCNR, &m)) < 0)
		return r;

	*proc_e = m.m_pm_lsys_getprocnr.endpt;
	return r;
}
