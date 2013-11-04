#include "syslib.h"
#include <unistd.h>
#include <string.h>

int
getprocnr(pid_t pid, endpoint_t *proc_e)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.PM_GETPROCNR_PID = pid;

	if ((r = _taskcall(PM_PROC_NR, PM_GETPROCNR, &m)) < 0)
		return r;

	*proc_e = m.PM_GETPROCNR_ENDPT;
	return r;
}
