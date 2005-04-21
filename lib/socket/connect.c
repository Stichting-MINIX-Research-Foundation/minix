
/* bsd-socket(2)-lookalike */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <net/ioctl.h>
#include <net/gen/in.h>
#include <net/gen/socket.h>
#include <net/gen/emu.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>

int
connect(int s, struct sockaddr *peer, socklen_t len)
{
	nwio_tcpcl_t tcpopt;
	nwio_tcpconf_t tcpconf;
	struct sockaddr_in *in_peer;

	if(!peer || peer->sa_family != AF_INET ||
		len != sizeof(struct sockaddr_in)) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	in_peer = (struct sockaddr_in *) peer;

	memset(&tcpconf, 0, sizeof(tcpconf));
	tcpconf.nwtc_flags = NWTC_EXCL | NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	tcpconf.nwtc_remaddr = in_peer->sin_addr.s_addr;
	tcpconf.nwtc_remport = in_peer->sin_port;

	if(ioctl(s, NWIOSTCPCONF, &tcpconf) < 0)
		return -1;

	tcpopt.nwtcl_flags = 0;
	tcpopt.nwtcl_ttl = 0;

	if(ioctl(s, NWIOTCPCONN, &tcpopt) < 0)
		return -1;

	return 0;
}

