#include "inc.h"

int
check_perm(struct ipc_perm * req, endpoint_t who, int mode)
{
	int req_mode;
	int cur_mode;
	uid_t uid;
	gid_t gid;

	uid = getnuid(who);
	gid = getngid(who);
	mode &= 0666;

	/* Root is allowed to do anything. */
	if (uid == 0)
		return 1;

	if (uid == req->uid || uid == req->cuid) {
		/* Same user. */
		req_mode = (req->mode >> 6) & 0x7;
		cur_mode = (mode >> 6) & 0x7;
	} else if (gid == req->gid || gid == req->cgid) {
		/* Same group. */
		req_mode = (req->mode >> 3) & 0x7;
		cur_mode = (mode >> 3) & 0x7;
	} else {
		/* Other user and group. */
		req_mode = req->mode & 0x7;
		cur_mode = mode & 0x7;
	}

	return (cur_mode && ((cur_mode & req_mode) == cur_mode));
}
