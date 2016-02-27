#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

/*
 * Put a socket in listening mode.
 */
static int
__listen(int fd, int backlog)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_listen.fd = fd;
	m.m_lc_vfs_listen.backlog = backlog;

	return _syscall(VFS_PROC_NR, VFS_LISTEN, &m);
}

int listen(int sock, int backlog)
{
	int r;

	r = __listen(sock, backlog);
	if (r != -1 || (errno != ENOTSOCK && errno != ENOSYS))
		return r;

	r= ioctl(sock, NWIOTCPLISTENQ, &backlog);
	if (r != -1 || errno != ENOTTY)
		return r;

	r= ioctl(sock, NWIOSUDSBLOG, &backlog);
	if (r != -1 || errno != ENOTTY)
		return r;

	errno = ENOTSOCK;
	return -1;
}
