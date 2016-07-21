/* Advanced tests for UNIX Domain Sockets - by D.C. van Moolenbroek */
/*
 * This is a somewhat random collection of in-depth tests, complementing the
 * more general functionality tests in test56.  The overall test set is still
 * by no means expected to be "complete."  The subtests are in random order.
 */
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

#include "common.h"
#include "socklib.h"

#define ITERATIONS	1

#define SOCK_PATH_A	"sock_a"
#define SOCK_PATH_B	"sock_b"
#define SOCK_PATH_C	"sock_c"
#define SOCK_PATH_D	"sock_d"

#define SOCK_PATH_A_X	".//sock_a"
#define SOCK_PATH_A_Y	"./././sock_a"

#define PRINT_STATS	0

/*
 * Check that the given returned socket address matches the given path.  A NULL
 * path may be passed in to indicate the result should be for an unbound
 * socket.
 */
static void
check_addr(struct sockaddr_un * sun, socklen_t len, const char * path)
{

	if (len < offsetof(struct sockaddr_un, sun_path)) e(0);

	if (sun->sun_family != AF_UNIX) e(0);
	if (sun->sun_len != len - ((path != NULL) ? 1 : 0)) e(0);

	len -= offsetof(struct sockaddr_un, sun_path);

	if (path != NULL) {
		if (len != strlen(path) + 1) e(0);
		if (sun->sun_path[len - 1] != '\0') e(0);
		if (strcmp(sun->sun_path, path)) e(0);
	} else
		if (len != 0) e(0);
}

/*
 * Get a socket of the given type, bound to the given path.  Return the file
 * descriptor, as well as the bound addres in 'sun'.
 */
static int
get_bound_socket(int type, const char * path, struct sockaddr_un * sun)
{
	int fd;

	if ((fd = socket(AF_UNIX, type, 0)) < 0) e(0);

	(void)unlink(path);

	memset(sun, 0, sizeof(*sun));
	sun->sun_family = AF_UNIX;
	strlcpy(sun->sun_path, path, sizeof(sun->sun_path));

	if (bind(fd, (struct sockaddr *)sun, sizeof(*sun)) != 0) e(0);

	return fd;
}

/*
 * Get a pair of connected sockets.
 */
static void
get_socket_pair(int type, int fd[2])
{
	struct sockaddr_un sunA, sunB;

	if ((type & ~SOCK_FLAGS_MASK) == SOCK_DGRAM) {
		fd[0] = get_bound_socket(type, SOCK_PATH_A, &sunA);
		fd[1] = get_bound_socket(type, SOCK_PATH_B, &sunB);

		if (connect(fd[0], (struct sockaddr *)&sunB,
		    sizeof(sunB)) != 0) e(0);
		if (connect(fd[1], (struct sockaddr *)&sunA,
		    sizeof(sunA)) != 0) e(0);

		if (unlink(SOCK_PATH_A) != 0) e(0);
		if (unlink(SOCK_PATH_B) != 0) e(0);
	} else
		if (socketpair(AF_UNIX, type, 0, fd) != 0) e(0);
}

/*
 * Return the receive buffer size of the given socket.
 */
static int
get_rcvbuf_len(int fd)
{
	socklen_t len;
	int val;

	len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, &len) != 0) e(0);

	if (len != sizeof(val)) e(0);
	if (val <= 0) e(0);

	return val;
}

static const enum state unix_connect_states[] = {
		S_NEW,		S_N_SHUT_R,	S_N_SHUT_W,	S_N_SHUT_RW,
		S_BOUND,	S_LISTENING,	S_L_SHUT_R,	S_L_SHUT_W,
		S_L_SHUT_RW,	S_CONNECTING,	S_CONNECTED,	S_ACCEPTED,
		S_SHUT_R,	S_SHUT_W,	S_SHUT_RW,	S_RSHUT_R,
		S_RSHUT_W,	S_RSHUT_RW,	S_SHUT2_R,	S_SHUT2_W,
		S_SHUT2_RW,	S_PRE_EOF,	S_AT_EOF,	S_POST_EOF,
		S_PRE_SHUT_R,	S_EOF_SHUT_R,	S_POST_SHUT_R,	S_PRE_SHUT_W,
		S_EOF_SHUT_W,	S_POST_SHUT_W,	S_PRE_SHUT_RW,	S_EOF_SHUT_RW,
		S_POST_SHUT_RW,	S_AT_RESET,	S_POST_RESET,	S_POST_FAILED
		/*
		 * It is impossible to generate the S_PRE_RESET state: we can
		 * only generate a reset on a connected socket for which the
		 * other end is pending acceptance, by closing the listening
		 * socket.  That means we cannot send data to the connected end
		 * (from the listening socket) before triggering the reset.
		 *
		 * It is impossible to generate the S_FAILED state: even a non-
		 * blocking connect will always fail immediately when it cannot
		 * connect to the target.
		 */
};

static const int unix_connect_results[][__arraycount(unix_connect_states)] = {
	[C_ACCEPT]		= {
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EAGAIN,	-ECONNABORTED,	-ECONNABORTED,
		-ECONNABORTED,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
	},
	[C_BIND]		= {
		0,		0,		0,		0,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	0,		0,		-EINVAL,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
	},
	[C_CONNECT]		= {
		0,		0,		0,		0,
		0,		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EALREADY,	-EISCONN,	-EISCONN,
		-EISCONN,	-EISCONN,	-EISCONN,	-EISCONN,
		-EISCONN,	-EISCONN,	-EISCONN,	-EISCONN,
		-EISCONN,	0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
	},
	[C_GETPEERNAME]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
	},
	[C_GETSOCKNAME]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
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
		0,		-ECONNRESET,	0,		0,
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
	},
	[C_GETSOCKOPT_RB]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
	},
	[C_IOCTL_NREAD]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		1,		0,		0,
		0,		0,		0,		1,
		0,		0,		0,		0,
		0,		0,		0,		0,
	},
	[C_LISTEN]		= {
		-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,
		0,		0,		0,		0,
		0,		-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,	-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,
		-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,
		-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,
		-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,	-EDESTADDRREQ,
	},
	[C_RECV]		= {
		-ENOTCONN,	0,		-ENOTCONN,	0,
		-ENOTCONN,	-ENOTCONN,	0,		-ENOTCONN,
		0,		-EAGAIN,	-EAGAIN,	-EAGAIN,
		0,		-EAGAIN,	0,		-EAGAIN,
		0,		0,		0,		0,
		0,		1,		0,		0,
		0,		0,		0,		1,
		0,		0,		0,		0,
		0,		-ECONNRESET,	0,		-ENOTCONN,
	},
	[C_RECVFROM]		= {
		-ENOTCONN,	0,		-ENOTCONN,	0,
		-ENOTCONN,	-ENOTCONN,	0,		-ENOTCONN,
		0,		-EAGAIN,	-EAGAIN,	-EAGAIN,
		0,		-EAGAIN,	0,		-EAGAIN,
		0,		0,		0,		0,
		0,		1,		0,		0,
		0,		0,		0,		1,
		0,		0,		0,		0,
		0,		-ECONNRESET,	0,		-ENOTCONN,
	},
	[C_SEND]		= {
		-ENOTCONN,	-ENOTCONN,	-EPIPE,		-EPIPE,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-EPIPE,
		-EPIPE,		-EAGAIN,	1,		1,
		1,		-EPIPE,		-EPIPE,		-EPIPE,
		1,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-ECONNRESET,	-EPIPE,		-ENOTCONN,
	},
	[C_SENDTO]		= {
		-ENOTCONN,	-ENOTCONN,	-EPIPE,		-EPIPE,
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-EPIPE,
		-EPIPE,		-EAGAIN,	1,		1,
		1,		-EPIPE,		-EPIPE,		-EPIPE,
		1,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-EPIPE,		-EPIPE,		-EPIPE,
		-EPIPE,		-ECONNRESET,	-EPIPE,		-ENOTCONN,
	},
	[C_SELECT_R]		= {
		1,		1,		1,		1,
		1,		0,		1,		1,
		1,		0,		0,		0,
		1,		0,		1,		0,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
	},
	[C_SELECT_W]		= {
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		0,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,		1,		1,		1,
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
	},
	[C_SHUTDOWN_RW]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
	},
	[C_SHUTDOWN_W]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		0,
	},
};

/*
 * Set up a connection-oriented socket file descriptor in the requested state
 * and pass it to socklib_sweep_call() along with local and remote addresses
 * and their length.
 */
static int
unix_connect_sweep(int domain, int type, int protocol __unused,
	enum state state, enum call call)
{
	struct sockaddr_un sunA, sunB, sunC;
	char buf[1];
	socklen_t len;
	fd_set fds;
	int r, fd, fd2, fd3, tmpfd, val, fl;

	(void)unlink(SOCK_PATH_A);
	(void)unlink(SOCK_PATH_B);
	(void)unlink(SOCK_PATH_C);

	memset(&sunA, 0, sizeof(sunA));
	sunA.sun_family = AF_UNIX;
	strlcpy(sunA.sun_path, SOCK_PATH_A, sizeof(sunA.sun_path));

	fd = fd3 = -1;

	fd2 = get_bound_socket(type | SOCK_NONBLOCK, SOCK_PATH_B, &sunB);

	if (listen(fd2, 1) == -1) e(0);

	switch (state) {
	case S_NEW:
	case S_N_SHUT_R:
	case S_N_SHUT_W:
	case S_N_SHUT_RW:
		if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

		switch (state) {
		case S_N_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_N_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_N_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		break;

	case S_BOUND:
	case S_LISTENING:
	case S_L_SHUT_R:
	case S_L_SHUT_W:
	case S_L_SHUT_RW:
		fd = get_bound_socket(type | SOCK_NONBLOCK, SOCK_PATH_A,
		    &sunA);

		if (state == S_BOUND)
			break;

		if (listen(fd, 1) == -1) e(0);

		switch (state) {
		case S_L_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_L_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_L_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
		}

		break;

	case S_CONNECTING:
		/*
		 * The following block is nonportable.  On NetBSD, the
		 * LOCAL_CONNWAIT socket option is present but seems somewhat..
		 * under-tested.  On Linux, it is not possible to put a UNIX
		 * domain socket in a connecting state.
		 */
		if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

		val = 1;
		if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0)
			e(0);

		if (connect(fd, (struct sockaddr *)&sunB, sizeof(sunB)) != -1)
			e(0);
		if (errno != EINPROGRESS) e(0);

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
		if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

		if (connect(fd, (struct sockaddr *)&sunB, sizeof(sunB)) != 0)
			e(0);

		len = sizeof(sunC);
		if ((fd3 = accept(fd2, (struct sockaddr *)&sunC, &len)) < 0)
			e(0);

		if ((fl = fcntl(fd3, F_GETFL)) == -1) e(0);
		if (fcntl(fd3, F_SETFL, fl | O_NONBLOCK) != 0) e(0);

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
		if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

		if (connect(fd, (struct sockaddr *)&sunB, sizeof(sunB)) != 0)
			e(0);

		len = sizeof(sunC);
		if ((fd3 = accept(fd2, (struct sockaddr *)&sunC, &len)) < 0)
			e(0);

		if ((fl = fcntl(fd3, F_GETFL)) == -1) e(0);
		if (fcntl(fd3, F_SETFL, fl | O_NONBLOCK) != 0) e(0);

		if (send(fd3, "", 1, 0) != 1) e(0);

		if (close(fd3) != 0) e(0);
		fd3 = -1;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);

		switch (state) {
		case S_AT_EOF:
		case S_EOF_SHUT_R:
		case S_EOF_SHUT_W:
		case S_EOF_SHUT_RW:
			if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
			break;
		case S_POST_EOF:
		case S_POST_SHUT_R:
		case S_POST_SHUT_W:
		case S_POST_SHUT_RW:
			if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
			if (recv(fd, buf, sizeof(buf), 0) != 0) e(0);
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

	case S_AT_RESET:
	case S_POST_RESET:
		if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

		if (connect(fd, (struct sockaddr *)&sunB, sizeof(sunB)) != 0)
			e(0);

		/*
		 * Closing the listening socket before the connection has been
		 * accepted should generate ECONNRESET on the connected socket.
		 * Well, should.. we choose to do that.  So does Linux.  NetBSD
		 * just returns EOF for that case.  There are really no strong
		 * arguments for either behavior.
		 */
		if (close(fd2) != 0) e(0);

		if (state == S_POST_RESET)
			(void)recv(fd, buf, sizeof(buf), 0);

		/* Recreate the listening socket just for consistency. */
		fd2 = get_bound_socket(type | SOCK_NONBLOCK, SOCK_PATH_B,
		    &sunB);

		if (listen(fd2, 1) == -1) e(0);

		break;

	case S_POST_FAILED:
		if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

		memset(&sunC, 0, sizeof(sunC));
		sunC.sun_family = AF_UNIX;
		strlcpy(sunC.sun_path, SOCK_PATH_C, sizeof(sunC.sun_path));

		r = connect(fd, (struct sockaddr *)&sunC, sizeof(sunC));
		if (r != -1 || errno != ENOENT)
			e(0);

		break;

	default:
		e(0);
	}

	r = socklib_sweep_call(call, fd, (struct sockaddr *)&sunA,
	    (struct sockaddr *)&sunB, sizeof(struct sockaddr_un));

	if (fd >= 0 && close(fd) != 0) e(0);
	if (fd2 >= 0 && close(fd2) != 0) e(0);
	if (fd3 >= 0 && close(fd3) != 0) e(0);

	(void)unlink(SOCK_PATH_A);
	(void)unlink(SOCK_PATH_B);

	return r;
}

static const enum state unix_dgram_states[] = {
		S_NEW,		S_N_SHUT_R,	S_N_SHUT_W,	S_N_SHUT_RW,
		S_BOUND,	S_CONNECTED,	S_SHUT_R,	S_SHUT_W,
		S_SHUT_RW,	S_RSHUT_R,	S_RSHUT_W,	S_RSHUT_RW,
		S_SHUT2_R,	S_SHUT2_W,	S_SHUT2_RW,	S_PRE_RESET,
		S_AT_RESET,	S_POST_RESET
};

static const int unix_dgram_results[][__arraycount(unix_dgram_states)] = {
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
		0,		0,		0,		-ECONNREFUSED,
		-ECONNREFUSED,	-ECONNREFUSED,
	},
	[C_GETPEERNAME]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	0,		0,		0,
		0,		0,		0,		0,
		0,		0,		0,		-ENOTCONN,
		-ENOTCONN,	-ENOTCONN,
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
		0,		0,		0,		-ECONNRESET,
		-ECONNRESET,	0,
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
		-ECONNRESET,	-EAGAIN,
	},
	[C_RECVFROM]		= {
		-EAGAIN,	0,		-EAGAIN,	0,
		-EAGAIN,	-EAGAIN,	0,		-EAGAIN,
		0,		-EAGAIN,	-EAGAIN,	-EAGAIN,
		0,		-EAGAIN,	0,		1,
		-ECONNRESET,	-EAGAIN,
	},
	[C_SEND]		= {
		-EDESTADDRREQ,	-EDESTADDRREQ,	-EPIPE,		-EPIPE,
		-EDESTADDRREQ,	1,		1,		-EPIPE,
		-EPIPE,		-ENOBUFS,	1,		-ENOBUFS,
		-ENOBUFS,	-EPIPE,		-EPIPE,		-ECONNRESET,
		-ECONNRESET,	-EDESTADDRREQ,
	},
	[C_SENDTO]		= {
		1,		1,		-EPIPE,		-EPIPE,
		1,		1,		1,		-EPIPE,
		-EPIPE,		-ENOBUFS,	1,		-ENOBUFS,
		-ENOBUFS,	-EPIPE,		-EPIPE,		-ECONNRESET,
		-ECONNRESET,	-ECONNREFUSED,
	},
	[C_SELECT_R]		= {
		0,		1,		0,		1,
		0,		0,		1,		0,
		1,		0,		0,		0,
		1,		0,		1,		1,
		1,		0,
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
 * Set up a datagram socket file descriptor in the requested state and pass it
 * to socklib_sweep_call() along with local and remote addresses and their
 * length.
 */
static int
unix_dgram_sweep(int domain __unused, int type, int protocol __unused,
	enum state state, enum call call)
{
	struct sockaddr_un sunA, sunB;
	char buf[1];
	int r, fd, fd2;

	(void)unlink(SOCK_PATH_A);
	(void)unlink(SOCK_PATH_B);

	/* Create a bound remote socket. */
	fd2 = get_bound_socket(type | SOCK_NONBLOCK, SOCK_PATH_B, &sunB);

	switch (state) {
	case S_NEW:
	case S_N_SHUT_R:
	case S_N_SHUT_W:
	case S_N_SHUT_RW:
		memset(&sunA, 0, sizeof(sunA));
		sunA.sun_family = AF_UNIX;
		strlcpy(sunA.sun_path, SOCK_PATH_A, sizeof(sunA.sun_path));

		if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0)
			e(0);

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
		fd = get_bound_socket(type | SOCK_NONBLOCK, SOCK_PATH_A,
		    &sunA);

		if (state == S_BOUND)
			break;

		if (connect(fd, (struct sockaddr *)&sunB, sizeof(sunB)) != 0)
			e(0);

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
			if (sendto(fd2, "", 1, 0, (struct sockaddr *)&sunA,
			    sizeof(sunA)) != 1) e(0);

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

	r = socklib_sweep_call(call, fd, (struct sockaddr *)&sunA,
	    (struct sockaddr *)&sunB, sizeof(struct sockaddr_un));

	if (close(fd) != 0) e(0);
	if (fd2 != -1 && close(fd2) != 0) e(0);

	(void)unlink(SOCK_PATH_A);
	(void)unlink(SOCK_PATH_B);

	return r;
}

/*
 * Sweep test for socket calls versus socket states of all socket types.
 */
static void
test90a(void)
{

	subtest = 1;

	socklib_sweep(AF_UNIX, SOCK_STREAM, 0, unix_connect_states,
	    __arraycount(unix_connect_states),
	    (const int *)unix_connect_results, unix_connect_sweep);

	socklib_sweep(AF_UNIX, SOCK_SEQPACKET, 0, unix_connect_states,
	    __arraycount(unix_connect_states),
	    (const int *)unix_connect_results, unix_connect_sweep);

	socklib_sweep(AF_UNIX, SOCK_DGRAM, 0, unix_dgram_states,
	    __arraycount(unix_dgram_states), (const int *)unix_dgram_results,
	    unix_dgram_sweep);

}

/*
 * Test for large sends and receives with MSG_WAITALL.
 */
static void
test90b(void)
{
	int fd[2];

	subtest = 2;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) != 0) e(0);

	socklib_large_transfers(fd);
}

/*
 * A randomized producer-consumer test for datagram sockets.
 */
static void
sub90c(int type)
{
	char *buf;
	time_t t;
	socklen_t len, size;
	ssize_t r;
	pid_t pid;
	unsigned int count;
	int i, fd[2], rcvlen, status, exp, flags, num, stat[2] = { 0, 0 };

	get_socket_pair(type, fd);

	size = rcvlen = get_rcvbuf_len(fd[0]);

	if ((buf = malloc(size)) == NULL) e(0);

	t = time(NULL);

	/*
	 * We vary small versus large (random) send and receive sizes,
	 * splitting the entire transfer in four phases along those lines.
	 *
	 * In theory, the use of an extra system call and the use of MSG_PEEK
	 * both contribute to the expectation that the consumer side will fall
	 * behind the producer.  In this case, we cannot vary receive sizes to
	 * compensate.  This not appear to be a major problem here, though.
	 */
#define NR_PACKETS	(256 * 1024)

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[0]) != 0) e(0);

		srand48(t + 1);

		for (count = 0; count < NR_PACKETS; ) {
			if (count < NR_PACKETS / 2)
				len = lrand48() % 64;
			else
				len = lrand48() % size;

			num = lrand48() % 16;
			flags = 0;
			if (num & 1) flags |= MSG_PEEK;
			if (num & 2) flags |= MSG_WAITALL;
			if (num & 4) flags |= MSG_DONTWAIT;
			if (num & 8) {
				/*
				 * Obviously there are race conditions here but
				 * the returned number should be accurate if
				 * not zero.  Hopefully it's not always zero.
				 */
				if (ioctl(fd[1], FIONREAD, &exp) != 0) e(0);
				if (exp < 0 || exp > rcvlen) e(0);
			} else
				exp = 0;

			stat[0]++;

			/*
			 * A lame approach to preventing unbounded spinning on
			 * ENOBUFS on the producer side.
			 */
			if (type == SOCK_DGRAM)
				(void)send(fd[1], "", 1, MSG_DONTWAIT);

			if ((r = recv(fd[1], buf, len, flags)) == -1) {
				if (errno != EWOULDBLOCK) e(0);
				if (exp > 0) e(0);

				stat[1]++;

				continue;
			}

			if (exp != 0) {
				if (r == len && exp < r) e(0);
				else if (r < len && exp != r) e(0);
			}

			if (r >= 2 &&
			    r > ((size_t)(unsigned char)buf[0] << 8) +
			    (size_t)(unsigned char)buf[1]) e(0);

			for (i = 2; i < r; i++)
				if (buf[i] != (char)i) e(0);

			if (!(flags & MSG_PEEK))
				count++;
		}

#if PRINT_STATS
		/*
		 * The second and third numbers should ideally be a large but
		 * non-dominating fraction of the first one.
		 */
		printf("RECV: total %d again %d\n", stat[0], stat[1]);
#endif

		if (close(fd[1]) != 0) e(0);
		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd[1]) != 0) e(0);

	srand48(t);

	for (count = 0; count < NR_PACKETS; ) {
		if (count < NR_PACKETS / 4 ||
		    (count >= NR_PACKETS / 2 && count < NR_PACKETS * 3 / 4))
			len = lrand48() % 64;
		else
			len = lrand48() % size;

		buf[0] = (len >> 8) & 0xff;
		buf[1] = len & 0xff;
		for (i = 2; i < len; i++)
			buf[i] = i;

		flags = (lrand48() % 2) ? MSG_DONTWAIT : 0;

		r = send(fd[0], buf, len, flags);

		if (r != len) {
			if (r != -1) e(0);

			if (errno != EMSGSIZE && errno != EWOULDBLOCK &&
			    errno != ENOBUFS) e(0);

			if (errno == ENOBUFS || errno == EWOULDBLOCK) {
				/*
				 * As stated above: lame.  Ideally we would
				 * continue only when the receiver side drains
				 * the queue, but it may block once it has done
				 * so.  Instead, by going through consumer
				 * "tokens" we will ultimately block here and
				 * let the receiver catch up.
				 */
				if (type == SOCK_DGRAM && errno == ENOBUFS)
					(void)recv(fd[0], buf, 1, 0);

				stat[0]++;
				stat[1]++;
			}
			continue;
		} else
			stat[0]++;

		if (count % (NR_PACKETS / 4) == 0)
			sleep(1);

		count++;
	}

#if PRINT_STATS
	/*
	 * The second number should ideally be a large but non-dominating
	 * fraction of the first one.
	 */
	printf("SEND: total %d again %d\n", stat[0], stat[1]);
#endif

	free(buf);

	if (close(fd[0]) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);
}

/*
 * A randomized producer-consumer test.  As part of this, we also perform very
 * basic bulk functionality tests of FIONREAD, MSG_PEEK, MSG_DONTWAIT, and
 * MSG_WAITALL.
 */
static void
test90c(void)
{
	int fd[2];

	subtest = 4;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) != 0) e(0);

	socklib_producer_consumer(fd);

	sub90c(SOCK_SEQPACKET);

	sub90c(SOCK_DGRAM);
}

/*
 * Test that immediately accepted non-blocking connect requests to a listening
 * socket with LOCAL_CONNWAIT turned on, return OK rather than EINPROGRESS.
 * This requires a hack in libsockevent.
 */
static void
test90d(void)
{
	struct sockaddr_un sunA, sunB;
	socklen_t len;
	pid_t pid;
	int fd, fd2, fd3, val, status;

	subtest = 4;

	/*
	 * First ensure that a non-blocking connect to a listening socket that
	 * does not have a accept call blocked on it, fails with EINPROGRESS.
	 */
	fd = get_bound_socket(SOCK_STREAM, SOCK_PATH_A, &sunA);

	val = 1;
	if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0) e(0);

	if (listen(fd, 1) != 0) e(0);

	if ((fd2 = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != -1) e(0);
	if (errno != EINPROGRESS) e(0);

	len = sizeof(sunB);
	if ((fd3 = accept(fd, (struct sockaddr *)&sunB, &len)) < 0) e(0);
	check_addr(&sunB, len, NULL);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);

	/*
	 * Second, ensure that a blocking connect eventually does return
	 * success if an accept call is made later on.
	 */
	fd = get_bound_socket(SOCK_STREAM, SOCK_PATH_A, &sunA);

	val = 1;
	if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0) e(0);

	if (listen(fd, 1) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		sleep(1);

		len = sizeof(sunB);
		if ((fd2 = accept(fd, (struct sockaddr *)&sunB, &len)) < 0)
			e(0);
		check_addr(&sunB, len, NULL);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (close(fd) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * Finally, test the most implementation-complex case: a non-blocking
	 * connect should succeed (i.e., yield return code 0) immediately if
	 * there is a accept call blocked on it.
	 */
	fd = get_bound_socket(SOCK_STREAM, SOCK_PATH_A, &sunA);

	val = 1;
	if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0) e(0);

	if (listen(fd, 1) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		len = sizeof(sunB);
		if ((fd2 = accept(fd, (struct sockaddr *)&sunB, &len)) < 0)
			e(0);
		check_addr(&sunB, len, SOCK_PATH_B);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd) != 0) e(0);

	sleep(1);

	fd = get_bound_socket(SOCK_STREAM | SOCK_NONBLOCK, SOCK_PATH_B, &sunB);

	if (connect(fd, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (close(fd) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
	if (unlink(SOCK_PATH_B) != 0) e(0);
}

/*
 * Test self-connecting datagram sockets.
 */
static void
test90e(void)
{
	struct sockaddr_un sunA, sunB, sunC;
	socklen_t len;
	char buf[3];
	pid_t pid;
	int fdA, fdB, val, status;

	subtest = 5;

	fdA = get_bound_socket(SOCK_DGRAM, SOCK_PATH_A, &sunA);
	fdB = get_bound_socket(SOCK_DGRAM, SOCK_PATH_B, &sunB);

	/* Connect the socket to itself, and attempt to communicate. */
	if (connect(fdA, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (send(fdA, "abc", 3, 0) != 3) e(0);

	if (recv(fdA, buf, sizeof(buf), 0) != 3) e(0);
	if (strncmp(buf, "abc", 3)) e(0);

	/* Reconnect the socket to another target. */
	if (connect(fdA, (struct sockaddr *)&sunB, sizeof(sunB)) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fdA, SOL_SOCKET, SO_ERROR, &val, &len) != 0) e(0);
	if (val != 0) e(0);

	if (send(fdA, "def", 3, 0) != 3) e(0);

	memset(&sunC, 0, sizeof(sunC));
	len = sizeof(sunC);
	if (recvfrom(fdB, buf, sizeof(buf), 0, (struct sockaddr *)&sunC,
	    &len) != 3) e(0);
	check_addr(&sunC, len, SOCK_PATH_A);
	if (strncmp(buf, "def", 3)) e(0);

	/* Reconnect the socket to itself again. */
	if (connect(fdA, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fdA, SOL_SOCKET, SO_ERROR, &val, &len) != 0) e(0);
	if (val != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (recv(fdA, buf, sizeof(buf), 0) != 3) e(0);
		if (strncmp(buf, "ghi", 3)) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	sleep(1);

	if (send(fdA, "ghi", 3, 0) != 3) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	if (close(fdA) != 0) e(0);
	if (close(fdB) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
	if (unlink(SOCK_PATH_B) != 0) e(0);
}

/*
 * Test multiple blocked calls getting resumed (or not) upon connect(2) success
 * or failure.  This test uses LOCAL_CONNWAIT.  TODO: rewrite this to use
 * interprocess communication rather than the current carefully arranged and
 * rather brittle timing approach.
 */
static void
sub90f(unsigned int test)
{
	struct sockaddr_un sun;
	pid_t pid[4], apid;
	char buf[1];
	unsigned int i;
	socklen_t len;
	int r, fd, fd2, fl, val, status;

	fd = get_bound_socket(SOCK_STREAM, SOCK_PATH_A, &sun);

	val = 1;
	if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0) e(0);

	if (listen(fd, 1) != 0) e(0);

	apid = fork();
	switch (apid) {
	case 0:
		errct = 0;

		sleep(3);

		if (test < 2) {
			len = sizeof(sun);
			if ((fd2 = accept(fd, (struct sockaddr *)&sun,
			    &len)) < 0) e(0);

			sleep(2);

			if (close(fd2) != 0) e(0);
		}

		if (close(fd) != 0) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) e(0);

	for (i = 0; i < __arraycount(pid); i++) {
		pid[i] = fork();
		switch (pid[i]) {
		case 0:
			errct = 0;

			sleep((i == 0) ? 1 : 2);

			if ((i & 1) == 0) {
				r = send(fd, "", 1, 0);

				switch (test) {
				case 0:
				case 1:
					if (r != 1) e(0);
					break;
				case 3:
					if (i == 0) {
						if (r != -1) e(0);
						if (errno != ECONNRESET) e(0);
						break;
					}
					/* FALLTHROUGH */
				case 2:
					if (r != -1) e(0);
					if (errno != ENOTCONN) e(0);
				}
			} else {
				r = recv(fd, buf, sizeof(buf), 0);

				if (test >= 2) {
					if (r != -1) e(0);
					if (errno != ENOTCONN) e(0);
				} else
					if (r != 0) e(0);
			}

			exit(errct);
		case -1:
			e(0);
		}
	}

	if (test & 1) {
		if ((fl = fcntl(fd, F_GETFL)) == -1) e(0);
		if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) e(0);
	}

	r = connect(fd, (struct sockaddr *)&sun, sizeof(sun));

	if (test & 1) {
		if (r != -1) e(0);
		if (errno != EINPROGRESS) e(0);

		if (fcntl(fd, F_SETFL, fl) != 0) e(0);

		sleep(4);
	} else {
		if (test >= 2) {
			if (r != -1) e(0);
			if (errno != ECONNRESET) e(0);
		} else
			if (r != 0) e(0);

		sleep(1);
	}

	/*
	 * If the connect failed, collect the senders and receivers.
	 * Otherwise, collect just the senders.
	 */
	for (i = 0; i < __arraycount(pid); i++) {
		r = waitpid(pid[i], &status, WNOHANG);
		if (r == pid[i]) {
			if (test < 2 && (i & 1)) e(0);
			if (!WIFEXITED(status)) e(0);
			if (WEXITSTATUS(status) != 0) e(0);
		} else if (r == 0) {
			if (test >= 2 || !(i & 1)) e(0);
		} else
			e(0);
	}

	if (close(fd) != 0) e(0);

	/* Wait for, and collect the accepting child. */
	if (waitpid(apid, &status, 0) != apid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * If the connect succeeded, collect the receivers, which will
	 * terminate once the accepting child closes the accepted socket.
	 */
	if (test < 2) {
		if (waitpid(pid[1], &status, 0) != pid[1]) e(0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);
		if (waitpid(pid[3], &status, 0) != pid[3]) e(0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);
	}

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * Test multiple blocked calls getting resumed (or not) upon connect(2) success
 * or failure.  In particular, ensure that the error code ends up with the
 * right call.
 */
static void
test90f(void)
{

	subtest = 6;

	/* If a connect succeeds, sends continue but reads block until EOF. */
	sub90f(0);	/* blocking connect */
	sub90f(1);	/* non-blocking connect */

	/* If a blocking connect fails, the connect call gets the error. */
	sub90f(2);

	/* If a non-blocking connect fails, the first blocked call gets it. */
	sub90f(3);
}

/*
 * Test whether various calls all return the same expected error code.
 */
static void
sub90g(struct sockaddr_un * sun, int err)
{
	int fd;

	if ((fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0) e(0);

	if (connect(fd, (struct sockaddr *)sun, sizeof(*sun)) != -1) e(0);
	if (errno != err) e(0);

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) e(0);

	if (sendto(fd, "", 1, 0, (struct sockaddr *)sun, sizeof(*sun)) != -1)
		e(0);
	if (errno != err) e(0);

	if (connect(fd, (struct sockaddr *)sun, sizeof(*sun)) != -1) e(0);
	if (errno != err) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test for error codes thrown by connect(2) and sendto(2) with problematic
 * destinations.  In particular, we verify that the errors for sendto(2) are
 * the same as for connect(2), just like on NetBSD and Linux, even though
 * POSIX does not document all of these under sendto(2).
 */
static void
test90g(void)
{
	struct sockaddr_un sun;
	int fd;

	subtest = 7;

	fd = get_bound_socket(SOCK_STREAM, SOCK_PATH_A, &sun);

	sub90g(&sun, EPROTOTYPE);

	if (listen(fd, 1) != 0) e(0);

	sub90g(&sun, EPROTOTYPE);

	if (close(fd) != 0) e(0);

	sub90g(&sun, ECONNREFUSED);

	if (unlink(SOCK_PATH_A) != 0) e(0);

	sub90g(&sun, ENOENT);
}

/*
 * Test addresses returned for unbound connection-type sockets by various
 * calls.
 */
static void
sub90h(int type)
{
	struct sockaddr_un sun;
	socklen_t len;
	char buf[1];
	int fd, fd2, fd3;

	fd = get_bound_socket(type, SOCK_PATH_A, &sun);

	if (listen(fd, 5) != 0) e(0);

	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != 0) e(0);

	/* Test for accept(2), which returns an empty address. */
	memset(&sun, 0, sizeof(sun));
	len = sizeof(sun);
	if ((fd3 = accept(fd, (struct sockaddr *)&sun, &len)) < 0) e(0);
	check_addr(&sun, len, NULL);

	/* Test for recvfrom(2), which ignores the address pointer. */
	if (send(fd2, "", 1, 0) != 1) e(0);

	memset(&sun, 0, sizeof(sun));
	len = sizeof(sun);
	if (recvfrom(fd3, buf, sizeof(buf), 0, (struct sockaddr *)&sun,
	    &len) != 1) e(0);
	if (len != 0) e(0);
	if (sun.sun_family != 0) e(0);
	if (sun.sun_len != 0) e(0);

	/* Test for getsockname(2), which returns an empty address. */
	memset(&sun, 0, sizeof(sun));
	len = sizeof(sun);
	if (getsockname(fd2, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, NULL);

	/* Test for getpeername(2), which returns an empty address. */
	memset(&sun, 0, sizeof(sun));
	len = sizeof(sun);
	if (getpeername(fd3, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, NULL);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * Test addresses returned for unbound sockets by various calls.
 */
static void
test90h(void)
{
	struct sockaddr_un sun;
	socklen_t len;
	char buf[1];
	int fd, fd2;

	subtest = 8;

	/* Connection-type socket tests. */
	sub90h(SOCK_STREAM);

	sub90h(SOCK_SEQPACKET);

	/* Datagram socket tests. */
	fd = get_bound_socket(SOCK_DGRAM, SOCK_PATH_A, &sun);

	if ((fd2 = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) e(0);

	if (sendto(fd2, "", 1, 0, (struct sockaddr *)&sun, sizeof(sun)) != 1)
		e(0);

	/*
	 * Datagram test for recvfrom(2), which returns no address.  This is
	 * the one result in this subtest that is not specified by POSIX and
	 * (not so coincidentally) is different between NetBSD and Linux.
	 * MINIX3 happens to follow Linux behavior for now, but this may be
	 * changed in the future.
	 */
	memset(&sun, 0, sizeof(sun));
	len = sizeof(sun);
	if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sun,
	    &len) != 1) e(0);
	if (len != 0) e(0);
	if (sun.sun_family != 0) e(0);
	if (sun.sun_len != 0) e(0);

	/* Datagram test for getsockname(2), which returns an empty address. */
	memset(&sun, 0, sizeof(sun));
	len = sizeof(sun);
	if (getsockname(fd2, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, NULL);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

#define MAX_FDS 7

/*
 * Send anywhere from zero to MAX_FDS file descriptors onto a socket, possibly
 * along with regular data.  Return the result of the sendmsg(2) call, with
 * errno preserved.  Written to be reusable outside this test set.
 */
static int
send_fds(int fd, const char * data, size_t len, int flags,
	struct sockaddr * addr, socklen_t addr_len, int * fds, int nfds)
{
	union {
		char buf[CMSG_SPACE(MAX_FDS * sizeof(int))];
		struct cmsghdr cmsg;
	} control;
	struct msghdr msg;
	struct iovec iov;

	assert(nfds >= 0 && nfds <= MAX_FDS);

	iov.iov_base = __UNCONST(data);
	iov.iov_len = len;

	memset(&control.cmsg, 0, sizeof(control.cmsg));
	control.cmsg.cmsg_len = CMSG_LEN(nfds * sizeof(int));
	control.cmsg.cmsg_level = SOL_SOCKET;
	control.cmsg.cmsg_type = SCM_RIGHTS;
	memcpy(CMSG_DATA(&control.cmsg), fds, nfds * sizeof(int));

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &control;
	msg.msg_controllen = control.cmsg.cmsg_len;
	msg.msg_name = addr;
	msg.msg_namelen = addr_len;

	return sendmsg(fd, &msg, flags);
}

/*
 * Receive anywhere from zero to up to MAX_FDS file descriptors from a socket,
 * possibly along with regular data.  The 'nfds' parameter must point to the
 * maximum number of file descriptors to be received.  Return the result of the
 * recvmsg(2) call, with errno preserved.  On success, return the received
 * flags in 'rflags', the received file descriptors stored in 'fds' and their
 * number stored in 'nfds'.  Written to be (somewhat) reusable.
 */
static int
recv_fds(int fd, char * buf, size_t size, int flags, int * rflags, int * fds,
	int * nfds)
{
	union {
		char buf[CMSG_SPACE(MAX_FDS * sizeof(int))];
		struct cmsghdr cmsg;
	} control;
	struct msghdr msg;
	struct iovec iov;
	size_t len;
	int r, rnfds;

	assert(*nfds >= 0 && *nfds <= MAX_FDS);

	iov.iov_base = buf;
	iov.iov_len = size;

	memset(&control.cmsg, 0, sizeof(control.cmsg));
	control.cmsg.cmsg_len = CMSG_LEN(*nfds * sizeof(int));
	control.cmsg.cmsg_level = SOL_SOCKET;
	control.cmsg.cmsg_type = SCM_RIGHTS;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &control;
	msg.msg_controllen = control.cmsg.cmsg_len;

	if ((r = recvmsg(fd, &msg, flags)) < 0)
		return r;

	if (msg.msg_controllen > 0) {
		assert(msg.msg_controllen <= sizeof(control));
		assert(msg.msg_controllen >= sizeof(control.cmsg));
		len = control.cmsg.cmsg_len - CMSG_LEN(0);
		assert(len % sizeof(int) == 0);
		rnfds = len / sizeof(int);
		assert(rnfds <= *nfds);

		memcpy(fds, CMSG_DATA(&control.cmsg), rnfds * sizeof(int));
	} else
		rnfds = 0;

	*rflags = msg.msg_flags;
	*nfds = rnfds;
	return r;
}

/*
 * Generate and send zero or more file descriptors onto a socket, possibly
 * along with regular data.  Return the result of the sendmsg(2) call, with
 * errno preserved.  Also return a set of peer FDs for each of the sent file
 * descriptors, which should later be used in a call to close_test_fds().
 */
static int
send_test_fds(int fd, const char * data, size_t len, int flags, int * peers,
	int nfds)
{
	int i, r, saved_errno, fds[MAX_FDS], pfd[2];

	if (nfds > MAX_FDS) e(0);

	for (i = 0; i < nfds; i++) {
		if (pipe2(pfd, O_NONBLOCK) != 0) e(0);

		peers[i] = pfd[0];
		fds[i] = pfd[1];
	}

	r = send_fds(fd, data, len, flags, NULL, 0, fds, nfds);
	saved_errno = errno;

	for (i = 0; i < nfds; i++)
		if (close(fds[i]) != 0) e(0);

	errno = saved_errno;
	return r;
}

/*
 * Given an array of peer file descriptors as returned from a call to
 * send_test_fds(), test if the original file descriptors have correctly been
 * closed, and close all peer file descriptors.  The ultimate goal here is to
 * detect any possible file descriptor leaks in the UDS service.
 */
static void
close_test_fds(int * peers, int nfds)
{
	char buf[1];
	unsigned int i;
	int fd;

	for (i = 0; i < nfds; i++) {
		fd = peers[i];

		/* If the other side is still open, we would get EAGAIN. */
		if (read(fd, buf, sizeof(buf)) != 0) e(0);

		if (close(peers[i]) != 0) e(0);
	}
}

/*
 * Receive and close zero or more file descriptors from a socket, possibly
 * along with regular data.  Return the result of the recvmsg(2) call, with
 * errno preserved.
 */
static int
recv_test_fds(int fd, char * buf, size_t size, int flags, int * rflags,
	int * nfds)
{
	int i, r, saved_errno, fds[MAX_FDS];

	if (*nfds > MAX_FDS) e(0);

	if ((r = recv_fds(fd, buf, size, flags, rflags, fds, nfds)) < 0)
		return r;
	saved_errno = errno;

	for (i = 0; i < *nfds; i++)
		if (close(fds[i]) != 0) e(0);

	errno = saved_errno;
	return r;
}

/*
 * Test receive requests on various socket states and in various forms.
 * Following this function requires a very close look at what is in the
 * receive queue versus what is being received.
 */
static void
sub90i_recv(int fd, int type, int state, int test, int sub, int sentfds)
{
	struct msghdr msg;
	struct iovec iov;
	char data[4];
	unsigned int i;
	int res, err, nfds, rflags;

	memset(data, 0, sizeof(data));

	if (sub & 2) {
		rflags = 0;
		nfds = sentfds;
		res = recv_test_fds(fd, data, (sub & 1) ? 0 : sizeof(data), 0,
		    &rflags, &nfds);
		if (rflags & MSG_CTRUNC) e(0);
		if (nfds != 0 && nfds != sentfds) e(0);
		if ((type == SOCK_STREAM) && (rflags & MSG_TRUNC)) e(0);
	} else {
		iov.iov_base = data;
		iov.iov_len = (sub & 1) ? 0 : sizeof(data);
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		res = recvmsg(fd, &msg, 0);
		if (res >= 0)
			rflags = msg.msg_flags;
		else
			rflags = 0;
		nfds = 0;
	}
	err = errno;

	if (res < -1 || res > (int)sizeof(data)) e(0);

	if (type == SOCK_STREAM) {
		if (sub & 1) {
			/*
			 * Zero-size requests should receive no regular data
			 * and no control data, even if the tail segment is
			 * zero-sized and terminated.  This policy is in place
			 * for simplicity reasons.
			 */
			if (res != 0) e(0);
			if (nfds != 0) e(0);
			if (rflags & MSG_CTRUNC) e(0);

			/*
			 * Since nothing happened yet, do another, now non-
			 * zero receive call immediately, and continue as if
			 * that was the first call.
			 */
			sub = (sub & ~1) | 2;
			nfds = sentfds;
			rflags = 0;
			res = recv_test_fds(fd, data, sizeof(data), 0, &rflags,
			    &nfds);
			if (rflags & (MSG_TRUNC | MSG_CTRUNC)) e(0);
			if (nfds != 0 && nfds != sentfds) e(0);
			err = errno;
			if (res < -1 || res > (int)sizeof(data)) e(0);
		}

		if (state == 0 && !(test & 1) && !(sub & 13)) {
			/*
			 * There are no regular data bytes to be received, and
			 * the current segment may still be extended (i.e.,
			 * there is no EOF condition), and we are trying to
			 * receive at least one data byte.  This is the
			 * SO_RCVLOWAT test.
			 */
			if (res != -1) e(0);
			if (err != EWOULDBLOCK) e(0);
			if (test == 4) {
				/*
				 * There are still pending file descriptors but
				 * we cannot get them, due to the SO_RCVLOWAT
				 * test.  This is proper behavior but somewhat
				 * annoying, because we want to see if UDS
				 * forgot to close any file descriptors.  So,
				 * we let it force-close them here.
				 */
				if (shutdown(fd, SHUT_RD) != 0) e(0);
				sub |= 8;
			}
		} else {
			i = 0;
			if (state == 1) {
				if (res < 1) e(0);
				if (data[i] != 'A') e(0);
				i++;
			}
			if ((state == 0 && (test & 1)) ||
			    (state == 1 && (test == 1 || test == 3))) {
				if (res < i + 1) e(0);
				if (data[i] != 'B') e(0);
				i++;
			}
			if ((sub & 4) && (state != 1 || test < 4)) {
				if (res < i + 1) e(0);
				if (data[i] != 'C') e(0);
				i++;
			}
			if (i != res) e(0);
			if (state == 0 && test >= 4) {
				if (sub & 2) {
					if (nfds != sentfds) e(0);
				} else
					if (!(rflags & MSG_CTRUNC)) e(0);
			}
		}

		if (state == 1 && test >= 4) {
			/*
			 * We just read the first segment, but there is a
			 * second segment with ancillary data.  Read it too.
			 */
			nfds = sentfds;
			rflags = 0;
			res = recv_test_fds(fd, data, sizeof(data), 0, &rflags,
			    &nfds);
			if (rflags & (MSG_TRUNC | MSG_CTRUNC)) e(0);
			if (nfds != sentfds) e(0); /* untouched on failure */
			if (res < -1 || res > (int)sizeof(data)) e(0);
			if (test != 5 && !(sub & 12)) {
				if (res != -1) e(0);
				if (errno != EWOULDBLOCK) e(0);
				/* As above. */
				if (shutdown(fd, SHUT_RD) != 0) e(0);
				sub |= 8;
			} else {
				if (res != (test == 5) + !!(sub & 4)) e(0);
				if (test == 5 && data[0] != 'B') e(0);
				if ((sub & 4) && data[res - 1] != 'C') e(0);
			}
		}
	} else {
		if (res != ((state == 1 || (test & 1)) && !(sub & 1))) e(0);
		if (state == 0 && test >= 4) {
			if (sub & 2) {
				if (nfds != sentfds) e(0);
			} else
				if (!(rflags & MSG_CTRUNC)) e(0);
		}
		if (res > 0 && data[0] != ((state == 1) ? 'A' : 'B')) e(0);

		if (state == 1) {
			nfds = sentfds;
			rflags = 0;
			res = recv_test_fds(fd, data, sizeof(data), 0, &rflags,
			    &nfds);
			if (res != (test & 1)) e(0);
			if (res > 0 && data[0] != 'B') e(0);
			if (nfds != ((test >= 4) ? sentfds : 0)) e(0);
		}

		if (sub & 4) {
			nfds = sentfds;
			rflags = 0;
			res = recv_test_fds(fd, data, sizeof(data), 0, &rflags,
			    &nfds);
			if (res != 1) e(0);
			if (data[0] != 'C') e(0);
			if (nfds != 0) e(0);
		}
	}

	/*
	 * At this point, there is nothing to receive.  Depending on
	 * whether we closed the socket, we expect EOF or EWOULDBLOCK.
	 */
	res = recv(fd, data, sizeof(data), 0);
	if (type == SOCK_DGRAM || !(sub & 8)) {
		if (res != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);
	} else
		if (res != 0) e(0);
}

/*
 * Test send requests on various socket states and in various forms.
 */
static void
sub90i_send(int type, int state, int test, int sub)
{
	char *buf;
	int r, res, err, fd[2], peers[2], rcvlen;

	get_socket_pair(type | SOCK_NONBLOCK, fd);

	/*
	 * State 0: an empty buffer.
	 * State 1: a non-empty, non-full buffer.
	 * State 2: a full buffer.
	 */
	if (state == 2) {
		if (type == SOCK_STREAM) {
			rcvlen = get_rcvbuf_len(fd[0]);

			if ((buf = malloc(rcvlen)) == NULL) e(0);

			memset(buf, 'A', rcvlen);

			if (send(fd[0], buf, rcvlen, 0) != rcvlen) e(0);

			free(buf);
		} else {
			while ((r = send(fd[0], "A", 1, 0)) == 1);
			if (r != -1) e(0);
			if (errno !=
			    ((type == SOCK_SEQPACKET) ? EAGAIN : ENOBUFS))
				e(0);
		}
	} else if (state == 1)
		if (send(fd[0], "A", 1, 0) != 1) e(0);

	/*
	 * Test 0: no data, no control data.
	 * Test 1: data, no control data.
	 * Test 2: no data, empty control data.
	 * Test 3: data, empty control data.
	 * Test 4: no data, control data with a file descriptor.
	 * Test 5: data, control data with a file descriptor.
	 */
	switch (test) {
	case 0:
	case 1:
		res = send(fd[0], "B", test % 2, 0);
		err = errno;
		break;
	case 2:
	case 3:
		res = send_test_fds(fd[0], "B", test % 2, 0, NULL, 0);
		err = errno;
		break;
	case 4:
	case 5:
		res = send_test_fds(fd[0], "B", test % 2, 0, peers,
		    __arraycount(peers));
		err = errno;
		break;
	default:
		res = -1;
		err = EINVAL;
		e(0);
	}

	if (res < -1 || res > 1) e(0);

	switch (state) {
	case 0:
	case 1:
		if (res != (test % 2)) e(0);

		/*
		 * Subtest bit 0x1: try a zero-size receive first.
		 * Subtest bit 0x2: try receiving control data.
		 * Subtest bit 0x4: send an extra segment with no control data.
		 * Subtest bit 0x8: after completing receives, expect EOF.
		 */
		if (sub & 4)
			if (send(fd[0], "C", 1, 0) != 1) e(0);
		if (sub & 8)
			if (shutdown(fd[0], SHUT_WR) != 0) e(0);

		/*
		 * Assuming (sub&4), which means there is an extra "C"..
		 *
		 * For stream sockets, we should now receive:
		 * - state 0:
		 *   - test 0, 2: "C"
		 *   - test 1, 3: "BC"
		 *   - test 4: "C" (w/fds)
		 *   - test 5: "BC" (w/fds)
		 * - state 1:
		 *   - test 0, 2: "AC"
		 *   - test 1, 3: "ABC"
		 *   - test 4: "A", "C" (w/fds)
		 *   - test 5: "A", "BC" (w/fds)
		 *
		 * For packet sockets, we should now receive:
		 * - state 1:
		 *   - all tests: "A", followed by..
		 * - state 0, 1:
		 *   - test 0, 2: "" (no fds), "C"
		 *   - test 1, 3: "B" (no fds), "C"
		 *   - test 4: "" (w/fds), "C"
		 *   - test 5: "B" (w/fds), "C"
		 */
		sub90i_recv(fd[1], type, state, test, sub,
		    __arraycount(peers));

		break;
	case 2:
		/*
		 * Alright, the results are a bit tricky to interpret here,
		 * because UDS's current strict send admission control prevents
		 * the receive buffer from being fully utilized.  We therefore
		 * only test the following aspects:
		 *
		 * - if we sent no regular or control data to a stream socket,
		 *   the call should have succeeded (note that the presence of
		 *   empty control data may cause the call to fail);
		 * - if we sent either regular or control data to a stream
		 *   socket, the call should have failed with EWOULDBLOCK;
		 * - if the call failed, the error should have been EWOULDBLOCK
		 *   for connection-type sockets and ENOBUFS for connectionless
		 *   sockets.
		 *
		 * Everything else gets a pass; we can't even be sure that for
		 * packet-oriented sockets we completely filled up the buffer.
		 */
		if (res == -1) {
			if (type == SOCK_STREAM && test == 0) e(0);

			if (type != SOCK_DGRAM && err != EWOULDBLOCK) e(0);
			if (type == SOCK_DGRAM && err != ENOBUFS) e(0);
		} else
			if (type == SOCK_STREAM && test != 0) e(0);
		break;
	}

	/*
	 * Make sure there are no more in-flight file descriptors now, even
	 * before closing the socket.
	 */
	if (res >= 0 && test >= 4)
		close_test_fds(peers, __arraycount(peers));

	close(fd[0]);
	close(fd[1]);
}

/*
 * Test send and receive requests with regular data, control data, both, or
 * neither, and test segment boundaries.
 */
static void
test90i(void)
{
	int state, test, sub;

	subtest = 9;

	for (state = 0; state < 3; state++) {
		for (test = 0; test < 6; test++) {
			for (sub = 0; sub < ((state < 2) ? 16 : 1); sub++) {
				sub90i_send(SOCK_STREAM, state, test, sub);

				sub90i_send(SOCK_SEQPACKET, state, test, sub);

				sub90i_send(SOCK_DGRAM, state, test, sub);
			}
		}
	}
}

/*
 * Test segmentation of file descriptor transfer on a particular socket type.
 */
static void
sub90j(int type)
{
	char path[PATH_MAX], buf[2];
	int i, fd[2], out[7], in[7], rflags, nfds;
	ssize_t len;

	get_socket_pair(type, fd);

	for (i = 0; i < __arraycount(out); i++) {
		snprintf(path, sizeof(path), "file%d", i);
		out[i] = open(path, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0644);
		if (out[i] < 0) e(0);
		if (write(out[i], path, strlen(path)) != strlen(path)) e(0);
		if (lseek(out[i], 0, SEEK_SET) != 0) e(0);
	}

	if (send_fds(fd[1], "A", 1, 0, NULL, 0, &out[0], 1) != 1) e(0);
	if (send_fds(fd[1], "B", 1, 0, NULL, 0, &out[1], 3) != 1) e(0);
	if (send_fds(fd[1], "C", 1, 0, NULL, 0, &out[4], 2) != 1) e(0);

	nfds = 2;
	if (recv_fds(fd[0], buf, sizeof(buf), 0, &rflags, &in[0], &nfds) != 1)
		e(0);
	if (buf[0] != 'A') e(0);
	if (rflags != 0) e(0);
	if (nfds != 1) e(0);

	nfds = 5;
	if (recv_fds(fd[0], buf, sizeof(buf), 0, &rflags, &in[1], &nfds) != 1)
		e(0);
	if (buf[0] != 'B') e(0);
	if (rflags != 0) e(0);
	if (nfds != 3) e(0);

	if (send_fds(fd[1], "D", 1, 0, NULL, 0, &out[6], 1) != 1) e(0);

	nfds = 2;
	if (recv_fds(fd[0], buf, sizeof(buf), 0, &rflags, &in[4], &nfds) != 1)
		e(0);
	if (buf[0] != 'C') e(0);
	if (rflags != 0) e(0);
	if (nfds != 2) e(0);

	nfds = 2;
	if (recv_fds(fd[0], buf, sizeof(buf), 0, &rflags, &in[6], &nfds) != 1)
		e(0);
	if (buf[0] != 'D') e(0);
	if (rflags != 0) e(0);
	if (nfds != 1) e(0);

	for (i = 0; i < __arraycount(in); i++) {
		len = read(in[i], path, sizeof(path));
		if (len < 5 || len > 7) e(0);
		path[len] = '\0';
		if (strncmp(path, "file", 4) != 0) e(0);
		if (atoi(&path[4]) != i) e(0);
		if (unlink(path) != 0) e(0);
		if (close(in[i]) != 0) e(0);
	}

	for (i = 0; i < __arraycount(out); i++)
		if (close(out[i]) != 0) e(0);

	/*
	 * While we're here, see if UDS properly closes any remaining in-flight
	 * file descriptors when the socket is closed.
	 */
	if (send_test_fds(fd[1], "E", 1, 0, out, 7) != 1) e(0);

	close(fd[0]);
	close(fd[1]);

	close_test_fds(out, 7);
}

/*
 * Test segmentation of file descriptor transfer.  That is, there are multiple
 * in-flight file descriptors, they must each be associated with their
 * respective segments.
 */
static void
test90j(void)
{

	subtest = 10;

	sub90j(SOCK_STREAM);

	sub90j(SOCK_SEQPACKET);

	sub90j(SOCK_DGRAM);
}

/*
 * Test whether we can deadlock UDS by making it close the last reference to
 * an in-flight file descriptor for a UDS socket.  Currently we allow VFS/UDS
 * to get away with throwing EDEADLK as a sledgehammer approach to preventing
 * problems with in-flight UDS sockets.
 */
static void
test90k(void)
{
	int r, fd[2], fd2;

	subtest = 11;

	get_socket_pair(SOCK_STREAM, fd);

	if ((fd2 = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) e(0);

	if ((r = send_fds(fd[0], "X", 1, 0, NULL, 0, &fd2, 1)) != 1) {
		if (r != -1) e(0);
		if (errno != EDEADLK) e(0); /* whew */
	}

	if (close(fd2) != 0) e(0);
	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0); /* boom */
}

/*
 * Test whether we can make UDS run out of file descriptors by transferring a
 * UDS socket over itself and then closing all other references while it is
 * in-flight.  Currently we allow VFS/UDS to get away with throwing EDEADLK as
 * a sledgehammer approach to preventing problems with in-flight UDS sockets.
 */
static void
test90l(void)
{
	struct sockaddr_un sun;
	int i, r, fd, fd2;

	subtest = 12;

	fd = get_bound_socket(SOCK_DGRAM, SOCK_PATH_A, &sun);

	for (i = 0; i < OPEN_MAX + 1; i++) {
		if ((fd2 = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) e(0);

		if ((r = send_fds(fd2, "X", 1, 0, (struct sockaddr *)&sun,
		    sizeof(sun), &fd2, 1)) != 1) {
			if (r != -1) e(0);
			if (errno != EDEADLK) e(0); /* whew */
		}

		if (close(fd2) != 0) e(0); /* have fun in limbo.. */
	}

	if (close(fd) != 0) e(0);
	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * Receive with credentials.
 */
static int
recv_creds(int fd, char * buf, size_t size, int flags, int * rflags,
	struct sockcred * sc, socklen_t * sc_len)
{
	union {
		char buf[CMSG_SPACE(SOCKCREDSIZE(NGROUPS_MAX))];
		struct cmsghdr cmsg;
	} control;
	struct msghdr msg;
	struct iovec iov;
	size_t len;
	int r;

	iov.iov_base = buf;
	iov.iov_len = size;

	memset(&control.cmsg, 0, sizeof(control.cmsg));
	control.cmsg.cmsg_len = CMSG_LEN(SOCKCREDSIZE(NGROUPS_MAX));
	control.cmsg.cmsg_level = SOL_SOCKET;
	control.cmsg.cmsg_type = SCM_RIGHTS;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &control;
	msg.msg_controllen = control.cmsg.cmsg_len;

	if ((r = recvmsg(fd, &msg, flags)) < 0)
		return r;

	if (msg.msg_controllen > 0) {
		assert(msg.msg_controllen <= sizeof(control));
		assert(msg.msg_controllen >= sizeof(control.cmsg));
		assert(control.cmsg.cmsg_len <= msg.msg_controllen);
		len = control.cmsg.cmsg_len - CMSG_LEN(0);
		assert(len >= sizeof(struct sockcred));
		assert(len <= SOCKCREDSIZE(NGROUPS_MAX));
		if (*sc_len > len)
			*sc_len = len;
		memcpy(sc, CMSG_DATA(&control.cmsg), *sc_len);
	} else
		*sc_len = 0;

	*rflags = msg.msg_flags;
	return r;
}

/*
 * Test basic credentials passing on connection-oriented sockets.
 */
static void
sub90m(int type)
{
	struct sockaddr_un sun;
	struct sockcred sc;
	struct msghdr msg;
	struct iovec iov;
	socklen_t len;
	char buf[1];
	int fd, fd2, fd3, val, rflags;

	fd = get_bound_socket(type, SOCK_PATH_A, &sun);

	val = 1;
	if (setsockopt(fd, 0, LOCAL_CREDS, &val, sizeof(val)) != 0) e(0);

	if (listen(fd, 1) != 0) e(0);

	if ((fd2 = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != 0) e(0);

	len = sizeof(sun);
	if ((fd3 = accept(fd, (struct sockaddr *)&sun, &len)) < 0) e(0);

	if (send(fd2, "A", 1, 0) != 1) e(0);
	if (send(fd2, "B", 1, 0) != 1) e(0);

	len = sizeof(sc);
	if (recv_creds(fd3, buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'A') e(0);
	if (rflags != 0) e(0);
	if (len != sizeof(sc)) e(0);
	if (sc.sc_uid != getuid()) e(0);
	if (sc.sc_euid != geteuid()) e(0);

	len = sizeof(sc);
	if (recv_creds(fd3, buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'B') e(0);
	if (rflags != 0) e(0);
	if (len != 0) e(0);

	if (send(fd3, "C", 1, 0) != 1) e(0);

	val = 1;
	if (setsockopt(fd2, 0, LOCAL_CREDS, &val, sizeof(val)) != 0) e(0);

	if (send(fd3, "D", 1, 0) != 1) e(0);

	val = 1;
	if (setsockopt(fd2, 0, LOCAL_CREDS, &val, sizeof(val)) != 0) e(0);

	if (send(fd3, "E", 1, 0) != 1) e(0);

	len = sizeof(sc);
	if (recv_creds(fd2, buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'C') e(0);
	if (rflags != 0) e(0);
	if (len != 0) e(0);

	len = sizeof(sc);
	if (recv_creds(fd2, buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'D') e(0);
	if (rflags != 0) e(0);
	if (len != sizeof(sc)) e(0);
	if (sc.sc_uid != getuid()) e(0);
	if (sc.sc_euid != geteuid()) e(0);

	memset(&msg, 0, sizeof(msg));
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (recvmsg(fd2, &msg, 0) != 1) e(0);
	if (buf[0] != 'E') e(0);
	if (msg.msg_flags != MSG_CTRUNC) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);

	val = 0;
	if (setsockopt(fd, 0, LOCAL_CREDS, &val, sizeof(val)) != 0) e(0);

	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != 0) e(0);

	len = sizeof(sun);
	if ((fd3 = accept(fd, (struct sockaddr *)&sun, &len)) < 0) e(0);

	if (send(fd2, "F", 1, 0) != 1) e(0);

	len = sizeof(sc);
	if (recv_creds(fd3, buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'F') e(0);
	if (rflags != 0) e(0);
	if (len != 0) e(0);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * A few tests for credentials passing that matter to some applications:
 * the credentials passing setting is inherited by accepted connections from
 * their listening socket, and, credentials are passed only once on a
 * connection-oriented socket.
 */
static void
test90m(void)
{
	struct sockcred sc;
	socklen_t len;
	char buf[1];
	int fd[2], val, rflags;

	subtest = 13;

	sub90m(SOCK_STREAM);

	sub90m(SOCK_SEQPACKET);

	get_socket_pair(SOCK_DGRAM, fd);

	val = 1;
	if (setsockopt(fd[0], 0, LOCAL_CREDS, &val, sizeof(val)) != 0) e(0);

	if (send(fd[1], "A", 1, 0) != 1) e(0);
	if (send(fd[0], "B", 1, 0) != 1) e(0);
	if (send(fd[1], "C", 1, 0) != 1) e(0);

	len = sizeof(sc);
	if (recv_creds(fd[0], buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'A') e(0);
	if (rflags != 0) e(0);
	if (len != sizeof(sc)) e(0);
	if (sc.sc_uid != getuid()) e(0);
	if (sc.sc_euid != geteuid()) e(0);

	len = sizeof(sc);
	if (recv_creds(fd[1], buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'B') e(0);
	if (rflags != 0) e(0);
	if (len != 0) e(0);

	len = sizeof(sc);
	if (recv_creds(fd[0], buf, sizeof(buf), 0, &rflags, &sc, &len) != 1)
		e(0);
	if (buf[0] != 'C') e(0);
	if (rflags != 0) e(0);
	if (len != sizeof(sc)) e(0);
	if (sc.sc_uid != getuid()) e(0);
	if (sc.sc_euid != geteuid()) e(0);

	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);
}

/*
 * Test whether MSG_CMSG_CLOEXEC is honored when copying in file descriptors.
 * We do not bother to test with execve(2w); obtaining the FD flags suffices.
 */
static void
test90n(void)
{
	char buf[1];
	int i, fd[2], sfd, rfd, fl, rflags, nfds;

	subtest = 14;

	get_socket_pair(SOCK_STREAM, fd);

	if ((sfd = open("/dev/null", O_RDONLY)) < 0) e(0);

	if (send_fds(fd[0], "A", 1, 0, NULL, 0, &sfd, 1) != 1) e(0);
	if (send_fds(fd[0], "B", 1, 0, NULL, 0, &sfd, 1) != 1) e(0);

	if ((fl = fcntl(sfd, F_GETFD, 0)) < 0) e(0);
	if (fcntl(sfd, F_SETFD, fl | FD_CLOEXEC) != 0) e(0);

	if (send_fds(fd[0], "C", 1, 0, NULL, 0, &sfd, 1) != 1) e(0);
	if (send_fds(fd[0], "D", 1, 0, NULL, 0, &sfd, 1) != 1) e(0);

	for (i = 0; i < 4; i++) {
		fl = (i & 1) ? MSG_CMSG_CLOEXEC : 0;
		nfds = 1;
		if (recv_fds(fd[1], buf, sizeof(buf), fl, &rflags, &rfd,
		    &nfds) != 1) e(0);
		if (buf[0] != 'A' + i) e(0);
		if (rflags != 0) e(0);
		if (nfds != 1) e(0);

		if ((fl = fcntl(rfd, F_GETFD, 0)) < 0) e(0);
		if (!!(fl & FD_CLOEXEC) != (i & 1)) e(0);

		if (close(rfd) != 0) e(0);
	}

	if (close(sfd) != 0) e(0);
	if (close(fd[0]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);
}

/*
 * Test failures sending and receiving sets of file descriptors.
 */
static void
sub90o(int type)
{
	static int ofd[OPEN_MAX];
	char buf[1];
	int i, fd[2], sfd[2], rfd[2], rflags, nfds;

	get_socket_pair(type, fd);

	if ((sfd[0] = open("/dev/null", O_RDONLY)) < 0) e(0);
	sfd[1] = -1;

	if (send_fds(fd[0], "A", 1, 0, NULL, 0, &sfd[1], 1) != -1) e(0);
	if (errno != EBADF) e(0);
	if (send_fds(fd[0], "B", 1, 0, NULL, 0, &sfd[0], 2) != -1) e(0);
	if (errno != EBADF) e(0);
	if ((sfd[1] = dup(sfd[0])) < 0) e(0);
	if (send_fds(fd[0], "C", 1, 0, NULL, 0, &sfd[0], 2) != 1) e(0);

	for (i = 0; i < __arraycount(ofd); i++) {
		if ((ofd[i] = dup(sfd[0])) < 0) {
			/* Either will do. */
			if (errno != EMFILE && errno != ENFILE) e(0);
			break;
		}
	}

	nfds = 2;
	if (recv_fds(fd[1], buf, sizeof(buf), 0, &rflags, rfd, &nfds) != -1)
		e(0);
	if (errno != ENFILE && errno != EMFILE) e(0);

	if (close(sfd[1]) != 0) e(0);

	nfds = 2;
	if (recv_fds(fd[1], buf, sizeof(buf), 0, &rflags, rfd, &nfds) != -1)
		e(0);
	if (errno != ENFILE && errno != EMFILE) e(0);

	if (close(sfd[0]) != 0) e(0);

	nfds = 2;
	if (recv_fds(fd[1], buf, sizeof(buf), 0, &rflags, rfd, &nfds) != 1)
		e(0);
	if (buf[0] != 'C') e(0);
	if (rflags != 0) e(0);
	if (nfds != 2) e(0);

	if (close(rfd[1]) != 0) e(0);
	if (close(rfd[0]) != 0) e(0);
	while (i-- > 0)
		if (close(ofd[i]) != 0) e(0);
	if (close(fd[1]) != 0) e(0);
	if (close(fd[0]) != 0) e(0);
}

/*
 * Test failures sending and receiving sets of file descriptors.
 */
static void
test90o(void)
{
	const int types[] = { SOCK_STREAM, SOCK_SEQPACKET, SOCK_DGRAM };
	int i;

	subtest = 15;

	for (i = 0; i < OPEN_MAX + 1; i++)
		sub90o(types[i % __arraycount(types)]);
}

/*
 * Test socket reuse for a particular socket type.
 */
static void
sub90p(int type)
{
	struct sockaddr_un sunA, sunB, sunC;
	socklen_t len;
	char buf[1];
	uid_t euid;
	gid_t egid;
	int fd, fd2, fd3, val;

	if ((fd = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);
	/* Unconnected. */

	if (getpeereid(fd, &euid, &egid) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	fd2 = get_bound_socket(type, SOCK_PATH_A, &sunA);

	val = 1;
	if (setsockopt(fd2, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0) e(0);

	if (listen(fd2, 5) != 0) e(0);

	if (connect(fd, (struct sockaddr *)&sunA, sizeof(sunA)) != -1) e(0);
	if (errno != EINPROGRESS) e(0);
	/* Connecting. */

	if (getpeereid(fd, &euid, &egid) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	len = sizeof(sunB);
	if ((fd3 = accept(fd2, (struct sockaddr *)&sunB, &len)) < 0) e(0);
	/* Connected. */

	len = sizeof(sunC);
	if (getpeername(fd, (struct sockaddr *)&sunC, &len) != 0) e(0);
	check_addr(&sunC, len, SOCK_PATH_A);

	if (getpeereid(fd, &euid, &egid) != 0) e(0);
	if (euid == -1 || egid == -1) e(0);

	if (getpeereid(fd3, &euid, &egid) != 0) e(0);
	if (euid == -1 || egid == -1) e(0);

	if (send(fd3, "A", 1, 0) != 1) e(0);
	if (send(fd3, "B", 1, 0) != 1) e(0);
	if (send(fd3, "C", 1, 0) != 1) e(0);

	if (close(fd3) != 0) e(0);
	/* Disconnected. */

	if (getpeereid(fd, &euid, &egid) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	if (close(fd2) != 0) e(0);
	fd2 = get_bound_socket(type, SOCK_PATH_B, &sunA);

	if (listen(fd2, 5) != 0) e(0);

	val = 1;
	if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0) e(0);

	if (connect(fd, (struct sockaddr *)&sunA, sizeof(sunA)) != -1) e(0);
	if (errno != EINPROGRESS) e(0);
	/* Connecting. */

	if (getpeereid(fd, &euid, &egid) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'A') e(0);

	len = sizeof(sunB);
	if ((fd3 = accept(fd2, (struct sockaddr *)&sunB, &len)) < 0) e(0);
	/* Connected. */

	if (send(fd3, "D", 1, 0) != 1) e(0);
	if (send(fd3, "E", 1, 0) != 1) e(0);

	len = sizeof(sunC);
	if (getpeername(fd, (struct sockaddr *)&sunC, &len) != 0) e(0);
	check_addr(&sunC, len, SOCK_PATH_B);

	if (getpeereid(fd, &euid, &egid) != 0) e(0);
	if (euid == -1 || egid == -1) e(0);

	if (close(fd2) != 0) e(0);
	if (unlink(SOCK_PATH_B) != 0) e(0);

	if (close(fd3) != 0) e(0);
	/* Disconnected. */

	if (connect(fd, (struct sockaddr *)&sunA, sizeof(sunA)) != -1) e(0);
	if (errno != ENOENT) e(0);
	/* Unconnected. */

	if (getpeereid(fd, &euid, &egid) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'B') e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);

	if (bind(fd, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'C') e(0);

	val = 0;
	if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0) e(0);

	if (listen(fd, 1) != 0) e(0);
	/* Listening. */

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'D') e(0);

	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	len = sizeof(sunC);
	if ((fd3 = accept(fd, (struct sockaddr *)&sunC, &len)) < 0) e(0);

	if (send(fd2, "F", 1, 0) != 1) e(0);

	if (recv(fd3, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'F') e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'E') e(0);

	if (recv(fd, buf, 1, 0) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	/* It should be possible to obtain peer credentials now. */
	if (getpeereid(fd2, &euid, &egid) != 0) e(0);
	if (euid == -1 || egid == -1) e(0);

	if (getpeereid(fd3, &euid, &egid) != 0) e(0);
	if (euid == -1 || egid == -1) e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd2) != 0) e(0);

	if (close(fd) != 0) e(0);
	/* Closed. */

	if (unlink(SOCK_PATH_B) != 0) e(0);

	if ((fd = socket(AF_UNIX, type, 0)) < 0) e(0);
	/* Unconnected. */

	fd2 = get_bound_socket(type, SOCK_PATH_A, &sunA);

	if (listen(fd2, 5) != 0) e(0);

	if (connect(fd, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);
	/* Connected. */

	len = sizeof(sunB);
	if ((fd3 = accept(fd2, (struct sockaddr *)&sunB, &len)) < 0) e(0);

	if (close(fd2) != 0) e(0);

	memset(&sunB, 0, sizeof(sunB));
	sunB.sun_family = AF_UNIX;
	strlcpy(sunB.sun_path, SOCK_PATH_B, sizeof(sunB.sun_path));

	if (bind(fd, (struct sockaddr *)&sunB, sizeof(sunB)) != 0) e(0);

	if (close(fd3) != 0) e(0);
	/* Disconnected. */

	if (listen(fd, 1) != 0) e(0);
	/* Listening. */

	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sunB, sizeof(sunB)) != 0) e(0);

	len = sizeof(sunC);
	if ((fd3 = accept(fd, (struct sockaddr *)&sunC, &len)) < 0) e(0);

	/* It should NOT be possible to obtain peer credentials now. */
	if (getpeereid(fd2, &euid, &egid) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (getpeereid(fd3, &euid, &egid) != 0) e(0);
	if (euid == -1 || egid == -1) e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd2) != 0) e(0);

	if (close(fd) != 0) e(0);
	/* Closed. */

	if (unlink(SOCK_PATH_A) != 0) e(0);
	if (unlink(SOCK_PATH_B) != 0) e(0);
}

/*
 * Test socket reuse, receiving left-overs in the receive buffer, and the
 * (in)ability to obtain peer credentials.
 */
static void
test90p(void)
{

	subtest = 16;

	sub90p(SOCK_STREAM);

	sub90p(SOCK_SEQPACKET);
}

/*
 * Test state changes and errors related to connected datagram sockets.
 */
static void
test90q(void)
{
	struct sockaddr_un sunA, sunB, sunC, sunD;
	socklen_t len;
	char buf[1];
	int fd, fd2, fd3, val;

	subtest = 17;

	/*
	 * Sending a datagram to a datagram socket connected elsewhere should
	 * fail explicitly (specifically, EPERM).
	 */
	fd = get_bound_socket(SOCK_DGRAM, SOCK_PATH_A, &sunA);
	fd2 = get_bound_socket(SOCK_DGRAM, SOCK_PATH_B, &sunB);
	fd3 = get_bound_socket(SOCK_DGRAM, SOCK_PATH_C, &sunC);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (sendto(fd3, "A", 1, 0, (struct sockaddr *)&sunB,
	    sizeof(sunB)) != -1) e(0);
	if (errno != EPERM) e(0);

	/* Similarly, connecting to such a socket should fail. */
	if (connect(fd3, (struct sockaddr *)&sunB, sizeof(sunB)) != -1) e(0);
	if (errno != EPERM) e(0);

	if (send(fd2, "B", 1, 0) != 1) e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'B') e(0);

	/* Reconnection of a socket's target should result in ECONNRESET. */
	if (connect(fd, (struct sockaddr *)&sunC, sizeof(sunC)) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd2, SOL_SOCKET, SO_ERROR, &val, &len) != 0) e(0);
	if (val != ECONNRESET) e(0);

	if (send(fd2, "C", 1, 0) != -1) e(0);
	if (errno != EDESTADDRREQ) e(0);

	if (send(fd, "D", 1, 0) != 1) e(0);

	len = sizeof(sunD);
	if (recvfrom(fd3, buf, 1, 0, (struct sockaddr *)&sunD, &len) != 1)
		e(0);
	if (buf[0] != 'D') e(0);
	check_addr(&sunD, len, SOCK_PATH_A);

	if (connect(fd2, (struct sockaddr *)&sunC, sizeof(sunC)) != 0) e(0);

	if (connect(fd3, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	memset(&sunD, 0, sizeof(sunD));
	sunD.sun_family = AF_UNIX;
	strlcpy(sunD.sun_path, SOCK_PATH_D, sizeof(sunD.sun_path));

	/* A failed reconnection attempt should not break the previous one. */
	if (connect(fd3, (struct sockaddr *)&sunD, sizeof(sunD)) != -1) e(0);
	if (errno != ENOENT) e(0);

	/* The destination address should be ignored here. */
	if (sendto(fd3, "E", 1, 0, (struct sockaddr *)&sunB,
	    sizeof(sunB)) != 1) e(0);

	if (recv(fd2, buf, 1, 0) != -1) e(0);
	if (errno != ECONNRESET) e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'E') e(0);

	if (close(fd3) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len) != 0) e(0);
	if (val != ECONNRESET) e(0);

	if (close(fd2) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len) != 0) e(0);
	if (val != 0) e(0);

	if (close(fd) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
	if (unlink(SOCK_PATH_B) != 0) e(0);
	if (unlink(SOCK_PATH_C) != 0) e(0);

	/*
	 * Finally, test unconnecting sockets.
	 */
	fd = get_bound_socket(SOCK_DGRAM, SOCK_PATH_A, &sunA);
	fd2 = get_bound_socket(SOCK_DGRAM, SOCK_PATH_B, &sunB);

	if (connect(fd, (struct sockaddr *)&sunB, sizeof(sunB)) != 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (sendto(fd2, "F", 1, 0, NULL, 0) != 1) e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'F') e(0);

	memset(&sunC, 0, sizeof(sunC));
	sunC.sun_family = AF_UNSPEC;
	if (connect(fd2, (struct sockaddr *)&sunC, sizeof(sunC)) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len) != 0) e(0);
	if (val != 0) e(0);

	if (send(fd, "G", 1, 0) != 1) e(0);

	if (sendto(fd2, "H", 1, 0, NULL, 0) != -1) e(0);
	if (errno != EDESTADDRREQ) e(0);

	if (sendto(fd2, "I", 1, 0, (struct sockaddr *)&sunA, sizeof(sunA)) != 1)
		e(0);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (sendto(fd2, "J", 1, 0, NULL, 0) != 1) e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'I') e(0);

	if (recv(fd, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'J') e(0);

	if (recv(fd2, buf, 1, 0) != 1) e(0);
	if (buf[0] != 'G') e(0);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
	if (unlink(SOCK_PATH_B) != 0) e(0);
}

/*
 * Test socket file name reuse.
 */
static void
test90r(void)
{
	struct sockaddr_un sun;
	socklen_t len;
	int fd, fd2, fd3, fd4;

	subtest = 18;

	fd = get_bound_socket(SOCK_STREAM | SOCK_NONBLOCK, SOCK_PATH_A, &sun);

	if (rename(SOCK_PATH_A, SOCK_PATH_B) != 0) e(0);

	if ((fd3 = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) e(0);

	if (connect(fd3, (struct sockaddr *)&sun, sizeof(sun)) != -1) e(0);
	if (errno != ENOENT) e(0);

	if (listen(fd, 1) != 0) e(0);

	len = sizeof(sun);
	if (getsockname(fd, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, SOCK_PATH_A);

	fd2 = get_bound_socket(SOCK_STREAM | SOCK_NONBLOCK, SOCK_PATH_A, &sun);

	if (listen(fd2, 1) != 0) e(0);

	len = sizeof(sun);
	if (getsockname(fd2, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, SOCK_PATH_A);

	len = sizeof(sun);
	if (getsockname(fd, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, SOCK_PATH_A);

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SOCK_PATH_B, sizeof(sun.sun_path));
	if (connect(fd3, (struct sockaddr *)&sun, sizeof(sun)) != 0) e(0);

	len = sizeof(sun);
	if ((fd4 = accept(fd2, (struct sockaddr *)&sun, &len)) >= 0) e(0);
	if (errno != EWOULDBLOCK) e(0);
	if ((fd4 = accept(fd, (struct sockaddr *)&sun, &len)) < 0) e(0);

	len = sizeof(sun);
	if (getpeername(fd3, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, SOCK_PATH_A);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);
	if (close(fd4) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
	if (unlink(SOCK_PATH_B) != 0) e(0);
}

/*
 * Test that non-canonized path names are accepted and returned.
 * Also test datagram send errors on disconnect.
 */
static void
test90s(void)
{
	struct sockaddr_un sun;
	socklen_t len;
	int fd, fd2;

	subtest = 19;

	fd = get_bound_socket(SOCK_DGRAM, SOCK_PATH_A_X, &sun);

	if ((fd2 = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SOCK_PATH_A_Y, sizeof(sun.sun_path));

	if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != 0) e(0);

	len = sizeof(sun);
	if (getpeername(fd2, (struct sockaddr *)&sun, &len) != 0) e(0);
	check_addr(&sun, len, SOCK_PATH_A_X);

	if (send(fd2, "A", 1, 0) != 1) e(0);

	if (close(fd) != 0) e(0);

	if (send(fd2, "B", 1, 0) != -1) e(0);
	if (errno != ECONNRESET) e(0);

	if (send(fd2, "B", 1, 0) != -1) e(0);
	if (errno != EDESTADDRREQ) e(0);

	if (close(fd2) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * Test basic sysctl(2) socket enumeration for a specific socket type.
 */
static void
sub90t(int type, const char * path)
{
	struct kinfo_pcb *ki;
	size_t i, len, oldlen;
	int fd, mib[8];

	if ((fd = socket(AF_UNIX, type, 0)) < 0) e(0);

	memset(mib, 0, sizeof(mib));

	len = __arraycount(mib);
	if (sysctlnametomib(path, mib, &len) != 0) e(0);
	if (len != 4) e(0);

	if (sysctl(mib, __arraycount(mib), NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);
	if (oldlen % sizeof(*ki)) e(0);

	if ((ki = (struct kinfo_pcb *)malloc(oldlen)) == NULL) e(0);

	if (sysctl(mib, __arraycount(mib), ki, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);
	if (oldlen % sizeof(*ki)) e(0);

	/*
	 * We cannot check a whole lot of things, because we have no way of
	 * knowing which is the socket we created.  Check some basics and leave
	 * it at that.  This subtest is mostly trying to guarantee that
	 * netstat(1) will not show nothing, anyway.
	 */
	for (i = 0; i < oldlen / sizeof(*ki); i++) {
		if (ki[i].ki_pcbaddr == 0) e(0);
		if (ki[i].ki_sockaddr == 0) e(0);
		if (ki[i].ki_family != AF_UNIX) e(0);
		if (ki[i].ki_type != type) e(0);
		if (ki[i].ki_protocol != 0) e(0);
	}

	free(ki);

	if (close(fd) != 0) e(0);
}

/*
 * Test basic sysctl(2) socket enumeration support.
 */
static void
test90t(void)
{

	subtest = 20;

	/*
	 * We test that for each of the socket types, when we create a socket,
	 * we can find at least one socket of that type in the respective
	 * sysctl(2) out.
	 */
	sub90t(SOCK_STREAM, "net.local.stream.pcblist");

	sub90t(SOCK_SEQPACKET, "net.local.seqpacket.pcblist");

	sub90t(SOCK_DGRAM, "net.local.dgram.pcblist");
}

/*
 * Cause a pending recv() call to return.  Here 'fd' is the file descriptor
 * identifying the other end of the socket pair.  If breaking the recv()
 * requires sending data, 'data' and 'len' identify the data that should be
 * sent.  Return 'fd' if it is still open, or -1 if it is closed.
 */
static int
break_uds_recv(int fd, const char * data, size_t len)
{
	int fd2;

	/*
	 * This UDS-specific routine makes the recv() in one of two ways
	 * depending on whether the recv() call already made partial progress:
	 * if it did, this send call creates a segment boundary which should
	 * cut short the current receive call.  If it did not, the send call
	 * will simply satisfy the receive call with regular data.
	 */
	if ((fd2 = open("/dev/null", O_RDONLY)) < 0) e(0);

	if (send_fds(fd, data, len, 0, NULL, 0, &fd2, 1) != len) e(0);

	if (close(fd2) != 0) e(0);

	return fd;
}

/*
 * Test for receiving on stream sockets.  In particular, test SO_RCVLOWAT,
 * MSG_PEEK, MSG_DONTWAIT, and MSG_WAITALL.
 */
static void
test90u(void)
{

	subtest = 21;

	socklib_stream_recv(socketpair, AF_UNIX, SOCK_STREAM, break_uds_recv);
}

#define MAX_BYTES	2	/* set to 3 for slightly better(?) testing */
#define USLEEP_TIME	250000	/* increase on wimpy platforms if needed */

/*
 * Signal handler which just needs to exist, so that invoking it will interrupt
 * an ongoing system call.
 */
static void
test90_got_signal(int sig __unused)
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
 */
static void
sub90v(int iroom, int istate, int slowat, int len, int bits, int act)
{
	const char *data = "ABC";	/* this limits MAX_BYTES to 3 */
	struct sigaction sa;
	struct timeval tv;
	fd_set fds;
	char buf[2], *sndbuf;
	pid_t pid;
	int fd[2], rcvlen, min, flags, res, err;
	int pfd[2], eroom, tstate, fl, status;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) != 0) e(0);

	/*
	 * Set up the initial condition on the sockets.
	 */
	rcvlen = get_rcvbuf_len(fd[1]);
	if (rcvlen <= iroom) e(0);
	rcvlen -= iroom;

	if ((sndbuf = malloc(rcvlen)) == NULL) e(0);

	memset(sndbuf, 'X', rcvlen);
	if (send(fd[0], sndbuf, rcvlen, 0) != rcvlen) e(0);

	free(sndbuf);

	switch (istate) {
	case 0: break;
	case 1: if (shutdown(fd[0], SHUT_WR) != 0) e(0); break;
	case 2: if (shutdown(fd[1], SHUT_RD) != 0) e(0); break;
	case 3: if (close(fd[1]) != 0) e(0); break;
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
		res = send(fd[0], data, len, flags);

		if (istate > 0) {
			if (res != -1) e(0);
			if (errno != EPIPE) e(0);
		} else if (iroom >= len) {
			if (res != len) e(0);
		} else if (iroom >= min) {
			if (res != iroom) e(0);
		} else {
			if (res != -1) e(0);
			if (errno != EWOULDBLOCK) e(0);
		}

		/* Early cleanup and return to avoid even more code clutter. */
		if (istate != 3 && close(fd[1]) != 0) e(0);
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
		if (send(fd[0], data, len, flags | MSG_DONTWAIT) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);
	}

	/*
	 * If (act < 9), we receive 0, 1, or 2 bytes from the receive queue
	 * before forcing the send call to terminate in one of three ways.
	 *
	 * If (act == 9), we use a signal to interrupt the send call.
	 */
	if (act < 9) {
		eroom = act % 3;
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
			sa.sa_handler = test90_got_signal;
			if (sigaction(SIGUSR1, &sa, NULL) != 0) e(0);
		}

		res = send(fd[0], data, len, flags);
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
			if ((fl = fcntl(pfd[0], F_GETFL)) == -1) e(0);
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
		 * Now test various ways to terminate the send call.  Ideally
		 * we would also like to have a case that raises a socket error
		 * here, but with UDS there is currently no way to do that.
		 */
		switch (tstate) {
		case 0: if (shutdown(fd[0], SHUT_WR) != 0) e(0); break;
		case 1: if (shutdown(fd[1], SHUT_RD) != 0) e(0); break;
		case 2: if (close(fd[1]) != 0) e(0); fd[1] = -1; break;
		}
	} else
		if (kill(pid, SIGUSR1) != 0) e(0);

	if ((fl = fcntl(pfd[0], F_GETFL)) == -1) e(0);
	if (fcntl(pfd[0], F_SETFL, fl & ~O_NONBLOCK) != 0) e(0);

	if (read(pfd[0], &res, sizeof(res)) != sizeof(res)) e(0);
	if (read(pfd[0], &err, sizeof(err)) != sizeof(err)) e(0);

	/*
	 * If the send met the threshold before being terminate or interrupted,
	 * we should at least have sent something.  Otherwise, the send was
	 * never admitted and should return EPIPE (if the send was terminated)
	 * or EINTR (if the child was killed).
	 */
	if (iroom + eroom >= min) {
		if (res != MIN(iroom + eroom, len)) e(0);
	} else {
		if (res != -1) e(0);
		if (act < 9) {
			if (err != EPIPE) e(0);
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
test90v(void)
{
	int iroom, istate, slowat, len, bits, act;

	subtest = 22;

	/* Insanity. */
	for (iroom = 0; iroom <= MAX_BYTES; iroom++)
		for (istate = 0; istate <= 3; istate++)
			for (slowat = 1; slowat <= MAX_BYTES; slowat++)
				for (len = 1; len <= MAX_BYTES; len++)
					for (bits = 0; bits < 2; bits++)
						for (act = 0; act <= 9; act++)
							sub90v(iroom, istate,
							    slowat, len, bits,
							    act);
}

/*
 * Test that SO_RCVLOWAT is limited to the size of the receive buffer.
 */
static void
sub90w_recv(int fill_delta, int rlowat_delta, int exp_delta)
{
	char *buf;
	int fd[2], rcvlen, fill, rlowat, res;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) != 0) e(0);

	rcvlen = get_rcvbuf_len(fd[0]);

	if ((buf = malloc(rcvlen + 1)) == NULL) e(0);

	fill = rcvlen + fill_delta;
	rlowat = rcvlen + rlowat_delta;

	memset(buf, 0, fill);

	if (send(fd[1], buf, fill, 0) != fill) e(0);

	if (setsockopt(fd[0], SOL_SOCKET, SO_RCVLOWAT, &rlowat,
	    sizeof(rlowat)) != 0) e(0);

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
 * Test that SO_SNDLOWAT is limited to the size of the "send" buffer.
 */
static void
sub90w_send(int fill, int slowat_delta, int exp_delta)
{
	char *buf;
	socklen_t len;
	int fd[2], sndlen, slowat, res;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) != 0) e(0);

	len = sizeof(sndlen);
	if (getsockopt(fd[0], SOL_SOCKET, SO_SNDBUF, &sndlen, &len) != 0) e(0);
	if (len != sizeof(sndlen)) e(0);

	if ((buf = malloc(sndlen + 1)) == NULL) e(0);

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
 * their respective buffer sizes.
 */
static void
test90w(void)
{

	subtest = 23;

	/*
	 * With the receive buffer filled except for one byte, all data should
	 * be retrieved unless the threshold is not met.
	 */
	sub90w_recv(-1, -1, 1);
	sub90w_recv(-1, 0, -1);
	sub90w_recv(-1, 1, -1);

	/*
	 * With the receive buffer filled completely, all data should be
	 * retrieved in all cases.
	 */
	sub90w_recv(0, -1, 0);
	sub90w_recv(0, 0, 0);
	sub90w_recv(0, 1, 0);

	/*
	 * With a "send" buffer that contains one byte, all data should be sent
	 * unless the threshold is not met.
	 */
	sub90w_send(1, -1, 1);
	sub90w_send(1, 0, -1);
	sub90w_send(1, 1, -1);

	/*
	 * With the "send" buffer filled completely, all data should be sent
	 * in all cases.
	 */
	sub90w_send(0, -1, 0);
	sub90w_send(0, 0, 0);
	sub90w_send(0, 1, 0);
}

/*
 * Test shutdown on listening sockets.
 */
static void
sub90x(int type, int how, int connwait)
{
	struct sockaddr_un sun;
	socklen_t len;
	char buf[1];
	int fd, fd2, fd3, val, fl;

	subtest = 24;

	fd = get_bound_socket(type, SOCK_PATH_A, &sun);

	if (listen(fd, 5) != 0) e(0);

	if (!connwait) {
		if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

		if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != 0)
			e(0);
	} else {
		val = 1;
		if (setsockopt(fd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0)
			e(0);

		if ((fd2 = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

		if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != -1)
			e(0);
		if (errno != EINPROGRESS) e(0);
	}

	if (shutdown(fd, how) != 0) e(0);

	len = sizeof(sun);
	if ((fd3 = accept(fd, (struct sockaddr *)&sun, &len)) < 0) e(0);

	if (write(fd2, "A", 1) != 1) e(0);
	if (read(fd3, buf, 1) != 1) e(0);
	if (buf[0] != 'A') e(0);

	if (write(fd3, "B", 1) != 1) e(0);
	if (read(fd2, buf, 1) != 1) e(0);
	if (buf[0] != 'B') e(0);

	len = sizeof(sun);
	if (accept(fd, (struct sockaddr *)&sun, &len) != -1) e(0);
	if (errno != ECONNABORTED) e(0);

	/*
	 * Strangely, both NetBSD and Linux (yes, my two reference platforms)
	 * return EWOULDBLOCK from non-blocking accept(2) calls even though
	 * they always return ECONNABORTED when blocking.  For consistency and
	 * select(2), we always return ECONNABORTED.
	 */
	if ((fl = fcntl(fd, F_GETFL)) == -1) e(0);
	if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) e(0);

	len = sizeof(sun);
	if (accept(fd, (struct sockaddr *)&sun, &len) != -1) e(0);
	if (errno != ECONNABORTED) e(0);

	if (fcntl(fd, F_SETFL, fl) != 0) e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd2) != 0) e(0);

	if ((fd2 = socket(AF_UNIX, type | SOCK_NONBLOCK, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != -1) e(0);
	if (errno != ECONNREFUSED) e(0);

	len = sizeof(sun);
	if (accept(fd, (struct sockaddr *)&sun, &len) != -1) e(0);
	if (errno != ECONNABORTED) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * Test shutdown on listening sockets.  Pending connections should still be
 * acceptable (and not inherit the shutdown flags), but new connections must be
 * refused, and the accept call must no longer ever block.
 */
static void
test90x(void)
{
	const int types[] = { SOCK_STREAM, SOCK_SEQPACKET };
	const int hows[] = { SHUT_RD, SHUT_WR, SHUT_RDWR };
	unsigned int i, j, k;

	for (i = 0; i < __arraycount(types); i++)
		for (j = 0; j < __arraycount(hows); j++)
			for (k = 0; k <= 1; k++)
				sub90x(types[i], hows[j], k);
}

/*
 * Test accepting connections without LOCAL_CONNWAIT for the given socket type.
 */
static void
sub90y(int type)
{
	struct sockaddr_un sunA, sunB, sunC;
	socklen_t len;
	struct timeval tv;
	fd_set fds;
	char buf[7];
	uid_t uid;
	gid_t gid;
	int fd, fd2, fd3, fd4, val;

	fd = get_bound_socket(type | SOCK_NONBLOCK, SOCK_PATH_A, &sunA);

	len = sizeof(val);
	if (getsockopt(fd, 0, LOCAL_CONNWAIT, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	if (listen(fd, 5) != 0) e(0);

	/*
	 * Any socket options should be inherited from the listening socket at
	 * connect time, and not be re-inherited at accept time.  It does not
	 * really matter what socket option we set here, as long as it is
	 * supposed to be inherited.
	 */
	val = 123;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &val, sizeof(val)) != 0)
		e(0);

	fd2 = get_bound_socket(type, SOCK_PATH_B, &sunB);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	val = 456;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &val, sizeof(val)) != 0)
		e(0);

	/*
	 * Obtaining the peer name should work.  As always, the name should be
	 * inherited from the listening socket.
	 */
	len = sizeof(sunC);
	if (getpeername(fd2, (struct sockaddr *)&sunC, &len) != 0) e(0);
	check_addr(&sunC, len, SOCK_PATH_A);

	/*
	 * Obtaining peer credentials should work.  This is why NetBSD obtains
	 * the peer credentials at bind time, not at accept time.
	 */
	if (getpeereid(fd2, &uid, &gid) != 0) e(0);
	if (uid != geteuid()) e(0);
	if (gid != getegid()) e(0);

	/*
	 * Sending to the socket should work, and it should be possible to
	 * receive the data from the other side once accepted.
	 */
	if (send(fd2, "Hello, ", 7, 0) != 7) e(0);
	if (send(fd2, "world!", 6, 0) != 6) e(0);

	/* Shutdown settings should be visible after accepting, too. */
	if (shutdown(fd2, SHUT_RDWR) != 0) e(0);

	len = sizeof(sunB);
	if ((fd3 = accept(fd, (struct sockaddr *)&sunB, &len)) < 0) e(0);
	check_addr(&sunB, len, SOCK_PATH_B);

	len = sizeof(val);
	if (getsockopt(fd3, SOL_SOCKET, SO_SNDLOWAT, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 123) e(0);

	if (recv(fd3, buf, 7, 0) != 7) e(0);
	if (memcmp(buf, "Hello, ", 7) != 0) e(0);
	if (recv(fd3, buf, 7, 0) != 6) e(0);
	if (memcmp(buf, "world!", 6) != 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 0) e(0);

	if (send(fd3, "X", 1, MSG_NOSIGNAL) != -1) e(0);
	if (errno != EPIPE) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd3) != 0) e(0);

	if (unlink(SOCK_PATH_B) != 0) e(0);

	/*
	 * If the socket pending acceptance is closed, the listening socket
	 * should pretend as though the connection was never there.
	 */
	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if (close(fd2) != 0) e(0);

	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);
	if (FD_ISSET(fd, &fds)) e(0);

	len = sizeof(sunB);
	if (accept(fd, (struct sockaddr *)&sunB, &len) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	/*
	 * Try the same thing, but now with the connection sandwiched between
	 * two different pending connections, which should be left intact.
	 */
	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (send(fd2, "A", 1, 0) != 1) e(0);

	if ((fd3 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd3, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (send(fd3, "B", 1, 0) != 1) e(0);

	if ((fd4 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd4, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (send(fd4, "C", 1, 0) != 1) e(0);

	if (close(fd3) != 0) e(0);

	len = sizeof(sunB);
	if ((fd3 = accept(fd, (struct sockaddr *)&sunB, &len)) < 0) e(0);

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

	len = sizeof(sunB);
	if ((fd3 = accept(fd, (struct sockaddr *)&sunB, &len)) < 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'C') e(0);

	if (close(fd3) != 0) e(0);
	if (close(fd4) != 0) e(0);

	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);
	if (FD_ISSET(fd, &fds)) e(0);

	len = sizeof(sunB);
	if (accept(fd, (struct sockaddr *)&sunB, &len) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	/*
	 * If the listening socket is closed, the socket pending acceptance
	 * should be reset.  We actually rely on this behavior in the sweep
	 * test, but we test this with more than one socket this time.
	 */
	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if ((fd3 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd3, (struct sockaddr *)&sunA, sizeof(sunA)) != 0) e(0);

	if (close(fd) != 0) e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != -1) e(0);
	if (errno != ECONNRESET) e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != -1) e(0);
	if (errno != ECONNRESET) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 0) e(0);

	if (close(fd3) != 0) e(0);

	if (close(fd2) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * Test accepting connections without LOCAL_CONNWAIT.  Since both the old UDS
 * service and the initial version of the new UDS service supported only the
 * LOCAL_CONNWAIT behavior, the alternative (which is now the default, as it is
 * on other platforms) has been a bit under-tested so far.
 */
static void
test90y(void)
{

	subtest = 25;

	sub90y(SOCK_STREAM);

	sub90y(SOCK_SEQPACKET);
}

/*
 * Test that SO_LINGER has no effect on sockets of the given type.
 */
static void
sub90z(int type)
{
	struct sockaddr_un sun;
	socklen_t len;
	struct linger l;
	char buf[1];
	int fd, fd2, fd3;

	fd = get_bound_socket(type, SOCK_PATH_A, &sun);

	if (listen(fd, 1) != 0) e(0);

	if ((fd2 = socket(AF_UNIX, type, 0)) < 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sun, sizeof(sun)) != 0) e(0);

	len = sizeof(sun);
	if ((fd3 = accept(fd, (struct sockaddr *)&sun, &len)) < 0) e(0);

	if (close(fd) != 0) e(0);

	if (send(fd2, "A", 1, 0) != 1) e(0);

	l.l_onoff = 1;
	l.l_linger = 0;
	if (setsockopt(fd2, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) != 0) e(0);

	if (close(fd2) != 0) e(0);

	if (recv(fd3, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'A') e(0);

	/* We should not get ECONNRESET now. */
	if (recv(fd3, buf, sizeof(buf), 0) != 0) e(0);

	if (close(fd3) != 0) e(0);

	if (unlink(SOCK_PATH_A) != 0) e(0);
}

/*
 * Test that SO_LINGER has no effect on UNIX domain sockets.  In particular, a
 * timeout of zero does not cause the connection to be reset forcefully.
 */
static void
test90z(void)
{

	subtest = 26;

	sub90z(SOCK_STREAM);

	sub90z(SOCK_SEQPACKET);
}

/*
 * Test program for UDS.
 */
int
main(int argc, char ** argv)
{
	int i, m;

	start(90);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFFFFFFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x0000001) test90a();
		if (m & 0x0000002) test90b();
		if (m & 0x0000004) test90c();
		if (m & 0x0000008) test90d();
		if (m & 0x0000010) test90e();
		if (m & 0x0000020) test90f();
		if (m & 0x0000040) test90g();
		if (m & 0x0000080) test90h();
		if (m & 0x0000100) test90i();
		if (m & 0x0000200) test90j();
		if (m & 0x0000400) test90k();
		if (m & 0x0000800) test90l();
		if (m & 0x0001000) test90m();
		if (m & 0x0002000) test90n();
		if (m & 0x0004000) test90o();
		if (m & 0x0008000) test90p();
		if (m & 0x0010000) test90q();
		if (m & 0x0020000) test90r();
		if (m & 0x0040000) test90s();
		if (m & 0x0080000) test90t();
		if (m & 0x0100000) test90u();
		if (m & 0x0200000) test90v();
		if (m & 0x0400000) test90w();
		if (m & 0x0800000) test90x();
		if (m & 0x1000000) test90y();
		if (m & 0x2000000) test90z();
	}

	quit();
	/* NOTREACHED */
}
