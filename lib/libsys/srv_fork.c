#include "syslib.h"

#include <string.h>

pid_t
srv_fork(uid_t reuid, gid_t regid)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_pm_srv_fork.uid = reuid;
	m.m_lsys_pm_srv_fork.gid = regid;
	return _taskcall(PM_PROC_NR, PM_SRV_FORK, &m);
}
