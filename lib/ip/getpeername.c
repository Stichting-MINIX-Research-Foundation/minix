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

#define DEBUG 0

static int _tcp_getpeername(int socket, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len, nwio_tcpconf_t *tcpconfp);

int getpeername(int socket, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len)
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
		return _tcp_getpeername(socket, address, address_len,
			&tcpconf);
	}

#if DEBUG
	fprintf(stderr, "getpeername: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_getpeername(int socket, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len, nwio_tcpconf_t *tcpconfp)
{
	socklen_t len;
	struct sockaddr_in sin;

	if (tcpconfp->nwtc_remaddr == 0 ||
		tcpconfp->nwtc_remport == 0)
	{
		errno= ENOTCONN;
		return -1;
	}

	memset(&sin, '\0', sizeof(sin));
	sin.sin_family= AF_INET;
	sin.sin_addr.s_addr= tcpconfp->nwtc_remaddr;
	sin.sin_port= tcpconfp->nwtc_remport;

	len= *address_len;
	if (len > sizeof(sin))
		len= sizeof(sin);
	memcpy(address, &sin, len);
	*address_len= len;

	return 0;
}

