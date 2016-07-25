#include "syslib.h"

#include <string.h>

/*
 * Non-blocking variant of close(2).  This call is to be used by system
 * services that need to close arbitrary local file descriptors.  The purpose
 * of the call is to avoid that such services end up blocking on closing socket
 * file descriptors with the SO_LINGER socket option enabled.  They cannot put
 * the file pointer in non-blocking mode to that end, because the file pointer
 * may be shared with other processes.
 *
 * Even though this call is defined for system services only, there is no harm
 * in letting arbitrary user processes use this functionality.  Thus, it needs
 * no separate VFS call number.
 */
int
closenb(int fd)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_close.fd = fd;
	m.m_lc_vfs_close.nblock = 1;

	return _taskcall(VFS_PROC_NR, VFS_CLOSE, &m);
}
