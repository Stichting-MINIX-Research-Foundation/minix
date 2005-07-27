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

static int _tcp_connect(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_tcpconf_t *tcpconfp);

int connect(int socket, const struct sockaddr *address,
	socklen_t address_len)
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
		return _tcp_connect(socket, address, address_len, &tcpconf);
	}
#if DEBUG
	fprintf(stderr, "connect: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_connect(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_tcpconf_t *tcpconfp)
{
	int r;
	struct sockaddr_in *sinp;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpcl;

	if (address_len != sizeof(*sinp))
	{
		errno= EINVAL;
		return -1;
	}
	sinp= (struct sockaddr_in *)address;
	if (sinp->sin_family != AF_INET)
	{
		errno= EINVAL;
		return -1;
	}
	tcpconf.nwtc_flags= NWTC_SET_RA | NWTC_SET_RP;
	if ((tcpconfp->nwtc_flags & NWTC_LOCPORT_MASK) == NWTC_LP_UNSET)
		tcpconf.nwtc_flags |= NWTC_LP_SEL;
	tcpconf.nwtc_remaddr= sinp->sin_addr.s_addr;
	tcpconf.nwtc_remport= sinp->sin_port;

	if (ioctl(socket, NWIOSTCPCONF, &tcpconf) == -1)
	{
		int  t_errno= errno;

		fprintf(stderr, "setconf failed: %s\n", strerror(errno));

		errno= t_errno;

		return -1;
	}

	tcpcl.nwtcl_flags= 0;

	r= ioctl(socket, NWIOTCPCONN, &tcpcl);
	if (r == -1)
	{
		int  t_errno= errno;

		fprintf(stderr, "connect failed: %s\n", strerror(errno));

		errno= t_errno;
	}
	return r;
}
