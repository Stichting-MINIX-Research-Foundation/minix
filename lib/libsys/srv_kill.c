#include "syslib.h"

#include <string.h>

int
srv_kill(pid_t pid, int sig)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.PM_SIG_PID = pid;
	m.PM_SIG_NR = sig;
	return _taskcall(PM_PROC_NR, PM_SRV_KILL, &m);
}
