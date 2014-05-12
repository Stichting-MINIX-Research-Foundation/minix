#include "syslib.h"
#include <string.h>
#include <unistd.h>

#include <sys/ucred.h>

static pid_t
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

int
getnucred(endpoint_t proc_ep, struct uucred *ucred)
{
	uid_t uid;
	gid_t gid;
	int r;

	if (ucred == NULL)
		return EFAULT;

	if ((r = getepinfo(proc_ep, &uid, &gid)) < 0)
		return r;

	/* Only two fields are used for now; ensure the rest is zeroed out. */
	memset(ucred, 0, sizeof(struct uucred));
	ucred->cr_uid = uid;
	ucred->cr_gid = gid;

	return r;
}
