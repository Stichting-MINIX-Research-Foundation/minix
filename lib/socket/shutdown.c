
/* bsd-socket(2)-lookalike */

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/gen/socket.h>
#include <net/gen/emu.h>
#include <net/gen/tcp.h>
#include <net/gen/in.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>

int
shutdown(int s, int how)
{
	nwio_tcpcl_t tcpopt;

	if(ioctl(s, NWIOTCPSHUTDOWN, NULL) < 0)
		return -1;

	return 0;
}

