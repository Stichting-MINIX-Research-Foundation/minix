#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

#define DEBUG 0

static int _tcp_accept(int socket, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len);

int accept(int socket, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len)
{
	int r;
	nwio_udpopt_t udpopt;

	r= _tcp_accept(socket, address, address_len);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
		return r;

	/* Unfortunately, we have to return EOPNOTSUPP for a socket that
	 * does not support accept (such as a UDP socket) and ENOTSOCK for
	 * filedescriptors that do not refer to a socket.
	 */
	r= ioctl(socket, NWIOGUDPOPT, &udpopt);
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

static int _tcp_accept(int socket, struct sockaddr *_RESTRICT address,
	socklen_t *_RESTRICT address_len)
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
	r= ioctl(socket, NWIOTCPACCEPTTO, &cookie);
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
