#ifndef MINIX_TEST_SOCKLIB_H
#define MINIX_TEST_SOCKLIB_H

/* TCP/IP test values. */
#define TEST_PORT_A	12345	/* this port should be free and usable */
#define TEST_PORT_B	12346	/* this port should be free and usable */

#define LOOPBACK_IFNAME		"lo0"		/* loopback interface name */
#define LOOPBACK_IPV4		"127.0.0.1"	/* IPv4 address */
#define LOOPBACK_LL_IPV6	"fe80::1"	/* link-local IPv6 address */

/* These address should simply eat all packets. */
/*
 * IMPORTANT: the ::2 address works only if there is a route for ::/64.  This
 * route is supposed to be added by /etc/rc.d/network, and is not present by
 * default.  As a result, the tests will pass only when regular system/network
 * initialization is not skipped.  We cannot add the route ourselves, since not
 * all tests run as root.
 */
#define TEST_BLACKHOLE_IPV4	"127.255.0.254"
#define TEST_BLACKHOLE_IPV6	"::2"
#define TEST_BLACKHOLE_LL_IPV6	"fe80::ffff"

#define BAD_SCOPE_ID	255	/* guaranteed not to belong to an interface */

enum state {
	S_NEW,
	S_N_SHUT_R,
	S_N_SHUT_W,
	S_N_SHUT_RW,
	S_BOUND,
	S_LISTENING,
	S_L_SHUT_R,
	S_L_SHUT_W,
	S_L_SHUT_RW,
	S_CONNECTING,
	S_C_SHUT_R,
	S_C_SHUT_W,
	S_C_SHUT_RW,
	S_CONNECTED,
	S_ACCEPTED,
	S_SHUT_R,
	S_SHUT_W,
	S_SHUT_RW,
	S_RSHUT_R,
	S_RSHUT_W,
	S_RSHUT_RW,
	S_SHUT2_R,
	S_SHUT2_W,
	S_SHUT2_RW,
	S_PRE_EOF,
	S_AT_EOF,
	S_POST_EOF,
	S_PRE_SHUT_R,
	S_EOF_SHUT_R,
	S_POST_SHUT_R,
	S_PRE_SHUT_W,
	S_EOF_SHUT_W,
	S_POST_SHUT_W,
	S_PRE_SHUT_RW,
	S_EOF_SHUT_RW,
	S_POST_SHUT_RW,
	S_PRE_RESET,
	S_AT_RESET,
	S_POST_RESET,
	S_FAILED,
	S_POST_FAILED,
	S_MAX
};

enum call {
	C_ACCEPT,
	C_BIND,
	C_CONNECT,
	C_GETPEERNAME,
	C_GETSOCKNAME,
	C_GETSOCKOPT_ERR,
	C_GETSOCKOPT_KA,
	C_GETSOCKOPT_RB,
	C_IOCTL_NREAD,
	C_LISTEN,
	C_RECV,
	C_RECVFROM,
	C_SEND,
	C_SENDTO,
	C_SELECT_R,
	C_SELECT_W,
	C_SELECT_X,
	C_SETSOCKOPT_BC,
	C_SETSOCKOPT_KA,
	C_SETSOCKOPT_L,
	C_SETSOCKOPT_RA,
	C_SHUTDOWN_R,
	C_SHUTDOWN_RW,
	C_SHUTDOWN_W,
	C_MAX
};

int socklib_sweep_call(enum call call, int fd, struct sockaddr * local_addr,
	struct sockaddr * remote_addr, socklen_t addr_len);
void socklib_sweep(int domain, int type, int protocol,
	const enum state * states, unsigned int nstates, const int * results,
	int (* proc)(int domain, int type, int protocol, enum state,
	enum call));

void socklib_multicast_tx_options(int type);
void socklib_large_transfers(int fd[2]);
void socklib_producer_consumer(int fd[2]);
void socklib_stream_recv(int (* socket_pair)(int, int, int, int *), int domain,
	int type, int (* break_recv)(int, const char *, size_t));
int socklib_find_pcb(const char * path, int protocol, uint16_t local_port,
	uint16_t remote_port, struct kinfo_pcb * ki);
void socklib_test_addrs(int type, int protocol);
void socklib_test_multicast(int type, int protocol);

#endif /* !MINIX_TEST_SOCKLIB_H */
