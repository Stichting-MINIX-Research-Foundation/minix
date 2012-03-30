#include <sys/cdefs.h>
#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <netinet/tcp.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

#include <minix/type.h>

#define DEBUG 0

static int _tcp_getsockopt(int sock, int level, int option_name,
	void *__restrict option_value, socklen_t *__restrict option_len);
static int _udp_getsockopt(int sock, int level, int option_name,
	void *__restrict option_value, socklen_t *__restrict option_len);
static int _uds_getsockopt(int sock, int level, int option_name,
	void *__restrict option_value, socklen_t *__restrict option_len);
static void getsockopt_copy(void *return_value, size_t return_len,
	void *__restrict option_value, socklen_t *__restrict option_len);

int getsockopt(int sock, int level, int option_name,
        void *__restrict option_value, socklen_t *__restrict option_len)
{
	int r;
	nwio_tcpopt_t tcpopt;
	nwio_udpopt_t udpopt;
	struct sockaddr_un uds_addr;

	r= ioctl(sock, NWIOGTCPOPT, &tcpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _tcp_getsockopt(sock, level, option_name,
			option_value, option_len);
	}

	r= ioctl(sock, NWIOGUDPOPT, &udpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _udp_getsockopt(sock, level, option_name,
			option_value, option_len);
	}

	r= ioctl(sock, NWIOGUDSADDR, &uds_addr);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _uds_getsockopt(sock, level, option_name,
			option_value, option_len);
	}


#if DEBUG
	fprintf(stderr, "getsockopt: not implemented for fd %d\n", sock);
#endif
	errno= ENOTSOCK;
	return -1;
}

static void getsockopt_copy(void *return_value, size_t return_len,
	void *__restrict option_value, socklen_t *__restrict option_len)
{
	/* copy as much data as possible */
	if (*option_len < return_len)
		memcpy(option_value, return_value, *option_len);
	else
		memcpy(option_value, return_value, return_len);

	/* return length */
	*option_len = return_len;
}

static int _tcp_getsockopt(int sock, int level, int option_name,
	void *__restrict option_value, socklen_t *__restrict option_len)
{
	int i, r, err;

	if (level == SOL_SOCKET && option_name == SO_REUSEADDR)
	{
		i = 1;	/* Binds to TIME_WAIT sockets never cause errors */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}
	if (level == SOL_SOCKET && option_name == SO_KEEPALIVE)
	{
		i = 1;	/* Keepalive is always on */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}
	if (level == SOL_SOCKET && option_name == SO_ERROR)
	{
		r = ioctl(sock, NWIOTCPGERROR, &err);
		if (r != 0)
			return r;

		getsockopt_copy(&err, sizeof(err), option_value, option_len);
		return 0;
	}
	if (level == SOL_SOCKET && option_name == SO_RCVBUF)
	{
		i = 32 * 1024;	/* Receive buffer in the current 
		              	 * implementation 
				 */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}
	if (level == SOL_SOCKET && option_name == SO_SNDBUF)
	{
		i = 32 * 1024;	/* Send buffer in the current implementation */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}
	if (level == SOL_SOCKET && option_name == SO_TYPE)
	{
		i = SOCK_STREAM;	/* this is a TCP socket */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}
	if (level == IPPROTO_TCP && option_name == TCP_NODELAY)
	{
		i = 0;	/* nodelay is always off */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}
#if DEBUG
	fprintf(stderr, "_tcp_getsocketopt: level %d, name %d\n",
		level, option_name);
#endif

	errno= ENOPROTOOPT;
	return -1;
}

static int _udp_getsockopt(int sock, int level, int option_name,
	void *__restrict option_value, socklen_t *__restrict option_len)
{
	int i;

	if (level == SOL_SOCKET && option_name == SO_TYPE)
	{
		i = SOCK_DGRAM;	/* this is a UDP socket */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}
#if DEBUG
	fprintf(stderr, "_udp_getsocketopt: level %d, name %d\n",
		level, option_name);
#endif

	errno= ENOSYS;
	return -1;
}

static int _uds_getsockopt(int sock, int level, int option_name,
	void *__restrict option_value, socklen_t *__restrict option_len)
{
	int i, r;
	size_t size;

	if (level == SOL_SOCKET && option_name == SO_RCVBUF)
	{
 		r= ioctl(sock, NWIOGUDSRCVBUF, &size);
		if (r == -1) {
			return r;
		}

		getsockopt_copy(&size, sizeof(size), option_value, option_len);
		return 0;
	}

	if (level == SOL_SOCKET && option_name == SO_SNDBUF)
	{
 		r= ioctl(sock, NWIOGUDSSNDBUF, &size);
		if (r == -1) {
			return r;
		}

		getsockopt_copy(&size, sizeof(size), option_value, option_len);
		return 0;
	}

	if (level == SOL_SOCKET && option_name == SO_TYPE)
	{
 		r= ioctl(sock, NWIOGUDSSOTYPE, &i);
		if (r == -1) {
			return r;
		}

		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}

	if (level == SOL_SOCKET && option_name == SO_PEERCRED)
	{
		struct ucred cred;

		r= ioctl(sock, NWIOGUDSPEERCRED, &cred);
		if (r == -1) {
			return -1;
		}

		getsockopt_copy(&cred, sizeof(struct ucred), option_value,
							option_len);
		return 0;
	}


	if (level == SOL_SOCKET && option_name == SO_REUSEADDR)
	{
		i = 1;	/* as long as nobody is listen()ing on the address,
			 * it can be reused without waiting for a 
			 * timeout to expire.
			 */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}

	if (level == SOL_SOCKET && option_name == SO_PASSCRED)
	{
		i = 1;	/* option is always 'on' */
		getsockopt_copy(&i, sizeof(i), option_value, option_len);
		return 0;
	}

#if DEBUG
	fprintf(stderr, "_uds_getsocketopt: level %d, name %d\n",
		level, option_name);
#endif

	errno= ENOSYS;
	return -1;
}
