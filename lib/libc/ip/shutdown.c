#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#define DEBUG 1

static int _tcp_shutdown(int socket, int how);

int shutdown(int socket, int how)
{
	int r;
	nwio_tcpconf_t tcpconf;

	r= ioctl(socket, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _tcp_shutdown(socket, how);
	}
#if DEBUG
	fprintf(stderr, "shutdown: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_shutdown(int socket, int how)
{
	int r;

	if (how == SHUT_WR || how == SHUT_RDWR)
	{
		r= ioctl(socket, NWIOTCPSHUTDOWN, NULL);
		if (r == -1)
			return -1;
		if (how == SHUT_WR)
			return 0;
	}

	/* We can't shutdown the read side of the socket. */
	errno= ENOSYS;
	return -1;
}


