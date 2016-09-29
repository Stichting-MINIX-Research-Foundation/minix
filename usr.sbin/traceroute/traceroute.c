/*	$NetBSD: traceroute.c,v 1.81 2012/08/16 00:40:28 zafer Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char rcsid[] =
    "@(#)Id: traceroute.c,v 1.68 2000/12/14 08:04:33 leres Exp  (LBL)";
#else
__COPYRIGHT("@(#) Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996, 1997,\
 1998, 1999, 2000\
 The Regents of the University of California.  All rights reserved.");
__RCSID("$NetBSD: traceroute.c,v 1.81 2012/08/16 00:40:28 zafer Exp $");
#endif
#endif

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to 30 hops & can be changed with the -m flag).  Three
 * probes (change with -q flag) are sent at each ttl setting and a
 * line is printed showing the ttl, address of the gateway and
 * round trip time of each probe.  If the probe answers come from
 * different gateways, the address of each responding system will
 * be printed.  If there is no response within a 5 sec. timeout
 * interval (changed with the -w flag), a "*" is printed for that
 * probe.
 *
 * Probe packets are UDP format.  We don't want the destination
 * host to process them so the destination port is set to an
 * unlikely value (if some clod on the destination is using that
 * value, it can be changed with the -p flag).
 *
 * A sample use might be:
 *
 *     [yak 71]% traceroute nis.nsf.net.
 *     traceroute to nis.nsf.net (35.1.1.48), 30 hops max, 56 byte packet
 *      1  helios.ee.lbl.gov (128.3.112.1)  19 ms  19 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  39 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  40 ms  59 ms  59 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  59 ms
 *      8  129.140.70.13 (129.140.70.13)  99 ms  99 ms  80 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  239 ms  319 ms
 *     10  129.140.81.7 (129.140.81.7)  220 ms  199 ms  199 ms
 *     11  nic.merit.edu (35.1.1.48)  239 ms  239 ms  239 ms
 *
 * Note that lines 2 & 3 are the same.  This is due to a buggy
 * kernel on the 2nd hop system -- lbl-csam.arpa -- that forwards
 * packets with a zero ttl.
 *
 * A more interesting example is:
 *
 *     [yak 72]% traceroute allspice.lcs.mit.edu.
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 30 hops max
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  19 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  19 ms  39 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  20 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  59 ms  119 ms  39 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  39 ms
 *      8  129.140.70.13 (129.140.70.13)  80 ms  79 ms  99 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  139 ms  159 ms
 *     10  129.140.81.7 (129.140.81.7)  199 ms  180 ms  300 ms
 *     11  129.140.72.17 (129.140.72.17)  300 ms  239 ms  239 ms
 *     12  * * *
 *     13  128.121.54.72 (128.121.54.72)  259 ms  499 ms  279 ms
 *     14  * * *
 *     15  * * *
 *     16  * * *
 *     17  * * *
 *     18  ALLSPICE.LCS.MIT.EDU (18.26.0.115)  339 ms  279 ms  279 ms
 *
 * (I start to see why I'm having so much trouble with mail to
 * MIT.)  Note that the gateways 12, 14, 15, 16 & 17 hops away
 * either don't send ICMP "time exceeded" messages or send them
 * with a ttl too small to reach us.  14 - 17 are running the
 * MIT C Gateway code that doesn't send "time exceeded"s.  God
 * only knows what's going on with 12.
 *
 * The silent gateway 12 in the above may be the result of a bug in
 * the 4.[23]BSD network code (and its derivatives):  4.x (x <= 3)
 * sends an unreachable message using whatever ttl remains in the
 * original datagram.  Since, for gateways, the remaining ttl is
 * zero, the icmp "time exceeded" is guaranteed to not make it back
 * to us.  The behavior of this bug is slightly more interesting
 * when it appears on the destination system:
 *
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  39 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  19 ms
 *      5  ccn-nerif35.Berkeley.EDU (128.32.168.35)  39 ms  39 ms  39 ms
 *      6  csgw.Berkeley.EDU (128.32.133.254)  39 ms  59 ms  39 ms
 *      7  * * *
 *      8  * * *
 *      9  * * *
 *     10  * * *
 *     11  * * *
 *     12  * * *
 *     13  rip.Berkeley.EDU (128.32.131.22)  59 ms !  39 ms !  39 ms !
 *
 * Notice that there are 12 "gateways" (13 is the final
 * destination) and exactly the last half of them are "missing".
 * What's really happening is that rip (a Sun-3 running Sun OS3.5)
 * is using the ttl from our arriving datagram as the ttl in its
 * icmp reply.  So, the reply will time out on the return path
 * (with no notice sent to anyone since icmp's aren't sent for
 * icmp's) until we probe with a ttl that's at least twice the path
 * length.  I.e., rip is really only 7 hops away.  A reply that
 * returns with a ttl of 1 is a clue this problem exists.
 * Traceroute prints a "!" after the time if the ttl is <= 1.
 * Since vendors ship a lot of obsolete (DEC's Ultrix, Sun 3.x) or
 * non-standard (HPUX) software, expect to see this problem
 * frequently and/or take care picking the target host of your
 * probes.
 *
 * Other possible annotations after the time are !H, !N, !P (got a host,
 * network or protocol unreachable, respectively), !S or !F (source
 * route failed or fragmentation needed -- neither of these should
 * ever occur and the associated gateway is busted if you see one).  If
 * almost all the probes result in some kind of unreachable, traceroute
 * will give up and exit.
 *
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * This program requires a kernel mod that does not appear in any
 * system available from Berkeley:  A raw ip socket using proto
 * IPPROTO_RAW must interpret the data sent as an ip datagram (as
 * opposed to data to be wrapped in a ip datagram).  See the README
 * file that came with the source to this program for a description
 * of the mods I made to /sys/netinet/raw_ip.c.  Your mileage may
 * vary.  But, again, ANY 4.x (x < 4) BSD KERNEL WILL HAVE TO BE
 * MODIFIED TO RUN THIS PROGRAM.
 *
 * The udp port usage may appear bizarre (well, ok, it is bizarre).
 * The problem is that an icmp message only contains 8 bytes of
 * data from the original datagram.  8 bytes is the size of a udp
 * header so, if we want to associate replies with the original
 * datagram, the necessary information must be encoded into the
 * udp header (the ip id could be used but there's no way to
 * interlock with the kernel's assignment of ip id's and, anyway,
 * it would have taken a lot more kernel hacking to allow this
 * code to set the ip id).  So, to allow two or more users to
 * use traceroute simultaneously, we use this task's pid as the
 * source port (the high bit is set to move the port number out
 * of the "likely" range).  To keep track of which probe is being
 * replied to (so times and/or hop counts don't get confused by a
 * reply that was delayed in transit), we increment the destination
 * port number before each probe.
 *
 * Don't use this as a coding example.  I was trying to find a
 * routing problem and this code sort-of popped out after 48 hours
 * without sleep.  I was amazed it ever compiled, much less ran.
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <memory.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#ifdef IPSEC
#include <net/route.h>
#include <netipsec/ipsec.h>
#endif

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

/* rfc1716 */
#ifndef ICMP_UNREACH_FILTER_PROHIB
#define ICMP_UNREACH_FILTER_PROHIB	13	/* admin prohibited filter */
#endif
#ifndef ICMP_UNREACH_HOST_PRECEDENCE
#define ICMP_UNREACH_HOST_PRECEDENCE	14	/* host precedence violation */
#endif
#ifndef ICMP_UNREACH_PRECEDENCE_CUTOFF
#define ICMP_UNREACH_PRECEDENCE_CUTOFF	15	/* precedence cutoff */
#endif

#include "ifaddrlist.h"
#include "as.h"
#include "prog_ops.h"

/* Maximum number of gateways (include room for one noop) */
#define NGATEWAYS ((int)((MAX_IPOPTLEN - IPOPT_MINOFF - 1) / sizeof(u_int32_t)))

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

#define Fprintf (void)fprintf
#define Printf (void)printf

/* Host name and address list */
struct hostinfo {
	char *name;
	int n;
	u_int32_t *addrs;
};

/* Data section of the probe packet */
struct outdata {
	u_char seq;		/* sequence number of this packet */
	u_char ttl;		/* ttl packet left with */
	struct tv32 {
		int32_t tv32_sec;
		int32_t tv32_usec;
	} tv;			/* time packet left */
};

/*
 * Support for ICMP extensions
 *
 * http://www.ietf.org/proceedings/01aug/I-D/draft-ietf-mpls-icmp-02.txt
 */
#define ICMP_EXT_OFFSET    8 /* ICMP type, code, checksum, unused */ + \
                         128 /* original datagram */
#define ICMP_EXT_VERSION 2
/*
 * ICMP extensions, common header
 */
struct icmp_ext_cmn_hdr {
#if BYTE_ORDER == BIG_ENDIAN
	unsigned char   version:4;
	unsigned char   reserved1:4;
#else
	unsigned char   reserved1:4;
	unsigned char   version:4;
#endif
	unsigned char   reserved2;
	unsigned short  checksum;
};

/*
 * ICMP extensions, object header
 */
struct icmp_ext_obj_hdr {
    u_short length;
    u_char  class_num;
#define MPLS_STACK_ENTRY_CLASS 1
    u_char  c_type;
#define MPLS_STACK_ENTRY_C_TYPE 1
};

struct mpls_header {
#if BYTE_ORDER == BIG_ENDIAN
	 uint32_t	label:20;
	 unsigned char  exp:3;
	 unsigned char  s:1;
	 unsigned char  ttl:8;
#else
	 unsigned char  ttl:8;
	 unsigned char  s:1;
	 unsigned char  exp:3;
	 uint32_t	label:20;
#endif
};

#ifndef HAVE_ICMP_NEXTMTU
/* Path MTU Discovery (RFC1191) */
struct my_pmtu {
	u_short ipm_void;
	u_short ipm_nextmtu;
};
#endif

static u_char	packet[512];		/* last inbound (icmp) packet */

static struct ip *outip;		/* last output (udp) packet */
static struct udphdr *outudp;		/* last output (udp) packet */
static void *outmark;			/* packed location of struct outdata */
static struct outdata outsetup;	/* setup and copy for alignment */

static struct icmp *outicmp;		/* last output (icmp) packet */

/* loose source route gateway list (including room for final destination) */
static u_int32_t gwlist[NGATEWAYS + 1];

static int s;				/* receive (icmp) socket file descriptor */
static int sndsock;			/* send (udp/icmp) socket file descriptor */

static struct sockaddr whereto;		/* Who to try to reach */
static struct sockaddr wherefrom;	/* Who we are */
static int packlen;			/* total length of packet */
static int minpacket;			/* min ip packet size */
static int maxpacket = 32 * 1024;	/* max ip packet size */
static int printed_ttl = 0;
static int pmtu;			/* Path MTU Discovery (RFC1191) */
static u_int pausemsecs;

static const char *prog;
static char *source;
static char *hostname;
static char *device;
#ifdef notdef
static const char devnull[] = "/dev/null";
#endif

static int nprobes = 3;
static int max_ttl = 30;
static int first_ttl = 1;
static u_int16_t ident;
static in_port_t port = 32768 + 666;	/* start udp dest port # for probe packets */

static int options;			/* socket options */
static int verbose;
static int waittime = 5;		/* time to wait for response (in seconds) */
static int nflag;			/* print addresses numerically */
static int dump;
static int Mflag;			/* show MPLS labels if any */
static int as_path;			/* print as numbers for each hop */
static char *as_server = NULL;
static void *asn;
static int useicmp = 0;		/* use icmp echo instead of udp packets */
#ifdef CANT_HACK_CKSUM
static int doipcksum = 0;		/* don't calculate checksums */
#else
static int doipcksum = 1;		/* calculate checksums */
#endif
static int optlen;			/* length of ip options */

static int mtus[] = {
        17914,
         8166,
         4464,
         4352,
         2048,
         2002,
         1536,
         1500,
         1492,
         1480,
         1280,
         1006,
          576,
          552,
          544,
          512,
          508,
          296,
           68,
            0
};
static int *mtuptr = &mtus[0];
static int mtudisc = 0;
static int nextmtu;   /* from ICMP error, set by packet_ok(), might be 0 */

/* Forwards */
static double deltaT(struct timeval *, struct timeval *);
static void freehostinfo(struct hostinfo *);
static void getaddr(u_int32_t *, char *);
static struct hostinfo *gethostinfo(char *);
static u_int16_t in_cksum(u_int16_t *, int);
static u_int16_t in_cksum2(u_int16_t, u_int16_t *, int);
static char *inetname(struct in_addr);
static int packet_ok(u_char *, ssize_t, struct sockaddr_in *, int);
static const char *pr_type(u_char);
static void print(u_char *, int, struct sockaddr_in *);
static void resize_packet(void);
static void dump_packet(void);
static void send_probe(int, int, struct timeval *);
static void setsin(struct sockaddr_in *, u_int32_t);
static int str2val(const char *, const char *, int, int);
static void tvsub(struct timeval *, struct timeval *);
static void usage(void) __attribute__((__noreturn__));
static ssize_t wait_for_reply(int, struct sockaddr_in *, const struct timeval *);
static void decode_extensions(unsigned char *buf, int ip_len);
static void frag_err(void);
static int find_local_ip(struct sockaddr_in *, struct sockaddr_in *);
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
static int setpolicy(int, const char *);
#endif
#endif

int
main(int argc, char **argv)
{
	int op, code, n;
	u_char *outp;
	u_int32_t *ap;
	struct sockaddr_in *from = (struct sockaddr_in *)&wherefrom;
	struct sockaddr_in *to = (struct sockaddr_in *)&whereto;
	struct hostinfo *hi;
	int on = 1;
	int ttl, probe, i;
	int seq = 0;
	int tos = 0, settos = 0, ttl_flag = 0;
	int lsrr = 0;
	u_int16_t off = 0;
	struct ifaddrlist *al, *al2;
	char errbuf[132];
	int mib[4] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_DEFTTL };
	size_t size = sizeof(max_ttl);

	setprogname(argv[0]);
	prog = getprogname();

	if (prog_init && prog_init() == -1)
		err(1, "init failed");

#ifdef notdef
	/* Kernel takes care of it */
	/* Insure the socket fds won't be 0, 1 or 2 */
	if (open(devnull, O_RDONLY) < 0 ||
	    open(devnull, O_RDONLY) < 0 ||
	    open(devnull, O_RDONLY) < 0)
		err(1, "Cannot open `%s'", devnull);
#endif
	if ((s = prog_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
		err(1, "icmp socket");

	/*
	 * XXX 'useicmp' will always be zero here. I think the HP-UX users
	 * running our traceroute code will forgive us.
	 */
#ifndef __hpux
	sndsock = prog_socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
#else
	sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW
	    useicmp ? IPPROTO_ICMP : IPPROTO_UDP);
#endif
	if (sndsock < 0)
		err(1, "raw socket");

	(void) prog_sysctl(mib, sizeof(mib)/sizeof(mib[0]), &max_ttl, &size,
	    NULL, 0);

	opterr = 0;
	while ((op = getopt(argc, argv, "aA:dDFPIMnlrvxf:g:i:m:p:q:s:t:w:z:")) != -1)
		switch (op) {

		case 'a':
			as_path = 1;
			break;

		case 'A':
			as_path = 1;
			as_server = optarg;
			break;

		case 'd':
			options |= SO_DEBUG;
			break;

		case 'D':
			dump = 1;
			break;

		case 'f':
			first_ttl = str2val(optarg, "first ttl", 1, 255);
			break;

		case 'F':
			off = IP_DF;
			break;

		case 'g':
			if (lsrr >= NGATEWAYS)
				errx(1, "more than %d gateways", NGATEWAYS);
			getaddr(gwlist + lsrr, optarg);
			++lsrr;
			break;

		case 'i':
			device = optarg;
			break;

		case 'I':
			++useicmp;
			break;

		case 'l':
			++ttl_flag;
			break;

		case 'm':
			max_ttl = str2val(optarg, "max ttl", 1, 255);
			break;

		case 'M':
			Mflag = 1;
			break;

		case 'n':
			++nflag;
			break;

		case 'p':
			port = (u_short)str2val(optarg, "port",
			    1, (1 << 16) - 1);
			break;

		case 'q':
			nprobes = str2val(optarg, "nprobes", 1, -1);
			break;

		case 'r':
			options |= SO_DONTROUTE;
			break;

		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;

		case 't':
			tos = str2val(optarg, "tos", 0, 255);
			++settos;
			break;

		case 'v':
			++verbose;
			break;

		case 'x':
			doipcksum = (doipcksum == 0);
			break;

		case 'w':
			waittime = str2val(optarg, "wait time",
			    2, 24 * 60 * 60);
			break;

		case 'z':
			pausemsecs = str2val(optarg, "pause msecs",
			    0, 60 * 60 * 1000);

		case 'P':
			off = IP_DF;
			mtudisc = 1;
			break;

		default:
			usage();
		}

	if (first_ttl > max_ttl)
		errx(1, "first ttl (%d) may not be greater than max ttl (%d)",
		    first_ttl, max_ttl);

	if (!doipcksum)
		warnx("ip checksums disabled");

	if (lsrr > 0)
		optlen = (lsrr + 1) * sizeof(gwlist[0]);
	minpacket = sizeof(*outip) + sizeof(struct outdata) + optlen;
	if (useicmp)
		minpacket += 8;			/* XXX magic number */
	else
		minpacket += sizeof(*outudp);
	packlen = minpacket;		/* minimum sized packet */

	if (mtudisc)
		packlen = *mtuptr++;

	/* Process destination and optional packet size */
	switch (argc - optind) {

	case 2:
		packlen = str2val(argv[optind + 1],
		    "packet length", minpacket, maxpacket);
		/* Fall through */

	case 1:
		hostname = argv[optind];
		hi = gethostinfo(hostname);
		setsin(to, hi->addrs[0]);
		if (hi->n > 1)
			warnx("%s has multiple addresses; using %s",
			    hostname, inet_ntoa(to->sin_addr));
		hostname = hi->name;
		hi->name = NULL;
		freehostinfo(hi);
		break;

	default:
		usage();
	}

#ifdef HAVE_SETLINEBUF
	setlinebuf (stdout);
#else
	setvbuf(stdout, NULL, _IOLBF, 0);
#endif

	outip = malloc((unsigned)packlen);
	if (outip == NULL)
		err(1, "malloc");
	memset(outip, 0, packlen);

	outip->ip_v = IPVERSION;
	if (settos)
		outip->ip_tos = tos;
#ifdef BYTESWAP_IP_HDR
	outip->ip_len = htons(packlen);
	outip->ip_off = htons(off);
#else
	outip->ip_len = packlen;
	outip->ip_off = off;
#endif
	outp = (u_char *)(outip + 1);
#ifdef HAVE_RAW_OPTIONS
	if (lsrr > 0) {
		u_char *optlist;

		optlist = outp;
		outp += optlen;

		/* final hop */
		gwlist[lsrr] = to->sin_addr.s_addr;

		outip->ip_dst.s_addr = gwlist[0];

		/* force 4 byte alignment */
		optlist[0] = IPOPT_NOP;
		/* loose source route option */
		optlist[1] = IPOPT_LSRR;
		i = lsrr * sizeof(gwlist[0]);
		optlist[2] = i + 3;
		/* Pointer to LSRR addresses */
		optlist[3] = IPOPT_MINOFF;
		memcpy(optlist + 4, gwlist + 1, i);
	} else
#endif
		outip->ip_dst = to->sin_addr;

	outip->ip_hl = (outp - (u_char *)outip) >> 2;
	ident = htons(arc4random() & 0xffff) | 0x8000;
	if (useicmp) {
		outip->ip_p = IPPROTO_ICMP;

		outicmp = (struct icmp *)outp;
		outicmp->icmp_type = ICMP_ECHO;
		outicmp->icmp_id = htons(ident);

		outmark = outp + 8;	/* XXX magic number */
	} else {
		outip->ip_p = IPPROTO_UDP;

		outudp = (struct udphdr *)outp;
		outudp->uh_sport = htons(ident);
		outudp->uh_ulen =
		    htons((u_int16_t)(packlen - (sizeof(*outip) + optlen)));
		outmark = outudp + 1;
	}

	if (options & SO_DEBUG)
		(void)prog_setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&on,
		    sizeof(on));
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	/*
	 * do not raise error even if setsockopt fails, kernel may have ipsec
	 * turned off.
	 */
	if (setpolicy(s, "in bypass") < 0)
		exit(1);
	if (setpolicy(s, "out bypass") < 0)
		exit(1);
#else
    {
	int level = IPSEC_LEVEL_AVAIL;

	(void)prog_setsockopt(s, IPPROTO_IP, IP_ESP_TRANS_LEVEL, &level,
		sizeof(level));
	(void)prog_setsockopt(s, IPPROTO_IP, IP_ESP_NETWORK_LEVEL, &level,
		sizeof(level));
#ifdef IP_AUTH_TRANS_LEVEL
	(void)prog_setsockopt(s, IPPROTO_IP, IP_AUTH_TRANS_LEVEL, &level,
		sizeof(level));
#else
	(void)prog_setsockopt(s, IPPROTO_IP, IP_AUTH_LEVEL, &level,
		sizeof(level));
#endif
#ifdef IP_AUTH_NETWORK_LEVEL
	(void)prog_setsockopt(s, IPPROTO_IP, IP_AUTH_NETWORK_LEVEL, &level,
		sizeof(level));
#endif
    }
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/

#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	/*
	 * do not raise error even if setsockopt fails, kernel may have ipsec
	 * turned off.
	 */
	if (setpolicy(sndsock, "in bypass") < 0)
		exit(1);
	if (setpolicy(sndsock, "out bypass") < 0)
		exit(1);
#else
    {
	int level = IPSEC_LEVEL_BYPASS;

	(void)prog_setsockopt(sndsock, IPPROTO_IP, IP_ESP_TRANS_LEVEL, &level,
		sizeof(level));
	(void)prog_setsockopt(sndsock, IPPROTO_IP, IP_ESP_NETWORK_LEVEL, &level,
		sizeof(level));
#ifdef IP_AUTH_TRANS_LEVEL
	(void)prog_setsockopt(sndsock, IPPROTO_IP, IP_AUTH_TRANS_LEVEL, &level,
		sizeof(level));
#else
	(void)prog_setsockopt(sndsock, IPPROTO_IP, IP_AUTH_LEVEL, &level,
		sizeof(level));
#endif
#ifdef IP_AUTH_NETWORK_LEVEL
	(void)prog_setsockopt(sndsock, IPPROTO_IP, IP_AUTH_NETWORK_LEVEL, &level,
		sizeof(level));
#endif
    }
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/

#if defined(IP_OPTIONS) && !defined(HAVE_RAW_OPTIONS)
	if (lsrr > 0) {
		u_char optlist[MAX_IPOPTLEN];

		/* final hop */
		gwlist[lsrr] = to->sin_addr.s_addr;
		++lsrr;

		/* force 4 byte alignment */
		optlist[0] = IPOPT_NOP;
		/* loose source route option */
		optlist[1] = IPOPT_LSRR;
		i = lsrr * sizeof(gwlist[0]);
		optlist[2] = i + 3;
		/* Pointer to LSRR addresses */
		optlist[3] = IPOPT_MINOFF;
		memcpy(optlist + 4, gwlist, i);

		if ((prog_setsockopt(sndsock, IPPROTO_IP, IP_OPTIONS, optlist,
		    i + sizeof(gwlist[0]))) < 0)
			err(1, "IP_OPTIONS");
	}
#endif

#ifdef SO_SNDBUF
	if (prog_setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&packlen,
	    sizeof(packlen)) < 0)
		err(1, "SO_SNDBUF");
#endif
#ifdef IP_HDRINCL
	if (prog_setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
	    sizeof(on)) < 0)
		err(1, "IP_HDRINCL");
#else
#ifdef IP_TOS
	if (settos && prog_setsockopt(sndsock, IPPROTO_IP, IP_TOS,
	    &tos, sizeof(tos)) < 0)
		err(1, "setsockopt tos %d", tos);
#endif
#endif
	if (options & SO_DEBUG)
		if (prog_setsockopt(sndsock, SOL_SOCKET, SO_DEBUG, &on,
		    sizeof(on)) < 0)
			err(1, "setsockopt debug %d", tos);
	if (options & SO_DONTROUTE)
		if (prog_setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE, &on,
		    sizeof(on)) < 0)
			err(1, "setsockopt dontroute %d", tos);

	/* Get the interface address list */
	n = ifaddrlist(&al, errbuf, sizeof errbuf);
	al2 = al;
	if (n < 0)
		errx(1, "ifaddrlist (%s)", errbuf);
	if (n == 0)
		errx(1, "Can't find any network interfaces");

	/* Look for a specific device */
	if (device != NULL) {
		for (i = n; i > 0; --i, ++al2)
			if (strcmp(device, al2->device) == 0)
				break;
		if (i <= 0)
			errx(1, "Can't find interface %.32s", device);
	}

	/* Determine our source address */
	if (source == NULL) {
		/*
		 * If a device was specified, use the interface address.
		 * Otherwise, try to determine our source address.
		 * Warn if there are more than one.
		 */
		setsin(from, al2->addr);
		if (n > 1 && device == NULL && !find_local_ip(from, to)) {
			warnx("Multiple interfaces found; using %s @ %s",
			    inet_ntoa(from->sin_addr), al2->device);
		}
	} else {
		hi = gethostinfo(source);
		source = hi->name;
		hi->name = NULL;
		if (device == NULL) {
			/*
			 * Use the first interface found.
			 * Warn if there are more than one.
			 */
			setsin(from, hi->addrs[0]);
			if (hi->n > 1)
				warnx("%s has multiple addresses; using %s",
				    source, inet_ntoa(from->sin_addr));
		} else {
			/*
			 * Make sure the source specified matches the
			 * interface address.
			 */
			for (i = hi->n, ap = hi->addrs; i > 0; --i, ++ap)
				if (*ap == al2->addr)
					break;
			if (i <= 0)
				errx(1, "%s is not on interface %s",
				    source, device);
			setsin(from, *ap);
		}
		freehostinfo(hi);
	}

	/* Revert to non-privileged user after opening sockets */
	setgid(getgid());
	setuid(getuid());

	/* 
	 * If not root, make sure source address matches a local interface.
	 * (The list of addresses produced by ifaddrlist() automatically
	 * excludes interfaces that are marked down and/or loopback.)
	 */
	if (getuid())  {
		al2 = al;
		for (i = n; i > 0; --i, ++al2)
			if (from->sin_addr.s_addr == al2->addr)
			    break;
		if (i <= 0)
			errx(1, "%s is not a valid local address "
			    "and you are not superuser.",
			    inet_ntoa(from->sin_addr));
	}

	outip->ip_src = from->sin_addr;
#ifndef IP_HDRINCL
	if (bind(sndsock, (struct sockaddr *)from, sizeof(*from)) < 0)
		err(1, "bind");
#endif

	if (as_path) {
		asn = as_setup(as_server);
		if (asn == NULL) {
			warnx("as_setup failed, AS# lookups disabled");
			(void)fflush(stderr);
			as_path = 0;
		}
	}

	setuid(getuid());
	Fprintf(stderr, "%s to %s (%s)",
	    prog, hostname, inet_ntoa(to->sin_addr));
	if (source)
		Fprintf(stderr, " from %s", source);
	Fprintf(stderr, ", %d hops max, %d byte packets\n", max_ttl, packlen);
	(void)fflush(stderr);

	for (ttl = first_ttl; ttl <= max_ttl; ++ttl) {
		u_int32_t lastaddr = 0;
		int gotlastaddr = 0;
		int got_there = 0;
		int unreachable = 0;
		int sentfirst = 0;

again:
		printed_ttl = 0;
		for (probe = 0; probe < nprobes; ++probe) {
			int cc;
			struct timeval t1, t2;
			struct ip *ip;
			if (sentfirst && pausemsecs > 0)
				usleep(pausemsecs * 1000);
			(void)gettimeofday(&t1, NULL);
			if (!useicmp && htons(port + seq + 1) == 0)
				seq++;
			send_probe(++seq, ttl, &t1);
			++sentfirst;
			while ((cc = wait_for_reply(s, from, &t1)) != 0) {
				(void)gettimeofday(&t2, NULL);
				/*
				 * Since we'll be receiving all ICMP
				 * messages to this host above, we may
				 * never end up with cc=0, so we need
				 * an additional termination check.
				 */
				if (t2.tv_sec - t1.tv_sec > waittime) {
					cc = 0;
					break;
				}
				i = packet_ok(packet, cc, from, seq);
				/* Skip short packet */
				if (i == 0)
					continue;
				if (!gotlastaddr ||
				    from->sin_addr.s_addr != lastaddr) {
					if (gotlastaddr) printf("\n   ");
					print(packet, cc, from);
					lastaddr = from->sin_addr.s_addr;
					++gotlastaddr;
				}
				ip = (struct ip *)packet;
				Printf("  %.3f ms", deltaT(&t1, &t2));
				if (ttl_flag)
					Printf(" (ttl = %d)", ip->ip_ttl);
				if (i == -2) {
#ifndef ARCHAIC
					if (ip->ip_ttl <= 1)
						Printf(" !");
#endif
					++got_there;
					break;
				}

				/* time exceeded in transit */
				if (i == -1)
					break;
				code = i - 1;
				switch (code) {

				case ICMP_UNREACH_PORT:
#ifndef ARCHAIC
					if (ip->ip_ttl <= 1)
						Printf(" !");
#endif
					++got_there;
					break;

				case ICMP_UNREACH_NET:
					++unreachable;
					Printf(" !N");
					break;

				case ICMP_UNREACH_HOST:
					++unreachable;
					Printf(" !H");
					break;

				case ICMP_UNREACH_PROTOCOL:
					++got_there;
					Printf(" !P");
					break;

				case ICMP_UNREACH_NEEDFRAG:
					if (mtudisc) {
						frag_err();
						goto again;
					} else {
						++unreachable;
						Printf(" !F-%d", pmtu);
					}
					break;

				case ICMP_UNREACH_SRCFAIL:
					++unreachable;
					Printf(" !S");
					break;

				case ICMP_UNREACH_FILTER_PROHIB:
					++unreachable;
					Printf(" !X");
					break;

				case ICMP_UNREACH_HOST_PRECEDENCE:
					++unreachable;
					Printf(" !V");
					break;

				case ICMP_UNREACH_PRECEDENCE_CUTOFF:
					++unreachable;
					Printf(" !C");
					break;

				default:
					++unreachable;
					Printf(" !<%d>", code);
					break;
				}
				break;
			}
			if (cc == 0)
				Printf(" *");
			else if (cc && probe == nprobes - 1 && Mflag)
				decode_extensions(packet, cc);
			(void)fflush(stdout);
		}
		putchar('\n');
		if (got_there ||
		    (unreachable > 0 && unreachable >= ((nprobes + 1) / 2)))
			break;
	}

	if (as_path)
		as_shutdown(asn);

	exit(0);
}

static ssize_t
wait_for_reply(int sock, struct sockaddr_in *fromp, const struct timeval *tp)
{
	struct pollfd set[1];
	struct timeval now, wait;
	ssize_t cc = 0;
	socklen_t fromlen = sizeof(*fromp);
	int retval;

	set[0].fd = sock;
	set[0].events = POLLIN;

	wait.tv_sec = tp->tv_sec + waittime;
	wait.tv_usec = tp->tv_usec;
	(void)gettimeofday(&now, NULL);
	tvsub(&wait, &now);

	if (wait.tv_sec < 0) {
		wait.tv_sec = 0;
		wait.tv_usec = 0;
	}

	retval = prog_poll(set, 1, wait.tv_sec * 1000 + wait.tv_usec / 1000);
	if (retval < 0) 
		/* If we continue, we probably just flood the remote host. */
		err(1, "poll");
	if (retval > 0)  {
		cc = prog_recvfrom(sock, (char *)packet, sizeof(packet), 0,
			    (struct sockaddr *)fromp, &fromlen);
	}

	return cc;
}

static void
decode_extensions(unsigned char *buf, int ip_len)
{
        struct icmp_ext_cmn_hdr *cmn_hdr;
        struct icmp_ext_obj_hdr *obj_hdr;
        union {
                struct mpls_header mpls;
                uint32_t mpls_h;
        } mpls;
        size_t datalen, obj_len;
        struct ip *ip;

        ip = (struct ip *)buf;

        if (ip_len < (int)((ip->ip_hl << 2) + ICMP_EXT_OFFSET +
	    sizeof(struct icmp_ext_cmn_hdr))) {
		/*
		 * No support for ICMP extensions on this host
		 */
		return;
        }

        /*
         * Move forward to the start of the ICMP extensions, if present
         */
        buf += (ip->ip_hl << 2) + ICMP_EXT_OFFSET;
        cmn_hdr = (struct icmp_ext_cmn_hdr *)buf;

        if (cmn_hdr->version != ICMP_EXT_VERSION) {
		/*
		 * Unknown version
		 */
		return;
        }

        datalen = ip_len - ((u_char *)cmn_hdr - (u_char *)ip);

        /*
         * Check the checksum, cmn_hdr->checksum == 0 means no checksum'ing
         * done by sender.
         *
        * If the checksum is ok, we'll get 0, as the checksum is calculated
         * with the checksum field being 0'd.
         */
        if (ntohs(cmn_hdr->checksum) &&
            in_cksum((u_short *)cmn_hdr, datalen)) {
 
            return;
        }
 
        buf += sizeof(*cmn_hdr);
        datalen -= sizeof(*cmn_hdr);
 
        while (datalen >= sizeof(struct icmp_ext_obj_hdr)) {
		obj_hdr = (struct icmp_ext_obj_hdr *)buf;
		obj_len = ntohs(obj_hdr->length);

		/*
		 * Sanity check the length field
		 */
		if (obj_len > datalen)
			return;

		datalen -= obj_len;
 
		/*
		 * Move past the object header
		 */
		buf += sizeof(struct icmp_ext_obj_hdr);
		obj_len -= sizeof(struct icmp_ext_obj_hdr);
 
		switch (obj_hdr->class_num) {
		case MPLS_STACK_ENTRY_CLASS:
			switch (obj_hdr->c_type) {
			case MPLS_STACK_ENTRY_C_TYPE:
				while (obj_len >= sizeof(uint32_t)) {
					mpls.mpls_h = ntohl(*(uint32_t *)buf);
 
					buf += sizeof(uint32_t);
					obj_len -= sizeof(uint32_t);
 
					printf(" [MPLS: Label %d Exp %d]",
					    mpls.mpls.label, mpls.mpls.exp);
				}
				if (obj_len > 0) {
					/*
					 * Something went wrong, and we're at
					 * a unknown offset into the packet,
					 * ditch the rest of it.
					 */
					return;
				}
				break;
			default:
				/*
				 * Unknown object, skip past it
				 */
				buf += ntohs(obj_hdr->length) -
				    sizeof(struct icmp_ext_obj_hdr);
				break;
			}
			break;
 
		default:
			/*
			 * Unknown object, skip past it
			 */
			buf += ntohs(obj_hdr->length) -
			    sizeof(struct icmp_ext_obj_hdr);
			break;
		}
	}
}

static void
dump_packet(void)
{
	u_char *p;
	int i;

	Fprintf(stderr, "packet data:");

#ifdef __hpux
	for (p = useicmp ? (u_char *)outicmp : (u_char *)outudp, i = 0; i < 
	    i < packlen - (sizeof(*outip) + optlen); i++)
#else
	for (p = (u_char *)outip, i = 0; i < packlen; i++)
#endif
	{
		if ((i % 24) == 0)
			Fprintf(stderr, "\n ");
		Fprintf(stderr, " %02x", *p++);
	}
	Fprintf(stderr, "\n");
}

void
send_probe(int seq, int ttl, struct timeval *tp)
{
	int cc;
	struct udpiphdr * ui, *oui;
	int oldmtu = packlen;
 	struct ip tip;

again:
#ifdef BYTESWAP_IP_LEN
	outip->ip_len = htons(packlen);
#else
	outip->ip_len = packlen;
#endif
	outip->ip_ttl = ttl;
#ifndef __hpux
	outip->ip_id = htons(ident + seq);
#endif

	/*
	 * In most cases, the kernel will recalculate the ip checksum.
	 * But we must do it anyway so that the udp checksum comes out
	 * right.
	 */
	if (doipcksum) {
		outip->ip_sum =
		    in_cksum((u_int16_t *)outip, sizeof(*outip) + optlen);
		if (outip->ip_sum == 0)
			outip->ip_sum = 0xffff;
	}

	/* Payload */
	outsetup.seq = seq;
	outsetup.ttl = ttl;
	outsetup.tv.tv32_sec = htonl(tp->tv_sec);
	outsetup.tv.tv32_usec = htonl(tp->tv_usec);
	memcpy(outmark,&outsetup,sizeof(outsetup));

	if (useicmp)
		outicmp->icmp_seq = htons(seq);
	else
		outudp->uh_dport = htons(port + seq);

	if (useicmp) {
		/* Always calculate checksum for icmp packets */
		outicmp->icmp_cksum = 0;
		outicmp->icmp_cksum = in_cksum((u_short *)outicmp,
		    packlen - (sizeof(*outip) + optlen));
		if (outicmp->icmp_cksum == 0)
			outicmp->icmp_cksum = 0xffff;
	} else if (doipcksum) {
		/* Checksum (we must save and restore ip header) */
		tip = *outip;
		ui = (struct udpiphdr *)outip;
		oui = (struct udpiphdr *)&tip;
		/* Easier to zero and put back things that are ok */
		memset(ui, 0, sizeof(ui->ui_i));
		ui->ui_src = oui->ui_src;
		ui->ui_dst = oui->ui_dst;
		ui->ui_pr = oui->ui_pr;
		ui->ui_len = outudp->uh_ulen;
		outudp->uh_sum = 0;
		outudp->uh_sum = in_cksum((u_short *)ui, packlen);
		if (outudp->uh_sum == 0)
			outudp->uh_sum = 0xffff;
		*outip = tip;
	}

	/* XXX undocumented debugging hack */
	if (verbose > 1) {
		const u_int16_t *sp;
		int nshorts, i;

		sp = (u_int16_t *)outip;
		nshorts = (u_int)packlen / sizeof(u_int16_t);
		i = 0;
		Printf("[ %d bytes", packlen);
		while (--nshorts >= 0) {
			if ((i++ % 8) == 0)
				Printf("\n\t");
			Printf(" %04x", ntohs(*sp++));
		}
		if (packlen & 1) {
			if ((i % 8) == 0)
				Printf("\n\t");
			Printf(" %02x", *(const u_char *)sp);
		}
		Printf("]\n");
	}

#if !defined(IP_HDRINCL) && defined(IP_TTL)
	if (prog_setsockopt(sndsock, IPPROTO_IP, IP_TTL,
	    (char *)&ttl, sizeof(ttl)) < 0)
		err(1, "setsockopt ttl %d", ttl);
#endif
	if (dump)
		dump_packet();

#ifdef __hpux
	cc = sendto(sndsock, useicmp ? (char *)outicmp : (char *)outudp,
	    packlen - (sizeof(*outip) + optlen), 0, &whereto, sizeof(whereto));
	if (cc > 0)
		cc += sizeof(*outip) + optlen;
#else
	cc = prog_sendto(sndsock, (char *)outip,
	    packlen, 0, &whereto, sizeof(whereto));
#endif
	if (cc < 0 || cc != packlen)  {
		if (cc < 0) {
			/*
			 * An errno of EMSGSIZE means we're writing too big a
			 * datagram for the interface.  We have to just
			 * decrease the packet size until we find one that
			 * works.
			 *
			 * XXX maybe we should try to read the outgoing if's 
			 * mtu?
			 */
			if (errno == EMSGSIZE) {
				packlen = *mtuptr++;
				resize_packet();
				goto again;
			} else
				warn("sendto");
		}
		
		Printf("%s: wrote %s %d chars, ret=%d\n",
		    prog, hostname, packlen, cc);
		(void)fflush(stdout);
	}
	if (oldmtu != packlen) {
		Printf("message too big, "
		    "trying new MTU = %d\n", packlen);
		printed_ttl = 0;
	}
	if (!printed_ttl) {
		Printf("%2d ", ttl);
		printed_ttl = 1;
	}
	
}

static double
deltaT(struct timeval *t1p, struct timeval *t2p)
{
	double dt;

	dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
	     (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
	return dt;
}

/*
 * Convert an ICMP "type" field to a printable string.
 */
static const char *
pr_type(u_char t)
{
	static const char *ttab[] = {
	"Echo Reply",	"ICMP 1",	"ICMP 2",	"Dest Unreachable",
	"Source Quench", "Redirect",	"ICMP 6",	"ICMP 7",
	"Echo",		"ICMP 9",	"ICMP 10",	"Time Exceeded",
	"Param Problem", "Timestamp",	"Timestamp Reply", "Info Request",
	"Info Reply"
	};

	if (t > 16)
		return "OUT-OF-RANGE";

	return ttab[t];
}

static int
packet_ok(u_char *buf, ssize_t cc, struct sockaddr_in *from, int seq)
{
	struct icmp *icp;
	u_char type, code;
	int hlen;
#ifndef ARCHAIC
	struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			Printf("packet too short (%zd bytes) from %s\n", cc,
				inet_ntoa(from->sin_addr));
		return 0;
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
#else
	icp = (struct icmp *)buf;
#endif
	type = icp->icmp_type;
	code = icp->icmp_code;
	/* Path MTU Discovery (RFC1191) */
	if (code != ICMP_UNREACH_NEEDFRAG)
		pmtu = 0;
	else {
#ifdef HAVE_ICMP_NEXTMTU
		pmtu = ntohs(icp->icmp_nextmtu);
#else
		pmtu = ntohs(((struct my_pmtu *)&icp->icmp_void)->ipm_nextmtu);
#endif
	}
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH || type == ICMP_ECHOREPLY) {
		struct ip *hip;
		struct udphdr *up;
		struct icmp *hicmp;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;

		nextmtu = ntohs(icp->icmp_nextmtu);	/* for frag_err() */
			
		if (useicmp) {
			/* XXX */
			if (type == ICMP_ECHOREPLY &&
			    icp->icmp_id == htons(ident) &&
			    icp->icmp_seq == htons(seq))
				return -2;

			hicmp = (struct icmp *)((u_char *)hip + hlen);
			/* XXX 8 is a magic number */
			if (hlen + 8 <= cc &&
			    hip->ip_p == IPPROTO_ICMP &&
			    hicmp->icmp_id == htons(ident) &&
			    hicmp->icmp_seq == htons(seq))
				return type == ICMP_TIMXCEED ? -1 : code + 1;
		} else {
			up = (struct udphdr *)((u_char *)hip + hlen);
			/* XXX 8 is a magic number */
			if (hlen + 12 <= cc &&
			    hip->ip_p == IPPROTO_UDP &&
			    up->uh_sport == htons(ident) &&
			    up->uh_dport == htons(port + seq))
				return type == ICMP_TIMXCEED ? -1 : code + 1;
		}
	}
#ifndef ARCHAIC
	if (verbose) {
		int i;
		u_int32_t *lp = (u_int32_t *)&icp->icmp_ip;

		Printf("\n%zd bytes from %s to ", cc, inet_ntoa(from->sin_addr));
		Printf("%s: icmp type %d (%s) code %d\n",
		    inet_ntoa(ip->ip_dst), type, pr_type(type), icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(*lp))
			Printf("%2d: x%8.8x\n", i, *lp++);
	}
#endif
	return(0);
}

static void
resize_packet(void)
{
	if (useicmp) {
		outicmp->icmp_cksum = 0;
		outicmp->icmp_cksum = in_cksum((u_int16_t *)outicmp,
		    packlen - (sizeof(*outip) + optlen));
		if (outicmp->icmp_cksum == 0)
			outicmp->icmp_cksum = 0xffff;
	} else {
		outudp->uh_ulen =
		    htons((u_int16_t)(packlen - (sizeof(*outip) + optlen)));
	}
}

static void
print(u_char *buf, int cc, struct sockaddr_in *from)
{
	struct ip *ip;
	int hlen;
	char addr[INET_ADDRSTRLEN];

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	cc -= hlen;

	strlcpy(addr, inet_ntoa(from->sin_addr), sizeof(addr));

	if (as_path)
		Printf(" [AS%u]", as_lookup(asn, addr, AF_INET));

	if (nflag)
		Printf(" %s", addr);
	else
		Printf(" %s (%s)", inetname(from->sin_addr), addr);

	if (verbose)
		Printf(" %d bytes to %s", cc, inet_ntoa (ip->ip_dst));
}

static u_int16_t
in_cksum(u_int16_t *addr, int len)
{

	return ~in_cksum2(0, addr, len);
}

/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
static u_int16_t
in_cksum2(u_int16_t seed, u_int16_t *addr, int len)
{
	int nleft = len;
	u_int16_t *w = addr;
	union {
		u_int16_t w;
		u_int8_t b[2];
	} answer;
	int32_t sum = seed;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		answer.b[0] = *(u_char *)w;
		answer.b[1] = 0;
		sum += answer.w;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer.w = sum;				/* truncate to 16 bits */
	return answer.w;
}

/*
 * Subtract 2 timeval structs:  out = out - in.
 * Out is assumed to be >= in.
 */
static void
tvsub(struct timeval *out, struct timeval *in)
{

	if ((out->tv_usec -= in->tv_usec) < 0)   {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
static char *
inetname(struct in_addr in)
{
	char *cp;
	struct hostent *hp;
	static int first = 1;
	static char domain[MAXHOSTNAMELEN + 1], line[MAXHOSTNAMELEN + 1];

	if (first && !nflag) {

		first = 0;
		if (gethostname(domain, sizeof(domain) - 1) < 0)
 			domain[0] = '\0';
		else {
			cp = strchr(domain, '.');
			if (cp == NULL) {
				hp = gethostbyname(domain);
				if (hp != NULL)
					cp = strchr(hp->h_name, '.');
			}
			if (cp == NULL)
				domain[0] = '\0';
			else {
				++cp;
				(void)strlcpy(domain, cp, sizeof(domain));
			}
		}
	}
	if (!nflag && in.s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)&in, sizeof(in), AF_INET);
		if (hp != NULL) {
			if ((cp = strchr(hp->h_name, '.')) != NULL &&
			    strcmp(cp + 1, domain) == 0)
				*cp = '\0';
			(void)strlcpy(line, hp->h_name, sizeof(line));
			return line;
		}
	}
	return inet_ntoa(in);
}

static struct hostinfo *
gethostinfo(char *hname)
{
	int n;
	struct hostent *hp;
	struct hostinfo *hi;
	char **p;
	u_int32_t *ap;
	struct in_addr addr;

	hi = calloc(1, sizeof(*hi));
	if (hi == NULL)
		err(1, "calloc");
	if (inet_aton(hname, &addr) != 0) {
		hi->name = strdup(hname);
		if (!hi->name)
			err(1, "strdup");
		hi->n = 1;
		hi->addrs = calloc(1, sizeof(hi->addrs[0]));
		if (hi->addrs == NULL)
			err(1, "calloc");
		hi->addrs[0] = addr.s_addr;
		return hi;
	}

	hp = gethostbyname(hname);
	if (hp == NULL)
		errx(1, "unknown host %s", hname);
	if (hp->h_addrtype != AF_INET || hp->h_length != 4)
		errx(1, "bad host %s", hname);
	hi->name = strdup(hp->h_name);
	if (!hi->name)
		err(1, "strdup");
	for (n = 0, p = hp->h_addr_list; *p != NULL; ++n, ++p)
		continue;
	hi->n = n;
	hi->addrs = calloc(n, sizeof(hi->addrs[0]));
	if (hi->addrs == NULL)
		err(1, "calloc");
	for (ap = hi->addrs, p = hp->h_addr_list; *p != NULL; ++ap, ++p)
		memcpy(ap, *p, sizeof(*ap));
	return hi;
}

static void
freehostinfo(struct hostinfo *hi)
{
	if (hi->name != NULL) {
		free(hi->name);
		hi->name = NULL;
	}
	free(hi->addrs);
	free(hi);
}

static void
getaddr(u_int32_t *ap, char *hname)
{
	struct hostinfo *hi;

	hi = gethostinfo(hname);
	*ap = hi->addrs[0];
	freehostinfo(hi);
}

static void
setsin(struct sockaddr_in *sin, u_int32_t addr)
{

	memset(sin, 0, sizeof(*sin));
#ifdef HAVE_SOCKADDR_SA_LEN
	sin->sin_len = sizeof(*sin);
#endif
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = addr;
}

/* String to value with optional min and max. Handles decimal and hex. */
static int
str2val(const char *str, const char *what, int mi, int ma)
{
	const char *cp;
	long val;
	char *ep;

	errno = 0;
	ep = NULL;
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		cp = str + 2;
		val = strtol(cp, &ep, 16);
	} else
		val = strtol(str, &ep, 10);
	if (errno || str[0] == '\0' || *ep != '\0')
		errx(1, "\"%s\" bad value for %s", str, what);
	if (val < mi && mi >= 0) {
		if (mi == 0)
			errx(1, "%s must be >= %d", what, mi);
		else
			errx(1, "%s must be > %d", what, mi - 1);
	}
	if (val > ma && ma >= 0)
		errx(1, "%s must be <= %d", what, ma);
	return (int)val;
}

__dead void
usage(void)
{
	extern char version[];

	Fprintf(stderr, "Version %s\n", version);
	Fprintf(stderr, "Usage: %s [-adDFPIlMnrvx] [-g gateway] [-i iface] \
[-f first_ttl]\n\t[-m max_ttl] [-p port] [-q nqueries] [-s src_addr] [-t tos]\n\t\
[-w waittime] [-z pausemsecs] [-A as_server] host [packetlen]\n",
	    getprogname());
	exit(1);
}

/*
 * Received ICMP unreachable (fragmentation required and DF set).
 * If the ICMP error was from a "new" router, it'll contain the next-hop
 * MTU that we should use next.  Otherwise we'll just keep going in the
 * mtus[] table, trying until we hit a valid MTU.
 */


void
frag_err()
{
        int i;

        if (nextmtu > 0 && nextmtu < packlen) {
                Printf("\nfragmentation required and DF set, "
		     "next hop MTU = %d\n",
                        nextmtu);
                packlen = nextmtu;
                for (i = 0; mtus[i] > 0; i++) {
                        if (mtus[i] < nextmtu) {
                                mtuptr = &mtus[i];    /* next one to try */
                                break;
                        }
                }
        } else {
                Printf("\nfragmentation required and DF set. ");
		if (nextmtu)
			Printf("\nBogus next hop MTU = %d > last MTU = %d. ",
			    nextmtu, packlen);
                packlen = *mtuptr++;
		Printf("Trying new MTU = %d\n", packlen);
        }
	resize_packet();
}

int
find_local_ip(struct sockaddr_in *from, struct sockaddr_in *to)
{
	int sock;
	struct sockaddr_in help;
	socklen_t help_len;

	sock = prog_socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return 0;

	help.sin_family = AF_INET;
	/*
	 * At this point the port number doesn't matter
	 * since it only has to be greater than zero.
	 */
	help.sin_port = 42;
	help.sin_addr.s_addr = to->sin_addr.s_addr;
	if (prog_connect(sock, (struct sockaddr *)&help, sizeof(help)) < 0) {
		(void)prog_close(sock);
		return 0;
	}

	help_len = sizeof(help);
	if (prog_getsockname(sock, (struct sockaddr *)&help, &help_len) < 0 ||
	    help_len != sizeof(help) ||
	    help.sin_addr.s_addr == INADDR_ANY) {
		(void)prog_close(sock);
		return 0;
	}

	(void)prog_close(sock);
	setsin(from, help.sin_addr.s_addr);
	return 1;
}

#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
static int
setpolicy(int so, const char *policy)
{
	char *buf;

	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL) {
		warnx("%s", ipsec_strerror());
		return -1;
	}
	(void)prog_setsockopt(so, IPPROTO_IP, IP_IPSEC_POLICY,
		buf, ipsec_get_policylen(buf));

	free(buf);

	return 0;
}
#endif
#endif

