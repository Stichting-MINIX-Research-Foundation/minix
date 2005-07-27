#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

#define DEBUG 0

int listen(int socket, int backlog)
{
	int r;

	r= ioctl(socket, NWIOTCPLISTENQ, &backlog);
	if (r != -1 || errno != EBADIOCTL)
		return r;

#if DEBUG
	fprintf(stderr, "listen: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

