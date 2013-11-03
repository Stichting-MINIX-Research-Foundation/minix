#include "syslib.h"

#include <string.h>

int
srv_kill(pid_t pid, int sig)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m1_i1 = pid;
	m.m1_i2 = sig;
	return _taskcall(PM_PROC_NR, SRV_KILL, &m);
}
