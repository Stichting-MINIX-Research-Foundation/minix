/*	$NetBSD: main.c,v 1.95 2014/11/12 03:34:59 christos Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	Regents of the University of California.  All rights reserved.
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1988, 1993\
 Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)main.c	8.4 (Berkeley) 3/1/94";
#else
__RCSID("$NetBSD: main.c,v 1.95 2014/11/12 03:34:59 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"
#include "rtutil.h"
#include "prog_ops.h"

struct nlist nl[] = {
#define	N_MBSTAT	0
	{ "_mbstat", 0, 0, 0, 0 },
#define	N_IPSTAT	1
	{ "_ipstat", 0, 0, 0, 0 },	/* not available via kvm */
#define	N_TCBTABLE	2
	{ "_tcbtable", 0, 0, 0, 0 },
#define	N_TCPSTAT	3
	{ "_tcpstat", 0, 0, 0, 0 },	/* not available via kvm */
#define	N_UDBTABLE	4
	{ "_udbtable", 0, 0, 0, 0 },
#define	N_UDPSTAT	5
	{ "_udpstat", 0, 0, 0, 0 },	/* not available via kvm */
#define	N_IFNET_LIST		6
	{ "_ifnet_list", 0, 0, 0, 0 },
#define	N_ICMPSTAT	7
	{ "_icmpstat", 0, 0, 0, 0 },	/* not available via kvm */
#define	N_RTSTAT	8
	{ "_rtstat", 0, 0, 0, 0 },
#define	N_UNIXSW	9
	{ "_unixsw", 0, 0, 0, 0 },
#define N_RTREE		10
	{ "_rt_tables", 0, 0, 0, 0 },
#define	N_NFILE		11
	{ "_nfile", 0, 0, 0, 0 },
#define N_IGMPSTAT	12
	{ "_igmpstat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_MRTPROTO	13
	{ "_ip_mrtproto", 0, 0, 0, 0 },
#define N_MRTSTAT	14
	{ "_mrtstat", 0, 0, 0, 0 },
#define N_MFCHASHTBL	15
	{ "_mfchashtbl", 0, 0, 0, 0 },
#define	N_MFCHASH	16
	{ "_mfchash", 0, 0, 0, 0 },
#define N_VIFTABLE	17
	{ "_viftable", 0, 0, 0, 0 },
#define N_MSIZE		18
	{ "_msize", 0, 0, 0, 0 },
#define N_MCLBYTES	19
	{ "_mclbytes", 0, 0, 0, 0 },
#define N_DDPSTAT	20
	{ "_ddpstat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_DDPCB		21
	{ "_ddpcb", 0, 0, 0, 0 },
#define N_MBPOOL	22
	{ "_mbpool", 0, 0, 0, 0 },
#define N_MCLPOOL	23
	{ "_mclpool", 0, 0, 0, 0 },
#define N_IP6STAT	24
	{ "_ip6stat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_TCP6STAT	25
	{ "_tcp6stat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_UDP6STAT	26
	{ "_udp6stat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_ICMP6STAT	27
	{ "_icmp6stat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_IPSECSTAT	28
	{ "_ipsecstat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_IPSEC6STAT	29
	{ "_ipsec6stat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_PIM6STAT	30
	{ "_pim6stat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_MRT6PROTO	31
	{ "_ip6_mrtproto", 0, 0, 0, 0 },
#define N_MRT6STAT	32
	{ "_mrt6stat", 0, 0, 0, 0 },
#define N_MF6CTABLE	33
	{ "_mf6ctable", 0, 0, 0, 0 },
#define N_MIF6TABLE	34
	{ "_mif6table", 0, 0, 0, 0 },
#define N_PFKEYSTAT	35
	{ "_pfkeystat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_ARPSTAT	36
	{ "_arpstat", 0, 0, 0, 0 },	/* not available via kvm */
#define N_RIP6STAT	37
	{ "_rip6stat", 0, 0, 0, 0 },	/* not available via kvm */
#define	N_ARPINTRQ	38
	{ "_arpintrq", 0, 0, 0, 0 },
#define	N_IPINTRQ	39
	{ "_ipintrq", 0, 0, 0, 0 },
#define	N_IP6INTRQ	40
	{ "_ip6intrq", 0, 0, 0, 0 },
#define	N_ATINTRQ1	41
	{ "_atintrq1", 0, 0, 0, 0 },
#define	N_ATINTRQ2	42
	{ "_atintrq2", 0, 0, 0, 0 },
#define	N_NSINTRQ	43
	{ "_nsintrq", 0, 0, 0, 0 },
#define	N_LLCINTRQ	44
	{ "_llcintrq", 0, 0, 0, 0 },
#define	N_HDINTRQ	45
	{ "_hdintrq", 0, 0, 0, 0 },
#define	N_NATMINTRQ	46
	{ "_natmintrq", 0, 0, 0, 0 },
#define	N_PPPOEDISCINQ	47
	{ "_ppoediscinq", 0, 0, 0, 0 },
#define	N_PPPOEINQ	48
	{ "_ppoeinq", 0, 0, 0, 0 },
#define	N_PKINTRQ	49
	{ "_pkintrq", 0, 0, 0, 0 },
#define	N_HARDCLOCK_TICKS 50
	{ "_hardclock_ticks", 0, 0, 0, 0 },
#define N_PIMSTAT	51
	{ "_pimstat", 0, 0, 0, 0 },
#define N_CARPSTAT	52
	{ "_carpstats", 0, 0, 0, 0 },	/* not available via kvm */
#define N_PFSYNCSTAT	53
	{ "_pfsyncstats", 0, 0, 0, 0},  /* not available via kvm */
	{ "", 0, 0, 0, 0 },
};

struct protox {
	u_char	pr_index;		/* index into nlist of cb head */
	u_char	pr_sindex;		/* index into nlist of stat block */
	u_char	pr_wanted;		/* 1 if wanted, 0 otherwise */
	void	(*pr_cblocks)		/* control blocks printing routine */
			__P((u_long, const char *));
	void	(*pr_stats)		/* statistics printing routine */
			__P((u_long, const char *));
	void	(*pr_istats)
			__P((const char *));	/* per/if statistics printing routine */
	void	(*pr_dump)		/* PCB state dump routine */
			__P((u_long, const char *, u_long));
	const char *pr_name;		/* well-known name */
} protox[] = {
	{ N_TCBTABLE,	N_TCPSTAT,	1,	protopr,
	  tcp_stats,	NULL,		tcp_dump,	"tcp" },
	{ N_UDBTABLE,	N_UDPSTAT,	1,	protopr,
	  udp_stats,	NULL,		0,	"udp" },
	{ -1,		N_IPSTAT,	1,	0,
	  ip_stats,	NULL,		0,	"ip" },
	{ -1,		N_ICMPSTAT,	1,	0,
	  icmp_stats,	NULL,		0,	"icmp" },
	{ -1,		N_IGMPSTAT,	1,	0,
	  igmp_stats,	NULL,		0,	"igmp" },
	{ -1,		N_CARPSTAT,	1,	0,
	  carp_stats,	NULL,		0,	"carp" },
#ifdef IPSEC
	{ -1,		N_IPSECSTAT,	1,	0,
	  fast_ipsec_stats, NULL,	0,	"ipsec" },
#endif
	{ -1,		N_PIMSTAT,	1,	0,
	  pim_stats,	NULL,		0,	"pim" },
	{ -1,		N_PFSYNCSTAT,  1,  0,
	  pfsync_stats,  NULL,		0,  "pfsync" },	
	{ -1,		-1,		0,	0,
	  0,		NULL,		0,	0 }
};

#ifdef INET6
struct protox ip6protox[] = {
	{ -1,		N_IP6STAT,	1,	0,
	  ip6_stats,	ip6_ifstats,	0,	"ip6" },
	{ -1,		N_ICMP6STAT,	1,	0,
	  icmp6_stats,	icmp6_ifstats,	0,	"icmp6" },
#ifdef TCP6
	{ N_TCBTABLE,	N_TCP6STAT,	1,	ip6protopr,
	  tcp6_stats,	NULL,		tcp6_dump,	"tcp6" },
#else
	{ N_TCBTABLE,	N_TCP6STAT,	1,	ip6protopr,
	  tcp_stats,	NULL,		tcp6_dump,	"tcp6" },
#endif
	{ N_UDBTABLE,	N_UDP6STAT,	1,	ip6protopr,
	  udp6_stats,	NULL,		0,	"udp6" },
#ifdef IPSEC
	{ -1,		N_IPSEC6STAT,	1,	0,
	  fast_ipsec_stats, NULL,	0,	"ipsec6" },
#endif
	{ -1,		N_PIM6STAT,	1,	0,
	  pim6_stats,	NULL,		0,	"pim6" },
	{ -1,		N_RIP6STAT,	1,	0,
	  rip6_stats,	NULL,		0,	"rip6" },
	{ -1,		-1,		0,	0,
	  0,		NULL,		0,	0 }
};
#endif

struct protox arpprotox[] = {
	{ -1,		N_ARPSTAT,	1,	0,
	  arp_stats,	NULL,		0,	"arp" },
	{ -1,		-1,		0,	0,
	  0,		NULL,		0,	0 }
};

#ifdef IPSEC
struct protox pfkeyprotox[] = {
	{ -1,		N_PFKEYSTAT,	1,	0,
	  pfkey_stats,	NULL,		0,	"pfkey" },
	{ -1,		-1,		0,	0,
	  0,		NULL,		0,	0 }
};
#endif

#ifndef SMALL
struct protox atalkprotox[] = {
	{ N_DDPCB,	N_DDPSTAT,	1,	atalkprotopr,
	  ddp_stats,	NULL,		0,	"ddp" },
	{ -1,		-1,		0,	0,
	  0,		NULL,		0,	NULL }
};
#endif

struct protox *protoprotox[] = { protox,
#ifdef INET6
				 ip6protox,
#endif
				 arpprotox,
#ifdef IPSEC
				 pfkeyprotox,
#endif
#ifndef SMALL
				 atalkprotox,
#endif
				 NULL };

const struct softintrq {
	const char *siq_name;
	int siq_index;
} softintrq[] = {
	{ "arpintrq", N_ARPINTRQ },
	{ "ipintrq", N_IPINTRQ },
	{ "ip6intrq", N_IP6INTRQ },
	{ "atintrq1", N_ATINTRQ1 },
	{ "atintrq2", N_ATINTRQ2 },
	{ "llcintrq", N_LLCINTRQ },
	{ "hdintrq", N_HDINTRQ },
	{ "natmintrq", N_NATMINTRQ },
	{ "ppoediscinq", N_PPPOEDISCINQ },
	{ "ppoeinq", N_PPPOEINQ },
	{ "pkintrq", N_PKINTRQ },
	{ NULL, -1 },
};

int main __P((int, char *[]));
static void printproto __P((struct protox *, const char *));
static void print_softintrq __P((void));
__dead static void usage(void);
static struct protox *name2protox __P((const char *));
static struct protox *knownname __P((const char *));
static void prepare(const char *, const char *, struct protox *tp);
static kvm_t *prepare_kvmd(const char *, const char *, char *);

static kvm_t *kvmd = NULL;
gid_t egid;
int interval;	/* repeat interval for i/f stats */
static const char *nlistf = NULL, *memf = NULL;

kvm_t *
get_kvmd(void)
{
	char buf[_POSIX2_LINE_MAX];

	if (kvmd != NULL)
		return kvmd;
	if ((kvmd = prepare_kvmd(nlistf, memf, buf)) == NULL)
		errx(1, "kvm error: %s", buf);
	return kvmd;
}

static kvm_t *
prepare_kvmd(const char *nf, const char *mf, char *errbuf)
{
	kvm_t *k;

	(void)setegid(egid);
	k = kvm_openfiles(nf, mf, NULL, O_RDONLY, errbuf);
	(void)setgid(getgid());
	return k;
}

void
prepare(const char *nf, const char *mf, struct protox *tp)
{
	char buf[_POSIX2_LINE_MAX];
	/*
	 * Try to figure out if we can use sysctl or not.
	 */
	if (nf != NULL || mf != NULL) {
		/* Of course, we can't use sysctl with dumps. */
		if (force_sysctl)
			errx(EXIT_FAILURE, "can't use sysctl with dumps");

		/*
		 * If we have -M or -N, we're not dealing with live memory
		 * or want to use kvm interface explicitly.  It is sometimes
		 * useful to dig inside of kernel without extending
		 * sysctl interface (i.e., without rebuilding kernel).
		 */
		use_sysctl = 0;
	} else if (qflag ||
		   iflag ||
#ifndef SMALL
		   gflag ||
#endif
		   (pflag && tp->pr_sindex == N_PIMSTAT) ||
		   Pflag) {
		/* These flags are not yet supported via sysctl(3). */
		use_sysctl = 0;
	} else {
		/* We can use sysctl(3). */
		use_sysctl = 1;
	}

	if (force_sysctl && !use_sysctl) {
		/* Let the user know what's about to happen. */
		warnx("forcing sysctl usage even though it might not be "\
		    "supported");
		use_sysctl = 1;
	}

#ifdef __minix
	use_sysctl = 1;
#endif /* __minix */

	kvmd = prepare_kvmd(nf, mf, buf);

	if (!use_sysctl) {

		if (kvmd == NULL)
			errx(1, "kvm error: %s", buf);
		if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0) {
			if (nf)
				errx(1, "%s: no namelist", nf);
			else
				errx(1, "no namelist");
		}
	} else
		(void)setgid(getgid());
}

int
main(int argc, char *argv[])
{
	struct protoent *p;
	struct protox *tp;	/* for printing cblocks & stats */
	int ch;
	char *cp;
	char *afname, *afnames;
	u_long pcbaddr;

	if (prog_init) {
		if (prog_init() == -1)
			err(1, "init failed");
		force_sysctl = 1; /* cheap trick */
	}

	egid = getegid();
	(void)setegid(getgid());
	tp = NULL;
	af = AF_UNSPEC;
	afnames = NULL;
	pcbaddr = 0;

	while ((ch = getopt(argc, argv,
	    "AabBdf:ghI:LliM:mN:nP:p:qrsStTuVvw:X")) != -1)
		switch (ch) {
		case 'A':
			Aflag = RT_AFLAG;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'B':
			Bflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			afnames = optarg;
			break;
#ifndef SMALL
		case 'g':
			gflag = 1;
			break;
#endif
		case 'h':
			hflag = 1;
			break;
		case 'I':
			iflag = 1;
			interface = optarg;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'L':
			Lflag = RT_LFLAG;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			numeric_addr = numeric_port = nflag = RT_NFLAG;
			break;
		case 'P':
			errno = 0;
			pcbaddr = strtoul(optarg, &cp, 16);
			if (*cp != '\0' || errno == ERANGE)
				errx(1, "invalid PCB address %s",
				    optarg);
			Pflag = 1;
			break;
		case 'p':
			if ((tp = name2protox(optarg)) == NULL)
				errx(1, "%s: unknown or uninstrumented protocol",
				    optarg);
			pflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			++sflag;
			break;
		case 'S':
			numeric_addr = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'T':
			tagflag = RT_TFLAG;
			break;
		case 'u':
			af = AF_LOCAL;
			break;
		case 'V':
			Vflag++;
			break;
		case 'v':
			vflag = RT_VFLAG;
			break;
		case 'w':
			interval = atoi(optarg);
			iflag = 1;
			break;
		case 'X':
			force_sysctl = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		if (isdigit((unsigned char)**argv)) {
			interval = atoi(*argv);
			if (interval <= 0)
				usage();
			++argv;
			iflag = 1;
		}
		if (*argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif

	prepare(nlistf, memf, tp);

#ifndef SMALL
	if (Bflag) {
		if (sflag)
			bpf_stats();
		else
			bpf_dump(interface);
		exit(0);
	}
#endif

	if (mflag) {
		mbpr(nl[N_MBSTAT].n_value,  nl[N_MSIZE].n_value,
		    nl[N_MCLBYTES].n_value, nl[N_MBPOOL].n_value,
		    nl[N_MCLPOOL].n_value);
		exit(0);
	}
	if (Pflag) {
		if (tp == NULL) {
			/* Default to TCP. */
			tp = name2protox("tcp");
		}
		if (tp->pr_dump)
			(*tp->pr_dump)(nl[tp->pr_index].n_value, tp->pr_name,
			    pcbaddr);
		else
			printf("%s: no PCB dump routine\n", tp->pr_name);
		exit(0);
	}
	if (pflag) {
		if (iflag && tp->pr_istats)
			intpr(interval, nl[N_IFNET_LIST].n_value, tp->pr_istats);
		else if (tp->pr_stats)
			(*tp->pr_stats)(nl[tp->pr_sindex].n_value,
				tp->pr_name);
		else
			printf("%s: no stats routine\n", tp->pr_name);
		exit(0);
	}
	if (qflag) {
		print_softintrq();
		exit(0);
	}
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
	/*
	 * If -f was used afnames != NULL, loop over the address families.
	 * Otherwise do this at least once (with af == AF_UNSPEC).
	 */
	afname = NULL;
	do {
		if (afnames != NULL) {
			afname = strsep(&afnames, ",");
			if (afname == NULL)
				break;		/* Exit early */
			if (strcmp(afname, "inet") == 0)
				af = AF_INET;
			else if (strcmp(afname, "inet6") == 0)
				af = AF_INET6;
			else if (strcmp(afname, "arp") == 0)
				af = AF_ARP;
			else if (strcmp(afname, "pfkey") == 0)
				af = PF_KEY;
			else if (strcmp(afname, "unix") == 0
			    || strcmp(afname, "local") == 0)
				af = AF_LOCAL;
			else if (strcmp(afname, "atalk") == 0)
				af = AF_APPLETALK;
			else if (strcmp(afname, "mpls") == 0)
				af = AF_MPLS;
			else {
				warnx("%s: unknown address family",
				    afname);
				continue;
			}
		}

		if (iflag) {
			if (af != AF_UNSPEC)
				goto protostat;

			intpr(interval, nl[N_IFNET_LIST].n_value, NULL);
			break;
		}
		if (rflag) {
			if (sflag)
				rt_stats(use_sysctl ? 0 : nl[N_RTSTAT].n_value);
			else {
				if (use_sysctl)
					p_rttables(af,
					    nflag|tagflag|vflag|Lflag, 0, ~0);
				else
					routepr(nl[N_RTREE].n_value);
			}
			break;
		}
#ifndef SMALL
		if (gflag) {
			if (sflag) {
				if (af == AF_INET || af == AF_UNSPEC)
					mrt_stats(nl[N_MRTPROTO].n_value,
						  nl[N_MRTSTAT].n_value);
#ifdef INET6
				if (af == AF_INET6 || af == AF_UNSPEC)
					mrt6_stats(nl[N_MRT6PROTO].n_value,
						   nl[N_MRT6STAT].n_value);
#endif
			}
			else {
				if (af == AF_INET || af == AF_UNSPEC)
					mroutepr(nl[N_MRTPROTO].n_value,
						 nl[N_MFCHASHTBL].n_value,
						 nl[N_MFCHASH].n_value,
						 nl[N_VIFTABLE].n_value);
#ifdef INET6
				if (af == AF_INET6 || af == AF_UNSPEC)
					mroute6pr(nl[N_MRT6PROTO].n_value,
						  nl[N_MF6CTABLE].n_value,
						  nl[N_MIF6TABLE].n_value);
#endif
			}
			break;
		}
#endif
	  protostat:
		if (af == AF_INET || af == AF_UNSPEC) {
			setprotoent(1);
			setservent(1);
			/* ugh, this is O(MN) ... why do we do this? */
			while ((p = getprotoent()) != NULL) {
				for (tp = protox; tp->pr_name; tp++)
					if (strcmp(tp->pr_name, p->p_name) == 0)
						break;
				if (tp->pr_name == 0 || tp->pr_wanted == 0)
					continue;
				printproto(tp, p->p_name);
				tp->pr_wanted = 0;
			}
			endprotoent();
			for (tp = protox; tp->pr_name; tp++)
				if (tp->pr_wanted)
					printproto(tp, tp->pr_name);
		}
#ifdef INET6
		if (af == AF_INET6 || af == AF_UNSPEC)
			for (tp = ip6protox; tp->pr_name; tp++)
				printproto(tp, tp->pr_name);
#endif
		if (af == AF_ARP || af == AF_UNSPEC)
			for (tp = arpprotox; tp->pr_name; tp++)
				printproto(tp, tp->pr_name);
#ifdef IPSEC
		if (af == PF_KEY || af == AF_UNSPEC)
			for (tp = pfkeyprotox; tp->pr_name; tp++)
				printproto(tp, tp->pr_name);
#endif
#ifndef SMALL
		if (af == AF_APPLETALK || af == AF_UNSPEC)
			for (tp = atalkprotox; tp->pr_name; tp++)
				printproto(tp, tp->pr_name);
		if ((af == AF_LOCAL || af == AF_UNSPEC) && !sflag)
			unixpr(nl[N_UNIXSW].n_value);
#endif
	} while (afnames != NULL && afname != NULL);
	exit(0);
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * If the interface was not specifically requested, and the symbol
 * is not in the namelist, ignore this one.
 */
static void
printproto(struct protox *tp, const char *name)
{
	void (*pr) __P((u_long, const char *));
	u_long off;

	if (sflag) {
		if (iflag) {
			if (tp->pr_istats)
				intpr(interval, nl[N_IFNET_LIST].n_value,
				      tp->pr_istats);
			return;
		}
		else {
			pr = tp->pr_stats;
			off = nl[tp->pr_sindex].n_value;
		}
	} else {
		pr = tp->pr_cblocks;
		off = nl[tp->pr_index].n_value;
	}
	if (pr != NULL && ((off || af != AF_UNSPEC) || use_sysctl)) {
		(*pr)(off, name);
	}
}

/*
 * Print softintrq status.
 */
void
print_softintrq(void)
{
	struct ifqueue intrq, *ifq = &intrq;
	const struct softintrq *siq;
	u_long off;

	for (siq = softintrq; siq->siq_name != NULL; siq++) {
		off = nl[siq->siq_index].n_value;
		if (off == 0)
			continue;

		kread(off, (char *)ifq, sizeof(*ifq));
		printf("%s:\n", siq->siq_name);
		printf("\tqueue length: %d\n", ifq->ifq_len);
		printf("\tmaximum queue length: %d\n", ifq->ifq_maxlen);
		printf("\tpackets dropped: %d\n", ifq->ifq_drops);
	}
}

/*
 * Read kernel memory, return 0 on success.
 */
int
kread(u_long addr, char *buf, int size)
{

	if (kvm_read(kvmd, addr, buf, size) != size) {
		warnx("%s", kvm_geterr(kvmd));
		return (-1);
	}
	return (0);
}

const char *
plural(int n)
{

	return (n != 1 ? "s" : "");
}

const char *
plurales(int n)
{

	return (n != 1 ? "es" : "");
}

int
get_hardticks(void)
{
	int hardticks;

	kread(nl[N_HARDCLOCK_TICKS].n_value, (char *)&hardticks,
	    sizeof(hardticks));
	return (hardticks);
}

/*
 * Find the protox for the given "well-known" name.
 */
static struct protox *
knownname(const char *name)
{
	struct protox **tpp, *tp;

	for (tpp = protoprotox; *tpp; tpp++)
		for (tp = *tpp; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, name) == 0)
				return (tp);
	return (NULL);
}

/*
 * Find the protox corresponding to name.
 */
static struct protox *
name2protox(const char *name)
{
	struct protox *tp;
	char **alias;			/* alias from p->aliases */
	struct protoent *p;

	/*
	 * Try to find the name in the list of "well-known" names. If that
	 * fails, check if name is an alias for an Internet protocol.
	 */
	if ((tp = knownname(name)) != NULL)
		return (tp);

	setprotoent(1);			/* make protocol lookup cheaper */
	while ((p = getprotoent()) != NULL) {
		/* assert: name not same as p->name */
		for (alias = p->p_aliases; *alias; alias++)
			if (strcmp(name, *alias) == 0) {
				endprotoent();
				return (knownname(p->p_name));
			}
	}
	endprotoent();
	return (NULL);
}

static void
usage(void)
{
	const char *progname = getprogname();

	(void)fprintf(stderr,
"usage: %s [-Aan] [-f address_family[,family ...]] [-M core] [-N system]\n", progname);
	(void)fprintf(stderr,
"       %s [-bdgiLmnqrsSv] [-f address_family[,family ...]] [-M core] [-N system]\n", 
	progname);
	(void)fprintf(stderr,
"       %s [-dn] [-I interface] [-M core] [-N system] [-w wait]\n", progname);
	(void)fprintf(stderr,
"       %s [-p protocol] [-M core] [-N system]\n", progname);
	(void)fprintf(stderr,
"       %s [-p protocol] [-M core] [-N system] -P pcbaddr\n", progname);
	(void)fprintf(stderr,
"       %s [-p protocol] [-i] [-I Interface] \n", progname);
	(void)fprintf(stderr,
"       %s [-s] [-f address_family[,family ...]] [-i] [-I Interface]\n", progname);
	(void)fprintf(stderr,
"       %s [-s] [-B] [-I interface]\n", progname);
	exit(1);
}
