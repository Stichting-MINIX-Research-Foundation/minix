#include "syslib.h"
#include <string.h>
#include <unistd.h>

#include <sys/ucred.h>

pid_t
getepinfo(endpoint_t proc_ep, uid_t *uid, gid_t *gid)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.m_lsys_pm_getepinfo.endpt = proc_ep;

	if ((r = _taskcall(PM_PROC_NR, PM_GETEPINFO, &m)) < 0)
		return r;

	if (uid != NULL)
		*uid = m.m_pm_lsys_getepinfo.uid;
	if (gid != NULL)
		*gid = m.m_pm_lsys_getepinfo.gid;
	return (pid_t) r;
}

pid_t
getnpid(endpoint_t proc_ep)
{
	return getepinfo(proc_ep, NULL, NULL);
}

uid_t
getnuid(endpoint_t proc_ep)
{
	uid_t uid;
	int r;

	if ((r = getepinfo(proc_ep, &uid, NULL)) < 0)
		return (uid_t) r;

	return uid;
}

gid_t
getngid(endpoint_t proc_ep)
{
	gid_t gid;
	int r;

	if ((r = getepinfo(proc_ep, NULL, &gid)) < 0)
		return (gid_t) r;

	return gid;
}
