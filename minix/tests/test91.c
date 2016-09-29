/* Advanced tests for TCP and UDP sockets (LWIP) - by D.C. van Moolenbroek */
/*
 * This is a somewhat random collection of in-depth tests, complementing the
 * more general functionality tests in test80 and test81.  The overall test set
 * is still by no means expected to be "complete."  The subtests are in random
 * order.
 */
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_var.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <machine/vmparam.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "common.h"
#include "socklib.h"

#define ITERATIONS	1

static const enum state tcp_states[] = {
		S_NEW,		S_N_SHUT_R,	S_BOUND,	S_LISTENING,
		S_L_SHUT_R,	S_L_SHUT_W,	S_L_SHUT_RW,	S_CONNECTING,
		S_C_SHUT_R,	S_C_SHUT_W,	S_C_SHUT_RW,	S_CONNECTED,
		S_ACCEPTED,	S_SHUT_R,	S_SHUT_W,	S_SHUT_RW,
		S_RSHUT_R,	S_RSHUT_W,	S_RSHUT_RW,	S_SHUT2_R,
		S_SHUT2_W,	S_SHUT2_RW,	S_PRE_EOF,	S_AT_EOF,
		S_POST_EOF,	S_PRE_SHUT_R,	S_EOF_SHUT_R,	S_POST_SHUT_R,
		S_PRE_SHUT_W,	S_EOF_SHUT_W,	S_POST_SHUT_W,	S_PRE_SHUT_RW,
		S_EOF_SHUT_RW,	S_POST_SHUT_RW,	S_PRE_RESET,	S_AT_RESET,
		S_POST_RESET,	S_FAILED,	S_POST_FAILED
};

static const int tcp_results[][__arraycount(tcp_states)] = {
	[C_ACCEPT]		= {
		-EINVAL,	-EINVAL,	-EINVAL,	-EAGAIN,
		-ECONNABORTED,	-ECONNABORTED,	-ECONNABORTED,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,
	},
	[C_BIND]		= {
		0,		0,		-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,
	},
	[C_CONNECT]		= {
		-EINPROGRESS,	-EINPROGRESS,	-EINPROGRESS,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EALREADY,
		-EALREADY,	-EINVAL,	-EINVAL,	-EISCONN,
		-EISCONN,	-EISCONN,	-EISCONN,	-EISCONN,
		-EISCONN,	-EISCONN,	-EISCONN,	-EISCONN,
		-EINVAL,	-EINVAL,	-EISCONN,	-EISCONN,
		-EISCONN,	-EISCONN,	-EISCONN,	-EISCONN,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,
	},
	[C_GETPEERNAME]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		-ENOTCONN,	-ENOTCONN,	0,		0,
		0,		0,		0,		0,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
	},
	[C_GETSOCKNAME]		= {
		0,		0,		0,		0,
		-EINVAL,	-EINVAL,	-EINVAL,	0,
		0,		-EINVAL,	-EINVAL,	0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		-EINVAL,	-EINVAL,	0,		0,
		0,		0,		0,		0,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,
	},
	[C_GETSOCKOPT_ERR]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		-ECONNRESET,	-ECONNRESET,
		0,		-ECONNREFUSED,	0,
	},
	[C_GETSOCKOPT_KA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,
	},
	[C_GETSOCKOPT_RB]	= {
		0,		0,		0,		0,
		-ECONNRESET,	-ECONNRESET,	-ECONNRESET,	0,
		0,		-ECONNRESET,	-ECONNRESET,	0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		-ECONNRESET,	-ECONNRESET,	0,		0,
		0,		0,		0,		0,
		-ECONNRESET,	-ECONNRESET,	-ECONNRESET,	-ECONNRESET,
		-ECONNRESET,	-ECONNRESET,	-ECONNRESET,	-ECONNRESET,
		-ECONNRESET,	-ECONNRESET,	-ECONNRESET,
	},
	[C_IOCTL_NREAD]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		1,		0,
		0,		0,		0,		0,
		1,		0,		0,		0,
		0,		0,		1,		0,
		0,		0,		0,
	},
	[C_LISTEN]		= {
		0,		0,		0,		0,
		0,		0,		0,		-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,
	},
	[C_RECV]		= {
		-ENOTCONN,	0,		-ENOTCONN,	-ENOTCONN,
		0,		0,		0,		-EAGAIN,
		0,		0,		0,		-EAGAIN,
		-EAGAIN,	0,		-EAGAIN,	0,
		-EAGAIN,	0,		0,		0,
		0,		0,		1,		0,
		0,		0,		0,		0,
		1,		0,		0,		0,
		0,		0,		1,		-ECONNRESET,
		0,		-ECONNREFUSED,	0,
	},
	[C_RECVFROM]		= {
		-ENOTCONN,	0,		-ENOTCONN,	-ENOTCONN,
		0,		0,		0,		-EAGAIN,
		0,		0,		0,		-EAGAIN,
		-EAGAIN,	0,		-EAGAIN,	0,
		-EAGAIN,	0,		0,		0,
		0,		0,		1,		0,
		0,		0,		0,		0,
		1,		0,		0,		0,
		0,		0,		1,		-ECONNRESET,
		0,		-ECONNREFUSED,	0,
	},
	[C_SEND]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-EPIPE,		-EPIPE,		-EPIPE,		-EAGAIN,
		-EAGAIN,	-EPIPE,		-EPIPE,		1,
		1,		1,		-EPIPE,		-EPIPE,
		1,		1,		1,		1,
		-EPIPE,		-EPIPE,		1,		1,
		1,		1,		1,		1,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-ECONNRESET,	-ECONNRESET,
		-EPIPE,		-ECONNREFUSED,	-EPIPE,
	},
	[C_SENDTO]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-EPIPE,		-EPIPE,		-EPIPE,		-EAGAIN,
		-EAGAIN,	-EPIPE,		-EPIPE,		1,
		1,		1,		-EPIPE,		-EPIPE,
		1,		1,		1,		1,
		-EPIPE,		-EPIPE,		1,		1,
		1,		1,		1,		1,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-ECONNRESET,	-ECONNRESET,
		-EPIPE,		-ECONNREFUSED,	-EPIPE,
	},
	[C_SELECT_R]		= {
		1,		1,		1,		0,
		1,		1,		1,		0,
		1,		1,		1,		0,
		0,		1,		0,		1,
		0,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,
	},
	[C_SELECT_W]		= {
		1,		1,		1,		1,
		1,		1,		1,		0,
		0,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,
	},
	[C_SELECT_X]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,
	},
	[C_SETSOCKOPT_BC]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,
	},
	[C_SETSOCKOPT_KA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,
	},
	[C_SETSOCKOPT_L]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,
	},
	[C_SETSOCKOPT_RA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,
	},
	[C_SHUTDOWN_R]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,
	},
	[C_SHUTDOWN_RW]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	0,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	0,
		0,		-ENOTCONN,	-ENOTCONN,	0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		-ENOTCONN,	-ENOTCONN,	0,		0,
		0,		0,		0,		0,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
	},
	[C_SHUTDOWN_W]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	0,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	0,
		0,		-ENOTCONN,	-ENOTCONN,	0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		-ENOTCONN,	-ENOTCONN,	0,		0,
		0,		0,		0,		0,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
	},
};

/*
 * Set up a TCP socket file descriptor in the requested state and pass it to
 * socklib_sweep_call() along with local and remote addresses and their length.
 */
static int
tcp_sweep(int domain, int type, int protocol, enum state state, enum call call)
{
	struct sockaddr_in sinA, sinB, sinC, sinD;
	struct sockaddr_in6 sin6A, sin6B, sin6C, sin6D;
	struct sockaddr *addrA, *addrB, *addrC, *addrD;
	socklen_t addr_len, len;
	struct linger l;
	fd_set fds;
	char buf[1];
	int r, fd, fd2, fd3, tmpfd, val;

	if (domain == AF_INET) {
		memset(&sin6A, 0, sizeof(sin6A));
		sinA.sin_family = domain;
		sinA.sin_port = htons(TEST_PORT_A);
		sinA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		memcpy(&sinB, &sinA, sizeof(sinB));
		sinB.sin_port = htons(0);

		memcpy(&sinC, &sinA, sizeof(sinC));
		sinC.sin_addr.s_addr = inet_addr(TEST_BLACKHOLE_IPV4);

		memcpy(&sinD, &sinA, sizeof(sinD));
		sinD.sin_port = htons(TEST_PORT_B);

		addrA = (struct sockaddr *)&sinA;
		addrB = (struct sockaddr *)&sinB;
		addrC = (struct sockaddr *)&sinC;
		addrD = (struct sockaddr *)&sinD;
		addr_len = sizeof(sinA);
	} else {
		assert(domain == AF_INET6);

		memset(&sin6A, 0, sizeof(sin6A));
		sin6A.sin6_family = domain;
		sin6A.sin6_port = htons(TEST_PORT_A);
		memcpy(&sin6A.sin6_addr, &in6addr_loopback,
		    sizeof(sin6A.sin6_addr));

		memcpy(&sin6B, &sin6A, sizeof(sin6B));
		sin6B.sin6_port = htons(0);

		memcpy(&sin6C, &sin6A, sizeof(sin6C));
		if (inet_pton(domain, TEST_BLACKHOLE_IPV6,
		    &sin6C.sin6_addr) != 1) e(0);

		memcpy(&sin6D, &sin6A, sizeof(sin6D));
		sin6D.sin6_port = htons(TEST_PORT_B);

		addrA = (struct sockaddr *)&sin6A;
		addrB = (struct sockaddr *)&sin6B;
		addrC = (struct sockaddr *)&sin6C;
		addrD = (struct sockaddr *)&sin6D;
		addr_len = sizeof(sin6A);
	}

	/* Create a bound remote socket. */
	if ((fd2 = socket(domain, type | SOCK_NONBLOCK, protocol)) < 0) e(0);

	if (bind(fd2, addrB, addr_len) != 0) e(0);

	len = addr_len;
	if (getsockname(fd2, addrB, &len) != 0) e(0);
	if (len != addr_len) e(0);

	if (listen(fd2, 1) != 0) e(0);

	fd3 = -1;

	switch (state) {
	case S_NEW:
	case S_N_SHUT_R:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		val = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (state == S_N_SHUT_R && shutdown(fd, SHUT_RD)) e(0);

		break;

	case S_BOUND:
	case S_LISTENING:
	case S_L_SHUT_R:
	case S_L_SHUT_W:
	case S_L_SHUT_RW:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		val = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (bind(fd, addrA, addr_len) != 0) e(0);

		if (state == S_BOUND)
			break;

		if (listen(fd, 1) != 0) e(0);

		switch (state) {
		case S_L_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_L_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_L_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		break;

	case S_CONNECTING:
	case S_C_SHUT_R:
	case S_C_SHUT_W:
	case S_C_SHUT_RW:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		if (connect(fd, addrC, addr_len) != -1) e(0);
		if (errno != EINPROGRESS) e(0);

		switch (state) {
		case S_C_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_C_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_C_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		break;

	case S_CONNECTED:
	case S_ACCEPTED:
	case S_SHUT_R:
	case S_SHUT_W:
	case S_SHUT_RW:
	case S_RSHUT_R:
	case S_RSHUT_W:
	case S_RSHUT_RW:
	case S_SHUT2_R:
	case S_SHUT2_W:
	case S_SHUT2_RW:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		if (connect(fd, addrB, addr_len) != -1) e(0);
		if (errno != EINPROGRESS) e(0);

		/* Just to make sure, wait for the socket to be acceptable. */
		FD_ZERO(&fds);
		FD_SET(fd2, &fds);
		if (select(fd2 + 1, &fds, NULL, NULL, NULL) != 1) e(0);

		len = addr_len;
		if ((fd3 = accept(fd2, addrC, &len)) < 0) e(0);

		/* Just to make sure, wait for the socket to be connected. */
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (select(fd + 1, NULL, &fds, NULL, NULL) != 1) e(0);

		switch (state) {
		case S_SHUT_R:
		case S_SHUT2_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_SHUT_W:
		case S_SHUT2_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_SHUT_RW:
		case S_SHUT2_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		switch (state) {
		case S_RSHUT_R:
		case S_SHUT2_R: if (shutdown(fd3, SHUT_RD)) e(0); break;
		case S_RSHUT_W:
		case S_SHUT2_W: if (shutdown(fd3, SHUT_WR)) e(0); break;
		case S_RSHUT_RW:
		case S_SHUT2_RW: if (shutdown(fd3, SHUT_RDWR)) e(0); break;
		default: break;
		}

		if (state == S_ACCEPTED) {
			tmpfd = fd;
			fd = fd3;
			fd3 = tmpfd;
		}

		break;

	case S_PRE_EOF:
	case S_AT_EOF:
	case S_POST_EOF:
	case S_PRE_SHUT_R:
	case S_EOF_SHUT_R:
	case S_POST_SHUT_R:
	case S_PRE_SHUT_W:
	case S_EOF_SHUT_W:
	case S_POST_SHUT_W:
	case S_PRE_SHUT_RW:
	case S_EOF_SHUT_RW:
	case S_POST_SHUT_RW:
	case S_PRE_RESET:
	case S_AT_RESET:
	case S_POST_RESET:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		if (connect(fd, addrB, addr_len) != -1) e(0);
		if (errno != EINPROGRESS) e(0);

		/* Just to make sure, wait for the socket to be acceptable. */
		FD_ZERO(&fds);
		FD_SET(fd2, &fds);
		if (select(fd2 + 1, &fds, NULL, NULL, NULL) != 1) e(0);

		len = addr_len;
		if ((fd3 = accept(fd2, addrC, &len)) < 0) e(0);

		if (send(fd3, "", 1, 0) != 1) e(0);

		switch (state) {
		case S_PRE_RESET:
		case S_AT_RESET:
		case S_POST_RESET:
			l.l_onoff = 1;
			l.l_linger = 0;

			if (setsockopt(fd3, SOL_SOCKET, SO_LINGER, &l,
			    sizeof(l)) != 0) e(0);

			break;
		default:
			break;
		}

		if (close(fd3) != 0) e(0);
		fd3 = -1;

		/* Just to make sure, wait for the socket to receive data. */
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);

		switch (state) {
		case S_AT_EOF:
		case S_EOF_SHUT_R:
		case S_EOF_SHUT_W:
		case S_EOF_SHUT_RW:
		case S_AT_RESET:
			if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
			break;
		case S_POST_EOF:
		case S_POST_SHUT_R:
		case S_POST_SHUT_W:
		case S_POST_SHUT_RW:
			if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
			if (recv(fd, buf, sizeof(buf), 0) != 0) e(0);
			break;
		case S_POST_RESET:
			if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
			(void)recv(fd, buf, sizeof(buf), 0);
			break;
		default:
			break;
		}

		switch (state) {
		case S_PRE_SHUT_R:
		case S_EOF_SHUT_R:
		case S_POST_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_PRE_SHUT_W:
		case S_EOF_SHUT_W:
		case S_POST_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_PRE_SHUT_RW:
		case S_EOF_SHUT_RW:
		case S_POST_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		break;

	case S_FAILED:
	case S_POST_FAILED:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		if (connect(fd, addrD, addr_len) != -1) e(0);
		if (errno != EINPROGRESS) e(0);

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);

		if (state == S_POST_FAILED) {
			if (recv(fd, buf, sizeof(buf), 0) != -1) e(0);
			if (errno != ECONNREFUSED) e(0);
		}

		break;

	default:
		fd = -1;
		e(0);
	}

	r = socklib_sweep_call(call, fd, addrA, addrB, addr_len);

	if (close(fd) != 0) e(0);
	if (fd2 != -1 && close(fd2) != 0) e(0);
	if (fd3 != -1 && close(fd3) != 0) e(0);

	return r;
}

static const enum state udp_states[] = {
		S_NEW,		S_N_SHUT_R,	S_N_SHUT_W,	S_N_SHUT_RW,
		S_BOUND,	S_CONNECTED,	S_SHUT_R,	S_SHUT_W,
		S_SHUT_RW,	S_RSHUT_R,	S_RSHUT_W,	S_RSHUT_RW,
		S_SHUT2_R,	S_SHUT2_W,	S_SHUT2_RW,	S_PRE_RESET,
		S_AT_RESET,	S_POST_RESET
};

static const int udp_results[][__arraycount(udp_states)] = {
	[C_ACCEPT]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_BIND]		= {
		0,		0,		0,		0,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,
	},
	[C_CONNECT]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_GETPEERNAME]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_GETSOCKNAME]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_GETSOCKOPT_ERR]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_GETSOCKOPT_KA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_GETSOCKOPT_RB]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_IOCTL_NREAD]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		1,
		0,		0,
	},
	[C_LISTEN]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_RECV]		= {
		-EAGAIN,	0,		-EAGAIN,	0,
		-EAGAIN,	-EAGAIN,	0,		-EAGAIN,
		0,		-EAGAIN,	-EAGAIN,	-EAGAIN,
		0,		-EAGAIN,	0,		1,
		-EAGAIN,	-EAGAIN,
	},
	[C_RECVFROM]		= {
		-EAGAIN,	0,		-EAGAIN,	0,
		-EAGAIN,	-EAGAIN,	0,		-EAGAIN,
		0,		-EAGAIN,	-EAGAIN,	-EAGAIN,
		0,		-EAGAIN,	0,		1,
		-EAGAIN,	-EAGAIN,
	},
	[C_SEND]		= {
		-EDESTADDRREQ,	-EDESTADDRREQ,	-EPIPE,		-EPIPE,
		-EDESTADDRREQ,	1,		1,		-EPIPE,
		-EPIPE,		1,		1,		1,
		1,		-EPIPE,		-EPIPE,		1,
		1,		1,
	},
	[C_SENDTO]		= {
		1,		1,		-EPIPE,		-EPIPE,
		1,		1,		1,		-EPIPE,
		-EPIPE,		1,		1,		1,
		1,		-EPIPE,		-EPIPE,		1,
		1,		1,
	},
	[C_SELECT_R]		= {
		0,		1,		0,		1,
		0,		0,		1,		0,
		1,		0,		0,		0,
		1,		0,		1,		1,
		0,		0,
	},
	[C_SELECT_W]		= {
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,
	},
	[C_SELECT_X]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_SETSOCKOPT_BC]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_SETSOCKOPT_KA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_SETSOCKOPT_L]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_SETSOCKOPT_RA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_SHUTDOWN_R]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_SHUTDOWN_RW]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
	[C_SHUTDOWN_W]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,
	},
};

/*
 * Set up a UDP socket file descriptor in the requested state and pass it to
 * socklib_sweep_call() along with local and remote addresses and their length.
 */
static int
udp_sweep(int domain, int type, int protocol, enum state state, enum call call)
{
	struct sockaddr_in sinA, sinB;
	struct sockaddr_in6 sin6A, sin6B;
	struct sockaddr *addrA, *addrB;
	socklen_t addr_len;
	char buf[1];
	int r, fd, fd2;

	if (domain == AF_INET) {
		memset(&sinA, 0, sizeof(sinA));
		sinA.sin_family = domain;
		sinA.sin_port = htons(TEST_PORT_A);
		sinA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		memcpy(&sinB, &sinA, sizeof(sinB));
		sinB.sin_port = htons(TEST_PORT_B);

		addrA = (struct sockaddr *)&sinA;
		addrB = (struct sockaddr *)&sinB;
		addr_len = sizeof(sinA);
	} else {
		assert(domain == AF_INET6);

		memset(&sin6A, 0, sizeof(sin6A));
		sin6A.sin6_family = domain;
		sin6A.sin6_port = htons(TEST_PORT_A);
		memcpy(&sin6A.sin6_addr, &in6addr_loopback,
		    sizeof(sin6A.sin6_addr));

		memcpy(&sin6B, &sin6A, sizeof(sin6B));
		sin6B.sin6_port = htons(TEST_PORT_B);

		addrA = (struct sockaddr *)&sin6A;
		addrB = (struct sockaddr *)&sin6B;
		addr_len = sizeof(sin6A);
	}

	/* Create a bound remote socket. */
	if ((fd2 = socket(domain, type | SOCK_NONBLOCK, protocol)) < 0) e(0);

	if (bind(fd2, addrB, addr_len) != 0) e(0);

	switch (state) {
	case S_NEW:
	case S_N_SHUT_R:
	case S_N_SHUT_W:
	case S_N_SHUT_RW:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		switch (state) {
		case S_N_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_N_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_N_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		break;

	case S_BOUND:
	case S_CONNECTED:
	case S_SHUT_R:
	case S_SHUT_W:
	case S_SHUT_RW:
	case S_RSHUT_R:
	case S_RSHUT_W:
	case S_RSHUT_RW:
	case S_SHUT2_R:
	case S_SHUT2_W:
	case S_SHUT2_RW:
	case S_PRE_RESET:
	case S_AT_RESET:
	case S_POST_RESET:
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		if (bind(fd, addrA, addr_len) != 0) e(0);

		if (state == S_BOUND)
			break;

		if (connect(fd, addrB, addr_len) != 0) e(0);

		switch (state) {
		case S_SHUT_R:
		case S_SHUT2_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_SHUT_W:
		case S_SHUT2_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_SHUT_RW:
		case S_SHUT2_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		switch (state) {
		case S_RSHUT_R:
		case S_SHUT2_R: if (shutdown(fd2, SHUT_RD)) e(0); break;
		case S_RSHUT_W:
		case S_SHUT2_W: if (shutdown(fd2, SHUT_WR)) e(0); break;
		case S_RSHUT_RW:
		case S_SHUT2_RW: if (shutdown(fd2, SHUT_RDWR)) e(0); break;
		case S_PRE_RESET:
		case S_AT_RESET:
		case S_POST_RESET:
			if (sendto(fd2, "", 1, 0, addrA, addr_len) != 1) e(0);

			if (close(fd2) != 0) e(0);
			fd2 = -1;

			if (state != S_PRE_RESET) {
				if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
			}
			if (state == S_POST_RESET) {
				(void)recv(fd, buf, sizeof(buf), 0);
			}
		default:
			break;
		}

		break;

	default:
		fd = -1;
		e(0);
	}

	r = socklib_sweep_call(call, fd, addrA, addrB, addr_len);

	if (close(fd) != 0) e(0);
	if (fd2 != -1 && close(fd2) != 0) e(0);

	return r;
}

/*
 * Sweep test for socket calls versus socket states of TCP and UDP sockets.
 */
static void
test91a(void)
{

	subtest = 1;

	socklib_sweep(AF_INET, SOCK_STREAM, 0, tcp_states,
	    __arraycount(tcp_states), (const int *)tcp_results, tcp_sweep);

	socklib_sweep(AF_INET6, SOCK_STREAM, 0, tcp_states,
	    __arraycount(tcp_states), (const int *)tcp_results, tcp_sweep);

	socklib_sweep(AF_INET, SOCK_DGRAM, 0, udp_states,
	    __arraycount(udp_states), (const int *)udp_results, udp_sweep);

	socklib_sweep(AF_INET6, SOCK_DGRAM, 0, udp_states,
	    __arraycount(udp_states), (const int *)udp_results, udp_sweep);
}

#define F_SKIP	-1	/* skip this entry */
#define F_NO	0	/* binding or connecting should fail */
#define F_YES	1	/* binding or connecting should succeed */
#define F_DUAL	2	/* always fails on IPV6_V6ONLY sockets */
#define F_ZONE	4	/* binding works only if a scope ID is given */
#define F_UDP	8	/* do not test on TCP sockets */
#define F_BAD	16	/* operations on this address result in EINVAL */

static const struct {
	const char *addr;
	int may_bind;
	int may_connect;	/* UDP only */
} addrs_v4[] = {
	{ "0.0.0.0",		F_YES,		F_NO },
	{ "0.0.0.1",		F_NO,		F_SKIP },
	{ "127.0.0.1",		F_YES,		F_YES },
	{ "127.0.0.255",	F_NO,		F_YES },
	{ "127.255.255.255",	F_NO,		F_YES },
	{ "172.31.255.254",	F_NO,		F_SKIP }, /* may be valid.. */
	{ "224.0.0.0",		F_YES | F_UDP,	F_SKIP },
	{ "239.255.255.255",	F_YES | F_UDP,	F_SKIP },
	{ "240.0.0.0",		F_NO,		F_SKIP },
	{ "255.255.255.255",	F_NO,		F_SKIP },
};

static const struct {
	const char *addr;
	int may_bind;
	int may_connect;	/* UDP only */
} addrs_v6[] = {
	{ "::0",		F_YES,			F_NO },
	{ "::1",		F_YES,			F_YES },
	{ "::2",		F_NO,			F_YES },
	{ "::127.0.0.1",	F_NO,			F_YES },
	{ "::ffff:7f00:1",	F_YES | F_DUAL,		F_YES | F_DUAL },
	{ "::ffff:7f00:ff",	F_NO | F_DUAL,		F_YES | F_DUAL },
	{ "100::1",		F_NO,			F_SKIP },
	{ "2fff:ffff::",	F_NO,			F_SKIP },
	{ "fc00::1",		F_NO,			F_SKIP },
	{ "fe00::1",		F_NO,			F_SKIP },
	{ "fe80::1",		F_YES | F_ZONE,		F_YES | F_ZONE },
	{ "fec0::1",		F_NO,			F_SKIP },
	{ "ff01::1",		F_YES | F_ZONE | F_UDP,	F_YES | F_ZONE },
	{ "ff02::1",		F_YES | F_ZONE | F_UDP,	F_YES | F_ZONE },
	{ "ff02::2",		F_YES | F_ZONE | F_UDP,	F_YES | F_ZONE },
	{ "ff0e::1",		F_YES | F_UDP,		F_SKIP },
	{ "ffff::1",		F_NO | F_UDP | F_BAD,	F_NO | F_BAD },
};

/*
 * Test binding sockets of a particular type to various addresses.
 */
static void
sub91b(int type)
{
	struct sockaddr_in sin, lsin;
	struct sockaddr_in6 sin6, lsin6;
	socklen_t len;
	unsigned int i, ifindex;
	int r, fd, val;

	ifindex = if_nametoindex(LOOPBACK_IFNAME);

	/* Test binding IPv4 sockets to IPv4 addresses. */
	for (i = 0; i < __arraycount(addrs_v4); i++) {
		if (type == SOCK_STREAM && (addrs_v4[i].may_bind & F_UDP))
			continue;

		if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		if (inet_pton(AF_INET, addrs_v4[i].addr, &sin.sin_addr) != 1)
			e(0);

		r = bind(fd, (struct sockaddr *)&sin, sizeof(sin));
		if (r == -1 && errno != EADDRNOTAVAIL) e(0);
		if (r + 1 != !!(addrs_v4[i].may_bind & F_YES)) e(0);

		len = sizeof(lsin);
		if (getsockname(fd, (struct sockaddr *)&lsin, &len) != 0) e(0);
		if (lsin.sin_len != sizeof(lsin)) e(0);
		if (lsin.sin_family != AF_INET) e(0);
		if (r == 0) {
			if (lsin.sin_port == 0) e(0);
			if (lsin.sin_addr.s_addr != sin.sin_addr.s_addr) e(0);
		} else {
			if (lsin.sin_port != 0) e(0);
			if (lsin.sin_addr.s_addr != htonl(INADDR_ANY)) e(0);
		}

		/* Rebinding never works; binding after a failed bind does. */
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		r = bind(fd, (struct sockaddr *)&sin, sizeof(sin));
		if (r == -1 && errno != EINVAL) e(0);
		if (!!r != !!(addrs_v4[i].may_bind & F_YES)) e(0);

		if (close(fd) != 0) e(0);
	}

	/* Test binding IPv6 sockets to IPv6 addresses. */
	for (i = 0; i < __arraycount(addrs_v6); i++) {
		if (type == SOCK_STREAM && (addrs_v6[i].may_bind & F_UDP))
			continue;

		/* Try without IPV6_V6ONLY. */
		if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

		/* IPV6_V6ONLY may or may not be enabled by default.. */
		val = 0;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
		    sizeof(val)) != 0) e(0);

		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, addrs_v6[i].addr,
		    &sin6.sin6_addr) != 1) e(0);

		if (addrs_v6[i].may_bind & F_ZONE) {
			if (bind(fd, (struct sockaddr *)&sin6,
			    sizeof(sin6)) != -1) e(0);
			if (errno != EADDRNOTAVAIL) e(0);

			sin6.sin6_scope_id = ifindex;
		}

		r = bind(fd, (struct sockaddr *)&sin6, sizeof(sin6));
		if (r == -1) {
			if (addrs_v6[i].may_bind & F_BAD) {
				if (errno != EINVAL) e(0);
			} else {
				if (errno != EADDRNOTAVAIL) e(0);
			}
		}
		if (r + 1 != !!(addrs_v6[i].may_bind & F_YES)) e(0);

		len = sizeof(lsin6);
		if (getsockname(fd, (struct sockaddr *)&lsin6, &len) != 0)
			e(0);
		if (lsin6.sin6_len != sizeof(lsin6)) e(0);
		if (lsin6.sin6_family != AF_INET6) e(0);
		if (r == 0) {
			if (lsin6.sin6_port == 0) e(0);
			if (memcmp(&lsin6.sin6_addr, &sin6.sin6_addr,
			    sizeof(lsin6.sin6_addr))) e(0);
			if (lsin6.sin6_scope_id !=
			    ((addrs_v6[i].may_bind & F_ZONE) ? ifindex : 0))
				e(0);
		} else {
			if (lsin6.sin6_port != 0) e(0);
			if (!IN6_IS_ADDR_UNSPECIFIED(&lsin6.sin6_addr)) e(0);
			if (lsin6.sin6_scope_id != 0) e(0);
		}

		if (close(fd) != 0) e(0);

		/* Try with IPV6_V6ONLY. */
		if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

		val = 1;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
		    sizeof(val)) != 0) e(0);

		r = bind(fd, (struct sockaddr *)&sin6, sizeof(sin6));
		if (r == -1) {
			if (addrs_v6[i].may_bind & (F_BAD | F_DUAL)) {
				if (errno != EINVAL) e(0);
			} else
				if (errno != EADDRNOTAVAIL) e(0);
		}
		if (r + 1 !=
		    ((addrs_v6[i].may_bind & (F_YES | F_DUAL)) == F_YES)) e(0);

		if (close(fd) != 0) e(0);
	}

	/* Test binding an IPv6 socket to an IPv4 address. */
	if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EINVAL) e(0);

	assert(sizeof(sin) <= sizeof(sin6));
	memset(&sin6, 0, sizeof(sin6));
	memcpy(&sin6, &sin, sizeof(sin));
	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != -1) e(0);
	if (errno != EAFNOSUPPORT) e(0);

	if (close(fd) != 0) e(0);

	/* Test binding an IPv4 socket to an IPv6 address. */
	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_any, sizeof(sin6.sin6_addr));

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin)) != -1) e(0);
	if (errno != EAFNOSUPPORT) e(0);

	if (close(fd) != 0) e(0);

	/* Test binding a socket to AF_UNSPEC. */
	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_UNSPEC;

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EAFNOSUPPORT) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test binding sockets to various addresses.
 */
static void
test91b(void)
{

	subtest = 2;

	sub91b(SOCK_STREAM);

	sub91b(SOCK_DGRAM);
}

/*
 * Test connecting TCP sockets to various addresses.  We cannot test much here,
 * because we do not actually want this test to generate outgoing traffic.  In
 * effect, we test calls that should fail only.
 */
static void
sub91c_tcp(void)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	int fd, val;

	/*
	 * Test connecting to address zero (0.0.0.0 and ::0).  Apparently the
	 * traditional BSD behavior for IPv4 is to use the first interface's
	 * local address as destination instead, but our implementation does
	 * not support that at this time: these 'any' addresses always result
	 * in connection failures right away, hopefully eliminating some tricky
	 * implementation boundary cases.
	 */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EHOSTUNREACH && errno != ENETUNREACH) e(0);

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);
	memcpy(&sin6.sin6_addr, &in6addr_any, sizeof(sin6.sin6_addr));

	if (connect(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != -1) e(0);
	if (errno != EHOSTUNREACH && errno != ENETUNREACH) e(0);

	if (close(fd) != 0) e(0);

	/*
	 * Test connecting to an IPv6-mapped IPv4 address on an IPv6 socket
	 * with INET6_V6ONLY enabled.
	 */
	if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);
	if (inet_pton(AF_INET6, "::ffff:"LOOPBACK_IPV4, &sin6.sin6_addr) != 1)
		e(0);

	if (connect(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (close(fd) != 0) e(0);

	/* Test connecting to an AF_UNSPEC address. */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_UNSPEC;

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EAFNOSUPPORT) e(0);

	if (close(fd) != 0) e(0);

	/* Test connecting to port zero. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test connecting UDP sockets to various addresses.
 */
static void
sub91c_udp(void)
{
	struct sockaddr_in sin, rsin;
	struct sockaddr_in6 sin6, rsin6;
	socklen_t len;
	unsigned int i, ifindex;
	int r, fd, val;

	ifindex = if_nametoindex(LOOPBACK_IFNAME);

	/* Test connecting IPv4 sockets to IPv4 addresses. */
	for (i = 0; i < __arraycount(addrs_v4); i++) {
		if (addrs_v4[i].may_connect == F_SKIP)
			continue;

		if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(TEST_PORT_A);
		if (inet_pton(AF_INET, addrs_v4[i].addr, &sin.sin_addr) != 1)
			e(0);

		r = connect(fd, (struct sockaddr *)&sin, sizeof(sin));
		if (r + 1 != !!(addrs_v4[i].may_connect & F_YES)) e(0);

		len = sizeof(rsin);
		if (r == 0) {
			if (getpeername(fd, (struct sockaddr *)&rsin,
			    &len) != 0) e(0);
			if (rsin.sin_len != sizeof(rsin)) e(0);
			if (rsin.sin_family != AF_INET) e(0);
			if (rsin.sin_port != htons(TEST_PORT_A)) e(0);
			if (rsin.sin_addr.s_addr != sin.sin_addr.s_addr) e(0);
		} else {
			if (getpeername(fd, (struct sockaddr *)&rsin,
			    &len) != -1) e(0);
			if (errno != ENOTCONN) e(0);
		}

		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		r = bind(fd, (struct sockaddr *)&sin, sizeof(sin));
		if (r == -1 && errno != EINVAL) e(0);
		if (r + 1 != !(addrs_v4[i].may_connect & F_YES)) e(0);

		if (close(fd) != 0) e(0);
	}

	/* Test connecting IPv6 sockets to IPv6 addresses. */
	for (i = 0; i < __arraycount(addrs_v6); i++) {
		if (addrs_v6[i].may_connect == F_SKIP)
			continue;

		/* Try without IPV6_V6ONLY. */
		if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

		/* IPV6_V6ONLY may or may not be enabled by default.. */
		val = 0;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
		    sizeof(val)) != 0) e(0);

		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(TEST_PORT_A);
		if (inet_pton(AF_INET6, addrs_v6[i].addr,
		    &sin6.sin6_addr) != 1) e(0);
		sin6.sin6_scope_id = ifindex;

		r = connect(fd, (struct sockaddr *)&sin6, sizeof(sin6));
		if (r + 1 != !!(addrs_v6[i].may_connect & F_YES)) e(0);

		len = sizeof(rsin6);
		if (r == 0) {
			if (getpeername(fd, (struct sockaddr *)&rsin6,
			    &len) != 0) e(0);
			if (rsin6.sin6_len != sizeof(rsin6)) e(0);
			if (rsin6.sin6_family != AF_INET6) e(0);
			if (rsin6.sin6_port != htons(TEST_PORT_A)) e(0);
			if (memcmp(&rsin6.sin6_addr, &sin6.sin6_addr,
			    sizeof(rsin6.sin6_addr))) e(0);
			if (rsin6.sin6_scope_id !=
			    ((addrs_v6[i].may_connect & F_ZONE) ? ifindex : 0))
				e(0);
		} else {
			if (getpeername(fd, (struct sockaddr *)&rsin,
			    &len) != -1) e(0);
			if (errno != ENOTCONN) e(0);
		}

		if (close(fd) != 0) e(0);

		/* Try with IPV6_V6ONLY. */
		if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

		val = 1;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
		    sizeof(val)) != 0) e(0);

		r = connect(fd, (struct sockaddr *)&sin6, sizeof(sin6));
		if (r == -1 && errno != EINVAL && errno != EHOSTUNREACH) e(0);
		if (r + 1 !=
		    ((addrs_v6[i].may_connect & (F_YES | F_DUAL)) == F_YES))
			e(0);

		if (close(fd) != 0) e(0);
	}

	/* Test connecting an IPv6 socket to an IPv4 address. */
	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EINVAL) e(0);

	assert(sizeof(sin) <= sizeof(sin6));
	memset(&sin6, 0, sizeof(sin6));
	memcpy(&sin6, &sin, sizeof(sin));
	if (connect(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != -1) e(0);
	if (errno != EAFNOSUPPORT) e(0);

	if (close(fd) != 0) e(0);

	/* Test connecting an IPv4 socket to an IPv6 address. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_any, sizeof(sin6.sin6_addr));

	if (connect(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (connect(fd, (struct sockaddr *)&sin6, sizeof(sin)) != -1) e(0);
	if (errno != EAFNOSUPPORT) e(0);

	if (close(fd) != 0) e(0);

	/* Test unconnecting a socket using AF_UNSPEC. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_UNSPEC;

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	len = sizeof(rsin);
	if (getpeername(fd, (struct sockaddr *)&rsin, &len) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	if (close(fd) != 0) e(0);

	/* Test connecting to port zero. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test connecting sockets to various addresses.
 */
static void
test91c(void)
{

	subtest = 3;

	sub91c_tcp();

	sub91c_udp();
}

/*
 * Test binding with IPv4/IPv6 on the same port for the given socket type.
 */
static void
sub91d(int type)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	int r, fd, fd2, val;

	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	/* IPv4 bound; IPv6 bind without IPV6_V6ONLY may or may not work. */
	if ((fd2 = socket(AF_INET6, type, 0)) < 0) e(0);

	val = 0;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);
	memcpy(&sin6.sin6_addr, &in6addr_any, sizeof(sin6.sin6_addr));

	r = bind(fd2, (struct sockaddr *)&sin6, sizeof(sin6));
	if (r == -1 && errno != EADDRINUSE) e(0);

	if (close(fd2) != 0) e(0);

	/* IPv4 bound; IPv6 bind with IPV6_V6ONLY should work. */
	if ((fd2 = socket(AF_INET6, type, 0)) < 0) e(0);

	val = 1;
	if (setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0)
		e(0);
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	if (bind(fd2, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* IPv6 bound with IPV6_V6ONLY; IPv4 bind may or may not work. */
	if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0)
		e(0);
	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	if ((fd2 = socket(AF_INET, type, 0)) < 0) e(0);

	r = bind(fd2, (struct sockaddr *)&sin, sizeof(sin));
	if (r == -1 && errno != EADDRINUSE) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* IPv6 bound with IPV6_V6ONLY; IPv4 bind should work. */
	if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

	val = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0)
		e(0);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	if ((fd2 = socket(AF_INET, type, 0)) < 0) e(0);

	if (bind(fd2, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);
}

/*
 * Test binding with IPv4/IPv6 on the same port, and IPV6_V6ONLY.
 */
static void
test91d(void)
{

	subtest = 4;

	sub91d(SOCK_STREAM);

	sub91d(SOCK_DGRAM);
}

/*
 * Test sending large and small UDP packets.
 */
static void
test91e(void)
{
	struct sockaddr_in sin;
	struct msghdr msg;
	struct iovec iov;
	char *buf;
	unsigned int i, j;
	int r, fd, fd2, val;

	subtest = 5;

	if ((buf = malloc(65536)) == NULL) e(0);

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	val = 65536;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val)) != 0)
		e(0);

	if ((fd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	/*
	 * A maximum send buffer size of a full packet size's worth may always
	 * be set, although this is not necessarily the actual maximum.
	 */
	val = 65535;
	if (setsockopt(fd2, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val)) != 0)
		e(0);

	/* Find the largest possible packet size that can actually be sent. */
	for (i = 0; i < val; i += sizeof(int)) {
		j = i ^ 0xdeadbeef;
		memcpy(&buf[i], &j, sizeof(j));
	}

	for (val = 65536; val > 0; val--) {
		if ((r = sendto(fd2, buf, val, 0, (struct sockaddr *)&sin,
		    sizeof(sin))) == val)
			break;
		if (r != -1) e(0);
		if (errno != EMSGSIZE) e(0);
	}

	if (val != 65535 - sizeof(struct udphdr) - sizeof(struct ip)) e(0);

	memset(buf, 0, val);
	buf[val] = 'X';

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = val + 1;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (recvmsg(fd, &msg, 0) != val) e(0);
	if (msg.msg_flags != 0) e(0);

	for (i = 0; i < val; i += sizeof(int)) {
		j = i ^ 0xdeadbeef;
		if (memcmp(&buf[i], &j, MIN(sizeof(j), val - i))) e(0);
	}
	if (buf[val] != 'X') e(0);

	if (sendto(fd2, buf, val, 0, (struct sockaddr *)&sin, sizeof(sin)) !=
	    val) e(0);

	/*
	 * Make sure that there are no off-by-one errors in the receive code,
	 * and that MSG_TRUNC is set (only) when not the whole packet was
	 * received.
	 */
	memset(&iov, 0, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = val;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (recvmsg(fd, &msg, 0) != val) e(0);
	if (msg.msg_flags != 0) e(0);

	if (sendto(fd2, buf, val, 0, (struct sockaddr *)&sin, sizeof(sin)) !=
	    val) e(0);

	buf[val - 1] = 'Y';

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = val - 1;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (recvmsg(fd, &msg, 0) != val - 1) e(0);
	if (msg.msg_flags != MSG_TRUNC) e(0);

	for (i = 0; i < val - 1; i += sizeof(int)) {
		j = i ^ 0xdeadbeef;
		if (memcmp(&buf[i], &j, MIN(sizeof(j), val - 1 - i))) e(0);
	}
	if (buf[val - 1] != 'Y') e(0);

	if (sendto(fd2, buf, val, 0, (struct sockaddr *)&sin, sizeof(sin)) !=
	    val) e(0);

	buf[0] = 'Z';

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = 0;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (recvmsg(fd, &msg, 0) != 0) e(0);
	if (msg.msg_flags != MSG_TRUNC) e(0);
	if (buf[0] != 'Z') e(0);

	/* Make sure that zero-sized packets can be sent and received. */
	if (sendto(fd2, buf, 0, 0, (struct sockaddr *)&sin, sizeof(sin)) != 0)
		e(0);

	/*
	 * Note how we currently assume that packets sent over localhost will
	 * arrive immediately, so that we can use MSG_DONTWAIT to avoid that
	 * the test freezes.
	 */
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (recvmsg(fd, &msg, MSG_DONTWAIT) != 0) e(0);
	if (msg.msg_flags != 0) e(0);
	if (buf[0] != 'Z') e(0);

	if (recv(fd, buf, val, MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	/*
	 * When sending lots of small packets, ensure that fewer packets arrive
	 * than we sent.  This sounds weird, but we cannot actually check the
	 * internal TCP/IP buffer granularity and yet we want to make sure that
	 * the receive queue is measured in terms of buffers rather than packet
	 * sizes.  In addition, we check that older packets are favored,
	 * instead discarding new ones when the receive buffer is full.
	 */
	for (i = 0; i < 65536 / sizeof(j); i++) {
		j = i;
		if (sendto(fd2, &j, sizeof(j), 0, (struct sockaddr *)&sin,
		    sizeof(sin)) != sizeof(j)) e(0);
	}

	for (i = 0; i < 1025; i++) {
		r = recv(fd, &j, sizeof(j), MSG_DONTWAIT);
		if (r == -1) {
			if (errno != EWOULDBLOCK) e(0);
			break;
		}
		if (r != sizeof(j)) e(0);
		if (i != j) e(0);
	}
	if (i == 1025) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	free(buf);
}

/*
 * Test setting and retrieving IP-level options for the given socket type.  For
 * TCP sockets, we cannot test whether they are actually applied, but for UDP
 * sockets, we do a more complete test later on.
 */
static void
sub91f(int type)
{
	socklen_t len;
	int fd, val, def;

	/* Test IPv4 first. */
	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	/* Test obtaining the default TOS and TTL values. */
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IP, IP_TOS, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	len = sizeof(def);
	if (getsockopt(fd, IPPROTO_IP, IP_TTL, &def, &len) != 0) e(0);
	if (len != sizeof(def)) e(0);
	if (def < 16 || def > UINT8_MAX) e(0);

	/* Test changing the TOS field. */
	for (val = 0; val <= UINT8_MAX; val++)
		if (setsockopt(fd, IPPROTO_IP, IP_TOS, &val, sizeof(val)) != 0)
			e(0);
	val = -1; /* not a special value for IPv4 */
	if (setsockopt(fd, IPPROTO_IP, IP_TOS, &val, sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);
	val = UINT8_MAX + 1;
	if (setsockopt(fd, IPPROTO_IP, IP_TOS, &val, sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IP, IP_TOS, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != UINT8_MAX) e(0);

	/* Test changing the TTL field. */
	for (val = 0; val <= UINT8_MAX; val++)
		if (setsockopt(fd, IPPROTO_IP, IP_TTL, &val, sizeof(val)) != 0)
			e(0);
	val = 39;
	if (setsockopt(fd, IPPROTO_IP, IP_TTL, &val, sizeof(val)) != 0) e(0);
	val = -1; /* not a special value for IPv4 */
	if (setsockopt(fd, IPPROTO_IP, IP_TTL, &val, sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);
	val = UINT8_MAX + 1;
	if (setsockopt(fd, IPPROTO_IP, IP_TTL, &val, sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IP, IP_TTL, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 39) e(0);

	/* It must not be possible to set IPv6 options on IPv4 sockets. */
	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, sizeof(val)) != -1)
		e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, &len) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, &len) != -1)
		e(0);
	if (errno != ENOPROTOOPT) e(0);

	if (close(fd) != 0) e(0);

	/* Test IPv6 next. */
	if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

	/* Test obtaining the default TCLASS and HOPS values. */
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	len = sizeof(def);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &def, &len) != 0)
		e(0);
	if (len != sizeof(def)) e(0);
	if (def < 16 || def > UINT8_MAX) e(0);

	/* Test changing the TCLASS field. */
	for (val = 0; val <= UINT8_MAX; val++)
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val,
		    sizeof(val)) != 0) e(0);
	val = -2;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, sizeof(val)) != -1)
		e(0);
	if (errno != EINVAL) e(0);
	val = UINT8_MAX + 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, sizeof(val)) != -1)
		e(0);
	if (errno != EINVAL) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != UINT8_MAX) e(0);

	val = -1; /* reset to default */
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, sizeof(val)) != 0)
		e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	/* Test changing the HOPS field. */
	for (val = 0; val <= UINT8_MAX; val++)
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
		    sizeof(val)) != 0) e(0);
	val = 49;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
	    sizeof(val)) != 0) e(0);
	val = -2;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);
	val = UINT8_MAX + 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 49) e(0);

	val = -1; /* reset to default */
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
	    sizeof(val)) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != def) e(0);

	/* It must not be possible to set IPv4 options on IPv6 sockets. */
	val = 0;
	if (setsockopt(fd, IPPROTO_IP, IP_TOS, &val, sizeof(val)) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (setsockopt(fd, IPPROTO_IP, IP_TTL, &val, sizeof(val)) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IP, IP_TOS, &val, &len) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (getsockopt(fd, IPPROTO_IP, IP_TTL, &val, &len) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test setting and retrieving IP-level options.
 */
static void
test91f(void)
{

	subtest = 6;

	sub91f(SOCK_STREAM);

	sub91f(SOCK_DGRAM);
}

/*
 * Test setting and retrieving IP-level options on UDP sockets and packets.
 * As part of this, ensure that the maximum set of supported control options
 * can be both sent and received, both for IPv4 and IPv6.  Any options that are
 * newly added to the service and may be combined with the existing ones should
 * be added to this subtest as well.  The control data handling code is shared
 * between UDP and RAW, so there is no need to repeat this test for the latter.
 */
static void
test91g(void)
{
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg, cmsg2;
	struct in_pktinfo ipi;
	struct in6_pktinfo ipi6;
	unsigned int ifindex;
	char buf[1];
	union {
		struct cmsghdr cmsg;
		char buf[256];
	} control;
	uint8_t byte;
	size_t size;
	int fd, fd2, val, seen_tos, seen_ttl, seen_pktinfo;

	subtest = 7;

	ifindex = if_nametoindex(LOOPBACK_IFNAME);
	if (ifindex == 0) e(0);

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	val = 1;
	/* Strangely, IP_RECVTOS is not a thing.. */
	if (setsockopt(fd, IPPROTO_IP, IP_RECVTTL, &val, sizeof(val)) != 0)
		e(0);
	if (setsockopt(fd, IPPROTO_IP, IP_RECVPKTINFO, &val, sizeof(val)) != 0)
		e(0);

	if ((fd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	iov.iov_base = "A";
	iov.iov_len = 1;

	val = 39;
	control.cmsg.cmsg_len = CMSG_LEN(sizeof(val));
	control.cmsg.cmsg_level = IPPROTO_IP;
	control.cmsg.cmsg_type = IP_TTL;
	memcpy(CMSG_DATA(&control.cmsg), &val, sizeof(val));

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr *)&sin;
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = control.cmsg.cmsg_len;

	if (sendmsg(fd2, &msg, 0) != 1) e(0);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = sizeof(control);

	if (recvmsg(fd, &msg, 0) != 1) e(0);
	if (buf[0] != 'A') e(0);

	seen_ttl = seen_pktinfo = 0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != IPPROTO_IP) e(0);
		switch (cmsg->cmsg_type) {
		case IP_TTL:
			/* The odd one out, using a uint8_t.. */
			if (seen_ttl++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(byte))) e(0);
			memcpy(&byte, CMSG_DATA(cmsg), sizeof(byte));
			if (byte != 39) e(0);
			break;
		case IP_PKTINFO:
			if (seen_pktinfo++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(ipi))) e(0);
			memcpy(&ipi, CMSG_DATA(cmsg), sizeof(ipi));
			if (ipi.ipi_addr.s_addr != sin.sin_addr.s_addr) e(0);
			if (ipi.ipi_ifindex != ifindex) e(0);
			break;
		default:
			e(0);
		}
	}
	if (!seen_ttl) e(0);
	if (!seen_pktinfo) e(0);

	/* Test that we can provide all supported IPv4 options at once. */
	iov.iov_base = "B";
	iov.iov_len = 1;

	val = 1;
	control.cmsg.cmsg_len = CMSG_LEN(sizeof(val));
	control.cmsg.cmsg_level = IPPROTO_IP;
	control.cmsg.cmsg_type = IP_TOS;
	memcpy(CMSG_DATA(&control.cmsg), &val, sizeof(val));

	size = CMSG_SPACE(sizeof(val));

	if ((cmsg = CMSG_NXTHDR(&msg, &control.cmsg)) == NULL) e(0);
	val = 41;
	cmsg2.cmsg_len = CMSG_LEN(sizeof(val));
	cmsg2.cmsg_level = IPPROTO_IP;
	cmsg2.cmsg_type = IP_TTL;
	memcpy(cmsg, &cmsg2, sizeof(cmsg2));
	memcpy(CMSG_DATA(cmsg), &val, sizeof(val));

	size += CMSG_SPACE(sizeof(val));

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr *)&sin;
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = size;

	if (sendmsg(fd2, &msg, 0) != 1) e(0);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = sizeof(control);

	if (recvmsg(fd, &msg, 0) != 1) e(0);
	if (buf[0] != 'B') e(0);

	/* Check just the TTL this time. */
	seen_ttl = 0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != IPPROTO_IP) e(0);
		if (cmsg->cmsg_type == IP_TTL) {
			/* The odd one out, using a uint8_t.. */
			if (seen_ttl++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(byte))) e(0);
			memcpy(&byte, CMSG_DATA(cmsg), sizeof(byte));
			if (byte != 41) e(0);
		}
	}
	if (!seen_ttl) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* That was IPv4, onto IPv6.. */
	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVTCLASS, &val,
	    sizeof(val)) != 0) e(0);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &val,
	    sizeof(val)) != 0) e(0);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val,
	    sizeof(val)) != 0) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	val = 94;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_TCLASS, &val, sizeof(val)) != 0)
		e(0);

	iov.iov_base = "C";
	iov.iov_len = 1;

	val = 39;
	control.cmsg.cmsg_len = CMSG_LEN(sizeof(val));
	control.cmsg.cmsg_level = IPPROTO_IPV6;
	control.cmsg.cmsg_type = IPV6_HOPLIMIT;
	memcpy(CMSG_DATA(&control.cmsg), &val, sizeof(val));

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr *)&sin6;
	msg.msg_namelen = sizeof(sin6);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = control.cmsg.cmsg_len;

	if (sendmsg(fd2, &msg, 0) != 1) e(0);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = sizeof(control);

	if (recvmsg(fd, &msg, 0) != 1) e(0);
	if (buf[0] != 'C') e(0);

	seen_tos = seen_ttl = seen_pktinfo = 0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != IPPROTO_IPV6) e(0);
		switch (cmsg->cmsg_type) {
		case IPV6_TCLASS:
			if (seen_tos++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(val))) e(0);
			memcpy(&val, CMSG_DATA(cmsg), sizeof(val));
			if (val != 94) e(0);
			break;
		case IPV6_HOPLIMIT:
			if (seen_ttl++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(val))) e(0);
			memcpy(&val, CMSG_DATA(cmsg), sizeof(val));
			if (val != 39) e(0);
			break;
		case IPV6_PKTINFO:
			if (seen_pktinfo++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(ipi6))) e(0);
			memcpy(&ipi6, CMSG_DATA(cmsg), sizeof(ipi6));
			if (memcmp(&ipi6.ipi6_addr, &in6addr_loopback,
			    sizeof(ipi6.ipi6_addr))) e(0);
			if (ipi6.ipi6_ifindex != ifindex) e(0);
			break;
		default:
			e(0);
		}
	}
	if (!seen_tos) e(0);
	if (!seen_ttl) e(0);
	if (!seen_pktinfo) e(0);

	/*
	 * Test that (for IPv6) an option of -1 overrides setsockopt.
	 * Also test that we can provide all supported IPv6 options at once.
	 */
	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val,
	    sizeof(val)) != 0) e(0);

	iov.iov_base = "D";
	iov.iov_len = 1;

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = control.buf;
	msg.msg_controllen = sizeof(control.buf);

	val = -1;
	control.cmsg.cmsg_len = CMSG_LEN(sizeof(val));
	control.cmsg.cmsg_level = IPPROTO_IPV6;
	control.cmsg.cmsg_type = IPV6_TCLASS;
	memcpy(CMSG_DATA(&control.cmsg), &val, sizeof(val));

	size = CMSG_SPACE(sizeof(val));

	if ((cmsg = CMSG_NXTHDR(&msg, &control.cmsg)) == NULL) e(0);
	val = 78;
	cmsg2.cmsg_len = CMSG_LEN(sizeof(val));
	cmsg2.cmsg_level = IPPROTO_IPV6;
	cmsg2.cmsg_type = IPV6_HOPLIMIT;
	memcpy(cmsg, &cmsg2, sizeof(cmsg2));
	memcpy(CMSG_DATA(cmsg), &val, sizeof(val));

	size += CMSG_SPACE(sizeof(val));

	if ((cmsg = CMSG_NXTHDR(&msg, cmsg)) == NULL) e(0);
	cmsg2.cmsg_len = CMSG_LEN(sizeof(ipi6));
	cmsg2.cmsg_level = IPPROTO_IPV6;
	cmsg2.cmsg_type = IPV6_PKTINFO;
	memcpy(cmsg, &cmsg2, sizeof(cmsg2));
	memset(&ipi6, 0, sizeof(ipi6));
	memcpy(CMSG_DATA(cmsg), &ipi6, sizeof(ipi6));

	size += CMSG_SPACE(sizeof(ipi6));

	if (size > sizeof(control.buf)) e(0);

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr *)&sin6;
	msg.msg_namelen = sizeof(sin6);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = size;

	if (sendmsg(fd2, &msg, 0) != 1) e(0);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = sizeof(control);

	if (recvmsg(fd, &msg, 0) != 1) e(0);
	if (buf[0] != 'D') e(0);

	seen_tos = seen_ttl = 0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != IPPROTO_IPV6) e(0);
		switch (cmsg->cmsg_type) {
		case IPV6_TCLASS:
			if (seen_tos++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(val))) e(0);
			memcpy(&val, CMSG_DATA(cmsg), sizeof(val));
			if (val != 0) e(0);
			break;
		case IPV6_HOPLIMIT:
			if (seen_ttl++) e(0);
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(val))) e(0);
			memcpy(&val, CMSG_DATA(cmsg), sizeof(val));
			if (val != 78) e(0);
			break;
		default:
			e(0);
		}
	}
	if (!seen_tos) e(0);
	if (!seen_ttl) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);
}

/*
 * Test receiving IPv4 packets on IPv6 sockets.
 */
static void
test91h(void)
{
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct in6_pktinfo ipi6;
	unsigned int ifindex;
	char buf[1], buf2[256];
	int fd, fd2, val;

	subtest = 8;

	ifindex = if_nametoindex(LOOPBACK_IFNAME);
	if (ifindex == 0) e(0);

	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val,
	    sizeof(val)) != 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	if ((fd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (sendto(fd2, "A", 1, 0, (struct sockaddr *)&sin, sizeof(sin)) != 1)
		e(0);

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (struct sockaddr *)&sin6;
	msg.msg_namelen = sizeof(sin6);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = buf2;
	msg.msg_controllen = sizeof(buf2);

	if (recvmsg(fd, &msg, 0) != 1) e(0);
	if (buf[0] != 'A') e(0);

	if (msg.msg_namelen != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	if ((cmsg = CMSG_FIRSTHDR(&msg)) == NULL) e(0);
	if (cmsg->cmsg_level != IPPROTO_IPV6) e(0);
	if (cmsg->cmsg_type != IPV6_PKTINFO) e(0);
	if (cmsg->cmsg_len != CMSG_LEN(sizeof(ipi6))) e(0);

	/*
	 * The packet was sent from loopback to loopback, both with IPv4-mapped
	 * IPv6 addresses, so we can simply compare source and destination.
	 */
	memcpy(&ipi6, CMSG_DATA(cmsg), sizeof(ipi6));
	if (memcmp(&sin6.sin6_addr, &ipi6.ipi6_addr, sizeof(sin6.sin6_addr)))
		e(0);
	if (ipi6.ipi6_ifindex != ifindex) e(0);

	if (CMSG_NXTHDR(&msg, cmsg) != NULL) e(0);

	/*
	 * Sqeeze in a quick test to see what happens if the receiver end does
	 * not provide a control buffer after having requested control data,
	 * because a half-complete version of this test triggered a bug there..
	 */
	if (sendto(fd2, "B", 1, 0, (struct sockaddr *)&sin, sizeof(sin)) != 1)
		e(0);

	if (recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL) != 1) e(0);
	if (buf[0] != 'B') e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);
}

/*
 * Test that binding a socket of the given type to a privileged port is
 * disallowed.
 */
static void
sub91i(int type)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	int fd, port;

	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	for (port = IPPORT_RESERVED - 1; port >= 0; port--) {
		sin.sin_port = htons(port);

		if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
		if (errno == EADDRINUSE) continue;
		if (errno != EACCES) e(0);
		break;
	}

	for (port = IPPORT_RESERVED; port <= UINT16_MAX; port++) {
		sin.sin_port = htons(port);

		if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == 0)
			break;
		if (errno != EADDRINUSE) e(0);
	}

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	for (port = IPV6PORT_RESERVED - 1; port >= 0; port--) {
		sin6.sin6_port = htons(port);

		if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != -1)
			e(0);
		if (errno == EADDRINUSE) continue;
		if (errno != EACCES) e(0);
		break;
	}

	for (port = IPV6PORT_RESERVED; port <= UINT16_MAX; port++) {
		sin6.sin6_port = htons(port);

		if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) == 0)
			break;
		if (errno != EADDRINUSE) e(0);
	}

	if (close(fd) != 0) e(0);
}

/*
 * Test that binding to privileged ports is disallowed for non-root users.
 * Also make sure that such users cannot create raw sockets at all.  This test
 * is not to be run by root, but for convenience we first try to drop
 * privileges for the duration of the test anyway.
 */
static void
test91i(void)
{
	int i;

	subtest = 9;

	(void)seteuid(1);

	sub91i(SOCK_STREAM);

	sub91i(SOCK_DGRAM);

	for (i = 0; i < IPPROTO_MAX; i++) {
		if (socket(AF_INET, SOCK_RAW, i) != -1) e(0);
		if (errno != EACCES) e(0);
		if (socket(AF_INET6, SOCK_RAW, i) != -1) e(0);
		if (errno != EACCES) e(0);
	}

	(void)seteuid(0);
}

/*
 * Test setting and getting basic UDP/RAW multicast transmission options.
 */
static void
test91j(void)
{

	subtest = 10;

	socklib_multicast_tx_options(SOCK_DGRAM);
}

/*
 * Test TCP socket state changes related to the listen queue.  This test is
 * derived from test90y, but sufficiently different to be its own copy.
 */
static void
test91k(void)
{
	struct sockaddr_in6 sin6A, sin6B, sin6C;
	socklen_t len;
	struct timeval tv;
	struct linger l;
	fd_set fds;
	char buf[7];
	int fd, fd2, fd3, fd4, val, fl;

	subtest = 11;

	if ((fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) e(0);

	val = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;
	sin6A.sin6_port = htons(TEST_PORT_A);
	memcpy(&sin6A.sin6_addr, &in6addr_loopback, sizeof(sin6A.sin6_addr));

	if (bind(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	/*
	 * Any socket options should be inherited from the listening socket at
	 * connect time, and not be re-inherited at accept time, to the extent
	 * that they are inherited at all.  TCP/IP level options are not.
	 */
	val = 123;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &val, sizeof(val)) != 0)
		e(0);
	val = 32768;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val)) != 0)
		e(0);

	if (listen(fd, 5) != 0) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	memset(&sin6B, 0, sizeof(sin6B));
	sin6B.sin6_family = AF_INET6;
	sin6B.sin6_port = htons(0);
	memcpy(&sin6B.sin6_addr, &in6addr_loopback, sizeof(sin6B.sin6_addr));

	val = 1;
	if (setsockopt(fd2, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) != 0)
		e(0);

	if (bind(fd2, (struct sockaddr *)&sin6B, sizeof(sin6B)) != 0) e(0);

	len = sizeof(sin6B);
	if (getsockname(fd2, (struct sockaddr *)&sin6B, &len) != 0) e(0);
	if (len != sizeof(sin6B)) e(0);
	if (sin6B.sin6_port == htons(0)) e(0);

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	val = 456;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &val, sizeof(val)) != 0)
		e(0);
	val = 16384;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val)) != 0)
		e(0);

	/*
	 * Obtaining the peer name should work.  As always, the name should be
	 * inherited from the listening socket.
	 */
	len = sizeof(sin6C);
	if (getpeername(fd2, (struct sockaddr *)&sin6C, &len) != 0) e(0);
	if (sin6C.sin6_len != sizeof(sin6C)) e(0);
	if (sin6C.sin6_family != AF_INET6) e(0);
	if (sin6C.sin6_port != htons(TEST_PORT_A)) e(0);
	if (memcmp(&sin6C.sin6_addr, &in6addr_loopback,
	    sizeof(sin6C.sin6_addr)) != 0) e(0);

	/*
	 * Sending to the socket should work, and it should be possible to
	 * receive the data from the other side once accepted.
	 */
	if (send(fd2, "Hello, ", 7, 0) != 7) e(0);
	if (send(fd2, "world!", 6, 0) != 6) e(0);

	/* Shutdown settings should be visible after accepting, too. */
	if (shutdown(fd2, SHUT_RDWR) != 0) e(0);

	memset(&sin6C, 0, sizeof(sin6C));
	len = sizeof(sin6C);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6C, &len)) < 0) e(0);
	if (sin6C.sin6_len != sizeof(sin6C)) e(0);
	if (sin6C.sin6_family != AF_INET6) e(0);
	if (sin6C.sin6_port != sin6B.sin6_port) e(0);
	if (memcmp(&sin6C.sin6_addr, &in6addr_loopback,
	    sizeof(sin6C.sin6_addr)) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd3, SOL_SOCKET, SO_SNDLOWAT, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 123) e(0);

	len = sizeof(val);
	if (getsockopt(fd3, SOL_SOCKET, SO_RCVBUF, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 32768) e(0);

	if ((fl = fcntl(fd3, F_GETFL)) == -1) e(0);
	if (!(fl & O_NONBLOCK)) e(0);
	if (fcntl(fd3, F_SETFL, fl & ~O_NONBLOCK) != 0) e(0);

	if (recv(fd3, buf, 7, 0) != 7) e(0);
	if (memcmp(buf, "Hello, ", 7) != 0) e(0);
	if (recv(fd3, buf, 7, 0) != 6) e(0);
	if (memcmp(buf, "world!", 6) != 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 0) e(0);

	/*
	 * Unlike in the UDS test, the other side's shutdown-for-reading is not
	 * visible to this side, so sending data should work just fine until we
	 * close or shut down the socket ourselves.  The other side will simply
	 * discard the incoming data.
	 */
	if (send(fd3, "", 1, MSG_NOSIGNAL) != 1) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);

	/*
	 * If the connection pending acceptance is closed, the connection must
	 * remain on the queue, and the accepting party will read EOF from it.
	 * Try once without pending data, once with pending data.
	 */
	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (close(fd2) != 0) e(0);

	len = sizeof(sin6B);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6B, &len)) < 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd3, SOL_SOCKET, SO_SNDLOWAT, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 456) e(0);

	len = sizeof(val);
	if (getsockopt(fd3, SOL_SOCKET, SO_RCVBUF, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 16384) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 0) e(0);

	if (close(fd3) != 0) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (send(fd2, "Hello!", 6, 0) != 6) e(0);
	if (close(fd2) != 0) e(0);

	len = sizeof(sin6B);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6B, &len)) < 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 6) e(0);
	if (memcmp(buf, "Hello!", 6) != 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 0) e(0);

	if (close(fd3) != 0) e(0);

	/*
	 * If the connection pending acceptance is aborted, the listening
	 * socket should pretend as though the connection was never there.
	 */
	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	memset(&l, 0, sizeof(l));
	l.l_onoff = 1;
	l.l_linger = 0;
	if (setsockopt(fd2, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) != 0) e(0);

	if (close(fd2) != 0) e(0);

	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);
	if (FD_ISSET(fd, &fds)) e(0);

	len = sizeof(sin6B);
	if (accept(fd, (struct sockaddr *)&sin6B, &len) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	/*
	 * Try the same thing, but now with the connection sandwiched between
	 * two different pending connections, which should be left intact.
	 */
	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (send(fd2, "A", 1, 0) != 1) e(0);

	if ((fd3 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd3, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (send(fd3, "B", 1, 0) != 1) e(0);

	if ((fd4 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd4, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (send(fd4, "C", 1, 0) != 1) e(0);

	if (setsockopt(fd3, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) != 0) e(0);

	if (close(fd3) != 0) e(0);

	len = sizeof(sin6B);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6B, &len)) < 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'A') e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd2) != 0) e(0);

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	len = sizeof(sin6B);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6B, &len)) < 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'C') e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd4) != 0) e(0);

	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);
	if (FD_ISSET(fd, &fds)) e(0);

	len = sizeof(sin6B);
	if (accept(fd, (struct sockaddr *)&sin6B, &len) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	/*
	 * If the listening socket was closed, the sockets pending acceptance
	 * should be reset.
	 */
	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if ((fd3 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd3, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (close(fd) != 0) e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != -1) e(0);
	if (errno != ECONNRESET) e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != -1) e(0);
	if (errno != ECONNRESET) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 0) e(0);

	if (close(fd3) != 0) e(0);

	if (close(fd2) != 0) e(0);
}

/*
 * Obtain a pair of connected TCP socket.
 */
static int
get_tcp_pair(int domain, int type, int protocol, int fd[2])
{
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	struct sockaddr *addr;
	socklen_t addr_len, len;
	int lfd, val;

	if (domain == AF_INET6) {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		memcpy(&sin6.sin6_addr, &in6addr_loopback,
		    sizeof(sin6.sin6_addr));

		addr = (struct sockaddr *)&sin6;
		addr_len = sizeof(sin6);
	} else {
		assert(domain == AF_INET);

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		addr = (struct sockaddr *)&sin;
		addr_len = sizeof(sin);
	}

	if ((lfd = socket(domain, type, protocol)) < 0) e(0);

	if (bind(lfd, addr, addr_len) != 0) e(0);

	len = addr_len;
	if (getsockname(lfd, addr, &len) != 0) e(0);
	if (len != addr_len) e(0);

	if (listen(lfd, 1) != 0) e(0);

	if ((fd[0] = socket(domain, type, protocol)) < 0) e(0);

	val = 1;
	if (setsockopt(fd[0], IPPROTO_TCP, TCP_NODELAY, &val,
	    sizeof(val)) != 0) e(0);

	if (connect(fd[0], addr, addr_len) != 0) e(0);

	len = addr_len;
	if ((fd[1] = accept(lfd, addr, &len)) < 0) e(0);
	if (len != addr_len) e(0);

	if (setsockopt(fd[1], IPPROTO_TCP, TCP_NODELAY, &val,
	    sizeof(val)) != 0) e(0);

	if (close(lfd) != 0) e(0);

	return 0;
}

/*
 * Test large transfers and MSG_WAITALL.
 */
static void
test91l(void)
{
	int fd[2];

	subtest = 12;

	get_tcp_pair(AF_INET6, SOCK_STREAM, 0, fd);

	socklib_large_transfers(fd);

	get_tcp_pair(AF_INET, SOCK_STREAM, 0, fd);

	socklib_large_transfers(fd);
}

/*
 * A randomized producer-consumer test for stream sockets.  As part of this,
 * we also perform very basic bulk functionality tests of FIONREAD, MSG_PEEK,
 * MSG_DONTWAIT, and MSG_WAITALL.
 */
static void
test91m(void)
{
	int fd[2];

	subtest = 13;

	get_tcp_pair(AF_INET6, SOCK_STREAM, 0, fd);

	socklib_producer_consumer(fd);

	get_tcp_pair(AF_INET, SOCK_STREAM, 0, fd);

	socklib_producer_consumer(fd);
}

/*
 * Cause a receive call on the peer side of the connection of 'fd' to be
 * aborted in a protocol-specific way.  Return -1 to indicate that the given
 * file descriptor has been closed.
 */
static int
test91_reset(int fd, const char * data __unused, size_t len __unused)
{
	struct linger l;

	l.l_onoff = 1;
	l.l_linger = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) != 0) e(0);

	if (close(fd) != 0) e(0);

	return -1;
}

/*
 * Test for receiving on stream sockets.  In particular, test SO_RCVLOWAT,
 * MSG_PEEK, MSG_DONTWAIT, and MSG_WAITALL.
 */
static void
test91n(void)
{

	subtest = 14;

	socklib_stream_recv(get_tcp_pair, AF_INET, SOCK_STREAM,
	    test91_reset);
}

/*
 * Return the send and receive buffer sizes for sockets of the given type.  The
 * two individual values are stored in 'sndbuf' and 'rcvbuf', for each that is
 * not NULL, and the sum is returned from the call.
 */
static int
get_buf_sizes(int type, int * sndbufp, int * rcvbufp)
{
	socklen_t len;
	int fd, sndbuf, rcvbuf;

	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	len = sizeof(sndbuf);
	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &len) != 0) e(0);
	if (len != sizeof(sndbuf)) e(0);
	if (sndbufp != NULL)
		*sndbufp = sndbuf;

	len = sizeof(rcvbuf);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &len) != 0) e(0);
	if (len != sizeof(rcvbuf)) e(0);
	if (rcvbufp != NULL)
		*rcvbufp = rcvbuf;

	if (close(fd) != 0) e(0);

	return sndbuf + rcvbuf;
}

/*
 * The following constant should be set to the window size used within lwIP.
 * There is currently no way to obtain this constant from the LWIP service, nor
 * would that be information that should ever be used by general applications,
 * but we need it to fill socket receive queues in a reliable way.  TODO: find
 * a better solution for this general problem.
 */
#define WINDOW_SIZE	16384	/* TCP_WND in lwipopt.h, keep in sync! */

#define CHUNK		4096	/* base I/O chunk size */
#define USLEEP_TIME	250000	/* increase on wimpy platforms if needed */

/*
 * Fill the receive of socket 'rfd' with data, and if 'fill_send' is non-zero,
 * also the send queue of socket 'sfd'.  If 'fill_send' is zero, 'delta' may be
 * a non-zero value indicating how many bytes extra (delta > 0) or fewer
 * (delta < 0) should be sent compared to the receive queue size.
 */
static void
fill_tcp_bufs(int sfd, int rfd, int fill_send, int delta)
{
	unsigned char buf[CHUNK], c;
	socklen_t len;
	int sndbuf, rcvbuf, mss, chunk, left, res;

	assert(!fill_send || delta == 0);

	(void)get_buf_sizes(SOCK_STREAM, &sndbuf, &rcvbuf);

	len = sizeof(mss);
	if (getsockopt(sfd, IPPROTO_TCP, TCP_MAXSEG, &mss, &len) != 0) e(0);

	left = rcvbuf;
	if (delta < 0)
		left += delta;

	memset(buf, 0, sizeof(buf));

	/*
	 * In general, TCP is not designed for what we want to do here, which
	 * is to control the contents of the receive buffer down to the last
	 * byte.  We already assume that the caller has disabled the Nagle
	 * algorithm, but we still have to deal with other algorithms that
	 * effectively get in the way of full control of the receive buffer.
	 *
	 * In particular, we have to work around an issue where lwIP decides to
	 * start shrinking the window earlier than necessary.  This issue
	 * triggers during the transition from a fully open window to a reduced
	 * window.  If no acknowledgement is sent when exactly that point is
	 * reached, the next acknowlegment will not announce the full size of
	 * the remainder of the window.  This appears to be part of the silly
	 * window avoidance logic, so it is probably intentional behavior and
	 * thus we have to work around it.
	 *
	 * So far it appears that filling up just the window size does the job,
	 * as long as the last segment is a full MSS-sized segment and each
	 * segment is acknowledged (which is why we send data in the other
	 * direction).  Anything short of that may trigger edge cases that, in
	 * some cases, show up only on slow platforms (e.g. BeagleBones).
	 *
	 * Note that while test91z also fills up receive queues using its own
	 * algorithm, it sets the receive queue to the window size, thereby
	 * avoiding the need for this more complicated algorithm.
	 */
	for (left = rcvbuf - WINDOW_SIZE; left > 0; left -= chunk) {
		chunk = (left % mss != 0) ? (left % mss) : mss;
		assert(chunk <= left);

		if (send(sfd, buf, chunk, 0) != chunk) e(0);

		if (send(rfd, ".", 1, 0) != 1) e(0);

		if (recv(sfd, &c, 1, 0) != 1) e(0);
		if (c != '.') e(0);
	}

	/* We are done with the hard part.  Now fill up the rest. */
	if (fill_send)
		delta = sndbuf;

	for (left = WINDOW_SIZE + delta; left > 0; left -= res) {
		chunk = MIN(left, sizeof(buf));

		res = send(sfd, buf, chunk, 0);

		if (res <= 0) e(0);
		if (res > chunk) e(0);
	}
}

/*
 * Signal handler which just needs to exist, so that invoking it will interrupt
 * an ongoing system call.
 */
static void
test91_got_signal(int sig __unused)
{

	/* Nothing. */
}

/*
 * Test for sending on stream sockets.  The quick summary here is that send()
 * should basically act as the mirror of recv(MSG_WAITALL), i.e., it should
 * keep suspending until all data is sent (or the call is interrupted or no
 * more can possibly be sent), and, SO_SNDLOWAT, mirroring SO_RCVLOWAT, acts as
 * an admission test for the send: nothing is sent until there is room in the
 * send buffer (i.e., the peer's receive buffer) for at least the low send
 * watermark, or the whole send request length, whichever is smaller.  In
 * addition, select(2) should use the same threshold.
 *
 * This test is a copy of test90v, and would be in socklib instead, were it not
 * for the fact that TCP's segmentation and silly window avoidance make it
 * impossible to perform the same exact, byte-granular test.  Instead, this TCP
 * implementation paints with a somewhat broader brush, using send and receive
 * chunk sizes large enough to overcome the normally desirable TCP features
 * that are now getting in the way.  As a result, this copy of the test is not
 * only somewhat less effective but also a bit more reliant on specific (TCP)
 * settings, although the whole test is still way too useful to skip at all.
 */
static void
sub91o(int iroom, int istate, int slowat, int len, int bits, int act)
{
	struct sigaction sa;
	struct timeval tv;
	char buf[CHUNK * 4];
	fd_set fds;
	pid_t pid;
	int fd[2], min, flags, res, err;
	int pfd[2], orig_iroom, eroom, tstate, fl, status;

	if (get_tcp_pair(AF_INET6, SOCK_STREAM, 0, fd) != 0) e(0);

	/*
	 * Set up the initial condition on the sockets.
	 */
	fill_tcp_bufs(fd[0], fd[1], 1 /*fill_send*/, 0 /*delta*/);

	/*
	 * Receive a bit more than we send, to free up enough room (the MSS) to
	 * get things going again.
	 */
	orig_iroom = iroom;
	iroom += iroom / 2;
	if (iroom > 0)
		if (recv(fd[1], buf, iroom, 0) != iroom) e(0);

	switch (istate) {
	case 0: break;
	case 1: if (shutdown(fd[0], SHUT_WR) != 0) e(0); break;
	case 2: if (close(fd[1]) != 0) e(0); break;
	}

	if (setsockopt(fd[0], SOL_SOCKET, SO_SNDLOWAT, &slowat,
	    sizeof(slowat)) != 0) e(0);

	/* SO_SNDLOWAT is always bounded by the actual send length. */
	min = MIN(len, slowat);

	flags = MSG_NOSIGNAL;
	if (bits & 1) flags |= MSG_DONTWAIT;

	/*
	 * Do a quick select test to see if its result indeed matches whether
	 * the available space in the "send" buffer meets the threshold.
	 */
	FD_ZERO(&fds);
	FD_SET(fd[0], &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	res = select(fd[0] + 1, NULL, &fds, NULL, &tv);
	if (res < 0 || res > 1) e(0);
	if (res != (iroom >= slowat || istate > 0)) e(0);
	if (res == 1 && !FD_ISSET(fd[0], &fds)) e(0);

	/*
	 * Cut short a whole lot of cases, to avoid the overhead of forking,
	 * namely when we know the call should return immediately.  This is the
	 * case when the socket state disallows further sending, or when all
	 * data could be sent, or when the call was non-blocking.  The low
	 * send watermark only helps determine whether anything was sent here.
	 */
	if (istate > 0 || iroom >= len || (flags & MSG_DONTWAIT)) {
		res = send(fd[0], buf, len, flags);

		if (istate > 0) {
			if (res != -1) e(0);
			if (errno != EPIPE && errno != ECONNRESET) e(0);
		} else if (iroom >= len) {
			if (res != len) e(0);
		} else if (iroom >= min) {
			if (res < orig_iroom || res > iroom) e(0);
		} else {
			if (res != -1) e(0);
			if (errno != EWOULDBLOCK) e(0);
		}

		/* Early cleanup and return to avoid even more code clutter. */
		if (istate != 2 && close(fd[1]) != 0) e(0);
		if (close(fd[0]) != 0) e(0);

		return;
	}

	/*
	 * Now starts the interesting stuff: the send call should now block,
	 * even though if we add MSG_DONTWAIT it may not return EWOULDBLOCK,
	 * because MSG_DONTWAIT prevents the send from blocking after partial
	 * completion.  As such, we can only test our expectations by letting
	 * the call block, in a child process, and waiting.  We do test as much
	 * of the above assumption as we can for safety right here, but this is
	 * not a substitute for actually blocking even in these cases!
	 */
	if (iroom < min) {
		if (send(fd[0], buf, len, flags | MSG_DONTWAIT) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);
	}

	/*
	 * If (act < 9), we receive 0, 1, or 2 bytes from the receive queue
	 * before forcing the send call to terminate in one of three ways.
	 *
	 * If (act == 9), we use a signal to interrupt the send call.
	 */
	if (act < 9) {
		eroom = (act % 3) * (CHUNK + CHUNK / 2 - 1);
		tstate = act / 3;
	} else
		eroom = tstate = 0;

	if (pipe2(pfd, O_NONBLOCK) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[1]) != 0) e(0);
		if (close(pfd[0]) != 0) e(0);

		if (act == 9) {
			memset(&sa, 0, sizeof(sa));
			sa.sa_handler = test91_got_signal;
			if (sigaction(SIGUSR1, &sa, NULL) != 0) e(0);
		}

		res = send(fd[0], buf, len, flags);
		err = errno;

		if (write(pfd[1], &res, sizeof(res)) != sizeof(res)) e(0);
		if (write(pfd[1], &err, sizeof(err)) != sizeof(err)) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(pfd[1]) != 0) e(0);

	/*
	 * Allow the child to enter the blocking send(2), and check the pipe
	 * to see if it is really blocked.
	 */
	if (usleep(USLEEP_TIME) != 0) e(0);

	if (read(pfd[0], &res, sizeof(res)) != -1) e(0);
	if (errno != EAGAIN) e(0);

	if (eroom > 0) {
		if (recv(fd[1], buf, eroom, 0) != eroom) e(0);

		/*
		 * The threshold for the send is now met if the entire request
		 * has been satisfied.
		 */
		if (iroom + eroom >= len) {
			if ((fl = fcntl(pfd[0], F_GETFL, 0)) == -1) e(0);
			if (fcntl(pfd[0], F_SETFL, fl & ~O_NONBLOCK) != 0)
				e(0);

			if (read(pfd[0], &res, sizeof(res)) != sizeof(res))
				e(0);
			if (read(pfd[0], &err, sizeof(err)) != sizeof(err))
				e(0);

			if (res != len) e(0);

			/* Bail out. */
			goto cleanup;
		}
	}

	if (act < 9) {
		/*
		 * Now test various ways to terminate the send call.
		 *
		 * For other socket drivers, there should also be a case where
		 * a socket error is raised instead.  For UDS there is no way
		 * to do that on stream-type sockets, not even with SO_LINGER.
		 */
		switch (tstate) {
		case 0: if (shutdown(fd[0], SHUT_WR) != 0) e(0); break;
		case 1: if (close(fd[1]) != 0) e(0); fd[1] = -1; break;
		case 2: fd[1] = test91_reset(fd[1], NULL, 0); break;
		}
	} else
		if (kill(pid, SIGUSR1) != 0) e(0);

	if ((fl = fcntl(pfd[0], F_GETFL, 0)) == -1) e(0);
	if (fcntl(pfd[0], F_SETFL, fl & ~O_NONBLOCK) != 0) e(0);

	if (read(pfd[0], &res, sizeof(res)) != sizeof(res)) e(0);
	if (read(pfd[0], &err, sizeof(err)) != sizeof(err)) e(0);

	/*
	 * If the send met the threshold before being terminate or interrupted,
	 * we should at least have sent something.  Otherwise, the send was
	 * never admitted and should return EPIPE or ECONNRESET (if the send
	 * was terminated) or EINTR (if the child was killed).
	 */
	if (iroom + eroom >= min) {
		if (res < MIN(orig_iroom, len)) e(0);
		if (res > MIN(iroom + eroom, len)) e(0);
	} else {
		if (res != -1) e(0);
		if (act < 9) {
			if (err != EPIPE && err != ECONNRESET) e(0);
		} else
			if (err != EINTR) e(0);
	}

cleanup:
	if (close(pfd[0]) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	if (fd[1] != -1 && close(fd[1]) != 0) e(0);
	if (close(fd[0]) != 0) e(0);
}

/*
 * Test for sending on stream sockets.  In particular, test SO_SNDLOWAT and
 * MSG_DONTWAIT.
 */
static void
test91o(void)
{
	int iroom, istate, slowat, len, bits, act;

	subtest = 15;

	/* Insanity. */
	for (iroom = 0; iroom <= CHUNK * 2; iroom += CHUNK)
		for (istate = 0; istate <= 2; istate++)
			for (slowat = CHUNK; slowat <= CHUNK * 2;
			    slowat += CHUNK)
				for (len = CHUNK; len <= CHUNK * 2;
				    len += CHUNK)
					for (bits = 0; bits < 2; bits++)
						for (act = 0; act <= 9; act++)
							sub91o(iroom, istate,
							    slowat, len, bits,
							    act);
}

/*
 * Test filling up the TCP receive queue.  In particular, verify that one bug I
 * ran into (lwIP bug #49128) is resolved.
 */
static void
test91p(void)
{
	char buf[CHUNK];
	size_t total, left;
	ssize_t res;
	int fd[2];

	subtest = 16;

	if (get_tcp_pair(AF_INET, SOCK_STREAM, 0, fd) != 0) e(0);

	/*
	 * Fill up the sockets' queues.
	 */
	total = get_buf_sizes(SOCK_STREAM, NULL, NULL);

	fill_tcp_bufs(fd[0], fd[1], 1 /*fill_send*/, 0 /*delta*/);

	/*
	 * Wait long enough for the zero window probing to kick in, which used
	 * to cause an ACK storm livelock (lwIP bug #49128).
	 */
	sleep(1);

	/*
	 * Actually sleep a bit longer, so that the polling timer kicks in and
	 * at least attempts to send more.  This is merely an attempt to
	 * exercise some of the polling code, and should not have any actual
	 * effect on the rest of the test.
	 */
	sleep(5);

	/*
	 * Make sure all the data still arrives.
	 */
	for (left = total; left > 0; left -= res) {
		res = recv(fd[1], buf, sizeof(buf), 0);
		if (res <= 0) e(0);
		if (res > left) e(0);
	}

	if (recv(fd[1], buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	/*
	 * Attempt to shut down the socket for writing after filling up the
	 * send queue.  The TCP FIN should then arrive after all the data.
	 */
	for (left = total; left > 0; left -= res) {
		res = send(fd[0], buf, MIN(left, sizeof(buf)), 0);
		if (res <= 0) e(0);
		if (res > left) e(0);
	}

	if (shutdown(fd[0], SHUT_WR) != 0) e(0);

	for (left = total; left > 0; left -= res) {
		res = recv(fd[1], buf, sizeof(buf), 0);
		if (res <= 0) e(0);
		if (res > left) e(0);
	}

	if (recv(fd[1], buf, sizeof(buf), 0) != 0) e(0);

	if (send(fd[1], "A", 1, 0) != 1) e(0);

	if (recv(fd[0], buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'A') e(0);

	if (close(fd[1]) != 0) e(0);
	if (close(fd[0]) != 0) e(0);
}

/*
 * Attempt to fill up a TCP send queue with small amounts of data.  While it
 * may or may not be possible to fill up the entire send queue with small
 * requests, but at least trying should not cause any problems, like the one I
 * filed as lwIP bug #49218.
 */
static void
test91q(void)
{
	ssize_t res;
	size_t count;
	char c, c2;
	int fd[2];

	subtest = 17;

	if (get_tcp_pair(AF_INET6, SOCK_STREAM, 0, fd) != 0) e(0);

	count = 0;
	for (c = 0; (res = send(fd[0], &c, sizeof(c), MSG_DONTWAIT)) > 0; c++)
		count += res;
	if (res != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);
	if (count < CHUNK) e(0);

	if (shutdown(fd[0], SHUT_WR) != 0) e(0);

	for (c2 = 0; count > 0; count--, c2++) {
		if (recv(fd[1], &c, sizeof(c), 0) != 1) e(0);
		if (c != c2) e(0);
	}

	if (recv(fd[1], &c, sizeof(c), 0) != 0) e(0);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);
}

/*
 * Test that SO_RCVLOWAT is limited to the size of the receive buffer.
 */
static void
sub91r_recv(int fill_delta, int rlowat_delta, int exp_delta)
{
	char *buf;
	size_t buflen;
	int fd[2], rlowat, rcvlen, res;

	if (get_tcp_pair(AF_INET, SOCK_STREAM, 0, fd) != 0) e(0);

	/*
	 * Fill up the socket's receive queue, possibly minus one byte.
	 */
	(void)get_buf_sizes(SOCK_STREAM, NULL, &rcvlen);

	buflen = MAX(CHUNK, rcvlen + 1);
	if ((buf = malloc(buflen)) == NULL) e(0);

	fill_tcp_bufs(fd[1], fd[0], 0 /*fill_send*/, fill_delta);

	rlowat = rcvlen + rlowat_delta;
	if (setsockopt(fd[0], SOL_SOCKET, SO_RCVLOWAT, &rlowat,
	    sizeof(rlowat)) != 0) e(0);

	if (ioctl(fd[0], FIONREAD, &res) != 0) e(0);
	if (res != rcvlen + fill_delta) e(0);

	res = recv(fd[0], buf, rcvlen + 1, MSG_DONTWAIT);
	if (exp_delta < 0) {
		if (res != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);
	} else
		if (res != rcvlen - exp_delta) e(0);

	free(buf);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);
}

/*
 * Test that SO_SNDLOWAT is limited to the size of the send buffer.
 */
static void
sub91r_send(int fill, int slowat_delta, int exp_delta)
{
	char *buf;
	size_t buflen;
	int fd[2], sndlen, slowat, res;

	if (get_tcp_pair(AF_INET6, SOCK_STREAM, 0, fd) != 0) e(0);

	/*
	 * Fill up the socket's receive queue, and possibly put one extra byte
	 * in the other socket's send queue.
	 */
	(void)get_buf_sizes(SOCK_STREAM, &sndlen, NULL);

	buflen = MAX(CHUNK, sndlen + 1);
	if ((buf = malloc(buflen)) == NULL) e(0);

	memset(buf, 0, buflen);

	fill_tcp_bufs(fd[0], fd[1], 0 /*fill_send*/, 0 /*delta*/);

	slowat = sndlen + slowat_delta;

	if (fill > 0) {
		memset(buf, 0, fill);

		if (send(fd[0], buf, fill, 0) != fill) e(0);
	}

	if (setsockopt(fd[0], SOL_SOCKET, SO_SNDLOWAT, &slowat,
	    sizeof(slowat)) != 0) e(0);

	res = send(fd[0], buf, sndlen + 1, MSG_DONTWAIT);
	if (exp_delta < 0) {
		if (res != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);
	} else
		if (res != sndlen - exp_delta) e(0);

	free(buf);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);
}

/*
 * Test that on stream sockets, SO_RCVLOWAT and SO_SNDLOWAT are limited to
 * their respective buffer sizes.  This test is derived from test90w, but
 * merging the two into socklib would get too messy unfortunately.
 */
static void
test91r(void)
{

	subtest = 18;

	/*
	 * With the receive buffer filled except for one byte, all data should
	 * be retrieved unless the threshold is not met.
	 */
	sub91r_recv(-1, -1, 1);
	sub91r_recv(-1, 0, -1);
	sub91r_recv(-1, 1, -1);

	/*
	 * With the receive buffer filled completely, all data should be
	 * retrieved in all cases.
	 */
	sub91r_recv(0, -1, 0);
	sub91r_recv(0, 0, 0);
	sub91r_recv(0, 1, 0);

	/*
	 * With a send buffer that contains one byte, all data should be sent
	 * unless the threshold is not met.
	 */
	sub91r_send(1, -1, 1);
	sub91r_send(1, 0, -1);
	sub91r_send(1, 1, -1);

	/*
	 * With the send buffer filled completely, all data should be sent
	 * in all cases.
	 */
	sub91r_send(0, -1, 0);
	sub91r_send(0, 0, 0);
	sub91r_send(0, 1, 0);
}

/*
 * Test sending and receiving with bad pointers on a TCP socket.
 */
static void
sub91s_tcp(char * ptr)
{
	int fd[2];

	memset(ptr, 'X', PAGE_SIZE);

	if (get_tcp_pair(AF_INET, SOCK_STREAM, 0, fd) != 0) e(0);

	if (send(fd[0], "A", 1, 0) != 1) e(0);

	if (send(fd[0], ptr, PAGE_SIZE * 2, MSG_DONTWAIT) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (send(fd[0], "B", 1, 0) != 1) e(0);

	if (shutdown(fd[0], SHUT_WR) != 0) e(0);

	if (recv(fd[1], &ptr[PAGE_SIZE - 1], PAGE_SIZE, MSG_WAITALL) != -1)
		e(0);
	if (errno != EFAULT) e(0);

	if (recv(fd[1], ptr, 3, MSG_DONTWAIT) != 2) e(0);
	if (ptr[0] != 'A') e(0);
	if (ptr[1] != 'B') e(0);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);
}

/*
 * Test sending and receiving with bad pointers on a UDP socket.
 */
static void
sub91s_udp(char * ptr)
{
	struct sockaddr_in6 sin6;
	int i, fd;

	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	memset(ptr, 'A', PAGE_SIZE);

	if (sendto(fd, &ptr[PAGE_SIZE / 2], PAGE_SIZE, 0,
	    (struct sockaddr *)&sin6, sizeof(sin6)) != -1) e(0);
	if (errno != EFAULT) e(0);

	memset(ptr, 'B', PAGE_SIZE);

	if (sendto(fd, ptr, PAGE_SIZE, 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != PAGE_SIZE) e(0);

	memset(ptr, 0, PAGE_SIZE);

	if (recvfrom(fd, &ptr[PAGE_SIZE / 2], PAGE_SIZE, 0, NULL, 0) != -1)
		e(0);
	if (errno != EFAULT) e(0);

	if (recvfrom(fd, ptr, PAGE_SIZE * 2, 0, NULL, 0) != PAGE_SIZE) e(0);
	for (i = 0; i < PAGE_SIZE; i++)
		if (ptr[i] != 'B') e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test sending and receiving with bad pointers.
 */
static void
test91s(void)
{
	char *ptr;

	subtest = 19;

	if ((ptr = mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED) e(0);

	if (munmap(&ptr[PAGE_SIZE], PAGE_SIZE) != 0) e(0);

	sub91s_tcp(ptr);
	sub91s_udp(ptr);

	if (munmap(ptr, PAGE_SIZE) != 0) e(0);
}

/*
 * Test closing TCP sockets and SO_LINGER.
 */
static void
test91t(void)
{
	char buf[CHUNK];
	size_t total, left;
	ssize_t res;
	int i, fd[2];

	subtest = 20;

	total = get_buf_sizes(SOCK_STREAM, NULL, NULL);

	memset(buf, 0, sizeof(buf));

	/*
	 * Test two cases of handling connection closure:
	 *
	 * 1) the FIN+ACK case, where the closing side finishes the close
	 *    operation once its FIN has been acknowledged;
	 * 2) the FIN+FIN case, where the closing side finishes the close
	 *    operation once it has sent its own FIN (possibly without getting
	 *    an ACK yet) and also receives a FIN from the other side.
	 *
	 * Since lwIP prevents us from detecting #1 without polling, which
	 * happens twice a second, we can test #2 by shutting down the peer
	 * connection immediately after (i=0/2) or even before (i=4/5) closing
	 * this side.
	 */
	for (i = 0; i <= 5; i++) {
		if (get_tcp_pair(AF_INET, SOCK_STREAM, 0, fd) != 0) e(0);

		fill_tcp_bufs(fd[0], fd[1], 1 /*fill_send*/, 0 /*delta*/);

		if (close(fd[0]) != 0) e(0);

		if (i >= 4 && shutdown(fd[1], SHUT_WR) != 0) e(0);

		for (left = total; left > 0; left -= res) {
			res = recv(fd[1], buf, sizeof(buf), 0);
			if (res <= 0) e(0);
			if (res > left) e(0);
		}

		if (recv(fd[1], buf, sizeof(buf), 0) != 0) e(0);

		sleep(i & 1);

		/*
		 * We can still send to the receiving end, but this will cause
		 * a reset.  We do this only if we have not just shut down the
		 * writing end of this socket.  Also test regular closing.
		 */
		if (i / 2 == 1) {
			if (send(fd[1], "B", 1, 0) != 1) e(0);

			if (recv(fd[1], buf, sizeof(buf), 0) != -1) e(0);
			if (errno != ECONNRESET) e(0);
		}

		if (close(fd[1]) != 0) e(0);
	}

	/*
	 * Test that closing a socket with data still in its receive queue
	 * causes a RST to be issued.
	 */
	if (get_tcp_pair(AF_INET6, SOCK_STREAM, 0, fd) != 0) e(0);

	if (send(fd[0], "C", 1, 0) != 1) e(0);

	if (recv(fd[1], buf, sizeof(buf), MSG_PEEK) != 1) e(0);

	if (close(fd[1]) != 0) e(0);

	if (recv(fd[0], buf, sizeof(buf), 0) != -1) e(0);
	if (errno != ECONNRESET) e(0);

	if (close(fd[0]) != 0) e(0);
}

/*
 * Test closing a socket with a particular SO_LINGER setting.
 */
static void
sub91u(int nb, int mode, int intr, int onoff, int linger)
{
	char buf[CHUNK];
	struct timeval tv1, tv2;
	struct linger l;
	pid_t pid;
	int fd[2], pfd[2], fl, val, res, status;

	get_tcp_pair((mode & 1) ? AF_INET6 : AF_INET, SOCK_STREAM, 0, fd);

	/*
	 * Set up the socket pair.
	 */
	fill_tcp_bufs(fd[0], fd[1], 0 /*fill_send*/, 1 /*delta*/);

	if (mode == 3 && shutdown(fd[1], SHUT_WR) != 0) e(0);

	l.l_onoff = onoff;
	l.l_linger = (linger) ? (2 + intr) : 0;
	if (setsockopt(fd[0], SOL_SOCKET, SO_LINGER, &l, sizeof(l)) != 0) e(0);

	if (nb) {
		if ((fl = fcntl(fd[0], F_GETFL)) == -1) e(0);
		if (fcntl(fd[0], F_SETFL, fl | O_NONBLOCK) != 0) e(0);
	}

	/* We need two-way parent-child communication for this test. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pfd) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(pfd[1]) != 0) e(0);

		if (close(fd[1]) != 0) e(0);

		signal(SIGUSR1, test91_got_signal);

		/*
		 * Do not start closing the file descriptor until after the
		 * parent has closed its copy.
		 */
		if (read(pfd[0], &val, sizeof(val)) != sizeof(val)) e(0);
		if (val != 0) e(0);

		if (gettimeofday(&tv1, NULL) != 0) e(0);

		/* Perform the possibly blocking close(2) call. */
		if (intr) {
			if (close(fd[0]) != -1) e(0);
			if (errno != EINPROGRESS) e(0);
		} else
			if (close(fd[0]) != 0) e(0);

		if (gettimeofday(&tv2, NULL) != 0) e(0);

		timersub(&tv2, &tv1, &tv1);

		/* Polling may take 500ms. */
		val = tv1.tv_sec + ((tv1.tv_usec > 750000) ? 1 : 0);

		if (val < 0 || val > 2) e(0);

		/* Tell the parent how long the close(2) took, in seconds. */
		if (write(pfd[0], &val, sizeof(val)) != sizeof(val)) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	/* Close file descriptors here and then let the child run. */
	if (close(pfd[0]) != 0) e(0);

	if (close(fd[0]) != 0) e(0);

	val = 0;
	if (write(pfd[1], &val, sizeof(val)) != sizeof(val)) e(0);

	/*
	 * Wait one second until we try to close the connection ourselves, if
	 * applicable.  If we are killing the child, we add yet another second
	 * to tell the difference between a clean close and a timeout/reset.
	 */
	sleep(1);

	if (intr) {
		if (kill(pid, SIGUSR1) != 0) e(0);

		sleep(1);
	}

	/*
	 * Trigger various ways in which the connection is closed, or not, in
	 * which case the linger timeout should cause a reset.
	 */
	switch (mode) {
	case 0:	/* do nothing; expect reset */
		break;

	case 1:	/* FIN + rFIN */
		if (shutdown(fd[1], SHUT_WR) != 0) e(0);

		/*
		 * The FIN cannot yet be sent due to the zero-sized receive
		 * window.  Make some room so that it can be sent.
		 */
		/* FALLTHROUGH */
	case 2:	/* FIN + ACK */
	case 3:	/* rFIN + FIN */
		if (recv(fd[1], buf, sizeof(buf), 0) <= 0) e(0);
		break;

	case 4:		/* RST */
		l.l_onoff = 1;
		l.l_linger = 0;
		if (setsockopt(fd[1], SOL_SOCKET, SO_LINGER, &l,
		    sizeof(l)) != 0) e(0);

		if (close(fd[1]) != 0) e(0);
		fd[1] = -1;
		break;

	default:
		e(0);
	}

	/*
	 * Make absolutely sure that the linger timer has triggered and we do
	 * not end up exploiting race conditions in the tests below.  As a
	 * result this subtest takes over a minute but at least it has already
	 * triggered a whole bunch of bugs (and produced lwIP patch #9125).
	 */
	sleep(2);

	/* Get the number of seconds spent in the close(2) call. */
	if (read(pfd[1], &val, sizeof(val)) != sizeof(val)) e(0);

	/*
	 * See if the close(2) call took as long as expected and check that the
	 * other side of the connection sees either EOF or a reset as expected.
	 */
	if (mode == 0) {
		if (nb) {
			if (val != 0) e(0);

			sleep(2);
		} else if (!intr) {
			if (val != linger * 2) e(0);
		} else
			if (val != 1) e(0);

		/* See if the connection was indeed reset. */
		while ((res = recv(fd[1], buf, sizeof(buf), 0)) > 0)
			;
		if (res != -1) e(0);
		if (errno != ECONNRESET) e(0);
	} else {
		if (val != ((onoff && !nb) || intr)) e(0);

		/* Check for EOF unless we already closed the socket. */
		if (fd[1] != -1) {
			while ((res = recv(fd[1], buf, sizeof(buf), 0)) > 0)
				;
			if (res != 0) e(0);
		}
	}

	/* Clean up. */
	if (fd[1] != -1 && close(fd[1]) != 0) e(0);

	if (close(pfd[1]) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);
}

/*
 * Test SO_LINGER support in various configurations.  It is worth noting that I
 * implemented a somewhat broken version of SO_LINGER because lwIP does not
 * allow for proper detection of our FIN being acknowledged in all cases (this
 * is documented in the service).  As a result, a close(2) call may return
 * earlier than it is supposed to, namely as soon as 1) we sent a FIN, and
 * 2) we received a FIN from the other side.  We also test the somewhat broken
 * behavior here, as above all else the aim is to make sure that the service
 * code works as expected.
 */
static void
test91u(void)
{
	int nb, mode;

	subtest = 21;

	/*
	 *
	 * In all of the following scenarios, close(2) should only ever return
	 * success, so that the caller knows that the file descriptor has been
	 * closed.
	 */
	for (nb = 0; nb <= 1; nb++) {
		/*
		 * SO_LINGER off: the close(2) call should return immediately,
		 * and the connection should be closed in the background.
		 */
		for (mode = 1; mode <= 4; mode++)
			sub91u(nb, mode, 0, 0, 0);

		/*
		 * SO_LINGER on with a zero timeout: the close(2) call should
		 * return immediately, and the connection should be reset.
		 */
		sub91u(nb, 0, 0, 1, 0);

		/*
		 * SO_LINGER on with a non-zero timeout: the close(2) call
		 * should return immediately for non-blocking sockets only, and
		 * otherwise as soon as either the connection is closed or the
		 * timeout triggers, in which case the connection is reset.
		 */
		for (mode = 0; mode <= 4; mode++)
			sub91u(nb, mode, 0, 1, 1);
	}

	/*
	 * Test signal-interrupting blocked close(2) calls with SO_LINGER.  In
	 * such cases, the close(2) should return EINPROGRESS to indicate that
	 * the file descriptor has been closed, and the original close action
	 * (with the original timeout) should proceed in the background.
	 */
	for (mode = 0; mode <= 4; mode++)
		sub91u(0, mode, 1, 1, 1);
}

/*
 * Test shutdown on listening TCP sockets.
 */
static void
sub91v(int how)
{
	struct sockaddr_in sin;
	socklen_t len;
	char c;
	int fd, fd2, fd3, fl;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	len = sizeof(sin);
	if (getsockname(fd, (struct sockaddr *)&sin, &len) != 0) e(0);
	if (len != sizeof(sin)) e(0);

	if (listen(fd, 1) != 0) e(0);

	if ((fd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	if (shutdown(fd, how) != 0) e(0);

	len = sizeof(sin);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin, &len)) < 0) e(0);
	if (len != sizeof(sin)) e(0);

	if (write(fd2, "A", 1) != 1) e(0);
	if (read(fd3, &c, 1) != 1) e(0);
	if (c != 'A') e(0);

	if (write(fd3, "B", 1) != 1) e(0);
	if (read(fd2, &c, 1) != 1) e(0);
	if (c != 'B') e(0);

	len = sizeof(sin);
	if (accept(fd, (struct sockaddr *)&sin, &len) != -1) e(0);
	if (errno != ECONNABORTED) e(0);

	if ((fl = fcntl(fd, F_GETFL)) == -1) e(0);
	if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) e(0);

	len = sizeof(sin);
	if (accept(fd, (struct sockaddr *)&sin, &len) != -1) e(0);
	if (errno != ECONNABORTED) e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);
}

/*
 * Test shutdown on listening TCP sockets.  This test is derived from test90x.
 */
static void
test91v(void)
{
	const int hows[] = { SHUT_RD, SHUT_WR, SHUT_RDWR };
	int i;

	subtest = 22;

	for (i = 0; i < __arraycount(hows); i++)
		sub91v(hows[i]);
}

/*
 * Test basic sysctl(2) socket enumeration support.
 */
static void
test91w(void)
{
	struct kinfo_pcb ki;
	struct sockaddr_in lsin, rsin;
	struct sockaddr_in6 lsin6, rsin6;
	char buf[CHUNK];
	uint16_t local_port, remote_port;
	socklen_t len;
	int fd[2], val, sndbuf, rcvbuf;

	subtest = 23;

	/*
	 * First test TCP.
	 */
	get_tcp_pair(AF_INET, SOCK_STREAM, 0, fd);

	val = 0;
	if (setsockopt(fd[1], IPPROTO_TCP, TCP_NODELAY, &val,
	    sizeof(val)) != 0) e(0);

	len = sizeof(lsin);
	if (getsockname(fd[0], (struct sockaddr *)&lsin, &len) != 0) e(0);
	if (len != sizeof(lsin)) e(0);
	local_port = ntohs(lsin.sin_port);

	if (getpeername(fd[0], (struct sockaddr *)&rsin, &len) != 0) e(0);
	if (len != sizeof(rsin)) e(0);
	remote_port = ntohs(rsin.sin_port);

	if (send(fd[0], "ABCDE", 5, 0) != 5) e(0);

	/* Allow the data to reach the other side and be acknowledged. */
	sleep(1);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_ESTABLISHED) e(0);
	if (!(ki.ki_tflags & TF_NODELAY)) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, remote_port,
	    local_port, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_ESTABLISHED) e(0);
	if (ki.ki_tflags & TF_NODELAY) e(0);
	if (memcmp(&ki.ki_src, &rsin, sizeof(rsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &lsin, sizeof(lsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 5) e(0);

	if (recv(fd[1], buf, sizeof(buf), 0) != 5) e(0);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_TIME_WAIT) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	/* Test IPv6 sockets as well. */
	get_tcp_pair(AF_INET6, SOCK_STREAM, 0, fd);

	len = sizeof(lsin6);
	if (getsockname(fd[0], (struct sockaddr *)&lsin6, &len) != 0) e(0);
	if (len != sizeof(lsin6)) e(0);
	local_port = ntohs(lsin6.sin6_port);

	if (getpeername(fd[0], (struct sockaddr *)&rsin6, &len) != 0) e(0);
	if (len != sizeof(rsin6)) e(0);
	remote_port = ntohs(rsin6.sin6_port);

	memset(buf, 0, sizeof(buf));

	/* We fill up the queues so we do not need to sleep in this case. */
	(void)get_buf_sizes(SOCK_STREAM, &sndbuf, &rcvbuf);

	fill_tcp_bufs(fd[0], fd[1], 1 /*fill_send*/, 0 /*delta*/);

	if (send(fd[0], buf, 1, MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_ESTABLISHED) e(0);
	if (!(ki.ki_tflags & TF_NODELAY)) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (ki.ki_sndq != (size_t)sndbuf) e(0);
	if (ki.ki_rcvq != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP,
	    remote_port, local_port, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_ESTABLISHED) e(0);
	if (!(ki.ki_tflags & TF_NODELAY)) e(0);
	if (memcmp(&ki.ki_src, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != (size_t)rcvbuf) e(0);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);

	/* Bound and listening sockets should show up as well. */
	if ((fd[0] = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, 0, 0,
	    &ki) != 0) e(0);

	memset(&lsin, 0, sizeof(lsin));
	lsin.sin_len = sizeof(lsin);
	lsin.sin_family = AF_INET;
	lsin.sin_port = htons(TEST_PORT_A);
	lsin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(fd[0], (struct sockaddr *)&lsin, sizeof(lsin)) != 0) e(0);

	memset(&rsin, 0, sizeof(rsin));
	rsin.sin_len = sizeof(rsin);
	rsin.sin_family = AF_INET;

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, TEST_PORT_A,
	    0, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_CLOSED) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	if (listen(fd[0], 1)) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, TEST_PORT_A,
	    0, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_LISTEN) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	if (close(fd[0]) != 0) e(0);

	/* Test IPv6 sockets as well. */
	if ((fd[0] = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, 0, 0,
	    &ki) != 0) e(0);

	val = 1;
	if (setsockopt(fd[0], IPPROTO_IPV6, IPV6_V6ONLY, &val,
	    sizeof(val)) != 0) e(0);

	memset(&lsin6, 0, sizeof(lsin6));
	lsin6.sin6_len = sizeof(lsin6);
	lsin6.sin6_family = AF_INET6;
	lsin6.sin6_port = htons(TEST_PORT_A);
	memcpy(&lsin6.sin6_addr, &in6addr_loopback, sizeof(lsin6.sin6_addr));
	if (bind(fd[0], (struct sockaddr *)&lsin6, sizeof(lsin6)) != 0) e(0);

	memset(&rsin6, 0, sizeof(rsin6));
	rsin6.sin6_len = sizeof(rsin6);
	rsin6.sin6_family = AF_INET6;

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP,
	    TEST_PORT_A, 0, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_CLOSED) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (!(ki.ki_pflags & IN6P_IPV6_V6ONLY)) e(0);

	if (listen(fd[0], 1)) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP,
	    TEST_PORT_A, 0, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_LISTEN) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (!(ki.ki_pflags & IN6P_IPV6_V6ONLY)) e(0);

	if (close(fd[0]) != 0) e(0);

	/*
	 * I do not dare binding to ANY so we cannot test IPV6_V6ONLY properly
	 * here.  Instead we repeat the test and ensure the IN6P_IPV6_V6ONLY
	 * flag accurately represents the current state.
	 */
	if ((fd[0] = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, 0, 0,
	    &ki) != 0) e(0);

	val = 0;
	if (setsockopt(fd[0], IPPROTO_IPV6, IPV6_V6ONLY, &val,
	    sizeof(val)) != 0) e(0);

	if (bind(fd[0], (struct sockaddr *)&lsin6, sizeof(lsin6)) != 0) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP,
	    TEST_PORT_A, 0, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_CLOSED) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (!(ki.ki_pflags & IN6P_IPV6_V6ONLY)) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, TEST_PORT_A,
	    0, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_TCP, TEST_PORT_A,
	    0, &ki) != 0) e(0);

	if (close(fd[0]) != 0) e(0);

	/*
	 * Then test UDP.
	 */
	if ((fd[0] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_UDP, 0, 0,
	    &ki) != 0) e(0);

	memset(&lsin, 0, sizeof(lsin));
	lsin.sin_len = sizeof(lsin);
	lsin.sin_family = AF_INET;
	lsin.sin_port = htons(TEST_PORT_A);
	lsin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	memset(&rsin, 0, sizeof(rsin));
	rsin.sin_len = sizeof(rsin);
	rsin.sin_family = AF_INET;

	if (bind(fd[0], (struct sockaddr *)&lsin, sizeof(lsin)) != 0) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_UDP, TEST_PORT_A,
	    0, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_DGRAM) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	rsin.sin_port = htons(TEST_PORT_B);
	rsin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(fd[0], (struct sockaddr *)&rsin, sizeof(rsin)) != 0) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_UDP, TEST_PORT_A,
	    TEST_PORT_B, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_DGRAM) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_UDP, TEST_PORT_B,
	    TEST_PORT_A, &ki) != 0) e(0);

	if ((fd[1] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	if (bind(fd[1], (struct sockaddr *)&rsin, sizeof(rsin)) != 0) e(0);

	if (sendto(fd[1], "ABC", 3, 0, (struct sockaddr *)&lsin,
	    sizeof(lsin)) != 3) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_UDP, TEST_PORT_A,
	    TEST_PORT_B, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_DGRAM) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq < 3) e(0);	/* size is rounded up */

	if (socklib_find_pcb("net.inet6.udp6.pcblist", IPPROTO_UDP,
	    TEST_PORT_A, TEST_PORT_B, &ki) != 0) e(0);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);

	/* Test IPv6 sockets as well. */
	if ((fd[0] = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	if (socklib_find_pcb("net.inet6.udp6.pcblist", IPPROTO_UDP, 0, 0,
	    &ki) != 0) e(0);

	memset(&lsin6, 0, sizeof(lsin6));
	lsin6.sin6_len = sizeof(lsin6);
	lsin6.sin6_family = AF_INET6;
	lsin6.sin6_port = htons(TEST_PORT_A);
	memcpy(&lsin6.sin6_addr, &in6addr_loopback, sizeof(lsin6.sin6_addr));
	if (bind(fd[0], (struct sockaddr *)&lsin6, sizeof(lsin6)) != 0) e(0);

	memset(&rsin6, 0, sizeof(rsin6));
	rsin6.sin6_len = sizeof(rsin6);
	rsin6.sin6_family = AF_INET6;

	if (socklib_find_pcb("net.inet6.udp6.pcblist", IPPROTO_UDP,
	    TEST_PORT_A, 0, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_DGRAM) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);
	if (!(ki.ki_pflags & IN6P_IPV6_V6ONLY)) e(0);

	rsin6.sin6_port = htons(TEST_PORT_B);
	memcpy(&rsin6.sin6_addr, &in6addr_loopback, sizeof(rsin6.sin6_addr));
	if (connect(fd[0], (struct sockaddr *)&rsin6, sizeof(rsin6)) != 0)
		e(0);

	if (socklib_find_pcb("net.inet6.udp6.pcblist", IPPROTO_UDP,
	    TEST_PORT_A, TEST_PORT_B, &ki) != 1) e(0);
	if (ki.ki_type != SOCK_DGRAM) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);
	if (!(ki.ki_pflags & IN6P_IPV6_V6ONLY)) e(0);

	if (close(fd[0]) != 0) e(0);

	if (socklib_find_pcb("net.inet6.udp6.pcblist", IPPROTO_UDP,
	    TEST_PORT_A, TEST_PORT_B, &ki) != 0) e(0);
}

/*
 * Test socket enumeration of sockets using IPv4-mapped IPv6 addresses.
 */
static void
test91x(void)
{
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	socklen_t len;
	struct kinfo_pcb ki;
	unsigned short local_port, remote_port;
	int fd, fd2, fd3, val;

	subtest = 24;

	/*
	 * Test that information from an IPv6 socket bound to an IPv4-mapped
	 * IPv6 address is as expected.  For socket enumeration, due to lwIP
	 * limitations we return an IPv4 address instead of an IPv4-mapped IPv6
	 * address, and that is what this test checks for various sockets.
	 */
	if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, "::ffff:"LOOPBACK_IPV4, &sin6.sin6_addr) != 1)
		e(0);

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(fd, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	local_port = ntohs(sin6.sin6_port);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, local_port,
	    0, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, local_port,
	    0, &ki) != 1) e(0);

	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_CLOSED) e(0);

	memcpy(&sin, &ki.ki_src, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(local_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	memcpy(&sin, &ki.ki_dst, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(0)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_ANY)) e(0);

	if (listen(fd, 1) != 0) e(0);

	/*
	 * Test that information from an accepted (IPv6) socket is correct
	 * for a connection from an IPv4 address.
	 */
	if ((fd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(local_port);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(fd2, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	len = sizeof(sin);
	if (getsockname(fd2, (struct sockaddr *)&sin, &len) != 0) e(0);
	if (len != sizeof(sin)) e(0);
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	remote_port = ntohs(sin.sin_port);

	len = sizeof(sin6);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6, &len)) < 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(remote_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	len = sizeof(sin6);
	if (getsockname(fd3, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(local_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	len = sizeof(sin6);
	if (getpeername(fd3, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(remote_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 1) e(0);

	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_ESTABLISHED) e(0);

	memcpy(&sin, &ki.ki_src, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(local_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	memcpy(&sin, &ki.ki_dst, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(remote_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd2) != 0) e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 1) e(0);

	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_TIME_WAIT) e(0);

	memcpy(&sin, &ki.ki_src, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(local_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	memcpy(&sin, &ki.ki_dst, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(remote_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	/*
	 * Test that information from a connected (IPv6) socket is correct
	 * after connecting it to an IPv4 address.
	 */
	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	val = 0;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(local_port);
	if (inet_pton(AF_INET6, "::ffff:"LOOPBACK_IPV4, &sin6.sin6_addr) != 1)
		e(0);

	if (connect(fd2, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(fd2, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);
	remote_port = ntohs(sin6.sin6_port);

	len = sizeof(sin6);
	if (getpeername(fd2, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(local_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP,
	    remote_port, local_port, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, remote_port,
	    local_port, &ki) != 1) e(0);

	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_ESTABLISHED) e(0);

	memcpy(&sin, &ki.ki_src, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(remote_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	memcpy(&sin, &ki.ki_dst, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(local_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	len = sizeof(sin6);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6, &len)) < 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(remote_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);
	if (close(fd) != 0) e(0);

	/*
	 * Do one more test on an accepted socket, now without binding the
	 * listening socket to an IPv4-mapped IPv6 address.
	 */
	if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_any, sizeof(sin6.sin6_addr));

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(fd, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	local_port = ntohs(sin6.sin6_port);

	if (listen(fd, 1) != 0) e(0);

	if ((fd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(local_port);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(fd2, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	len = sizeof(sin);
	if (getsockname(fd2, (struct sockaddr *)&sin, &len) != 0) e(0);
	if (len != sizeof(sin)) e(0);
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	remote_port = ntohs(sin.sin_port);

	len = sizeof(sin6);
	if ((fd3 = accept(fd, (struct sockaddr *)&sin6, &len)) < 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(remote_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	len = sizeof(sin6);
	if (getsockname(fd3, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(local_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	len = sizeof(sin6);
	if (getpeername(fd3, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port != htons(remote_port)) e(0);
	if (!IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) e(0);
	if (sin6.sin6_addr.__u6_addr.__u6_addr32[3] != htonl(INADDR_LOOPBACK))
		e(0);

	if (socklib_find_pcb("net.inet6.tcp6.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.tcp.pcblist", IPPROTO_TCP, local_port,
	    remote_port, &ki) != 1) e(0);

	if (ki.ki_type != SOCK_STREAM) e(0);
	if (ki.ki_tstate != TCPS_ESTABLISHED) e(0);

	memcpy(&sin, &ki.ki_src, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(local_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	memcpy(&sin, &ki.ki_dst, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(remote_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/*
	 * Do some very simple UDP socket enumeration tests.  The rest is
	 * already tested elsewhere.
	 */
	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, "::ffff:"LOOPBACK_IPV4, &sin6.sin6_addr) != 1)
		e(0);

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(fd, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	local_port = ntohs(sin6.sin6_port);

	if (socklib_find_pcb("net.inet6.udp6.pcblist", IPPROTO_UDP, local_port,
	    0, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_UDP, local_port,
	    0, &ki) != 1) e(0);

	if (ki.ki_type != SOCK_DGRAM) e(0);
	if (ki.ki_tstate != 0) e(0);

	memcpy(&sin, &ki.ki_src, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(local_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	memcpy(&sin, &ki.ki_dst, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(0)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_ANY)) e(0);

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_A);
	if (inet_pton(AF_INET6, "::ffff:"LOOPBACK_IPV4, &sin6.sin6_addr) != 1)
		e(0);

	if (connect(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(fd, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	local_port = ntohs(sin6.sin6_port);

	if (socklib_find_pcb("net.inet6.udp6.pcblist", IPPROTO_UDP, local_port,
	    TEST_PORT_A, &ki) != 0) e(0);

	if (socklib_find_pcb("net.inet.udp.pcblist", IPPROTO_UDP, local_port,
	    TEST_PORT_A, &ki) != 1) e(0);

	if (ki.ki_type != SOCK_DGRAM) e(0);
	if (ki.ki_tstate != 0) e(0);

	memcpy(&sin, &ki.ki_src, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(local_port)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	memcpy(&sin, &ki.ki_dst, sizeof(sin));
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port != htons(TEST_PORT_A)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test local and remote IPv6 address handling.  In particular, test scope IDs
 * and IPv4-mapped IPv6 addresses.
 */
static void
test91y(void)
{

	subtest = 25;

	socklib_test_addrs(SOCK_STREAM, 0);

	socklib_test_addrs(SOCK_DGRAM, 0);
}

/*
 * Test low-memory conditions for TCP.
 */
static void
test91z(void)
{
	struct sockaddr_in6 sin6;
	socklen_t len;
	unsigned char buf[CHUNK];
	struct timeval tv;
	unsigned int i, j, k;
	ssize_t res, left;
	pid_t pid, pid2;
	static int fds[OPEN_MAX];
	static size_t pos[OPEN_MAX];
	int lfd, pfd[2], val, sndlen, rcvlen, status;

	subtest = 26;

	/*
	 * We use custom send and receive buffer sizes, such that we can
	 * trigger the case that we run out of send buffers without causing
	 * buffers used on the receiving side to empty the buffer pool first.
	 * While the latter case is not unrealistic for practical scenarios, it
	 * is not what we want to test here.  It would also cause practical
	 * problems for this test, as the result may be that the loopback
	 * interface (that we use here) starts dropping packets due to being
	 * unable to make copies.
	 *
	 * The aim with these two is that the ratio is such that we run into
	 * the 75% usage limit for the send side without using the other 25%
	 * for receiving purposes.  Since our TCP buffer merging guarantees at
	 * most a 50% overhead on the receiving side, the minimum ratio of 5:1
	 * translates to a worst-case ratio is 10:3 which is just above 75%.
	 * Thus, we should be able to use 80K:16K.  Instead, we use 128K:16K,
	 * because otherwise we will run out of sockets before we run out of
	 * buffers.  After all, we are not generating any traffic on the socket
	 * pairs in the other direction--something for which we do provision.
	 */
	sndlen = 131072;
	rcvlen = 16384;

	/*
	 * Unfortunately, filling up receive queues is not easy, and for any
	 * size other than the window size (which is by nature also the minimum
	 * receive queue length that may be set) we would need to work around
	 * the same issue described in fill_tcp_bufs(), which would massively
	 * complicate the implementation of this subtest.  For now, make sure
	 * that inconsistent internal changes will trigger this assert.
	 */
	assert(rcvlen == WINDOW_SIZE);

	if ((lfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	if (bind(lfd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(lfd, (struct sockaddr *)&sin6, &len) != 0) e(0);

	if (listen(lfd, 1) != 0) e(0);

	/*
	 * Start a child process for the receiving ends.  We have to use
	 * another process because we aim to open a total concurrent number of
	 * TCP sockets that exceeds OPEN_MAX.
	 */
	if (pipe(pfd) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(lfd) != 0) e(0);
		if (close(pfd[1]) != 0) e(0);

		/* Create socket pairs. */
		for (i = 0; ; i++) {
			if (i == __arraycount(fds)) e(0);

			if ((fds[i] = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
			    e(0);

			if (connect(fds[i], (struct sockaddr *)&sin6,
			    sizeof(sin6)) != 0) e(0);

			val = 1;
			if (setsockopt(fds[i], IPPROTO_TCP, TCP_NODELAY, &val,
			    sizeof(val)) != 0) e(0);

			if (setsockopt(fds[i], SOL_SOCKET, SO_RCVBUF, &rcvlen,
			    sizeof(rcvlen)) != 0) e(0);

			/* Synchronization point A. */
			if (read(pfd[0], &k, sizeof(k)) != sizeof(k)) e(0);
			if (k == 0)
				break;
		}

		/* Synchronization point B. */
		if (read(pfd[0], &k, sizeof(k)) != sizeof(k)) e(0);
		if (k != 2) e(0);

		/* Receive some data from one socket. */
		pos[0] = 0;
		for (left = sizeof(buf) * 2; left > 0; left -= res) {
			res = recv(fds[0], buf, MIN(left, sizeof(buf)), 0);
			if (res <= 0) e(0);
			if (res > left) e(0);

			for (j = 0; j < res; j++)
				if (buf[j] != (unsigned char)(pos[0]++)) e(0);
		}

		/* Synchronization point C. */
		if (read(pfd[0], &k, sizeof(k)) != sizeof(k)) e(0);
		if (k != 3) e(0);

		/*
		 * Receive all remaining data from all sockets.  Do this in two
		 * steps.  First enlarge the receive buffer and empty it, so
		 * that upon resumption, all remaining data is transferred from
		 * the sender to the receiver in one go.  Then actually wait
		 * for any remaining data, and the EOF.  If we do both in one
		 * step, this part of the test will take several minutes to
		 * complete.  Note that the last socket needs special treatment
		 * because its send queue may not have been filled entirely.
		 */
		for (k = 0; k <= i; k++) {
			if (setsockopt(fds[i], SOL_SOCKET, SO_RCVBUF, &rcvlen,
			    sizeof(rcvlen)) != 0) e(0);

			pos[k] = (k == 0) ? (sizeof(buf) * 2) : 0;

			for (left = sndlen + rcvlen - pos[k]; left > 0;
			    left -= res) {
				res = recv(fds[k], buf, MIN(left, sizeof(buf)),
				    MSG_DONTWAIT);
				if (res == -1 && errno == EWOULDBLOCK)
					break;
				if (res == 0 && k == i) {
					pos[i] = sndlen + rcvlen;
					break;
				}
				if (res <= 0) e(0);
				if (res > left) e(0);

				for (j = 0; j < res; j++)
					if (buf[j] != (unsigned char)(k +
					    pos[k]++)) e(0);
			}
		}

		for (k = 0; k <= i; k++) {
			for (left = sndlen + rcvlen - pos[k]; left > 0;
			   left -= res) {
				res = recv(fds[k], buf, MIN(left, sizeof(buf)),
				    0);
				if (res == 0 && k == i)
					break;
				if (res <= 0) e(0);
				if (res > left) e(0);

				for (j = 0; j < res; j++)
					if (buf[j] != (unsigned char)(k +
					    pos[k]++)) e(0);
			}

			if (recv(fds[k], buf, 1, 0) != 0) e(0);
		}

		/* Clean up. */
		do {
			if (close(fds[i]) != 0) e(0);
		} while (i-- > 0);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(pfd[0]) != 0) e(0);

	for (i = 0; ; i++) {
		if (i == __arraycount(fds)) e(0);

		len = sizeof(sin6);
		if ((fds[i] = accept(lfd, (struct sockaddr *)&sin6, &len)) < 0)
			e(0);

		val = 1;
		if (setsockopt(fds[i], IPPROTO_TCP, TCP_NODELAY, &val,
		    sizeof(val)) != 0) e(0);

		if (setsockopt(fds[i], SOL_SOCKET, SO_SNDBUF, &sndlen,
		    sizeof(sndlen)) != 0) e(0);

		/*
		 * Try to pump as much data into one end of the socket.  This
		 * may fail at any time due to being out of buffers, so we use
		 * a send timeout to break the resulting blocking call.
		 */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if (setsockopt(fds[i], SOL_SOCKET, SO_SNDTIMEO, &tv,
		    sizeof(tv)) != 0) e(0);

		/*
		 * Since buffer corruption is most likely to be detected when
		 * lots of buffers are actually in use, also make sure that we
		 * (eventually) receive what we send.
		 */
		res = sizeof(buf);
		pos[i] = 0;
		for (left = sndlen + rcvlen; left > 0; left -= res) {
			/* One byte at a time, for simplicity.. */
			for (j = sizeof(buf) - res; j < sizeof(buf); j++)
				buf[j] = (unsigned char)(i + pos[i]++);

			res = send(fds[i], buf, MIN(left, sizeof(buf)), 0);
			if (res == -1 && errno == EWOULDBLOCK)
				break;

			if (res <= 0) e(0);
			if (res > left) e(0);

			if (res < sizeof(buf))
				memmove(buf, &buf[res], sizeof(buf) - res);
		}

		/* Synchronization point A. */
		k = (left == 0);
		if (write(pfd[1], &k, sizeof(k)) != sizeof(k)) e(0);

		if (left > 0)
			break;
	}

	if (close(lfd) != 0) e(0);

	/*
	 * We should always be able to fill at least two socket pairs' buffers
	 * completely this way; in fact with a 512x512 pool it should be three,
	 * but some sockets may be in use in the background.  With the default
	 * settings of the memory pool system, we should ideally be able to get
	 * up to 96 socket pairs.
	 */
	if (i < 3) e(0);

	/*
	 * Mix things up a bit by fully shutting down one file descriptor and
	 * closing another, both on the sending side.
	 */
	if (shutdown(fds[1], SHUT_RDWR) != 0) e(0);
	if (close(fds[2]) != 0) e(0);

	/*
	 * Make sure that when there is buffer space available again, pending
	 * send() calls get woken up.  We do this using a child process that
	 * blocks on a send() call and a parent process that frees up some
	 * buffer space by receiving from another socket.
	 */
	pid2 = fork();
	switch (pid2) {
	case 0:
		errct = 0;

		/* Disable the timeout again. */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		if (setsockopt(fds[i], SOL_SOCKET, SO_SNDTIMEO, &tv,
		    sizeof(tv)) != 0) e(0);

		/*
		 * Try sending.  This should block until there are more buffers
		 * available.
		 */
		res = send(fds[i], buf, MIN(left, sizeof(buf)), 0);
		if (res <= 0) e(0);
		if (res > left) e(0);

		exit(errct);

	case -1:
		e(0);
	}

	/* Make sure the child's send() call is indeed hanging. */
	sleep(2);

	if (waitpid(pid2, &status, WNOHANG) != 0) e(0);

	/* Then receive some data on another socket. */

	/* Synchronization point B. */
	k = 2;
	if (write(pfd[1], &k, sizeof(k)) != sizeof(k)) e(0);

	/* The send() call should now be woken up, eventually. */
	if (waitpid(pid2, &status, 0) != pid2) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * Shut down all (remaining) sending file descriptors for sending, so
	 * that we can receive until we get EOF.  For all but the last socket,
	 * we must get the full size of what we intended to send; for the first
	 * socket, we have already received two buffers worth of data.  Note
	 * that the receipt may take a while, mainly because it takes some time
	 * for sockets that were previously blocked to get going again.
	 */
	for (k = 0; k <= i; k++) {
		if (k != 1 && k != 2 && shutdown(fds[k], SHUT_WR) != 0)
			e(0);
	}

	/* Synchronization point C. */
	k = 3;
	if (write(pfd[1], &k, sizeof(k)) != sizeof(k)) e(0);

	if (close(pfd[1]) != 0) e(0);

	/* Wait for the child to receive everything and terminate. */
	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/* Clean up. */
	do {
		if (i != 2 && close(fds[i]) != 0) e(0);
	} while (i-- > 0);
}

/*
 * Test multicast support.
 */
static void
test91aa(void)
{

	subtest = 27;

	socklib_test_multicast(SOCK_DGRAM, 0);
}

/*
 * Test that putting an unbound TCP socket in listening mode will bind the
 * socket to a port.
 */
static void
test91ab(void)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	socklen_t len;
	int fd;

	subtest = 28;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(0);

	if (listen(fd, 1) != 0) e(0);

	len = sizeof(sin);
	if (getsockname(fd, (struct sockaddr *)&sin, &len) != 0) e(0);
	if (len != sizeof(sin)) e(0);
	if (sin.sin_len != sizeof(sin)) e(0);
	if (sin.sin_family != AF_INET) e(0);
	if (sin.sin_port == htons(0)) e(0);
	if (sin.sin_addr.s_addr != htonl(INADDR_ANY)) e(0);

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (listen(fd, 1) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(fd, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);
	if (sin6.sin6_len != sizeof(sin6)) e(0);
	if (sin6.sin6_family != AF_INET6) e(0);
	if (sin6.sin6_port == htons(0)) e(0);
	if (memcmp(&sin6.sin6_addr, &in6addr_any, sizeof(sin6.sin6_addr)) != 0)
		e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test for connecting to the same remote TCP endpoint with the same local
 * endpoint twice in a row.  The second connection should fail due to the
 * TIME_WAIT state left behind from the first connection, but this previously
 * caused an infinite loop instead.  lwIP bug #50498.
 */
static void
test91ac(void)
{
	struct sockaddr_in6 lsin6, rsin6;
	socklen_t len;
	int fd, fd2, fd3;

	subtest = 29;

	if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	memset(&rsin6, 0, sizeof(rsin6));
	rsin6.sin6_family = AF_INET6;
	memcpy(&rsin6.sin6_addr, &in6addr_loopback, sizeof(rsin6.sin6_addr));

	if (bind(fd, (struct sockaddr *)&rsin6, sizeof(rsin6)) != 0) e(0);

	len = sizeof(rsin6);
	if (getsockname(fd, (struct sockaddr *)&rsin6, &len) != 0) e(0);
	if (len != sizeof(rsin6)) e(0);

	if (listen(fd, 1) != 0) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&rsin6, sizeof(rsin6)) != 0) e(0);

	if ((fd3 = accept(fd, (struct sockaddr *)&lsin6, &len)) < 0) e(0);
	if (len != sizeof(rsin6)) e(0);

	/* The server end must initiate the close for this to work. */
	if (close(fd3) != 0) e(0);

	if (close(fd2) != 0) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) e(0);

	if (bind(fd2, (struct sockaddr *)&lsin6, sizeof(lsin6)) != 0) e(0);

	/*
	 * The timeout should occur almost immediately, due to a shortcut in
	 * lwIP (which was also the source of the problem here).  The actual
	 * error code is not really important though.  In fact, if in the
	 * future the connection does get established, that is still not an
	 * issue - in fact, it would be nice to have a working rsh(1), which is
	 * how this problem showed up in the first place - but at the very
	 * least the service should keep operating.
	 */
	if (connect(fd2, (struct sockaddr *)&rsin6, sizeof(rsin6)) != -1) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);
}

/*
 * Test program for LWIP TCP/UDP sockets.
 */
int
main(int argc, char ** argv)
{
	unsigned int m;
	int i;

	start(91);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFFFFFFFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x00000001) test91a();
		if (m & 0x00000002) test91b();
		if (m & 0x00000004) test91c();
		if (m & 0x00000008) test91d();
		if (m & 0x00000010) test91e();
		if (m & 0x00000020) test91f();
		if (m & 0x00000040) test91g();
		if (m & 0x00000080) test91h();
		if (m & 0x00000100) test91i();
		if (m & 0x00000200) test91j();
		if (m & 0x00000400) test91k();
		if (m & 0x00000800) test91l();
		if (m & 0x00001000) test91m();
		if (m & 0x00002000) test91n();
		if (m & 0x00004000) test91o();
		if (m & 0x00008000) test91p();
		if (m & 0x00010000) test91q();
		if (m & 0x00020000) test91r();
		if (m & 0x00040000) test91s();
		if (m & 0x00080000) test91t();
		if (m & 0x00100000) test91u();
		if (m & 0x00200000) test91v();
		if (m & 0x00400000) test91w();
		if (m & 0x00800000) test91x();
		if (m & 0x01000000) test91y();
		if (m & 0x02000000) test91z();
		if (m & 0x04000000) test91aa();
		if (m & 0x08000000) test91ab();
		if (m & 0x10000000) test91ac();
	}

	quit();
	/* NOTREACHED */
}
