
/* bsd-socket(2)-lookalike */

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <net/gen/socket.h>
#include <net/gen/emu.h>
#include <net/gen/tcp.h>
#include <net/gen/in.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>

int
socket(int domain, int type, int protocol)
{
	int s;
	char *tcpname;

	/* only domain is AF_INET */
	if(domain != AF_INET) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	/* only type is SOCK_STREAM */
	if(type != SOCK_STREAM) {
		errno = EPROTONOSUPPORT;
		return -1;
	}

	/* default protocol type is TCP */
	if(!protocol)
		protocol = IPPROTO_TCP;

	/* only protocol type is TCP */
	if(protocol != IPPROTO_TCP) {
		errno = EPROTONOSUPPORT;
		return -1;
	}

	/* tcp device name */
	if(!tcpname)
		tcpname = getenv("TCP_DEVICE");
	if(!tcpname || !*tcpname)
		tcpname = "/dev/tcp";

	if((s = open(tcpname, O_RDWR)) < 0) {
		perror(tcpname);
		return -1;
	}

	return s;
}

