#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#define DEBUG 0

static int _tcp_shutdown(int sock, int how);
static int _uds_shutdown(int sock, int how);

/*
 * Shut down socket send and receive operations.
 */
static int
__shutdown(int fd, int how)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_shutdown.fd = fd;
	m.m_lc_vfs_shutdown.how = how;

	return _syscall(VFS_PROC_NR, VFS_SHUTDOWN, &m);
}

int shutdown(int sock, int how)
{
	int r;
	struct sockaddr_un uds_addr;
	nwio_tcpconf_t tcpconf;

	r = __shutdown(sock, how);
	if (r != -1 || (errno != ENOTSOCK && errno != ENOSYS))
		return r;

	r= ioctl(sock, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _tcp_shutdown(sock, how);
	}

	r= ioctl(sock, NWIOGUDSADDR, &uds_addr);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _uds_shutdown(sock, how);
	}

	errno = ENOTSOCK;
	return -1;
}

static int _tcp_shutdown(int sock, int how)
{
	int r;

	if (how == SHUT_WR || how == SHUT_RDWR)
	{
		r= ioctl(sock, NWIOTCPSHUTDOWN, NULL);
		if (r == -1)
			return -1;
		if (how == SHUT_WR)
			return 0;
	}

	/* We can't shutdown the read side of the socket. */
	errno= ENOSYS;
	return -1;
}

static int _uds_shutdown(int sock, int how)
{
	return ioctl(sock, NWIOSUDSSHUT, &how);
}
