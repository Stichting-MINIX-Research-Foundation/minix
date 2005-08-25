#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#define DEBUG 0

static int _tcp_getsockopt(int socket, int level, int option_name,
	void *_RESTRICT option_value, socklen_t *_RESTRICT option_len);

int getsockopt(int socket, int level, int option_name,
        void *_RESTRICT option_value, socklen_t *_RESTRICT option_len)
{
	int r;
	nwio_tcpopt_t tcpopt;

	r= ioctl(socket, NWIOGTCPOPT, &tcpopt);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _tcp_getsockopt(socket, level, option_name,
			option_value, option_len);
	}

#if DEBUG
	fprintf(stderr, "getsockopt: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_getsockopt(int socket, int level, int option_name,
	void *_RESTRICT option_value, socklen_t *_RESTRICT option_len)
{
	int i;

	if (level == SOL_SOCKET && option_name == SO_KEEPALIVE)
	{
		i= 1;	/* Keepalive is always on */
		if (*option_len < sizeof(i))
			memcpy(option_value, &i, *option_len);
		else
			memcpy(option_value, &i, sizeof(i));
		*option_len= sizeof(i);
		return 0;
	}
#if DEBUG
	fprintf(stderr, "_tcp_getsocketopt: level %d, name %d\n",
		level, option_name);
#endif

	errno= ENOSYS;
	return -1;
}

