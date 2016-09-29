/*
 * test80: use the functions originally written for test56 to test TCP
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "common.h"
#include "common-socket.h"

#define PORT1	4321
#define PORT2	4322

static void callback_check_sockaddr(const struct sockaddr *sockaddr,
	socklen_t sockaddrlen, const char *callname, int addridx) {
	char buf[256];
	int port;
	const struct sockaddr_in *sockaddr_in =
		(const struct sockaddr_in *) sockaddr;

	switch (addridx) {
	case 1: port = PORT1; break;
	case 2: port = PORT2; break;
	default:
		fprintf(stderr, "error: invalid addridx %d in "
			"callback_check_sockaddr\n", addridx);
		abort();
	}

	if (sockaddr_in->sin_family != AF_INET ||
		sockaddr_in->sin_port != htons(port)) {
		snprintf(buf, sizeof(buf), "%s() didn't return the right addr",
			callname);
		test_fail(buf);

		memset(buf, 0, sizeof(buf));
		inet_ntop(sockaddr_in->sin_family, &sockaddr_in->sin_addr,
			buf, sizeof(buf));
		fprintf(stderr, "exp: localhost:%d | got: %s:%d\n", port, buf,
			ntohs(sockaddr_in->sin_port));
	}
}

static void callback_cleanup(void) {
	/* nothing to do */
}

int main(int argc, char *argv[])
{
	struct sockaddr_in clientaddr = {
		.sin_family = AF_INET,
		.sin_port = htons(PORT1),
		.sin_addr = { .s_addr = htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in clientaddr2 = {
		.sin_family = AF_INET,
		.sin_port = htons(PORT2),
		.sin_addr = { .s_addr = htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in serveraddr = {
		.sin_family = AF_INET,
		.sin_port = htons(PORT1),
		.sin_addr = { .s_addr = htonl(INADDR_ANY) },
	};
	struct sockaddr_in serveraddr2 = {
		.sin_family = AF_INET,
		.sin_port = htons(PORT2),
		.sin_addr = { .s_addr = htonl(INADDR_ANY) },
	};
	const struct socket_test_info info = {
		.clientaddr                = (struct sockaddr *) &clientaddr,
		.clientaddrlen             = sizeof(clientaddr),
		.clientaddr2               = (struct sockaddr *) &clientaddr2,
		.clientaddr2len            = sizeof(clientaddr2),
		.clientaddrsym             = (struct sockaddr *) &clientaddr,
		.clientaddrsymlen          = sizeof(clientaddr),
		.domain                    = PF_INET,
		.expected_rcvbuf           = -1,
		.expected_sndbuf           = -1,
		.serveraddr                = (struct sockaddr *) &serveraddr,
		.serveraddrlen             = sizeof(serveraddr),
		.serveraddr2               = (struct sockaddr *) &serveraddr2,
		.serveraddr2len            = sizeof(serveraddr2),
		.type                      = SOCK_STREAM,
		.types                     = &info.type,
		.typecount                 = 1,

		/*
		 * Maintainer's note: common-socket was adapted from test56 in
		 * a time that UDS's LOCAL_CONNWAIT was the default.  Due to
		 * this as well as inherent behavioral differences between TCP
		 * and UDS, these exceptions basically work around the fact
		 * that common-socket was not designed for its current task.
		 */
		.ignore_accept_delay       = 1,
		.ignore_connect_unaccepted = 1,
		.ignore_connect_delay      = 1,
		.ignore_select_delay       = 1,
		.ignore_send_waiting       = 1,
		.ignore_write_conn_reset   = 1,

		.callback_check_sockaddr   = callback_check_sockaddr,
		.callback_cleanup          = callback_cleanup,
		.callback_xfer_prepclient  = NULL,
		.callback_xfer_peercred    = NULL,
	};

	debug("entering main()");

	start(80);

	test_socket(&info);
	test_bind(&info);
	test_listen(&info);
	test_getsockname(&info);
	test_shutdown(&info);
	test_close(&info);
	test_dup(&info);
	test_dup2(&info);
	test_shutdown(&info);
	test_read(&info);
	test_write(&info);
	test_sockopts(&info);
	test_xfer(&info);
	test_simple_client_server(&info, info.type);
	test_abort_client_server(&info, 1);
	test_abort_client_server(&info, 2);
	test_nonblock(&info);
	test_connect_nb(&info);
	test_intr(&info);
	test_connect_close(&info);
	/* test_listen_close(&info); -- not suitable for TCP */
	test_listen_close_nb(&info);

	quit();

	return -1;	/* we should never get here */
}
