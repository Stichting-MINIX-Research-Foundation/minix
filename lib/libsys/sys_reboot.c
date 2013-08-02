#include "syslib.h"

int
sys_reboot(int how)
{
	message m;

	m.m1_i1 = how;

	return _taskcall(PM_PROC_NR, REBOOT, &m);
}
