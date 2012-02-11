#include <sys/cdefs.h>
#include "namespace.h"

#undef NDEBUG

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

static ssize_t _tcp_sendto(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
static ssize_t _udp_sendto(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len,
	nwio_udpopt_t *udpoptp);
static ssize_t _uds_sendto_conn(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
static ssize_t _uds_sendto_dgram(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len);

ssize_t sendto(int sock, const void *message, size_t length, int flags,
	const struct sockaddr *dest_addr, socklen_t dest_len)
{
	int r;
	nwio_tcpopt_t tcpopt;
	nwio_udpopt_t udpopt;
	int uds_sotype = -1;

	r= ioctl(sock, NWIOGTCPOPT, &tcpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _tcp_sendto(sock, message, length, flags,
			dest_addr, dest_len);
	}

	r= ioctl(sock, NWIOGUDPOPT, &udpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _udp_sendto(sock, message, length, flags,
			dest_addr, dest_len, &udpopt);
	}

	r= ioctl(sock, NWIOGUDSSOTYPE, &uds_sotype);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1) {
			return r;
		}

		if (uds_sotype == SOCK_DGRAM) {

			return _uds_sendto_dgram(sock, message, 
				length, flags,dest_addr, dest_len);
		} else {

			return _uds_sendto_conn(sock, message,
				length, flags, dest_addr, dest_len);
		}
	}

#if DEBUG
	fprintf(stderr, "sendto: not implemented for fd %d\n", sock);
#endif
	errno= ENOSYS;
	return -1;
}

static ssize_t _tcp_sendto(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "sendto(tcp): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	/* Silently ignore destination, if given. */

	return write(sock, message, length);
}

static ssize_t _udp_sendto(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len,
	nwio_udpopt_t *udpoptp)
{
	int r, t_errno;
	size_t buflen;
	void *buf;
	struct sockaddr_in *sinp;
	udp_io_hdr_t *io_hdrp;

	if (flags)
	{
#if DEBUG
		fprintf(stderr, "sendto(udp): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	if (udpoptp->nwuo_flags & NWUO_RWDATONLY)
		return write(sock, message, length);

	if ((udpoptp->nwuo_flags & NWUO_RP_ANY) ||
		(udpoptp->nwuo_flags & NWUO_RA_ANY))
	{
		if (!dest_addr)
		{
			errno= ENOTCONN;
			return -1;
		}

		/* Check destination address */
		if (dest_len < sizeof(*sinp))
		{
			errno= EINVAL;
			return -1;
		}
		sinp= (struct sockaddr_in *) __UNCONST(dest_addr);
		if (sinp->sin_family != AF_INET)
		{
			errno= EAFNOSUPPORT;
			return -1;
		}
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

	io_hdrp= buf;
	io_hdrp->uih_src_addr= 0;	/* Unused */
	io_hdrp->uih_src_port= 0;	/* Will cause error if NWUO_LP_ANY */
	if (udpoptp->nwuo_flags & NWUO_RA_ANY)
		io_hdrp->uih_dst_addr= sinp->sin_addr.s_addr;
	else
		io_hdrp->uih_dst_addr= 0;
	if (udpoptp->nwuo_flags & NWUO_RP_ANY)
		io_hdrp->uih_dst_port= sinp->sin_port;
	else
		io_hdrp->uih_dst_port= 0;
	io_hdrp->uih_ip_opt_len= 0;
	io_hdrp->uih_data_len= 0;

	memcpy(&io_hdrp[1], message, length);
	r= write(sock, buf, buflen);
	if (r == -1)
	{
		t_errno= errno;
		free(buf);
		errno= t_errno;
		return -1;
	}
	assert(r == buflen);
	free(buf);
	return length;
}

static ssize_t _uds_sendto_conn(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{

	/* for connection oriented unix domain sockets (SOCK_STREAM / 
	 * SOCK_SEQPACKET)
	 */

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "sendto(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	/* Silently ignore destination, if given. */

	return write(sock, message, length);
}

static ssize_t _uds_sendto_dgram(int sock, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	int r;

	/* for connectionless unix domain sockets (SOCK_DGRAM) */

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "sendto(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	if (dest_addr == NULL) {
		errno = EFAULT;
		return -1;
	}

	/* set the target address */
	r= ioctl(sock, NWIOSUDSTADDR, (void *) __UNCONST(dest_addr));
	if (r == -1) {
		return r;
	}

	/* do the send */
	return write(sock, message, length);
}
