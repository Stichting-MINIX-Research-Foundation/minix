/*
setgroups.c
*/

#include <errno.h>
#include <unistd.h>

int setgroups(int ngroups, const gid_t *gidset)
{
	/* Not implemented */

	errno= ENOSYS;
	return -1;
}

