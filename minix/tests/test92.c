/* Tests for RAW sockets (LWIP) - by D.C. van Moolenbroek */
/* This test needs to be run as root: creating raw sockets is root-only. */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/icmp6.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#include <arpa/inet.h>
#include <machine/vmparam.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "common.h"
#include "socklib.h"

#define ITERATIONS	2

#define TEST_PROTO		253	/* from RFC 3692 */
#define TEST_ICMPV6_TYPE_A	200	/* from RFC 4443 */
#define TEST_ICMPV6_TYPE_B	201	/* from RFC 4443 */

static const enum state raw_states[] = {
		S_NEW,		S_N_SHUT_R,	S_N_SHUT_W,	S_N_SHUT_RW,
		S_BOUND,	S_CONNECTED,	S_SHUT_R,	S_SHUT_W,
		S_SHUT_RW,
};

static const int raw_results[][__arraycount(raw_states)] = {
	[C_ACCEPT]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,
	},
	[C_BIND]		= {
		0,		0,		0,		0,
		0,		-EINVAL,	-EINVAL,	-EINVAL,
		-EINVAL,
	},
	[C_CONNECT]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_GETPEERNAME]		= {
		-ENOTCONN,	-ENOTCONN,	-ENOTCONN,	-ENOTCONN,
		-ENOTCONN,	0,		0,		0,
		0,
	},
	[C_GETSOCKNAME]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_GETSOCKOPT_ERR]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_GETSOCKOPT_KA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_GETSOCKOPT_RB]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_IOCTL_NREAD]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_LISTEN]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
		-EOPNOTSUPP,
	},
	[C_RECV]		= {
		-EAGAIN,	0,		-EAGAIN,	0,
		-EAGAIN,	-EAGAIN,	0,		-EAGAIN,
		0,
	},
	[C_RECVFROM]		= {
		-EAGAIN,	0,		-EAGAIN,	0,
		-EAGAIN,	-EAGAIN,	0,		-EAGAIN,
		0,
	},
	[C_SEND]		= {
		-EDESTADDRREQ,	-EDESTADDRREQ,	-EPIPE,		-EPIPE,
		-EDESTADDRREQ,	1,		1,		-EPIPE,
		-EPIPE,
	},
	[C_SENDTO]		= {
		1,		1,		-EPIPE,		-EPIPE,
		1,		1,		1,		-EPIPE,
		-EPIPE,
	},
	[C_SELECT_R]		= {
		0,		1,		0,		1,
		0,		0,		1,		0,
		1,
	},
	[C_SELECT_W]		= {
		1,		1,		1,		1,
		1,		1,		1,		1,
		1,
	},
	[C_SELECT_X]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_SETSOCKOPT_BC]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_SETSOCKOPT_KA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_SETSOCKOPT_L]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_SETSOCKOPT_RA]	= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_SHUTDOWN_R]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_SHUTDOWN_RW]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
	[C_SHUTDOWN_W]		= {
		0,		0,		0,		0,
		0,		0,		0,		0,
		0,
	},
};

/*
 * Set up a RAW socket file descriptor in the requested state and pass it to
 * socklib_sweep_call() along with local and remote addresses and their length.
 */
static int
raw_sweep(int domain, int type, int protocol, enum state state,
	enum call call)
{
	struct sockaddr_in sinA, sinB;
	struct sockaddr_in6 sin6A, sin6B;
	struct sockaddr *addrA, *addrB;
	socklen_t addr_len;
	int r, fd, fd2;

	if (domain == AF_INET) {
		memset(&sinA, 0, sizeof(sinA));
		sinA.sin_family = domain;
		sinA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		memcpy(&sinB, &sinA, sizeof(sinB));

		addrA = (struct sockaddr *)&sinA;
		addrB = (struct sockaddr *)&sinB;
		addr_len = sizeof(sinA);
	} else {
		assert(domain == AF_INET6);

		memset(&sin6A, 0, sizeof(sin6A));
		sin6A.sin6_family = domain;
		memcpy(&sin6A.sin6_addr, &in6addr_loopback,
		    sizeof(sin6A.sin6_addr));

		memcpy(&sin6B, &sin6A, sizeof(sin6B));

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
		if ((fd = socket(domain, type | SOCK_NONBLOCK,
		    protocol)) < 0) e(0);

		if (bind(fd, addrA, addr_len) != 0) e(0);

		if (state == S_BOUND)
			break;

		if (connect(fd, addrB, addr_len) != 0) e(0);

		switch (state) {
		case S_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
		case S_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
		case S_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
		default: break;
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
 * Sweep test for socket calls versus socket states of RAW sockets.
 */
static void
test92a(void)
{

	subtest = 1;

	socklib_sweep(AF_INET, SOCK_RAW, TEST_PROTO, raw_states,
	    __arraycount(raw_states), (const int *)raw_results, raw_sweep);

	socklib_sweep(AF_INET6, SOCK_RAW, TEST_PROTO, raw_states,
	    __arraycount(raw_states), (const int *)raw_results, raw_sweep);
}

/*
 * Basic I/O test for raw sockets.
 */
static void
test92b(void)
{
	struct sockaddr_in sinA, sinB, sinC;
	struct sockaddr_in6 sin6A, sin6B, sin6C;
	socklen_t len;
	unsigned int i;
	uint8_t buf[256], packet[5];
	int fd, fd2;

	subtest = 2;

	/* First test IPv4. */
	if ((fd = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	memset(&sinA, 0, sizeof(sinA));
	sinA.sin_family = AF_INET;
	sinA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	for (i = 0; i < __arraycount(packet); i++)
		packet[i] = (uint8_t)(-i);

	if (sendto(fd, packet, sizeof(packet), 0, (struct sockaddr *)&sinA,
	    sizeof(sinA)) != sizeof(packet)) e(0);

	memset(buf, 0, sizeof(buf));
	len = sizeof(sinB);
	if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sinB,
	    &len) != sizeof(struct ip) + sizeof(packet)) e(0);

	if (memcmp(&buf[sizeof(struct ip)], packet, sizeof(packet)) != 0) e(0);

	if (len != sizeof(sinB)) e(0);
	if (sinB.sin_len != sizeof(sinB)) e(0);
	if (sinB.sin_family != AF_INET) e(0);
	if (sinB.sin_port != htons(0)) e(0);
	if (sinB.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) e(0);

	/*
	 * Test two additional things:
	 *
	 * 1) a non-zero port number is ignored when sending;
	 * 2) multiple raw sockets may receive the same packet.
	 */
	sinA.sin_port = htons(22);

	if ((fd2 = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	if (sendto(fd, packet, sizeof(packet), 0, (struct sockaddr *)&sinA,
	    sizeof(sinA)) != sizeof(packet)) e(0);

	memset(buf, 0, sizeof(buf));
	len = sizeof(sinC);
	if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sinC,
	    &len) != sizeof(struct ip) + sizeof(packet)) e(0);

	if (memcmp(&buf[sizeof(struct ip)], packet, sizeof(packet)) != 0) e(0);

	if (len != sizeof(sinC)) e(0);
	if (memcmp(&sinB, &sinC, sizeof(sinB)) != 0) e(0);

	memset(buf, 0, sizeof(buf));
	len = sizeof(sinC);
	if (recvfrom(fd2, buf, sizeof(buf), 0, (struct sockaddr *)&sinC,
	    &len) != sizeof(struct ip) + sizeof(packet)) e(0);

	if (memcmp(&buf[sizeof(struct ip)], packet, sizeof(packet)) != 0) e(0);

	if (len != sizeof(sinC)) e(0);
	if (memcmp(&sinB, &sinC, sizeof(sinB)) != 0) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* Then test IPv6. */
	if ((fd = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;
	memcpy(&sin6A.sin6_addr, &in6addr_loopback, sizeof(sin6A.sin6_addr));

	if (sendto(fd, packet, sizeof(packet), 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != sizeof(packet)) e(0);

	memset(buf, 0, sizeof(buf));
	len = sizeof(sin6B);
	if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin6B,
	    &len) != sizeof(packet)) e(0);

	if (memcmp(buf, packet, sizeof(packet)) != 0) e(0);

	if (len != sizeof(sin6B)) e(0);
	if (sin6B.sin6_len != sizeof(sin6B)) e(0);
	if (sin6B.sin6_family != AF_INET6) e(0);
	if (sin6B.sin6_port != htons(0)) e(0);
	if (memcmp(&sin6B.sin6_addr, &in6addr_loopback,
	    sizeof(sin6B.sin6_addr)) != 0) e(0);

	/* As above. */
	sin6A.sin6_port = htons(22);

	if ((fd2 = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	if (sendto(fd, packet, sizeof(packet), 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != sizeof(packet)) e(0);

	memset(buf, 0, sizeof(buf));
	len = sizeof(sin6C);
	if (recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin6C,
	    &len) != sizeof(packet)) e(0);

	if (memcmp(buf, packet, sizeof(packet)) != 0) e(0);

	if (len != sizeof(sin6C)) e(0);
	if (memcmp(&sin6B, &sin6C, sizeof(sin6B)) != 0) e(0);

	memset(buf, 0, sizeof(buf));
	len = sizeof(sin6C);
	if (recvfrom(fd2, buf, sizeof(buf), 0, (struct sockaddr *)&sin6C,
	    &len) != sizeof(packet)) e(0);

	if (memcmp(buf, packet, sizeof(packet)) != 0) e(0);

	if (len != sizeof(sin6C)) e(0);
	if (memcmp(&sin6B, &sin6C, sizeof(sin6B)) != 0) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);
}

/*
 * Test the IPV6_CHECKSUM socket option.
 */
static void
test92c(void)
{
	struct sockaddr_in6 sin6;
	struct icmp6_hdr icmp6_hdr;
	uint8_t buf[6], buf2[6], *buf3;
	socklen_t len;
	unsigned int i;
	int fd, fd2, val;

	subtest = 3;

	if ((fd = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	if (shutdown(fd, SHUT_RD) != 0) e(0);

	/* For non-ICMPv6 sockets, checksumming is disabled by default. */
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != -1) e(0);

	/* Test bad offsets. */
	val = -2;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* Now test real checksum computation. */
	val = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	if (shutdown(fd2, SHUT_WR) != 0) e(0);

	memset(buf, 0, sizeof(buf));
	buf[2] = 0xfe;
	buf[3] = 0x95;
	buf[4] = 0x4d;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	if (sendto(fd, buf, 5, 0, (struct sockaddr *)&sin6, sizeof(sin6)) != 5)
		e(0);

	if (recv(fd2, buf2, sizeof(buf2), 0) != 5) e(0);

	if (buf2[0] != 0xb3 || buf2[1] != 0x65) e(0);
	if (memcmp(&buf2[2], &buf[2], 3) != 0) e(0);

	/* Turn on checksum verification on the receiving socket. */
	val = 0;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	/*
	 * The current value of the checksum field should not be incorporated
	 * in the checksum, as that would result in an invalid checksum.
	 */
	buf[0] = 0xab;
	buf[1] = 0xcd;

	if (sendto(fd, buf, 5, 0, (struct sockaddr *)&sin6, sizeof(sin6)) != 5)
		e(0);

	if (recv(fd2, buf2, sizeof(buf2), 0) != 5) e(0);

	if (buf2[0] != 0xb3 || buf2[1] != 0x65) e(0);
	if (memcmp(&buf2[2], &buf[2], 3) != 0) e(0);

	/*
	 * Turn off checksum computation on the sending side, so that the
	 * packet ends up being dropped on the receiving side.
	 */
	val = -1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	if (sendto(fd, buf, 5, 0, (struct sockaddr *)&sin6, sizeof(sin6)) != 5)
		e(0);

	/* Send some packets that are too small to contain the checksum. */
	if (sendto(fd, buf, 0, 0, (struct sockaddr *)&sin6, sizeof(sin6)) != 0)
		e(0);
	if (sendto(fd, buf, 1, 0, (struct sockaddr *)&sin6, sizeof(sin6)) != 1)
		e(0);

	/*
	 * If this recv call is "too soon" (it should not be) and the packets
	 * arrive later anyway, then we will get a failure below.
	 */
	if (recv(fd2, buf2, sizeof(buf2), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	buf[0] = 0;
	buf[1] = 0x67;
	if (sendto(fd, buf, 4, 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != 4) e(0);

	if (recv(fd2, buf2, sizeof(buf2), 0) != 4) e(0);
	if (memcmp(buf, buf2, 4) != 0) e(0);

	/*
	 * We repeat some of the tests with a non-zero checksum offset, just to
	 * be sure.
	 */
	val = 2;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 2) e(0);

	buf[0] = 0x56;
	buf[1] = 0x78;

	for (i = 0; i <= 3; i++) {
		if (sendto(fd, buf, i, 0, (struct sockaddr *)&sin6,
		    sizeof(sin6)) != -1) e(0);
		if (errno != EINVAL) e(0);
	}

	val = 2;
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	if (sendto(fd, buf, 4, 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != 4) e(0);

	if (recv(fd2, buf2, sizeof(buf2), 0) != 4) e(0);
	if (memcmp(buf, buf2, 2) != 0) e(0);
	if (buf2[2] != 0xa8 || buf2[3] != 0x84) e(0);

	val = -1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	buf[2] = 0xa8;
	buf[3] = 0x85; /* deliberately bad checksum */

	/* All these should be dropped on the receiver side. */
	for (i = 0; i <= 4; i++) {
		if (sendto(fd, buf, i, 0, (struct sockaddr *)&sin6,
		    sizeof(sin6)) != i) e(0);
	}

	buf[3] = 0x84; /* good checksum */
	if (sendto(fd, buf, 4, 0, (struct sockaddr *)&sin6, sizeof(sin6)) != 4)
		e(0);

	if (recv(fd2, buf2, sizeof(buf2), 0) != 4) e(0);
	if (memcmp(buf, buf2, 4) != 0) e(0);

	if (recv(fd2, buf2, sizeof(buf2), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	val = -1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);
	if (setsockopt(fd2, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	buf[3] = 0x85;
	if (sendto(fd, buf, 4, 0, (struct sockaddr *)&sin6, sizeof(sin6)) != 4)
		e(0);

	if (recv(fd2, buf2, sizeof(buf2), 0) != 4) e(0);
	if (memcmp(buf, buf2, 4) != 0) e(0);

	/*
	 * The following is a lwIP-specific test: lwIP does not support storing
	 * generated checksums beyond the first pbuf.  We do not know the size
	 * of the first pbuf until we actually send a packet, so the setsockopt
	 * call will not fail, but sending the packet will.  Depending on the
	 * buffer allocation strategy, the following test may or may not
	 * trigger this case; simply ensure that we do not crash the service.
	 */
	if ((buf3 = malloc(4096)) == NULL) e(0);

	val = 4094;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != 0) e(0);

	/* This call may or may not fail, but if it fails, it yields EINVAL. */
	if (sendto(fd, buf3, 4096, 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) == -1 && errno != EINVAL) e(0);

	free(buf3);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* For ICMPv6 packets, checksumming is always enabled. */
	if ((fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 2) e(0);

	val = -1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
	    sizeof(val)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memset(&icmp6_hdr, 0, sizeof(icmp6_hdr));
	icmp6_hdr.icmp6_type = TEST_ICMPV6_TYPE_A;
	icmp6_hdr.icmp6_code = 123;
	icmp6_hdr.icmp6_cksum = htons(0);

	len = offsetof(struct icmp6_hdr, icmp6_dataun);
	if (sendto(fd, &icmp6_hdr, len, 0, (struct sockaddr *)&sin6,
	   sizeof(sin6)) != len) e(0);

	if (recv(fd, &icmp6_hdr, sizeof(icmp6_hdr), 0) != len) e(0);

	if (icmp6_hdr.icmp6_type != TEST_ICMPV6_TYPE_A) e(0);
	if (icmp6_hdr.icmp6_code != 123) e(0);
	if (ntohs(icmp6_hdr.icmp6_cksum) != 0x3744) e(0);

	if (close(fd) != 0) e(0);

	/* For IPv4 and non-RAW IPv6 sockets, the option does not work. */
	for (i = 0; i <= 2; i++) {
		switch (i) {
		case 0: fd = socket(AF_INET6, SOCK_DGRAM, 0); break;
		case 1: fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMPV6); break;
		case 2: fd = socket(AF_INET, SOCK_RAW, TEST_PROTO); break;
		}
		if (fd < 0) e(0);

		val = -1;
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
		    sizeof(val)) != -1) e(0);
		if (errno != ENOPROTOOPT) e(0);

		len = sizeof(val);
		if (getsockopt(fd, IPPROTO_IPV6, IPV6_CHECKSUM, &val,
		    &len) != -1) e(0);
		if (errno != ENOPROTOOPT) e(0);

		if (close(fd) != 0) e(0);
	}
}

/*
 * Test the ICMP6_FILTER socket option.
 */
static void
test92d(void)
{
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	struct icmp6_filter filter;
	struct icmp6_hdr packet;
	socklen_t len;
	struct timeval tv;
	unsigned int i;
	int fd, fd2;

	subtest = 4;

	/*
	 * We use two different sockets to eliminate the possibility that the
	 * filter is also applied when sending packets--it should not be.
	 */
	if ((fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) e(0);

	if (shutdown(fd, SHUT_WR) != 0) e(0);

	len = sizeof(filter);
	if (getsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, &len) != 0)
		e(0);

	/* We do not aim to test the ICMP6_FILTER macros here. */
	for (i = 0; i <= UINT8_MAX; i++)
		if (!ICMP6_FILTER_WILLPASS(i, &filter)) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) e(0);

	ICMP6_FILTER_SETBLOCKALL(&filter);
	if (setsockopt(fd2, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
	    sizeof(filter)) != 0) e(0);

	len = sizeof(filter);
	if (getsockopt(fd2, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, &len) != 0)
		e(0);

	for (i = 0; i <= UINT8_MAX; i++)
		if (ICMP6_FILTER_WILLPASS(i, &filter)) e(0);

	ICMP6_FILTER_SETPASSALL(&filter);
	ICMP6_FILTER_SETBLOCK(TEST_ICMPV6_TYPE_A, &filter);
	if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
	    sizeof(filter)) != 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	memset(&packet, 0, sizeof(packet));
	packet.icmp6_type = TEST_ICMPV6_TYPE_A;
	packet.icmp6_code = 12;

	if (sendto(fd2, &packet, sizeof(packet), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != sizeof(packet)) e(0);

	packet.icmp6_type = TEST_ICMPV6_TYPE_B;
	packet.icmp6_code = 34;

	if (sendto(fd2, &packet, sizeof(packet), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != sizeof(packet)) e(0);

	memset(&packet, 0, sizeof(packet));

	if (recv(fd, &packet, sizeof(packet), 0) != sizeof(packet)) e(0);
	if (packet.icmp6_type != TEST_ICMPV6_TYPE_B) e(0);
	if (packet.icmp6_code != 34) e(0);

	if (recv(fd, &packet, sizeof(packet), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	ICMP6_FILTER_SETBLOCKALL(&filter);
	ICMP6_FILTER_SETPASS(TEST_ICMPV6_TYPE_A, &filter);
	if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
	    sizeof(filter)) != 0) e(0);

	memset(&packet, 0, sizeof(packet));
	packet.icmp6_type = TEST_ICMPV6_TYPE_B;
	packet.icmp6_code = 56;

	if (sendto(fd2, &packet, sizeof(packet), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != sizeof(packet)) e(0);

	packet.icmp6_type = TEST_ICMPV6_TYPE_A;
	packet.icmp6_code = 78;

	if (sendto(fd2, &packet, sizeof(packet), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != sizeof(packet)) e(0);

	/*
	 * RFC 3542 states that setting a zero-length filter resets the filter.
	 * This seems like one of those things that a standardization RFC
	 * should not mandate: it is redundant at the API level (one can set a
	 * PASSALL filter, which is the required default), it relies on an edge
	 * case (setsockopt taking a zero-length argument), and as a "shortcut"
	 * it does not even cover a case that is likely to occur (no actual
	 * program would reset its filter on a regular basis).  Presumably it
	 * is a way to deallocate filter memory on some platforms, but was that
	 * worth the RFC inclusion?  Anyhow, we support it; NetBSD does not.
	 */
	if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, NULL, 0) != 0) e(0);

	packet.icmp6_type = TEST_ICMPV6_TYPE_B;
	packet.icmp6_code = 90;

	if (sendto(fd2, &packet, sizeof(packet), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != sizeof(packet)) e(0);

	memset(&packet, 0, sizeof(packet));

	if (recv(fd, &packet, sizeof(packet), 0) != sizeof(packet)) e(0);
	if (packet.icmp6_type != TEST_ICMPV6_TYPE_A) e(0);
	if (packet.icmp6_code != 78) e(0);

	if (recv(fd, &packet, sizeof(packet), 0) != sizeof(packet)) e(0);
	if (packet.icmp6_type != TEST_ICMPV6_TYPE_B) e(0);
	if (packet.icmp6_code != 90) e(0);

	if (recv(fd, &packet, sizeof(packet), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (recv(fd2, &packet, sizeof(packet), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	len = sizeof(filter);
	if (getsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, &len) != 0)
		e(0);

	for (i = 0; i <= UINT8_MAX; i++)
		if (!ICMP6_FILTER_WILLPASS(i, &filter)) e(0);

	if (close(fd2) != 0) e(0);

	/*
	 * Let's get weird and send an ICMPv6 packet from an IPv4 socket.
	 * Currently, such packets are always dropped based on the rule that
	 * IPv6 sockets with checksumming enabled drop all IPv4 packets.  As it
	 * happens, that is also all that is keeping this packet from arriving.
	 */
	if ((fd2 = socket(AF_INET, SOCK_RAW, IPPROTO_ICMPV6)) < 0) e(0);

	ICMP6_FILTER_SETBLOCKALL(&filter);
	if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
	    sizeof(filter)) != 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	memset(&packet, 0, sizeof(packet));
	packet.icmp6_type = TEST_ICMPV6_TYPE_A;
	packet.icmp6_code = 123;
	packet.icmp6_cksum = htons(0); /* TODO: use valid checksum */

	if (sendto(fd2, &packet, sizeof(packet), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != sizeof(packet)) e(0);

	/*
	 * If the packet were to arrive at all, it should arrive instantly, so
	 * this is just an excuse to use SO_RCVTIMEO.
	 */
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
		e(0);

	if (recv(fd, &packet, sizeof(packet), 0) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (close(fd2) != 0) e(0);

	if (close(fd) != 0) e(0);

	/* Make sure ICMP6_FILTER works on IPv6-ICMPv6 sockets only. */
	for (i = 0; i <= 2; i++) {
		switch (i) {
		case 0: fd = socket(AF_INET6, SOCK_DGRAM, 0); break;
		case 1: fd = socket(AF_INET6, SOCK_RAW, TEST_PROTO); break;
		case 2: fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMPV6); break;
		}
		if (fd < 0) e(0);

		if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
		    sizeof(filter)) != -1) e(0);
		if (errno != ENOPROTOOPT) e(0);

		len = sizeof(filter);
		if (getsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
		    &len) != -1) e(0);
		if (errno != ENOPROTOOPT) e(0);

		if (close(fd) != 0) e(0);
	}
}

/*
 * Test that IPPROTO_ICMPV6 has no special value on IPv4 raw sockets.  In
 * particular, test that no checksum is generated or verified.  By now we have
 * already tested that none of the IPv6 socket options work on such sockets.
 */
static void
test92e(void)
{
	char buf[sizeof(struct ip) + sizeof(struct icmp6_hdr)];
	struct sockaddr_in sin;
	struct icmp6_hdr packet;
	int fd;

	subtest = 5;

	if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMPV6)) < 0) e(0);

	memset(&packet, 0, sizeof(packet));
	packet.icmp6_type = TEST_ICMPV6_TYPE_A;
	packet.icmp6_code = 123;
	packet.icmp6_cksum = htons(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (sendto(fd, &packet, sizeof(packet), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != sizeof(packet)) e(0);

	if (recv(fd, buf, sizeof(buf), 0) != sizeof(buf)) e(0);

	memcpy(&packet, &buf[sizeof(struct ip)], sizeof(packet));
	if (packet.icmp6_type != TEST_ICMPV6_TYPE_A) e(0);
	if (packet.icmp6_code != 123) e(0);
	if (packet.icmp6_cksum != htons(0)) e(0);

	if (close(fd) != 0) e(0);
}

struct testpkt {
	struct ip ip;
	struct udphdr udp;
	uint8_t data[6];
} __packed;

/*
 * Test the IP_HDRINCL socket option.
 */
static void
test92f(void)
{
	struct sockaddr_in sin;
	struct testpkt pkt, pkt2;
	socklen_t len;
	char buf[7];
	unsigned int i;
	int fd, fd2, val;

	subtest = 6;

	/* See if we can successfully feign a UDP packet. */
	memset(&pkt, 0, sizeof(pkt));
	pkt.ip.ip_v = IPVERSION;
	pkt.ip.ip_hl = sizeof(pkt.ip) >> 2;
	pkt.ip.ip_tos = 123;
	pkt.ip.ip_len = sizeof(pkt);			/* swapped by OS */
	pkt.ip.ip_id = htons(456);
	pkt.ip.ip_off = IP_DF;				/* swapped by OS */
	pkt.ip.ip_ttl = 78;
	pkt.ip.ip_p = IPPROTO_UDP;
	pkt.ip.ip_sum = htons(0);			/* filled by OS */
	pkt.ip.ip_src.s_addr = htonl(INADDR_LOOPBACK);
	pkt.ip.ip_dst.s_addr = htonl(INADDR_LOOPBACK);
	pkt.udp.uh_sport = htons(TEST_PORT_B);
	pkt.udp.uh_dport = htons(TEST_PORT_A);
	pkt.udp.uh_sum = htons(0); /* lazy.. */
	pkt.udp.uh_ulen = htons(sizeof(pkt.udp) + sizeof(pkt.data));
	memcpy(pkt.data, "Hello!", sizeof(pkt.data));

	if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) e(0);

	if (shutdown(fd, SHUT_RD) != 0) e(0);

	/* IP_HDRINCL is never enabled by default. */
	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) != 0)
		e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 1) e(0);

	if ((fd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_A);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(fd2, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	sin.sin_port = htons(0);

	if (sendto(fd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != sizeof(pkt)) e(0);

	if (recv(fd2, &buf, sizeof(buf), 0) != sizeof(pkt.data)) e(0);
	if (memcmp(buf, pkt.data, sizeof(pkt.data)) != 0) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	len = sizeof(val);
	if (getsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val, &len) != 0) e(0);
	if (len != sizeof(val)) e(0);
	if (val != 0) e(0);

	if (shutdown(fd, SHUT_RD) != 0) e(0);

	/* See if we can receive a packet for our own protocol. */
	pkt.ip.ip_p = TEST_PROTO;

	if ((fd2 = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) != 0)
		e(0);

	if (sendto(fd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != sizeof(pkt)) e(0);

	if (recv(fd2, &pkt2, sizeof(pkt2), 0) != sizeof(pkt2)) e(0);

	if (pkt2.ip.ip_v != pkt.ip.ip_v) e(0);
	if (pkt2.ip.ip_hl != pkt.ip.ip_hl) e(0);
	if (pkt2.ip.ip_tos != pkt.ip.ip_tos) e(0);
	if (pkt2.ip.ip_len != pkt.ip.ip_len) e(0);
	if (pkt2.ip.ip_id != pkt.ip.ip_id) e(0);
	if (pkt2.ip.ip_off != pkt.ip.ip_off) e(0);
	if (pkt2.ip.ip_ttl != pkt.ip.ip_ttl) e(0);
	if (pkt2.ip.ip_p != pkt.ip.ip_p) e(0);
	if (pkt2.ip.ip_sum == htons(0)) e(0);
	if (pkt2.ip.ip_src.s_addr != pkt.ip.ip_src.s_addr) e(0);
	if (pkt2.ip.ip_dst.s_addr != pkt.ip.ip_dst.s_addr) e(0);

	/*
	 * Test sending packets with weird sizes to ensure that we do not crash
	 * the service.  These packets would never arrive anyway.
	 */
	if (sendto(fd, &pkt, 0, 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sendto(fd, &pkt, sizeof(pkt.ip) - 1, 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sendto(fd, &pkt, sizeof(pkt.ip), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != sizeof(pkt.ip)) e(0);

	if (recv(fd2, &pkt2, sizeof(pkt2), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* Ensure that the socket option does not work on other types. */
	for (i = 0; i <= 1; i++) {
		switch (i) {
		case 0: fd = socket(AF_INET, SOCK_DGRAM, 0); break;
		case 1: fd = socket(AF_INET6, SOCK_RAW, TEST_PROTO); break;
		}
		if (fd < 0) e(0);

		len = sizeof(val);
		if (getsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val,
		    &len) != -1) e(0);
		if (errno != ENOPROTOOPT) e(0);

		if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val,
		    sizeof(val)) != -1) e(0);
		if (errno != ENOPROTOOPT) e(0);

		if (close(fd) != 0) e(0);
	}
}

/*
 * Test the IPPROTO_RAW socket protocol.  This test mostly shows that the
 * IPPROTO_RAW protocol is nothing special: for both IPv4 and IPv6, it sends
 * and receives packets with that protocol number.  We already tested earlier
 * that IP_HDRINCL is disabled by default on IPPROTO_RAW sockets, too.
 */
static void
test92g(void)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	char buf[sizeof(struct ip) + 1];
	int fd;

	subtest = 7;

	if ((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (sendto(fd, "A", 1, 0, (struct sockaddr *)&sin,
	    sizeof(sin)) != 1) e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != sizeof(buf)) e(0);
	if (buf[sizeof(struct ip)] != 'A') e(0);

	if (close(fd) != 0) e(0);

	if ((fd = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	if (sendto(fd, "B", 1, 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != 1) e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != 1) e(0);
	if (buf[0] != 'B') e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test that connected raw sockets perform correct source-based filtering.
 */
static void
test92h(void)
{
	struct sockaddr_in sinA, sinB;
	struct sockaddr_in6 sin6A, sin6B;
	struct sockaddr sa;
	socklen_t len;
	char buf[sizeof(struct ip) + 1];
	int fd, fd2;

	subtest = 8;

	if ((fd = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	len = sizeof(sinB);
	if (getpeername(fd, (struct sockaddr *)&sinB, &len) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	memset(&sinA, 0, sizeof(sinA));
	sinA.sin_family = AF_INET;
	sinA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	/*
	 * First test that packets with the right source are accepted.
	 * Unfortunately, source and destination are the same in this case, so
	 * this test is far from perfect.
	 */
	if (connect(fd, (struct sockaddr *)&sinA, sizeof(sinA)) != 0) e(0);

	if (getpeername(fd, (struct sockaddr *)&sinB, &len) != 0) e(0);

	if ((fd2 = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	if (sendto(fd2, "A", 1, 0, (struct sockaddr *)&sinA,
	    sizeof(sinA)) != 1) e(0);

	buf[0] = '\0';
	if (recv(fd2, buf, sizeof(buf), 0) != sizeof(struct ip) + 1) e(0);
	if (buf[sizeof(struct ip)] != 'A') e(0);

	buf[0] = '\0';
	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) !=
	    sizeof(struct ip) + 1) e(0);
	if (buf[sizeof(struct ip)] != 'A') e(0);

	memset(&sa, 0, sizeof(sa));
	sa.sa_family = AF_UNSPEC;

	sinA.sin_addr.s_addr = htonl(INADDR_NONE);

	/* While here, test unconnecting the socket. */
	if (connect(fd, &sa, sizeof(sa)) != 0) e(0);

	if (getpeername(fd, (struct sockaddr *)&sinB, &len) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	/* Then test that packets with the wrong source are ignored. */
	if (connect(fd, (struct sockaddr *)&sinA, sizeof(sinA)) != 0) e(0);

	if (sendto(fd2, "B", 1, 0, (struct sockaddr *)&sinB,
	    sizeof(sinB)) != 1) e(0);

	buf[0] = '\0';
	if (recv(fd2, buf, sizeof(buf), 0) != sizeof(struct ip) + 1) e(0);
	if (buf[sizeof(struct ip)] != 'B') e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);

	/* Repeat for IPv6, but now the other way around. */
	if ((fd = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	len = sizeof(sin6B);
	if (getpeername(fd, (struct sockaddr *)&sin6B, &len) != -1) e(0);
	if (errno != ENOTCONN) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;
	memcpy(&sin6A.sin6_addr, &in6addr_loopback, sizeof(sin6A.sin6_addr));

	memcpy(&sin6B, &sin6A, sizeof(sin6B));
	if (inet_pton(AF_INET6, "::2", &sin6B.sin6_addr) != 1) e(0);

	if (connect(fd, (struct sockaddr *)&sin6B, sizeof(sin6B)) != 0) e(0);

	if ((fd2 = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	if (sendto(fd2, "C", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	buf[0] = '\0';
	if (recv(fd2, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'C') e(0);

	if (recv(fd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (connect(fd, &sa, sizeof(sa)) != 0) e(0);

	if (connect(fd, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (sendto(fd2, "D", 1, 0, (struct sockaddr *)&sin6A,
	    sizeof(sin6A)) != 1) e(0);

	buf[0] = '\0';
	if (recv(fd2, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'D') e(0);

	buf[0] = '\0';
	if (recv(fd, buf, sizeof(buf), 0) != 1) e(0);
	if (buf[0] != 'D') e(0);

	if (close(fd2) != 0) e(0);
	if (close(fd) != 0) e(0);
}

/*
 * Test sending large and small RAW packets.  This test is an altered copy of
 * test91e, but has been changed to IPv6 to cover a greater spectrum together.
 */
static void
test92i(void)
{
	struct sockaddr_in6 sin6;
	struct msghdr msg;
	struct iovec iov;
	char *buf;
	unsigned int i, j;
	int r, fd, fd2, val;

	subtest = 9;

	if ((buf = malloc(65536)) == NULL) e(0);

	if ((fd = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	val = 65536;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val)) != 0)
		e(0);

	if ((fd2 = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

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
		if ((r = sendto(fd2, buf, val, 0, (struct sockaddr *)&sin6,
		    sizeof(sin6))) == val)
			break;
		if (r != -1) e(0);
		if (errno != EMSGSIZE) e(0);
	}

	if (val != 65535 - sizeof(struct ip6_hdr)) e(0);

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

	if (sendto(fd2, buf, val, 0, (struct sockaddr *)&sin6, sizeof(sin6)) !=
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

	if (sendto(fd2, buf, val, 0, (struct sockaddr *)&sin6, sizeof(sin6)) !=
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

	if (sendto(fd2, buf, val, 0, (struct sockaddr *)&sin6, sizeof(sin6)) !=
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
	if (sendto(fd2, buf, 0, 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != 0) e(0);

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
		if (sendto(fd2, &j, sizeof(j), 0, (struct sockaddr *)&sin6,
		    sizeof(sin6)) != sizeof(j)) e(0);
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
 * Test sending and receiving with bad pointers.
 */
static void
test92j(void)
{
	struct sockaddr_in sin;
	char *ptr;
	int i, fd;

	subtest = 10;

	if ((ptr = mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED) e(0);

	if (munmap(&ptr[PAGE_SIZE], PAGE_SIZE) != 0) e(0);

	if ((fd = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	memset(ptr, 'A', PAGE_SIZE);

	if (sendto(fd, &ptr[PAGE_SIZE / 2], PAGE_SIZE, 0,
	    (struct sockaddr *)&sin, sizeof(sin)) != -1) e(0);
	if (errno != EFAULT) e(0);

	memset(ptr, 'B', PAGE_SIZE);

	if (sendto(fd, ptr, PAGE_SIZE - sizeof(struct ip), 0,
	    (struct sockaddr *)&sin, sizeof(sin)) !=
	    PAGE_SIZE - sizeof(struct ip)) e(0);

	memset(ptr, 0, PAGE_SIZE);

	if (recvfrom(fd, &ptr[PAGE_SIZE / 2], PAGE_SIZE, 0, NULL, 0) != -1)
		e(0);
	if (errno != EFAULT) e(0);

	if (recvfrom(fd, ptr, PAGE_SIZE * 2, 0, NULL, 0) != PAGE_SIZE) e(0);
	for (i = sizeof(struct ip); i < PAGE_SIZE; i++)
		if (ptr[i] != 'B') e(0);

	if (close(fd) != 0) e(0);

	if (munmap(ptr, PAGE_SIZE) != 0) e(0);
}

/*
 * Test basic sysctl(2) socket enumeration support.
 */
static void
test92k(void)
{
	struct kinfo_pcb ki;
	struct sockaddr_in lsin, rsin;
	struct sockaddr_in6 lsin6, rsin6;
	int fd, fd2, val;

	subtest = 11;

	if (socklib_find_pcb("net.inet.raw.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 0) e(0);

	if ((fd = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	memset(&lsin, 0, sizeof(lsin));
	lsin.sin_len = sizeof(lsin);
	lsin.sin_family = AF_INET;

	memset(&rsin, 0, sizeof(rsin));
	rsin.sin_len = sizeof(rsin);
	rsin.sin_family = AF_INET;

	if (socklib_find_pcb("net.inet.raw.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 1) e(0);
	if (ki.ki_type != SOCK_RAW) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	if (socklib_find_pcb("net.inet6.raw6.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 0) e(0);

	lsin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(fd, (struct sockaddr *)&lsin, sizeof(lsin)) != 0) e(0);

	if (socklib_find_pcb("net.inet.raw.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 1) e(0);
	if (ki.ki_type != SOCK_RAW) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);
	if (ki.ki_pflags & INP_HDRINCL) e(0);

	rsin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(fd, (struct sockaddr *)&rsin, sizeof(rsin)) != 0) e(0);

	if (socklib_find_pcb("net.inet.raw.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 1) e(0);
	if (ki.ki_type != SOCK_RAW) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	if ((fd2 = socket(AF_INET, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	if (sendto(fd2, "ABC", 3, 0, (struct sockaddr *)&lsin,
	    sizeof(lsin)) != 3) e(0);

	if (close(fd2) != 0) e(0);

	val = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) != 0)
		e(0);

	if (socklib_find_pcb("net.inet.raw.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 1) e(0);
	if (ki.ki_type != SOCK_RAW) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin, sizeof(lsin)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin, sizeof(rsin)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq < 3) e(0);	/* size is rounded up */
	if (!(ki.ki_pflags & INP_HDRINCL)) e(0);

	if (socklib_find_pcb("net.inet6.raw6.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 0) e(0);

	if (close(fd) != 0) e(0);

	/* Test IPv6 sockets as well. */
	if ((fd = socket(AF_INET6, SOCK_RAW, TEST_PROTO)) < 0) e(0);

	memset(&lsin6, 0, sizeof(lsin6));
	lsin6.sin6_len = sizeof(lsin6);
	lsin6.sin6_family = AF_INET6;

	memset(&rsin6, 0, sizeof(rsin6));
	rsin6.sin6_len = sizeof(rsin6);
	rsin6.sin6_family = AF_INET6;

	if (socklib_find_pcb("net.inet6.raw6.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 1) e(0);
	if (ki.ki_type != SOCK_RAW) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);

	memcpy(&lsin6.sin6_addr, &in6addr_loopback, sizeof(lsin6.sin6_addr));
	if (bind(fd, (struct sockaddr *)&lsin6, sizeof(lsin6)) != 0) e(0);

	if (socklib_find_pcb("net.inet6.raw6.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 1) e(0);
	if (ki.ki_type != SOCK_RAW) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);
	if (!(ki.ki_pflags & IN6P_IPV6_V6ONLY)) e(0);

	memcpy(&rsin6.sin6_addr, &in6addr_loopback, sizeof(rsin6.sin6_addr));
	if (connect(fd, (struct sockaddr *)&rsin6, sizeof(rsin6)) != 0)
		e(0);

	if (socklib_find_pcb("net.inet6.raw6.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 1) e(0);
	if (ki.ki_type != SOCK_RAW) e(0);
	if (ki.ki_tstate != 0) e(0);
	if (memcmp(&ki.ki_src, &lsin6, sizeof(lsin6)) != 0) e(0);
	if (memcmp(&ki.ki_dst, &rsin6, sizeof(rsin6)) != 0) e(0);
	if (ki.ki_sndq != 0) e(0);
	if (ki.ki_rcvq != 0) e(0);
	if (!(ki.ki_pflags & IN6P_IPV6_V6ONLY)) e(0);

	if (socklib_find_pcb("net.inet.raw.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 0) e(0);

	if (close(fd) != 0) e(0);

	if (socklib_find_pcb("net.inet6.raw6.pcblist", TEST_PROTO, 0, 0,
	    &ki) != 0) e(0);
}

/*
 * Test local and remote IPv6 address handling.  In particular, test scope IDs
 * and IPv4-mapped IPv6 addresses.
 */
static void
test92l(void)
{

	subtest = 12;

	socklib_test_addrs(SOCK_RAW, TEST_PROTO);
}

/*
 * Test setting and retrieving basic multicast transmission options.
 */
static void
test92m(void)
{

	subtest = 13;

	socklib_multicast_tx_options(SOCK_RAW);
}

/*
 * Test multicast support.
 */
static void
test92n(void)
{

	subtest = 14;

	socklib_test_multicast(SOCK_RAW, TEST_PROTO);
}

/*
 * Test small and large ICMP echo ("ping") packets.  This test aims to confirm
 * expected behavior resulting from the LWIP service's memory pool policies:
 * lwIP should reply to ICMP echo requests that fit in a single 512-byte buffer
 * (including space for ethernet headers, even on loopback interfaces), but not
 * to requests exceeding a single buffer.
 */
static void
test92o(void)
{
	struct sockaddr_in6 sin6;
	struct icmp6_hdr packet;
	char buf[512];
	int fd;

	subtest = 15;

	/* IPv6 only for now, for simplicity reasons. */
	if ((fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));

	memset(&packet, 0, sizeof(packet));
	packet.icmp6_type = ICMP6_ECHO_REQUEST;
	packet.icmp6_code = 0;
	packet.icmp6_id = getpid();
	packet.icmp6_seq = 1;

	memset(buf, 'A', sizeof(buf));
	memcpy(buf, &packet, sizeof(packet));

	if (sendto(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != sizeof(buf)) e(0);

	packet.icmp6_seq = 2;

	memset(buf, 'B', sizeof(buf));
	memcpy(buf, &packet, sizeof(packet));

	if (sendto(fd, buf, sizeof(buf) - 100, 0, (struct sockaddr *)&sin6,
	    sizeof(sin6)) != sizeof(buf) - 100) e(0);

	do {
		memset(buf, '\0', sizeof(buf));

		if (recv(fd, buf, sizeof(buf), 0) <= 0) e(0);

		memcpy(&packet, buf, sizeof(packet));
	} while (packet.icmp6_type == ICMP6_ECHO_REQUEST);

	if (packet.icmp6_type != ICMP6_ECHO_REPLY) e(0);
	if (packet.icmp6_code != 0) e(0);
	if (packet.icmp6_id != getpid()) e(0);
	if (packet.icmp6_seq != 2) e(0);
	if (buf[sizeof(buf) - 101] != 'B') e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test program for LWIP RAW sockets.
 */
int
main(int argc, char ** argv)
{
	int i, m;

	start(92);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFFFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x0001) test92a();
		if (m & 0x0002) test92b();
		if (m & 0x0004) test92c();
		if (m & 0x0008) test92d();
		if (m & 0x0010) test92e();
		if (m & 0x0020) test92f();
		if (m & 0x0040) test92g();
		if (m & 0x0080) test92h();
		if (m & 0x0100) test92i();
		if (m & 0x0200) test92j();
		if (m & 0x0400) test92k();
		if (m & 0x0400) test92k();
		if (m & 0x0800) test92l();
		if (m & 0x1000) test92m();
		if (m & 0x2000) test92n();
		if (m & 0x4000) test92o();
	}

	quit();
	/* NOTREACHED */
}
