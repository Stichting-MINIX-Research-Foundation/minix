#include <sys/cdefs.h>
#include "namespace.h"
#include <minix/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

#include <minix/const.h>

#define DEBUG 0

static int _tcp_connect(int sock, const struct sockaddr *address,
	socklen_t address_len, nwio_tcpconf_t *tcpconfp);
static int _udp_connect(int sock, const struct sockaddr *address,
	socklen_t address_len, nwio_udpopt_t *udpoptp);
static int _uds_connect(int sock, const struct sockaddr *address,
	socklen_t address_len);

int connect(int sock, const struct sockaddr *address,
	socklen_t address_len)
{
	int r;
	nwio_tcpconf_t tcpconf;
	nwio_udpopt_t udpopt;

	r= ioctl(sock, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _tcp_connect(sock, address, address_len, &tcpconf);
	}

	r= ioctl(sock, NWIOGUDPOPT, &udpopt);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _udp_connect(sock, address, address_len, &udpopt);
	}

	r= _uds_connect(sock, address, address_len);
	if (r != -1 || (errno != ENOTTY && errno != EAFNOSUPPORT))
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}

		return r;
	}

#if DEBUG
	fprintf(stderr, "connect: not implemented for fd %d\n", sock);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_connect(int sock, const struct sockaddr *address,
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
	sinp= (struct sockaddr_in *) __UNCONST(address);
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

	if (ioctl(sock, NWIOSTCPCONF, &tcpconf) == -1)
        {
		/* Ignore EISCONN error. The NWIOTCPCONN ioctl will get the
		 * right error.
		 */
		if (errno != EISCONN)
			return -1;
	}

	tcpcl.nwtcl_flags= TCF_DEFAULT;

	r= fcntl(sock, F_GETFL);
	if (r == 1)
		return -1;
	if (r & O_NONBLOCK)
		tcpcl.nwtcl_flags |= TCF_ASYNCH;

	r= ioctl(sock, NWIOTCPCONN, &tcpcl);
	return r;
}

static int _udp_connect(int sock, const struct sockaddr *address,
	socklen_t address_len, nwio_udpopt_t *udpoptp)
{
	int r;
	struct sockaddr_in *sinp;
	nwio_udpopt_t udpopt;

	if (address == NULL)
	{
		/* Unset remote address */
		udpopt.nwuo_flags= NWUO_RP_ANY | NWUO_RA_ANY | NWUO_RWDATALL;

		r= ioctl(sock, NWIOSUDPOPT, &udpopt);
		return r;
	}

	if (address_len != sizeof(*sinp))
	{
		errno= EINVAL;
		return -1;
	}
	sinp= (struct sockaddr_in *) __UNCONST(address);
	if (sinp->sin_family != AF_INET)
	{
		errno= EINVAL;
		return -1;
	}
	udpopt.nwuo_flags= NWUO_RP_SET | NWUO_RA_SET | NWUO_RWDATONLY;
	if ((udpoptp->nwuo_flags & NWUO_LOCPORT_MASK) == NWUO_LP_ANY)
		udpopt.nwuo_flags |= NWUO_LP_SEL;
	udpopt.nwuo_remaddr= sinp->sin_addr.s_addr;
	udpopt.nwuo_remport= sinp->sin_port;

	r= ioctl(sock, NWIOSUDPOPT, &udpopt);
	return r;
}

static int _uds_connect(int sock, const struct sockaddr *address,
	socklen_t address_len)
{

	if (address == NULL) {
		errno = EFAULT;
		return -1;
	}

	/* perform the connect */
	return ioctl(sock, NWIOSUDSCONN, (void *) __UNCONST(address));
}
