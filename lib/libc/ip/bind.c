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

static int _tcp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_tcpconf_t *tcpconfp);
static int _udp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_udpopt_t *udpoptp);

int bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	int r;
	nwio_tcpconf_t tcpconf;
	nwio_udpopt_t udpopt;

	r= ioctl(socket, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		r= _tcp_bind(socket, address, address_len, &tcpconf);
#if DEBUG
		if (r == -1)
		{
			int t_errno= errno;
			fprintf(stderr, "bind(tcp) failed: %s\n",
				strerror(errno));
			errno= t_errno;
		}
#endif
		return r;
	}

	r= ioctl(socket, NWIOGUDPOPT, &udpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _udp_bind(socket, address, address_len, &udpopt);
	}

#if DEBUG
	fprintf(stderr, "bind: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_tcpconf_t *tcpconfp)
{
	int r;
	nwio_tcpconf_t tcpconf;
	struct sockaddr_in *sinp;

	sinp= (struct sockaddr_in *)address;
	if (sinp->sin_family != AF_INET || address_len != sizeof(*sinp))
	{
#if DEBUG
		fprintf(stderr, "bind(tcp): sin_family = %d, len = %d\n",
			sinp->sin_family, address_len);
#endif
		errno= EAFNOSUPPORT;
		return -1;
	}

	if (sinp->sin_addr.s_addr != INADDR_ANY &&
		sinp->sin_addr.s_addr != tcpconfp->nwtc_locaddr)
	{
		errno= EADDRNOTAVAIL;
		return -1;
	}

	tcpconf.nwtc_flags= 0;

	if (sinp->sin_port == 0)
		tcpconf.nwtc_flags |= NWTC_LP_SEL;
	else
	{
		tcpconf.nwtc_flags |= NWTC_LP_SET;
		tcpconf.nwtc_locport= sinp->sin_port;
	}

	r= ioctl(socket, NWIOSTCPCONF, &tcpconf);
	return r;
}

static int _udp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_udpopt_t *udpoptp)
{
	int r;
	unsigned long curr_flags;
	nwio_udpopt_t udpopt;
	struct sockaddr_in *sinp;

	sinp= (struct sockaddr_in *)address;
	if (sinp->sin_family != AF_INET || address_len != sizeof(*sinp))
	{
#if DEBUG
		fprintf(stderr, "bind(udp): sin_family = %d, len = %d\n",
			sinp->sin_family, address_len);
#endif
		errno= EAFNOSUPPORT;
		return -1;
	}

	if (sinp->sin_addr.s_addr != INADDR_ANY &&
		sinp->sin_addr.s_addr != udpoptp->nwuo_locaddr)
	{
		errno= EADDRNOTAVAIL;
		return -1;
	}

	udpopt.nwuo_flags= 0;

	if (sinp->sin_port == 0)
		udpopt.nwuo_flags |= NWUO_LP_SEL;
	else
	{
		udpopt.nwuo_flags |= NWUO_LP_SET;
		udpopt.nwuo_locport= sinp->sin_port;
	}

	curr_flags= udpoptp->nwuo_flags;
	if (!(curr_flags & NWUO_ACC_MASK))
		udpopt.nwuo_flags |= NWUO_EXCL;
	if (!(curr_flags & (NWUO_EN_LOC|NWUO_DI_LOC)))
		udpopt.nwuo_flags |= NWUO_EN_LOC;
	if (!(curr_flags & (NWUO_EN_BROAD|NWUO_DI_BROAD)))
		udpopt.nwuo_flags |= NWUO_EN_BROAD;
	if (!(curr_flags & (NWUO_RP_SET|NWUO_RP_ANY)))
		udpopt.nwuo_flags |= NWUO_RP_ANY;
	if (!(curr_flags & (NWUO_RA_SET|NWUO_RA_ANY)))
		udpopt.nwuo_flags |= NWUO_RA_ANY;
	if (!(curr_flags & (NWUO_RWDATONLY|NWUO_RWDATALL)))
		udpopt.nwuo_flags |= NWUO_RWDATALL;
	if (!(curr_flags & (NWUO_EN_IPOPT|NWUO_DI_IPOPT)))
		udpopt.nwuo_flags |= NWUO_DI_IPOPT;

	r= ioctl(socket, NWIOSUDPOPT, &udpopt);
	return r;
}
