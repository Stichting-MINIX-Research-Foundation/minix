#include "syslib.h"

#include <string.h>

int
copyfd(endpoint_t endpt, int fd, int what)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vfs_copyfd.endpt = endpt;
	m.m_lsys_vfs_copyfd.fd = fd;
	m.m_lsys_vfs_copyfd.what = what;

	return _taskcall(VFS_PROC_NR, VFS_COPYFD, &m);
}
