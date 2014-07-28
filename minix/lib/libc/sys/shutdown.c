#include <sys/cdefs.h>
#include "namespace.h"

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

int shutdown(int sock, int how)
{
	int r;
	struct sockaddr_un uds_addr;
	nwio_tcpconf_t tcpconf;

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

#if DEBUG
	fprintf(stderr, "shutdown: not implemented for fd %d\n", sock);
#endif
	errno= ENOSYS;
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
