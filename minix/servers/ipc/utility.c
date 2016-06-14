#include "inc.h"

int
check_perm(struct ipc_perm * req, endpoint_t who, int mode)
{
	int req_mode;
	uid_t uid;
	gid_t gid;

	uid = getnuid(who);
	gid = getngid(who);
	mode &= 0700;

	/* Root is allowed to do anything. */
	if (uid == 0)
		return TRUE;

	if (uid == req->uid || uid == req->cuid) {
		/* Same user. */
		req_mode = req->mode & 0700;
	} else if (gid == req->gid || gid == req->cgid) {
		/* Same group. */
		req_mode = req->mode & 0070;
		mode >>= 3;
	} else {
		/* Other user and group. */
		req_mode = req->mode & 0007;
		mode >>= 6;
	}

	return (mode && ((mode & req_mode) == mode));
}

/*
 * Copy over an ipc_perm structure to an ipc_perm_sysctl structure.
 */
void
prepare_mib_perm(struct ipc_perm_sysctl * perms, const struct ipc_perm * perm)
{

	memset(perms, 0, sizeof(*perms));
	perms->_key = perm->_key;
	perms->uid = perm->uid;
	perms->gid = perm->gid;
	perms->cuid = perm->cuid;
	perms->cgid = perm->cgid;
	perms->mode = perm->mode;
	perms->_seq = perm->_seq;
}
