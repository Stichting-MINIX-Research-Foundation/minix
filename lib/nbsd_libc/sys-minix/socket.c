#include <sys/cdefs.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(socket, _socket)
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioc_net.h>

#include <net/netlib.h>
#include <netinet/in.h>

#define DEBUG 0

static int _tcp_socket(int protocol);
static int _udp_socket(int protocol);
static int _uds_socket(int type, int protocol);

int socket(int domain, int type, int protocol)
{
#if DEBUG
	fprintf(stderr, "socket: domain %d, type %d, protocol %d\n",
		domain, type, protocol);
#endif
	if (domain != AF_INET && domain != AF_UNIX)
	{
#if DEBUG
		fprintf(stderr, "socket: bad domain %d\n", domain);
#endif
		errno= EAFNOSUPPORT;
		return -1;
	}

	if (domain == AF_UNIX && (type == SOCK_STREAM ||
				type == SOCK_DGRAM || type == SOCK_SEQPACKET))
		return _uds_socket(type, protocol);

	if (domain == AF_INET && type == SOCK_STREAM)
		return _tcp_socket(protocol);

	if (domain == AF_INET && type == SOCK_DGRAM)
		return _udp_socket(protocol);

#if DEBUG
	fprintf(stderr, "socket: nothing for domain %d, type %d, protocol %d\n",
		domain, type, protocol);
#endif
	errno= EPROTOTYPE;
	return -1;
}

static int _tcp_socket(int protocol)
{
	int fd;
	if (protocol != 0 && protocol != IPPROTO_TCP)
	{
#if DEBUG
		fprintf(stderr, "socket(tcp): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}
	fd= open(TCP_DEVICE, O_RDWR);
	return fd;
}

static int _udp_socket(int protocol)
{
	int r, fd, t_errno;
	struct sockaddr_in sin;

	if (protocol != 0 && protocol != IPPROTO_UDP)
	{
#if DEBUG
		fprintf(stderr, "socket(udp): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}
	fd= open(UDP_DEVICE, O_RDWR);
	if (fd == -1)
		return fd;

	/* Bind is implict for UDP sockets? */
	sin.sin_family= AF_INET;
	sin.sin_addr.s_addr= INADDR_ANY;
	sin.sin_port= 0;
	r= bind(fd, (struct sockaddr *)&sin, sizeof(sin));
	if (r != 0)
	{
		t_errno= errno;
		close(fd);
		errno= t_errno;
		return -1;
	}
	return fd;
}

static int _uds_socket(int type, int protocol)
{
	int fd, r;
	if (protocol != 0)
	{
#if DEBUG
		fprintf(stderr, "socket(uds): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}

	fd= open(UDS_DEVICE, O_RDWR);
	if (fd == -1) {
		return fd;
	}

	/* set the type for the socket via ioctl (SOCK_DGRAM, 
	 * SOCK_STREAM, SOCK_SEQPACKET, etc)
	 */
	r= ioctl(fd, NWIOSUDSTYPE, &type);
	if (r == -1) {
		int ioctl_errno;

		/* if that failed rollback socket creation */
		ioctl_errno= errno;
		close(fd);

		/* return the error thrown by the call to ioctl */
		errno= ioctl_errno;
		return -1;
	}

	return fd;
}
