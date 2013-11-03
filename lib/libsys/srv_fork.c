#include "syslib.h"

#include <string.h>

pid_t
srv_fork(uid_t reuid, gid_t regid)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m1_i1 = (int) reuid;
	m.m1_i2 = (int) regid;
	return _taskcall(PM_PROC_NR, SRV_FORK, &m);
}
