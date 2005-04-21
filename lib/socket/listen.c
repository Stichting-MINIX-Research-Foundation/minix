
/* bsd-socket(2)-lookalike */

#include <sys/types.h>
#include <net/gen/socket.h>
#include <net/gen/in.h>
#include <net/ioctl.h>
#include <net/gen/emu.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>

int
listen(int s, int backlog)
{
	nwio_tcpcl_t tcpopt;
	struct sockaddr_in *in_peer;

	tcpopt.nwtcl_flags = tcpopt.nwtcl_ttl = 0;

	if(ioctl(s, NWIOTCPLISTEN, &tcpopt) < 0)
		return -1;

	return 0;
}

