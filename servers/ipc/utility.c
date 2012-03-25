#include "inc.h"

int check_perm(struct ipc_perm *req, endpoint_t who, int mode)
{
	int req_mode;
	int cur_mode;
	uid_t uid = getnuid(who);
	gid_t gid = getngid(who);

	mode &= 0666;

	/* is root? */
	if (uid == 0)
		return 1;

	if (uid == req->uid || uid == req->cuid) {
		/* same user */
		req_mode = (req->mode >> 6) & 0x7;
		cur_mode = (mode >> 6) & 0x7;
	} else if (gid == req->gid || gid == req->cgid) {
		/* same group */
		req_mode = (req->mode >> 3) & 0x7;
		cur_mode = (mode >> 3) & 0x7;
	} else {
		/* other group */
		req_mode = req->mode & 0x7;
		cur_mode = mode & 0x7;
	}

	if (cur_mode && ((cur_mode & req_mode) == cur_mode))
		return 1;
	else
		return 0;
}

