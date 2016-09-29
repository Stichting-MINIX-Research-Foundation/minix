/*
 * Socket test code library.  This file contains code that is worth sharing
 * between TCP/IP and UDS tests, as well as code that is worth sharing between
 * various TCP/IP tests.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet6/in6_var.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "socklib.h"

#define TEST_PORT_A	12345	/* this port should be free and usable */
#define TEST_PORT_B	12346	/* this port should be free and usable */

#define LOOPBACK_IFNAME		"lo0"		/* loopback interface name */
#define LOOPBACK_IPV4		"127.0.0.1"	/* IPv4 address */
#define LOOPBACK_IPV6_LL	"fe80::1"	/* link-local IPv6 address */

/* These address should simply eat all packets. */
#define TEST_BLACKHOLE_IPV4	"127.255.0.254"
#define TEST_BLACKHOLE_IPV6	"::2"
#define TEST_BLACKHOLE_IPV6_LL	"fe80::ffff"

/* Addresses for multicast-related testing. */
#define TEST_MULTICAST_IPV4	"233.252.0.1"		/* RFC 5771 Sec. 9.2 */
#define TEST_MULTICAST_IPV6	"ff0e::db8:0:1"		/* RFC 6676 Sec. 3 */
#define TEST_MULTICAST_IPV6_LL	"ff02::db8:0:1"
#define TEST_MULTICAST_IPV6_BAD	"ff00::db8:0:1"

#define BAD_IFINDEX	255	/* guaranteed not to belong to an interface */

/* 0 = check, 1 = generate source, 2 = generate CSV */
#define SOCKLIB_SWEEP_GENERATE 0

#if SOCKLIB_SWEEP_GENERATE
/* Link against minix/usr.bin/trace/error.o to make this work! */
const char *get_error_name(int err);

#if SOCKLIB_SWEEP_GENERATE == 2
static const char *statename[S_MAX] = {
	"S_NEW",
	"S_N_SHUT_R",
	"S_N_SHUT_W",
	"S_N_SHUT_RW",
	"S_BOUND",
	"S_LISTENING",
	"S_L_SHUT_R",
	"S_L_SHUT_W",
	"S_L_SHUT_RW",
	"S_CONNECTING",
	"S_C_SHUT_R",
	"S_C_SHUT_W",
	"S_C_SHUT_RW",
	"S_CONNECTED",
	"S_ACCEPTED",
	"S_SHUT_R",
	"S_SHUT_W",
	"S_SHUT_RW",
	"S_RSHUT_R",
	"S_RSHUT_W",
	"S_RSHUT_RW",
	"S_SHUT2_R",
	"S_SHUT2_W",
	"S_SHUT2_RW",
	"S_PRE_EOF",
	"S_AT_EOF",
	"S_POST_EOF",
	"S_PRE_SHUT_R",
	"S_EOF_SHUT_R",
	"S_POST_SHUT_R",
	"S_PRE_SHUT_W",
	"S_EOF_SHUT_W",
	"S_POST_SHUT_W",
	"S_PRE_SHUT_RW",
	"S_EOF_SHUT_RW",
	"S_POST_SHUT_RW",
	"S_PRE_RESET",
	"S_AT_RESET",
	"S_POST_RESET",
	"S_FAILED",
	"S_POST_FAILED",
};
#endif

static const char *callname[C_MAX] = {
	"C_ACCEPT",
	"C_BIND",
	"C_CONNECT",
	"C_GETPEERNAME",
	"C_GETSOCKNAME",
	"C_GETSOCKOPT_ERR",
	"C_GETSOCKOPT_KA",
	"C_GETSOCKOPT_RB",
	"C_IOCTL_NREAD",
	"C_LISTEN",
	"C_RECV",
	"C_RECVFROM",
	"C_SEND",
	"C_SENDTO",
	"C_SELECT_R",
	"C_SELECT_W",
	"C_SELECT_X",
	"C_SETSOCKOPT_BC",
	"C_SETSOCKOPT_KA",
	"C_SETSOCKOPT_L",
	"C_SETSOCKOPT_RA",
	"C_SHUTDOWN_R",
	"C_SHUTDOWN_RW",
	"C_SHUTDOWN_W",
};
#endif

static int socklib_sigpipe;

/*
 * Signal handler for SIGPIPE signals.
 */
static void
socklib_signal(int sig)
{

	if (sig != SIGPIPE) e(0);

	socklib_sigpipe++;
}

/*
 * The given socket file descriptor 'fd' has been set up in the desired state.
 * Perform the given call 'call' on it, possibly using local socket address
 * 'local_addr' (for binding) or remote socket address 'remote_addr' (for
 * connecting or to store resulting addresses), both of size 'addr_len'.
 * Return the result of the call, using a positive value if the call succeeded,
 * or a negated errno code if the call failed.
 */
int
socklib_sweep_call(enum call call, int fd, struct sockaddr * local_addr,
	struct sockaddr * remote_addr, socklen_t addr_len)
{
	char data[1];
	struct linger l;
	fd_set fd_set;
	struct timeval tv;
	socklen_t len;
	int i, r, fd2;

	fd2 = -1;

	switch (call) {
	case C_ACCEPT:
		r = accept(fd, remote_addr, &addr_len);

		if (r >= 0)
			fd2 = r;

		break;

	case C_BIND:
		r = bind(fd, local_addr, addr_len);

		break;

	case C_CONNECT:
		r = connect(fd, remote_addr, addr_len);

		break;

	case C_GETPEERNAME:
		r = getpeername(fd, remote_addr, &addr_len);

		break;

	case C_GETSOCKNAME:
		r = getsockname(fd, remote_addr, &addr_len);

		break;

	case C_GETSOCKOPT_ERR:
		len = sizeof(i);

		r = getsockopt(fd, SOL_SOCKET, SO_ERROR, &i, &len);

		/*
		 * We assume this call always succeeds, and test against the
		 * pending error.
		 */
		if (r != 0) e(0);
		if (i != 0) {
			r = -1;
			errno = i;
		}

		break;

	case C_GETSOCKOPT_KA:
		len = sizeof(i);

		r = getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &i, &len);

		break;

	case C_GETSOCKOPT_RB:
		len = sizeof(i);

		r = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &i, &len);

		break;

	case C_IOCTL_NREAD:
		r = ioctl(fd, FIONREAD, &i);

		/* On success, we test against the returned value here. */
		if (r == 0)
			r = i;

		break;

	case C_LISTEN:
		r = listen(fd, 1);

		break;

	case C_RECV:
		r = recv(fd, data, sizeof(data), 0);

		break;

	case C_RECVFROM:
		r = recvfrom(fd, data, sizeof(data), 0, remote_addr,
		    &addr_len);

		break;

	case C_SEND:
		data[0] = 0;

		r = send(fd, data, sizeof(data), 0);

		break;

	case C_SENDTO:
		data[0] = 0;

		r = sendto(fd, data, sizeof(data), 0, remote_addr, addr_len);

		break;

	case C_SETSOCKOPT_BC:
		i = 0;

		r = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i));

		break;

	case C_SETSOCKOPT_KA:
		i = 1;

		r = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));

		break;

	case C_SETSOCKOPT_L:
		l.l_onoff = 1;
		l.l_linger = 0;

		r = setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));

		break;

	case C_SETSOCKOPT_RA:
		i = 1;

		r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

		break;

	case C_SELECT_R:
	case C_SELECT_W:
	case C_SELECT_X:
		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);

		tv.tv_sec = 0;
		tv.tv_usec = 0;

		r = select(fd + 1, (call == C_SELECT_R) ? &fd_set : NULL,
		    (call == C_SELECT_W) ? &fd_set : NULL,
		    (call == C_SELECT_X) ? &fd_set : NULL, &tv);

		break;

	case C_SHUTDOWN_R:
		r = shutdown(fd, SHUT_RD);

		break;

	case C_SHUTDOWN_W:
		r = shutdown(fd, SHUT_WR);

		break;

	case C_SHUTDOWN_RW:
		r = shutdown(fd, SHUT_RDWR);

		break;

	default:
		r = -1;
		errno = EINVAL;
		e(0);
	}

	if (r < -1) e(0);

	if (r == -1)
		r = -errno;

	if (fd2 >= 0 && close(fd2) != 0) e(0);

	return r;
}

/*
 * Perform a sweep of socket calls vs socket states, testing the outcomes
 * against provided tables or (if SOCKLIB_SWEEP_GENERATE is set) reporting on
 * the outcomes instead.  The caller must provide the following:
 *
 * - the socket domain, type, and protocol to test; these are simply forwarded
 *   to the callback function (see below);
 * - the set of S_ states to test, as array 'states' with 'nstates' elements;
 * - unless generating output, a matrix of expected results as 'results', which
 *   is actually a two-dimensional array with dimensions [C_MAX][nstates], with
 *   either positive call output or a negated call errno code in each cell;
 * - a callback function 'proc' that must set up a socket in the given state
 *   and pass it to socklib_sweep_call().
 *
 * The 'states' array allows each socket sweep test to support a different set
 * of states, because not every type of socket can be put in every possible
 * state.  All calls are always tried in each state, though.
 *
 * The sweep also tests for SIGPIPE generation, which assumes that all calls on
 * SOCK_STREAM sockets that return EPIPE, also raise a SIGPIPE signal, and that
 * no other SIGPIPE signal is ever raised otherwise.
 *
 * Standard e() error throwing is used for set-up and result mismatches.
 */
void
socklib_sweep(int domain, int type, int protocol, const enum state * states,
	unsigned int nstates, const int * results, int (* proc)(int domain,
	int type, int protocol, enum state, enum call))
{
	struct sigaction act, oact;
	enum state state;
	enum call call;
#if SOCKLIB_SWEEP_GENERATE
	const char *name;
	int res, *nresults;
#else
	int res, exp;
#endif

	memset(&act, 0, sizeof(act));
	act.sa_handler = socklib_signal;
	if (sigaction(SIGPIPE, &act, &oact) != 0) e(0);

#if SOCKLIB_SWEEP_GENERATE
	if ((nresults = malloc(nstates * C_MAX)) == NULL) e(0);
#endif

	for (state = 0; state < nstates; state++) {
		for (call = 0; call < C_MAX; call++) {
			socklib_sigpipe = 0;

			res = proc(domain, type, protocol, states[state],
			    call);

			/*
			 * If the result was EPIPE and this is a stream-type
			 * socket, we must have received exactly one SIGPIPE
			 * signal.  Otherwise, we must not have received one.
			 * Note that technically, the SIGPIPE could arrive
			 * sometime after this check, but with regular system
			 * service scheduling that will never happen.
			 */
			if (socklib_sigpipe !=
			    (res == -EPIPE && type == SOCK_STREAM)) e(0);

#if SOCKLIB_SWEEP_GENERATE
			nresults[call * nstates + state] = res;
#else
			exp = results[call * nstates + state];

			if (res != exp) {
				printf("FAIL state %d call %d res %d exp %d\n",
				    state, call, res, exp);
				e(0);
			}
#endif
		}
	}

	if (sigaction(SIGPIPE, &oact, NULL) != 0) e(0);

#if SOCKLIB_SWEEP_GENERATE
#if SOCKLIB_SWEEP_GENERATE == 1
	/*
	 * Generate a table in C form, ready to be pasted into test source.
	 * Obviously, generated results should be hand-checked carefully before
	 * being pasted into a test.  Arguably these tables should be hand-made
	 * for maximum scrutiny, but I already checked the results from the
	 * CSV form (#define SOCKLIB_SWEEP_GENERATE 2) and have no desire for
	 * RSI -dcvmoole
	 */
	printf("\nstatic const int X_results[][__arraycount(X_states)] = {\n");
	for (call = 0; call < C_MAX; call++) {
		if ((name = callname[call]) == NULL) e(0);
		printf("\t[%s]%s%s%s= {", name,
		    (strlen(name) <= 21) ? "\t" : "",
		    (strlen(name) <= 13) ? "\t" : "",
		    (strlen(name) <= 5) ? "\t" : "");
		for (state = 0; state < nstates; state++) {
			if (state % 4 == 0)
				printf("\n\t\t");
			res = nresults[call * nstates + state];
			name = (res < 0) ? get_error_name(-res) : NULL;
			if (name != NULL) {
				printf("-%s,", name);
				if ((state + 1) % 4 != 0 &&
				    state < nstates - 1)
					printf("%s%s",
					    (strlen(name) <= 13) ? "\t" : "",
					    (strlen(name) <= 5) ? "\t" : "");
			} else {
				printf("%d,", res);
				if ((state + 1) % 4 != 0 &&
				    state < nstates - 1)
					printf("\t\t");
			}
		}
		printf("\n\t},\n");
	}
	printf("};\n");
#elif SOCKLIB_SWEEP_GENERATE == 2
	/* Generate table in CSV form. */
	printf("\n");
	for (state = 0; state < nstates; state++)
		printf(",%s", statename[states[state]] + 2);
	for (call = 0; call < C_MAX; call++) {
		printf("\n%s", callname[call] + 2);
		for (state = 0; state < nstates; state++) {
			res = nresults[call * nstates + state];
			name = (res < 0) ? get_error_name(-res) : NULL;
			if (name != NULL)
				printf(",%s", name);
			else
				printf(",%d", res);
		}
	}
	printf("\n");
#endif

	free(nresults);
#endif
}

/*
 * Test for setting and retrieving UDP/RAW multicast transmission options.
 * This is an interface-level test only: we do not (yet) test whether the
 * options have any effect.  The given 'type' must be SOCK_DGRAM or SOCK_RAW.
 */
void
socklib_multicast_tx_options(int type)
{
	struct in_addr in_addr;
	socklen_t len;
	unsigned int ifindex;
	uint8_t byte;
	int fd, val;

	subtest = 10;

	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	/*
	 * Initially, the multicast TTL is expected be 1, looping should be
	 * enabled, and the multicast source address should be <any>.
	 */
	byte = 0;
	len = sizeof(byte);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &byte, &len) != 0)
		e(0);
	if (len != sizeof(byte)) e(0);
	if (type != SOCK_STREAM && byte != 1) e(0);

	byte = 0;
	len = sizeof(byte);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &byte, &len) != 0)
		e(0);
	if (len != sizeof(byte)) e(0);
	if (byte != 1) e(0);

	len = sizeof(in_addr);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &in_addr, &len) != 0)
		e(0);
	if (len != sizeof(in_addr)) e(0);
	if (in_addr.s_addr != htonl(INADDR_ANY)) e(0);

	/* It must not be possible to get/set IPv6 options on IPv4 sockets. */
	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val) /*wrong but it doesn't matter*/) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val,
	    &len) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val,
	    &len) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val, &len) != -1)
		e(0);
	if (errno != ENOPROTOOPT) e(0);

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

	/*
	 * Expect the same defaults as for IPv4.  IPV6_MULTICAST_IF uses an
	 * interface index rather than an IP address, though.
	 */
	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (type != SOCK_STREAM && val != 1) e(0);

	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 1) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	/* It must not be possible to get/set IPv4 options on IPv6 sockets. */
	byte = 0;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &byte,
	    sizeof(byte)) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &byte,
	    sizeof(byte)) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &byte,
	    sizeof(byte) /* wrong but it doesn't matter */) != -1) e(0);
	if (errno != ENOPROTOOPT) e(0);

	len = sizeof(byte);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &val, &len) != -1)
		e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, &len) != -1)
		e(0);
	if (errno != ENOPROTOOPT) e(0);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &val, &len) != -1)
		e(0);
	if (errno != ENOPROTOOPT) e(0);

	if (close(fd) != 0) e(0);

	/* Test changing options. */
	if ((fd = socket(AF_INET, type, 0)) < 0) e(0);

	byte = 129;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &byte,
	    sizeof(byte)) != 0) e(0);

	byte = 0;
	len = sizeof(byte);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &byte, &len) != 0)
		e(0);
	if (len != sizeof(byte)) e(0);
	if (byte != 129) e(0);

	byte = 0;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &byte,
	    sizeof(byte)) != 0)
		e(0);

	byte = 1;
	len = sizeof(byte);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &byte, &len) != 0)
		e(0);
	if (len != sizeof(byte)) e(0);
	if (byte != 0) e(0);

	in_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &in_addr,
	   sizeof(in_addr)) != 0)
		e(0);

	in_addr.s_addr = htonl(INADDR_ANY);
	len = sizeof(in_addr);
	if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &in_addr, &len) != 0)
		e(0);
	if (len != sizeof(in_addr)) e(0);
	if (in_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET6, type, 0)) < 0) e(0);

	val = 137;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val,
	    sizeof(val)) != 0) e(0);

	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 137) e(0);

	val = -2;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	val = 256;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 137) e(0);

	val = -1; /* use default */
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val,
	    sizeof(val)) != 0) e(0);

	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 1) e(0);

	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val,
	    sizeof(val)) != 0) e(0);

	val = 1;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val,
	    sizeof(val)) != 0) e(0);

	val = -1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	val = 2;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 1) e(0);

	val = -1;
	ifindex = if_nametoindex(LOOPBACK_IFNAME);

	val = ifindex;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != ifindex) e(0);

	val = BAD_IFINDEX;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != -1) e(0);

	val = -1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != -1) e(0);

	val = 0;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != ifindex) e(0);

	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	val = ifindex;
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val, &len) != 0)
		e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test for large sends and receives on stream sockets with MSG_WAITALL.
 */
void
socklib_large_transfers(int fd[2])
{
	char *buf;
	pid_t pid;
	int i, status;

#define LARGE_BUF	(4096*1024)

	if ((buf = malloc(LARGE_BUF)) == NULL) e(0);
	memset(buf, 0, LARGE_BUF);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[0]) != 0) e(0);

		/* Part 1. */
		if (recv(fd[1], buf, LARGE_BUF, MSG_WAITALL) != LARGE_BUF)
			e(0);

		for (i = 0; i < LARGE_BUF; i++)
			if (buf[i] != (char)(i + (i >> 16))) e(0);

		if (recv(fd[1], buf, LARGE_BUF,
		    MSG_DONTWAIT | MSG_WAITALL) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);

		/* Part 2. */
		if (send(fd[1], buf, LARGE_BUF / 2, 0) != LARGE_BUF / 2) e(0);

		if (shutdown(fd[1], SHUT_WR) != 0) e(0);

		/* Part 3. */
		memset(buf, 'y', LARGE_BUF);

		if (recv(fd[1], buf, LARGE_BUF, MSG_WAITALL) != LARGE_BUF - 1)
			e(0);

		for (i = 0; i < LARGE_BUF - 1; i++)
			if (buf[i] != (char)(i + (i >> 16))) e(0);
		if (buf[LARGE_BUF - 1] != 'y') e(0);

		if (recv(fd[1], buf, LARGE_BUF, MSG_WAITALL) != 0) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd[1]) != 0) e(0);

	/* Part 1: check that a large send fully arrives. */
	for (i = 0; i < LARGE_BUF; i++)
		buf[i] = (char)(i + (i >> 16));

	if (send(fd[0], buf, LARGE_BUF, 0) != LARGE_BUF) e(0);

	/* Part 2: check that remote shutdown terminates a partial receive. */
	memset(buf, 'x', LARGE_BUF);

	if (recv(fd[0], buf, LARGE_BUF, MSG_WAITALL) != LARGE_BUF / 2) e(0);

	for (i = 0; i < LARGE_BUF / 2; i++)
		if (buf[i] != (char)(i + (i >> 16))) e(0);
	for (; i < LARGE_BUF; i++)
		if (buf[i] != 'x') e(0);

	if (recv(fd[0], buf, LARGE_BUF, MSG_WAITALL) != 0) e(0);

	/* Part 3: check that remote close terminates a partial receive. */
	for (i = 0; i < LARGE_BUF; i++)
		buf[i] = (char)(i + (i >> 16));

	if (send(fd[0], buf, LARGE_BUF - 1, 0) != LARGE_BUF - 1) e(0);

	if (close(fd[0]) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	free(buf);
}

#define PRINT_STATS	0

/*
 * A randomized producer-consumer test for stream sockets.  As part of this,
 * we also perform very basic bulk functionality tests of FIONREAD, MSG_PEEK,
 * MSG_DONTWAIT, and MSG_WAITALL.
 */
void
socklib_producer_consumer(int fd[2])
{
	char *buf;
	time_t t;
	socklen_t len, size, off;
	ssize_t r;
	pid_t pid;
	int i, rcvlen, status, exp, flags, num, stat[3] = { 0, 0, 0 };

	len = sizeof(rcvlen);
	if (getsockopt(fd[0], SOL_SOCKET, SO_RCVBUF, &rcvlen, &len) != 0) e(0);
	if (len != sizeof(rcvlen)) e(0);

	size = rcvlen * 3;

	if ((buf = malloc(size)) == NULL) e(0);

	t = time(NULL);

	/*
	 * We vary small versus large (random) send and receive sizes,
	 * splitting the entire transfer in four phases along those lines.
	 *
	 * In theory, the use of an extra system call, the use of MSG_PEEK, and
	 * the fact that without MSG_WAITALL a receive call may return any
	 * partial result, all contribute to the expectation that the consumer
	 * side will fall behind the producer.  In order to test both filling
	 * and draining the receive queue, we use a somewhat larger small
	 * receive size for the consumer size (up to 256 bytes rather than 64)
	 * during each half of the four phases.  The effectiveness of these
	 * numbers can be verified with statistics (disabled by default).
	 */
#define TRANSFER_SIZE	(16 * 1024 * 1024)

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[0]) != 0) e(0);

		srand48(t + 1);

		for (off = 0; off < TRANSFER_SIZE; ) {
			if (off < TRANSFER_SIZE / 2)
				len = lrand48() %
				    ((off / (TRANSFER_SIZE / 8) % 2) ? 64 :
				    256);
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
				 * the returned number should be a lower bound.
				 */
				if (ioctl(fd[1], FIONREAD, &exp) != 0) e(0);
				if (exp < 0 || exp > rcvlen) e(0);
			} else
				exp = -1;

			stat[0]++;

			if ((r = recv(fd[1], buf, len, flags)) == -1) {
				if (errno != EWOULDBLOCK) e(0);
				if (exp > 0) e(0);

				stat[2]++;
				continue;
			}

			if (r < len) {
				stat[1]++;

				if (exp > r) e(0);
			}

			for (i = 0; i < r; i++)
				if (buf[i] != (char)((off + i) +
				    ((off + i) >> 16))) e(0);

			if (!(flags & MSG_PEEK)) {
				off += r;

				if ((flags & (MSG_DONTWAIT | MSG_WAITALL)) ==
				    MSG_WAITALL && r != len &&
				    off < TRANSFER_SIZE) e(0);
			}
		}

#if PRINT_STATS
		/*
		 * The second and third numbers should ideally be a large but
		 * non-dominating fraction of the first one.
		 */
		printf("RECV: total %d short %d again %d\n",
		    stat[0], stat[1], stat[2]);
#endif

		if (close(fd[1]) != 0) e(0);
		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd[1]) != 0) e(0);

	srand48(t);

	for (off = 0; off < TRANSFER_SIZE; ) {
		if (off < TRANSFER_SIZE / 4 ||
		    (off >= TRANSFER_SIZE / 2 && off < TRANSFER_SIZE * 3 / 4))
			len = lrand48() % 64;
		else
			len = lrand48() % size;

		if (len > TRANSFER_SIZE - off)
			len = TRANSFER_SIZE - off;

		for (i = 0; i < len; i++)
			buf[i] = (off + i) + ((off + i) >> 16);

		flags = (lrand48() % 2) ? MSG_DONTWAIT : 0;

		stat[0]++;

		r = send(fd[0], buf, len, flags);

		if (r != len) {
			if (r > (ssize_t)len) e(0);
			if (!(flags & MSG_DONTWAIT)) e(0);
			if (r == -1) {
				if (errno != EWOULDBLOCK) e(0);
				r = 0;

				stat[2]++;
			} else
				stat[1]++;
		}

		if (off / (TRANSFER_SIZE / 4) !=
		    (off + r) / (TRANSFER_SIZE / 4))
			sleep(1);

		off += r;
	}

#if PRINT_STATS
	/*
	 * The second and third numbers should ideally be a large but non-
	 * dominating fraction of the first one.
	 */
	printf("SEND: total %d short %d again %d\n",
	    stat[0], stat[1], stat[2]);
#endif

	free(buf);

	if (close(fd[0]) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);
}

/*
 * Signal handler which just needs to exist, so that invoking it will interrupt
 * an ongoing system call.
 */
static void
socklib_got_signal(int sig __unused)
{

	/* Nothing. */
}

/*
 * Test for receiving on stream sockets.  The quick summary here is that
 * recv(MSG_WAITALL) should keep suspending until as many bytes as requested
 * are also received (or the call is interrupted, or no more can possibly be
 * received - the meaning of the latter depends on the domain), and,
 * SO_RCVLOWAT acts as an admission test for the receive: nothing is received
 * until there are at least as many bytes are available in the receive buffer
 * as the low receive watermark, or the whole receive request length, whichever
 * is smaller. In addition, select(2) should use the same threshold.
 */
#define MAX_BYTES	2	/* set to 3 for slightly better(?) testing */
#define USLEEP_TIME	250000	/* increase on wimpy platforms if needed */

static void
socklib_stream_recv_sub(int (* socket_pair)(int, int, int, int *), int domain,
	int type, int idata, int istate, int rlowat, int len, int bits,
	int act, int (* break_recv)(int, const char *, size_t))
{
	const char *data = "ABCDE";	/* this limits MAX_BYTES to 3 */
	struct sigaction sa;
	struct timeval tv;
	fd_set fds;
	char buf[3];
	pid_t pid;
	int fd[2], val, flags, min, res, err;
	int pfd[2], edata, tstate, fl, status;

	if (socket_pair(domain, type, 0, fd) != 0) e(0);

	/*
	 * Set up the initial condition on the sockets.
	 */
	if (idata > 0)
		if (send(fd[1], data, idata, 0) != idata) e(0);

	switch (istate) {
	case 0: break;
	case 1: if (shutdown(fd[0], SHUT_RD) != 0) e(0); break;
	case 2: if (shutdown(fd[1], SHUT_WR) != 0) e(0); break;
	case 3: if (close(fd[1]) != 0) e(0); break;
	}

	/* Set the low receive water mark. */
	if (setsockopt(fd[0], SOL_SOCKET, SO_RCVLOWAT, &rlowat,
	    sizeof(rlowat)) != 0) e(0);

	/* SO_RCVLOWAT is always bounded by the actual receive length. */
	min = MIN(len, rlowat);

	/*
	 * Do a quick select test to see if its result indeed matches whether
	 * the available data in the receive buffer meets the threshold.
	 */
	FD_ZERO(&fds);
	FD_SET(fd[0], &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	res = select(fd[0] + 1, &fds, NULL, NULL, &tv);
	if (res < 0 || res > 1) e(0);
	if (res != (idata >= rlowat || istate > 0)) e(0);
	if (res == 1 && !FD_ISSET(fd[0], &fds)) e(0);

	/* Also do a quick test for ioctl(FIONREAD). */
	if (ioctl(fd[0], FIONREAD, &val) != 0) e(0);
	if (val != ((istate != 1) ? idata : 0)) e(0);

	/* Translate the given bits to receive call flags. */
	flags = 0;
	if (bits & 1) flags |= MSG_PEEK;
	if (bits & 2) flags |= MSG_DONTWAIT;
	if (bits & 4) flags |= MSG_WAITALL;

	/*
	 * Cut short a whole lot of cases, to avoid the overhead of forking,
	 * namely when we know the call should return immediately.  This is
	 * the case when MSG_DONTWAIT is set, or if a termination condition has
	 * been raised, or if enough initial data are available to meet the
	 * conditions for the receive call.
	 */
	if ((flags & MSG_DONTWAIT) || istate > 0 || (idata >= min &&
	    ((flags & (MSG_PEEK | MSG_WAITALL)) != MSG_WAITALL ||
	    idata >= len))) {
		res = recv(fd[0], buf, len, flags);

		if (res == -1 && errno != EWOULDBLOCK) e(0);

		/*
		 * If the socket has been shutdown locally, we will never get
		 * anything but zero.  Otherwise, if we meet the SO_RCVLOWAT
		 * test, we should have received as much as was available and
		 * requested.  Otherwise, if the remote end has been shut down
		 * or closed, we expected to get any available data or
		 * otherwise EOF (implied with idata==0).  If none of these
		 * cases apply, we should have gotten EWOULDBLOCK.
		 */
		if (istate == 1) {
			if (res != 0) e(0);
		} else if (idata >= min) {
			if (res != MIN(len, idata)) e(0);
			if (strncmp(buf, data, res)) e(0);
		} else if (istate > 0) {
			if (res != idata) e(0);
			if (strncmp(buf, data, res)) e(0);
		} else
			if (res != -1) e(0);

		/* Early cleanup and return to avoid even more code clutter. */
		if (istate != 3 && close(fd[1]) != 0) e(0);
		if (close(fd[0]) != 0) e(0);

		return;
	}

	/*
	 * Now starts the interesting stuff: the receive call should now block,
	 * even though if we add MSG_DONTWAIT it may not return EWOULDBLOCK,
	 * because MSG_DONTWAIT overrides MSG_WAITALL.  As such, we can only
	 * test our expectations by actually letting the call block, in a child
	 * process, and waiting.  We do test as much of the above assumption as
	 * we can just for safety right here, but this is not a substitute for
	 * actually blocking even in these cases!
	 */
	if (!(flags & MSG_WAITALL)) {
		if (recv(fd[0], buf, len, flags | MSG_DONTWAIT) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);
	}

	/*
	 * If (act < 12), we send 0, 1, or 2 extra data bytes before forcing
	 * the receive call to terminate in one of four ways.
	 *
	 * If (act == 12), we use a signal to interrupt the receive call.
	 */
	if (act < 12) {
		edata = act % 3;
		tstate = act / 3;
	} else
		edata = tstate = 0;

	if (pipe2(pfd, O_NONBLOCK) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[1]) != 0) e(0);
		if (close(pfd[0]) != 0) e(0);

		if (act == 12) {
			memset(&sa, 0, sizeof(sa));
			sa.sa_handler = socklib_got_signal;
			if (sigaction(SIGUSR1, &sa, NULL) != 0) e(0);
		}

		res = recv(fd[0], buf, len, flags);
		err = errno;

		if (write(pfd[1], &res, sizeof(res)) != sizeof(res)) e(0);
		if (write(pfd[1], &err, sizeof(err)) != sizeof(err)) e(0);

		if (res > 0 && strncmp(buf, data, res)) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(pfd[1]) != 0) e(0);

	/*
	 * Allow the child to enter the blocking recv(2), and check the pipe
	 * to see if it is really blocked.
	 */
	if (usleep(USLEEP_TIME) != 0) e(0);

	if (read(pfd[0], buf, 1) != -1) e(0);
	if (errno != EAGAIN) e(0);

	if (edata > 0) {
		if (send(fd[1], &data[idata], edata, 0) != edata) e(0);

		/*
		 * The threshold for the receive is now met if both the minimum
		 * is met and MSG_WAITALL was not set (or overridden by
		 * MSG_PEEK) or the entire request has been satisfied.
		 */
		if (idata + edata >= min &&
		    ((flags & (MSG_PEEK | MSG_WAITALL)) != MSG_WAITALL ||
		    idata + edata >= len)) {
			if ((fl = fcntl(pfd[0], F_GETFL, 0)) == -1) e(0);
			if (fcntl(pfd[0], F_SETFL, fl & ~O_NONBLOCK) != 0)
			    e(0);

			if (read(pfd[0], &res, sizeof(res)) != sizeof(res))
			    e(0);
			if (read(pfd[0], &err, sizeof(err)) != sizeof(err))
			    e(0);

			if (res != MIN(idata + edata, len)) e(0);

			/* Bail out. */
			goto cleanup;
		}

		/* Sleep and test once more. */
		if (usleep(USLEEP_TIME) != 0) e(0);

		if (read(pfd[0], buf, 1) != -1) e(0);
		if (errno != EAGAIN) e(0);
	}

	if (act < 12) {
		/*
		 * Now test various ways to terminate the receive call.
		 */
		switch (tstate) {
		case 0: if (shutdown(fd[0], SHUT_RD) != 0) e(0); break;
		case 1: if (shutdown(fd[1], SHUT_WR) != 0) e(0); break;
		case 2: if (close(fd[1]) != 0) e(0); fd[1] = -1; break;
		case 3: fd[1] = break_recv(fd[1], data, strlen(data)); break;
		}
	} else
		if (kill(pid, SIGUSR1) != 0) e(0);

	if ((fl = fcntl(pfd[0], F_GETFL, 0)) == -1) e(0);
	if (fcntl(pfd[0], F_SETFL, fl & ~O_NONBLOCK) != 0) e(0);

	if (read(pfd[0], &res, sizeof(res)) != sizeof(res)) e(0);
	if (read(pfd[0], &err, sizeof(err)) != sizeof(err)) e(0);

	if (act < 12) {
		/*
		 * If there were any data we should have received them now;
		 * after all the receive minimum stops being relevant when
		 * another condition has been raised.  There is one exception:
		 * if the receive threshold was never met and we now shut down
		 * the socket for reading, EOF is acceptable as return value.
		 */
		if (tstate == 0 && idata + edata < min) {
			if (res != 0) e(0);
		} else if (idata + edata > 0) {
			if (res != MIN(idata + edata, len)) e(0);
		} else if (tstate == 3) {
			if (fd[1] == -1) {
				if (res != -1) e(0);
				if (err != ECONNRESET) e(0);
			} else
				if (res != len) e(0);
		} else
			if (res != 0) e(0);
	} else {
		/*
		 * If the receive met the threshold before being interrupted,
		 * we should have received at least something.  Otherwise, the
		 * receive was never admitted and should just return EINTR.
		 */
		if (idata >= min) {
			if (res != MIN(idata, len)) e(0);
		} else {
			if (res != -1) e(0);
			if (err != EINTR) e(0);
		}
	}

cleanup:
	if (close(pfd[0]) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	if (fd[1] != -1 && close(fd[1]) != 0) e(0);
	if (close(fd[0]) != 0) e(0);
}

/*
 * Test for receiving on stream sockets.  In particular, test SO_RCVLOWAT,
 * MSG_PEEK, MSG_DONTWAIT, and MSG_WAITALL.
 */
void
socklib_stream_recv(int (* socket_pair)(int, int, int, int *), int domain,
	int type, int (* break_recv)(int, const char *, size_t))
{
	int idata, istate, rlowat, len, bits, act;

	/* Insanity. */
	for (idata = 0; idata <= MAX_BYTES; idata++)
		for (istate = 0; istate <= 3; istate++)
			for (rlowat = 1; rlowat <= MAX_BYTES; rlowat++)
				for (len = 1; len <= MAX_BYTES; len++)
					for (bits = 0; bits < 8; bits++)
						for (act = 0; act <= 12; act++)
							socklib_stream_recv_sub
							    (socket_pair,
							    domain, type,
							    idata, istate,
							    rlowat, len, bits,
							    act, break_recv);
}

/*
 * Obtain information for a matching protocol control block, using sysctl(7).
 * The PCB is to be obtained through the given sysctl path string, and must
 * match the other given parameters.  Return 1 if found with 'ki' filled with
 * the PCB information, or 0 if not.
 */
int
socklib_find_pcb(const char * path, int protocol, uint16_t local_port,
	uint16_t remote_port, struct kinfo_pcb * ki)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct kinfo_pcb *array;
	size_t i, miblen, oldlen;
	uint16_t lport, rport;
	int mib[CTL_MAXNAME], found;

	miblen = __arraycount(mib);
	if (sysctlnametomib(path, mib, &miblen) != 0) e(0);
	if (miblen > __arraycount(mib) - 4) e(0);
	mib[miblen++] = 0;
	mib[miblen++] = 0;
	mib[miblen++] = sizeof(*array);
	mib[miblen++] = 0;

	if (sysctl(mib, miblen, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0)
		return 0;	/* should not happen due to added slop space */
	if (oldlen % sizeof(*array)) e(0);

	if ((array = (struct kinfo_pcb *)malloc(oldlen)) == NULL) e(0);

	if (sysctl(mib, miblen, array, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen % sizeof(*array)) e(0);

	found = -1;
	for (i = 0; i < oldlen / sizeof(*array); i++) {
		/* Perform some basic checks. */
		if (array[i].ki_pcbaddr == 0) e(0);
		if (array[i].ki_ppcbaddr == 0) e(0);
		if (array[i].ki_family != mib[1]) e(0);

		if (mib[1] == AF_INET6) {
			memcpy(&sin6, &array[i].ki_src, sizeof(sin6));
			if (sin6.sin6_family != AF_INET6) e(0);
			if (sin6.sin6_len != sizeof(sin6)) e(0);
			lport = ntohs(sin6.sin6_port);

			memcpy(&sin6, &array[i].ki_dst, sizeof(sin6));
			if (sin6.sin6_family != AF_INET6) e(0);
			if (sin6.sin6_len != sizeof(sin6)) e(0);
			rport = ntohs(sin6.sin6_port);
		} else {
			memcpy(&sin, &array[i].ki_src, sizeof(sin));
			if (sin.sin_family != AF_INET) e(0);
			if (sin.sin_len != sizeof(sin)) e(0);
			lport = ntohs(sin.sin_port);

			memcpy(&sin, &array[i].ki_dst, sizeof(sin));
			if (sin.sin_family != AF_UNSPEC) {
				if (sin.sin_family != AF_INET) e(0);
				if (sin.sin_len != sizeof(sin)) e(0);
				rport = ntohs(sin.sin_port);
			} else
				rport = 0;
		}

		/* Try to match every PCB.  We must find at most one match. */
		if (array[i].ki_protocol == protocol && lport == local_port &&
		    rport == remote_port) {
			if (found != -1) e(0);

			found = (int)i;
		}
	}

	if (found >= 0)
		memcpy(ki, &array[found], sizeof(*ki));

	free(array);

	return (found != -1);
}

#ifdef NO_INET6
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

void
inet6_getscopeid(struct sockaddr_in6 * sin6 __unused, int flags __unused)
{

	/*
	 * Nothing.  The tests linked to socklib make heavy use of IPv6, and
	 * are expected to fail if IPv6 support is disabled at compile time.
	 * Therefore, what this replacement function does is not relevant.
	 */
}
#endif /* NO_INET6 */

#define F_ANY	0x01	/* not bound, or bound to an 'any' address */
#define F_V4	0x02	/* address is IPv4-mapped IPv6 address */
#define F_REM	0x04	/* address is remote (not assigned to an interface) */
#define F_MIX	0x08	/* address has non-loopback scope */

/*
 * Test local and remote IPv6 address handling on TCP or UDP sockets.
 */
void
socklib_test_addrs(int type, int protocol)
{
	struct sockaddr_in6 sin6, sin6_any, sin6_any_scope, sin6_lo,
	    sin6_lo_scope, sin6_ll_all, sin6_ll_lo, sin6_ll_rem, sin6_ll_kame,
	    sin6_ll_bad, sin6_ll_mix, sin6_rem, sin6_v4_any, sin6_v4_lo,
	    sin6_v4_rem, rsin6;
	const struct sockaddr_in6 *sin6p;
	const struct {
		const struct sockaddr_in6 *addr;
		int res;
		int flags;
		const struct sockaddr_in6 *name;
	} bind_array[] = {
		{ NULL,			0, F_ANY,	&sin6_any },
		{ &sin6_any,		0, F_ANY,	&sin6_any },
		{ &sin6_any_scope,	0, F_ANY,	&sin6_any },
		{ &sin6_lo,		0, 0,		&sin6_lo },
		{ &sin6_lo_scope,	0, 0,		&sin6_lo },
		{ &sin6_ll_lo,		0, 0,		&sin6_ll_lo },
		{ &sin6_v4_lo,		0, F_V4,	&sin6_v4_lo },
		{ &sin6_rem,		EADDRNOTAVAIL },
		{ &sin6_ll_all,		EADDRNOTAVAIL },
		{ &sin6_ll_rem,		EADDRNOTAVAIL },
		{ &sin6_ll_kame,	EINVAL },
		{ &sin6_ll_bad,		ENXIO },
		{ &sin6_v4_any,		EADDRNOTAVAIL },
		{ &sin6_v4_rem,		EADDRNOTAVAIL },
		/* The following entry MUST be last. */
		{ &sin6_ll_mix,		EADDRNOTAVAIL },
	}, *bp;
	const struct {
		const struct sockaddr_in6 *addr;
		int res;
		int flags;
		const struct sockaddr_in6 *name;
	} conn_array[] = {
		{ &sin6_any,		EHOSTUNREACH, 0 },
		{ &sin6_any_scope,	EHOSTUNREACH, 0 },
		{ &sin6_ll_kame,	EINVAL, 0 },
		{ &sin6_ll_bad,		ENXIO, 0 },
		{ &sin6_v4_any,		EHOSTUNREACH, F_V4 },
		{ &sin6_lo,		0, 0,		&sin6_lo },
		{ &sin6_lo_scope,	0, 0,		&sin6_lo },
		{ &sin6_ll_all,		0, 0,		&sin6_ll_lo },
		{ &sin6_ll_lo,		0, 0,		&sin6_ll_lo },
		{ &sin6_v4_lo,		0, F_V4,	&sin6_v4_lo },
		{ &sin6_rem,		0, F_REM,	&sin6_rem },
		{ &sin6_ll_rem,		0, F_REM,	&sin6_ll_rem },
		{ &sin6_v4_rem,		0, F_V4|F_REM,	&sin6_v4_rem },
		/* The following entry MUST be last. */
		{ &sin6_ll_mix,		0, F_REM|F_MIX,	&sin6_ll_mix },
	}, *cp;
	struct ifaddrs *ifa, *ifp, *ifp2;
	struct in6_ifreq ifr;
	char name[IF_NAMESIZE], buf[1];
	socklen_t len;
	uint32_t port;
	unsigned int i, j, ifindex, ifindex2, have_mix, found;
	int r, fd, fd2, fd3, val, sfl, exp, link_state;

	ifindex = if_nametoindex(LOOPBACK_IFNAME);
	if (ifindex == 0) e(0);

	/* An IPv6 'any' address - ::0. */
	memset(&sin6_any, 0, sizeof(sin6_any));
	sin6_any.sin6_len = sizeof(sin6_any);
	sin6_any.sin6_family = AF_INET6;
	memcpy(&sin6_any.sin6_addr, &in6addr_any, sizeof(sin6_any.sin6_addr));

	/* An IPv6 'any' address, but with a bad scope ID set. */
	memcpy(&sin6_any_scope, &sin6_any, sizeof(sin6_any_scope));
	sin6_any_scope.sin6_scope_id = BAD_IFINDEX;

	/* An IPv6 loopback address - ::1. */
	memcpy(&sin6_lo, &sin6_any, sizeof(sin6_lo));
	memcpy(&sin6_lo.sin6_addr, &in6addr_loopback,
	    sizeof(sin6_lo.sin6_addr));

	/* An IPv6 loopback address, but with a bad scope ID set. */
	memcpy(&sin6_lo_scope, &sin6_lo, sizeof(sin6_lo_scope));
	sin6_lo_scope.sin6_scope_id = BAD_IFINDEX;

	/* An IPv6 link-local address without scope - fe80::1. */
	memcpy(&sin6_ll_all, &sin6_any, sizeof(sin6_ll_all));
	if (inet_pton(AF_INET6, LOOPBACK_IPV6_LL, &sin6_ll_all.sin6_addr) != 1)
		e(0);

	/* An IPv6 link-local address with the loopback scope - fe80::1%lo0. */
	memcpy(&sin6_ll_lo, &sin6_ll_all, sizeof(sin6_ll_lo));
	sin6_ll_lo.sin6_scope_id = ifindex;

	/* An unassigned IPv6 link-local address - fe80::ffff%lo0. */
	memcpy(&sin6_ll_rem, &sin6_ll_lo, sizeof(sin6_ll_rem));
	if (inet_pton(AF_INET6, TEST_BLACKHOLE_IPV6_LL,
	    &sin6_ll_rem.sin6_addr) != 1) e(0);

	/* A KAME-style IPv6 link-local loopback address - fe80:ifindex::1. */
	memcpy(&sin6_ll_kame, &sin6_ll_all, sizeof(sin6_ll_kame));
	sin6_ll_kame.sin6_addr.s6_addr[2] = ifindex >> 8;
	sin6_ll_kame.sin6_addr.s6_addr[3] = ifindex % 0xff;

	/* An IPv6 link-local address with a bad scope - fe80::1%<bad>. */
	memcpy(&sin6_ll_bad, &sin6_ll_all, sizeof(sin6_ll_bad));
	sin6_ll_bad.sin6_scope_id = BAD_IFINDEX;

	/* A global IPv6 address not assigned to any interface - ::2. */
	memcpy(&sin6_rem, &sin6_any, sizeof(sin6_rem));
	if (inet_pton(AF_INET6, TEST_BLACKHOLE_IPV6,
	    &sin6_rem.sin6_addr) != 1) e(0);

	/* An IPv4-mapped IPv6 address for 'any' - ::ffff:0.0.0.0. */
	memcpy(&sin6_v4_any, &sin6_any, sizeof(sin6_v4_any));
	if (inet_pton(AF_INET6, "::ffff:0:0", &sin6_v4_any.sin6_addr) != 1)
		e(0);

	/* An IPv4-mapped IPv6 loopback address - ::ffff:127.0.0.1. */
	memcpy(&sin6_v4_lo, &sin6_any, sizeof(sin6_v4_lo));
	if (inet_pton(AF_INET6, "::ffff:"LOOPBACK_IPV4,
	    &sin6_v4_lo.sin6_addr) != 1) e(0);

	/* An unassigned IPv4-mapped IPv6 address - ::ffff:127.255.0.254. */
	memcpy(&sin6_v4_rem, &sin6_any, sizeof(sin6_v4_rem));
	if (inet_pton(AF_INET6, "::ffff:"TEST_BLACKHOLE_IPV4,
	    &sin6_v4_rem.sin6_addr) != 1) e(0);

	/*
	 * An IPv6 link-local address with a scope for another interface, for
	 * example fe80::1%em0.  Since no other interfaces may be present, we
	 * may not be able to generate such an address.
	 */
	have_mix = 0;
	for (i = 1; i < BAD_IFINDEX; i++) {
		if (if_indextoname(i, name) == NULL) {
			if (errno != ENXIO) e(0);
			continue;
		}

		if (!strcmp(name, LOOPBACK_IFNAME))
			continue;

		/* Found one! */
		memcpy(&sin6_ll_mix, &sin6_ll_all, sizeof(sin6_ll_mix));
		sin6_ll_mix.sin6_scope_id = i;
		have_mix = 1;
		break;
	}

	/*
	 * Test a whole range of combinations of local and remote addresses,
	 * both for TCP and UDP, and for UDP both for connect+send and sendto.
	 * Not all addresses and not all combinations are compatible, and that
	 * is exactly what we want to test.  We first test binding to local
	 * addresses.  Then we test connect (and for UDP, on success, send)
	 * with remote addresses on those local addresses that could be bound
	 * to.  Finally, for UDP sockets, we separately test sendto.
	 */
	for (i = 0; i < __arraycount(bind_array) - !have_mix; i++) {
		bp = &bind_array[i];

		/* Test bind(2) and getsockname(2). */
		if (bind_array[i].addr != NULL) {
			if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

			val = 0;
			if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
			    sizeof(val)) != 0) e(0);

			r = bind(fd, (struct sockaddr *)bp->addr,
			    sizeof(*bp->addr));

			/* Did the bind(2) call produce the expected result? */
			if (r == 0) {
				if (bp->res != 0) e(0);
			} else
				if (r != -1 || bp->res != errno) e(0);

			/* The rest is for successful bind(2) calls. */
			if (r != 0) {
				if (close(fd) != 0) e(0);

				continue;
			}

			/* Get the bound address. */
			len = sizeof(sin6);
			if (getsockname(fd, (struct sockaddr *)&sin6,
			    &len) != 0) e(0);
			if (len != sizeof(sin6)) e(0);

			/* A port must be set.  Clear it for the comparison. */
			if ((sin6.sin6_port == 0) == (type != SOCK_RAW)) e(0);

			sin6.sin6_port = 0;
			if (memcmp(&sin6, bp->name, sizeof(sin6)) != 0) e(0);

			if (close(fd) != 0) e(0);
		}

		/* Test connect(2), send(2), and getpeername(2). */
		for (j = 0; j < __arraycount(conn_array) - !have_mix; j++) {
			cp = &conn_array[j];

			/*
			 * We cannot test remote addresses without having bound
			 * to a local address, because we may end up generating
			 * external traffic as a result.
			 */
			if ((bp->flags & F_ANY) && (cp->flags & F_REM))
				continue;

			/*
			 * Use non-blocking sockets only if connecting is going
			 * to take a while before ultimately failing; TCP only.
			 */
			sfl = ((cp->flags & F_REM) && (type == SOCK_STREAM)) ?
			    SOCK_NONBLOCK : 0;
			if ((fd = socket(AF_INET6, type | sfl, protocol)) < 0)
				e(0);

			val = 0;
			if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
			    sizeof(val)) != 0) e(0);

			if (bp->addr != NULL) {
				if (bind(fd, (struct sockaddr *)bp->addr,
				    sizeof(*bp->addr)) != 0) e(0);

				len = sizeof(sin6);
				if (getsockname(fd, (struct sockaddr *)&sin6,
				    &len) != 0) e(0);

				port = sin6.sin6_port;
			} else
				port = 0;

			memcpy(&sin6, cp->addr, sizeof(sin6));
			sin6.sin6_port = htons(TEST_PORT_A);

			if ((exp = cp->res) == 0 && type == SOCK_STREAM) {
				if (cp->flags & F_REM)
					exp = EINPROGRESS;
				if (cp->flags & F_MIX)
					exp = EHOSTUNREACH;
			}

			/*
			 * The IPv4/IPv6 mismatch check precedes most other
			 * checks, but (currently) not the bad-scope-ID check.
			 */
			if (exp != ENXIO && !(bp->flags & F_ANY) &&
			    ((bp->flags ^ cp->flags) & F_V4))
				exp = EINVAL;

			/*
			 * Create a listening or receiving socket if we expect
			 * the test to succeed and operate on a loopback target
			 * so that we can test addresses on that end as well.
			 */
			if (exp == 0 && !(cp->flags & F_REM)) {
				if ((fd2 = socket(AF_INET6, type,
				    protocol)) < 0) e(0);

				val = 0;
				if (setsockopt(fd2, IPPROTO_IPV6, IPV6_V6ONLY,
				    &val, sizeof(val)) != 0) e(0);

				val = 1;
				if (setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR,
				    &val, sizeof(val)) != 0) e(0);

				memcpy(&rsin6, cp->name, sizeof(rsin6));
				rsin6.sin6_port = htons(TEST_PORT_A);

				if (bind(fd2, (struct sockaddr *)&rsin6,
				    sizeof(rsin6)) != 0) e(0);

				if (type == SOCK_STREAM && listen(fd2, 1) != 0)
					e(0);
			} else
				fd2 = -1;

			r = connect(fd, (struct sockaddr *)&sin6,
			    sizeof(sin6));

			if (r == 0) {
				if (exp != 0) e(0);
			} else
				if (r != -1 || exp != errno) e(0);

			if (r != 0) {
				if (close(fd) != 0) e(0);

				continue;
			}

			/*
			 * Connecting should always assign a local address if
			 * no address was assigned, even if a port was assigned
			 * already.  In the latter case, the port number must
			 * obviously not change.  Test getsockname(2) again, if
			 * we can.
			 */
			len = sizeof(sin6);
			if (getsockname(fd, (struct sockaddr *)&sin6,
			    &len) != 0) e(0);
			if (len != sizeof(sin6)) e(0);

			if (type != SOCK_RAW) {
				if (sin6.sin6_port == 0) e(0);
				if (port != 0 && port != sin6.sin6_port) e(0);
			} else
				if (sin6.sin6_port != 0) e(0);
			port = sin6.sin6_port;

			if (!(bp->flags & F_ANY))
				sin6p = bp->name;
			else if (!(cp->flags & F_REM))
				sin6p = cp->name;
			else
				sin6p = NULL; /* can't test: may vary */

			if (sin6p != NULL) {
				sin6.sin6_port = 0;

				if (memcmp(&sin6, sin6p, sizeof(sin6)) != 0)
					e(0);
			}

			/*
			 * Test getpeername(2).  It should always be the
			 * "normalized" version of the target address.
			 */
			len = sizeof(sin6);
			if (getpeername(fd, (struct sockaddr *)&sin6,
			    &len) != 0) e(0);
			if (len != sizeof(sin6)) e(0);

			if (type != SOCK_RAW) {
				if (sin6.sin6_port != htons(TEST_PORT_A)) e(0);
			} else {
				if (sin6.sin6_port != 0) e(0);
			}

			sin6.sin6_port = 0;
			if (memcmp(&sin6, cp->name, sizeof(sin6)) != 0) e(0);

			/* Test send(2) on UDP sockets. */
			if (type != SOCK_STREAM) {
				r = send(fd, "A", 1, 0);

				/*
				 * For remote (rejected) addresses and scope
				 * mixing, actual send calls may fail after the
				 * connect succeeded.
				 */
				if (r == -1 &&
				    !(cp->flags & (F_REM | F_MIX))) e(0);
				else if (r != -1 && r != 1) e(0);

				if (r != 1 && fd2 != -1) {
					if (close(fd2) != 0) e(0);
					fd2 = -1;
				}
			}

			if (fd2 == -1) {
				if (close(fd) != 0) e(0);

				continue;
			}

			/*
			 * The connect or send call succeeded, so we should now
			 * be able to check the other end.
			 */
			if (type == SOCK_STREAM) {
				/* Test accept(2). */
				len = sizeof(sin6);
				if ((fd3 = accept(fd2,
				    (struct sockaddr *)&sin6, &len)) < 0) e(0);
				if (len != sizeof(sin6)) e(0);

				if (close(fd2) != 0) e(0);

				if (sin6.sin6_port != port) e(0);
				sin6.sin6_port = 0;

				if (memcmp(&sin6, sin6p, sizeof(sin6)) != 0)
					e(0);

				/* Test getpeername(2). */
				if (getpeername(fd3, (struct sockaddr *)&sin6,
				    &len) != 0) e(0);
				if (len != sizeof(sin6)) e(0);

				if (sin6.sin6_port != port) e(0);
				sin6.sin6_port = 0;

				if (memcmp(&sin6, sin6p, sizeof(sin6)) != 0)
					e(0);

				/* Test getsockname(2). */
				if (getsockname(fd3, (struct sockaddr *)&sin6,
				    &len) != 0) e(0);
				if (len != sizeof(sin6)) e(0);

				if (sin6.sin6_port != htons(TEST_PORT_A)) e(0);
				sin6.sin6_port = 0;

				if (memcmp(&sin6, cp->name, sizeof(sin6)) != 0)
					e(0);

				if (close(fd3) != 0) e(0);
			} else {
				/* Test recvfrom(2). */
				len = sizeof(sin6);
				if (recvfrom(fd2, buf, sizeof(buf), 0,
				    (struct sockaddr *)&sin6, &len) != 1) e(0);

				if (buf[0] != 'A') e(0);
				if (len != sizeof(sin6)) e(0);

				if (sin6.sin6_port != port) e(0);
				sin6.sin6_port = 0;

				if (memcmp(&sin6, sin6p, sizeof(sin6)) != 0)
					e(0);

				if (close(fd2) != 0) e(0);
			}

			if (close(fd) != 0) e(0);
		}

		if (type == SOCK_STREAM)
			continue;

		/* Test sendto(2). */
		for (j = 0; j < __arraycount(conn_array) - !have_mix; j++) {
			cp = &conn_array[j];

			/*
			 * We cannot test remote addresses without having bound
			 * to a local address, because we may end up generating
			 * external traffic as a result.
			 */
			if ((bp->flags & F_ANY) && (cp->flags & F_REM))
				continue;

			if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

			val = 0;
			if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
			    sizeof(val)) != 0) e(0);

			if (bp->addr != NULL) {
				if (bind(fd, (struct sockaddr *)bp->addr,
				   sizeof(*bp->addr)) != 0) e(0);

				len = sizeof(sin6);
				if (getsockname(fd, (struct sockaddr *)&sin6,
				    &len) != 0) e(0);

				port = sin6.sin6_port;
			} else
				port = 0;

			memcpy(&sin6, cp->addr, sizeof(sin6));
			if (type != SOCK_RAW)
				sin6.sin6_port = htons(TEST_PORT_B);

			if ((exp = cp->res) == 0) {
				if (cp->flags & (F_REM | F_MIX))
					exp = EHOSTUNREACH;
			}

			/*
			 * The IPv4/IPv6 mismatch check precedes most other
			 * checks, but (currently) not the bad-scope-ID check.
			 */
			if (exp != ENXIO && !(bp->flags & F_ANY) &&
			    ((bp->flags ^ cp->flags) & F_V4))
				exp = EINVAL;

			/*
			 * If we expect the sendto(2) call to succeed and to be
			 * able to receive the packet, create a receiving
			 * socket to test recvfrom(2) addresses.
			 */
			if (exp == 0 && !(cp->flags & F_REM)) {
				if ((fd2 = socket(AF_INET6, type,
				    protocol)) < 0) e(0);

				val = 0;
				if (setsockopt(fd2, IPPROTO_IPV6, IPV6_V6ONLY,
				    &val, sizeof(val)) != 0) e(0);

				val = 1;
				if (setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR,
				    &val, sizeof(val)) != 0) e(0);

				memcpy(&rsin6, cp->name, sizeof(rsin6));
				if (type != SOCK_RAW)
					rsin6.sin6_port = htons(TEST_PORT_B);

				if (bind(fd2, (struct sockaddr *)&rsin6,
				    sizeof(rsin6)) != 0) e(0);
			} else
				fd2 = -1;

			r = sendto(fd, "B", 1, 0, (struct sockaddr *)&sin6,
			    sizeof(sin6));

			if (r != 1) {
				if (r != -1 || exp != errno) e(0);

				if (close(fd) != 0) e(0);

				continue;
			}

			if (exp != 0) e(0);

			/*
			 * The sendto(2) call should assign a local port to the
			 * socket if none was assigned before, but it must not
			 * assign a local address.
			 */
			len = sizeof(sin6);
			if (getsockname(fd, (struct sockaddr *)&sin6,
			    &len) != 0) e(0);
			if (len != sizeof(sin6)) e(0);

			if (type != SOCK_RAW) {
				if (sin6.sin6_port == 0) e(0);
				if (port != 0 && port != sin6.sin6_port) e(0);
			} else
				if (sin6.sin6_port != 0) e(0);
			port = sin6.sin6_port;

			sin6.sin6_port = 0;
			if (memcmp(&sin6, bp->name, sizeof(sin6)) != 0) e(0);

			if (fd2 != -1) {
				/* Test recvfrom(2) on the receiving socket. */
				len = sizeof(sin6);
				if (recvfrom(fd2, buf, sizeof(buf), 0,
				    (struct sockaddr *)&sin6, &len) != 1) e(0);

				if (buf[0] != 'B') e(0);
				if (len != sizeof(sin6)) e(0);

				if (sin6.sin6_port != port) e(0);
				sin6.sin6_port = 0;

				if (bp->flags & F_ANY)
					sin6p = cp->name;
				else
					sin6p = bp->name;

				if (memcmp(&sin6, sin6p, sizeof(sin6)) != 0)
					e(0);

				if (close(fd2) != 0) e(0);
			}

			if (close(fd) != 0) e(0);
		}
	}

	/*
	 * Test that scoped addresses actually work as expected.  For this we
	 * need two interfaces with assigned link-local addresses, one of which
	 * being the loopback interface.  Start by finding another one.
	 */
	if (getifaddrs(&ifa) != 0) e(0);

	found = 0;
	for (ifp = ifa; ifp != NULL; ifp = ifp->ifa_next) {
		if (strcmp(ifp->ifa_name, LOOPBACK_IFNAME) == 0)
			continue;

		if (!(ifp->ifa_flags & IFF_UP) || ifp->ifa_addr == NULL ||
		    ifp->ifa_addr->sa_family != AF_INET6)
			continue;

		memcpy(&sin6, ifp->ifa_addr, sizeof(sin6));

		if (!IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr))
			continue;

		/*
		 * Not only the interface, but also the link has to be up for
		 * this to work.  lwIP will drop all packets, including those
		 * sent to locally assigned addresses, if the link is down.
		 * Of course, figuring out whether the interface link is down
		 * is by no means convenient, especially if we want to do it
		 * right (i.e., not rely on getifaddrs' address sorting).
		 */
		link_state = LINK_STATE_DOWN;

		for (ifp2 = ifa; ifp2 != NULL; ifp2 = ifp2->ifa_next) {
			if (!strcmp(ifp2->ifa_name, ifp->ifa_name) &&
			    ifp2->ifa_addr != NULL &&
			    ifp2->ifa_addr->sa_family == AF_LINK &&
			    ifp2->ifa_data != NULL) {
				memcpy(&link_state, &((struct if_data *)
				    ifp2->ifa_data)->ifi_link_state,
				    sizeof(link_state));

				break;
			}
		}

		if (link_state == LINK_STATE_DOWN)
			continue;

		/*
		 * In addition, the address has to be in a state where it can
		 * be used as source address.  In practice, that means it must
		 * not be in ND6 duplicated or tentative state.
		 */
		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ifp->ifa_name, sizeof(ifr.ifr_name));
		memcpy(&ifr.ifr_addr, &sin6, sizeof(sin6));

		if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

		if (ioctl(fd, SIOCGIFAFLAG_IN6, &ifr) != 0) e(0);

		if (close(fd) != 0) e(0);

		if (ifr.ifr_ifru.ifru_flags6 &
		    (IN6_IFF_DUPLICATED | IN6_IFF_TENTATIVE))
			continue;

		/* Compensate for poor decisions made by the KAME project. */
		inet6_getscopeid(&sin6, INET6_IS_ADDR_LINKLOCAL);

		if (sin6.sin6_scope_id == 0 || sin6.sin6_scope_id == ifindex)
			e(0);

		found = 1;

		break;
	}

	freeifaddrs(ifa);

	/*
	 * If no second interface with a link-local address was found, we
	 * cannot perform the rest of this subtest.
	 */
	if (!found)
		return;

	/*
	 * Create one socket that binds to the link-local address of the
	 * non-loopback interface.  The main goal of this subtest is to ensure
	 * that traffic directed to that same link-local address but with the
	 * loopback scope ID does not arrive on this socket.
	 */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	len = sizeof(sin6);
	if (getsockname(fd, (struct sockaddr *)&sin6, &len) != 0) e(0);
	if (len != sizeof(sin6)) e(0);

	ifindex2 = sin6.sin6_scope_id;

	if (type == SOCK_STREAM) {
		if (listen(fd, 2) != 0) e(0);

		if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

		/* Connecting to the loopback-scope address should time out. */
		signal(SIGALRM, socklib_got_signal);
		alarm(1);

		sin6.sin6_scope_id = ifindex;

		if (connect(fd2, (struct sockaddr *)&sin6, sizeof(sin6)) != -1)
			e(0);

		if (errno != EINTR) e(0);

		if (close(fd2) != 0) e(0);

		/* Connecting to the real interface's address should work. */
		if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

		sin6.sin6_scope_id = ifindex2;

		if (connect(fd2, (struct sockaddr *)&sin6, sizeof(sin6)) != 0)
			e(0);

		if (close(fd2) != 0) e(0);
	} else {
		/*
		 * First connect+send.  Sending to the loopback-scope address
		 * should result in a rejected packet.
		 */
		if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

		sin6.sin6_scope_id = ifindex;

		if (connect(fd2, (struct sockaddr *)&sin6, sizeof(sin6)) != 0)
			e(0);

		if (send(fd2, "C", 1, 0) != -1) e(0);
		if (errno != EHOSTUNREACH) e(0);

		if (close(fd2) != 0) e(0);

		/* Sending to the real-interface address should work. */
		if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

		sin6.sin6_scope_id = ifindex2;

		if (connect(fd2, (struct sockaddr *)&sin6, sizeof(sin6)) != 0)
			e(0);

		if (send(fd2, "D", 1, 0) != 1) e(0);

		if (close(fd2) != 0) e(0);

		/*
		 * Then sendto.  Sending to the loopback-scope address should
		 * result in a rejected packet.
		 */
		if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

		sin6.sin6_scope_id = ifindex;

		if (sendto(fd2, "E", 1, 0, (struct sockaddr *)&sin6,
		    sizeof(sin6)) != -1) e(0);
		if (errno != EHOSTUNREACH) e(0);

		if (close(fd2) != 0) e(0);

		/* Sending to the real-interface address should work. */
		if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

		sin6.sin6_scope_id = ifindex2;

		if (sendto(fd2, "F", 1, 0, (struct sockaddr *)&sin6,
		    sizeof(sin6)) != 1) e(0);

		if (close(fd2) != 0) e(0);

		len = sizeof(sin6);
		if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin6,
		    &len) != 1) e(0);
		if (buf[0] != 'D') e(0);

		if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin6,
		    &len) != 1) e(0);
		if (buf[0] != 'F') e(0);
	}

	if (close(fd) != 0) e(0);
}

/*
 * Test multicast support for the given socket type, which may be SOCK_DGRAM or
 * SOCK_RAW.
 */
void
socklib_test_multicast(int type, int protocol)
{
	struct sockaddr_in sinA, sinB, sin_array[3];
	struct sockaddr_in6 sin6A, sin6B, sin6_array[3];
	struct ip_mreq imr;
	struct ipv6_mreq ipv6mr;
	struct in6_pktinfo ipi6;
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	socklen_t len, hdrlen;
	unsigned int count, ifindex, ifindex2;
	union {
		struct cmsghdr cmsg;
		char buf[256];
	} control;
	char buf[sizeof(struct ip) + 1], *buf2, name[IF_NAMESIZE];
	uint8_t byte, ttl;
	int i, j, r, fd, fd2, val;

	/*
	 * Start with testing join/leave mechanics, for both IPv4 and IPv6.
	 * Note that we cannot test specifying no interface along with a
	 * multicast address (except for scoped IPv6 addresses), because the
	 * auto-selected interface is likely a public one, and joining the
	 * group will thus create external traffic, which is generally
	 * something we want to avoid in the tests.
	 */
	if ((fd = socket(AF_INET, type, protocol)) < 0) e(0);

	memset(&imr, 0, sizeof(imr));

	/* Basic join-leave combo. */
	imr.imr_multiaddr.s_addr = inet_addr(TEST_MULTICAST_IPV4);
	imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	/* Joining the same multicast group twice is an error. */
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != EEXIST) e(0);

	/* If an interface address is specified, it must match an interface. */
	imr.imr_interface.s_addr = htonl(TEST_BLACKHOLE_IPV4);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	/* The given multicast address must be an actual multicast address. */
	imr.imr_multiaddr.s_addr = htonl(INADDR_ANY);
	imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	imr.imr_multiaddr.s_addr = htonl(INADDR_LOOPBACK);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	/* Leaving a multicast group not joined is an error. */
	imr.imr_multiaddr.s_addr =
	    htonl(ntohl(inet_addr(TEST_MULTICAST_IPV4)) + 1);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	/*
	 * When leaving a group, an interface address need not be specified,
	 * even if one was specified when joining.  As mentioned, we cannot
	 * test joining the same address on multiple interfaces, though.
	 */
	imr.imr_multiaddr.s_addr = inet_addr(TEST_MULTICAST_IPV4);
	imr.imr_interface.s_addr = htonl(INADDR_ANY);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	/* There must be a reasonable per-socket group membership limit. */
	imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);

	for (count = 0; count < IP_MAX_MEMBERSHIPS + 1; count++) {
		imr.imr_multiaddr.s_addr =
		    htonl(ntohl(inet_addr(TEST_MULTICAST_IPV4)) + count);

		r = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
		    sizeof(imr));

		if (r != 0) {
			if (r != -1 || errno != ENOBUFS) e(0);
			break;
		}
	}
	if (count < 8 || count > IP_MAX_MEMBERSHIPS) e(0);

	/* Test leaving a group at the start of the per-socket list. */
	imr.imr_multiaddr.s_addr =
	    htonl(ntohl(inet_addr(TEST_MULTICAST_IPV4)) + count - 1);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	/* Test leaving a group in the middle of the per-socket list. */
	imr.imr_multiaddr.s_addr =
	    htonl(ntohl(inet_addr(TEST_MULTICAST_IPV4)) + count / 2);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	/* Test leaving a group at the end of the per-socket list. */
	imr.imr_multiaddr.s_addr = inet_addr(TEST_MULTICAST_IPV4);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	if (close(fd) != 0) e(0);

	/* Still basic join/leave mechanics.. on to IPv6.. */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&ipv6mr, 0, sizeof(ipv6mr));

	/* Basic join-leave combo. */
	ifindex = if_nametoindex(LOOPBACK_IFNAME);

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	/* Joining the same multicast group twice is an error. */
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != EEXIST) e(0);

	/* If an interface index is specified, it must be valid. */
	ipv6mr.ipv6mr_interface = BAD_IFINDEX;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ENXIO) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ENXIO) e(0);

	ipv6mr.ipv6mr_interface = 0x80000000UL | ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ENXIO) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ENXIO) e(0);

	/* The given multicast address must be an actual multicast address. */
	ipv6mr.ipv6mr_interface = ifindex;
	memcpy(&ipv6mr.ipv6mr_multiaddr, &in6addr_any, sizeof(in6addr_any));

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	memcpy(&ipv6mr.ipv6mr_multiaddr, &in6addr_loopback,
	    sizeof(in6addr_loopback));

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	/* Leaving a multicast group not joined is an error. */
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_multiaddr.s6_addr[15]++;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	/*
	 * When leaving a group, an interface index need not be specified,
	 * even if one was specified when joining.  If one is specified, it
	 * must match, though.  As mentioned, we cannot test joining the same
	 * address on multiple interfaces, though.
	 */
	ipv6mr.ipv6mr_multiaddr.s6_addr[15]--;
	ipv6mr.ipv6mr_interface = ifindex + 1; /* lazy: may or may not exist */

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ENXIO && errno != ESRCH) e(0);

	ipv6mr.ipv6mr_interface = 0;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	/* For link-local addresses, an interface must always be specified. */
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6_LL,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = 0;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	ipv6mr.ipv6mr_interface = 0;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	/* IPv4-mapped IPv6 multicast addresses are currently not supported. */
	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val)) != 0)
		e(0);

	if (inet_pton(AF_INET6, "::ffff:"TEST_MULTICAST_IPV4,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	/*
	 * There must be a reasonable per-socket group membership limit.
	 * Apparently there is no IPv6 equivalent of IP_MAX_MEMBERSHIPS..
	 */
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	for (count = 0; count < IP_MAX_MEMBERSHIPS + 1; count++) {
		r = setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
		    sizeof(ipv6mr));

		if (r != 0) {
			if (r != -1 || errno != ENOBUFS) e(0);
			break;
		}

		ipv6mr.ipv6mr_multiaddr.s6_addr[15]++;
	}
	if (count < 8 || count > IP_MAX_MEMBERSHIPS) e(0);

	/* Test leaving a group at the start of the per-socket list. */
	ipv6mr.ipv6mr_multiaddr.s6_addr[15]--;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	/* Test leaving a group in the middle of the per-socket list. */
	ipv6mr.ipv6mr_multiaddr.s6_addr[15] -= count / 2;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	/* Test leaving a group at the end of the per-socket list. */
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != ESRCH) e(0);

	if (close(fd) != 0) e(0);

	/*
	 * Test sending multicast packets, multicast transmission options, and
	 * basic receipt.  Note that we cannot test IP(V6)_MULTICAST_LOOP
	 * because no extra duplicates are generated on loopback interfaces.
	 */
	if ((fd = socket(AF_INET, type, protocol)) < 0) e(0);

	/* For UDP, get an assigned port number. */
	memset(&sinA, 0, sizeof(sinA));
	sinA.sin_family = AF_INET;

	if (type == SOCK_DGRAM) {
		sinA.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(fd, (struct sockaddr *)&sinA,
		    sizeof(sinA)) != 0) e(0);

		len = sizeof(sinA);
		if (getsockname(fd, (struct sockaddr *)&sinA, &len) != 0) e(0);
	}

	imr.imr_multiaddr.s_addr = inet_addr(TEST_MULTICAST_IPV4);
	imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if ((fd2 = socket(AF_INET, type, protocol)) < 0) e(0);

	/* Regular packet, default unicast TTL, sendto. */
	sinA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (sendto(fd2, "A", 1, 0, (struct sockaddr *)&sinA,
	    sizeof(sinA)) != 1) e(0);

	/* Multicast packet, default multicast TTL, sendto. */
	if (setsockopt(fd2, IPPROTO_IP, IP_MULTICAST_IF, &sinA.sin_addr,
	    sizeof(sinA.sin_addr)) != 0) e(0);

	sinA.sin_addr.s_addr = inet_addr(TEST_MULTICAST_IPV4);

	if (sendto(fd2, "B", 1, 0, (struct sockaddr *)&sinA,
	    sizeof(sinA)) != 1) e(0);

	/* Multicast packet, custom multicast TTL, connect+send. */
	byte = 123;
	if (setsockopt(fd2, IPPROTO_IP, IP_MULTICAST_TTL, &byte,
	    sizeof(byte)) != 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sinA, sizeof(sinA)) != 0) e(0);

	if (send(fd2, "C", 1, 0) != 1) e(0);

	/* Receive and validate what we sent. */
	len = sizeof(sinA);
	if (getsockname(fd2, (struct sockaddr *)&sinA, &len) != 0) e(0);

	len = sizeof(ttl);
	if (getsockopt(fd2, IPPROTO_IP, IP_TTL, &ttl, &len) != 0) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_RECVTTL, &val, sizeof(val)) != 0)
		e(0);

	hdrlen = (type == SOCK_RAW) ? sizeof(struct ip) : 0;

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = hdrlen + 1;

	for (i = 0; i < 3; ) {
		memset(&msg, 0, sizeof(msg));
		msg.msg_name = &sinB;
		msg.msg_namelen = sizeof(sinB);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control.buf;
		msg.msg_controllen = sizeof(control);

		r = recvmsg(fd, &msg, 0);
		if (r < 0) e(0);

		if (msg.msg_namelen != sizeof(sinB)) e(0);

		/*
		 * There is a tiny possibility that we receive other packets
		 * on the receiving socket, as it is not bound to a particular
		 * address, and there is currently no way to bind a socket to
		 * a particular interface.  We therefore skip packets not from
		 * the sending socket, conveniently testing the accuracy of the
		 * reported source address as a side effect.
		 */
		if (memcmp(&sinA, &sinB, sizeof(sinA)))
			continue;

		if (r != hdrlen + 1) e(0);
		if (buf[hdrlen] != 'A' + i) e(0);

		if (msg.msg_flags & MSG_BCAST) e(0);

		if ((cmsg = CMSG_FIRSTHDR(&msg)) == NULL) e(0);
		if (cmsg->cmsg_level != IPPROTO_IP) e(0);
		if (cmsg->cmsg_type != IP_TTL) e(0);
		if (cmsg->cmsg_len != CMSG_LEN(sizeof(byte))) e(0);
		memcpy(&byte, CMSG_DATA(cmsg), sizeof(byte));

		switch (i) {
		case 0:
			if (msg.msg_flags & MSG_MCAST) e(0);
			if (byte != ttl) e(0);
			break;
		case 1:
			if (!(msg.msg_flags & MSG_MCAST)) e(0);
			if (byte != 1) e(0);
			break;
		case 2:
			if (!(msg.msg_flags & MSG_MCAST)) e(0);
			if (byte != 123) e(0);
			break;
		}

		i++;
	}

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* Still the send tests, but now IPv6.. */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	/* For UDP, get an assigned port number. */
	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;

	if (type == SOCK_DGRAM) {
		if (bind(fd, (struct sockaddr *)&sin6A,
		    sizeof(sin6A)) != 0) e(0);

		len = sizeof(sin6A);
		if (getsockname(fd, (struct sockaddr *)&sin6A, &len) != 0)
			e(0);
	}

	memcpy(&sin6B, &sin6A, sizeof(sin6B));

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	/* Regular packet, default unicast TTL, sendto. */
	if (inet_pton(AF_INET6, LOOPBACK_IPV6_LL, &sin6A.sin6_addr) != 1) e(0);
	sin6A.sin6_scope_id = ifindex;

	if (sendto(fd2, "D", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	/* Multicast packet, default multicast TTL, sendto. */
	val = (int)ifindex;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &sin6A.sin6_addr) != 1) e(0);
	sin6A.sin6_scope_id = 0;

	if (sendto(fd2, "E", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	/* Multicast packet, custom multicast TTL, connect+send. */
	val = 125;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val,
	    sizeof(val)) != 0) e(0);

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (send(fd2, "F", 1, 0) != 1) e(0);

	len = sizeof(sin6A);
	if (getsockname(fd2, (struct sockaddr *)&sin6A, &len) != 0) e(0);

	/*
	 * Repeat the last two tests, but now with a link-local multicast
	 * address.  In particular link-local destination addresses do not need
	 * a zone ID, and the system should be smart enough to pick the right
	 * zone ID if an outgoing multicast interface is configured.  Zone
	 * violations should be detected and result in errors.
	 */
	if (close(fd2) != 0) e(0);

	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	if (bind(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	memcpy(&sin6A, &sin6B, sizeof(sin6A));

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6_LL,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	/* Link-local multicast packet, sendto. */
	val = (int)ifindex;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6_LL,
	    &sin6A.sin6_addr) != 1) e(0);
	sin6A.sin6_scope_id = 0;

	if (sendto(fd2, "G", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	sin6A.sin6_scope_id = ifindex + 1; /* lazy: may or may not be valid */

	if (sendto(fd2, "X", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != -1) e(0);
	if (errno != ENXIO && errno != EHOSTUNREACH) e(0);

	sin6A.sin6_scope_id = ifindex;

	if (sendto(fd2, "H", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	/* Link-local multicast packet, connect+send. */
	sin6A.sin6_scope_id = 0;

	if (connect(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (send(fd2, "I", 1, 0) != 1) e(0);

	len = sizeof(sin6A);
	if (getsockname(fd2, (struct sockaddr *)&sin6A, &len) != 0) e(0);

	/* Receive and validate what we sent. */
	len = sizeof(val);
	if (getsockopt(fd2, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &val,
	    &len) != 0) e(0);
	ttl = (uint8_t)val;

	val = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &val,
	    sizeof(val)) != 0) e(0);

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = 1;

	for (i = 0; i < 6; ) {
		memset(&msg, 0, sizeof(msg));
		msg.msg_name = &sin6B;
		msg.msg_namelen = sizeof(sin6B);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control.buf;
		msg.msg_controllen = sizeof(control);

		r = recvmsg(fd, &msg, 0);
		if (r < 0) e(0);

		if (msg.msg_namelen != sizeof(sin6B)) e(0);

		if (memcmp(&sin6A, &sin6B, sizeof(sin6A)))
			continue;

		if (r != 1) e(0);
		if (buf[0] != 'D' + i) e(0);

		if (msg.msg_flags & MSG_BCAST) e(0);

		if ((cmsg = CMSG_FIRSTHDR(&msg)) == NULL) e(0);
		if (cmsg->cmsg_level != IPPROTO_IPV6) e(0);
		if (cmsg->cmsg_type != IPV6_HOPLIMIT) e(0);
		if (cmsg->cmsg_len != CMSG_LEN(sizeof(val))) e(0);
		memcpy(&val, CMSG_DATA(cmsg), sizeof(val));

		switch (i) {
		case 0:
			if (msg.msg_flags & MSG_MCAST) e(0);
			if (val != (int)ttl) e(0);
			break;
		case 1:
		case 3:
		case 4:
		case 5:
			if (!(msg.msg_flags & MSG_MCAST)) e(0);
			if (val != 1) e(0);
			break;
		case 2:
			if (!(msg.msg_flags & MSG_MCAST)) e(0);
			if (val != 125) e(0);
			break;
		}

		i++;
	}

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/*
	 * Test receiving multicast packets on a bound socket.  We have already
	 * tested receiving packets on an unbound socket, so we need not
	 * incorporate that into this test as well.
	 */
	memset(sin_array, 0, sizeof(sin_array));
	sin_array[0].sin_family = AF_INET;
	sin_array[0].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin_array[1].sin_family = AF_INET;
	sin_array[1].sin_addr.s_addr = inet_addr(TEST_MULTICAST_IPV4);
	sin_array[2].sin_family = AF_INET;
	sin_array[2].sin_addr.s_addr =
	    htonl(ntohl(sin_array[1].sin_addr.s_addr) + 1);

	for (i = 0; i < __arraycount(sin_array); i++) {
		if ((fd = socket(AF_INET, type, protocol)) < 0) e(0);

		if (bind(fd, (struct sockaddr *)&sin_array[i],
		    sizeof(sin_array[i])) != 0) e(0);

		imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);
		memcpy(&imr.imr_multiaddr, &sin_array[1].sin_addr,
		    sizeof(imr.imr_multiaddr));
		if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
		    sizeof(imr)) != 0) e(0);

		memcpy(&imr.imr_multiaddr, &sin_array[2].sin_addr,
		    sizeof(imr.imr_multiaddr));
		if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
		    sizeof(imr)) != 0) e(0);

		len = sizeof(sinA);
		if (getsockname(fd, (struct sockaddr *)&sinA, &len) != 0) e(0);

		if ((fd2 = socket(AF_INET, type, protocol)) < 0) e(0);

		if (setsockopt(fd2, IPPROTO_IP, IP_MULTICAST_IF,
		    &imr.imr_interface, sizeof(imr.imr_interface)) != 0) e(0);

		for (j = 0; j < __arraycount(sin_array); j++) {
			memcpy(&sinA.sin_addr, &sin_array[j].sin_addr,
			    sizeof(sinA.sin_addr));

			byte = 'A' + j;
			if (sendto(fd2, &byte, sizeof(byte), 0,
			    (struct sockaddr *)&sinA,
			    sizeof(sinA)) != sizeof(byte)) e(0);
		}

		if (recv(fd, buf, sizeof(buf), 0) !=
		    hdrlen + sizeof(byte)) e(0);
		if (buf[hdrlen] != 'A' + i) e(0);

		if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);

		if (close(fd2) != 0) e(0);
		if (close(fd) != 0) e(0);
	}

	/* Still testing receiving on bound sockets, now IPv6.. */
	memset(sin6_array, 0, sizeof(sin6_array));
	sin6_array[0].sin6_family = AF_INET6;
	memcpy(&sin6_array[0].sin6_addr, &in6addr_loopback,
	    sizeof(sin6_array[0].sin6_addr));
	sin6_array[1].sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &sin6_array[1].sin6_addr) != 1) e(0);
	sin6_array[2].sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6_LL,
	    &sin6_array[2].sin6_addr) != 1) e(0);

	/*
	 * As with unicast addresses, binding to link-local multicast addresses
	 * requires a proper zone ID.
	 */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	if (bind(fd, (struct sockaddr *)&sin6_array[2],
	    sizeof(sin6_array[2])) != -1) e(0);
	if (errno != EADDRNOTAVAIL) e(0);

	sin6_array[2].sin6_scope_id = BAD_IFINDEX;

	if (bind(fd, (struct sockaddr *)&sin6_array[2],
	    sizeof(sin6_array[2])) != -1) e(0);
	if (errno != ENXIO) e(0);

	sin6_array[2].sin6_scope_id = ifindex;

	if (close(fd) != 0) e(0);

	for (i = 0; i < __arraycount(sin6_array); i++) {
		if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

		if (bind(fd, (struct sockaddr *)&sin6_array[i],
		    sizeof(sin6_array[i])) != 0) e(0);

		ipv6mr.ipv6mr_interface = ifindex;
		memcpy(&ipv6mr.ipv6mr_multiaddr, &sin6_array[1].sin6_addr,
		    sizeof(ipv6mr.ipv6mr_multiaddr));
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
		    sizeof(ipv6mr)) != 0) e(0);

		memcpy(&ipv6mr.ipv6mr_multiaddr, &sin6_array[2].sin6_addr,
		    sizeof(ipv6mr.ipv6mr_multiaddr));
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
		    sizeof(ipv6mr)) != 0) e(0);

		len = sizeof(sin6A);
		if (getsockname(fd, (struct sockaddr *)&sin6A,
		    &len) != 0) e(0);

		if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

		val = (int)ifindex;
		if (setsockopt(fd2, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
		    sizeof(val)) != 0) e(0);

		for (j = 0; j < __arraycount(sin6_array); j++) {
			memcpy(&sin6A.sin6_addr, &sin6_array[j].sin6_addr,
			    sizeof(sin6A.sin6_addr));

			byte = 'A' + j;
			if (sendto(fd2, &byte, sizeof(byte), 0,
			    (struct sockaddr *)&sin6A,
			    sizeof(sin6A)) != sizeof(byte)) e(0);
		}

		if (recv(fd, buf, sizeof(buf), 0) != sizeof(byte)) e(0);
		if (buf[0] != 'A' + i) e(0);

		if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);

		if (close(fd2) != 0) e(0);
		if (close(fd) != 0) e(0);
	}

	/*
	 * Now test *sending* on a socket bound to a multicast address.  The
	 * multicast address must not show up as the packet's source address.
	 * No actual multicast groups are involved here.
	 */
	if ((fd = socket(AF_INET, type, protocol)) < 0) e(0);

	if (bind(fd, (struct sockaddr *)&sin_array[1],
	    sizeof(sin_array[1])) != 0) e(0);

	if ((fd2 = socket(AF_INET, type, protocol)) < 0) e(0);

	if (bind(fd2, (struct sockaddr *)&sin_array[0],
	    sizeof(sin_array[0])) != 0) e(0);

	len = sizeof(sinA);
	if (getsockname(fd2, (struct sockaddr *)&sinA, &len) != 0) e(0);

	if (sendto(fd, "D", 1, 0, (struct sockaddr *)&sinA, sizeof(sinA)) != 1)
		e(0);

	len = sizeof(sinB);
	if (recvfrom(fd2, buf, sizeof(buf), 0, (struct sockaddr *)&sinB,
	    &len) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'D') e(0);

	if (sinB.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* Sending from a bound socket, IPv6 version.. */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	if (bind(fd, (struct sockaddr *)&sin6_array[1],
	    sizeof(sin6_array[1])) != 0) e(0);

	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	if (bind(fd2, (struct sockaddr *)&sin6_array[0],
	    sizeof(sin6_array[0])) != 0) e(0);

	len = sizeof(sin6A);
	if (getsockname(fd2, (struct sockaddr *)&sin6A, &len) != 0) e(0);

	if (sendto(fd, "E", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	len = sizeof(sin6B);
	if (recvfrom(fd2, buf, sizeof(buf), 0, (struct sockaddr *)&sin6B,
	    &len) != 1) e(0);
	if (buf[0] != 'E') e(0);

	if (!IN6_IS_ADDR_LOOPBACK(&sin6B.sin6_addr)) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/*
	 * A quick, partial test to see if connecting to a particular address
	 * does not accidentally block packet receipt.  What we do not test is
	 * whether connecting does filter traffic from other sources.
	 */
	if ((fd = socket(AF_INET, type, protocol)) < 0) e(0);

	memset(&sinA, 0, sizeof(sinA));
	sinA.sin_family = AF_INET;
	sinA.sin_addr.s_addr = inet_addr(TEST_MULTICAST_IPV4);

	if (bind(fd, (struct sockaddr *)&sinA, sizeof(sinA)) != 0) e(0);

	len = sizeof(sinA);
	if (getsockname(fd, (struct sockaddr *)&sinA, &len) != 0) e(0);

	imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);
	imr.imr_multiaddr.s_addr = sinA.sin_addr.s_addr;
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if ((fd2 = socket(AF_INET, type, protocol)) < 0) e(0);

	memset(&sinB, 0, sizeof(sinB));
	sinB.sin_family = AF_INET;
	sinB.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(fd2, (struct sockaddr *)&sinB, sizeof(sinB)) != 0) e(0);

	len = sizeof(sinB);
	if (getsockname(fd2, (struct sockaddr *)&sinB, &len) != 0) e(0);

	if (connect(fd, (struct sockaddr *)&sinB, sizeof(sinB)) != 0) e(0);

	/* Note that binding to a particular source address is not enough! */
	if (setsockopt(fd2, IPPROTO_IP, IP_MULTICAST_IF, &imr.imr_interface,
	    sizeof(imr.imr_interface)) != 0) e(0);

	if (sendto(fd2, "F", 1, 0, (struct sockaddr *)&sinA,
	    sizeof(sinA)) != 1) e(0);

	if (recv(fd, buf, sizeof(buf), 0) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'F') e(0);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);

	/* Also try connecting with IPv6. */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6_LL,
	    &sin6A.sin6_addr) != 1) e(0);
	sin6A.sin6_scope_id = ifindex;

	if (bind(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	len = sizeof(sin6A);
	if (getsockname(fd, (struct sockaddr *)&sin6A, &len) != 0) e(0);

	ipv6mr.ipv6mr_interface = ifindex;
	memcpy(&ipv6mr.ipv6mr_multiaddr, &sin6A.sin6_addr,
	    sizeof(ipv6mr.ipv6mr_multiaddr));
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&sin6B, 0, sizeof(sin6B));
	sin6B.sin6_family = AF_INET6;
	memcpy(&sin6B.sin6_addr, &in6addr_loopback, sizeof(sin6B.sin6_addr));

	if (bind(fd2, (struct sockaddr *)&sin6B, sizeof(sin6B)) != 0) e(0);

	len = sizeof(sin6B);
	if (getsockname(fd2, (struct sockaddr *)&sin6B, &len) != 0) e(0);

	if (connect(fd, (struct sockaddr *)&sin6B, sizeof(sin6B)) != 0) e(0);

	/* Unlike with IPv4, here the interface is implied by the zone. */
	if (sendto(fd2, "G", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'G') e(0);

	if (close(fd) != 0) e(0);
	if (close(fd2) != 0) e(0);

	/*
	 * Test multiple receivers.  For UDP, we need to set the SO_REUSEADDR
	 * option on all sockets for this to be guaranteed to work.
	 */
	if ((fd = socket(AF_INET, type, protocol)) < 0) e(0);
	if ((fd2 = socket(AF_INET, type, protocol)) < 0) e(0);

	memset(&sinA, 0, sizeof(sinA));
	sinA.sin_family = AF_INET;

	if (type == SOCK_DGRAM) {
		if (bind(fd, (struct sockaddr *)&sinA, sizeof(sinA)) != 0)
			e(0);

		len = sizeof(sinA);
		if (getsockname(fd, (struct sockaddr *)&sinA, &len) != 0)
			e(0);

		val = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (bind(fd2, (struct sockaddr *)&sinA, sizeof(sinA)) != 0)
			e(0);
	}

	imr.imr_multiaddr.s_addr = inet_addr(TEST_MULTICAST_IPV4);
	imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd2, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &imr.imr_interface,
	    sizeof(imr.imr_interface)) != 0) e(0);

	sinA.sin_addr.s_addr = imr.imr_multiaddr.s_addr;

	if (sendto(fd, "H", 1, 0, (struct sockaddr *)&sinA, sizeof(sinA)) != 1)
		e(0);

	if (recv(fd, buf, sizeof(buf), 0) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'H') e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'H') e(0);

	/*
	 * Also test with a larger buffer, to ensure that packet duplication
	 * actually works properly.  As of writing, we need to patch lwIP to
	 * make this work at all.
	 */
	len = 8000;
	if ((buf2 = malloc(hdrlen + len + 1)) == NULL) e(0);
	buf2[len - 1] = 'I';

	if (sendto(fd, buf2, len, 0, (struct sockaddr *)&sinA,
	    sizeof(sinA)) != len) e(0);

	buf2[hdrlen + len - 1] = '\0';
	if (recv(fd, buf2, hdrlen + len + 1, 0) != hdrlen + len) e(0);
	if (buf2[hdrlen + len - 1] != 'I') e(0);

	buf2[hdrlen + len - 1] = '\0';
	if (recv(fd2, buf2, hdrlen + len + 1, 0) != hdrlen + len) e(0);
	if (buf2[hdrlen + len - 1] != 'I') e(0);

	free(buf2);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* Multiple-receivers test, IPv6 version. */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);
	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;

	if (type == SOCK_DGRAM) {
		if (bind(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0)
			e(0);

		len = sizeof(sin6A);
		if (getsockname(fd, (struct sockaddr *)&sin6A, &len) != 0)
			e(0);

		val = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (bind(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0)
			e(0);
	}

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	val = (int)ifindex;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	memcpy(&sin6A.sin6_addr, &ipv6mr.ipv6mr_multiaddr,
	    sizeof(sin6A.sin6_addr));

	if (sendto(fd, "J", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'J') e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'J') e(0);

	len = 8000;
	if ((buf2 = malloc(len + 1)) == NULL) e(0);
	buf2[len - 1] = 'K';

	if (sendto(fd, buf2, len, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != len) e(0);

	buf2[len - 1] = '\0';
	if (recv(fd, buf2, len + 1, 0) != len) e(0);
	if (buf2[len - 1] != 'K') e(0);

	buf2[len - 1] = '\0';
	if (recv(fd2, buf2, len + 1, 0) != len) e(0);
	if (buf2[len - 1] != 'K') e(0);

	free(buf2);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/*
	 * Test proper multicast group departure.  This test relies on the fact
	 * that actual group membership is not checked on arrival of a
	 * multicast-destined packet, so that membership of one socket can be
	 * tested by another socket sending packets to itself while having
	 * joined a different group.  We test both explicit group departure
	 * and implicit departure on close.
	 */
	if ((fd = socket(AF_INET, type, protocol)) < 0) e(0);
	if ((fd2 = socket(AF_INET, type, protocol)) < 0) e(0);

	memset(&sinA, 0, sizeof(sinA));
	sinA.sin_family = AF_INET;

	if (type == SOCK_DGRAM) {
		if (bind(fd, (struct sockaddr *)&sinA, sizeof(sinA)) != 0)
			e(0);

		len = sizeof(sinA);
		if (getsockname(fd, (struct sockaddr *)&sinA, &len) != 0)
			e(0);

		val = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (bind(fd2, (struct sockaddr *)&sinA, sizeof(sinA)) != 0)
			e(0);
	}

	imr.imr_multiaddr.s_addr = inet_addr(TEST_MULTICAST_IPV4);
	imr.imr_interface.s_addr = htonl(INADDR_LOOPBACK);

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	imr.imr_multiaddr.s_addr = htonl(ntohl(imr.imr_multiaddr.s_addr) + 1);

	if (setsockopt(fd2, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &imr.imr_interface,
	    sizeof(imr.imr_interface)) != 0) e(0);

	sinA.sin_addr.s_addr = imr.imr_multiaddr.s_addr;

	if (sendto(fd, "L", 1, 0, (struct sockaddr *)&sinA, sizeof(sinA)) != 1)
		e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'L') e(0);

	if (setsockopt(fd2, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (sendto(fd, "M", 1, 0, (struct sockaddr *)&sinA, sizeof(sinA)) != 1)
		e(0);

	if (recv(fd, buf, sizeof(buf), 0) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'L') e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (setsockopt(fd2, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr,
	    sizeof(imr)) != 0) e(0);

	if (sendto(fd, "N", 1, 0, (struct sockaddr *)&sinA, sizeof(sinA)) != 1)
		e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'N') e(0);

	if (close(fd2) != 0) e(0);

	if (sendto(fd, "O", 1, 0, (struct sockaddr *)&sinA, sizeof(sinA)) != 1)
		e(0);

	if (recv(fd, buf, sizeof(buf), 0) != hdrlen + 1) e(0);
	if (buf[hdrlen] != 'N') e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (close(fd) != 0) e(0);

	/* Multicast group departure, now IPv6.. this is getting boring. */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);
	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;

	if (type == SOCK_DGRAM) {
		if (bind(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0)
			e(0);

		len = sizeof(sin6A);
		if (getsockname(fd, (struct sockaddr *)&sin6A, &len) != 0)
			e(0);

		val = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (setsockopt(fd2, SOL_SOCKET, SO_REUSEADDR, &val,
		    sizeof(val)) != 0) e(0);

		if (bind(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0)
			e(0);
	}

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	ipv6mr.ipv6mr_multiaddr.s6_addr[15]++;

	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	val = (int)ifindex;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	memcpy(&sin6A.sin6_addr, &ipv6mr.ipv6mr_multiaddr,
	    sizeof(sin6A.sin6_addr));

	if (sendto(fd, "P", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'P') e(0);

	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (sendto(fd, "Q", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'P') e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if (sendto(fd, "R", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	if (recv(fd2, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'R') e(0);

	if (close(fd2) != 0) e(0);

	if (sendto(fd, "S", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'R') e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (close(fd) != 0) e(0);

	/*
	 * Lastly, some IPv6-only tests.
	 */
	/*
	 * Test that IPV6_PKTINFO overrides IPV6_MULTICAST_IF.  For this we
	 * need two valid interface indices.  If we cannot find a second one,
	 * simply test that the IPV6_PKTINFO information is used at all.
	 */
	for (ifindex2 = 1; ifindex2 < BAD_IFINDEX; ifindex2++) {
		if (if_indextoname(ifindex2, name) == NULL) {
			if (errno != ENXIO) e(0);
			continue;
		}

		if (strcmp(name, LOOPBACK_IFNAME))
			break;
	}

	if (ifindex2 == BAD_IFINDEX)
		ifindex2 = 0; /* too bad; fallback mode */

	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;

	if (type == SOCK_DGRAM) {
		if (bind(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0)
			e(0);

		len = sizeof(sin6A);
		if (getsockname(fd, (struct sockaddr *)&sin6A, &len) != 0)
			e(0);
	}

	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6_LL,
	    &ipv6mr.ipv6mr_multiaddr) != 1) e(0);
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != 0) e(0);

	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	memcpy(&sin6A.sin6_addr, &ipv6mr.ipv6mr_multiaddr,
	    sizeof(sin6A.sin6_addr));

	val = (int)ifindex2;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = "T";
	iov.iov_len = 1;

	memset(&ipi6, 0, sizeof(ipi6));
	memcpy(&ipi6.ipi6_addr, &in6addr_loopback, sizeof(ipi6.ipi6_addr));
	ipi6.ipi6_ifindex = ifindex;

	control.cmsg.cmsg_len = CMSG_LEN(sizeof(ipi6));
	control.cmsg.cmsg_level = IPPROTO_IPV6;
	control.cmsg.cmsg_type = IPV6_PKTINFO;
	memcpy(CMSG_DATA(&control.cmsg), &ipi6, sizeof(ipi6));

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &sin6A;
	msg.msg_namelen = sizeof(sin6A);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.buf;
	msg.msg_controllen = control.cmsg.cmsg_len;

	if (sendmsg(fd2, &msg, 0) != 1) e(0);

	len = sizeof(sin6B);
	if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin6B,
	    &len) != 1) e(0);
	if (buf[0] != 'T') e(0);

	if (len != sizeof(sin6B)) e(0);
	if (sin6B.sin6_len != sizeof(sin6B)) e(0);
	if (sin6B.sin6_family != AF_INET6) e(0);
	if (memcmp(&sin6B.sin6_addr, &in6addr_loopback,
	    sizeof(sin6B.sin6_addr)) != 0) e(0);

	if (close(fd2) != 0) e(0);

	/* Repeat the same test, but now with a sticky IPV6_PKTINFO setting. */
	if ((fd2 = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&ipi6, 0, sizeof(ipi6));
	memcpy(&ipi6.ipi6_addr, &in6addr_loopback, sizeof(ipi6.ipi6_addr));
	ipi6.ipi6_ifindex = ifindex;

	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_PKTINFO, &ipi6,
	    sizeof(ipi6)) != 0) e(0);

	val = (int)ifindex2;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_MULTICAST_IF, &val,
	    sizeof(val)) != 0) e(0);

	if (sendto(fd2, "U", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	len = sizeof(sin6B);
	if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin6B,
	    &len) != 1) e(0);
	if (buf[0] != 'U') e(0);

	if (len != sizeof(sin6B)) e(0);
	if (sin6B.sin6_len != sizeof(sin6B)) e(0);
	if (sin6B.sin6_family != AF_INET6) e(0);
	if (memcmp(&sin6B.sin6_addr, &in6addr_loopback,
	    sizeof(sin6B.sin6_addr)) != 0) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/*
	 * Test that invalid multicast addresses are not accepted anywhere.
	 */
	if ((fd = socket(AF_INET6, type, protocol)) < 0) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, TEST_MULTICAST_IPV6_BAD,
	    &sin6A.sin6_addr) != 1) e(0);

	if (bind(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != -1) e(0);
	if (errno != EINVAL) e(0);

	sin6A.sin6_port = htons(TEST_PORT_A);
	if (connect(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (sendto(fd, "X", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&ipv6mr.ipv6mr_multiaddr, &sin6A.sin6_addr,
	    sizeof(ipv6mr.ipv6mr_multiaddr));
	ipv6mr.ipv6mr_interface = ifindex;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mr,
	    sizeof(ipv6mr)) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (close(fd) != 0) e(0);
}
