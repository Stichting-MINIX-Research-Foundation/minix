#include <sys/cdefs.h>
#include "namespace.h"

#undef NDEBUG

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>

#define DEBUG 0

static ssize_t _tcp_recvfrom(int sock, void *__restrict buffer, size_t length,
	int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_tcpconf_t *tcpconfp);
static ssize_t _udp_recvfrom(int sock, void *__restrict buffer, size_t length,
	int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_udpopt_t *udpoptp);
static ssize_t _uds_recvfrom_conn(int sock, void *__restrict buffer,
	size_t length, int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, struct sockaddr_un *uds_addr);
static ssize_t _uds_recvfrom_dgram(int sock, void *__restrict buffer,
	size_t length, int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len);

ssize_t recvfrom(int sock, void *__restrict buffer, size_t length,
	int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int r;
	nwio_tcpconf_t tcpconf;
	nwio_udpopt_t udpopt;
	struct sockaddr_un uds_addr;
	int uds_sotype = -1;

#if DEBUG
	fprintf(stderr, "recvfrom: for fd %d\n", sock);
#endif

	r= ioctl(sock, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _tcp_recvfrom(sock, buffer, length, flags,
			address, address_len, &tcpconf);
	}

	r= ioctl(sock, NWIOGUDPOPT, &udpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _udp_recvfrom(sock, buffer, length, flags,
			address, address_len, &udpopt);
	}

	r= ioctl(sock, NWIOGUDSSOTYPE, &uds_sotype);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{

		if (r == -1) {
			return r;
		}

		if (uds_sotype == SOCK_DGRAM) {
			return _uds_recvfrom_dgram(sock, buffer, 
				length, flags, address, address_len);
		} else {
			return _uds_recvfrom_conn(sock, buffer, 
				length, flags, address, address_len, 
				&uds_addr);
		}
	}

#if DEBUG
	fprintf(stderr, "recvfrom: not implemented for fd %d\n", sock);
#endif
	errno= ENOSYS;
	assert(0);
	return -1;
}

static ssize_t _tcp_recvfrom(int sock, void *__restrict buffer, size_t length,
	int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_tcpconf_t *tcpconfp)
{
	int r;
	size_t len;
	struct sockaddr_in sin;

	if (flags != 0)
	{
#if DEBUG
		fprintf(stderr, "recvfrom(tcp): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	r = read(sock, buffer, length);

	if (r >= 0 && address != NULL)
	{
		sin.sin_family= AF_INET;
		sin.sin_addr.s_addr= tcpconfp->nwtc_remaddr;
		sin.sin_port= tcpconfp->nwtc_remport;
		len= *address_len;
		if (len > sizeof(sin))
			len= sizeof(sin);
		memcpy(address, &sin, len);
		*address_len= sizeof(sin);
	}	

	return r;
}

static ssize_t _udp_recvfrom(int sock, void *__restrict buffer, size_t length,
	int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_udpopt_t *udpoptp)
{
	int r, t_errno;
	size_t buflen, len;
	void *buf;
	udp_io_hdr_t *io_hdrp;
	struct sockaddr_in sin;

	if (flags)
	{
#if DEBUG
		fprintf(stderr, "recvfrom(udp): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	if (udpoptp->nwuo_flags & NWUO_RWDATONLY)
	{
		if (address != NULL &&
			(udpoptp->nwuo_flags & (NWUO_RA_SET | NWUO_RP_SET)) !=
			(NWUO_RA_SET | NWUO_RP_SET))
		{

#if DEBUG
			fprintf(stderr,
			"recvfrom(udp): RWDATONLY on unconnected socket\n");
#endif
			errno= ENOTCONN;
			return -1;
		}

		r= read(sock, buffer, length);
		if (r == -1)
			return r;

		if (address != NULL)
		{
			sin.sin_family= AF_INET;
			sin.sin_addr.s_addr= udpoptp->nwuo_remaddr;
			sin.sin_port= udpoptp->nwuo_remport;
			len= *address_len;
			if (len > sizeof(sin))
				len= sizeof(sin);
			memcpy(address, &sin, len);
			*address_len= sizeof(sin);
		}

		return r;
	}

	buflen= sizeof(*io_hdrp) + length;
	if (buflen < length)
	{	
		/* Overflow */
		errno= EMSGSIZE;
		return -1;
	}
	buf= malloc(buflen);
	if (buf == NULL)
		return -1;

	r= read(sock, buf, buflen);
	if (r == -1)
	{
		t_errno= errno;
#if DEBUG
		fprintf(stderr, "recvfrom(udp): read failed: %s\n",
			strerror(errno));
		fprintf(stderr, "udp opt flags = 0x%x\n", udpoptp->nwuo_flags);
#endif
		free(buf);
		errno= t_errno;
		return -1;
	}

	assert(r >= sizeof(*io_hdrp));
	length= r-sizeof(*io_hdrp);

	io_hdrp= buf;
	memcpy(buffer, &io_hdrp[1], length);

	if (address != NULL)
	{
		sin.sin_family= AF_INET;
		sin.sin_addr.s_addr= io_hdrp->uih_src_addr;
		sin.sin_port= io_hdrp->uih_src_port;
		len= *address_len;
		if (len > sizeof(sin))
			len= sizeof(sin);
		memcpy(address, &sin, len);
		*address_len= sizeof(sin);
	}	
	free(buf);
	return length;
}

static ssize_t _uds_recvfrom_conn(int sock, void *__restrict buffer, 
	size_t length, int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, struct sockaddr_un *uds_addr)
{
	int r;
	size_t len;

	/* for connection oriented unix domain sockets (SOCK_STREAM / 
	 * SOCK_SEQPACKET)
	 */

	if (flags != 0)
	{
#if DEBUG
		fprintf(stderr, "recvfrom(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	r = read(sock, buffer, length);

	if (r >= 0 && address != NULL)
	{

		len= *address_len;
		if (len > sizeof(struct sockaddr_un))
			len= sizeof(struct sockaddr_un);
		memcpy(address, uds_addr, len);
		*address_len= sizeof(struct sockaddr_un);
	}	

	return r;
}

static ssize_t _uds_recvfrom_dgram(int sock, void *__restrict buffer, 
	size_t length, int flags, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int r;
	size_t len;

	/* for connectionless unix domain sockets (SOCK_DGRAM) */

	if (flags != 0)
	{
#if DEBUG
		fprintf(stderr, "recvfrom(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	r = read(sock, buffer, length);

	if (r >= 0 && address != NULL)
	{
		len= *address_len;
		if (len > sizeof(struct sockaddr_un))
			len= sizeof(struct sockaddr_un);
		ioctl(sock, NWIOGUDSFADDR, address);
		*address_len= sizeof(struct sockaddr_un);
	}	

	return r;
}

