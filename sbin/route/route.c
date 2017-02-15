/*	$NetBSD: route.c,v 1.151 2015/03/23 18:33:17 roy Exp $	*/

/*
 * Copyright (c) 1983, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1983, 1989, 1991, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)route.c	8.6 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: route.c,v 1.151 2015/03/23 18:33:17 roy Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net80211/ieee80211_netbsd.h>
#include <netinet/in.h>
#include <netatalk/at.h>
#include <netmpls/mpls.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <paths.h>
#include <err.h>

#include "keywords.h"
#include "extern.h"
#include "prog_ops.h"
#include "rtutil.h"

union sockunion {
	struct	sockaddr sa;
	struct	sockaddr_in sin;
#ifdef INET6
	struct	sockaddr_in6 sin6;
#endif
	struct	sockaddr_at sat;
	struct	sockaddr_dl sdl;
#ifndef SMALL
	struct	sockaddr_mpls smpls;
#endif /* SMALL */
	struct	sockaddr_storage sstorage;
};

typedef union sockunion *sup;

struct sou {
	union sockunion *so_dst, *so_gate, *so_mask, *so_genmask, *so_ifa,
		*so_ifp, *so_mpls;
};

static const char *route_strerror(int);
static void set_metric(const char *, int);
static int newroute(int, char *const *);
static void inet_makenetandmask(u_int32_t, struct sockaddr_in *, struct sou *);
#ifdef INET6
static int inet6_makenetandmask(const struct sockaddr_in6 *, struct sou *);
#endif
static int getaddr(int, const char *, struct hostent **, struct sou *);
static int flushroutes(int, char *const [], int);
static char *netmask_string(const struct sockaddr *, int, int);
static int prefixlen(const char *, struct sou *);
#ifndef SMALL
static void interfaces(void);
__dead static void monitor(void);
static int print_getmsg(struct rt_msghdr *, int, struct sou *);
static const char *linkstate(struct if_msghdr *);
static sup readtag(sup, const char *);
static void addtag(sup, const char *, int);
#endif /* SMALL */
static int rtmsg(int, int, struct sou *);
static void mask_addr(struct sou *);
static void print_rtmsg(struct rt_msghdr *, int);
static void pmsg_common(struct rt_msghdr *);
static void pmsg_addrs(const char *, int);
static void bprintf(FILE *, int, const char *);
static void sodump(sup, const char *);
static void sockaddr(const char *, struct sockaddr *);

int	pid, rtm_addrs;
int	sock;
int	forcehost, forcenet, doflush, af;
int	iflag, Lflag, nflag, qflag, tflag, Sflag, Tflag;
int	verbose, aflen = sizeof(struct sockaddr_in), rtag;
int	locking, lockrest, debugonly, shortoutput;
struct	rt_metrics rt_metrics;
int	rtm_inits;
short ns_nullh[] = {0,0,0};
short ns_bh[] = {-1,-1,-1};

static const char opts[] = "dfLnqSsTtv";

void
usage(const char *cp)
{

	if (cp)
		warnx("botched keyword: %s", cp);
	(void)fprintf(stderr,
	    "Usage: %s [-%s] cmd [[-<qualifers>] args]\n", getprogname(), opts);
	exit(1);
	/* NOTREACHED */
}

#define	PRIETHER	"02x:%02x:%02x:%02x:%02x:%02x"
#define	PRIETHER_ARGS(__enaddr)	(__enaddr)[0], (__enaddr)[1], (__enaddr)[2], \
				(__enaddr)[3], (__enaddr)[4], (__enaddr)[5]

int
main(int argc, char * const *argv)
{
	int ch;

	if (argc < 2)
		usage(NULL);

	while ((ch = getopt(argc, argv, opts)) != -1)
		switch (ch) {
		case 'd':
			debugonly = 1;
			break;
		case 'f':
			doflush = 1;
			break;
		case 'L':
			Lflag = RT_LFLAG;
			break;
		case 'n':
			nflag = RT_NFLAG;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'S':
			Sflag = 1;
			break;
		case 's':
			shortoutput = 1;
			break;
		case 'T':
			Tflag = RT_TFLAG;
			break;
		case 't':
			tflag = 1;
			break;
		case 'v':
			verbose = RT_VFLAG;
			break;
		case '?':
		default:
			usage(NULL);
			/*NOTREACHED*/
		}
	argc -= optind;
	argv += optind;

	if (prog_init && prog_init() == -1)
		err(1, "init failed");

	pid = prog_getpid();
	if (tflag)
		sock = prog_open("/dev/null", O_WRONLY, 0);
	else
		sock = prog_socket(PF_ROUTE, SOCK_RAW, 0);
	if (sock < 0)
		err(EXIT_FAILURE, "socket");

	if (*argv == NULL) {
		if (doflush)
			ch = K_FLUSH;
		else
			goto no_cmd;
	} else
		ch = keyword(*argv);

	switch (ch) {
#ifndef SMALL
	case K_GET:
#endif /* SMALL */
	case K_CHANGE:
	case K_ADD:
	case K_DELETE:
		if (doflush)
			(void)flushroutes(1, argv, 0);
		return newroute(argc, argv);

	case K_SHOW:
		show(argc, argv, Lflag|nflag|Tflag|verbose);
		return 0;

#ifndef SMALL
	case K_MONITOR:
		monitor();
		return 0;

#endif /* SMALL */
	case K_FLUSH:
		return flushroutes(argc, argv, 0);

	case K_FLUSHALL:
		return flushroutes(argc, argv, 1);
	no_cmd:
	default:
		usage(*argv);
		/*NOTREACHED*/
	}
}

static char *
netmask_string(const struct sockaddr *mask, int len, int family)
{
	static char smask[INET6_ADDRSTRLEN];
	struct sockaddr_in nsin;
	struct sockaddr_in6 nsin6;

	if (len >= 0)
		snprintf(smask, sizeof(smask), "%d", len);
	else {
		switch (family) {
		case AF_INET:
			memset(&nsin, 0, sizeof(nsin));
			memcpy(&nsin, mask, mask->sa_len);
			snprintf(smask, sizeof(smask), "%s",
			    inet_ntoa(nsin.sin_addr));
			break;
		case AF_INET6:
			memset(&nsin6, 0, sizeof(nsin6));
			memcpy(&nsin6, mask, mask->sa_len);
			inet_ntop(family, &nsin6.sin6_addr, smask,
			    sizeof(smask));
			break;
		default:
			snprintf(smask, sizeof(smask), "%s", any_ntoa(mask));
		}
	}

	return smask;
}
/*
 * Purge all entries in the routing tables not
 * associated with network interfaces.
 */
static int
flushroutes(int argc, char * const argv[], int doall)
{
	struct sockaddr *sa;
	size_t needed;
	int flags, mib[6], rlen, seqno;
	char *buf, *next, *lim;
	const char *afname;
	struct rt_msghdr *rtm;

	flags = 0;
	af = AF_UNSPEC;
	/* Don't want to read back our messages */
	prog_shutdown(sock, SHUT_RD);
	parse_show_opts(argc, argv, &af, &flags, &afname, false);
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	if (prog_sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(EXIT_FAILURE, "route-sysctl-estimate");
	buf = lim = NULL;
	if (needed) {
		if ((buf = malloc(needed)) == NULL)
			err(EXIT_FAILURE, "malloc");
		if (prog_sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
			err(EXIT_FAILURE, "actual retrieval of routing table");
		lim = buf + needed;
	}
	if (verbose) {
		(void)printf("Examining routing table from sysctl\n");
		if (af != AF_UNSPEC)
			printf("(address family %s)\n", afname);
	}
	if (needed == 0)
		return 0;
	seqno = 0;		/* ??? */
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sa = (struct sockaddr *)(rtm + 1);
		if (verbose)
			print_rtmsg(rtm, rtm->rtm_msglen);
		if ((rtm->rtm_flags & flags) != flags)
			continue;
		if (!(rtm->rtm_flags & (RTF_GATEWAY | RTF_STATIC |
					RTF_LLINFO)) && !doall)
			continue;
#if defined(__minix)
		/*
		 * MINIX3 only: routes with the RTF_LOCAL flag are immutable,
		 * so do not try to delete them.
		 */
		if (rtm->rtm_flags & RTF_LOCAL)
			continue;
#endif /* defined(__minix) */
		if (af != AF_UNSPEC && sa->sa_family != af)
			continue;
		if (debugonly)
			continue;
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		if ((rlen = prog_write(sock, next,
		    rtm->rtm_msglen)) < 0) {
			warnx("writing to routing socket: %s",
			    route_strerror(errno));
			return 1;
		}
		if (rlen < (int)rtm->rtm_msglen) {
			warnx("write to routing socket, got %d for rlen", rlen);
			return 1;
		}
		seqno++;
		if (qflag)
			continue;
		if (verbose)
			print_rtmsg(rtm, rlen);
		else {
			(void)printf("%-20.20s ", netname(sa, NULL, nflag));
			sa = (struct sockaddr *)(RT_ROUNDUP(sa->sa_len) +
			    (char *)sa);
			(void)printf("%-20.20s ", routename(sa, nflag));
			(void)printf("done\n");
		}
	}
	free(buf);
	return 0;
}

static const char *
route_strerror(int error)
{

	switch (error) {
	case ESRCH:
		return "not in table";
	case EBUSY:
		return "entry in use";
	case ENOBUFS:
		return "routing table overflow";
	default:
		return strerror(error);
	}
}

static void
set_metric(const char *value, int key)
{
	int flag = 0;
	uint64_t noval, *valp = &noval;

	switch (key) {
#define caseof(x, y, z) \
	case x: valp = (uint64_t *)&rt_metrics.z; flag = y; break
	caseof(K_MTU, RTV_MTU, rmx_mtu);
	caseof(K_HOPCOUNT, RTV_HOPCOUNT, rmx_hopcount);
	caseof(K_EXPIRE, RTV_EXPIRE, rmx_expire);
	caseof(K_RECVPIPE, RTV_RPIPE, rmx_recvpipe);
	caseof(K_SENDPIPE, RTV_SPIPE, rmx_sendpipe);
	caseof(K_SSTHRESH, RTV_SSTHRESH, rmx_ssthresh);
	caseof(K_RTT, RTV_RTT, rmx_rtt);
	caseof(K_RTTVAR, RTV_RTTVAR, rmx_rttvar);
	}
	rtm_inits |= flag;
	if (lockrest || locking)
		rt_metrics.rmx_locks |= flag;
	if (locking)
		locking = 0;
	*valp = strtoul(value, NULL, 0);
}

static int
newroute(int argc, char *const *argv)
{
	const char *cmd, *dest = "", *gateway = "";
	int ishost = 0, ret, attempts, oerrno, flags = RTF_STATIC;
	int key;
	struct hostent *hp = 0;
	struct sou sou, *soup = &sou;

	sou.so_dst = calloc(1, sizeof(union sockunion));
	sou.so_gate = calloc(1, sizeof(union sockunion));
	sou.so_mask = calloc(1, sizeof(union sockunion));
	sou.so_genmask = calloc(1, sizeof(union sockunion));
	sou.so_ifa = calloc(1, sizeof(union sockunion));
	sou.so_ifp = calloc(1, sizeof(union sockunion));
	sou.so_mpls = calloc(1, sizeof(union sockunion));

	if (sou.so_dst == NULL || sou.so_gate == NULL || sou.so_mask == NULL ||
	    sou.so_genmask == NULL || sou.so_ifa == NULL || sou.so_ifp == NULL ||
	    sou.so_mpls == NULL)
		errx(EXIT_FAILURE, "Cannot allocate memory");

	cmd = argv[0];
	af = AF_UNSPEC;
	if (*cmd != 'g') {
		/* Don't want to read back our messages */
		prog_shutdown(sock, SHUT_RD);
	}
	while (--argc > 0) {
		if (**(++argv)== '-') {
			switch (key = keyword(1 + *argv)) {

			case K_SA:
				af = PF_ROUTE;
				aflen = sizeof(union sockunion);
				break;

#ifndef SMALL
			case K_ATALK:
				af = AF_APPLETALK;
				aflen = sizeof(struct sockaddr_at);
				break;
#endif

			case K_INET:
				af = AF_INET;
				aflen = sizeof(struct sockaddr_in);
				break;

#ifdef INET6
			case K_INET6:
				af = AF_INET6;
				aflen = sizeof(struct sockaddr_in6);
				break;
#endif

			case K_LINK:
				af = AF_LINK;
				aflen = sizeof(struct sockaddr_dl);
				break;

#ifndef SMALL
			case K_MPLS:
				af = AF_MPLS;
				aflen = sizeof(struct sockaddr_mpls);
				break;
			case K_TAG:
				if (!--argc)
					usage(1+*argv);
				af = AF_MPLS;
				aflen = sizeof(struct sockaddr_mpls);
				(void)getaddr(RTA_TAG, *++argv, 0, soup);
				break;
#endif /* SMALL */

			case K_IFACE:
			case K_INTERFACE:
				iflag++;
				break;
			case K_NOSTATIC:
				flags &= ~RTF_STATIC;
				break;
			case K_LLINFO:
				flags |= RTF_LLINFO;
				break;
			case K_LOCK:
				locking = 1;
				break;
			case K_LOCKREST:
				lockrest = 1;
				break;
			case K_HOST:
				forcehost++;
				break;
			case K_REJECT:
				flags |= RTF_REJECT;
				break;
			case K_NOREJECT:
				flags &= ~RTF_REJECT;
				break;
			case K_BLACKHOLE:
				flags |= RTF_BLACKHOLE;
				break;
			case K_NOBLACKHOLE:
				flags &= ~RTF_BLACKHOLE;
				break;
			case K_CLONED:
				flags |= RTF_CLONED;
				break;
			case K_NOCLONED:
				flags &= ~RTF_CLONED;
				break;
			case K_PROTO1:
				flags |= RTF_PROTO1;
				break;
			case K_PROTO2:
				flags |= RTF_PROTO2;
				break;
			case K_PROXY:
				flags |= RTF_ANNOUNCE;
				break;
			case K_CLONING:
				flags |= RTF_CLONING;
				break;
			case K_NOCLONING:
				flags &= ~RTF_CLONING;
				break;
			case K_XRESOLVE:
				flags |= RTF_XRESOLVE;
				break;
			case K_STATIC:
				flags |= RTF_STATIC;
				break;
			case K_IFA:
				if (!--argc)
					usage(1+*argv);
				(void)getaddr(RTA_IFA, *++argv, 0, soup);
				break;
			case K_IFP:
				if (!--argc)
					usage(1+*argv);
				(void)getaddr(RTA_IFP, *++argv, 0, soup);
				break;
			case K_GENMASK:
				if (!--argc)
					usage(1+*argv);
				(void)getaddr(RTA_GENMASK, *++argv, 0, soup);
				break;
			case K_GATEWAY:
				if (!--argc)
					usage(1+*argv);
				(void)getaddr(RTA_GATEWAY, *++argv, 0, soup);
				break;
			case K_DST:
				if (!--argc)
					usage(1+*argv);
				ishost = getaddr(RTA_DST, *++argv, &hp, soup);
				dest = *argv;
				break;
			case K_NETMASK:
				if (!--argc)
					usage(1+*argv);
				(void)getaddr(RTA_NETMASK, *++argv, 0, soup);
				/* FALLTHROUGH */
			case K_NET:
				forcenet++;
				break;
			case K_PREFIXLEN:
				if (!--argc)
					usage(1+*argv);
				ishost = prefixlen(*++argv, soup);
				break;
			case K_MTU:
			case K_HOPCOUNT:
			case K_EXPIRE:
			case K_RECVPIPE:
			case K_SENDPIPE:
			case K_SSTHRESH:
			case K_RTT:
			case K_RTTVAR:
				if (!--argc)
					usage(1+*argv);
				set_metric(*++argv, key);
				break;
			default:
				usage(1+*argv);
			}
		} else {
			if ((rtm_addrs & RTA_DST) == 0) {
				dest = *argv;
				ishost = getaddr(RTA_DST, *argv, &hp, soup);
			} else if ((rtm_addrs & RTA_GATEWAY) == 0) {
				gateway = *argv;
				(void)getaddr(RTA_GATEWAY, *argv, &hp, soup);
			} else {
				ret = atoi(*argv);

				if (ret == 0) {
				    if (strcmp(*argv, "0") == 0) {
					if (!qflag)  {
					    warnx("%s, %s",
						"old usage of trailing 0",
						"assuming route to if");
					}
				    } else
					usage(NULL);
				    iflag = 1;
				    continue;
				} else if (ret > 0 && ret < 10) {
				    if (!qflag) {
					warnx("%s, %s",
					    "old usage of trailing digit",
					    "assuming route via gateway");
				    }
				    iflag = 0;
				    continue;
				}
				(void)getaddr(RTA_NETMASK, *argv, 0, soup);
			}
		}
	}
	if ((rtm_addrs & RTA_DST) == 0)
		errx(EXIT_FAILURE, "missing destination specification");
	if (*cmd == 'a' && (rtm_addrs & RTA_GATEWAY) == 0)
		errx(EXIT_FAILURE, "missing gateway specification");
	if (forcehost && forcenet)
		errx(EXIT_FAILURE, "-host and -net conflict");
	else if (forcehost)
		ishost = 1;
	else if (forcenet)
		ishost = 0;
	flags |= RTF_UP;
	if (ishost)
		flags |= RTF_HOST;
	if (iflag == 0)
		flags |= RTF_GATEWAY;
	for (attempts = 1; ; attempts++) {
		errno = 0;
		if ((ret = rtmsg(*cmd, flags, soup)) == 0)
			break;
		if (errno != ENETUNREACH && errno != ESRCH)
			break;
		if (af == AF_INET && *gateway && hp && hp->h_addr_list[1]) {
			hp->h_addr_list++;
			memmove(&soup->so_gate->sin.sin_addr, hp->h_addr_list[0],
			    hp->h_length);
		} else
			break;
	}
	if (*cmd == 'g')
		return ret != 0;
	if (!qflag) {
		oerrno = errno;
		(void)printf("%s %s %s", cmd, ishost? "host" : "net", dest);
		if (*gateway) {
			(void)printf(": gateway %s", gateway);
			if (attempts > 1 && ret == 0 && af == AF_INET)
			    (void)printf(" (%s)",
			        inet_ntoa(soup->so_gate->sin.sin_addr));
		}
		if (ret == 0)
			(void)printf("\n");
		else
			(void)printf(": %s\n", route_strerror(oerrno));
	}
	free(sou.so_dst);
	free(sou.so_gate);
	free(sou.so_mask);
	free(sou.so_genmask);
	free(sou.so_ifa);
	free(sou.so_ifp);
	free(sou.so_mpls);

	return ret != 0;
}

static void
inet_makenetandmask(const u_int32_t net, struct sockaddr_in * const isin,
    struct sou *soup)
{
	struct sockaddr_in *sin;
	u_int32_t addr, mask = 0;
	char *cp;

	rtm_addrs |= RTA_NETMASK;
	if (net == 0)
		mask = addr = 0;
	else if (net < 128) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSA_NET;
	} else if (net < 192) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 224) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSC_NET;
	} else if (net < 256) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSD_NET;
	} else if (net < 49152) { /* 192 * 256 */
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 57344) { /* 224 * 256 */
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSC_NET;
	} else if (net < 65536) {
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 14680064L) { /* 224 * 65536 */
		addr = net << IN_CLASSC_NSHIFT;
		mask = IN_CLASSC_NET;
	} else if (net < 16777216L) { 
		addr = net << IN_CLASSC_NSHIFT;
		mask = IN_CLASSD_NET;
	} else {
		addr = net;
		if ((addr & IN_CLASSA_HOST) == 0)
			mask =  IN_CLASSA_NET;
		else if ((addr & IN_CLASSB_HOST) == 0)
			mask =  IN_CLASSB_NET;
		else if ((addr & IN_CLASSC_HOST) == 0)
			mask =  IN_CLASSC_NET;
		else
			mask = -1;
	}
	isin->sin_addr.s_addr = htonl(addr);
	sin = &soup->so_mask->sin;
	sin->sin_addr.s_addr = htonl(mask);
	sin->sin_len = 0;
	sin->sin_family = 0;
	cp = (char *)(&sin->sin_addr + 1);
	while (*--cp == 0 && cp > (char *)sin)
		;
	sin->sin_len = 1 + cp - (char *)sin;
	sin->sin_family = AF_INET;
}

#ifdef INET6
/*
 * XXX the function may need more improvement...
 */
static int
inet6_makenetandmask(const struct sockaddr_in6 * const sin6, struct sou *soup)
{
	const char *plen;
	struct in6_addr in6;

	plen = NULL;
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) &&
	    sin6->sin6_scope_id == 0) {
		plen = "0";
	} else if ((sin6->sin6_addr.s6_addr[0] & 0xe0) == 0x20) {
		/* aggregatable global unicast - RFC2374 */
		memset(&in6, 0, sizeof(in6));
		if (!memcmp(&sin6->sin6_addr.s6_addr[8], &in6.s6_addr[8], 8))
			plen = "64";
	}

	if (!plen || strcmp(plen, "128") == 0)
		return 1;
	else {
		rtm_addrs |= RTA_NETMASK;
		(void)prefixlen(plen, soup);
		return 0;
	}
}
#endif

/*
 * Interpret an argument as a network address of some kind,
 * returning 1 if a host address, 0 if a network address.
 */
static int
getaddr(int which, const char *s, struct hostent **hpp, struct sou *soup)
{
	sup su;
	struct hostent *hp;
	struct netent *np;
	u_int32_t val;
	char *t;
	int afamily;  /* local copy of af so we can change it */

	if (af == AF_UNSPEC) {
		af = AF_INET;
		aflen = sizeof(struct sockaddr_in);
	}
	afamily = af;
	rtm_addrs |= which;
	switch (which) {
	case RTA_DST:
		su = soup->so_dst;
		break;
	case RTA_GATEWAY:
		su = soup->so_gate;
		break;
	case RTA_NETMASK:
		su = soup->so_mask;
		break;
	case RTA_GENMASK:
		su = soup->so_genmask;
		break;
	case RTA_IFP:
		su = soup->so_ifp;
		afamily = AF_LINK;
		break;
	case RTA_IFA:
		su = soup->so_ifa;
		su->sa.sa_family = af;
		break;
#ifndef SMALL
	case RTA_TAG:
		su = soup->so_mpls;
		afamily = AF_MPLS;
		break;
#endif
	default:
		su = NULL;
		usage("Internal Error");
		/*NOTREACHED*/
	}
	su->sa.sa_len = aflen;
	su->sa.sa_family = afamily; /* cases that don't want it have left already */
	if (strcmp(s, "default") == 0) {
		switch (which) {
		case RTA_DST:
			forcenet++;
			(void)getaddr(RTA_NETMASK, s, 0, soup);
			break;
		case RTA_NETMASK:
		case RTA_GENMASK:
			su->sa.sa_len = 0;
		}
		return 0;
	}
	switch (afamily) {
#ifdef INET6
	case AF_INET6:
	    {
		struct addrinfo hints, *res;
		char *slash = 0;

		if (which == RTA_DST && (slash = (strrchr(s, '/'))) != 0)
			*slash = '\0';
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = afamily;	/*AF_INET6*/
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
		if (getaddrinfo(s, "0", &hints, &res) != 0) {
			hints.ai_flags = 0;
			if (slash) {
				*slash = '/';
				slash = 0;
			}
			if (getaddrinfo(s, "0", &hints, &res) != 0)
				errx(EXIT_FAILURE, "%s: bad value", s);
		}
		if (slash)
			*slash = '/';
		if (sizeof(su->sin6) != res->ai_addrlen)
			errx(EXIT_FAILURE, "%s: bad value", s);
		if (res->ai_next) {
			errx(EXIT_FAILURE,
			    "%s: address resolved to multiple values", s);
		}
		memcpy(&su->sin6, res->ai_addr, sizeof(su->sin6));
		freeaddrinfo(res);
		inet6_putscopeid(&su->sin6, INET6_IS_ADDR_LINKLOCAL|
		    INET6_IS_ADDR_MC_LINKLOCAL);
		if (hints.ai_flags == AI_NUMERICHOST) {
			if (slash)
				return prefixlen(slash + 1, soup);
			if (which == RTA_DST)
				return inet6_makenetandmask(&su->sin6, soup);
			return 0;
		} else
			return 1;
	    }
#endif

	case PF_ROUTE:
		su->sa.sa_len = sizeof(*su);
		sockaddr(s, &su->sa);
		return 1;

#ifndef SMALL
	case AF_APPLETALK:
		t = strchr (s, '.');
		if (!t) {
badataddr:
			errx(EXIT_FAILURE, "bad address: %s", s);
		}
		val = atoi (s);
		if (val > 65535)
			goto badataddr;
		su->sat.sat_addr.s_net = val;
		val = atoi (t);
		if (val > 256)
			goto badataddr;
		su->sat.sat_addr.s_node = val;
		rtm_addrs |= RTA_NETMASK;
		return(forcehost || su->sat.sat_addr.s_node != 0);
	case AF_MPLS:
		if (which == RTA_DST)
			soup->so_dst = readtag(su, s);
		else if (which == RTA_TAG)
			soup->so_mpls = readtag(su, s);
		else
			errx(EXIT_FAILURE, "MPLS can be used only as "
			    "DST or TAG");
		return 1;
#endif

	case AF_LINK:
		link_addr(s, &su->sdl);
		return 1;

	case AF_INET:
	default:
		break;
	}

	if (hpp == NULL)
		hpp = &hp;
	*hpp = NULL;

	if ((t = strchr(s, '/')) != NULL && which == RTA_DST) {
		*t = '\0';
		if (forcenet == 0) {
			if ((val = inet_addr(s)) != INADDR_NONE) {
				inet_makenetandmask(htonl(val), &su->sin, soup);
				return prefixlen(&t[1], soup);
			}
		} else {
			if ((val = inet_network(s)) != INADDR_NONE) {
				inet_makenetandmask(val, &su->sin, soup);
				return prefixlen(&t[1], soup);
			}
		}
		*t = '/';
	}
	if (inet_aton(s, &su->sin.sin_addr) &&
	    (which != RTA_DST || forcenet == 0)) {
		val = su->sin.sin_addr.s_addr;
		if (inet_lnaof(su->sin.sin_addr) != INADDR_ANY)
			return 1;
		else {
			val = ntohl(val);
			goto netdone;
		}
	}
	if ((val = inet_network(s)) != INADDR_NONE ||
	    ((np = getnetbyname(s)) != NULL && (val = np->n_net) != 0)) {
netdone:
		if (which == RTA_DST)
			inet_makenetandmask(val, &su->sin, soup);
		return 0;
	}
	hp = gethostbyname(s);
	if (hp) {
		*hpp = hp;
		su->sin.sin_family = hp->h_addrtype;
		memmove(&su->sin.sin_addr, hp->h_addr, hp->h_length);
		return 1;
	}
	errx(EXIT_FAILURE, "%s: bad value", s);
	/*NOTREACHED*/
}

#ifndef SMALL
static sup
readtag(sup su, const char *s)
{
	char *p, *n, *norig;
	int mplssize = 0;
	sup retsu = su;

	n = strdup(s);
	if (n == NULL)
		errx(EXIT_FAILURE, "%s: Cannot allocate memory", s);
	norig = n;
	for (uint i = 0; i < strlen(n); i++)
		if(n[i] == ',')
			mplssize++;

#define MPLS_NEW_SIZE (sizeof(struct sockaddr_mpls) + \
    mplssize * sizeof(union mpls_shim))

	if (mplssize != 0 && sizeof(union sockunion) < MPLS_NEW_SIZE) {
		free(su);
		retsu = malloc(MPLS_NEW_SIZE);
		retsu->smpls.smpls_family = AF_MPLS;
	}
	retsu->smpls.smpls_len = MPLS_NEW_SIZE;
	mplssize = 0;
	while ((p = strchr(n, ',')) != NULL) {
		p[0] = '\0';
		addtag(retsu, n, mplssize);
		n = p + 1;
		mplssize++;
	}
	addtag(retsu, n, mplssize);

	free(norig);
	return retsu;
}

static void
addtag(sup su, const char *s, int where)
{
	union mpls_shim *ms = &su->smpls.smpls_addr;

	if (atoi(s) < 0 || atoi(s) >= (1 << 20))
		errx(EXIT_FAILURE, "%s: Bad tag", s);
	ms[where].s_addr = 0;
	ms[where].shim.label = atoi(s);
	ms[where].s_addr = htonl(ms[where].s_addr);
}
#endif	/* SMALL */

int
prefixlen(const char *s, struct sou *soup)
{
	int max, len = atoi(s);
#ifdef INET6
	int q, r;
#endif

	switch (af) {
	case AF_INET:
		max = sizeof(struct in_addr) * 8;
		break;
#ifdef INET6
	case AF_INET6:
		max = sizeof(struct in6_addr) * 8;
		break;
#endif
	default:
		errx(EXIT_FAILURE, "prefixlen is not supported with af %d", af);
		/*NOTREACHED*/
	}

	rtm_addrs |= RTA_NETMASK;	
	if (len < -1 || len > max)
		errx(EXIT_FAILURE, "%s: bad value", s);
	
#ifdef INET6
	q = len >> 3;
	r = len & 7;
#endif
	switch (af) {
	case AF_INET:
		memset(soup->so_mask, 0, sizeof(*soup->so_mask));
		soup->so_mask->sin.sin_family = AF_INET;
		soup->so_mask->sin.sin_len = sizeof(struct sockaddr_in);
		soup->so_mask->sin.sin_addr.s_addr = (len == 0 ? 0
				: htonl(0xffffffff << (32 - len)));
		break;
#ifdef INET6
	case AF_INET6:
		soup->so_mask->sin6.sin6_family = AF_INET6;
		soup->so_mask->sin6.sin6_len = sizeof(struct sockaddr_in6);
		memset(&soup->so_mask->sin6.sin6_addr, 0,
			sizeof(soup->so_mask->sin6.sin6_addr));
		if (q > 0)
			memset(&soup->so_mask->sin6.sin6_addr, 0xff, q);
		if (r > 0)
			*((u_char *)&soup->so_mask->sin6.sin6_addr + q) =
			    (0xff00 >> r) & 0xff;
		break;
#endif
	}
	return len == max;
}

#ifndef SMALL
static void
interfaces(void)
{
	size_t needed;
	int mib[6];
	char *buf, *lim, *next;
	struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		/* no flags */
	if (prog_sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(EXIT_FAILURE, "route-sysctl-estimate");
	if (needed) {
		if ((buf = malloc(needed)) == NULL)
			err(EXIT_FAILURE, "malloc");
		if (prog_sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
			err(EXIT_FAILURE,
			    "actual retrieval of interface table");
		}
		lim = buf + needed;
		for (next = buf; next < lim; next += rtm->rtm_msglen) {
			rtm = (struct rt_msghdr *)next;
			print_rtmsg(rtm, rtm->rtm_msglen);
		}
		free(buf);
	}
}

static void
monitor(void)
{
	int n;
	union {
		char msg[2048];
		struct rt_msghdr hdr;
	} u;

	verbose = 1;
	if (debugonly) {
		interfaces();
		exit(0);
	}
	for(;;) {
		time_t now;
		n = prog_read(sock, &u, sizeof(u));
		now = time(NULL);
		(void)printf("got message of size %d on %s", n, ctime(&now));
		print_rtmsg(&u.hdr, n);
	}
}

#endif /* SMALL */


struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} m_rtmsg;

static int
rtmsg(int cmd, int flags, struct sou *soup)
{
	static int seq;
	int rlen;
	char *cp = m_rtmsg.m_space;
	int l;

#define NEXTADDR(w, u) \
	if (rtm_addrs & (w)) {\
	    l = RT_ROUNDUP(u->sa.sa_len); memmove(cp, u, l); cp += l;\
	    if (verbose && ! shortoutput) sodump(u,#u);\
	}

	errno = 0;
	memset(&m_rtmsg, 0, sizeof(m_rtmsg));
	if (cmd == 'a')
		cmd = RTM_ADD;
	else if (cmd == 'c')
		cmd = RTM_CHANGE;
	else if (cmd == 'g') {
#ifdef	SMALL
		return -1;
#else	/* SMALL */
		cmd = RTM_GET;
		if (soup->so_ifp->sa.sa_family == AF_UNSPEC) {
			soup->so_ifp->sa.sa_family = AF_LINK;
			soup->so_ifp->sa.sa_len = sizeof(struct sockaddr_dl);
			rtm_addrs |= RTA_IFP;
		}
#endif	/* SMALL */
	} else
		cmd = RTM_DELETE;
#define rtm m_rtmsg.m_rtm
	rtm.rtm_type = cmd;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;

	if (rtm_addrs & RTA_NETMASK)
		mask_addr(soup);
	NEXTADDR(RTA_DST, soup->so_dst);
	NEXTADDR(RTA_GATEWAY, soup->so_gate);
	NEXTADDR(RTA_NETMASK, soup->so_mask);
	NEXTADDR(RTA_GENMASK, soup->so_genmask);
	NEXTADDR(RTA_IFP, soup->so_ifp);
	NEXTADDR(RTA_IFA, soup->so_ifa);
#ifndef SMALL
	NEXTADDR(RTA_TAG, soup->so_mpls);
#endif
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;
	if (verbose && ! shortoutput) {
		if (rtm_addrs)
			putchar('\n');
		print_rtmsg(&rtm, l);
	}
	if (debugonly)
		return 0;
	if ((rlen = prog_write(sock, (char *)&m_rtmsg, l)) < 0) {
		warnx("writing to routing socket: %s", route_strerror(errno));
		return -1;
	}
	if (rlen < l) {
		warnx("write to routing socket, got %d for rlen", rlen);
		return 1;
	}
#ifndef	SMALL
	if (cmd == RTM_GET) {
		do {
			l = prog_read(sock,
			    (char *)&m_rtmsg, sizeof(m_rtmsg));
		} while (l > 0 && (rtm.rtm_seq != seq || rtm.rtm_pid != pid));
		if (l < 0)
			err(EXIT_FAILURE, "read from routing socket");
		else
			return print_getmsg(&rtm, l, soup);
	}
#endif	/* SMALL */
#undef rtm
	return 0;
}

static void
mask_addr(struct sou *soup)
{
	int olen = soup->so_mask->sa.sa_len;
	char *cp1 = olen + (char *)soup->so_mask, *cp2;

	for (soup->so_mask->sa.sa_len = 0; cp1 > (char *)soup->so_mask; )
		if (*--cp1 != 0) {
			soup->so_mask->sa.sa_len = 1 + cp1 - (char *)soup->so_mask;
			break;
		}
	if ((rtm_addrs & RTA_DST) == 0)
		return;
	switch (soup->so_dst->sa.sa_family) {
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
#ifndef SMALL
	case AF_APPLETALK:
#endif /* SMALL */
	case 0:
		return;
	}
	cp1 = soup->so_mask->sa.sa_len + 1 + (char *)soup->so_dst;
	cp2 = soup->so_dst->sa.sa_len + 1 + (char *)soup->so_dst;
	while (cp2 > cp1)
		*--cp2 = 0;
	cp2 = soup->so_mask->sa.sa_len + 1 + (char *)soup->so_mask;
	while (cp1 > soup->so_dst->sa.sa_data)
		*--cp1 &= *--cp2;
}

const char * const msgtypes[] = {
	[RTM_ADD] = "RTM_ADD: Add Route",
	[RTM_DELETE] = "RTM_DELETE: Delete Route",
	[RTM_CHANGE] = "RTM_CHANGE: Change Metrics, Flags or Gateway",
	[RTM_GET] = "RTM_GET: Report Metrics",
	[RTM_LOSING] = "RTM_LOSING: Kernel Suspects Partitioning",
	[RTM_REDIRECT] = "RTM_REDIRECT: Told to use different route",
	[RTM_MISS] = "RTM_MISS: Lookup failed on this address",
	[RTM_LOCK] = "RTM_LOCK: fix specified metrics",
	[RTM_OLDADD] = "RTM_OLDADD: caused by SIOCADDRT",
	[RTM_OLDDEL] = "RTM_OLDDEL: caused by SIOCDELRT",
	[RTM_RESOLVE] = "RTM_RESOLVE: Route created by cloning",
	[RTM_NEWADDR] = "RTM_NEWADDR: address being added to iface",
	[RTM_DELADDR] = "RTM_DELADDR: address being removed from iface",
	[RTM_OOIFINFO] = "RTM_OOIFINFO: iface status change (pre-1.5)",
	[RTM_OIFINFO] = "RTM_OIFINFO: iface status change (pre-64bit time)",
	[RTM_IFANNOUNCE] = "RTM_IFANNOUNCE: iface arrival/departure",
	[RTM_IEEE80211] = "RTM_IEEE80211: IEEE80211 wireless event",
	[RTM_IFINFO] = "RTM_IFINFO: iface status change",
	[RTM_CHGADDR] = "RTM_CHGADDR: address being changed on iface",
};

const char metricnames[] =
"\011pksent\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire\2hopcount\1mtu";
const char routeflags[] =
"\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE\010MASK_PRESENT\011CLONING\012XRESOLVE\013LLINFO\014STATIC\015BLACKHOLE\016CLONED\017PROTO2\020PROTO1\023LOCAL\024BROADCAST";
const char ifnetflags[] =
"\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5PTP\6NOTRAILERS\7RUNNING\010NOARP\011PPROMISC\012ALLMULTI\013OACTIVE\014SIMPLEX\015LINK0\016LINK1\017LINK2\020MULTICAST";
const char addrnames[] =
"\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD\011TAG";


#ifndef SMALL
static const char *
linkstate(struct if_msghdr *ifm)
{
	static char buf[64];

	switch (ifm->ifm_data.ifi_link_state) {
	case LINK_STATE_UNKNOWN:
		return "carrier: unknown";
	case LINK_STATE_DOWN:
		return "carrier: no carrier";
	case LINK_STATE_UP:
		return "carrier: active";
	default:
		(void)snprintf(buf, sizeof(buf), "carrier: 0x%x",
		    ifm->ifm_data.ifi_link_state);
		return buf;
	}
}
#endif /* SMALL */

static void
print_rtmsg(struct rt_msghdr *rtm, int msglen)
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct if_announcemsghdr *ifan;
	union {
		struct ieee80211_join_event join;
		struct ieee80211_leave_event leave;
		struct ieee80211_replay_event replay;
		struct ieee80211_michael_event michael;
	} ev;
	size_t evlen = 0;

	if (verbose == 0)
		return;
	if (rtm->rtm_version != RTM_VERSION) {
		(void)printf("routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	if (msgtypes[rtm->rtm_type])
		(void)printf("%s: ", msgtypes[rtm->rtm_type]);
	else
		(void)printf("#%d: ", rtm->rtm_type);
	(void)printf("len %d, ", rtm->rtm_msglen);
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		(void)printf("if# %d, %s, flags: ", ifm->ifm_index,
#ifdef SMALL
		    ""
#else
		    linkstate(ifm)
#endif /* SMALL */
		    );
		bprintf(stdout, ifm->ifm_flags, ifnetflags);
		pmsg_addrs((char *)(ifm + 1), ifm->ifm_addrs);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
	case RTM_CHGADDR:
		ifam = (struct ifa_msghdr *)rtm;
		(void)printf("metric %d, flags: ", ifam->ifam_metric);
		bprintf(stdout, ifam->ifam_flags, routeflags);
		pmsg_addrs((char *)(ifam + 1), ifam->ifam_addrs);
		break;
	case RTM_IEEE80211:
		ifan = (struct if_announcemsghdr *)rtm;
		(void)printf("if# %d, what: ", ifan->ifan_index);
		switch (ifan->ifan_what) {
		case RTM_IEEE80211_ASSOC:
			printf("associate");
			break;
		case RTM_IEEE80211_REASSOC:
			printf("re-associate");
			break;
		case RTM_IEEE80211_DISASSOC:
			printf("disassociate");
			break;
		case RTM_IEEE80211_SCAN:
			printf("scan complete");
			break;
		case RTM_IEEE80211_JOIN:
			evlen = sizeof(ev.join);
			printf("join");
			break;
		case RTM_IEEE80211_LEAVE:
			evlen = sizeof(ev.leave);
			printf("leave");
			break;
		case RTM_IEEE80211_MICHAEL:
			evlen = sizeof(ev.michael);
			printf("michael");
			break;
		case RTM_IEEE80211_REPLAY:
			evlen = sizeof(ev.replay);
			printf("replay");
			break;
		default:
			evlen = 0;
			printf("#%d", ifan->ifan_what);
			break;
		}
		if (sizeof(*ifan) + evlen > ifan->ifan_msglen) {
			printf(" (truncated)\n");
			break;
		}
		(void)memcpy(&ev, (ifan + 1), evlen);
		switch (ifan->ifan_what) {
		case RTM_IEEE80211_JOIN:
		case RTM_IEEE80211_LEAVE:
			printf(" mac %" PRIETHER,
			    PRIETHER_ARGS(ev.join.iev_addr));
			break;
		case RTM_IEEE80211_REPLAY:
		case RTM_IEEE80211_MICHAEL:
			printf(" src %" PRIETHER " dst %" PRIETHER
			       " cipher %" PRIu8 " keyix %" PRIu8,
			       PRIETHER_ARGS(ev.replay.iev_src),
			       PRIETHER_ARGS(ev.replay.iev_dst),
			       ev.replay.iev_cipher,
			       ev.replay.iev_keyix);
			if (ifan->ifan_what == RTM_IEEE80211_REPLAY) {
				printf(" key rsc %#" PRIx64
				       " frame rsc %#" PRIx64,
				       ev.replay.iev_keyrsc, ev.replay.iev_rsc);
			}
			break;
		default:
			break;
		}
		printf("\n");
		break;
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		(void)printf("if# %d, what: ", ifan->ifan_index);
		switch (ifan->ifan_what) {
		case IFAN_ARRIVAL:
			printf("arrival");
			break;
		case IFAN_DEPARTURE:
			printf("departure");
			break;
		default:
			printf("#%d", ifan->ifan_what);
			break;
		}
		printf("\n");
		break;
	default:
		(void)printf("pid %d, seq %d, errno %d, flags: ",
			rtm->rtm_pid, rtm->rtm_seq, rtm->rtm_errno);
		bprintf(stdout, rtm->rtm_flags, routeflags);
		pmsg_common(rtm);
	}
}

#ifndef	SMALL
static int
print_getmsg(struct rt_msghdr *rtm, int msglen, struct sou *soup)
{
	struct sockaddr *dst = NULL, *gate = NULL, *mask = NULL, *ifa = NULL, *mpls = NULL;
	struct sockaddr_dl *ifp = NULL;
	struct sockaddr *sa;
	char *cp;
	int i;

	if (! shortoutput) {
		(void)printf("   route to: %s\n",
		    routename(&soup->so_dst->sa, nflag));
	}
	if (rtm->rtm_version != RTM_VERSION) {
		warnx("routing message version %d not understood",
		    rtm->rtm_version);
		return 1;
	}
	if (rtm->rtm_msglen > msglen) {
		warnx("message length mismatch, in packet %d, returned %d",
		    rtm->rtm_msglen, msglen);
	}
	if (rtm->rtm_errno)  {
		warnx("RTM_GET: %s (errno %d)",
		    strerror(rtm->rtm_errno), rtm->rtm_errno);
		return 1;
	}
	cp = ((char *)(rtm + 1));
	if (rtm->rtm_addrs)
		for (i = 1; i; i <<= 1)
			if (i & rtm->rtm_addrs) {
				sa = (struct sockaddr *)cp;
				switch (i) {
				case RTA_DST:
					dst = sa;
					break;
				case RTA_GATEWAY:
					gate = sa;
					break;
				case RTA_NETMASK:
					mask = sa;
					break;
				case RTA_IFP:
					if (sa->sa_family == AF_LINK &&
					   ((struct sockaddr_dl *)sa)->sdl_nlen)
						ifp = (struct sockaddr_dl *)sa;
					break;
				case RTA_IFA:
					ifa = sa;
					break;
				case RTA_TAG:
					mpls = sa;
					break;
				}
				RT_ADVANCE(cp, sa);
			}
	if (dst && mask)
		mask->sa_family = dst->sa_family;	/* XXX */
	if (dst && ! shortoutput)
		(void)printf("destination: %s\n",
		    routename(dst, nflag));
	if (mask && ! shortoutput) {
		int savenflag = nflag;

		nflag = RT_NFLAG;
		(void)printf("       mask: %s\n",
		    routename(mask, nflag));
		nflag = savenflag;
	}
	if (gate && rtm->rtm_flags & RTF_GATEWAY) {
		const char *name;

		name = routename(gate, nflag);
		if (shortoutput) {
			if (*name == '\0')
				return 1;
			(void)printf("%s\n", name);
		} else
			(void)printf("    gateway: %s\n", name);
	}
	if (mpls) {
		const char *name;
		name = routename(mpls, nflag);
		if(shortoutput) {
			if (*name == '\0')
				return 1;
			printf("%s\n", name);
		} else
			printf("        Tag: %s\n", name);
	}
		
	if (ifa && ! shortoutput)
		(void)printf(" local addr: %s\n",
		    routename(ifa, nflag));
	if (ifp && ! shortoutput)
		(void)printf("  interface: %.*s\n",
		    ifp->sdl_nlen, ifp->sdl_data);
	if (! shortoutput) {
		(void)printf("      flags: ");
		bprintf(stdout, rtm->rtm_flags, routeflags);
	}

#define lock(f)	((rtm->rtm_rmx.rmx_locks & __CONCAT(RTV_,f)) ? 'L' : ' ')
#define msec(u)	(((u) + 500) / 1000)		/* usec to msec */

	if (! shortoutput) {
		(void)printf("\n%s\n", "\
 recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount      mtu     expire");
		printf("%8"PRId64"%c ", rtm->rtm_rmx.rmx_recvpipe, lock(RPIPE));
		printf("%8"PRId64"%c ", rtm->rtm_rmx.rmx_sendpipe, lock(SPIPE));
		printf("%8"PRId64"%c ", rtm->rtm_rmx.rmx_ssthresh, lock(SSTHRESH));
		printf("%8"PRId64"%c ", msec(rtm->rtm_rmx.rmx_rtt), lock(RTT));
		printf("%8"PRId64"%c ", msec(rtm->rtm_rmx.rmx_rttvar), lock(RTTVAR));
		printf("%8"PRId64"%c ", rtm->rtm_rmx.rmx_hopcount, lock(HOPCOUNT));
		printf("%8"PRId64"%c ", rtm->rtm_rmx.rmx_mtu, lock(MTU));
		if (rtm->rtm_rmx.rmx_expire)
			rtm->rtm_rmx.rmx_expire -= time(0);
		printf("%8"PRId64"%c\n", rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
	}
#undef lock
#undef msec
#define	RTA_IGN	(RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_IFP|RTA_IFA|RTA_BRD)

	if (shortoutput)
		return (rtm->rtm_addrs & RTF_GATEWAY) == 0;
	else if (verbose)
		pmsg_common(rtm);
	else if (rtm->rtm_addrs &~ RTA_IGN) {
		(void)printf("sockaddrs: ");
		bprintf(stdout, rtm->rtm_addrs, addrnames);
		putchar('\n');
	}
	return 0;
#undef	RTA_IGN
}
#endif	/* SMALL */

void
pmsg_common(struct rt_msghdr *rtm)
{
	(void)printf("\nlocks: ");
	bprintf(stdout, rtm->rtm_rmx.rmx_locks, metricnames);
	(void)printf(" inits: ");
	bprintf(stdout, rtm->rtm_inits, metricnames);
	pmsg_addrs((char *)(rtm + 1), rtm->rtm_addrs);
}

static void
extract_addrs(const char *cp, int addrs, const struct sockaddr *sa[], int *nmfp)
{
	int i, nmf = -1;

	for (i = 0; i < RTAX_MAX; i++) {
		if ((1 << i) & addrs) {
			sa[i] = (const struct sockaddr *)cp;
			if ((i == RTAX_DST || i == RTAX_IFA) &&
			    nmf == -1)
				nmf = sa[i]->sa_family;
			RT_ADVANCE(cp, sa[i]);
		} else
			sa[i] = NULL;
	}

	if (nmfp != NULL)
		*nmfp = nmf;
}

static void
pmsg_addrs(const char *cp, int addrs)
{
	const struct sockaddr *sa[RTAX_MAX];
	int i, nmf;

	if (addrs != 0) {
		(void)printf("\nsockaddrs: ");
		bprintf(stdout, addrs, addrnames);
		(void)putchar('\n');
		extract_addrs(cp, addrs, sa, &nmf);
		for (i = 0; i < RTAX_MAX; i++) {
			if (sa[i] == NULL)
				continue;

			if (i == RTAX_NETMASK && sa[i]->sa_len)
				(void)printf(" %s",
				    netmask_string(sa[i], -1, nmf));
			else
				(void)printf(" %s",
				    routename(sa[i], nflag));
		}
	}
	(void)putchar('\n');
	(void)fflush(stdout);
}

static void
bprintf(FILE *fp, int b, const char *f)
{
	int i;
	int gotsome = 0;
	const uint8_t *s = (const uint8_t *)f;

	if (b == 0) {
		fputs("none", fp);
		return;
	}
	while ((i = *s++) != 0) {
		if (b & (1 << (i-1))) {
			if (gotsome == 0)
				i = '<';
			else
				i = ',';
			(void)putc(i, fp);
			gotsome = 1;
			for (; (i = *s) > 32; s++)
				(void)putc(i, fp);
		} else
			while (*s > 32)
				s++;
	}
	if (gotsome)
		(void)putc('>', fp);
}

int
keyword(const char *cp)
{
	struct keytab *kt = keywords;

	while (kt->kt_cp && strcmp(kt->kt_cp, cp))
		kt++;
	return kt->kt_i;
}

static void
sodump(sup su, const char *which)
{
#ifdef INET6
	char ntop_buf[NI_MAXHOST];
#endif

	switch (su->sa.sa_family) {
	case AF_INET:
		(void)printf("%s: inet %s; ",
		    which, inet_ntoa(su->sin.sin_addr));
		break;
#ifndef SMALL
	case AF_APPLETALK:
		(void)printf("%s: atalk %d.%d; ",
		    which, su->sat.sat_addr.s_net, su->sat.sat_addr.s_node);
		break;
#endif
	case AF_LINK:
		(void)printf("%s: link %s; ",
		    which, link_ntoa(&su->sdl));
		break;
#ifdef INET6
	case AF_INET6:
		(void)printf("%s: inet6 %s; ",
		    which, inet_ntop(AF_INET6, &su->sin6.sin6_addr,
				     ntop_buf, sizeof(ntop_buf)));
		break;
#endif
#ifndef SMALL
	case AF_MPLS:
	    {
		union mpls_shim ms;
		const union mpls_shim *pms;
		int psize = sizeof(struct sockaddr_mpls);

		ms.s_addr = ntohl(su->smpls.smpls_addr.s_addr);
		printf("%s: mpls %u; ",
		    which, ms.shim.label);

		pms = &su->smpls.smpls_addr;
		while(psize < su->smpls.smpls_len) {
			pms++;
			ms.s_addr = ntohl(pms->s_addr);
			printf("%u; ", ms.shim.label);
			psize += sizeof(ms);
		}
		break;
	    }
#endif /* SMALL */
	default:
		(void)printf("%s: (%d) %s; ",
		    which, su->sa.sa_family, any_ntoa(&su->sa));
	}
	(void)fflush(stdout);
}

/* States*/
#define VIRGIN	0
#define GOTONE	1
#define GOTTWO	2
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define DELIM	(4*2)

static void
sockaddr(const char *addr, struct sockaddr *sa)
{
	char *cp = (char *)sa;
	int size = sa->sa_len;
	char *cplim = cp + size;
	int byte = 0, state = VIRGIN, new = 0;

	(void)memset(cp, 0, size);
	cp++;
	do {
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0)
			state |= END;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case GOTTWO | DIGIT:
			*cp++ = byte; /*FALLTHROUGH*/
		case VIRGIN | DIGIT:
			state = GOTONE; byte = new; continue;
		case GOTONE | DIGIT:
			state = GOTTWO; byte = new + (byte << 4); continue;
		default: /* | DELIM */
			state = VIRGIN; *cp++ = byte; byte = 0; continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte; /* FALLTHROUGH */
		case VIRGIN | END:
			break;
		}
		break;
	} while (cp < cplim);
	sa->sa_len = cp - (char *)sa;
}
