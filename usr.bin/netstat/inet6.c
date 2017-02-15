/*	$NetBSD: inet6.c,v 1.68 2015/02/08 15:09:45 christos Exp $	*/
/*	BSDI inet.c,v 2.3 1995/10/24 02:19:29 prb Exp	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
static char sccsid[] = "@(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
__RCSID("$NetBSD: inet6.c,v 1.68 2015/02/08 15:09:45 christos Exp $");
#endif
#endif /* not lint */

#define _CALLOUT_PRIVATE

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/in_systm.h>
#ifndef TCP6
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#endif
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_var.h>
#ifdef TCP6
#include <netinet/tcp6.h>
#include <netinet/tcp6_seq.h>
#define TCP6STATES
#include <netinet/tcp6_fsm.h>
#define TCP6TIMERS
#include <netinet/tcp6_timer.h>
#include <netinet/tcp6_var.h>
#include <netinet/tcp6_debug.h>
#else
#define TCP6T_NTIMERS	TCPT_NTIMERS
#define tcp6timers tcptimers
#define tcp6states tcpstates
#define TCP6_NSTATES	TCP_NSTATES
#define tcp6cb tcpcb
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_fsm.h>
extern const char * const tcpstates[];
extern const char * const tcptimers[];
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#endif /*TCP6*/
#include <netinet6/udp6.h>
#include <netinet6/udp6_var.h>
#include <netinet6/pim6_var.h>
#include <netinet6/raw_ip6.h>
#include <netinet/tcp_vtw.h>

#include <arpa/inet.h>
#if 0
#include "gethostbyname2.h"
#endif
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include "netstat.h"
#include "vtw.h"
#include "prog_ops.h"

#ifdef INET6

struct	in6pcb in6pcb;
#ifdef TCP6
struct	tcp6cb tcp6cb;
#else
struct	tcpcb tcpcb;
#endif
struct	socket sockb;

char	*inet6name(const struct in6_addr *);
void	inet6print(const struct in6_addr *, int, const char *);
void	print_vtw_v6(const vtw_t *);

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
ip6protoprhdr(void)
{
	
	printf("Active Internet6 connections");
	
	if (aflag)
		printf(" (including servers)");
	putchar('\n');
	
	if (Aflag) {
		printf("%-8.8s ", "PCB");
		width = 18;
	}
	printf(
	    Vflag ? "%-5.5s %-6.6s %-6.6s  %*.*s %*.*s %-13.13s Expires\n"
	          : "%-5.5s %-6.6s %-6.6s  %*.*s %*.*s %s\n",
	    "Proto", "Recv-Q", "Send-Q",
	    -width, width, "Local Address",
	    -width, width, "Foreign Address", "(state)");
}

static void
ip6protopr0(intptr_t ppcb, u_long rcv_sb_cc, u_long snd_sb_cc,
	const struct in6_addr *laddr, u_int16_t lport,
	const struct in6_addr *faddr, u_int16_t fport,
	short t_state, const char *name, const struct timeval *expires)
{
	static const char *shorttcpstates[] = {
		"CLOSED",       "LISTEN",       "SYNSEN",       "SYSRCV",
		"ESTABL",       "CLWAIT",       "FWAIT1",       "CLOSNG",
		"LASTAK",       "FWAIT2",       "TMWAIT",
	};
	int istcp;

	istcp = strcmp(name, "tcp6") == 0;
	if (Aflag)
		printf("%8" PRIxPTR " ", ppcb);

	printf("%-5.5s %6ld %6ld%s", name, rcv_sb_cc, snd_sb_cc,
	    compact ? "" : " ");

	inet6print(laddr, (int)lport, name);
	inet6print(faddr, (int)fport, name);
	if (istcp) {
#ifdef TCP6
		if (t_state < 0 || t_state >= TCP6_NSTATES)
			printf(" %d", t_state);
		else
			printf(" %s", tcp6states[t_state]);
#else
		if (t_state < 0 || t_state >= TCP_NSTATES)
			printf(" %d", t_state);
		else
			printf(" %s", compact ? shorttcpstates[t_state] :
			    tcpstates[t_state]);
#endif
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
print_vtw_v6(const vtw_t *vtw)
{
	const vtw_v6_t *v6 = (const vtw_v6_t *)vtw;
	struct timeval delta;
	char buf[2][128];
	static const struct timeval zero = {.tv_sec = 0, .tv_usec = 0};

	inet_ntop(AF_INET6, &v6->laddr, buf[0], sizeof(buf[0]));
	inet_ntop(AF_INET6, &v6->faddr, buf[1], sizeof(buf[1]));

	timersub(&vtw->expire, &now, &delta);

	if (vtw->expire.tv_sec == 0 && vtw->expire.tv_usec == -1) {
		dbg_printf("%15.15s:%d %15.15s:%d reclaimed\n"
		    ,buf[0], ntohs(v6->lport)
		    ,buf[1], ntohs(v6->fport));
		if (!(Vflag && vflag))
			return;
	} else if (vtw->expire.tv_sec == 0)
		return;
	else if (timercmp(&delta, &zero, <) && !(Vflag && vflag)) {
		dbg_printf("%15.15s:%d %15.15s:%d expired\n"
		    ,buf[0], ntohs(v6->lport)
		    ,buf[1], ntohs(v6->fport));
		return;
	} else {
		dbg_printf("%15.15s:%d %15.15s:%d expires in %.3fms\n"
		    ,buf[0], ntohs(v6->lport)
		    ,buf[1], ntohs(v6->fport)
		    ,delta.tv_sec * 1000.0 + delta.tv_usec / 1000.0);
	}
	ip6protopr0(0, 0, 0,
		 &v6->laddr, v6->lport,
		 &v6->faddr, v6->fport,
		 TCPS_TIME_WAIT, "tcp6", &vtw->expire);
}


static struct kinfo_pcb *
getpcblist_kmem(u_long off, const char *name, size_t *len) {

	struct inpcbtable table;
	struct inpcb_hdr *next, *prev;
	int istcp = strcmp(name, "tcp6") == 0;
	struct kinfo_pcb *pcblist;
	size_t size = 100, i;
	struct sockaddr_in6 sin6;
	struct inpcbqueue *head;

	if (off == 0) {
		*len = 0;
		return NULL;
	}
	kread(off, (char *)&table, sizeof (table));
	head = &table.inpt_queue;
	next = TAILQ_FIRST(head);
	prev = TAILQ_END(head);

	if ((pcblist = malloc(size * sizeof(*pcblist))) == NULL)
		err(1, "malloc");

	i = 0;
	while (next != TAILQ_END(head)) {
		kread((u_long)next, (char *)&in6pcb, sizeof in6pcb);
		next = TAILQ_NEXT(&in6pcb, in6p_queue);
		prev = next;

		if (in6pcb.in6p_af != AF_INET6)
			continue;

		kread((u_long)in6pcb.in6p_socket, (char *)&sockb, 
		    sizeof (sockb));
		if (istcp) {
#ifdef TCP6
			kread((u_long)in6pcb.in6p_ppcb,
			    (char *)&tcp6cb, sizeof (tcp6cb));
#else
			kread((u_long)in6pcb.in6p_ppcb,
			    (char *)&tcpcb, sizeof (tcpcb));
#endif
		}
		pcblist[i].ki_ppcbaddr = 
		    istcp ? (uintptr_t) in6pcb.in6p_ppcb : (uintptr_t) prev;
		pcblist[i].ki_rcvq = (uint64_t)sockb.so_rcv.sb_cc;
		pcblist[i].ki_sndq = (uint64_t)sockb.so_snd.sb_cc;
		sin6.sin6_addr = in6pcb.in6p_laddr;
		sin6.sin6_port = in6pcb.in6p_lport;
		memcpy(&pcblist[i].ki_s, &sin6, sizeof(sin6));
		sin6.sin6_addr = in6pcb.in6p_faddr;
		sin6.sin6_port = in6pcb.in6p_fport;
		memcpy(&pcblist[i].ki_d, &sin6, sizeof(sin6));
		pcblist[i].ki_tstate = tcpcb.t_state;
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
ip6protopr(u_long off, const char *name)
{
	struct kinfo_pcb *pcblist;
	size_t i, len;
	static int first = 1;

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
		struct sockaddr_in6 src, dst;

		memcpy(&src, &pcblist[i].ki_s, sizeof(src));
		memcpy(&dst, &pcblist[i].ki_d, sizeof(dst));

		if (!aflag && IN6_IS_ADDR_UNSPECIFIED(&dst.sin6_addr))
			continue;

		if (first) {
			ip6protoprhdr();
			first = 0;
		}

		ip6protopr0((intptr_t) pcblist[i].ki_ppcbaddr,
		    pcblist[i].ki_rcvq, pcblist[i].ki_sndq,
		    &src.sin6_addr, src.sin6_port,
		    &dst.sin6_addr, dst.sin6_port,
		    pcblist[i].ki_tstate, name, NULL);
	}

	free(pcblist);

#ifndef __minix
	if (strcmp(name, "tcp6") == 0) {
		struct timeval t;
		timebase(&t);
		gettimeofday(&now, NULL);
		timersub(&now, &t, &now);
		show_vtw_v6(print_vtw_v6);
	}
#endif /* !__minix */
}

#ifdef TCP6
/*
 * Dump TCP6 statistics structure.
 */
void
tcp6_stats(u_long off, const char *name)
{
	struct tcp6stat tcp6stat;

	if (use_sysctl) {
		size_t size = sizeof(tcp6stat);

		if (sysctlbyname("net.inet6.tcp6.stats", &tcp6stat, &size,
		    NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf ("%s:\n", name);

#define	p(f, m) if (tcp6stat.f || sflag <= 1) \
    printf(m, tcp6stat.f, plural(tcp6stat.f))
#define	p2(f1, f2, m) if (tcp6stat.f1 || tcp6stat.f2 || sflag <= 1) \
    printf(m, tcp6stat.f1, plural(tcp6stat.f1), tcp6stat.f2, plural(tcp6stat.f2))
#define	p3(f, m) if (tcp6stat.f || sflag <= 1) \
    printf(m, tcp6stat.f, plurales(tcp6stat.f))

	p(tcp6s_sndtotal, "\t%ld packet%s sent\n");
	p2(tcp6s_sndpack,tcp6s_sndbyte,
		"\t\t%ld data packet%s (%ld byte%s)\n");
	p2(tcp6s_sndrexmitpack, tcp6s_sndrexmitbyte,
		"\t\t%ld data packet%s (%ld byte%s) retransmitted\n");
	p2(tcp6s_sndacks, tcp6s_delack,
		"\t\t%ld ack-only packet%s (%ld packet%s delayed)\n");
	p(tcp6s_sndurg, "\t\t%ld URG only packet%s\n");
	p(tcp6s_sndprobe, "\t\t%ld window probe packet%s\n");
	p(tcp6s_sndwinup, "\t\t%ld window update packet%s\n");
	p(tcp6s_sndctrl, "\t\t%ld control packet%s\n");
	p(tcp6s_rcvtotal, "\t%ld packet%s received\n");
	p2(tcp6s_rcvackpack, tcp6s_rcvackbyte, "\t\t%ld ack%s (for %ld byte%s)\n");
	p(tcp6s_rcvdupack, "\t\t%ld duplicate ack%s\n");
	p(tcp6s_rcvacktoomuch, "\t\t%ld ack%s for unsent data\n");
	p2(tcp6s_rcvpack, tcp6s_rcvbyte,
		"\t\t%ld packet%s (%ld byte%s) received in-sequence\n");
	p2(tcp6s_rcvduppack, tcp6s_rcvdupbyte,
		"\t\t%ld completely duplicate packet%s (%ld byte%s)\n");
	p(tcp6s_pawsdrop, "\t\t%ld old duplicate packet%s\n");
	p2(tcp6s_rcvpartduppack, tcp6s_rcvpartdupbyte,
		"\t\t%ld packet%s with some dup. data (%ld byte%s duped)\n");
	p2(tcp6s_rcvoopack, tcp6s_rcvoobyte,
		"\t\t%ld out-of-order packet%s (%ld byte%s)\n");
	p2(tcp6s_rcvpackafterwin, tcp6s_rcvbyteafterwin,
		"\t\t%ld packet%s (%ld byte%s) of data after window\n");
	p(tcp6s_rcvwinprobe, "\t\t%ld window probe%s\n");
	p(tcp6s_rcvwinupd, "\t\t%ld window update packet%s\n");
	p(tcp6s_rcvafterclose, "\t\t%ld packet%s received after close\n");
	p(tcp6s_rcvbadsum, "\t\t%ld discarded for bad checksum%s\n");
	p(tcp6s_rcvbadoff, "\t\t%ld discarded for bad header offset field%s\n");
	p(tcp6s_rcvshort, "\t\t%ld discarded because packet%s too short\n");
	p(tcp6s_connattempt, "\t%ld connection request%s\n");
	p(tcp6s_accepts, "\t%ld connection accept%s\n");
	p(tcp6s_badsyn, "\t%ld bad connection attempt%s\n");
	p(tcp6s_connects, "\t%ld connection%s established (including accepts)\n");
	p2(tcp6s_closed, tcp6s_drops,
		"\t%ld connection%s closed (including %ld drop%s)\n");
	p(tcp6s_conndrops, "\t%ld embryonic connection%s dropped\n");
	p2(tcp6s_rttupdated, tcp6s_segstimed,
		"\t%ld segment%s updated rtt (of %ld attempt%s)\n");
	p(tcp6s_rexmttimeo, "\t%ld retransmit timeout%s\n");
	p(tcp6s_timeoutdrop, "\t\t%ld connection%s dropped by rexmit timeout\n");
	p(tcp6s_persisttimeo, "\t%ld persist timeout%s\n");
	p(tcp6s_persistdrop, "\t%ld connection%s timed out in persist\n");
	p(tcp6s_keeptimeo, "\t%ld keepalive timeout%s\n");
	p(tcp6s_keepprobe, "\t\t%ld keepalive probe%s sent\n");
	p(tcp6s_keepdrops, "\t\t%ld connection%s dropped by keepalive\n");
	p(tcp6s_predack, "\t%ld correct ACK header prediction%s\n");
	p(tcp6s_preddat, "\t%ld correct data packet header prediction%s\n");
	p3(tcp6s_pcbcachemiss, "\t%ld PCB cache miss%s\n");
#undef p
#undef p2
#undef p3
}
#endif

/*
 * Dump UDP6 statistics structure.
 */
void
udp6_stats(u_long off, const char *name)
{
	uint64_t udp6stat[UDP6_NSTATS];
	u_quad_t delivered;

	if (use_sysctl) {
		size_t size = sizeof(udp6stat);

		if (sysctlbyname("net.inet6.udp6.stats", udp6stat, &size,
		    NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}
	printf("%s:\n", name);
#define	p(f, m) if (udp6stat[f] || sflag <= 1) \
    printf(m, (unsigned long long)udp6stat[f], plural(udp6stat[f]))
#define	p1(f, m) if (udp6stat[f] || sflag <= 1) \
    printf(m, (unsigned long long)udp6stat[f])
	p(UDP6_STAT_IPACKETS, "\t%llu datagram%s received\n");
	p1(UDP6_STAT_HDROPS, "\t%llu with incomplete header\n");
	p1(UDP6_STAT_BADLEN, "\t%llu with bad data length field\n");
	p1(UDP6_STAT_BADSUM, "\t%llu with bad checksum\n");
	p1(UDP6_STAT_NOSUM, "\t%llu with no checksum\n");
	p1(UDP6_STAT_NOPORT, "\t%llu dropped due to no socket\n");
	p(UDP6_STAT_NOPORTMCAST,
	    "\t%llu multicast datagram%s dropped due to no socket\n");
	p1(UDP6_STAT_FULLSOCK, "\t%llu dropped due to full socket buffers\n");
	delivered = udp6stat[UDP6_STAT_IPACKETS] -
		    udp6stat[UDP6_STAT_HDROPS] -
		    udp6stat[UDP6_STAT_BADLEN] -
		    udp6stat[UDP6_STAT_BADSUM] -
		    udp6stat[UDP6_STAT_NOPORT] -
		    udp6stat[UDP6_STAT_NOPORTMCAST] -
		    udp6stat[UDP6_STAT_FULLSOCK];
	if (delivered || sflag <= 1)
		printf("\t%llu delivered\n", (unsigned long long)delivered);
	p(UDP6_STAT_OPACKETS, "\t%llu datagram%s output\n");
#undef p
#undef p1
}

static	const char *ip6nh[] = {
/*0*/	"hop by hop",
	"ICMP",
	"IGMP",
	NULL,
	"IP",
/*5*/	NULL,
	"TCP",
	NULL,
	NULL,
	NULL,
/*10*/	NULL, NULL, NULL, NULL, NULL,
/*15*/	NULL,
	NULL,
	"UDP",
	NULL,
	NULL,
/*20*/	NULL,
	NULL,
	"IDP",
	NULL,
	NULL,
/*25*/	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
/*30*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*40*/	NULL,
	"IP6",
	NULL,
	"routing",
	"fragment",
/*45*/	NULL, NULL, NULL, NULL, NULL,
/*50*/	"ESP",
	"AH",
	NULL,
	NULL,
	NULL,
/*55*/	NULL,
	NULL,
	NULL,
	"ICMP6",
	"no next header",
/*60*/	"destination option",
	NULL,
	NULL,
	NULL,
	NULL,
/*65*/	NULL, NULL, NULL, NULL, NULL,
/*70*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*80*/	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"OSPF",
/*90*/	NULL, NULL, NULL, NULL, NULL,
/*95*/	NULL,
	NULL,
	"Ethernet",
	NULL,
	NULL,
/*100*/	NULL,
	NULL,
	NULL,
	"PIM",
	NULL,
/*105*/	NULL, NULL, NULL, NULL, NULL,
/*110*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*120*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*130*/	NULL,
	NULL,
	"SCTP",
	NULL,
	NULL,
/*135*/	NULL, NULL, NULL, NULL, NULL,
/*140*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*160*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*180*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*200*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*220*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/*240*/	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL,
};

/*
 * Dump IP6 statistics structure.
 */
void
ip6_stats(u_long off, const char *name)
{
	uint64_t ip6stat[IP6_NSTATS];
	int first, i;
	struct protoent *ep;
	const char *n;

	if (use_sysctl) {
		size_t size = sizeof(ip6stat);

		if (sysctlbyname("net.inet6.ip6.stats", ip6stat, &size,
		    NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}
	printf("%s:\n", name);

#define	p(f, m) if (ip6stat[f] || sflag <= 1) \
    printf(m, (unsigned long long)ip6stat[f], plural(ip6stat[f]))
#define	p1(f, m) if (ip6stat[f] || sflag <= 1) \
    printf(m, (unsigned long long)ip6stat[f])

	p(IP6_STAT_TOTAL, "\t%llu total packet%s received\n");
	p1(IP6_STAT_TOOSMALL, "\t%llu with size smaller than minimum\n");
	p1(IP6_STAT_TOOSHORT, "\t%llu with data size < data length\n");
	p1(IP6_STAT_BADOPTIONS, "\t%llu with bad options\n");
	p1(IP6_STAT_BADVERS, "\t%llu with incorrect version number\n");
	p(IP6_STAT_FRAGMENTS, "\t%llu fragment%s received\n");
	p(IP6_STAT_FRAGDROPPED,
	    "\t%llu fragment%s dropped (dup or out of space)\n");
	p(IP6_STAT_FRAGTIMEOUT, "\t%llu fragment%s dropped after timeout\n");
	p(IP6_STAT_FRAGOVERFLOW, "\t%llu fragment%s that exceeded limit\n");
	p(IP6_STAT_REASSEMBLED, "\t%llu packet%s reassembled ok\n");
	p(IP6_STAT_DELIVERED, "\t%llu packet%s for this host\n");
	p(IP6_STAT_FORWARD, "\t%llu packet%s forwarded\n");
	p(IP6_STAT_FASTFORWARD, "\t%llu packet%s fast forwarded\n");
	p1(IP6_STAT_FASTFORWARDFLOWS, "\t%llu fast forward flows\n");	
	p(IP6_STAT_CANTFORWARD, "\t%llu packet%s not forwardable\n");
	p(IP6_STAT_REDIRECTSENT, "\t%llu redirect%s sent\n");
	p(IP6_STAT_LOCALOUT, "\t%llu packet%s sent from this host\n");
	p(IP6_STAT_RAWOUT, "\t%llu packet%s sent with fabricated ip header\n");
	p(IP6_STAT_ODROPPED,
	    "\t%llu output packet%s dropped due to no bufs, etc.\n");
	p(IP6_STAT_NOROUTE, "\t%llu output packet%s discarded due to no route\n");
	p(IP6_STAT_FRAGMENTED, "\t%llu output datagram%s fragmented\n");
	p(IP6_STAT_OFRAGMENTS, "\t%llu fragment%s created\n");
	p(IP6_STAT_CANTFRAG, "\t%llu datagram%s that can't be fragmented\n");
	p(IP6_STAT_BADSCOPE, "\t%llu packet%s that violated scope rules\n");
	p(IP6_STAT_NOTMEMBER, "\t%llu multicast packet%s which we don't join\n");
	for (first = 1, i = 0; i < 256; i++)
		if (ip6stat[IP6_STAT_NXTHIST + i] != 0) {
			if (first) {
				printf("\tInput packet histogram:\n");
				first = 0;
			}
			n = NULL;
			if (ip6nh[i])
				n = ip6nh[i];
			else if ((ep = getprotobynumber(i)) != NULL)
				n = ep->p_name;
			if (n)
				printf("\t\t%s: %llu\n", n,
				    (unsigned long long)ip6stat[IP6_STAT_NXTHIST + i]);
			else
				printf("\t\t#%d: %llu\n", i,
				    (unsigned long long)ip6stat[IP6_STAT_NXTHIST + i]);
		}
	printf("\tMbuf statistics:\n");
	p(IP6_STAT_M1, "\t\t%llu one mbuf%s\n");
	for (first = 1, i = 0; i < 32; i++) {
		char ifbuf[IFNAMSIZ];
		if (ip6stat[IP6_STAT_M2M + i] != 0) {		
			if (first) {
				printf("\t\ttwo or more mbuf:\n");
				first = 0;
			}
			printf("\t\t\t%s = %llu\n",
			       if_indextoname(i, ifbuf),
			       (unsigned long long)ip6stat[IP6_STAT_M2M + i]);
		}
	}
	p(IP6_STAT_MEXT1, "\t\t%llu one ext mbuf%s\n");
	p(IP6_STAT_MEXT2M, "\t\t%llu two or more ext mbuf%s\n");
	p(IP6_STAT_EXTHDRTOOLONG,
	    "\t%llu packet%s whose headers are not continuous\n");
	p(IP6_STAT_NOGIF, "\t%llu tunneling packet%s that can't find gif\n");
	p(IP6_STAT_TOOMANYHDR,
	    "\t%llu packet%s discarded due to too many headers\n");

	/* for debugging source address selection */
#define PRINT_SCOPESTAT(s,i) do {\
		switch(i) { /* XXX hardcoding in each case */\
		case 1:\
			p(s, "\t\t%llu node-local%s\n");\
			break;\
		case 2:\
			p(s, "\t\t%llu link-local%s\n");\
			break;\
		case 5:\
			p(s, "\t\t%llu site-local%s\n");\
			break;\
		case 14:\
			p(s, "\t\t%llu global%s\n");\
			break;\
		default:\
			printf("\t\t%llu addresses scope=%x\n",\
			       (unsigned long long)ip6stat[s], i);\
		}\
	} while(/*CONSTCOND*/0);

	p(IP6_STAT_SOURCES_NONE,
	  "\t%llu failure%s of source address selection\n");
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat[IP6_STAT_SOURCES_SAMEIF + i]) {
			if (first) {
				printf("\tsource addresses on an outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(IP6_STAT_SOURCES_SAMEIF + i, i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat[IP6_STAT_SOURCES_OTHERIF + i]) {
			if (first) {
				printf("\tsource addresses on a non-outgoing I/F\n");
				first = 0;
			}
			PRINT_SCOPESTAT(IP6_STAT_SOURCES_OTHERIF + i, i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat[IP6_STAT_SOURCES_SAMESCOPE + i]) {
			if (first) {
				printf("\tsource addresses of same scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(IP6_STAT_SOURCES_SAMESCOPE + i, i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat[IP6_STAT_SOURCES_OTHERSCOPE + i]) {
			if (first) {
				printf("\tsource addresses of a different scope\n");
				first = 0;
			}
			PRINT_SCOPESTAT(IP6_STAT_SOURCES_OTHERSCOPE + i, i);
		}
	}
	for (first = 1, i = 0; i < 16; i++) {
		if (ip6stat[IP6_STAT_SOURCES_DEPRECATED + i]) {
			if (first) {
				printf("\tdeprecated source addresses\n");
				first = 0;
			}
			PRINT_SCOPESTAT(IP6_STAT_SOURCES_DEPRECATED + i, i);
		}
	}

	p1(IP6_STAT_FORWARD_CACHEHIT, "\t%llu forward cache hit\n");
	p1(IP6_STAT_FORWARD_CACHEMISS, "\t%llu forward cache miss\n");
#undef p
#undef p1
}

/*
 * Dump IPv6 per-interface statistics based on RFC 2465.
 */
void
ip6_ifstats(const char *ifname)
{
	struct in6_ifreq ifr;
	int s;
#define	p(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ifr.ifr_ifru.ifru_stat.f, \
	plural(ifr.ifr_ifru.ifru_stat.f))
#define	p_5(f, m) if (ifr.ifr_ifru.ifru_stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ip6stat.f)

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET6)");
		return;
	}

	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	printf("ip6 on %s:\n", ifname);

	if (ioctl(s, SIOCGIFSTAT_IN6, (char *)&ifr) < 0) {
		perror("Warning: ioctl(SIOCGIFSTAT_IN6)");
		goto end;
	}

	p(ifs6_in_receive, "\t%llu total input datagram%s\n");
	p(ifs6_in_hdrerr, "\t%llu datagram%s with invalid header received\n");
	p(ifs6_in_toobig, "\t%llu datagram%s exceeded MTU received\n");
	p(ifs6_in_noroute, "\t%llu datagram%s with no route received\n");
	p(ifs6_in_addrerr, "\t%llu datagram%s with invalid dst received\n");
	p(ifs6_in_truncated, "\t%llu truncated datagram%s received\n");
	p(ifs6_in_protounknown, "\t%llu datagram%s with unknown proto received\n");
	p(ifs6_in_discard, "\t%llu input datagram%s discarded\n");
	p(ifs6_in_deliver,
	  "\t%llu datagram%s delivered to an upper layer protocol\n");
	p(ifs6_out_forward, "\t%llu datagram%s forwarded to this interface\n");
	p(ifs6_out_request,
	  "\t%llu datagram%s sent from an upper layer protocol\n");
	p(ifs6_out_discard, "\t%llu total discarded output datagram%s\n");
	p(ifs6_out_fragok, "\t%llu output datagram%s fragmented\n");
	p(ifs6_out_fragfail, "\t%llu output datagram%s failed on fragment\n");
	p(ifs6_out_fragcreat, "\t%llu output datagram%s succeeded on fragment\n");
	p(ifs6_reass_reqd, "\t%llu incoming datagram%s fragmented\n");
	p(ifs6_reass_ok, "\t%llu datagram%s reassembled\n");
	p(ifs6_reass_fail, "\t%llu datagram%s failed on reassembling\n");
	p(ifs6_in_mcast, "\t%llu multicast datagram%s received\n");
	p(ifs6_out_mcast, "\t%llu multicast datagram%s sent\n");

  end:
	close(s);

#undef p
#undef p_5
}

static	const char *icmp6names[] = {
	"#0",
	"unreach",
	"packet too big",
	"time exceed",
	"parameter problem",
	"#5",
	"#6",
	"#7",
	"#8",
	"#9",
	"#10",
	"#11",
	"#12",
	"#13",
	"#14",
	"#15",
	"#16",
	"#17",
	"#18",
	"#19",	
	"#20",
	"#21",
	"#22",
	"#23",
	"#24",
	"#25",
	"#26",
	"#27",
	"#28",
	"#29",	
	"#30",
	"#31",
	"#32",
	"#33",
	"#34",
	"#35",
	"#36",
	"#37",
	"#38",
	"#39",	
	"#40",
	"#41",
	"#42",
	"#43",
	"#44",
	"#45",
	"#46",
	"#47",
	"#48",
	"#49",	
	"#50",
	"#51",
	"#52",
	"#53",
	"#54",
	"#55",
	"#56",
	"#57",
	"#58",
	"#59",	
	"#60",
	"#61",
	"#62",
	"#63",
	"#64",
	"#65",
	"#66",
	"#67",
	"#68",
	"#69",	
	"#70",
	"#71",
	"#72",
	"#73",
	"#74",
	"#75",
	"#76",
	"#77",
	"#78",
	"#79",	
	"#80",
	"#81",
	"#82",
	"#83",
	"#84",
	"#85",
	"#86",
	"#87",
	"#88",
	"#89",	
	"#80",
	"#91",
	"#92",
	"#93",
	"#94",
	"#95",
	"#96",
	"#97",
	"#98",
	"#99",	
	"#100",
	"#101",
	"#102",
	"#103",
	"#104",
	"#105",
	"#106",
	"#107",
	"#108",
	"#109",	
	"#110",
	"#111",
	"#112",
	"#113",
	"#114",
	"#115",
	"#116",
	"#117",
	"#118",
	"#119",	
	"#120",
	"#121",
	"#122",
	"#123",
	"#124",
	"#125",
	"#126",
	"#127",
	"echo",
	"echo reply",	
	"multicast listener query",
	"multicast listener report",
	"multicast listener done",
	"router solicitation",
	"router advertisement",
	"neighbor solicitation",
	"neighbor advertisement",
	"redirect",
	"router renumbering",
	"node information request",
	"node information reply",
	"#141",
	"#142",
	"#143",
	"#144",
	"#145",
	"#146",
	"#147",
	"#148",
	"#149",	
	"#150",
	"#151",
	"#152",
	"#153",
	"#154",
	"#155",
	"#156",
	"#157",
	"#158",
	"#159",	
	"#160",
	"#161",
	"#162",
	"#163",
	"#164",
	"#165",
	"#166",
	"#167",
	"#168",
	"#169",	
	"#170",
	"#171",
	"#172",
	"#173",
	"#174",
	"#175",
	"#176",
	"#177",
	"#178",
	"#179",	
	"#180",
	"#181",
	"#182",
	"#183",
	"#184",
	"#185",
	"#186",
	"#187",
	"#188",
	"#189",	
	"#180",
	"#191",
	"#192",
	"#193",
	"#194",
	"#195",
	"#196",
	"#197",
	"#198",
	"#199",	
	"#200",
	"#201",
	"#202",
	"#203",
	"#204",
	"#205",
	"#206",
	"#207",
	"#208",
	"#209",	
	"#210",
	"#211",
	"#212",
	"#213",
	"#214",
	"#215",
	"#216",
	"#217",
	"#218",
	"#219",	
	"#220",
	"#221",
	"#222",
	"#223",
	"#224",
	"#225",
	"#226",
	"#227",
	"#228",
	"#229",	
	"#230",
	"#231",
	"#232",
	"#233",
	"#234",
	"#235",
	"#236",
	"#237",
	"#238",
	"#239",	
	"#240",
	"#241",
	"#242",
	"#243",
	"#244",
	"#245",
	"#246",
	"#247",
	"#248",
	"#249",	
	"#250",
	"#251",
	"#252",
	"#253",
	"#254",
	"#255",
};

/*
 * Dump ICMPv6 statistics.
 */
void
icmp6_stats(u_long off, const char *name)
{
	uint64_t icmp6stat[ICMP6_NSTATS];
	int i, first;

	if (use_sysctl) {
		size_t size = sizeof(icmp6stat);

		if (sysctlbyname("net.inet6.icmp6.stats", icmp6stat, &size,
		    NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}
	
	printf("%s:\n", name);

#define	p(f, m) if (icmp6stat[f] || sflag <= 1) \
    printf(m, (unsigned long long)icmp6stat[f], plural(icmp6stat[f]))
#define p_oerr(f, m) if (icmp6stat[ICMP6_STAT_OUTERRHIST + f] || sflag <= 1) \
    printf(m, (unsigned long long)icmp6stat[ICMP6_STAT_OUTERRHIST + f])

	p(ICMP6_STAT_ERROR, "\t%llu call%s to icmp6_error\n");
	p(ICMP6_STAT_CANTERROR,
	    "\t%llu error%s not generated because old message was icmp6 or so\n");
	p(ICMP6_STAT_TOOFREQ,
	    "\t%llu error%s not generated because of rate limitation\n");
	for (first = 1, i = 0; i < 256; i++)
		if (icmp6stat[ICMP6_STAT_OUTHIST + i] != 0) {
			if (first) {
				printf("\tOutput packet histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			 (unsigned long long)icmp6stat[ICMP6_STAT_OUTHIST + i]);
		}
	p(ICMP6_STAT_BADCODE, "\t%llu message%s with bad code fields\n");
	p(ICMP6_STAT_TOOSHORT, "\t%llu message%s < minimum length\n");
	p(ICMP6_STAT_CHECKSUM, "\t%llu bad checksum%s\n");
	p(ICMP6_STAT_BADLEN, "\t%llu message%s with bad length\n");
	for (first = 1, i = 0; i < ICMP6_MAXTYPE; i++)
		if (icmp6stat[ICMP6_STAT_INHIST + i] != 0) {
			if (first) {
				printf("\tInput packet histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmp6names[i],
			  (unsigned long long)icmp6stat[ICMP6_STAT_INHIST + i]);
		}
	printf("\tHistogram of error messages to be generated:\n");
	p_oerr(ICMP6_ERRSTAT_DST_UNREACH_NOROUTE, "\t\t%llu no route\n");
	p_oerr(ICMP6_ERRSTAT_DST_UNREACH_ADMIN, "\t\t%llu administratively prohibited\n");
	p_oerr(ICMP6_ERRSTAT_DST_UNREACH_BEYONDSCOPE, "\t\t%llu beyond scope\n");
	p_oerr(ICMP6_ERRSTAT_DST_UNREACH_ADDR, "\t\t%llu address unreachable\n");
	p_oerr(ICMP6_ERRSTAT_DST_UNREACH_NOPORT, "\t\t%llu port unreachable\n");
	p_oerr(ICMP6_ERRSTAT_PACKET_TOO_BIG, "\t\t%llu packet too big\n");
	p_oerr(ICMP6_ERRSTAT_TIME_EXCEED_TRANSIT, "\t\t%llu time exceed transit\n");
	p_oerr(ICMP6_ERRSTAT_TIME_EXCEED_REASSEMBLY, "\t\t%llu time exceed reassembly\n");
	p_oerr(ICMP6_ERRSTAT_PARAMPROB_HEADER, "\t\t%llu erroneous header field\n");
	p_oerr(ICMP6_ERRSTAT_PARAMPROB_NEXTHEADER, "\t\t%llu unrecognized next header\n");
	p_oerr(ICMP6_ERRSTAT_PARAMPROB_OPTION, "\t\t%llu unrecognized option\n");
	p_oerr(ICMP6_ERRSTAT_REDIRECT, "\t\t%llu redirect\n");
	p_oerr(ICMP6_ERRSTAT_UNKNOWN, "\t\t%llu unknown\n");

	p(ICMP6_STAT_REFLECT, "\t%llu message response%s generated\n");
	p(ICMP6_STAT_ND_TOOMANYOPT, "\t%llu message%s with too many ND options\n");
	p(ICMP6_STAT_ND_BADOPT, "\t%llu message%s with bad ND options\n");
	p(ICMP6_STAT_BADNS, "\t%llu bad neighbor solicitation message%s\n");
	p(ICMP6_STAT_BADNA, "\t%llu bad neighbor advertisement message%s\n");
	p(ICMP6_STAT_BADRS, "\t%llu bad router solicitation message%s\n");
	p(ICMP6_STAT_BADRA, "\t%llu bad router advertisement message%s\n");
	p(ICMP6_STAT_DROPPED_RAROUTE, "\t%llu router advertisement route%s dropped\n");
	p(ICMP6_STAT_BADREDIRECT, "\t%llu bad redirect message%s\n");
	p(ICMP6_STAT_PMTUCHG, "\t%llu path MTU change%s\n");
#undef p
#undef p_oerr
}

/*
 * Dump ICMPv6 per-interface statistics based on RFC 2466.
 */
void
icmp6_ifstats(const char *ifname)
{
	struct in6_ifreq ifr;
	int s;
#define	p(f, m) if (ifr.ifr_ifru.ifru_icmp6stat.f || sflag <= 1) \
    printf(m, (unsigned long long)ifr.ifr_ifru.ifru_icmp6stat.f, \
	plural(ifr.ifr_ifru.ifru_icmp6stat.f))

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("Warning: socket(AF_INET6)");
		return;
	}

	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	printf("icmp6 on %s:\n", ifname);

	if (ioctl(s, SIOCGIFSTAT_ICMP6, (char *)&ifr) < 0) {
		perror("Warning: ioctl(SIOCGIFSTAT_ICMP6)");
		goto end;
	}

	p(ifs6_in_msg, "\t%llu total input message%s\n");
	p(ifs6_in_error, "\t%llu total input error message%s\n"); 
	p(ifs6_in_dstunreach, "\t%llu input destination unreachable error%s\n");
	p(ifs6_in_adminprohib, "\t%llu input administratively prohibited error%s\n");
	p(ifs6_in_timeexceed, "\t%llu input time exceeded error%s\n");
	p(ifs6_in_paramprob, "\t%llu input parameter problem error%s\n");
	p(ifs6_in_pkttoobig, "\t%llu input packet too big error%s\n");
	p(ifs6_in_echo, "\t%llu input echo request%s\n");
	p(ifs6_in_echoreply, "\t%llu input echo reply%s\n");
	p(ifs6_in_routersolicit, "\t%llu input router solicitation%s\n");
	p(ifs6_in_routeradvert, "\t%llu input router advertisement%s\n");
	p(ifs6_in_neighborsolicit, "\t%llu input neighbor solicitation%s\n");
	p(ifs6_in_neighboradvert, "\t%llu input neighbor advertisement%s\n");
	p(ifs6_in_redirect, "\t%llu input redirect%s\n");
	p(ifs6_in_mldquery, "\t%llu input MLD query%s\n");
	p(ifs6_in_mldreport, "\t%llu input MLD report%s\n");
	p(ifs6_in_mlddone, "\t%llu input MLD done%s\n");

	p(ifs6_out_msg, "\t%llu total output message%s\n");
	p(ifs6_out_error, "\t%llu total output error message%s\n");
	p(ifs6_out_dstunreach, "\t%llu output destination unreachable error%s\n");
	p(ifs6_out_adminprohib, "\t%llu output administratively prohibited error%s\n");
	p(ifs6_out_timeexceed, "\t%llu output time exceeded error%s\n");
	p(ifs6_out_paramprob, "\t%llu output parameter problem error%s\n");
	p(ifs6_out_pkttoobig, "\t%llu output packet too big error%s\n");
	p(ifs6_out_echo, "\t%llu output echo request%s\n");
	p(ifs6_out_echoreply, "\t%llu output echo reply%s\n");
	p(ifs6_out_routersolicit, "\t%llu output router solicitation%s\n");
	p(ifs6_out_routeradvert, "\t%llu output router advertisement%s\n");
	p(ifs6_out_neighborsolicit, "\t%llu output neighbor solicitation%s\n");
	p(ifs6_out_neighboradvert, "\t%llu output neighbor advertisement%s\n");
	p(ifs6_out_redirect, "\t%llu output redirect%s\n");
	p(ifs6_out_mldquery, "\t%llu output MLD query%s\n");
	p(ifs6_out_mldreport, "\t%llu output MLD report%s\n");
	p(ifs6_out_mlddone, "\t%llu output MLD done%s\n");

  end:
	close(s);
#undef p
}

/*
 * Dump PIM statistics structure.
 */
void
pim6_stats(u_long off, const char *name)
{
	uint64_t pim6stat[PIM6_NSTATS];

	if (use_sysctl) {
		size_t size = sizeof(pim6stat);

		if (sysctlbyname("net.inet6.pim6.stats", pim6stat, &size,
		    NULL, 0) == -1)
			return;
        } else {
		warnx("%s stats not available via KVM.", name);
		return;
	}
	printf("%s:\n", name);

#define	p(f, m) if (pim6stat[f] || sflag <= 1) \
    printf(m, (unsigned long long)pim6stat[f], plural(pim6stat[f]))
	p(PIM6_STAT_RCV_TOTAL, "\t%llu message%s received\n");
	p(PIM6_STAT_RCV_TOOSHORT, "\t%llu message%s received with too few bytes\n");
	p(PIM6_STAT_RCV_BADSUM, "\t%llu message%s received with bad checksum\n");
	p(PIM6_STAT_RCV_BADVERSION, "\t%llu message%s received with bad version\n");
	p(PIM6_STAT_RCV_REGISTERS, "\t%llu register%s received\n");
	p(PIM6_STAT_RCV_BADREGISTERS, "\t%llu bad register%s received\n");
	p(PIM6_STAT_SND_REGISTERS, "\t%llu register%s sent\n");
#undef p
}

/*
 * Dump raw ip6 statistics structure.
 */
void
rip6_stats(u_long off, const char *name)
{
	uint64_t rip6stat[RIP6_NSTATS];
	u_quad_t delivered;

	if (use_sysctl) {
		size_t size = sizeof(rip6stat);

		if (sysctlbyname("net.inet6.raw6.stats", rip6stat, &size,
		    NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}
	printf("%s:\n", name);

#define	p(f, m) if (rip6stat[f] || sflag <= 1) \
    printf(m, (unsigned long long)rip6stat[f], plural(rip6stat[f]))
	p(RIP6_STAT_IPACKETS, "\t%llu message%s received\n");
	p(RIP6_STAT_ISUM, "\t%llu checksum calculation%s on inbound\n");
	p(RIP6_STAT_BADSUM, "\t%llu message%s with bad checksum\n");
	p(RIP6_STAT_NOSOCK, "\t%llu message%s dropped due to no socket\n");
	p(RIP6_STAT_NOSOCKMCAST,
	    "\t%llu multicast message%s dropped due to no socket\n");
	p(RIP6_STAT_FULLSOCK,
	    "\t%llu message%s dropped due to full socket buffers\n");
	delivered = rip6stat[RIP6_STAT_IPACKETS] -
		    rip6stat[RIP6_STAT_BADSUM] -
		    rip6stat[RIP6_STAT_NOSOCK] -
		    rip6stat[RIP6_STAT_NOSOCKMCAST] -
		    rip6stat[RIP6_STAT_FULLSOCK];
	if (delivered || sflag <= 1)
		printf("\t%llu delivered\n", (unsigned long long)delivered);
	p(RIP6_STAT_OPACKETS, "\t%llu datagram%s output\n");
#undef p
}

/*
 * Pretty print an Internet address (net address + port).
 * Take numeric_addr and numeric_port into consideration.
 */
void
inet6print(const struct in6_addr *in6, int port, const char *proto)
{
#define GETSERVBYPORT6(port, proto, ret)\
do {\
	if (strcmp((proto), "tcp6") == 0)\
		(ret) = getservbyport((int)(port), "tcp");\
	else if (strcmp((proto), "udp6") == 0)\
		(ret) = getservbyport((int)(port), "udp");\
	else\
		(ret) = getservbyport((int)(port), (proto));\
} while (0)
	struct servent *sp = 0;
	char line[80], *cp;
	int lwidth;

	lwidth = Aflag ? 12 : 16;
	if (vflag && lwidth < (int)strlen(inet6name(in6)))
		lwidth = strlen(inet6name(in6));
	snprintf(line, sizeof(line), "%.*s.", lwidth, inet6name(in6));
	cp = strchr(line, '\0');
	if (!numeric_port && port)
		GETSERVBYPORT6(port, proto, sp);
	if (sp || port == 0)
		snprintf(cp, sizeof(line) - (cp - line),
		    "%s", sp ? sp->s_name : "*");
	else
		snprintf(cp, sizeof(line) - (cp - line),
		    "%d", ntohs((u_short)port));
	lwidth = Aflag ? 18 : 22;
	if (vflag && lwidth < (int)strlen(line))
		lwidth = strlen(line);
	printf(" %-*.*s", lwidth, lwidth, line);
}

/*
 * Construct an Internet address representation.
 * If the numeric_addr has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */

char *
inet6name(const struct in6_addr *in6p)
{
	char *cp;
	static char line[NI_MAXHOST];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;
	char hbuf[NI_MAXHOST];
	struct sockaddr_in6 sin6;
	const int niflag = NI_NUMERICHOST;

	if (first && !numeric_addr) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strlcpy(domain, cp + 1, sizeof(domain));
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!numeric_addr && !IN6_IS_ADDR_UNSPECIFIED(in6p)) {
		hp = gethostbyaddr((const char *)in6p, sizeof(*in6p), AF_INET6);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (IN6_IS_ADDR_UNSPECIFIED(in6p))
		strlcpy(line, "*", sizeof(line));
	else if (cp)
		strlcpy(line, cp, sizeof(line));
	else {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = *in6p;
		inet6_getscopeid(&sin6, INET6_IS_ADDR_LINKLOCAL|
		    INET6_IS_ADDR_MC_LINKLOCAL);
		if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
				hbuf, sizeof(hbuf), NULL, 0, niflag) != 0)
			strlcpy(hbuf, "?", sizeof(hbuf));
		strlcpy(line, hbuf, sizeof(line));
	}
	return (line);
}

/*
 * Dump the contents of a TCP6 PCB.
 */
void
tcp6_dump(u_long off, const char *name, u_long pcbaddr)
{
	callout_impl_t *ci;
	int i, hardticks;
	struct kinfo_pcb *pcblist;
#ifdef TCP6
#define mypcb tcp6cb
#else
#define mypcb tcpcb
#endif
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

	kread(pcbaddr, (char *)&mypcb, sizeof(mypcb));
	hardticks = get_hardticks();

	printf("TCP Protocol Control Block at 0x%08lx:\n\n", pcbaddr);
	printf("Timers:\n");
	for (i = 0; i < TCP6T_NTIMERS; i++) {
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

	if (mypcb.t_state < 0 || mypcb.t_state >= TCP6_NSTATES)
		printf("State: %d", mypcb.t_state);
	else
		printf("State: %s", tcp6states[mypcb.t_state]);
	printf(", flags 0x%x, in6pcb 0x%lx\n\n", mypcb.t_flags,
	    (u_long)mypcb.t_in6pcb);

	printf("rxtshift %d, rxtcur %d, dupacks %d\n", mypcb.t_rxtshift,
	    mypcb.t_rxtcur, mypcb.t_dupacks);
#ifdef TCP6
	printf("peermaxseg %u, maxseg %u, force %d\n\n", mypcb.t_peermaxseg,
	    mypcb.t_maxseg, mypcb.t_force);
#endif

	printf("snd_una %u, snd_nxt %u, snd_up %u\n",
	    mypcb.snd_una, mypcb.snd_nxt, mypcb.snd_up);
	printf("snd_wl1 %u, snd_wl2 %u, iss %u, snd_wnd %llu\n\n",
	    mypcb.snd_wl1, mypcb.snd_wl2, mypcb.iss,
	    (unsigned long long)mypcb.snd_wnd);

	printf("rcv_wnd %llu, rcv_nxt %u, rcv_up %u, irs %u\n\n",
	    (unsigned long long)mypcb.rcv_wnd, mypcb.rcv_nxt,
	    mypcb.rcv_up, mypcb.irs);

	printf("rcv_adv %u, snd_max %u, snd_cwnd %llu, snd_ssthresh %llu\n",
	    mypcb.rcv_adv, mypcb.snd_max, (unsigned long long)mypcb.snd_cwnd,
	    (unsigned long long)mypcb.snd_ssthresh);

#ifdef TCP6
	printf("idle %d, rtt %d, " mypcb.t_idle, mypcb.t_rtt)
#endif
	printf("rtseq %u, srtt %d, rttvar %d, rttmin %d, "
	    "max_sndwnd %llu\n\n", mypcb.t_rtseq,
	    mypcb.t_srtt, mypcb.t_rttvar, mypcb.t_rttmin,
	    (unsigned long long)mypcb.max_sndwnd);

	printf("oobflags %d, iobc %d, softerror %d\n\n", mypcb.t_oobflags,
	    mypcb.t_iobc, mypcb.t_softerror);

	printf("snd_scale %d, rcv_scale %d, req_r_scale %d, req_s_scale %d\n",
	    mypcb.snd_scale, mypcb.rcv_scale, mypcb.request_r_scale,
	    mypcb.requested_s_scale);
	printf("ts_recent %u, ts_regent_age %d, last_ack_sent %u\n",
	    mypcb.ts_recent, mypcb.ts_recent_age, mypcb.last_ack_sent);
}

#endif /*INET6*/
