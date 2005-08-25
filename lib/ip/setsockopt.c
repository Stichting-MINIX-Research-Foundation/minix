#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#define DEBUG 0

static int _tcp_setsockopt(int socket, int level, int option_name,
	const void *option_value, socklen_t option_len);

int setsockopt(int socket, int level, int option_name,
        const void *option_value, socklen_t option_len)
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
		return _tcp_setsockopt(socket, level, option_name,
			option_value, option_len);
	}

#if DEBUG
	fprintf(stderr, "setsockopt: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_setsockopt(int socket, int level, int option_name,
	const void *option_value, socklen_t option_len)
{
	int i;

	if (level == SOL_SOCKET && option_name == SO_KEEPALIVE)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(int *)option_value;
		if (!i)
		{
			/* At the moment there is no way to turn off 
			 * keepalives.
			 */
			errno= ENOSYS;
			return -1;
		}
		return 0;
	}
#if DEBUG
	fprintf(stderr, "_tcp_setsocketopt: level %d, name %d\n",
		level, option_name);
#endif

	errno= ENOSYS;
	return -1;
}

