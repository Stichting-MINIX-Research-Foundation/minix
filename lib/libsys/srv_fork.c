#include "syslib.h"

#include <string.h>

pid_t
srv_fork(uid_t reuid, gid_t regid)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.PM_SRV_FORK_UID = (int) reuid;
	m.PM_SRV_FORK_GID = (int) regid;
	return _taskcall(PM_PROC_NR, PM_SRV_FORK, &m);
}
