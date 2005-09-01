#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>

#include <net/netlib.h>
#include <netinet/in.h>

#define DEBUG 0

static int _tcp_socket(int protocol);
static int _udp_socket(int protocol);

int socket(int domain, int type, int protocol)
{
#if DEBUG
	fprintf(stderr, "socket: domain %d, type %d, protocol %d\n",
		domain, type, protocol);
#endif
	if (domain != AF_INET)
	{
#if DEBUG
		fprintf(stderr, "socket: bad domain %d\n", domain);
#endif
		errno= EAFNOSUPPORT;
		return -1;
	}
	if (type == SOCK_STREAM)
		return _tcp_socket(protocol);

	if (type == SOCK_DGRAM)
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
	int fd;

	if (protocol != 0 && protocol != IPPROTO_UDP)
	{
#if DEBUG
		fprintf(stderr, "socket(udp): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}
	fd= open(UDP_DEVICE, O_RDWR);
	return fd;
}

