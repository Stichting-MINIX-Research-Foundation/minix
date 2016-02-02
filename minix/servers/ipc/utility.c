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
