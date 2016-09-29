/*
 * test81: use the functions originally written for test56 to test UDP
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
		.expected_rcvbuf           = 32768,
		.expected_sndbuf           = 8192,
		.serveraddr                = (struct sockaddr *) &serveraddr,
		.serveraddrlen             = sizeof(serveraddr),
		.serveraddr2               = (struct sockaddr *) &serveraddr2,
		.serveraddr2len            = sizeof(serveraddr2),
		.type                      = SOCK_DGRAM,
		.types                     = &info.type,
		.typecount                 = 1,
		.callback_check_sockaddr   = callback_check_sockaddr,
		.callback_cleanup          = callback_cleanup,
		.callback_xfer_prepclient  = NULL,
		.callback_xfer_peercred    = NULL,
	};

	debug("entering main()");

	start(81);

	test_socket(&info);
	test_bind(&info);
	test_getsockname(&info);
	test_shutdown(&info);
	test_close(&info);
	test_dup(&info);
	test_dup2(&info);
	test_shutdown(&info);
	test_read(&info);
	test_write(&info);
	test_sockopts(&info);
	test_simple_client_server(&info, info.type);

	quit();

	return -1;	/* we should never get here */
}
