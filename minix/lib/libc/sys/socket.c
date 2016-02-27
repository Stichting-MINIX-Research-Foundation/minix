#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#ifdef __weak_alias
__weak_alias(socket, __socket30)
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <sys/ioc_net.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/ether.h>
#include <net/gen/eth_hdr.h>
#include <net/gen/eth_io.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/ip_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>
#include <net/gen/dhcp.h>

#include <net/netlib.h>
#include <netinet/in.h>

#define DEBUG 0

static int _tcp_socket(int type, int protocol);
static int _udp_socket(int type, int protocol);
static int _uds_socket(int type, int protocol);
static int _raw_socket(int type, int protocol);
static void _socket_flags(int type, int *result);

/*
 * Create a socket.
 */
static int
__socket(int domain, int type, int protocol)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_socket.domain = domain;
	m.m_lc_vfs_socket.type = type;
	m.m_lc_vfs_socket.protocol = protocol;

	return _syscall(VFS_PROC_NR, VFS_SOCKET, &m);
}

int socket(int domain, int type, int protocol)
{
	int r, sock_type;

	r = __socket(domain, type, protocol);
	if (r != -1 || (errno != EAFNOSUPPORT && errno != ENOSYS))
		return r;

	sock_type = type & ~SOCK_FLAGS_MASK;

#if DEBUG
	fprintf(stderr, "socket: domain %d, type %d, protocol %d\n",
		domain, type, protocol);
#endif

	if (domain == AF_UNIX)
		return _uds_socket(type, protocol);

	if (domain == AF_INET) {
		switch (sock_type) {
		case SOCK_STREAM:
			return _tcp_socket(type, protocol);
		case SOCK_DGRAM:
			return _udp_socket(type, protocol);
		case SOCK_RAW:
			return _raw_socket(type, protocol);
		default:
			errno = EPROTOTYPE;
			return -1;
		}
	}

	errno = EAFNOSUPPORT;
	return -1;
}

static void
_socket_flags(int type, int *result)
{
	/* Process socket flags */
	if (type & SOCK_CLOEXEC) {
		*result |= O_CLOEXEC;
	}
	if (type & SOCK_NONBLOCK) {
		*result |= O_NONBLOCK;
	}
	if (type & SOCK_NOSIGPIPE) {
		*result |= O_NOSIGPIPE;
	}
}

static int _tcp_socket(int type, int protocol)
{
	int flags = O_RDWR;

	if (protocol != 0 && protocol != IPPROTO_TCP)
	{
#if DEBUG
		fprintf(stderr, "socket(tcp): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}

	_socket_flags(type, &flags);

	return open(TCP_DEVICE, flags);
}

static int _udp_socket(int type, int protocol)
{
	int r, fd, t_errno, flags = O_RDWR;
	struct sockaddr_in sin;

	if (protocol != 0 && protocol != IPPROTO_UDP)
	{
#if DEBUG
		fprintf(stderr, "socket(udp): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}
	_socket_flags(type, &flags);
	fd= open(UDP_DEVICE, flags);
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

static int _raw_socket(int type, int protocol)
{
	int fd, flags = O_RDWR;
	nwio_ipopt_t ipopt;
	int result;

	if (protocol != IPPROTO_ICMP && protocol != IPPROTO_UDP && protocol != 0)
	{
#if DEBUG
		fprintf(stderr, "socket(icmp): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}
	_socket_flags(type, &flags);
	fd= open(IP_DEVICE, flags);
	if (fd == -1)
		return fd;

	memset(&ipopt, 0, sizeof(ipopt));

        ipopt.nwio_flags= NWIO_COPY;

	if(protocol) {
        	ipopt.nwio_flags |= NWIO_PROTOSPEC;
	        ipopt.nwio_proto = protocol;
	}

        result = ioctl (fd, NWIOSIPOPT, &ipopt);
        if (result<0) {
		close(fd);
		return -1;
	}

        result = ioctl (fd, NWIOGIPOPT, &ipopt);
        if (result<0) {
		close(fd);
		return -1;
	}

	return fd;
}

static int _uds_socket(int type, int protocol)
{
	int fd, r, flags = O_RDWR, sock_type;

	sock_type = type & ~SOCK_FLAGS_MASK;
	if (sock_type != SOCK_STREAM &&
	    sock_type != SOCK_DGRAM &&
	    sock_type != SOCK_SEQPACKET) {
		errno = EPROTOTYPE;
		return -1;
	}

	if (protocol != 0)
	{
#if DEBUG
		fprintf(stderr, "socket(uds): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}

	_socket_flags(type, &flags);
	fd= open(UDS_DEVICE, flags);
	if (fd == -1) {
		return fd;
	}

	/* set the type for the socket via ioctl (SOCK_DGRAM, 
	 * SOCK_STREAM, SOCK_SEQPACKET, etc)
	 */
	r= ioctl(fd, NWIOSUDSTYPE, &sock_type);
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
