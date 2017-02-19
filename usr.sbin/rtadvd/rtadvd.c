/*	$NetBSD: rtadvd.c,v 1.50 2015/06/15 04:15:33 ozaki-r Exp $	*/
/*	$KAME: rtadvd.c,v 1.92 2005/10/17 14:40:02 suz Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#if defined(__NetBSD__) || defined(__minix)
#include <util.h>
#endif
#include <poll.h>
#include <pwd.h>

#include "rtadvd.h"
#include "rrenum.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"
#include "dump.h"

struct msghdr rcvmhdr;
static unsigned char *rcvcmsgbuf;
static size_t rcvcmsgbuflen;
static unsigned char *sndcmsgbuf;
static size_t sndcmsgbuflen;
volatile sig_atomic_t do_dump;
volatile sig_atomic_t do_reconf;
volatile sig_atomic_t do_die;
struct msghdr sndmhdr;
struct iovec rcviov[2];
struct iovec sndiov[2];
struct sockaddr_in6 rcvfrom;
static const char *dumpfilename = "/var/run/rtadvd.dump"; /* XXX configurable */
static char *mcastif;
int sock;
int rtsock = -1;
int accept_rr = 0;
int dflag = 0, sflag = 0;

static char **if_argv;
static int if_argc;

char *conffile = NULL;

struct ralist_head_t ralist = TAILQ_HEAD_INITIALIZER(ralist);

struct nd_optlist {
	TAILQ_ENTRY(nd_optlist) next;
	struct nd_opt_hdr *opt;
};
union nd_opts {
	struct nd_opt_hdr *nd_opt_array[9];
	struct {
		struct nd_opt_hdr *zero;
		struct nd_opt_hdr *src_lladdr;
		struct nd_opt_hdr *tgt_lladdr;
		struct nd_opt_prefix_info *pi;
		struct nd_opt_rd_hdr *rh;
		struct nd_opt_mtu *mtu;
		TAILQ_HEAD(, nd_optlist) list;
	} nd_opt_each;
};
#define nd_opts_src_lladdr	nd_opt_each.src_lladdr
#define nd_opts_tgt_lladdr	nd_opt_each.tgt_lladdr
#define nd_opts_pi		nd_opt_each.pi
#define nd_opts_rh		nd_opt_each.rh
#define nd_opts_mtu		nd_opt_each.mtu
#define nd_opts_list		nd_opt_each.list

#define NDOPT_FLAG_SRCLINKADDR	(1 << 0)
#define NDOPT_FLAG_TGTLINKADDR	(1 << 1)
#define NDOPT_FLAG_PREFIXINFO	(1 << 2)
#define NDOPT_FLAG_RDHDR	(1 << 3)
#define NDOPT_FLAG_MTU		(1 << 4)
#define NDOPT_FLAG_RDNSS	(1 << 5)
#define NDOPT_FLAG_DNSSL	(1 << 6)

uint32_t ndopt_flags[] = {
	[ND_OPT_SOURCE_LINKADDR] =	NDOPT_FLAG_SRCLINKADDR,
	[ND_OPT_TARGET_LINKADDR] =	NDOPT_FLAG_TGTLINKADDR,
	[ND_OPT_PREFIX_INFORMATION] =	NDOPT_FLAG_PREFIXINFO,
	[ND_OPT_REDIRECTED_HEADER] =	NDOPT_FLAG_RDHDR,
	[ND_OPT_MTU] =			NDOPT_FLAG_MTU,
	[ND_OPT_RDNSS] =		NDOPT_FLAG_RDNSS,
	[ND_OPT_DNSSL] =		NDOPT_FLAG_DNSSL,
};

struct sockaddr_in6 sin6_linklocal_allnodes = {
	.sin6_len =	sizeof(sin6_linklocal_allnodes),
	.sin6_family =	AF_INET6,
	.sin6_addr =	IN6ADDR_LINKLOCAL_ALLNODES_INIT,
};
#ifdef notdef
struct sockaddr_in6 sin6_linklocal_allrouters = {
	.sin6_len =	sizeof(sin6_linklocal_allrouters),
	.sin6_family =	AF_INET6,
	.sin6_addr =	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT,
};
#endif
struct sockaddr_in6 sin6_sitelocal_allrouters = {
	.sin6_len =	sizeof(sin6_sitelocal_allrouters),
	.sin6_family =	AF_INET6,
	.sin6_addr =	IN6ADDR_SITELOCAL_ALLROUTERS_INIT,
};

static void set_die(int);
static void die(void);
static void set_reconf(int);
static void sock_open(void);
static void rtsock_open(void);
static void rtadvd_input(void);
static void rs_input(int, struct nd_router_solicit *,
    struct in6_pktinfo *, struct sockaddr_in6 *);
static void ra_input(int, struct nd_router_advert *,
    struct in6_pktinfo *, struct sockaddr_in6 *);
static struct rainfo *ra_output(struct rainfo *);
static int prefix_check(struct nd_opt_prefix_info *, struct rainfo *,
    struct sockaddr_in6 *);
static int nd6_options(struct nd_opt_hdr *, int, union nd_opts *, uint32_t);
static void free_ndopts(union nd_opts *);
static void rtmsg_input(void);
static void rtadvd_set_dump_file(int);

int
main(int argc, char *argv[])
{
	struct pollfd set[2];
	struct timespec *timeout;
	int i, ch;
	int fflag = 0, logopt;
	struct passwd *pw;

	/* get command line options and arguments */
#define OPTIONS "c:dDfM:Rs"
	while ((ch = getopt(argc, argv, OPTIONS)) != -1) {
#undef OPTIONS
		switch (ch) {
		case 'c':
			conffile = optarg;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'D':
			dflag = 2;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'M':
			mcastif = optarg;
			break;
		case 'R':
			fprintf(stderr, "rtadvd: "
				"the -R option is currently ignored.\n");
			/* accept_rr = 1; */
			/* run anyway... */
			break;
		case 's':
			sflag = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		fprintf(stderr,
			"usage: rtadvd [-DdfRs] [-c conffile]"
			" [-M ifname] interface ...\n");
		exit(1);
	}

	logopt = LOG_NDELAY | LOG_PID;
	if (fflag)
		logopt |= LOG_PERROR;
	openlog("rtadvd", logopt, LOG_DAEMON);

	/* set log level */
	if (dflag == 0)
		(void)setlogmask(LOG_UPTO(LOG_ERR));
	if (dflag == 1)
		(void)setlogmask(LOG_UPTO(LOG_INFO));

	errno = 0; /* Ensure errno is 0 so we know if getpwnam errors or not */
	if ((pw = getpwnam(RTADVD_USER)) == NULL) {
		if (errno == 0)
			syslog(LOG_ERR,
			    "user %s does not exist, aborting",
			    RTADVD_USER);
		else
			syslog(LOG_ERR, "getpwnam: %s: %m", RTADVD_USER);
		exit(1);
	}

	/* timer initialization */
	rtadvd_timer_init();

	if_argc = argc;
	if_argv = argv;
	while (argc--)
		getconfig(*argv++, 1);

	if (!fflag)
		daemon(1, 0);

	sock_open();

#if defined(__NetBSD__) || defined(__minix)
	/* record the current PID */
	if (pidfile(NULL) < 0) {
		syslog(LOG_ERR,
		    "<%s> failed to open the pid log file, run anyway.",
		    __func__);
	}
#endif

	set[0].fd = sock;
	set[0].events = POLLIN;
	if (sflag == 0) {
		rtsock_open();
		set[1].fd = rtsock;
		set[1].events = POLLIN;
	} else
		set[1].fd = -1;

	syslog(LOG_INFO, "dropping privileges to %s", RTADVD_USER);
	if (chroot(pw->pw_dir) == -1) {
		syslog(LOG_ERR, "chroot: %s: %m", pw->pw_dir);
		exit(1);
	}
	if (chdir("/") == -1) {
		syslog(LOG_ERR, "chdir: /: %m");
		exit(1);
	}
	if (setgroups(1, &pw->pw_gid) == -1 ||
	    setgid(pw->pw_gid) == -1 || 
	    setuid(pw->pw_uid) == -1)
	{
		syslog(LOG_ERR, "failed to drop privileges: %m");
		exit(1);
	}

	signal(SIGINT, set_die);
	signal(SIGTERM, set_die);
	signal(SIGHUP, set_reconf);
	signal(SIGUSR1, rtadvd_set_dump_file);

	for (;;) {
		if (do_dump) {	/* SIGUSR1 */
			do_dump = 0;
			rtadvd_dump_file(dumpfilename);
		}

		if (do_reconf) { /* SIGHUP */
			do_reconf = 0;
			syslog(LOG_INFO, "<%s> reloading config on SIGHUP",
			       __func__);
			argc = if_argc;
			argv = if_argv;
			while (argc--)
				getconfig(*argv++, 0);
		}

		/* timer expiration check and reset the timer */
		timeout = rtadvd_check_timer();

		if (do_die) {
			die();
			/*NOTREACHED*/
		}

		if (timeout != NULL) {
			syslog(LOG_DEBUG,
			    "<%s> set timer to %ld:%ld. waiting for "
			    "inputs or timeout", __func__,
			    (long int)timeout->tv_sec,
			    (long int)timeout->tv_nsec);
		} else {
			syslog(LOG_DEBUG,
			    "<%s> there's no timer. waiting for inputs",
			    __func__);
		}

		if ((i = poll(set, 2, timeout ? (timeout->tv_sec * 1000 +
		    (timeout->tv_nsec + 999999) / 1000000) : INFTIM)) < 0)
		{
			/* EINTR would occur upon SIGUSR1 for status dump */
			if (errno != EINTR)
				syslog(LOG_ERR, "<%s> poll: %m", __func__);
			continue;
		}
		if (i == 0)	/* timeout */
			continue;
		if (rtsock != -1 && set[1].revents & POLLIN)
			rtmsg_input();
		if (set[0].revents & POLLIN)
			rtadvd_input();
	}
	exit(0);		/* NOTREACHED */
}

static void
rtadvd_set_dump_file(__unused int sig)
{

	do_dump = 1;
}

static void
set_reconf(__unused int sig)
{

	do_reconf = 1;
}

static void
set_die(__unused int sig)
{

	do_die = 1;
}

static void
die(void)
{
	static int waiting;
	struct rainfo *rai, *ran;
	struct rdnss *rdnss;
	struct dnssl *dnssl;

	if (waiting) {
		if (TAILQ_FIRST(&ralist)) {
			syslog(LOG_INFO,
			       "<%s> waiting for expiration of all RA timers",
			       __func__);
			return;
		}
		syslog(LOG_NOTICE, "<%s> gracefully terminated", __func__);
		free(rcvcmsgbuf);
		free(sndcmsgbuf);
		exit(0);
		/* NOT REACHED */
	}

	if (TAILQ_FIRST(&ralist) == NULL) {
		syslog(LOG_NOTICE, "<%s> gracefully terminated", __func__);
		exit(0);
		/* NOT REACHED */
	}

	waiting = 1;
	syslog(LOG_NOTICE, "<%s> final RA transmission started", __func__);

	TAILQ_FOREACH_SAFE(rai, &ralist, next, ran) {
		if (rai->leaving) {
			TAILQ_REMOVE(&ralist, rai, next);
			TAILQ_INSERT_HEAD(&ralist, rai->leaving, next);
			rai->leaving->leaving = rai->leaving;
			rai->leaving->leaving_for = rai->leaving;
			free_rainfo(rai);
			continue;
		}
		rai->lifetime = 0;
		TAILQ_FOREACH(rdnss, &rai->rdnss, next)
			rdnss->lifetime = 0;
		TAILQ_FOREACH(dnssl, &rai->dnssl, next)
			dnssl->lifetime = 0;
		make_packet(rai);
		rai->leaving = rai;
		rai->leaving_for = rai;
		rai->initcounter = MAX_INITIAL_RTR_ADVERTISEMENTS;
		rai->mininterval = MIN_DELAY_BETWEEN_RAS;
		rai->maxinterval = MIN_DELAY_BETWEEN_RAS;
		rai->leaving_adv = MAX_FINAL_RTR_ADVERTISEMENTS;
		ra_output(rai);
		ra_timer_update((void *)rai, &rai->timer->tm);
		rtadvd_set_timer(&rai->timer->tm, rai->timer);
	}
}

static void
rtmsg_input(void)
{
	int n, type, ifindex = 0, plen;
	size_t len;
	union rt_msghdr_buf {
		struct rt_msghdr	rt_msghdr;
		char			data[2048];
	} buffer;
	char *msg, *next, *lim, **argv;
	char ifname[IF_NAMESIZE];
	struct prefix *prefix;
	struct rainfo *rai;
	struct in6_addr *addr;
	char addrbuf[INET6_ADDRSTRLEN];
	int prefixchange = 0, argc;

	memset(&buffer, 0, sizeof(buffer));
	n = read(rtsock, &buffer, sizeof(buffer));

	/* We read the buffer first to clear the FD */
	if (do_die)
		return;

	msg = buffer.data;
	if (dflag > 1) {
		syslog(LOG_DEBUG, "<%s> received a routing message "
		    "(type = %d, len = %d)", __func__, rtmsg_type(msg),
		    rtmsg_len(msg));
	}
	if (n > rtmsg_len(msg)) {
		/*
		 * This usually won't happen for messages received on 
		 * a routing socket.
		 */
		if (dflag > 1)
			syslog(LOG_DEBUG,
			    "<%s> received data length is larger than "
			    "1st routing message len. multiple messages? "
			    "read %d bytes, but 1st msg len = %d",
			    __func__, n, rtmsg_len(msg));
#if 0
		/* adjust length */
		n = rtmsg_len(msg);
#endif
	}

	lim = msg + n;
	for (next = msg; next < lim; next += len) {
		int oldifflags;

		next = get_next_msg(next, lim, 0, &len,
				    RTADV_TYPE2BITMASK(RTM_ADD) |
				    RTADV_TYPE2BITMASK(RTM_DELETE) |
				    RTADV_TYPE2BITMASK(RTM_NEWADDR) |
				    RTADV_TYPE2BITMASK(RTM_DELADDR) |
#ifdef RTM_IFANNOUNCE
				    RTADV_TYPE2BITMASK(RTM_IFANNOUNCE) |
#endif
				    RTADV_TYPE2BITMASK(RTM_IFINFO));
		if (len == 0)
			break;
		type = rtmsg_type(next);
		switch (type) {
		case RTM_ADD:
		case RTM_DELETE:
			ifindex = get_rtm_ifindex(next);
			break;
		case RTM_NEWADDR:
		case RTM_DELADDR:
			ifindex = get_ifam_ifindex(next);
			break;
#ifdef RTM_IFANNOUNCE
		case RTM_IFANNOUNCE:
			ifindex = get_ifan_ifindex(next);
			if (get_ifan_what(next) == IFAN_ARRIVAL) {
				syslog(LOG_DEBUG,
		    		       "<%s> interface %s arrived",
				       __func__,
				       if_indextoname(ifindex, ifname));
				if (if_argc == 0) {
					getconfig(ifname, 0);
					continue;
				}
				argc = if_argc;
				argv = if_argv;
				while (argc--) {
					if (strcmp(ifname, *argv++) == 0) {
						getconfig(ifname, 0);
						break;
					}
				}
				continue;
			}
			break;
#endif
		case RTM_IFINFO:
			ifindex = get_ifm_ifindex(next);
			break;
		default:
			/* should not reach here */
			if (dflag > 1) {
				syslog(LOG_DEBUG,
				       "<%s:%d> unknown rtmsg %d on %s",
				       __func__, __LINE__, type,
				       if_indextoname(ifindex, ifname));
			}
			continue;
		}

		if ((rai = if_indextorainfo(ifindex)) == NULL) {
			if (dflag > 1) {
				syslog(LOG_DEBUG,
				       "<%s> route changed on "
				       "non advertising interface %s (%d)",
				       __func__,
				       if_indextoname(ifindex, ifname),
				       ifindex);
			}
			continue;
		}
		oldifflags = rai->ifflags;

		switch (type) {
		case RTM_ADD:
			/* init ifflags because it may have changed */
			rai->ifflags = if_getflags(ifindex, rai->ifflags);

			if (sflag)
				break;	/* we aren't interested in prefixes  */

			addr = get_addr(msg);
			plen = get_prefixlen(msg);
			/* sanity check for plen */
			/* as RFC2373, prefixlen is at least 4 */
			if (plen < 4 || plen > 127) {
				syslog(LOG_INFO, "<%s> new interface route's"
				    "plen %d is invalid for a prefix",
				    __func__, plen);
				break;
			}
			prefix = find_prefix(rai, addr, plen);
			if (prefix) {
				if (prefix->timer) {
					/*
					 * If the prefix has been invalidated,
					 * make it available again.
					 */
					update_prefix(prefix);
					prefixchange = 1;
				} else if (dflag > 1) {
					syslog(LOG_DEBUG,
					    "<%s> new prefix(%s/%d) "
					    "added on %s, "
					    "but it was already in list",
					    __func__,
					    inet_ntop(AF_INET6, addr,
					    (char *)addrbuf, INET6_ADDRSTRLEN),
					    plen, rai->ifname);
				}
				break;
			}
			make_prefix(rai, ifindex, addr, plen);
			prefixchange = 1;
			break;
		case RTM_DELETE:
			/* init ifflags because it may have changed */
			rai->ifflags = if_getflags(ifindex, rai->ifflags);

			if (sflag)
				break;

			addr = get_addr(msg);
			plen = get_prefixlen(msg);
			/* sanity check for plen */
			/* as RFC2373, prefixlen is at least 4 */
			if (plen < 4 || plen > 127) {
				syslog(LOG_INFO,
				    "<%s> deleted interface route's "
				    "plen %d is invalid for a prefix",
				    __func__, plen);
				break;
			}
			prefix = find_prefix(rai, addr, plen);
			if (prefix == NULL) {
				if (dflag > 1) {
					syslog(LOG_DEBUG,
					    "<%s> prefix(%s/%d) was "
					    "deleted on %s, "
					    "but it was not in list",
					    __func__,
					    inet_ntop(AF_INET6, addr,
					    (char *)addrbuf, INET6_ADDRSTRLEN),
					    plen, rai->ifname);
				}
				break;
			}
			invalidate_prefix(prefix);
			prefixchange = 1;
			break;
		case RTM_NEWADDR:
		case RTM_DELADDR:
			/* init ifflags because it may have changed */
			rai->ifflags = if_getflags(ifindex, rai->ifflags);
			break;
		case RTM_IFINFO:
			rai->ifflags = get_ifm_flags(next);
			break;
#ifdef RTM_IFANNOUNCE
		case RTM_IFANNOUNCE:
			if (get_ifan_what(next) == IFAN_DEPARTURE) {
				syslog(LOG_DEBUG,
		    		       "<%s> interface %s departed",
				       __func__, rai->ifname);
				TAILQ_REMOVE(&ralist, rai, next);
				if (rai->leaving)
					free_rainfo(rai->leaving);
				free_rainfo(rai);
				continue;
			}
			break;
#endif
		default:
			/* should not reach here */
			if (dflag > 1) {
				syslog(LOG_DEBUG,
				    "<%s:%d> unknown rtmsg %d on %s",
				    __func__, __LINE__, type,
				    if_indextoname(ifindex, ifname));
			}
			return;
		}

		/* check if an interface flag is changed */
		if ((oldifflags & IFF_UP) != 0 &&	/* UP to DOWN */
		    (rai->ifflags & IFF_UP) == 0) {
			syslog(LOG_INFO,
			    "<%s> interface %s becomes down. stop timer.",
			    __func__, rai->ifname);
			rtadvd_remove_timer(&rai->timer);
		} else if ((oldifflags & IFF_UP) == 0 && /* DOWN to UP */
			 (rai->ifflags & IFF_UP) != 0) {
			syslog(LOG_INFO,
			    "<%s> interface %s becomes up. restart timer.",
			    __func__, rai->ifname);

			rai->initcounter = 0; /* reset the counter */
			rai->waiting = 0; /* XXX */
			rtadvd_remove_timer(&rai->timer);
			rai->timer = rtadvd_add_timer(ra_timeout,
			    ra_timer_update, rai, rai);
			ra_timer_update((void *)rai, &rai->timer->tm);
			rtadvd_set_timer(&rai->timer->tm, rai->timer);
		} else if (prefixchange && rai->ifflags & IFF_UP) {
			/*
			 * An advertised prefix has been added or invalidated.
			 * Will notice the change in a short delay.
			 */
			rai->initcounter = 0;
			ra_timer_set_short_delay(rai);
		}
	}

	return;
}

void
rtadvd_input(void)
{
	ssize_t i;
	int *hlimp = NULL;
#ifdef OLDRAWSOCKET
	struct ip6_hdr *ip;
#endif 
	struct icmp6_hdr *icp;
	int ifindex = 0;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi = NULL;
	char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	struct in6_addr dst = in6addr_any;
	struct rainfo *rai;

	/*
	 * Get message. We reset msg_controllen since the field could
	 * be modified if we had received a message before setting
	 * receive options.
	 */
	rcvmhdr.msg_controllen = rcvcmsgbuflen;
	if ((i = recvmsg(sock, &rcvmhdr, 0)) < 0)
		return;

	/* We read the buffer first to clear the FD */
	if (do_die)
		return;

	/* extract optional information via Advanced API */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rcvmhdr);
	     cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&rcvmhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
			ifindex = pi->ipi6_ifindex;
			dst = pi->ipi6_addr;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}
	if (ifindex == 0) {
		syslog(LOG_ERR,
		       "<%s> failed to get receiving interface",
		       __func__);
		return;
	}
	if (hlimp == NULL) {
		syslog(LOG_ERR,
		       "<%s> failed to get receiving hop limit",
		       __func__);
		return;
	}

	if ((rai = if_indextorainfo(pi->ipi6_ifindex)) == NULL) {
		if (dflag > 1) {
			syslog(LOG_DEBUG,
			       "<%s> received data for non advertising "
			       "interface (%s)",
			       __func__,
			       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		}
		return;
	}
	/*
	 * If we happen to receive data on an interface which is now down,
	 * just discard the data.
	 */
	if ((rai->ifflags & IFF_UP) == 0) {
		syslog(LOG_INFO,
		       "<%s> received data on a disabled interface (%s)",
		       __func__,
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

#ifdef OLDRAWSOCKET
	if ((size_t)i < sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr)) {
		syslog(LOG_ERR,
		       "<%s> packet size(%d) is too short",
		       __func__, i);
		return;
	}

	ip = (struct ip6_hdr *)rcvmhdr.msg_iov[0].iov_base;
	icp = (struct icmp6_hdr *)(ip + 1); /* XXX: ext. hdr? */
#else
	if ((size_t)i < sizeof(struct icmp6_hdr)) {
		syslog(LOG_ERR,
		       "<%s> packet size(%zd) is too short",
		       __func__, i);
		return;
	}

	icp = (struct icmp6_hdr *)rcvmhdr.msg_iov[0].iov_base;
#endif

	switch (icp->icmp6_type) {
	case ND_ROUTER_SOLICIT:
		/*
		 * Message verification - RFC-2461 6.1.1
		 * XXX: these checks must be done in the kernel as well,
		 *      but we can't completely rely on them.
		 */
		if (*hlimp != 255) {
			syslog(LOG_NOTICE,
			    "<%s> RS with invalid hop limit(%d) "
			    "received from %s on %s",
			    __func__, *hlimp,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (icp->icmp6_code) {
			syslog(LOG_NOTICE,
			    "<%s> RS with invalid ICMP6 code(%d) "
			    "received from %s on %s",
			    __func__, icp->icmp6_code,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if ((size_t)i < sizeof(struct nd_router_solicit)) {
			syslog(LOG_NOTICE,
			    "<%s> RS from %s on %s does not have enough "
			    "length (len = %zd)",
			    __func__,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf), i);
			return;
		}
		rs_input(i, (struct nd_router_solicit *)icp, pi, &rcvfrom);
		break;
	case ND_ROUTER_ADVERT:
		/*
		 * Message verification - RFC-2461 6.1.2
		 * XXX: there's a same dilemma as above... 
		 */
		if (*hlimp != 255) {
			syslog(LOG_NOTICE,
			    "<%s> RA with invalid hop limit(%d) "
			    "received from %s on %s",
			    __func__, *hlimp,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (icp->icmp6_code) {
			syslog(LOG_NOTICE,
			    "<%s> RA with invalid ICMP6 code(%d) "
			    "received from %s on %s",
			    __func__, icp->icmp6_code,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if ((size_t)i < sizeof(struct nd_router_advert)) {
			syslog(LOG_NOTICE,
			    "<%s> RA from %s on %s does not have enough "
			    "length (len = %zd)",
			    __func__,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf), i);
			return;
		}
		ra_input(i, (struct nd_router_advert *)icp, pi, &rcvfrom);
		break;
	case ICMP6_ROUTER_RENUMBERING:
		if (accept_rr == 0) {
			syslog(LOG_ERR, "<%s> received a router renumbering "
			    "message, but not allowed to be accepted",
			    __func__);
			break;
		}
		rr_input(i, (struct icmp6_router_renum *)icp, pi, &rcvfrom,
			 &dst);
		break;
	default:
		/*
		 * Note that this case is POSSIBLE, especially just
		 * after invocation of the daemon. This is because we
		 * could receive message after opening the socket and
		 * before setting ICMP6 type filter(see sock_open()).
		 */
		syslog(LOG_ERR, "<%s> invalid icmp type(%d)",
		    __func__, icp->icmp6_type);
		return;
	}

	return;
}

static void
rs_input(int len, struct nd_router_solicit *rs,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	union nd_opts ndopts;
	struct rainfo *rai;
	struct soliciter *sol;

	syslog(LOG_DEBUG,
	       "<%s> RS received from %s on %s",
	       __func__,
	       inet_ntop(AF_INET6, &from->sin6_addr,
			 ntopbuf, INET6_ADDRSTRLEN),
	       if_indextoname(pi->ipi6_ifindex, ifnamebuf));

	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	TAILQ_INIT(&ndopts.nd_opts_list);
	if (nd6_options((struct nd_opt_hdr *)(rs + 1),
			len - sizeof(struct nd_router_solicit),
			&ndopts, NDOPT_FLAG_SRCLINKADDR)) {
		syslog(LOG_INFO,
		       "<%s> ND option check failed for an RS from %s on %s",
		       __func__,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/*
	 * If the IP source address is the unspecified address, there
	 * must be no source link-layer address option in the message.
	 * (RFC-2461 6.1.1)
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&from->sin6_addr) &&
	    ndopts.nd_opts_src_lladdr) {
		syslog(LOG_INFO,
		       "<%s> RS from unspecified src on %s has a link-layer"
		       " address option",
		       __func__,
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	if ((rai = if_indextorainfo(pi->ipi6_ifindex)) == NULL) {
		syslog(LOG_INFO,
		       "<%s> RS received on non advertising interface(%s)",
		       __func__,
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	if (rai->leaving) {
		syslog(LOG_INFO,
		       "<%s> RS received on reconfiguring advertising interface(%s)",
		       __func__, rai->ifname);
		goto done;
	}

	rai->rsinput++;		/* increment statistics */

	/*
	 * Decide whether to send RA according to the rate-limit
	 * consideration.
	 */

	/* record sockaddr waiting for RA, if possible */
	sol = malloc(sizeof(*sol));
	if (sol) {
		sol->addr = *from;
		/* XXX RFC2553 need clarification on flowinfo */
		sol->addr.sin6_flowinfo = 0;
		TAILQ_INSERT_HEAD(&rai->soliciter, sol, next);
	}

	/*
	 * If there is already a waiting RS packet, don't
	 * update the timer.
	 */
	if (rai->waiting++)
		goto done;

	ra_timer_set_short_delay(rai);

done:
	free_ndopts(&ndopts);
	return;
}

void
ra_timer_set_short_delay(struct rainfo *rai)
{
	long delay;	/* must not be greater than 1000000 */
	struct timespec interval, now, min_delay, tm_tmp, *rest;

	/*
	 * Compute a random delay. If the computed value
	 * corresponds to a time later than the time the next
	 * multicast RA is scheduled to be sent, ignore the random
	 * delay and send the advertisement at the
	 * already-scheduled time. RFC2461 6.2.6
	 */
	delay = arc4random() % MAX_RA_DELAY_TIME;
	interval.tv_sec = 0;
	interval.tv_nsec = delay;
	rest = rtadvd_timer_rest(rai->timer);
	if (timespeccmp(rest, &interval, <)) {
		syslog(LOG_DEBUG, "<%s> random delay is larger than "
		    "the rest of current timer", __func__);
		interval = *rest;
	}

	/*
	 * If we sent a multicast Router Advertisement within
	 * the last MIN_DELAY_BETWEEN_RAS seconds, schedule
	 * the advertisement to be sent at a time corresponding to
	 * MIN_DELAY_BETWEEN_RAS plus the random value after the
	 * previous advertisement was sent.
	 */
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecsub(&now, &rai->lastsent, &tm_tmp);
	min_delay.tv_sec = MIN_DELAY_BETWEEN_RAS;
	min_delay.tv_nsec = 0;
	if (timespeccmp(&tm_tmp, &min_delay, <)) {
		timespecsub(&min_delay, &tm_tmp, &min_delay);
		timespecadd(&min_delay, &interval, &interval);
	}
	rtadvd_set_timer(&interval, rai->timer);
}

static void
ra_input(int len, struct nd_router_advert *ra,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	struct rainfo *rai;
	char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	union nd_opts ndopts;
	const char *on_off[] = {"OFF", "ON"};
	uint32_t reachabletime, retranstimer, mtu;
	struct nd_optlist *optp;
	int inconsistent = 0;

	syslog(LOG_DEBUG,
	       "<%s> RA received from %s on %s",
	       __func__,
	       inet_ntop(AF_INET6, &from->sin6_addr,
			 ntopbuf, INET6_ADDRSTRLEN),
	       if_indextoname(pi->ipi6_ifindex, ifnamebuf));

	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	TAILQ_INIT(&ndopts.nd_opts_list);
	if (nd6_options((struct nd_opt_hdr *)(ra + 1),
	    len - sizeof(struct nd_router_advert),
	    &ndopts, NDOPT_FLAG_SRCLINKADDR |
	    NDOPT_FLAG_PREFIXINFO | NDOPT_FLAG_MTU |
	    NDOPT_FLAG_RDNSS | NDOPT_FLAG_DNSSL))
	{
		syslog(LOG_INFO,
		    "<%s> ND option check failed for an RA from %s on %s",
		    __func__,
		    inet_ntop(AF_INET6, &from->sin6_addr,
		        ntopbuf, INET6_ADDRSTRLEN),
		        if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/*
	 * RA consistency check according to RFC-2461 6.2.7
	 */
	if ((rai = if_indextorainfo(pi->ipi6_ifindex)) == 0) {
		syslog(LOG_INFO,
		       "<%s> received RA from %s on non-advertising"
		       " interface(%s)",
		       __func__,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}
	if (rai->leaving) {
		syslog(LOG_DEBUG,
		       "<%s> received RA on re-configuring interface (%s)",
			__func__, rai->ifname);
		goto done;
	}
	rai->rainput++;		/* increment statistics */
	
	/* Cur Hop Limit value */
	if (ra->nd_ra_curhoplimit && rai->hoplimit &&
	    ra->nd_ra_curhoplimit != rai->hoplimit) {
		syslog(LOG_INFO,
		       "<%s> CurHopLimit inconsistent on %s:"
		       " %d from %s, %d from us",
		       __func__,
		       rai->ifname,
		       ra->nd_ra_curhoplimit,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->hoplimit);
		inconsistent++;
	}
	/* M flag */
	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) !=
	    rai->managedflg) {
		syslog(LOG_INFO,
		       "<%s> M flag inconsistent on %s:"
		       " %s from %s, %s from us",
		       __func__,
		       rai->ifname,
		       on_off[!rai->managedflg],
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       on_off[rai->managedflg]);
		inconsistent++;
	}
	/* O flag */
	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) !=
	    rai->otherflg) {
		syslog(LOG_INFO,
		       "<%s> O flag inconsistent on %s:"
		       " %s from %s, %s from us",
		       __func__,
		       rai->ifname,
		       on_off[!rai->otherflg],
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       on_off[rai->otherflg]);
		inconsistent++;
	}
	/* Reachable Time */
	reachabletime = ntohl(ra->nd_ra_reachable);
	if (reachabletime && rai->reachabletime &&
	    reachabletime != rai->reachabletime) {
		syslog(LOG_INFO,
		       "<%s> ReachableTime inconsistent on %s:"
		       " %d from %s, %d from us",
		       __func__,
		       rai->ifname,
		       reachabletime,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->reachabletime);
		inconsistent++;
	}
	/* Retrans Timer */
	retranstimer = ntohl(ra->nd_ra_retransmit);
	if (retranstimer && rai->retranstimer &&
	    retranstimer != rai->retranstimer) {
		syslog(LOG_INFO,
		       "<%s> RetranceTimer inconsistent on %s:"
		       " %d from %s, %d from us",
		       __func__,
		       rai->ifname,
		       retranstimer,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->retranstimer);
		inconsistent++;
	}
	/* Values in the MTU options */
	if (ndopts.nd_opts_mtu) {
		mtu = ntohl(ndopts.nd_opts_mtu->nd_opt_mtu_mtu);
		if (mtu && rai->linkmtu && mtu != rai->linkmtu) {
			syslog(LOG_INFO,
			       "<%s> MTU option value inconsistent on %s:"
			       " %d from %s, %d from us",
			       __func__,
			       rai->ifname, mtu,
			       inet_ntop(AF_INET6, &from->sin6_addr,
					 ntopbuf, INET6_ADDRSTRLEN),
			       rai->linkmtu);
			inconsistent++;
		}
	}
	/* Preferred and Valid Lifetimes for prefixes */
	if (ndopts.nd_opts_pi)
		if (prefix_check(ndopts.nd_opts_pi, rai, from))
			inconsistent++;
	TAILQ_FOREACH(optp, &ndopts.nd_opts_list, next)
		if (prefix_check((struct nd_opt_prefix_info *)optp->opt,
		    rai, from))
			inconsistent++;

	if (inconsistent)
		rai->rainconsistent++;
	
done:
	free_ndopts(&ndopts);
	return;
}

/* return a non-zero value if the received prefix is inconsitent with ours */
static int
prefix_check(struct nd_opt_prefix_info *pinfo,
	     struct rainfo *rai, struct sockaddr_in6 *from)
{
	uint32_t preferred_time, valid_time;
	struct prefix *pp;
	int inconsistent = 0;
	char ntopbuf[INET6_ADDRSTRLEN], prefixbuf[INET6_ADDRSTRLEN];
	struct timespec now;

#if 0				/* impossible */
	if (pinfo->nd_opt_pi_type != ND_OPT_PREFIX_INFORMATION)
		return(0);
#endif

	/*
	 * log if the adveritsed prefix has link-local scope(sanity check?)
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&pinfo->nd_opt_pi_prefix)) {
		syslog(LOG_INFO,
		       "<%s> link-local prefix %s/%d is advertised "
		       "from %s on %s",
		       __func__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->ifname);
	}

	if ((pp = find_prefix(rai, &pinfo->nd_opt_pi_prefix,
			      pinfo->nd_opt_pi_prefix_len)) == NULL) {
		syslog(LOG_INFO,
		       "<%s> prefix %s/%d from %s on %s is not in our list",
		       __func__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->ifname);
		return(0);
	}

	preferred_time = ntohl(pinfo->nd_opt_pi_preferred_time);
	if (pp->pltimeexpire) {
		/*
		 * The lifetime is decremented in real time, so we should
		 * compare the expiration time.
		 * (RFC 2461 Section 6.2.7.)
		 * XXX: can we really expect that all routers on the link
		 * have synchronized clocks?
		 */
		clock_gettime(CLOCK_MONOTONIC, &now);
		preferred_time += now.tv_sec;

		if (!pp->timer && rai->clockskew &&
		    llabs((long long)preferred_time - pp->pltimeexpire) > rai->clockskew) {
			syslog(LOG_INFO,
			       "<%s> preferred lifetime for %s/%d"
			       " (decr. in real time) inconsistent on %s:"
			       " %d from %s, %ld from us",
			       __func__,
			       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
					 prefixbuf, INET6_ADDRSTRLEN),
			       pinfo->nd_opt_pi_prefix_len,
			       rai->ifname, preferred_time,
			       inet_ntop(AF_INET6, &from->sin6_addr,
					 ntopbuf, INET6_ADDRSTRLEN),
			       pp->pltimeexpire);
			inconsistent++;
		}
	} else if (!pp->timer && preferred_time != pp->preflifetime) {
		syslog(LOG_INFO,
		       "<%s> preferred lifetime for %s/%d"
		       " inconsistent on %s:"
		       " %d from %s, %d from us",
		       __func__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       rai->ifname, preferred_time,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       pp->preflifetime);
	}

	valid_time = ntohl(pinfo->nd_opt_pi_valid_time);
	if (pp->vltimeexpire) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		valid_time += now.tv_sec;

		if (!pp->timer && rai->clockskew &&
		    llabs((long long)valid_time - pp->vltimeexpire) > rai->clockskew) {
			syslog(LOG_INFO,
			       "<%s> valid lifetime for %s/%d"
			       " (decr. in real time) inconsistent on %s:"
			       " %d from %s, %ld from us",
			       __func__,
			       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
					 prefixbuf, INET6_ADDRSTRLEN),
			       pinfo->nd_opt_pi_prefix_len,
			       rai->ifname, preferred_time,
			       inet_ntop(AF_INET6, &from->sin6_addr,
					 ntopbuf, INET6_ADDRSTRLEN),
			       pp->vltimeexpire);
			inconsistent++;
		}
	} else if (!pp->timer && valid_time != pp->validlifetime) {
		syslog(LOG_INFO,
		       "<%s> valid lifetime for %s/%d"
		       " inconsistent on %s:"
		       " %d from %s, %d from us",
		       __func__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       rai->ifname, valid_time,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       pp->validlifetime);
		inconsistent++;
	}

	return(inconsistent);
}

struct prefix *
find_prefix(struct rainfo *rai, struct in6_addr *prefix, int plen)
{
	struct prefix *pp;
	int bytelen, bitlen;
	unsigned char bitmask;

	TAILQ_FOREACH(pp, &rai->prefix, next) {
		if (plen != pp->prefixlen)
			continue;
		bytelen = plen / 8;
		bitlen = plen % 8;
		bitmask = 0xff << (8 - bitlen);
		if (memcmp((void *)prefix, (void *)&pp->prefix, bytelen))
			continue;
		if (bitlen == 0 ||
		    ((prefix->s6_addr[bytelen] & bitmask) == 
		     (pp->prefix.s6_addr[bytelen] & bitmask))) {
			return(pp);
		}
	}

	return(NULL);
}

/* check if p0/plen0 matches p1/plen1; return 1 if matches, otherwise 0. */
int
prefix_match(struct in6_addr *p0, int plen0,
	     struct in6_addr *p1, int plen1)
{
	int bytelen, bitlen;
	unsigned char bitmask;

	if (plen0 < plen1)
		return(0);
	bytelen = plen1 / 8;
	bitlen = plen1 % 8;
	bitmask = 0xff << (8 - bitlen);
	if (memcmp((void *)p0, (void *)p1, bytelen))
		return(0);
	if (bitlen == 0 ||
	    ((p0->s6_addr[bytelen] & bitmask) ==
	     (p1->s6_addr[bytelen] & bitmask))) { 
		return(1);
	}

	return(0);
}

static int
nd6_options(struct nd_opt_hdr *hdr, int limit,
	    union nd_opts *ndopts, uint32_t optflags)
{
	int optlen = 0;

	for (; limit > 0; limit -= optlen) {
		if ((size_t)limit < sizeof(struct nd_opt_hdr)) {
			syslog(LOG_INFO, "<%s> short option header", __func__);
			goto bad;
		}

		hdr = (struct nd_opt_hdr *)((char *)hdr + optlen);
		if (hdr->nd_opt_len == 0) {
			syslog(LOG_INFO,
			    "<%s> bad ND option length(0) (type = %d)",
			    __func__, hdr->nd_opt_type);
			goto bad;
		}
		optlen = hdr->nd_opt_len << 3;
		if (optlen > limit) {
			syslog(LOG_INFO, "<%s> short option", __func__);
			goto bad;
		}

		if (hdr->nd_opt_type > ND_OPT_MTU &&
		    hdr->nd_opt_type != ND_OPT_RDNSS &&
		    hdr->nd_opt_type != ND_OPT_DNSSL)
		{
			syslog(LOG_INFO, "<%s> unknown ND option(type %d)",
			    __func__, hdr->nd_opt_type);
			continue;
		}

		if ((ndopt_flags[hdr->nd_opt_type] & optflags) == 0) {
			syslog(LOG_INFO, "<%s> unexpected ND option(type %d)",
			    __func__, hdr->nd_opt_type);
			continue;
		}

		/*
		 * Option length check.  Do it here for all fixed-length
		 * options.
		 */
		if ((hdr->nd_opt_type == ND_OPT_MTU &&
		    (optlen != sizeof(struct nd_opt_mtu))) ||
		    ((hdr->nd_opt_type == ND_OPT_PREFIX_INFORMATION &&
		    optlen != sizeof(struct nd_opt_prefix_info))) ||
		    (hdr->nd_opt_type == ND_OPT_RDNSS &&
		    ((optlen < (int)sizeof(struct nd_opt_rdnss) ||
		    (optlen - sizeof(struct nd_opt_rdnss)) % 16 != 0))) ||
		    (hdr->nd_opt_type == ND_OPT_DNSSL &&
		    optlen < (int)sizeof(struct nd_opt_dnssl)))
		{
			syslog(LOG_INFO, "<%s> invalid option length",
			    __func__);
			continue;
		}

		switch (hdr->nd_opt_type) {
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_REDIRECTED_HEADER:
		case ND_OPT_RDNSS:
		case ND_OPT_DNSSL:
			break;	/* we don't care about these options */
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_MTU:
			if (ndopts->nd_opt_array[hdr->nd_opt_type]) {
				syslog(LOG_INFO,
				    "<%s> duplicated ND option (type = %d)",
				    __func__, hdr->nd_opt_type);
			}
			ndopts->nd_opt_array[hdr->nd_opt_type] = hdr;
			break;
		case ND_OPT_PREFIX_INFORMATION:
		{
			struct nd_optlist *pfxlist;

			if (ndopts->nd_opts_pi == 0) {
				ndopts->nd_opts_pi =
				    (struct nd_opt_prefix_info *)hdr;
				continue;
			}
			if ((pfxlist = malloc(sizeof(*pfxlist))) == NULL) {
				syslog(LOG_ERR, "<%s> can't allocate memory",
				    __func__);
				goto bad;
			}
			pfxlist->opt = hdr;
			TAILQ_INSERT_TAIL(&ndopts->nd_opts_list, pfxlist, next);

			break;
		}
		default:	/* impossible */
			break;
		}
	}

	return(0);

  bad:
	free_ndopts(ndopts);

	return(-1);
}

static void
free_ndopts(union nd_opts *ndopts)
{
	struct nd_optlist *opt;

	while ((opt = TAILQ_FIRST(&ndopts->nd_opts_list)) != NULL) {
		TAILQ_REMOVE(&ndopts->nd_opts_list, opt, next);
		free(opt);
	}
}

void
sock_open(void)
{
	struct icmp6_filter filt;
	struct ipv6_mreq mreq;
	struct rainfo *ra;
	int on;
	/* XXX: should be max MTU attached to the node */
	static unsigned char answer[1500];

	rcvcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
				CMSG_SPACE(sizeof(int));
	rcvcmsgbuf = malloc(rcvcmsgbuflen);
	if (rcvcmsgbuf == NULL) {
		syslog(LOG_ERR, "<%s> malloc: %m", __func__);
		exit(1);
	}

	sndcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo));
	sndcmsgbuf = malloc(sndcmsgbuflen);
	if (sndcmsgbuf == NULL) {
		syslog(LOG_ERR, "<%s> malloc: %m", __func__);
		exit(1);
	}

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %m", __func__);
		exit(1);
	}

	/* RFC 4861 Section 4.2 */
	on = 255;
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &on,
		       sizeof(on)) == -1) {
		syslog(LOG_ERR, "<%s> IPV6_MULTICAST_HOPS: %m", __func__);
		exit(1);
	}

	/* specify to tell receiving interface */
	on = 1;
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
		       sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_RECVPKTINFO: %m", __func__);
		exit(1);
	}
#else  /* old adv. API */
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, &on,
		       sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_PKTINFO: %m", __func__);
		exit(1);
	}
#endif 

	on = 1;
	/* specify to tell value of hoplimit field of received IP6 hdr */
#ifdef IPV6_RECVHOPLIMIT
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
		       sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_RECVHOPLIMIT: %m", __func__);
		exit(1);
	}
#else  /* old adv. API */
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_HOPLIMIT, &on,
		       sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_HOPLIMIT: %m", __func__);
		exit(1);
	}
#endif

	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	if (accept_rr)
		ICMP6_FILTER_SETPASS(ICMP6_ROUTER_RENUMBERING, &filt);
	if (setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
		       sizeof(filt)) < 0) {
		syslog(LOG_ERR, "<%s> IICMP6_FILTER: %m", __func__);
		exit(1);
	}

	/*
	 * join all routers multicast address on each advertising interface.
	 */
	if (inet_pton(AF_INET6, ALLROUTERS_LINK,
	    mreq.ipv6mr_multiaddr.s6_addr) != 1)
	{
		syslog(LOG_ERR, "<%s> inet_pton failed(library bug?)",
		    __func__);
		exit(1);
	}
	TAILQ_FOREACH(ra, &ralist, next) {
		mreq.ipv6mr_interface = ra->ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq,
			       sizeof(mreq)) < 0) {
			syslog(LOG_ERR, "<%s> IPV6_JOIN_GROUP(link) on %s: %m",
			       __func__, ra->ifname);
			exit(1);
		}
	}

	/*
	 * When attending router renumbering, join all-routers site-local
	 * multicast group. 
	 */
	if (accept_rr) {
		if (inet_pton(AF_INET6, ALLROUTERS_SITE,
		     mreq.ipv6mr_multiaddr.s6_addr) != 1)
		{
			syslog(LOG_ERR, "<%s> inet_pton failed(library bug?)",
			    __func__);
			exit(1);
		}
		ra = TAILQ_FIRST(&ralist);
		if (mcastif) {
			if ((mreq.ipv6mr_interface = if_nametoindex(mcastif))
			    == 0) {
				syslog(LOG_ERR,
				       "<%s> invalid interface: %s",
				       __func__, mcastif);
				exit(1);
			}
		} else
			mreq.ipv6mr_interface = ra->ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			       &mreq, sizeof(mreq)) < 0) {
			syslog(LOG_ERR,
			       "<%s> IPV6_JOIN_GROUP(site) on %s: %m",
			       __func__,
			       mcastif ? mcastif : ra->ifname);
			exit(1);
		}
	}
	
	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = answer;
	rcviov[0].iov_len = sizeof(answer);
	rcvmhdr.msg_name = &rcvfrom;
	rcvmhdr.msg_namelen = sizeof(rcvfrom);
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvmhdr.msg_control = rcvcmsgbuf;
	rcvmhdr.msg_controllen = rcvcmsgbuflen;

	/* initialize msghdr for sending packets */
	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 1;
	sndmhdr.msg_control = (void *)sndcmsgbuf;
	sndmhdr.msg_controllen = sndcmsgbuflen;
	
	return;
}

/* open a routing socket to watch the routing table */
static void
rtsock_open(void)
{
	if ((rtsock = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %m", __func__);
		exit(1);
	}
}

struct rainfo *
if_indextorainfo(unsigned int idx)
{
	struct rainfo *rai;

	TAILQ_FOREACH(rai, &ralist, next) {
		if (rai->ifindex == idx)
			return(rai);
	}

	return(NULL);		/* search failed */
}

struct rainfo *
ra_output(struct rainfo *rai)
{
	int i;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi;
	struct soliciter *sol;

	if ((rai->ifflags & IFF_UP) == 0) {
		syslog(LOG_DEBUG, "<%s> %s is not up, skip sending RA",
		       __func__, rai->ifname);
		return NULL;
	}

	make_packet(rai);	/* XXX: inefficient */

	sndmhdr.msg_name = (void *)&sin6_linklocal_allnodes;
	sndmhdr.msg_iov[0].iov_base = (void *)rai->ra_data;
	sndmhdr.msg_iov[0].iov_len = rai->ra_datalen;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	/* specify the outgoing interface */
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
	pi->ipi6_ifindex = rai->ifindex;

	syslog(LOG_DEBUG,
	       "<%s> send RA on %s, # of waitings = %d",
	       __func__, rai->ifname, rai->waiting); 

	i = sendmsg(sock, &sndmhdr, 0);

	if (i < 0 || (size_t)i != rai->ra_datalen)  {
		if (i < 0) {
			syslog(LOG_ERR, "<%s> sendmsg on %s: %m",
			       __func__, rai->ifname);
		}
	}

	/*
	 * unicast advertisements
	 * XXX commented out.  reason: though spec does not forbit it, unicast
	 * advert does not really help
	 */
	while ((sol = TAILQ_FIRST(&rai->soliciter)) != NULL) {
#if 0
		sndmhdr.msg_name = (void *)&sol->addr;
		i = sendmsg(sock, &sndmhdr, 0);
		if (i < 0 || i != rai->ra_datalen)  {
			if (i < 0) {
				syslog(LOG_ERR,
				    "<%s> unicast sendmsg on %s: %m",
				    __func__, rai->ifname);
			}
		}
#endif
		TAILQ_REMOVE(&rai->soliciter, sol, next);
		free(sol);
	}

	if (rai->leaving_adv > 0) {
		if (--(rai->leaving_adv) == 0) {
			/* leaving for ourself means we're shutting down */
			if (rai->leaving_for == rai) {
				TAILQ_REMOVE(&ralist, rai, next);
				free_rainfo(rai);
				return NULL;
			}
			syslog(LOG_DEBUG,
			       "<%s> expired RA,"
			       " new config active for interface (%s)",
			       __func__, rai->ifname);
			rai->leaving_for->timer = rtadvd_add_timer(ra_timeout,
			    ra_timer_update,
			    rai->leaving_for, rai->leaving_for);
			ra_timer_set_short_delay(rai->leaving_for);
			rai->leaving_for->leaving = NULL;
			free_rainfo(rai);
			return NULL;
		}
	}

	/* update counter */
	if (rai->initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS)
		rai->initcounter++;
	rai->raoutput++;

	/* update timestamp */
	clock_gettime(CLOCK_MONOTONIC, &rai->lastsent);

	/* reset waiting conter */
	rai->waiting = 0;

	return rai;
}

/* process RA timer */
struct rtadvd_timer *
ra_timeout(void *data)
{
	struct rainfo *rai = (struct rainfo *)data;

#ifdef notyet
	/* if necessary, reconstruct the packet. */
#endif

	syslog(LOG_DEBUG,
	       "<%s> RA timer on %s is expired",
	       __func__, rai->ifname);

	if (ra_output(rai))
		return(rai->timer);
	return NULL;
}

/* update RA timer */
void
ra_timer_update(void *data, struct timespec *tm)
{
	struct rainfo *rai = (struct rainfo *)data;
	long interval;

	/*
	 * Whenever a multicast advertisement is sent from an interface,
	 * the timer is reset to a uniformly-distributed random value
	 * between the interface's configured MinRtrAdvInterval and
	 * MaxRtrAdvInterval (RFC2461 6.2.4).
	 */
	interval = rai->mininterval;
	if (rai->mininterval != rai->maxinterval)
		interval += arc4random() % (rai->maxinterval-rai->mininterval);

	/*
	 * For the first few advertisements (up to
	 * MAX_INITIAL_RTR_ADVERTISEMENTS), if the randomly chosen interval
	 * is greater than MAX_INITIAL_RTR_ADVERT_INTERVAL, the timer
	 * SHOULD be set to MAX_INITIAL_RTR_ADVERT_INTERVAL instead.
	 * (RFC-2461 6.2.4)
	 */
	if (rai->initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS &&
	    interval > MAX_INITIAL_RTR_ADVERT_INTERVAL)
		interval = MAX_INITIAL_RTR_ADVERT_INTERVAL;

	tm->tv_sec = interval;
	tm->tv_nsec = 0;

	syslog(LOG_DEBUG,
	       "<%s> RA timer on %s is set to %ld:%ld",
	       __func__, rai->ifname,
	       (long int)tm->tv_sec, (long int)tm->tv_nsec);

	return;
}
