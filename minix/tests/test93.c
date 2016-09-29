/* Tests for network interfaces and routing (LWIP) - by D.C. van Moolenbroek */
/* This test needs to be run as root: it manipulates network settings. */
/*
 * TODO: due to time constraints, this test is currently absolutely minimal.
 * It does not yet test by far most of the service code it is supposed to test,
 * in particular interface management code, interface address assignment code,
 * routing sockets code, and routing code. The second subtest (test93b) in this
 * file serves as a reasonable example of how many of the future subtests
 * should operate, though: by issuing interface IOCTLs and routing commands on
 * a loopback interface created for the occasion.
 */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <arpa/inet.h>

#include "common.h"
#include "socklib.h"

#define TEST_IFNAME	"lo93"

#define ITERATIONS 2

static const enum state rtlnk_states[] = {
		S_NEW,		S_N_SHUT_R,	S_N_SHUT_W,	S_N_SHUT_RW,
};

static const int rt_results[][__arraycount(rtlnk_states)] = {
	[C_ACCEPT]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_BIND]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_CONNECT]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_GETPEERNAME]		= {
		0,		0,		0,		0,
	},
	[C_GETSOCKNAME]		= {
		0,		0,		0,		0,
	},
	[C_GETSOCKOPT_ERR]	= {
		0,		0,		0,		0,
	},
	[C_GETSOCKOPT_KA]	= {
		0,		0,		0,		0,
	},
	[C_GETSOCKOPT_RB]	= {
		0,		0,		0,		0,
	},
	[C_IOCTL_NREAD]		= {
		0,		0,		0,		0,
	},
	[C_LISTEN]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_RECV]		= {
		-EAGAIN,	0,		-EAGAIN,	0,
	},
	[C_RECVFROM]		= {
		-EAGAIN,	0,		-EAGAIN,	0,
	},
	[C_SEND]		= {
		-ENOBUFS,	-ENOBUFS,	-EPIPE,		-EPIPE,
	},
	[C_SENDTO]		= {
		-EISCONN,	-EISCONN,	-EPIPE,		-EPIPE,
	},
	[C_SELECT_R]		= {
		0,		1,		0,		1,
	},
	[C_SELECT_W]		= {
		1,		1,		1,		1,
	},
	[C_SELECT_X]		= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_BC]	= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_KA]	= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_L]	= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_RA]	= {
		0,		0,		0,		0,
	},
	[C_SHUTDOWN_R]		= {
		0,		0,		0,		0,
	},
	[C_SHUTDOWN_RW]		= {
		0,		0,		0,		0,
	},
	[C_SHUTDOWN_W]		= {
		0,		0,		0,		0,
	},
};

static const int lnk_results[][__arraycount(rtlnk_states)] = {
	[C_ACCEPT]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_BIND]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_CONNECT]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_GETPEERNAME]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_GETSOCKNAME]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_GETSOCKOPT_ERR]	= {
		0,		0,		0,		0,
	},
	[C_GETSOCKOPT_KA]	= {
		0,		0,		0,		0,
	},
	[C_GETSOCKOPT_RB]	= {
		-ENOPROTOOPT,	-ENOPROTOOPT,	-ENOPROTOOPT,	-ENOPROTOOPT,
	},
	[C_IOCTL_NREAD]		= {
		0,		0,		0,		0,
	},
	[C_LISTEN]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,	-EOPNOTSUPP,
	},
	[C_RECV]		= {
		-EOPNOTSUPP,	0,		-EOPNOTSUPP,	0,
	},
	[C_RECVFROM]		= {
		-EOPNOTSUPP,	0,		-EOPNOTSUPP,	0,
	},
	[C_SEND]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EPIPE,		-EPIPE,
	},
	[C_SENDTO]		= {
		-EOPNOTSUPP,	-EOPNOTSUPP,	-EPIPE,		-EPIPE,
	},
	[C_SELECT_R]		= {
		1,		1,		1,		1,
	},
	[C_SELECT_W]		= {
		1,		1,		1,		1,
	},
	[C_SELECT_X]		= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_BC]	= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_KA]	= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_L]	= {
		0,		0,		0,		0,
	},
	[C_SETSOCKOPT_RA]	= {
		0,		0,		0,		0,
	},
	[C_SHUTDOWN_R]		= {
		0,		0,		0,		0,
	},
	[C_SHUTDOWN_RW]		= {
		0,		0,		0,		0,
	},
	[C_SHUTDOWN_W]		= {
		0,		0,		0,		0,
	},
};

/*
 * Set up a routing or link socket file descriptor in the requested state and
 * pass it to socklib_sweep_call() along with local and remote addresses and
 * their lengths.
 */
static int
rtlnk_sweep(int domain, int type, int protocol, enum state state,
	enum call call)
{
	struct sockaddr sa;
	int r, fd;

	memset(&sa, 0, sizeof(sa));
	sa.sa_family = domain;

	if ((fd = socket(domain, type | SOCK_NONBLOCK, protocol)) < 0) e(0);

	switch (state) {
	case S_NEW: break;
	case S_N_SHUT_R: if (shutdown(fd, SHUT_RD)) e(0); break;
	case S_N_SHUT_W: if (shutdown(fd, SHUT_WR)) e(0); break;
	case S_N_SHUT_RW: if (shutdown(fd, SHUT_RDWR)) e(0); break;
	default: e(0);
	}

	r = socklib_sweep_call(call, fd, &sa, &sa,
	    offsetof(struct sockaddr, sa_data));

	if (close(fd) != 0) e(0);

	return r;
}

/*
 * Sweep test for socket calls versus socket states of routing and link
 * sockets.
 */
static void
test93a(void)
{

	subtest = 1;

	socklib_sweep(AF_ROUTE, SOCK_RAW, 0, rtlnk_states,
	    __arraycount(rtlnk_states), (const int *)rt_results, rtlnk_sweep);

	/*
	 * Our implementation of link sockets currently serves only one
	 * purpose, and that is to pass on ioctl() calls issued on the socket.
	 * As such, the results here are not too important.  The test mostly
	 * ensures that all calls actually complete--for example, that there is
	 * no function pointer NULL check missing in libsockevent.
	 */
	socklib_sweep(AF_LINK, SOCK_DGRAM, 0, rtlnk_states,
	    __arraycount(rtlnk_states), (const int *)lnk_results, rtlnk_sweep);
}

/*
 * Attempt to destroy the test loopback interface.  Return 0 if destruction was
 * successful, or -1 if no such interface existed.
 */
static int
test93_destroy_if(void)
{
	struct ifreq ifr;
	int r, fd;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, TEST_IFNAME, sizeof(ifr.ifr_name));

	r = ioctl(fd, SIOCIFDESTROY, &ifr);
	if (r != 0 && (r != -1 || errno != ENXIO)) e(0);

	if (close(fd) != 0) e(0);

	return r;
}

/*
 * Destroy the test interface at exit.  It is always safe to do so as its name
 * is sufficiently unique, and we do not want to leave it around.
 */
static void
test93_destroy_if_atexit(void)
{
	static int atexit_set = 0;

	if (!atexit_set) {
		(void)test93_destroy_if();

		atexit_set = 1;
	}
}

/*
 * Attempt to create a test loopback interface.  Return 0 if creation was
 * successful, or -1 if no more interfaces could be created.
 */
static int
test93_create_if(void)
{
	struct ifreq ifr;
	int r, fd;

	(void)test93_destroy_if();

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, TEST_IFNAME, sizeof(ifr.ifr_name));

	r = ioctl(fd, SIOCIFCREATE, &ifr);
	if (r != 0 && (r != -1 || errno != ENOBUFS)) e(0);

	if (close(fd) != 0) e(0);

	atexit(test93_destroy_if_atexit);

	return r;
}

/*
 * Set the interface-up value for an interface to the given boolean value.
 */
static void
test93_set_if_up(const char * ifname, int up)
{
	struct ifreq ifr;
	int fd;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, TEST_IFNAME, sizeof(ifr.ifr_name));

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0) e(0);

	if (up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Construct an IPv6 network mask for a certain prefix length.
 */
static void
test93_make_netmask6(struct sockaddr_in6 * sin6, unsigned int prefix)
{
	unsigned int byte, bit;

	if (prefix > 128) e(0);
	memset(sin6, 0, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;

	byte = prefix / NBBY;
	bit = prefix % NBBY;

	if (byte > 0)
		memset(sin6->sin6_addr.s6_addr, 0xff, byte);
	if (bit != 0)
		sin6->sin6_addr.s6_addr[byte] = 0xff << (NBBY - bit);
}

/*
 * Issue a modifying routing command, which must be one of RTM_ADD, RTM_CHANGE,
 * RTM_DELETE, or RTM_LOCK.  The destination address (IPv4 or IPv6) and netmask
 * prefix are required.  The flags (RTF_), interface name, and gateway are
 * optional depending on the command (and flags) being issued.  Return 0 on
 * success, and -1 with errno set on failure.
 */
static int
test93_route_cmd(int cmd, const struct sockaddr * dest, socklen_t dest_len,
	unsigned int prefix, int flags, const char * ifname,
	const struct sockaddr * gw, socklen_t gw_len)
{
	static unsigned int seq = 0;
	struct sockaddr_storage destss, maskss, ifpss, gwss;
	struct sockaddr_in mask4;
	struct sockaddr_in6 mask6;
	struct sockaddr_dl ifp;
	struct rt_msghdr rtm;
	struct iovec iov[5];
	struct msghdr msg;
	unsigned int i, iovlen;
	int r, fd, err;

	memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = cmd;
	rtm.rtm_flags = flags;
	rtm.rtm_addrs = RTA_DST | RTA_NETMASK;
	rtm.rtm_seq = ++seq;

	iovlen = 0;
	iov[iovlen].iov_base = &rtm;
	iov[iovlen++].iov_len = sizeof(rtm);

	memset(&destss, 0, sizeof(destss));
	memcpy(&destss, dest, dest_len);
	destss.ss_len = dest_len;

	iov[iovlen].iov_base = &destss;
	iov[iovlen++].iov_len = RT_ROUNDUP(dest_len);

	/* Do this in RTA order. */
	memset(&gwss, 0, sizeof(gwss));
	if (gw != NULL) {
		memcpy(&gwss, gw, gw_len);
		gwss.ss_len = gw_len;

		rtm.rtm_addrs |= RTA_GATEWAY;
		iov[iovlen].iov_base = &gwss;
		iov[iovlen++].iov_len = RT_ROUNDUP(gwss.ss_len);
	}

	memset(&maskss, 0, sizeof(maskss));
	switch (dest->sa_family) {
	case AF_INET:
		if (prefix > 32) e(0);
		memset(&mask4, 0, sizeof(mask4));
		mask4.sin_family = AF_INET;
		if (prefix < 32)
			mask4.sin_addr.s_addr = htonl(0xffffffffUL << prefix);

		memcpy(&maskss, &mask4, sizeof(mask4));
		maskss.ss_len = sizeof(mask4);

		break;

	case AF_INET6:
		test93_make_netmask6(&mask6, prefix);

		memcpy(&maskss, &mask6, sizeof(mask6));
		maskss.ss_len = sizeof(mask6);

		break;

	default:
		e(0);
	}

	iov[iovlen].iov_base = &maskss;
	iov[iovlen++].iov_len = RT_ROUNDUP(maskss.ss_len);

	if (ifname != NULL) {
		memset(&ifp, 0, sizeof(ifp));
		ifp.sdl_nlen = strlen(ifname);
		ifp.sdl_len = offsetof(struct sockaddr_dl, sdl_data) +
		    ifp.sdl_nlen;
		ifp.sdl_family = AF_LINK;

		memset(&ifpss, 0, sizeof(ifpss));
		memcpy(&ifpss, &ifp, ifp.sdl_len);
		memcpy(&((struct sockaddr_dl *)&ifpss)->sdl_data, ifname,
		    ifp.sdl_nlen);

		rtm.rtm_addrs |= RTA_IFP;
		iov[iovlen].iov_base = &ifpss;
		iov[iovlen++].iov_len = RT_ROUNDUP(ifpss.ss_len);
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = iovlen;

	if ((fd = socket(AF_ROUTE, SOCK_RAW, 0)) < 0) e(0);

	for (i = 0; i < iovlen; i++)
		rtm.rtm_msglen += iov[i].iov_len;

	r = sendmsg(fd, &msg, 0);
	if (r != rtm.rtm_msglen && r != -1) e(0);
	err = errno;

	/*
	 * We could just shut down the socket for reading, but this is just an
	 * extra test we can do basically for free.
	 */
	rtm.rtm_seq = 0;
	do {
		iov[0].iov_base = &rtm;
		iov[0].iov_len = sizeof(rtm);

		if (recvmsg(fd, &msg, 0) <= 0) e(0);
	} while (rtm.rtm_pid != getpid() || rtm.rtm_seq != seq);

	if (r == -1) {
		if (rtm.rtm_errno != err) e(0);
		if (rtm.rtm_flags & RTF_DONE) e(0);
	} else {
		if (rtm.rtm_errno != 0) e(0);
		if (!(rtm.rtm_flags & RTF_DONE)) e(0);
	}

	if (close(fd) != 0) e(0);

	errno = err;
	return (r > 0) ? 0 : -1;
}

/*
 * Add or delete an IPv6 address to or from an interface.  The interface name,
 * address, and prefix length must always be given.  When adding, a set of
 * flags (IN6_IFF) and lifetimes must be given as well.
 */
static void
test93_ipv6_addr(int add, const char * ifname,
	const struct sockaddr_in6 * sin6, unsigned int prefix, int flags,
	uint32_t valid_life, uint32_t pref_life)
{
	struct in6_aliasreq ifra;
	int fd;

	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	memcpy(&ifra.ifra_addr, sin6, sizeof(ifra.ifra_addr));
	/* leave ifra_dstaddr blank */
	test93_make_netmask6(&ifra.ifra_prefixmask, prefix);
	ifra.ifra_flags = flags;
	ifra.ifra_lifetime.ia6t_vltime = valid_life;
	ifra.ifra_lifetime.ia6t_pltime = pref_life;

	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	if (ioctl(fd, (add) ? SIOCAIFADDR_IN6 : SIOCDIFADDR_IN6, &ifra) != 0)
		e(0);

	if (close(fd) != 0) e(0);
}

static const struct {
	int result; /* 0..2 = prefer srcN, -1 = no preference */
	const char *dest_addr;
	const char *src0_addr;
	unsigned int src0_prefix;
	int src0_flags;
	const char *src1_addr;
	unsigned int src1_prefix;
	int src1_flags;
	const char *src2_addr;
	unsigned int src2_prefix;
	int src2_flags;
} test93b_table[] = {
	/*
	 * These are all the applicable tests from RFC 6724 Sec. 10.1, slightly
	 * changed not to use the default link-local address of lo0.
	 */
	/* Prefer appropriate scope: */
	{ 0, "2001:db8:1::1", "2001:db8:3::1", 64, 0, "fe80::93:1", 64, 0 },
	/* Prefer appropriate scope: */
	{ 0, "ff05::1", "2001:db8:3::1", 64, 0, "fe80::93:1", 64, 0 },
	/* Prefer same address: */
	{ 0, "2001:db8:1::1", "2001:db8:1::1", 64, IN6_IFF_DEPRECATED,
	    "2001:db8:2::1", 64, 0 },
	/* Prefer appropriate scope: */
	{ 0, "fe80::93:1", "fe80::93:2", 64, IN6_IFF_DEPRECATED,
	    "2001:db8:2::1", 64, 0 },
	/* Longest matching prefix: */
	{ 0, "2001:db8:1::1", "2001:db8:1::2", 64, 0, "2001:db8:3::2", 64, 0 },
	/* Prefer matching label: */
	{ 0, "2002:c633:6401::1", "2002:c633:6401::d5e3:7953:13eb:22e8", 64,
	    IN6_IFF_TEMPORARY, "2001:db8:1::2", 64, 0 },
	/* Prefer temporary address: */
	{ 1, "2001:db8:1::d5e3:0:0:1", "2001:db8:1::2", 64, 0,
	    "2001:db8:1::d5e3:7953:13eb:22e8", 64, IN6_IFF_TEMPORARY },
	/*
	 * Our own additional tests.
	 */
	/* Prefer same address: */
	{ 1, "4000:93::1", "2001:db8:3::1", 64, 0, "4000:93::1", 64, 0 },
	{ 2, "2001:db8:1::1", "2001:db8:3::1", 64, 0, "fe80::93:1", 64, 0,
	    "2001:db8:1::1", 64, 0 },
	/* Prefer appropriate scope: */
	{ 1, "ff01::1", "2001:db8:3::1", 64, 0, "fe80::93:1", 64, 0 },
	{ 1, "ff02::1", "2001:db8:3::1", 64, 0, "fe80::93:1", 64, 0 },
	{ 0, "ff0e::1", "2001:db8:3::1", 64, 0, "fe80::93:1", 64, 0 },
	{ 1, "fd00:93::1", "2001:db8:3::1", 64, 0, "fd00::93:2", 64, 0 },
	{ 1, "fd00:93::1", "fe80::93:1", 64, 0, "fd00::93:2", 64, 0 },
	{ 0, "fd00:93::1", "2001:db8:3::1", 64, 0, "fe80::93:1", 64, 0 },
	{ 1, "2001:db8:1::1", "fe80::93:1", 64, 0, "fd00::93:2", 64, 0 },
	{ 0, "2001:db8:1::1", "2001:db8:3::1", 64, 0, "4000:93::1", 64, 0 },
	{ 0, "4000:93::2", "2001:db8:3::1", 64, 0, "4000:93::1", 64, 0 },
	{ 2, "2001:db8:1::1", "fe80::93:1", 64, 0, "fd00::93:1", 64, 0,
	    "2001:db8:3::1", 64, 0 },
	{ 2, "2001:db8:1::1", "fe80::93:1", 64, IN6_IFF_DEPRECATED,
	    "fe80::93:2", 64, 0, "2001:db8:3::1", 64, 0 },
	/* Avoid deprecated address: */
	{ 1, "2002:c633:6401::1", "2002:c633:6401::d5e3:7953:13eb:22e8", 64,
	    IN6_IFF_DEPRECATED, "2001:db8:1::2", 64, 0 },
	{ 2, "2001:db8:1::1", "2001:db8:1::3", 64, IN6_IFF_DEPRECATED,
	    "2001:db8:2::1", 64, IN6_IFF_DEPRECATED, "2001:db8:3::1", 64, 0 },
	{ 2, "2001:db8:1::1", "2002:db8:1::3", 64, IN6_IFF_DEPRECATED,
	    "2001:db8:2::1", 64, IN6_IFF_DEPRECATED, "2001:db8:3::1", 64, 0 },
	/* Prefer matching label: */
	{ 0, "2002:c633:6401::1", "2002:c633:6401::d5e3:7953:13eb:22e8", 64, 0,
	    "2001:db8:1::2", 64, IN6_IFF_TEMPORARY },
	{ 2, "2002:c633:6401::1", "2001:db8:3::2", 64, 0, "2001:db8:1::2", 64,
	    IN6_IFF_TEMPORARY, "2002:c633:6401::d5e3:7953:13eb:22e8", 64, 0 },
	{ 2, "2001:db8:1::1", "2003:db8::1", 64, 0, "3ffe:db8::1", 64, 0,
	    "2001:db8:3::1", 64, 0 },
	/* Prefer temporary address: */
	{ 0, "2001:db8:1::d5e3:0:0:1", "2001:db8:1::2", 96, IN6_IFF_TEMPORARY,
	    "2001:db8:1::d5e3:7953:13eb:22e8", 96, 0 },
	{ 2, "2002:c633:6401::1", "2001:db8:3::2", 64, 0, "2002:c633:6401::2",
	    64, 0, "2002:c633:6401::d5e3:7953:13eb:22e8", 64,
	    IN6_IFF_TEMPORARY },
	/* Longest matching prefix: */
	{ 1, "2001:db8:1::d5e3:0:0:1", "2001:db8:1::2", 96, 0,
	    "2001:db8:1::d5e3:7953:13eb:22e8", 96, 0 },
	{ 2, "2001:db8:1:1::1", "2001:db8:2:1::2", 64, 0, "2001:db8:1:2::2",
	    64, 0, "2001:db8:1:1::2", 64, 0 },
	{ 0, "2001:db8:1::1", "2001:db8:1::2", 47, 0, "2001:db8:3::2", 47, 0 },
	/* No preference (a tie): */
	{ -1, "2001:db8:1::1", "2001:db8:1::2", 46, 0, "2001:db8:3::2", 46,
	    0 },
	{ -1, "2001:db8::1:0:0:1", "2001:db8::1:0:0:2", 64, 0,
	    "2001:db8::2:0:0:2", 64, 0, "2001:db8::3:0:0:2", 64, 0 },
};

struct src_addr {
	struct sockaddr_in6 addr;
	unsigned int prefix;
	int flags;
};

/*
 * Test source address selection with a particular destination address and two
 * or three source addresses.
 */
static void
sub93b(int result, const struct sockaddr_in6 * dest, unsigned int ifindex,
	const struct src_addr * src0, const struct src_addr * src1,
	const struct src_addr * src2)
{
	struct sockaddr_in6 dest_copy, src;
	socklen_t len;
	int fd, rt_res;

	/* Add the candidate source addresses. */
	test93_ipv6_addr(1, TEST_IFNAME, &src0->addr, src0->prefix,
	    src0->flags, 0xffffffffUL, 0xffffffffUL);

	test93_ipv6_addr(1, TEST_IFNAME, &src1->addr, src1->prefix,
	    src1->flags, 0xffffffffUL, 0xffffffffUL);

	if (src2 != NULL)
		test93_ipv6_addr(1, TEST_IFNAME, &src2->addr, src2->prefix,
		    src2->flags, 0xffffffffUL, 0xffffffffUL);

	/*
	 * We need to make sure that packets to the destination are routed to
	 * our test interface at all, so create a route for it.  Creating the
	 * route may fail if the destination address is equal to either of the
	 * source addresses, but that is fine.  We use a blackhole route here,
	 * but this test should not generate any traffic anyway.
	 */
	rt_res = test93_route_cmd(RTM_ADD, (struct sockaddr *)dest,
	    sizeof(*dest), 128, RTF_UP | RTF_BLACKHOLE | RTF_STATIC,
	    TEST_IFNAME, NULL, 0);
	if (rt_res != 0 && (rt_res != -1 || errno != EEXIST)) e(0);

	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	/* Set a scope ID if necessary. */
	memcpy(&dest_copy, dest, sizeof(dest_copy));
	dest_copy.sin6_port = 1; /* anything that is not zero */
	if (IN6_IS_ADDR_LINKLOCAL(&dest_copy.sin6_addr) ||
	    IN6_IS_ADDR_MC_NODELOCAL(&dest_copy.sin6_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&dest_copy.sin6_addr))
		dest_copy.sin6_scope_id = ifindex;

	/* Connecting also selects a source address. */
	if (connect(fd, (struct sockaddr *)&dest_copy, sizeof(dest_copy)) != 0)
		e(0);

	/* Obtain the selected source address. */
	len = sizeof(src);
	if (getsockname(fd, (struct sockaddr *)&src, &len) != 0) e(0);

	/*
	 * If the chosen destination address has a scope ID, it must be for our
	 * test interface.
	 */
	if (src.sin6_scope_id != 0 && src.sin6_scope_id != ifindex) e(0);

	/* Is it the expected candidate source address? */
	if (!memcmp(&src.sin6_addr, &src0->addr.sin6_addr,
	    sizeof(src.sin6_addr))) {
		if (result != 0) e(0);
	} else if (!memcmp(&src.sin6_addr, &src1->addr.sin6_addr,
	    sizeof(src.sin6_addr))) {
		if (result != 1) e(0);
	} else if (src2 != NULL && !memcmp(&src.sin6_addr,
	    &src2->addr.sin6_addr, sizeof(src.sin6_addr))) {
		if (result != 2) e(0);
	} else
		e(0);

	/* Clean up. */
	if (close(fd) != 0) e(0);

	if (rt_res == 0) {
		if (test93_route_cmd(RTM_DELETE, (struct sockaddr *)dest,
		    sizeof(*dest), 128, 0, NULL, NULL, 0) != 0) e(0);
	}

	if (src2 != NULL)
		test93_ipv6_addr(0, TEST_IFNAME, &src2->addr, src2->prefix, 0,
		    0, 0);

	test93_ipv6_addr(0, TEST_IFNAME, &src1->addr, src1->prefix, 0, 0, 0);

	test93_ipv6_addr(0, TEST_IFNAME, &src0->addr, src0->prefix, 0, 0, 0);
}

/*
 * IPv6 source address selection algorithm test.
 */
static void
test93b(void)
{
	static const int order[][3] = {
		{ 0, 1, 2 },
		{ 1, 0, 2 },
		{ 0, 2, 1 },
		{ 1, 2, 0 },
		{ 2, 0, 1 },
		{ 2, 1, 0 }
	};
	struct sockaddr_in6 dest;
	struct src_addr src[3];
	unsigned int i, j, k, count, ifindex;
	int result;

	subtest = 2;

	if (test93_create_if() != 0)
		return; /* skip this test */

	if ((ifindex = if_nametoindex(TEST_IFNAME)) == 0) e(0);

	test93_set_if_up(TEST_IFNAME, 1);

	for (i = 0; i < __arraycount(test93b_table); i++) {
		memset(&dest, 0, sizeof(dest));
		dest.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, test93b_table[i].dest_addr,
		    &dest.sin6_addr) != 1) e(0);

		memset(&src[0].addr, 0, sizeof(src[0].addr));
		src[0].addr.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, test93b_table[i].src0_addr,
		    &src[0].addr.sin6_addr) != 1) e(0);
		src[0].prefix = test93b_table[i].src0_prefix;
		src[0].flags = test93b_table[i].src0_flags;

		memset(&src[1].addr, 0, sizeof(src[1].addr));
		src[1].addr.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, test93b_table[i].src1_addr,
		    &src[1].addr.sin6_addr) != 1) e(0);
		src[1].prefix = test93b_table[i].src1_prefix;
		src[1].flags = test93b_table[i].src1_flags;

		if (test93b_table[i].src2_addr != NULL) {
			memset(&src[2].addr, 0, sizeof(src[2].addr));
			src[2].addr.sin6_family = AF_INET6;
			if (inet_pton(AF_INET6, test93b_table[i].src2_addr,
			    &src[2].addr.sin6_addr) != 1) e(0);
			src[2].prefix = test93b_table[i].src2_prefix;
			src[2].flags = test93b_table[i].src2_flags;

			count = 6;
		} else
			count = 2;

		result = test93b_table[i].result;

		/*
		 * Try all orders for the source addresses.  The permutation
		 * part can be done much better, but it really does not matter.
		 */
		for (j = 0; j < count; j++) {
			for (k = 0; k < count; k++)
				if (result == -1 || order[j][k] == result)
					break;

			sub93b((result != -1) ? k : 0, &dest, ifindex,
			    &src[order[j][0]], &src[order[j][1]],
			    (count > 2) ? &src[order[j][2]] : NULL);
		}
	}

	if (test93_destroy_if() != 0) e(0);
}

/*
 * Interface index number wrapping test.
 */
static void
test93c(void)
{
	unsigned int i;

	subtest = 3;

	/* There might not be an available loopback interface at all. */
	if (test93_create_if() != 0)
		return; /* skip this test */

	if (test93_destroy_if() != 0) e(0);

	/*
	 * During the development of the LWIP service, the lwIP library's
	 * interface index assignment was still in its infancy.  This test aims
	 * to ensure that future changes in the library do not break our
	 * service.
	 */
	for (i = 0; i < UINT8_MAX + 1; i++) {
		if (test93_create_if() != 0) e(0);

		if (test93_destroy_if() != 0) e(0);
	}
}

/*
 * Test program for LWIP interface and routing management.
 */
int
main(int argc, char ** argv)
{
	int i, m;

	start(93);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x01) test93a();
		if (m & 0x02) test93b();
		if (m & 0x04) test93c();
	}

	quit();
	/* NOTREACHED */
}
