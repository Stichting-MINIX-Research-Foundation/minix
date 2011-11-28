#include <sys/cdefs.h>
#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

#define DEBUG 0

static int _tcp_setsockopt(int sock, int level, int option_name,
	const void *option_value, socklen_t option_len);

static int _udp_setsockopt(int sock, int level, int option_name,
	const void *option_value, socklen_t option_len);

static int _uds_setsockopt(int sock, int level, int option_name,
	const void *option_value, socklen_t option_len);

int setsockopt(int sock, int level, int option_name,
        const void *option_value, socklen_t option_len)
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
		return _tcp_setsockopt(sock, level, option_name,
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
		return _udp_setsockopt(sock, level, option_name,
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
		return _uds_setsockopt(sock, level, option_name,
			option_value, option_len);
	}


#if DEBUG
	fprintf(stderr, "setsockopt: not implemented for fd %d\n", sock);
#endif
	errno= ENOTSOCK;
	return -1;
}

static int _tcp_setsockopt(int sock, int level, int option_name,
	const void *option_value, socklen_t option_len)
{
	int i;

	if (level == SOL_SOCKET && option_name == SO_REUSEADDR)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(const int *)option_value;
		if (!i)
		{
			/* At the moment there is no way to turn off 
			 * reusing addresses.
			 */
			errno= ENOSYS;
			return -1;
		}
		return 0;
	}
	if (level == SOL_SOCKET && option_name == SO_KEEPALIVE)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(const int *)option_value;
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
	if (level == SOL_SOCKET && option_name == SO_RCVBUF)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(const int *)option_value;
		if (i > 32*1024)
		{
			/* The receive buffer is limited to 32K at the moment.
			 */
			errno= ENOSYS;
			return -1;
		}
		/* There is no way to reduce the receive buffer, do we have to
		 * let this call fail for smaller buffers?
		 */
		return 0;
	}
	if (level == SOL_SOCKET && option_name == SO_SNDBUF)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(const int *)option_value;
		if (i > 32*1024)
		{
			/* The send buffer is limited to 32K at the moment.
			 */
			errno= ENOSYS;
			return -1;
		}
		/* There is no way to reduce the send buffer, do we have to
		 * let this call fail for smaller buffers?
		 */
		return 0;
	}
	if (level == IPPROTO_TCP && option_name == TCP_NODELAY)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(const int *)option_value;
		if (i)
		{
			/* At the moment there is no way to turn on 
			 * nodelay.
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

static int _udp_setsockopt(int sock, int level, int option_name,
	const void *option_value, socklen_t option_len)
{
#if DEBUG
	fprintf(stderr, "_udp_setsocketopt: level %d, name %d\n",
		level, option_name);
#endif

	errno= ENOSYS;
	return -1;
}


static int _uds_setsockopt(int sock, int level, int option_name,
	const void *option_value, socklen_t option_len)
{
	int i;
	size_t size;

	if (level == SOL_SOCKET && option_name == SO_RCVBUF)
	{
		if (option_len != sizeof(size))
		{
			errno= EINVAL;
			return -1;
		}
		size= *(const size_t *)option_value;
		return ioctl(sock, NWIOSUDSRCVBUF, &size);
	}

	if (level == SOL_SOCKET && option_name == SO_SNDBUF)
	{
		if (option_len != sizeof(size))
		{
			errno= EINVAL;
			return -1;
		}
		size= *(const size_t *)option_value;
		return ioctl(sock, NWIOSUDSSNDBUF, &size);
	}

	if (level == SOL_SOCKET && option_name == SO_REUSEADDR)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(const int *)option_value;
		if (!i)
		{
			/* At the moment there is no way to turn off 
			 * reusing addresses.
			 */
			errno= ENOSYS;
			return -1;
		}
		return 0;
	}

	if (level == SOL_SOCKET && option_name == SO_PASSCRED)
	{
		if (option_len != sizeof(i))
		{
			errno= EINVAL;
			return -1;
		}
		i= *(const int *)option_value;
		if (!i)
		{
			/* credentials can always be received. */
			errno= ENOSYS;
			return -1;
		}
		return 0;
	}

#if DEBUG
	fprintf(stderr, "_uds_setsocketopt: level %d, name %d\n",
		level, option_name);
#endif

	errno= ENOSYS;
	return -1;
}
