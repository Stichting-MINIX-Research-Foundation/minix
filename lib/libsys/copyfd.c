#include "syslib.h"

#include <string.h>

int
copyfd(endpoint_t endpt, int fd, int what)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VFS_COPYFD_ENDPT = endpt;
	m.VFS_COPYFD_FD = fd;
	m.VFS_COPYFD_WHAT = what;

	return _taskcall(VFS_PROC_NR, COPYFD, &m);
}
