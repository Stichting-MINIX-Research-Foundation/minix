#include "syslib.h"

#include <string.h>

int
srv_kill(pid_t pid, int sig)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_rs_pm_srv_kill.pid = pid;
	m.m_rs_pm_srv_kill.nr = sig;
	return _taskcall(PM_PROC_NR, PM_SRV_KILL, &m);
}
