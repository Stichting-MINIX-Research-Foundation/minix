
/* bsd-socket(2)-lookalike */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <net/ioctl.h>
#include <net/gen/socket.h>
#include <net/gen/emu.h>
#include <net/gen/tcp.h>
#include <net/gen/in.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>

int
bind(int s, struct sockaddr *addr, socklen_t len)
{
	nwio_tcpconf_t tcpconf;
	struct sockaddr_in *in_local;

	in_local = (struct sockaddr_in *) addr;

	memset(&tcpconf, 0, sizeof(tcpconf));
	tcpconf.nwtc_flags = NWTC_EXCL | NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
	tcpconf.nwtc_locport = in_local->sin_port;

	if(ioctl(s, NWIOSTCPCONF, &tcpconf) < 0)
		return -1;

	return 0;
}

