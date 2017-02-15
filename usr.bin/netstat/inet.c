/*	$NetBSD: inet.c,v 1.106 2015/02/08 15:09:45 christos Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
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
#if 0
static char sccsid[] = "from: @(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
__RCSID("$NetBSD: inet.c,v 1.106 2015/02/08 15:09:45 christos Exp $");
#endif
#endif /* not lint */

#define	_CALLOUT_PRIVATE	/* for defs in sys/callout.h */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/if_arp.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#define ICMP_STRINGS
#include <netinet/ip_icmp.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/pim_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#define	TCPTIMERS
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/ip_carp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp_vtw.h>

#include <arpa/inet.h>
#include <kvm.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <util.h>
#include "netstat.h"
#include "vtw.h"
#include "prog_ops.h"

char	*inetname(struct in_addr *);
void	inetprint(struct in_addr *, u_int16_t, const char *, int);

void	print_vtw_v4(const vtw_t *);

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
static int width;
static int compact;

/* VTW-related variables. */
static struct timeval now;

static void
protoprhdr(void)
{
	printf("Active Internet connections");
	if (aflag)
		printf(" (including servers)");
	putchar('\n');
	if (Aflag)
		printf("%-8.8s ", "PCB");
	printf(
	    Vflag ? "%-5.5s %-6.6s %-6.6s %s%-*.*s %-*.*s %-13.13s Expires\n"
	          : "%-5.5s %-6.6s %-6.6s %s%-*.*s %-*.*s %s\n",
		"Proto", "Recv-Q", "Send-Q", compact ? "" : " ",
		width, width, "Local Address",
		width, width, "Foreign Address",
		"State");
}

static void
protopr0(intptr_t ppcb, u_long rcv_sb_cc, u_long snd_sb_cc,
	 struct in_addr *laddr, u_int16_t lport,
	 struct in_addr *faddr, u_int16_t fport,
	 short t_state, const char *name, int inp_flags,
	 const struct timeval *expires)
{
	static const char *shorttcpstates[] = {
		"CLOSED",	"LISTEN",	"SYNSEN",	"SYSRCV",
		"ESTABL",	"CLWAIT",	"FWAIT1",	"CLOSNG",
		"LASTAK",	"FWAIT2",	"TMWAIT",
	};
	int istcp;

	istcp = strcmp(name, "tcp") == 0;

	if (Aflag) {
		printf("%8" PRIxPTR " ", ppcb);
	}
	printf("%-5.5s %6ld %6ld%s", name, rcv_sb_cc, snd_sb_cc,
	       compact ? "" : " ");
	if (numeric_port) {
		inetprint(laddr, lport, name, 1);
		inetprint(faddr, fport, name, 1);
	} else if (inp_flags & INP_ANONPORT) {
		inetprint(laddr, lport, name, 1);
		inetprint(faddr, fport, name, 0);
	} else {
		inetprint(laddr, lport, name, 0);
		inetprint(faddr, fport, name, 0);
	}
	if (istcp) {
		if (t_state < 0 || t_state >= TCP_NSTATES)
			printf(" %d", t_state);
		else
			printf(" %s", compact ? shorttcpstates[t_state] :
			       tcpstates[t_state]);
	}
	if (Vflag && expires != NULL) {
		if (expires->tv_sec == 0 && expires->tv_usec == -1)
			printf(" reclaimed");
		else {
			struct timeval delta;

			timersub(expires, &now, &delta);
			printf(" %.3fms",
			    delta.tv_sec * 1000.0 + delta.tv_usec / 1000.0);
		}
	}
	putchar('\n');
}

static void
dbg_printf(const char *fmt, ...)
{
	return;
}

void 
print_vtw_v4(const vtw_t *vtw)
{
	const vtw_v4_t *v4 = (const vtw_v4_t *)vtw;
	struct timeval delta;
	struct in_addr la, fa;
	char buf[2][32];
	static const struct timeval zero = {.tv_sec = 0, .tv_usec = 0};

	la.s_addr = v4->laddr;
	fa.s_addr = v4->faddr;

	snprintf(&buf[0][0], 32, "%s", inet_ntoa(la));
	snprintf(&buf[1][0], 32, "%s", inet_ntoa(fa));

	timersub(&vtw->expire, &now, &delta);

	if (vtw->expire.tv_sec == 0 && vtw->expire.tv_usec == -1) {
		dbg_printf("%15.15s:%d %15.15s:%d reclaimed\n"
		    ,buf[0], ntohs(v4->lport)
		    ,buf[1], ntohs(v4->fport));
		if (!(Vflag && vflag))
			return;
	} else if (vtw->expire.tv_sec == 0)
		return;
	else if (timercmp(&delta, &zero, <) && !(Vflag && vflag)) {
		dbg_printf("%15.15s:%d %15.15s:%d expired\n"
		    ,buf[0], ntohs(v4->lport)
		    ,buf[1], ntohs(v4->fport));
		return;
	} else {
		dbg_printf("%15.15s:%d %15.15s:%d expires in %.3fms\n"
		    ,buf[0], ntohs(v4->lport)
		    ,buf[1], ntohs(v4->fport)
		    ,delta.tv_sec * 1000.0 + delta.tv_usec / 1000.0);
	}
	protopr0(0, 0, 0,
		 &la, v4->lport,
		 &fa, v4->fport,
		 TCPS_TIME_WAIT, "tcp", 0, &vtw->expire);
}

struct kinfo_pcb *
getpcblist_sysctl(const char *name, size_t *len) {
	int mib[8];
	size_t namelen = 0, size = 0;
	char *mibname = NULL;
	struct kinfo_pcb *pcblist;

	memset(mib, 0, sizeof(mib));

	if (asprintf(&mibname, "net.inet%s.%s.pcblist", name + 3, name) == -1)
		err(1, "asprintf");

	/* get dynamic pcblist node */
	if (sysctlnametomib(mibname, mib, &namelen) == -1)
		err(1, "sysctlnametomib: %s", mibname);

	free(mibname);

	if (prog_sysctl(mib, __arraycount(mib), NULL, &size, NULL, 0) == -1)
		err(1, "sysctl (query)");

	if ((pcblist = malloc(size)) == NULL)
		err(1, "malloc");
	memset(pcblist, 0, size);

	mib[6] = sizeof(*pcblist);
	mib[7] = size / sizeof(*pcblist);

	if (prog_sysctl(mib, __arraycount(mib), pcblist, &size, NULL, 0) == -1)
		err(1, "sysctl (copy)");

	*len = size / sizeof(*pcblist);
	return pcblist;

}

static struct kinfo_pcb *
getpcblist_kmem(u_long off, const char *name, size_t *len) {
	struct inpcbtable table;
	struct inpcb_hdr *next, *prev;
	struct inpcb inpcb;
	struct tcpcb tcpcb;
	struct socket sockb;
	int istcp = strcmp(name, "tcp") == 0;
	struct kinfo_pcb *pcblist;
	size_t size = 100, i;
	struct sockaddr_in sin; 
	struct inpcbqueue *head;

	if (off == 0) {
		*len = 0;
		return NULL;
	}

	kread(off, (char *)&table, sizeof table);
	head = &table.inpt_queue;
	next = TAILQ_FIRST(head);
	prev = TAILQ_END(head);

	if ((pcblist = malloc(size * sizeof(*pcblist))) == NULL)
		err(1, "malloc");

	i = 0;
	while (next != TAILQ_END(head)) {
		kread((u_long)next, (char *)&inpcb, sizeof inpcb);
		prev = next;
		next = TAILQ_NEXT(&inpcb, inp_queue);

		if (inpcb.inp_af != AF_INET)
			continue;

		kread((u_long)inpcb.inp_socket, (char *)&sockb, sizeof(sockb));
		if (istcp) {
			kread((u_long)inpcb.inp_ppcb,
			    (char *)&tcpcb, sizeof (tcpcb));
		}
		pcblist[i].ki_ppcbaddr =
		    istcp ? (uintptr_t) inpcb.inp_ppcb : (uintptr_t) prev;
		pcblist[i].ki_rcvq = (uint64_t)sockb.so_rcv.sb_cc;
		pcblist[i].ki_sndq = (uint64_t)sockb.so_snd.sb_cc;

		sin.sin_addr = inpcb.inp_laddr;
		sin.sin_port = inpcb.inp_lport;
		memcpy(&pcblist[i].ki_s, &sin, sizeof(sin));
		sin.sin_addr = inpcb.inp_faddr;
		sin.sin_port = inpcb.inp_fport;
		memcpy(&pcblist[i].ki_d, &sin, sizeof(sin));
		pcblist[i].ki_tstate = tcpcb.t_state;
		pcblist[i].ki_pflags = inpcb.inp_flags;
		if (i++ == size) {
			size += 100;
			struct kinfo_pcb *n = realloc(pcblist,
			    size * sizeof(*pcblist));
			if (n == NULL)
				err(1, "realloc");
			pcblist = n;
		}
	}
	*len = i;
	return pcblist;
}

void
protopr(u_long off, const char *name)
{
	static int first = 1;
	struct kinfo_pcb *pcblist;
	size_t i, len;

	compact = 0;
	if (Aflag) {
		if (!numeric_addr)
			width = 18;
		else {
			width = 21;
			compact = 1;
		}
	} else
		width = 22;

	if (use_sysctl)
		pcblist = getpcblist_sysctl(name, &len);
	else
		pcblist = getpcblist_kmem(off, name, &len);	

	for (i = 0; i < len; i++) {
		struct sockaddr_in src, dst;

		memcpy(&src, &pcblist[i].ki_s, sizeof(src));
		memcpy(&dst, &pcblist[i].ki_d, sizeof(dst));

		if (!aflag &&
		    inet_lnaof(dst.sin_addr) == INADDR_ANY)
			continue;

		if (first) {
			protoprhdr();
			first = 0;
		}
		protopr0((intptr_t) pcblist[i].ki_ppcbaddr,
			 pcblist[i].ki_rcvq, pcblist[i].ki_sndq,
			 &src.sin_addr, src.sin_port,
			 &dst.sin_addr, dst.sin_port,
			 pcblist[i].ki_tstate, name,
			 pcblist[i].ki_pflags, NULL);
	}

	free(pcblist);

#ifndef __minix
	if (strcmp(name, "tcp") == 0) {
		struct timeval t;
		timebase(&t);
		gettimeofday(&now, NULL);
		timersub(&now, &t, &now);
		show_vtw_v4(print_vtw_v4);
	}
#endif /* !__minix */
}

/*
 * Dump TCP statistics structure.
 */
void
tcp_stats(u_long off, const char *name)
{
	uint64_t tcpstat[TCP_NSTATS];

	if (use_sysctl) {
		size_t size = sizeof(tcpstat);

		if (sysctlbyname("net.inet.tcp.stats", tcpstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf ("%s:\n", name);

#define	ps(f, m) if (tcpstat[f] || sflag <= 1) \
    printf(m, tcpstat[f])
#define	p(f, m) if (tcpstat[f] || sflag <= 1) \
    printf(m, tcpstat[f], plural(tcpstat[f]))
#define	p2(f1, f2, m) if (tcpstat[f1] || tcpstat[f2] || sflag <= 1) \
    printf(m, tcpstat[f1], plural(tcpstat[f1]), \
    tcpstat[f2], plural(tcpstat[f2]))
#define	p2s(f1, f2, m) if (tcpstat[f1] || tcpstat[f2] || sflag <= 1) \
    printf(m, tcpstat[f1], plural(tcpstat[f1]), \
    tcpstat[f2])
#define	p3(f, m) if (tcpstat[f] || sflag <= 1) \
    printf(m, tcpstat[f], plurales(tcpstat[f]))

	p(TCP_STAT_SNDTOTAL, "\t%" PRIu64 " packet%s sent\n");
	p2(TCP_STAT_SNDPACK,TCP_STAT_SNDBYTE,
		"\t\t%" PRIu64 " data packet%s (%" PRIu64 " byte%s)\n");
	p2(TCP_STAT_SNDREXMITPACK, TCP_STAT_SNDREXMITBYTE,
		"\t\t%" PRIu64 " data packet%s (%" PRIu64 " byte%s) retransmitted\n");
	p2s(TCP_STAT_SNDACKS, TCP_STAT_DELACK,
		"\t\t%" PRIu64 " ack-only packet%s (%" PRIu64 " delayed)\n");
	p(TCP_STAT_SNDURG, "\t\t%" PRIu64 " URG only packet%s\n");
	p(TCP_STAT_SNDPROBE, "\t\t%" PRIu64 " window probe packet%s\n");
	p(TCP_STAT_SNDWINUP, "\t\t%" PRIu64 " window update packet%s\n");
	p(TCP_STAT_SNDCTRL, "\t\t%" PRIu64 " control packet%s\n");
	p(TCP_STAT_SELFQUENCH,
	    "\t\t%" PRIu64 " send attempt%s resulted in self-quench\n");
	p(TCP_STAT_RCVTOTAL, "\t%" PRIu64 " packet%s received\n");
	p2(TCP_STAT_RCVACKPACK, TCP_STAT_RCVACKBYTE,
		"\t\t%" PRIu64 " ack%s (for %" PRIu64 " byte%s)\n");
	p(TCP_STAT_RCVDUPACK, "\t\t%" PRIu64 " duplicate ack%s\n");
	p(TCP_STAT_RCVACKTOOMUCH, "\t\t%" PRIu64 " ack%s for unsent data\n");
	p2(TCP_STAT_RCVPACK, TCP_STAT_RCVBYTE,
		"\t\t%" PRIu64 " packet%s (%" PRIu64 " byte%s) received in-sequence\n");
	p2(TCP_STAT_RCVDUPPACK, TCP_STAT_RCVDUPBYTE,
		"\t\t%" PRIu64 " completely duplicate packet%s (%" PRIu64 " byte%s)\n");
	p(TCP_STAT_PAWSDROP, "\t\t%" PRIu64 " old duplicate packet%s\n");
	p2(TCP_STAT_RCVPARTDUPPACK, TCP_STAT_RCVPARTDUPBYTE,
		"\t\t%" PRIu64 " packet%s with some dup. data (%" PRIu64 " byte%s duped)\n");
	p2(TCP_STAT_RCVOOPACK, TCP_STAT_RCVOOBYTE,
		"\t\t%" PRIu64 " out-of-order packet%s (%" PRIu64 " byte%s)\n");
	p2(TCP_STAT_RCVPACKAFTERWIN, TCP_STAT_RCVBYTEAFTERWIN,
		"\t\t%" PRIu64 " packet%s (%" PRIu64 " byte%s) of data after window\n");
	p(TCP_STAT_RCVWINPROBE, "\t\t%" PRIu64 " window probe%s\n");
	p(TCP_STAT_RCVWINUPD, "\t\t%" PRIu64 " window update packet%s\n");
	p(TCP_STAT_RCVAFTERCLOSE, "\t\t%" PRIu64 " packet%s received after close\n");
	p(TCP_STAT_RCVBADSUM, "\t\t%" PRIu64 " discarded for bad checksum%s\n");
	p(TCP_STAT_RCVBADOFF, "\t\t%" PRIu64 " discarded for bad header offset field%s\n");
	ps(TCP_STAT_RCVSHORT, "\t\t%" PRIu64 " discarded because packet too short\n");
	p(TCP_STAT_CONNATTEMPT, "\t%" PRIu64 " connection request%s\n");
	p(TCP_STAT_ACCEPTS, "\t%" PRIu64 " connection accept%s\n");
	p(TCP_STAT_CONNECTS,
		"\t%" PRIu64 " connection%s established (including accepts)\n");
	p2(TCP_STAT_CLOSED, TCP_STAT_DROPS,
		"\t%" PRIu64 " connection%s closed (including %" PRIu64 " drop%s)\n");
	p(TCP_STAT_CONNDROPS, "\t%" PRIu64 " embryonic connection%s dropped\n");
	p(TCP_STAT_DELAYED_FREE, "\t%" PRIu64 " delayed free%s of tcpcb\n");
	p2(TCP_STAT_RTTUPDATED, TCP_STAT_SEGSTIMED,
		"\t%" PRIu64 " segment%s updated rtt (of %" PRIu64 " attempt%s)\n");
	p(TCP_STAT_REXMTTIMEO, "\t%" PRIu64 " retransmit timeout%s\n");
	p(TCP_STAT_TIMEOUTDROP,
		"\t\t%" PRIu64 " connection%s dropped by rexmit timeout\n");
	p2(TCP_STAT_PERSISTTIMEO, TCP_STAT_PERSISTDROPS,
	   "\t%" PRIu64 " persist timeout%s (resulting in %" PRIu64 " dropped "
		"connection%s)\n");
	p(TCP_STAT_KEEPTIMEO, "\t%" PRIu64 " keepalive timeout%s\n");
	p(TCP_STAT_KEEPPROBE, "\t\t%" PRIu64 " keepalive probe%s sent\n");
	p(TCP_STAT_KEEPDROPS, "\t\t%" PRIu64 " connection%s dropped by keepalive\n");
	p(TCP_STAT_PREDACK, "\t%" PRIu64 " correct ACK header prediction%s\n");
	p(TCP_STAT_PREDDAT, "\t%" PRIu64 " correct data packet header prediction%s\n");
	p3(TCP_STAT_PCBHASHMISS, "\t%" PRIu64 " PCB hash miss%s\n");
	ps(TCP_STAT_NOPORT, "\t%" PRIu64 " dropped due to no socket\n");
	p(TCP_STAT_CONNSDRAINED, "\t%" PRIu64 " connection%s drained due to memory "
		"shortage\n");
	p(TCP_STAT_PMTUBLACKHOLE, "\t%" PRIu64 " PMTUD blackhole%s detected\n");

	p(TCP_STAT_BADSYN, "\t%" PRIu64 " bad connection attempt%s\n");
	ps(TCP_STAT_SC_ADDED, "\t%" PRIu64 " SYN cache entries added\n");
	p(TCP_STAT_SC_COLLISIONS, "\t\t%" PRIu64 " hash collision%s\n");
	ps(TCP_STAT_SC_COMPLETED, "\t\t%" PRIu64 " completed\n");
	ps(TCP_STAT_SC_ABORTED, "\t\t%" PRIu64 " aborted (no space to build PCB)\n");
	ps(TCP_STAT_SC_TIMED_OUT, "\t\t%" PRIu64 " timed out\n");
	ps(TCP_STAT_SC_OVERFLOWED, "\t\t%" PRIu64 " dropped due to overflow\n");
	ps(TCP_STAT_SC_BUCKETOVERFLOW, "\t\t%" PRIu64 " dropped due to bucket overflow\n");
	ps(TCP_STAT_SC_RESET, "\t\t%" PRIu64 " dropped due to RST\n");
	ps(TCP_STAT_SC_UNREACH, "\t\t%" PRIu64 " dropped due to ICMP unreachable\n");
	ps(TCP_STAT_SC_DELAYED_FREE, "\t\t%" PRIu64 " delayed free of SYN cache "
		"entries\n");
	p(TCP_STAT_SC_RETRANSMITTED, "\t%" PRIu64 " SYN,ACK%s retransmitted\n");
	p(TCP_STAT_SC_DUPESYN, "\t%" PRIu64 " duplicate SYN%s received for entries "
		"already in the cache\n");
	p(TCP_STAT_SC_DROPPED, "\t%" PRIu64 " SYN%s dropped (no route or no space)\n");
	p(TCP_STAT_BADSIG, "\t%" PRIu64 " packet%s with bad signature\n");
	p(TCP_STAT_GOODSIG, "\t%" PRIu64 " packet%s with good signature\n");

	p(TCP_STAT_ECN_SHS, "\t%" PRIu64 " successful ECN handshake%s\n");
	p(TCP_STAT_ECN_CE, "\t%" PRIu64 " packet%s with ECN CE bit\n");
	p(TCP_STAT_ECN_ECT, "\t%" PRIu64 " packet%s ECN ECT(0) bit\n");
#undef p
#undef ps
#undef p2
#undef p2s
#undef p3
	show_vtw_stats();
}

/*
 * Dump UDP statistics structure.
 */
void
udp_stats(u_long off, const char *name)
{
	uint64_t udpstat[UDP_NSTATS];
	u_quad_t delivered;

	if (use_sysctl) {
		size_t size = sizeof(udpstat);

		if (sysctlbyname("net.inet.udp.stats", udpstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf ("%s:\n", name);

#define	ps(f, m) if (udpstat[f] || sflag <= 1) \
    printf(m, udpstat[f])
#define	p(f, m) if (udpstat[f] || sflag <= 1) \
    printf(m, udpstat[f], plural(udpstat[f]))
#define	p3(f, m) if (udpstat[f] || sflag <= 1) \
    printf(m, udpstat[f], plurales(udpstat[f]))

	p(UDP_STAT_IPACKETS, "\t%" PRIu64 " datagram%s received\n");
	ps(UDP_STAT_HDROPS, "\t%" PRIu64 " with incomplete header\n");
	ps(UDP_STAT_BADLEN, "\t%" PRIu64 " with bad data length field\n");
	ps(UDP_STAT_BADSUM, "\t%" PRIu64 " with bad checksum\n");
	ps(UDP_STAT_NOPORT, "\t%" PRIu64 " dropped due to no socket\n");
	p(UDP_STAT_NOPORTBCAST,
	  "\t%" PRIu64 " broadcast/multicast datagram%s dropped due to no socket\n");
	ps(UDP_STAT_FULLSOCK, "\t%" PRIu64 " dropped due to full socket buffers\n");
	delivered = udpstat[UDP_STAT_IPACKETS] -
		    udpstat[UDP_STAT_HDROPS] -
		    udpstat[UDP_STAT_BADLEN] -
		    udpstat[UDP_STAT_BADSUM] -
		    udpstat[UDP_STAT_NOPORT] -
		    udpstat[UDP_STAT_NOPORTBCAST] -
		    udpstat[UDP_STAT_FULLSOCK];
	if (delivered || sflag <= 1)
		printf("\t%" PRIu64 " delivered\n", delivered);
	p3(UDP_STAT_PCBHASHMISS, "\t%" PRIu64 " PCB hash miss%s\n");
	p(UDP_STAT_OPACKETS, "\t%" PRIu64 " datagram%s output\n");

#undef ps
#undef p
#undef p3
}

/*
 * Dump IP statistics structure.
 */
void
ip_stats(u_long off, const char *name)
{
	uint64_t ipstat[IP_NSTATS];

	if (use_sysctl) {
		size_t size = sizeof(ipstat);

		if (sysctlbyname("net.inet.ip.stats", ipstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf("%s:\n", name);

#define	ps(f, m) if (ipstat[f] || sflag <= 1) \
    printf(m, ipstat[f])
#define	p(f, m) if (ipstat[f] || sflag <= 1) \
    printf(m, ipstat[f], plural(ipstat[f]))

	p(IP_STAT_TOTAL, "\t%" PRIu64 " total packet%s received\n");
	p(IP_STAT_BADSUM, "\t%" PRIu64 " bad header checksum%s\n");
	ps(IP_STAT_TOOSMALL, "\t%" PRIu64 " with size smaller than minimum\n");
	ps(IP_STAT_TOOSHORT, "\t%" PRIu64 " with data size < data length\n");
	ps(IP_STAT_TOOLONG, "\t%" PRIu64 " with length > max ip packet size\n");
	ps(IP_STAT_BADHLEN, "\t%" PRIu64 " with header length < data size\n");
	ps(IP_STAT_BADLEN, "\t%" PRIu64 " with data length < header length\n");
	ps(IP_STAT_BADOPTIONS, "\t%" PRIu64 " with bad options\n");
	ps(IP_STAT_BADVERS, "\t%" PRIu64 " with incorrect version number\n");
	p(IP_STAT_FRAGMENTS, "\t%" PRIu64 " fragment%s received\n");
	p(IP_STAT_FRAGDROPPED, "\t%" PRIu64 " fragment%s dropped (dup or out of space)\n");
	p(IP_STAT_RCVMEMDROP, "\t%" PRIu64 " fragment%s dropped (out of ipqent)\n");
	p(IP_STAT_BADFRAGS, "\t%" PRIu64 " malformed fragment%s dropped\n");
	p(IP_STAT_FRAGTIMEOUT, "\t%" PRIu64 " fragment%s dropped after timeout\n");
	p(IP_STAT_REASSEMBLED, "\t%" PRIu64 " packet%s reassembled ok\n");
	p(IP_STAT_DELIVERED, "\t%" PRIu64 " packet%s for this host\n");
	p(IP_STAT_NOPROTO, "\t%" PRIu64 " packet%s for unknown/unsupported protocol\n");
	p(IP_STAT_FORWARD, "\t%" PRIu64 " packet%s forwarded");
	p(IP_STAT_FASTFORWARD, " (%" PRIu64 " packet%s fast forwarded)");
	if (ipstat[IP_STAT_FORWARD] || sflag <= 1)
		putchar('\n');
	p(IP_STAT_CANTFORWARD, "\t%" PRIu64 " packet%s not forwardable\n");
	p(IP_STAT_REDIRECTSENT, "\t%" PRIu64 " redirect%s sent\n");
	p(IP_STAT_NOGIF, "\t%" PRIu64 " packet%s no matching gif found\n");
	p(IP_STAT_LOCALOUT, "\t%" PRIu64 " packet%s sent from this host\n");
	p(IP_STAT_RAWOUT, "\t%" PRIu64 " packet%s sent with fabricated ip header\n");
	p(IP_STAT_ODROPPED, "\t%" PRIu64 " output packet%s dropped due to no bufs, etc.\n");
	p(IP_STAT_NOROUTE, "\t%" PRIu64 " output packet%s discarded due to no route\n");
	p(IP_STAT_FRAGMENTED, "\t%" PRIu64 " output datagram%s fragmented\n");
	p(IP_STAT_OFRAGMENTS, "\t%" PRIu64 " fragment%s created\n");
	p(IP_STAT_CANTFRAG, "\t%" PRIu64 " datagram%s that can't be fragmented\n");
	p(IP_STAT_BADADDR, "\t%" PRIu64 " datagram%s with bad address in header\n");
#undef ps
#undef p
}

/*
 * Dump ICMP statistics.
 */
void
icmp_stats(u_long off, const char *name)
{
	uint64_t icmpstat[ICMP_NSTATS];
	int i, first;

	if (use_sysctl) {
		size_t size = sizeof(icmpstat);

		if (sysctlbyname("net.inet.icmp.stats", icmpstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf("%s:\n", name);

#define	p(f, m) if (icmpstat[f] || sflag <= 1) \
    printf(m, icmpstat[f], plural(icmpstat[f]))

	p(ICMP_STAT_ERROR, "\t%" PRIu64 " call%s to icmp_error\n");
	p(ICMP_STAT_OLDICMP,
	    "\t%" PRIu64 " error%s not generated because old message was icmp\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat[ICMP_STAT_OUTHIST + i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %" PRIu64 "\n", icmp_type[i],
			   icmpstat[ICMP_STAT_OUTHIST + i]);
		}
	p(ICMP_STAT_BADCODE, "\t%" PRIu64 " message%s with bad code fields\n");
	p(ICMP_STAT_TOOSHORT, "\t%" PRIu64 " message%s < minimum length\n");
	p(ICMP_STAT_CHECKSUM, "\t%" PRIu64 " bad checksum%s\n");
	p(ICMP_STAT_BADLEN, "\t%" PRIu64 " message%s with bad length\n");
	p(ICMP_STAT_BMCASTECHO, "\t%" PRIu64 " multicast echo request%s ignored\n");
	p(ICMP_STAT_BMCASTTSTAMP, "\t%" PRIu64 " multicast timestamp request%s ignored\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat[ICMP_STAT_INHIST + i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %" PRIu64 "\n", icmp_type[i],
			    icmpstat[ICMP_STAT_INHIST + i]);
		}
	p(ICMP_STAT_REFLECT, "\t%" PRIu64 " message response%s generated\n");
	p(ICMP_STAT_PMTUCHG, "\t%" PRIu64 " path MTU change%s\n");
#undef p
}

/*
 * Dump IGMP statistics structure.
 */
void
igmp_stats(u_long off, const char *name)
{
	uint64_t igmpstat[IGMP_NSTATS];

	if (use_sysctl) {
		size_t size = sizeof(igmpstat);

		if (sysctlbyname("net.inet.igmp.stats", igmpstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf("%s:\n", name);

#define	p(f, m) if (igmpstat[f] || sflag <= 1) \
    printf(m, igmpstat[f], plural(igmpstat[f]))
#define	py(f, m) if (igmpstat[f] || sflag <= 1) \
    printf(m, igmpstat[f], igmpstat[f] != 1 ? "ies" : "y")
	p(IGMP_STAT_RCV_TOTAL, "\t%" PRIu64 " message%s received\n");
        p(IGMP_STAT_RCV_TOOSHORT, "\t%" PRIu64 " message%s received with too few bytes\n");
        p(IGMP_STAT_RCV_BADSUM, "\t%" PRIu64 " message%s received with bad checksum\n");
        py(IGMP_STAT_RCV_QUERIES, "\t%" PRIu64 " membership quer%s received\n");
        py(IGMP_STAT_RCV_BADQUERIES, "\t%" PRIu64 " membership quer%s received with invalid field(s)\n");
        p(IGMP_STAT_RCV_REPORTS, "\t%" PRIu64 " membership report%s received\n");
        p(IGMP_STAT_RCV_BADREPORTS, "\t%" PRIu64 " membership report%s received with invalid field(s)\n");
        p(IGMP_STAT_RCV_OURREPORTS, "\t%" PRIu64 " membership report%s received for groups to which we belong\n");
        p(IGMP_STAT_SND_REPORTS, "\t%" PRIu64 " membership report%s sent\n");
#undef p
#undef py
}

/*
 * Dump CARP statistics structure.
 */
void
carp_stats(u_long off, const char *name)
{
	uint64_t carpstat[CARP_NSTATS];

	if (use_sysctl) {
		size_t size = sizeof(carpstat);

		if (sysctlbyname("net.inet.carp.stats", carpstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf("%s:\n", name);

#define p(f, m) if (carpstat[f] || sflag <= 1) \
	printf(m, carpstat[f], plural(carpstat[f]))
#define p2(f, m) if (carpstat[f] || sflag <= 1) \
	printf(m, carpstat[f])

	p(CARP_STAT_IPACKETS, "\t%" PRIu64 " packet%s received (IPv4)\n");
	p(CARP_STAT_IPACKETS6, "\t%" PRIu64 " packet%s received (IPv6)\n");
	p(CARP_STAT_BADIF,
	    "\t\t%" PRIu64 " packet%s discarded for bad interface\n");
	p(CARP_STAT_BADTTL,
	    "\t\t%" PRIu64 " packet%s discarded for wrong TTL\n");
	p(CARP_STAT_HDROPS, "\t\t%" PRIu64 " packet%s shorter than header\n");
	p(CARP_STAT_BADSUM, "\t\t%" PRIu64
		" packet%s discarded for bad checksum\n");
	p(CARP_STAT_BADVER,
	    "\t\t%" PRIu64 " packet%s discarded with a bad version\n");
	p2(CARP_STAT_BADLEN,
	    "\t\t%" PRIu64 " discarded because packet was too short\n");
	p(CARP_STAT_BADAUTH,
	    "\t\t%" PRIu64 " packet%s discarded for bad authentication\n");
	p(CARP_STAT_BADVHID, "\t\t%" PRIu64 " packet%s discarded for bad vhid\n");
	p(CARP_STAT_BADADDRS, "\t\t%" PRIu64
		" packet%s discarded because of a bad address list\n");
	p(CARP_STAT_OPACKETS, "\t%" PRIu64 " packet%s sent (IPv4)\n");
	p(CARP_STAT_OPACKETS6, "\t%" PRIu64 " packet%s sent (IPv6)\n");
	p2(CARP_STAT_ONOMEM,
	    "\t\t%" PRIu64 " send failed due to mbuf memory error\n");
#undef p
#undef p2
}

/*
 * Dump PIM statistics structure.
 */
void
pim_stats(u_long off, const char *name)
{
	struct pimstat pimstat;

	if (off == 0)
		return;
	if (kread(off, (char *)&pimstat, sizeof (pimstat)) != 0) {
		/* XXX: PIM is probably not enabled in the kernel */
		return;
	}

	printf("%s:\n", name);

#define	p(f, m) if (pimstat.f || sflag <= 1) \
	printf(m, pimstat.f, plural(pimstat.f))

	p(pims_rcv_total_msgs, "\t%" PRIu64 " message%s received\n");
	p(pims_rcv_total_bytes, "\t%" PRIu64 " byte%s received\n");
	p(pims_rcv_tooshort, "\t%" PRIu64 " message%s received with too few bytes\n");
        p(pims_rcv_badsum, "\t%" PRIu64 " message%s received with bad checksum\n");
	p(pims_rcv_badversion, "\t%" PRIu64 " message%s received with bad version\n");
	p(pims_rcv_registers_msgs, "\t%" PRIu64 " data register message%s received\n");
	p(pims_rcv_registers_bytes, "\t%" PRIu64 " data register byte%s received\n");
	p(pims_rcv_registers_wrongiif, "\t%" PRIu64 " data register message%s received on wrong iif\n");
	p(pims_rcv_badregisters, "\t%" PRIu64 " bad register%s received\n");
	p(pims_snd_registers_msgs, "\t%" PRIu64 " data register message%s sent\n");
	p(pims_snd_registers_bytes, "\t%" PRIu64 " data register byte%s sent\n");
#undef p
}

/*
 * Dump the ARP statistics structure.
 */
void
arp_stats(u_long off, const char *name)
{
	uint64_t arpstat[ARP_NSTATS];

	if (use_sysctl) {
		size_t size = sizeof(arpstat);

		if (sysctlbyname("net.inet.arp.stats", arpstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf("%s:\n", name);

#define	ps(f, m) if (arpstat[f] || sflag <= 1) \
    printf(m, arpstat[f])
#define	p(f, m) if (arpstat[f] || sflag <= 1) \
    printf(m, arpstat[f], plural(arpstat[f]))

	p(ARP_STAT_SNDTOTAL, "\t%" PRIu64 " packet%s sent\n");
	p(ARP_STAT_SNDREPLY, "\t\t%" PRIu64 " reply packet%s\n");
	p(ARP_STAT_SENDREQUEST, "\t\t%" PRIu64 " request packet%s\n");

	p(ARP_STAT_RCVTOTAL, "\t%" PRIu64 " packet%s received\n");
	p(ARP_STAT_RCVREPLY, "\t\t%" PRIu64 " reply packet%s\n");
	p(ARP_STAT_RCVREQUEST, "\t\t%" PRIu64 " valid request packet%s\n");
	p(ARP_STAT_RCVMCAST, "\t\t%" PRIu64 " broadcast/multicast packet%s\n");
	p(ARP_STAT_RCVBADPROTO, "\t\t%" PRIu64 " packet%s with unknown protocol type\n");
	p(ARP_STAT_RCVBADLEN, "\t\t%" PRIu64 " packet%s with bad (short) length\n");
	p(ARP_STAT_RCVZEROTPA, "\t\t%" PRIu64 " packet%s with null target IP address\n");
	p(ARP_STAT_RCVZEROSPA, "\t\t%" PRIu64 " packet%s with null source IP address\n");
	ps(ARP_STAT_RCVNOINT, "\t\t%" PRIu64 " could not be mapped to an interface\n");
	p(ARP_STAT_RCVLOCALSHA, "\t\t%" PRIu64 " packet%s sourced from a local hardware "
	    "address\n");
	p(ARP_STAT_RCVBCASTSHA, "\t\t%" PRIu64 " packet%s with a broadcast "
	    "source hardware address\n");
	p(ARP_STAT_RCVLOCALSPA, "\t\t%" PRIu64 " duplicate%s for a local IP address\n");
	p(ARP_STAT_RCVOVERPERM, "\t\t%" PRIu64 " attempt%s to overwrite a static entry\n");
	p(ARP_STAT_RCVOVERINT, "\t\t%" PRIu64 " packet%s received on wrong interface\n");
	p(ARP_STAT_RCVOVER, "\t\t%" PRIu64 " entry%s overwritten\n");
	p(ARP_STAT_RCVLENCHG, "\t\t%" PRIu64 " change%s in hardware address length\n");

	p(ARP_STAT_DFRTOTAL, "\t%" PRIu64 " packet%s deferred pending ARP resolution\n");
	ps(ARP_STAT_DFRSENT, "\t\t%" PRIu64 " sent\n");
	ps(ARP_STAT_DFRDROPPED, "\t\t%" PRIu64 " dropped\n");

	p(ARP_STAT_ALLOCFAIL, "\t%" PRIu64 " failure%s to allocate llinfo\n");

#undef ps
#undef p
}

/*
 * Pretty print an Internet address (net address + port).
 * Take numeric_addr and numeric_port into consideration.
 */
void
inetprint(struct in_addr *in, uint16_t port, const char *proto,
	  int port_numeric)
{
	struct servent *sp = 0;
	char line[80], *cp;
	size_t space;

	(void)snprintf(line, sizeof line, "%.*s.",
	    (Aflag && !numeric_addr) ? 12 : 16, inetname(in));
	cp = strchr(line, '\0');
	if (!port_numeric && port)
		sp = getservbyport((int)port, proto);
	space = sizeof line - (cp-line);
	if (sp || port == 0)
		(void)snprintf(cp, space, "%s", sp ? sp->s_name : "*");
	else
		(void)snprintf(cp, space, "%u", ntohs(port));
	(void)printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If numeric_addr has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(struct in_addr *inp)
{
	char *cp;
	static char line[50];
	struct hostent *hp;
	struct netent *np;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !numeric_addr) {
		first = 0;
		if (gethostname(domain, sizeof domain) == 0) {
			domain[sizeof(domain) - 1] = '\0';
			if ((cp = strchr(domain, '.')))
				(void) strlcpy(domain, cp + 1, sizeof(domain));
			else
				domain[0] = 0;
		} else
			domain[0] = 0;
	}
	cp = 0;
	if (!numeric_addr && inp->s_addr != INADDR_ANY) {
		int net = inet_netof(*inp);
		int lna = inet_lnaof(*inp);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)inp, sizeof (*inp), AF_INET);
			if (hp) {
				if ((cp = strchr(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = 0;
				cp = hp->h_name;
			}
		}
	}
	if (inp->s_addr == INADDR_ANY)
		strlcpy(line, "*", sizeof line);
	else if (cp)
		strlcpy(line, cp, sizeof line);
	else {
		inp->s_addr = ntohl(inp->s_addr);
#define C(x)	((x) & 0xff)
		(void)snprintf(line, sizeof line, "%u.%u.%u.%u",
		    C(inp->s_addr >> 24), C(inp->s_addr >> 16),
		    C(inp->s_addr >> 8), C(inp->s_addr));
#undef C
	}
	return (line);
}

/*
 * Dump the contents of a TCP PCB.
 */
void
tcp_dump(u_long off, const char *name, u_long pcbaddr)
{
	callout_impl_t *ci;
	struct tcpcb tcpcb;
	int i, hardticks;
	struct kinfo_pcb *pcblist;
	size_t j, len;

	if (use_sysctl)
		pcblist = getpcblist_sysctl(name, &len);	
	else
		pcblist = getpcblist_kmem(off, name, &len);	

	for (j = 0; j < len; j++)
		if (pcblist[j].ki_ppcbaddr == pcbaddr)
			break;
	free(pcblist);

	if (j == len)
		errx(1, "0x%lx is not a valid pcb address", pcbaddr);

	kread(pcbaddr, (char *)&tcpcb, sizeof(tcpcb));
	hardticks = get_hardticks();

	printf("TCP Protocol Control Block at 0x%08lx:\n\n", pcbaddr);

	printf("Timers:\n");
	for (i = 0; i < TCPT_NTIMERS; i++) {
		char buf[128];
		ci = (callout_impl_t *)&tcpcb.t_timer[i];
		snprintb(buf, sizeof(buf), CALLOUT_FMT, ci->c_flags);
		printf("\t%s\t%s", tcptimers[i], buf);
		if (ci->c_flags & CALLOUT_PENDING)
			printf("\t%d\n", ci->c_time - hardticks);
		else
			printf("\n");
	}
	printf("\n\n");

	if (tcpcb.t_state < 0 || tcpcb.t_state >= TCP_NSTATES)
		printf("State: %d", tcpcb.t_state);
	else
		printf("State: %s", tcpstates[tcpcb.t_state]);
	printf(", flags 0x%x, inpcb 0x%lx, in6pcb 0x%lx\n\n", tcpcb.t_flags,
	    (u_long)tcpcb.t_inpcb, (u_long)tcpcb.t_in6pcb);

	printf("rxtshift %d, rxtcur %d, dupacks %d\n", tcpcb.t_rxtshift,
	    tcpcb.t_rxtcur, tcpcb.t_dupacks);
        printf("peermss %u, ourmss %u, segsz %u, segqlen %u\n\n",
	    tcpcb.t_peermss, tcpcb.t_ourmss, tcpcb.t_segsz, tcpcb.t_segqlen);

	printf("snd_una %u, snd_nxt %u, snd_up %u\n",
	    tcpcb.snd_una, tcpcb.snd_nxt, tcpcb.snd_up);
	printf("snd_wl1 %u, snd_wl2 %u, iss %u, snd_wnd %lu\n\n",
	    tcpcb.snd_wl1, tcpcb.snd_wl2, tcpcb.iss, tcpcb.snd_wnd);

	printf("rcv_wnd %lu, rcv_nxt %u, rcv_up %u, irs %u\n\n",
	    tcpcb.rcv_wnd, tcpcb.rcv_nxt, tcpcb.rcv_up, tcpcb.irs);

	printf("rcv_adv %u, snd_max %u, snd_cwnd %lu, snd_ssthresh %lu\n",
	    tcpcb.rcv_adv, tcpcb.snd_max, tcpcb.snd_cwnd, tcpcb.snd_ssthresh);

	printf("rcvtime %u, rtttime %u, rtseq %u, srtt %d, rttvar %d, "
	    "rttmin %d, max_sndwnd %lu\n\n", tcpcb.t_rcvtime, tcpcb.t_rtttime,
	    tcpcb.t_rtseq, tcpcb.t_srtt, tcpcb.t_rttvar, tcpcb.t_rttmin,
	    tcpcb.max_sndwnd);

	printf("oobflags %d, iobc %d, softerror %d\n\n", tcpcb.t_oobflags,
	    tcpcb.t_iobc, tcpcb.t_softerror);

	printf("snd_scale %d, rcv_scale %d, req_r_scale %d, req_s_scale %d\n",
	    tcpcb.snd_scale, tcpcb.rcv_scale, tcpcb.request_r_scale,
	    tcpcb.requested_s_scale);
	printf("ts_recent %u, ts_regent_age %d, last_ack_sent %u\n",
	    tcpcb.ts_recent, tcpcb.ts_recent_age, tcpcb.last_ack_sent);
}
