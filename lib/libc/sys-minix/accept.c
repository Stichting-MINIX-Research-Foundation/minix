#include <sys/cdefs.h>
#include "namespace.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

#define DEBUG 0

static int _tcp_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len);

static int _uds_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len);

int accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int r;
	nwio_udpopt_t udpopt;

	r= _tcp_accept(sock, address, address_len);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
		return r;

	r= _uds_accept(sock, address, address_len);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
		return r;

	/* Unfortunately, we have to return EOPNOTSUPP for a socket that
	 * does not support accept (such as a UDP socket) and ENOTSOCK for
	 * filedescriptors that do not refer to a socket.
	 */
	r= ioctl(sock, NWIOGUDPOPT, &udpopt);
	if (r == 0)
	{
		/* UDP socket */
		errno= EOPNOTSUPP;
		return -1;
	}
	if ((errno == ENOTTY || errno == EBADIOCTL))
	{
		errno= ENOTSOCK;
		return -1;
	}

	return r;
}

static int _tcp_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int r, s1, t_errno;
	tcp_cookie_t cookie;

	s1= open(TCP_DEVICE, O_RDWR);
	if (s1 == -1)
		return s1;
	r= ioctl(s1, NWIOGTCPCOOKIE, &cookie);
	if (r == -1)
	{
		t_errno= errno;
		close(s1);
		errno= t_errno;
		return -1;
	}
	r= ioctl(sock, NWIOTCPACCEPTTO, &cookie);
	if (r == -1)
	{
		t_errno= errno;
		close(s1);
		errno= t_errno;
		return -1;
	}
	if (address != NULL)
		getpeername(s1, address, address_len);
	return s1;
}

static int _uds_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int s1;
	int r;
	struct sockaddr_un uds_addr;
	socklen_t len;

	memset(&uds_addr, '\0', sizeof(struct sockaddr_un));

	r= ioctl(sock, NWIOGUDSADDR, &uds_addr);
	if (r == -1) {
		return r;
	}

	if (uds_addr.sun_family != AF_UNIX) {
		errno= EINVAL;
		return -1;
	}

	len= *address_len;
	if (len > sizeof(struct sockaddr_un))
		len = sizeof(struct sockaddr_un);

	memcpy(address, &uds_addr, len);
	*address_len= len;

	s1= open(UDS_DEVICE, O_RDWR);
	if (s1 == -1)
		return s1;

	r= ioctl(s1, NWIOSUDSACCEPT, address);
	if (r == -1) {
		int ioctl_errno = errno;
		close(s1);
		errno = ioctl_errno;
		return r;
	}

	return s1;
}
