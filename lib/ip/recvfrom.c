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
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>

#define DEBUG 0

static ssize_t _udp_recvfrom(int socket, void *_RESTRICT buffer, size_t length,
	int flags, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len, nwio_udpopt_t *udpoptp);

ssize_t recvfrom(int socket, void *_RESTRICT buffer, size_t length,
	int flags, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len)
{
	int r;
	nwio_udpopt_t udpopt;

	fprintf(stderr, "recvfrom: for fd %d\n", socket);

	r= ioctl(socket, NWIOGUDPOPT, &udpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _udp_recvfrom(socket, buffer, length, flags,
			address, address_len, &udpopt);
	}

#if DEBUG
	fprintf(stderr, "recvfrom: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	assert(0);
	return -1;
}

static ssize_t _udp_recvfrom(int socket, void *_RESTRICT buffer, size_t length,
	int flags, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len, nwio_udpopt_t *udpoptp)
{
	int r, t_errno;
	size_t buflen, len;
	void *buf;
	struct sockaddr_in *sinp;
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
#if DEBUG
		fprintf(stderr,
			"recvfrom(udp): NWUO_RWDATONLY not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
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

	r= read(socket, buf, buflen);
	if (r == -1)
	{
		t_errno= errno;
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

