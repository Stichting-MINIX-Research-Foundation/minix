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
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>

#define DEBUG 0

static ssize_t _udp_sendto(int socket, const void *message, size_t length,
	int flags, const struct sockaddr *dest_addr, socklen_t dest_len,
	nwio_udpopt_t *udpoptp);

ssize_t sendto(int socket, const void *message, size_t length, int flags,
	const struct sockaddr *dest_addr, socklen_t dest_len)
{
	int r;
	nwio_udpopt_t udpopt;

	r= ioctl(socket, NWIOGUDPOPT, &udpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _udp_sendto(socket, message, length, flags,
			dest_addr, dest_len, &udpopt);
	}

#if DEBUG
	fprintf(stderr, "sendto: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static ssize_t _udp_sendto(int socket, const void *message, size_t length,
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
		return write(socket, message, length);

	if ((udpoptp->nwuo_flags & NWUO_RP_ANY) ||
		(udpoptp->nwuo_flags & NWUO_RA_ANY))
	{
		/* Check destination address */
		if (dest_len < sizeof(*sinp))
		{
			errno= EINVAL;
			return -1;
		}
		sinp= (struct sockaddr_in *)dest_addr;
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
	r= write(socket, buf, buflen);
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

