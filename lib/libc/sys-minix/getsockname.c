/*

   getsockname()

   from socket emulation library for Minix 2.0.x

*/

#include <sys/cdefs.h>
#include "namespace.h"
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
#include <sys/un.h>

/*
#define DEBUG 0
*/

static int _tcp_getsockname(int fd, struct sockaddr *__restrict address,
   socklen_t *__restrict address_len, nwio_tcpconf_t *tcpconfp);

static int _uds_getsockname(int fd, struct sockaddr *__restrict address,
   socklen_t *__restrict address_len, struct sockaddr_un *uds_addr);

int getsockname(int fd, struct sockaddr *__restrict address,
   socklen_t *__restrict address_len)
{
	int r;
	nwio_tcpconf_t tcpconf;
	struct sockaddr_un uds_addr;

#ifdef DEBUG
	fprintf(stderr,"mnx_getsockname: ioctl fd %d.\n", fd);
#endif

	r= ioctl(fd, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}

		return _tcp_getsockname(fd, address, address_len, &tcpconf);
	}

	r= ioctl(fd, NWIOGUDSADDR, &uds_addr);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}

		return _uds_getsockname(fd, address, address_len, &uds_addr);
	}

#if DEBUG
	fprintf(stderr, "getsockname: not implemented for fd %d\n", socket);
#endif

	errno= ENOSYS;
	return -1;
}


static int _tcp_getsockname(int fd, struct sockaddr *__restrict address,
   socklen_t *__restrict address_len, nwio_tcpconf_t *tcpconf)
{
	socklen_t len;
	struct sockaddr_in sin;

#ifdef DEBUG1
	fprintf(stderr, "mnx_getsockname: from %s, %u",
			inet_ntoa(tcpconf.nwtc_remaddr),
			ntohs(tcpconf.nwtc_remport));
	fprintf(stderr," for %s, %u\n",
			inet_ntoa(tcpconf.nwtc_locaddr),
			ntohs(tcpconf.nwtc_locport));
#endif

	memset(&sin, '\0', sizeof(sin));
	sin.sin_family= AF_INET;
	sin.sin_addr.s_addr= tcpconf->nwtc_locaddr ;
	sin.sin_port= tcpconf->nwtc_locport;

	len= *address_len;
	if (len > sizeof(sin))
		len= sizeof(sin);
	memcpy(address, &sin, len);
	*address_len= len;

	return 0;
}

static int _uds_getsockname(int fd, struct sockaddr *__restrict address,
   socklen_t *__restrict address_len, struct sockaddr_un *uds_addr)
{
	socklen_t len;

	if (uds_addr->sun_family != AF_UNIX)
	{
		errno= EINVAL;
		return -1;
	}

	len= *address_len;
	if (len > sizeof(struct sockaddr_un))
		len = sizeof(struct sockaddr_un);

	memcpy(address, uds_addr, len);
	*address_len= len;

	return 0;
}
