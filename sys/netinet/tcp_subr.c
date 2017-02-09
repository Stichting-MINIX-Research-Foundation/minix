/*	$NetBSD: tcp_subr.c,v 1.264 2015/09/07 01:56:50 ozaki-r Exp $	*/

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

/*-
 * Copyright (c) 1997, 1998, 2000, 2001, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Kevin M. Lahey of the Numerical Aerospace Simulation
 * Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
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
 *
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcp_subr.c,v 1.264 2015/09/07 01:56:50 ozaki-r Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_ipsec.h"
#include "opt_tcp_compat_42.h"
#include "opt_inet_csum.h"
#include "opt_mbuftrace.h"
#endif

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/once.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/md5.h>
#include <sys/cprng.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6protosw.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#endif

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_vtw.h>
#include <netinet/tcp_private.h>
#include <netinet/tcp_congctl.h>
#include <netinet/tcpip.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
 #include <netipsec/key.h>
#endif	/* IPSEC*/


struct	inpcbtable tcbtable;	/* head of queue of active tcpcb's */
u_int32_t tcp_now;		/* slow ticks, for RFC 1323 timestamps */

percpu_t *tcpstat_percpu;

/* patchable/settable parameters for tcp */
int 	tcp_mssdflt = TCP_MSS;
int	tcp_minmss = TCP_MINMSS;
int 	tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
int	tcp_do_rfc1323 = 1;	/* window scaling / timestamps (obsolete) */
int	tcp_do_rfc1948 = 0;	/* ISS by cryptographic hash */
int	tcp_do_sack = 1;	/* selective acknowledgement */
int	tcp_do_win_scale = 1;	/* RFC1323 window scaling */
int	tcp_do_timestamps = 1;	/* RFC1323 timestamps */
int	tcp_ack_on_push = 0;	/* set to enable immediate ACK-on-PUSH */
int	tcp_do_ecn = 0;		/* Explicit Congestion Notification */
#ifndef TCP_INIT_WIN
#define	TCP_INIT_WIN	4	/* initial slow start window */
#endif
#ifndef TCP_INIT_WIN_LOCAL
#define	TCP_INIT_WIN_LOCAL 4	/* initial slow start window for local nets */
#endif
/*
 * Up to 5 we scale linearly, to reach 3 * 1460; then (iw) * 1460.
 * This is to simulate current behavior for iw == 4
 */
int tcp_init_win_max[] = {
	 1 * 1460,
	 1 * 1460,
	 2 * 1460,
	 2 * 1460,
	 3 * 1460,
	 5 * 1460,
	 6 * 1460,
	 7 * 1460,
	 8 * 1460,
	 9 * 1460,
	10 * 1460
};
int	tcp_init_win = TCP_INIT_WIN;
int	tcp_init_win_local = TCP_INIT_WIN_LOCAL;
int	tcp_mss_ifmtu = 0;
#ifdef TCP_COMPAT_42
int	tcp_compat_42 = 1;
#else
int	tcp_compat_42 = 0;
#endif
int	tcp_rst_ppslim = 100;	/* 100pps */
int	tcp_ackdrop_ppslim = 100;	/* 100pps */
int	tcp_do_loopback_cksum = 0;
int	tcp_do_abc = 1;		/* RFC3465 Appropriate byte counting. */
int	tcp_abc_aggressive = 1;	/* 1: L=2*SMSS  0: L=1*SMSS */
int	tcp_sack_tp_maxholes = 32;
int	tcp_sack_globalmaxholes = 1024;
int	tcp_sack_globalholes = 0;
int	tcp_ecn_maxretries = 1;
int	tcp_msl_enable = 1;		/* enable TIME_WAIT truncation	*/
int	tcp_msl_loop   = PR_SLOWHZ;	/* MSL for loopback		*/
int	tcp_msl_local  = 5 * PR_SLOWHZ;	/* MSL for 'local'		*/
int	tcp_msl_remote = TCPTV_MSL;	/* MSL otherwise		*/
int	tcp_msl_remote_threshold = TCPTV_SRTTDFLT;	/* RTT threshold */ 
int	tcp_rttlocal = 0;		/* Use RTT to decide who's 'local' */

int	tcp4_vtw_enable = 0;		/* 1 to enable */
int	tcp6_vtw_enable = 0;		/* 1 to enable */
int	tcp_vtw_was_enabled = 0;
int	tcp_vtw_entries = 1 << 4;	/* 16 vestigial TIME_WAIT entries */

/* tcb hash */
#ifndef TCBHASHSIZE
#define	TCBHASHSIZE	128
#endif
int	tcbhashsize = TCBHASHSIZE;

/* syn hash parameters */
#define	TCP_SYN_HASH_SIZE	293
#define	TCP_SYN_BUCKET_SIZE	35
int	tcp_syn_cache_size = TCP_SYN_HASH_SIZE;
int	tcp_syn_cache_limit = TCP_SYN_HASH_SIZE*TCP_SYN_BUCKET_SIZE;
int	tcp_syn_bucket_limit = 3*TCP_SYN_BUCKET_SIZE;
struct	syn_cache_head tcp_syn_cache[TCP_SYN_HASH_SIZE];

int	tcp_freeq(struct tcpcb *);
static int	tcp_iss_secret_init(void);

#ifdef INET
static void	tcp_mtudisc_callback(struct in_addr);
#endif

#ifdef INET6
void	tcp6_mtudisc(struct in6pcb *, int);
#endif

static struct pool tcpcb_pool;

static int tcp_drainwanted;

#ifdef TCP_CSUM_COUNTERS
#include <sys/device.h>

#if defined(INET)
struct evcnt tcp_hwcsum_bad = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "hwcsum bad");
struct evcnt tcp_hwcsum_ok = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "hwcsum ok");
struct evcnt tcp_hwcsum_data = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "hwcsum data");
struct evcnt tcp_swcsum = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "swcsum");

EVCNT_ATTACH_STATIC(tcp_hwcsum_bad);
EVCNT_ATTACH_STATIC(tcp_hwcsum_ok);
EVCNT_ATTACH_STATIC(tcp_hwcsum_data);
EVCNT_ATTACH_STATIC(tcp_swcsum);
#endif /* defined(INET) */

#if defined(INET6)
struct evcnt tcp6_hwcsum_bad = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp6", "hwcsum bad");
struct evcnt tcp6_hwcsum_ok = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp6", "hwcsum ok");
struct evcnt tcp6_hwcsum_data = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp6", "hwcsum data");
struct evcnt tcp6_swcsum = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp6", "swcsum");

EVCNT_ATTACH_STATIC(tcp6_hwcsum_bad);
EVCNT_ATTACH_STATIC(tcp6_hwcsum_ok);
EVCNT_ATTACH_STATIC(tcp6_hwcsum_data);
EVCNT_ATTACH_STATIC(tcp6_swcsum);
#endif /* defined(INET6) */
#endif /* TCP_CSUM_COUNTERS */


#ifdef TCP_OUTPUT_COUNTERS
#include <sys/device.h>

struct evcnt tcp_output_bigheader = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "output big header");
struct evcnt tcp_output_predict_hit = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "output predict hit");
struct evcnt tcp_output_predict_miss = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "output predict miss");
struct evcnt tcp_output_copysmall = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "output copy small");
struct evcnt tcp_output_copybig = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "output copy big");
struct evcnt tcp_output_refbig = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp", "output reference big");

EVCNT_ATTACH_STATIC(tcp_output_bigheader);
EVCNT_ATTACH_STATIC(tcp_output_predict_hit);
EVCNT_ATTACH_STATIC(tcp_output_predict_miss);
EVCNT_ATTACH_STATIC(tcp_output_copysmall);
EVCNT_ATTACH_STATIC(tcp_output_copybig);
EVCNT_ATTACH_STATIC(tcp_output_refbig);

#endif /* TCP_OUTPUT_COUNTERS */

#ifdef TCP_REASS_COUNTERS
#include <sys/device.h>

struct evcnt tcp_reass_ = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "tcp_reass", "calls");
struct evcnt tcp_reass_empty = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "insert into empty queue");
struct evcnt tcp_reass_iteration[8] = {
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", ">7 iterations"),
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", "1 iteration"),
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", "2 iterations"),
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", "3 iterations"),
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", "4 iterations"),
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", "5 iterations"),
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", "6 iterations"),
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &tcp_reass_, "tcp_reass", "7 iterations"),
};
struct evcnt tcp_reass_prependfirst = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "prepend to first");
struct evcnt tcp_reass_prepend = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "prepend");
struct evcnt tcp_reass_insert = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "insert");
struct evcnt tcp_reass_inserttail = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "insert at tail");
struct evcnt tcp_reass_append = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "append");
struct evcnt tcp_reass_appendtail = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "append to tail fragment");
struct evcnt tcp_reass_overlaptail = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "overlap at end");
struct evcnt tcp_reass_overlapfront = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "overlap at start");
struct evcnt tcp_reass_segdup = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "duplicate segment");
struct evcnt tcp_reass_fragdup = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    &tcp_reass_, "tcp_reass", "duplicate fragment");

EVCNT_ATTACH_STATIC(tcp_reass_);
EVCNT_ATTACH_STATIC(tcp_reass_empty);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 0);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 1);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 2);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 3);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 4);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 5);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 6);
EVCNT_ATTACH_STATIC2(tcp_reass_iteration, 7);
EVCNT_ATTACH_STATIC(tcp_reass_prependfirst);
EVCNT_ATTACH_STATIC(tcp_reass_prepend);
EVCNT_ATTACH_STATIC(tcp_reass_insert);
EVCNT_ATTACH_STATIC(tcp_reass_inserttail);
EVCNT_ATTACH_STATIC(tcp_reass_append);
EVCNT_ATTACH_STATIC(tcp_reass_appendtail);
EVCNT_ATTACH_STATIC(tcp_reass_overlaptail);
EVCNT_ATTACH_STATIC(tcp_reass_overlapfront);
EVCNT_ATTACH_STATIC(tcp_reass_segdup);
EVCNT_ATTACH_STATIC(tcp_reass_fragdup);

#endif /* TCP_REASS_COUNTERS */

#ifdef MBUFTRACE
struct mowner tcp_mowner = MOWNER_INIT("tcp", "");
struct mowner tcp_rx_mowner = MOWNER_INIT("tcp", "rx");
struct mowner tcp_tx_mowner = MOWNER_INIT("tcp", "tx");
struct mowner tcp_sock_mowner = MOWNER_INIT("tcp", "sock");
struct mowner tcp_sock_rx_mowner = MOWNER_INIT("tcp", "sock rx");
struct mowner tcp_sock_tx_mowner = MOWNER_INIT("tcp", "sock tx");
#endif

callout_t tcp_slowtimo_ch;

static int
do_tcpinit(void)
{

	in_pcbinit(&tcbtable, tcbhashsize, tcbhashsize);
	pool_init(&tcpcb_pool, sizeof(struct tcpcb), 0, 0, 0, "tcpcbpl",
	    NULL, IPL_SOFTNET);

	tcp_usrreq_init();

	/* Initialize timer state. */
	tcp_timer_init();

	/* Initialize the compressed state engine. */
	syn_cache_init();

	/* Initialize the congestion control algorithms. */
	tcp_congctl_init();

	/* Initialize the TCPCB template. */
	tcp_tcpcb_template();

	/* Initialize reassembly queue */
	tcpipqent_init();

	/* SACK */
	tcp_sack_init();

	MOWNER_ATTACH(&tcp_tx_mowner);
	MOWNER_ATTACH(&tcp_rx_mowner);
	MOWNER_ATTACH(&tcp_reass_mowner);
	MOWNER_ATTACH(&tcp_sock_mowner);
	MOWNER_ATTACH(&tcp_sock_tx_mowner);
	MOWNER_ATTACH(&tcp_sock_rx_mowner);
	MOWNER_ATTACH(&tcp_mowner);

	tcpstat_percpu = percpu_alloc(sizeof(uint64_t) * TCP_NSTATS);

	vtw_earlyinit();

	callout_init(&tcp_slowtimo_ch, CALLOUT_MPSAFE);
	callout_reset(&tcp_slowtimo_ch, 1, tcp_slowtimo, NULL);

	return 0;
}

void
tcp_init_common(unsigned basehlen)
{
	static ONCE_DECL(dotcpinit);
	unsigned hlen = basehlen + sizeof(struct tcphdr);
	unsigned oldhlen;

	if (max_linkhdr + hlen > MHLEN)
		panic("tcp_init");
	while ((oldhlen = max_protohdr) < hlen)
		atomic_cas_uint(&max_protohdr, oldhlen, hlen);

	RUN_ONCE(&dotcpinit, do_tcpinit);
}

/*
 * Tcp initialization
 */
void
tcp_init(void)
{

	icmp_mtudisc_callback_register(tcp_mtudisc_callback);

	tcp_init_common(sizeof(struct ip));
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 */
struct mbuf *
tcp_template(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
#ifdef INET6
	struct in6pcb *in6p = tp->t_in6pcb;
#endif
	struct tcphdr *n;
	struct mbuf *m;
	int hlen;

	switch (tp->t_family) {
	case AF_INET:
		hlen = sizeof(struct ip);
		if (inp)
			break;
#ifdef INET6
		if (in6p) {
			/* mapped addr case */
			if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr)
			 && IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr))
				break;
		}
#endif
		return NULL;	/*EINVAL*/
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		if (in6p) {
			/* more sainty check? */
			break;
		}
		return NULL;	/*EINVAL*/
#endif
	default:
		hlen = 0;	/*pacify gcc*/
		return NULL;	/*EAFNOSUPPORT*/
	}
#ifdef DIAGNOSTIC
	if (hlen + sizeof(struct tcphdr) > MCLBYTES)
		panic("mclbytes too small for t_template");
#endif
	m = tp->t_template;
	if (m && m->m_len == hlen + sizeof(struct tcphdr))
		;
	else {
		if (m)
			m_freem(m);
		m = tp->t_template = NULL;
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m && hlen + sizeof(struct tcphdr) > MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m = NULL;
			}
		}
		if (m == NULL)
			return NULL;
		MCLAIM(m, &tcp_mowner);
		m->m_pkthdr.len = m->m_len = hlen + sizeof(struct tcphdr);
	}

	memset(mtod(m, void *), 0, m->m_len);

	n = (struct tcphdr *)(mtod(m, char *) + hlen);

	switch (tp->t_family) {
	case AF_INET:
	    {
		struct ipovly *ipov;
		mtod(m, struct ip *)->ip_v = 4;
		mtod(m, struct ip *)->ip_hl = hlen >> 2;
		ipov = mtod(m, struct ipovly *);
		ipov->ih_pr = IPPROTO_TCP;
		ipov->ih_len = htons(sizeof(struct tcphdr));
		if (inp) {
			ipov->ih_src = inp->inp_laddr;
			ipov->ih_dst = inp->inp_faddr;
		}
#ifdef INET6
		else if (in6p) {
			/* mapped addr case */
			bcopy(&in6p->in6p_laddr.s6_addr32[3], &ipov->ih_src,
				sizeof(ipov->ih_src));
			bcopy(&in6p->in6p_faddr.s6_addr32[3], &ipov->ih_dst,
				sizeof(ipov->ih_dst));
		}
#endif
		/*
		 * Compute the pseudo-header portion of the checksum
		 * now.  We incrementally add in the TCP option and
		 * payload lengths later, and then compute the TCP
		 * checksum right before the packet is sent off onto
		 * the wire.
		 */
		n->th_sum = in_cksum_phdr(ipov->ih_src.s_addr,
		    ipov->ih_dst.s_addr,
		    htons(sizeof(struct tcphdr) + IPPROTO_TCP));
		break;
	    }
#ifdef INET6
	case AF_INET6:
	    {
		struct ip6_hdr *ip6;
		mtod(m, struct ip *)->ip_v = 6;
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_src = in6p->in6p_laddr;
		ip6->ip6_dst = in6p->in6p_faddr;
		ip6->ip6_flow = in6p->in6p_flowinfo & IPV6_FLOWINFO_MASK;
		if (ip6_auto_flowlabel) {
			ip6->ip6_flow &= ~IPV6_FLOWLABEL_MASK;
			ip6->ip6_flow |=
			    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
		}
		ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6->ip6_vfc |= IPV6_VERSION;

		/*
		 * Compute the pseudo-header portion of the checksum
		 * now.  We incrementally add in the TCP option and
		 * payload lengths later, and then compute the TCP
		 * checksum right before the packet is sent off onto
		 * the wire.
		 */
		n->th_sum = in6_cksum_phdr(&in6p->in6p_laddr,
		    &in6p->in6p_faddr, htonl(sizeof(struct tcphdr)),
		    htonl(IPPROTO_TCP));
		break;
	    }
#endif
	}
	if (inp) {
		n->th_sport = inp->inp_lport;
		n->th_dport = inp->inp_fport;
	}
#ifdef INET6
	else if (in6p) {
		n->th_sport = in6p->in6p_lport;
		n->th_dport = in6p->in6p_fport;
	}
#endif
	n->th_seq = 0;
	n->th_ack = 0;
	n->th_x2 = 0;
	n->th_off = 5;
	n->th_flags = 0;
	n->th_win = 0;
	n->th_urp = 0;
	return (m);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */
int
tcp_respond(struct tcpcb *tp, struct mbuf *mtemplate, struct mbuf *m,
    struct tcphdr *th0, tcp_seq ack, tcp_seq seq, int flags)
{
	struct route *ro;
	int error, tlen, win = 0;
	int hlen;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	int family;	/* family on packet, not inpcb/in6pcb! */
	struct tcphdr *th;
	struct socket *so;

	if (tp != NULL && (flags & TH_RST) == 0) {
#ifdef DIAGNOSTIC
		if (tp->t_inpcb && tp->t_in6pcb)
			panic("tcp_respond: both t_inpcb and t_in6pcb are set");
#endif
#ifdef INET
		if (tp->t_inpcb)
			win = sbspace(&tp->t_inpcb->inp_socket->so_rcv);
#endif
#ifdef INET6
		if (tp->t_in6pcb)
			win = sbspace(&tp->t_in6pcb->in6p_socket->so_rcv);
#endif
	}

	th = NULL;	/* Quell uninitialized warning */
	ip = NULL;
#ifdef INET6
	ip6 = NULL;
#endif
	if (m == 0) {
		if (!mtemplate)
			return EINVAL;

		/* get family information from template */
		switch (mtod(mtemplate, struct ip *)->ip_v) {
		case 4:
			family = AF_INET;
			hlen = sizeof(struct ip);
			break;
#ifdef INET6
		case 6:
			family = AF_INET6;
			hlen = sizeof(struct ip6_hdr);
			break;
#endif
		default:
			return EAFNOSUPPORT;
		}

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m) {
			MCLAIM(m, &tcp_tx_mowner);
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m = NULL;
			}
		}
		if (m == NULL)
			return (ENOBUFS);

		if (tcp_compat_42)
			tlen = 1;
		else
			tlen = 0;

		m->m_data += max_linkhdr;
		bcopy(mtod(mtemplate, void *), mtod(m, void *),
			mtemplate->m_len);
		switch (family) {
		case AF_INET:
			ip = mtod(m, struct ip *);
			th = (struct tcphdr *)(ip + 1);
			break;
#ifdef INET6
		case AF_INET6:
			ip6 = mtod(m, struct ip6_hdr *);
			th = (struct tcphdr *)(ip6 + 1);
			break;
#endif
#if 0
		default:
			/* noone will visit here */
			m_freem(m);
			return EAFNOSUPPORT;
#endif
		}
		flags = TH_ACK;
	} else {

		if ((m->m_flags & M_PKTHDR) == 0) {
#if 0
			printf("non PKTHDR to tcp_respond\n");
#endif
			m_freem(m);
			return EINVAL;
		}
#ifdef DIAGNOSTIC
		if (!th0)
			panic("th0 == NULL in tcp_respond");
#endif

		/* get family information from m */
		switch (mtod(m, struct ip *)->ip_v) {
		case 4:
			family = AF_INET;
			hlen = sizeof(struct ip);
			ip = mtod(m, struct ip *);
			break;
#ifdef INET6
		case 6:
			family = AF_INET6;
			hlen = sizeof(struct ip6_hdr);
			ip6 = mtod(m, struct ip6_hdr *);
			break;
#endif
		default:
			m_freem(m);
			return EAFNOSUPPORT;
		}
		/* clear h/w csum flags inherited from rx packet */
		m->m_pkthdr.csum_flags = 0;

		if ((flags & TH_SYN) == 0 || sizeof(*th0) > (th0->th_off << 2))
			tlen = sizeof(*th0);
		else
			tlen = th0->th_off << 2;

		if (m->m_len > hlen + tlen && (m->m_flags & M_EXT) == 0 &&
		    mtod(m, char *) + hlen == (char *)th0) {
			m->m_len = hlen + tlen;
			m_freem(m->m_next);
			m->m_next = NULL;
		} else {
			struct mbuf *n;

#ifdef DIAGNOSTIC
			if (max_linkhdr + hlen + tlen > MCLBYTES) {
				m_freem(m);
				return EMSGSIZE;
			}
#endif
			MGETHDR(n, M_DONTWAIT, MT_HEADER);
			if (n && max_linkhdr + hlen + tlen > MHLEN) {
				MCLGET(n, M_DONTWAIT);
				if ((n->m_flags & M_EXT) == 0) {
					m_freem(n);
					n = NULL;
				}
			}
			if (!n) {
				m_freem(m);
				return ENOBUFS;
			}

			MCLAIM(n, &tcp_tx_mowner);
			n->m_data += max_linkhdr;
			n->m_len = hlen + tlen;
			m_copyback(n, 0, hlen, mtod(m, void *));
			m_copyback(n, hlen, tlen, (void *)th0);

			m_freem(m);
			m = n;
			n = NULL;
		}

#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
		switch (family) {
		case AF_INET:
			ip = mtod(m, struct ip *);
			th = (struct tcphdr *)(ip + 1);
			ip->ip_p = IPPROTO_TCP;
			xchg(ip->ip_dst, ip->ip_src, struct in_addr);
			ip->ip_p = IPPROTO_TCP;
			break;
#ifdef INET6
		case AF_INET6:
			ip6 = mtod(m, struct ip6_hdr *);
			th = (struct tcphdr *)(ip6 + 1);
			ip6->ip6_nxt = IPPROTO_TCP;
			xchg(ip6->ip6_dst, ip6->ip6_src, struct in6_addr);
			ip6->ip6_nxt = IPPROTO_TCP;
			break;
#endif
#if 0
		default:
			/* noone will visit here */
			m_freem(m);
			return EAFNOSUPPORT;
#endif
		}
		xchg(th->th_dport, th->th_sport, u_int16_t);
#undef xchg
		tlen = 0;	/*be friendly with the following code*/
	}
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_x2 = 0;
	if ((flags & TH_SYN) == 0) {
		if (tp)
			win >>= tp->rcv_scale;
		if (win > TCP_MAXWIN)
			win = TCP_MAXWIN;
		th->th_win = htons((u_int16_t)win);
		th->th_off = sizeof (struct tcphdr) >> 2;
		tlen += sizeof(*th);
	} else
		tlen += th->th_off << 2;
	m->m_len = hlen + tlen;
	m->m_pkthdr.len = hlen + tlen;
	m->m_pkthdr.rcvif = NULL;
	th->th_flags = flags;
	th->th_urp = 0;

	switch (family) {
#ifdef INET
	case AF_INET:
	    {
		struct ipovly *ipov = (struct ipovly *)ip;
		memset(ipov->ih_x1, 0, sizeof ipov->ih_x1);
		ipov->ih_len = htons((u_int16_t)tlen);

		th->th_sum = 0;
		th->th_sum = in_cksum(m, hlen + tlen);
		ip->ip_len = htons(hlen + tlen);
		ip->ip_ttl = ip_defttl;
		break;
	    }
#endif
#ifdef INET6
	case AF_INET6:
	    {
		th->th_sum = 0;
		th->th_sum = in6_cksum(m, IPPROTO_TCP, sizeof(struct ip6_hdr),
				tlen);
		ip6->ip6_plen = htons(tlen);
		if (tp && tp->t_in6pcb)
			ip6->ip6_hlim = in6_selecthlim_rt(tp->t_in6pcb);
		else
			ip6->ip6_hlim = ip6_defhlim;
		ip6->ip6_flow &= ~IPV6_FLOWINFO_MASK;
		if (ip6_auto_flowlabel) {
			ip6->ip6_flow |=
			    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
		}
		break;
	    }
#endif
	}

	if (tp && tp->t_inpcb)
		so = tp->t_inpcb->inp_socket;
#ifdef INET6
	else if (tp && tp->t_in6pcb)
		so = tp->t_in6pcb->in6p_socket;
#endif
	else
		so = NULL;

	if (tp != NULL && tp->t_inpcb != NULL) {
		ro = &tp->t_inpcb->inp_route;
#ifdef DIAGNOSTIC
		if (family != AF_INET)
			panic("tcp_respond: address family mismatch");
		if (!in_hosteq(ip->ip_dst, tp->t_inpcb->inp_faddr)) {
			panic("tcp_respond: ip_dst %x != inp_faddr %x",
			    ntohl(ip->ip_dst.s_addr),
			    ntohl(tp->t_inpcb->inp_faddr.s_addr));
		}
#endif
	}
#ifdef INET6
	else if (tp != NULL && tp->t_in6pcb != NULL) {
		ro = (struct route *)&tp->t_in6pcb->in6p_route;
#ifdef DIAGNOSTIC
		if (family == AF_INET) {
			if (!IN6_IS_ADDR_V4MAPPED(&tp->t_in6pcb->in6p_faddr))
				panic("tcp_respond: not mapped addr");
			if (memcmp(&ip->ip_dst,
			    &tp->t_in6pcb->in6p_faddr.s6_addr32[3],
			    sizeof(ip->ip_dst)) != 0) {
				panic("tcp_respond: ip_dst != in6p_faddr");
			}
		} else if (family == AF_INET6) {
			if (!IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
			    &tp->t_in6pcb->in6p_faddr))
				panic("tcp_respond: ip6_dst != in6p_faddr");
		} else
			panic("tcp_respond: address family mismatch");
#endif
	}
#endif
	else
		ro = NULL;

	switch (family) {
#ifdef INET
	case AF_INET:
		error = ip_output(m, NULL, ro,
		    (tp && tp->t_mtudisc ? IP_MTUDISC : 0), NULL, so);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = ip6_output(m, NULL, ro, 0, NULL, so, NULL);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		break;
	}

	return (error);
}

/*
 * Template TCPCB.  Rather than zeroing a new TCPCB and initializing
 * a bunch of members individually, we maintain this template for the
 * static and mostly-static components of the TCPCB, and copy it into
 * the new TCPCB instead.
 */
static struct tcpcb tcpcb_template = {
	.t_srtt = TCPTV_SRTTBASE,
	.t_rttmin = TCPTV_MIN,

	.snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT,
	.snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT,
	.snd_numholes = 0,
	.snd_cubic_wmax = 0,
	.snd_cubic_wmax_last = 0,
	.snd_cubic_ctime = 0,

	.t_partialacks = -1,
	.t_bytes_acked = 0,
	.t_sndrexmitpack = 0,
	.t_rcvoopack = 0,
	.t_sndzerowin = 0,
};

/*
 * Updates the TCPCB template whenever a parameter that would affect
 * the template is changed.
 */
void
tcp_tcpcb_template(void)
{
	struct tcpcb *tp = &tcpcb_template;
	int flags;

	tp->t_peermss = tcp_mssdflt;
	tp->t_ourmss = tcp_mssdflt;
	tp->t_segsz = tcp_mssdflt;

	flags = 0;
	if (tcp_do_rfc1323 && tcp_do_win_scale)
		flags |= TF_REQ_SCALE;
	if (tcp_do_rfc1323 && tcp_do_timestamps)
		flags |= TF_REQ_TSTMP;
	tp->t_flags = flags;

	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_rttvar = tcp_rttdflt * PR_SLOWHZ << (TCP_RTTVAR_SHIFT + 2 - 1);
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
	    TCPTV_MIN, TCPTV_REXMTMAX);

	/* Keep Alive */
	tp->t_keepinit = tcp_keepinit;
	tp->t_keepidle = tcp_keepidle;
	tp->t_keepintvl = tcp_keepintvl;
	tp->t_keepcnt = tcp_keepcnt;
	tp->t_maxidle = tp->t_keepcnt * tp->t_keepintvl;

	/* MSL */
	tp->t_msl = TCPTV_MSL;
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 */
/* family selects inpcb, or in6pcb */
struct tcpcb *
tcp_newtcpcb(int family, void *aux)
{
	struct tcpcb *tp;
	int i;

	/* XXX Consider using a pool_cache for speed. */
	tp = pool_get(&tcpcb_pool, PR_NOWAIT);	/* splsoftnet via tcp_usrreq */
	if (tp == NULL)
		return (NULL);
	memcpy(tp, &tcpcb_template, sizeof(*tp));
	TAILQ_INIT(&tp->segq);
	TAILQ_INIT(&tp->timeq);
	tp->t_family = family;		/* may be overridden later on */
	TAILQ_INIT(&tp->snd_holes);
	LIST_INIT(&tp->t_sc);		/* XXX can template this */

	/* Don't sweat this loop; hopefully the compiler will unroll it. */
	for (i = 0; i < TCPT_NTIMERS; i++) {
		callout_init(&tp->t_timer[i], CALLOUT_MPSAFE);
		TCP_TIMER_INIT(tp, i);
	}
	callout_init(&tp->t_delack_ch, CALLOUT_MPSAFE);

	switch (family) {
	case AF_INET:
	    {
		struct inpcb *inp = (struct inpcb *)aux;

		inp->inp_ip.ip_ttl = ip_defttl;
		inp->inp_ppcb = (void *)tp;

		tp->t_inpcb = inp;
		tp->t_mtudisc = ip_mtudisc;
		break;
	    }
#ifdef INET6
	case AF_INET6:
	    {
		struct in6pcb *in6p = (struct in6pcb *)aux;

		in6p->in6p_ip6.ip6_hlim = in6_selecthlim_rt(in6p);
		in6p->in6p_ppcb = (void *)tp;

		tp->t_in6pcb = in6p;
		/* for IPv6, always try to run path MTU discovery */
		tp->t_mtudisc = 1;
		break;
	    }
#endif /* INET6 */
	default:
		for (i = 0; i < TCPT_NTIMERS; i++)
			callout_destroy(&tp->t_timer[i]);
		callout_destroy(&tp->t_delack_ch);
		pool_put(&tcpcb_pool, tp);	/* splsoftnet via tcp_usrreq */
		return (NULL);
	}

	/*
	 * Initialize our timebase.  When we send timestamps, we take
	 * the delta from tcp_now -- this means each connection always
	 * gets a timebase of 1, which makes it, among other things,
	 * more difficult to determine how long a system has been up,
	 * and thus how many TCP sequence increments have occurred.
	 *
	 * We start with 1, because 0 doesn't work with linux, which
	 * considers timestamp 0 in a SYN packet as a bug and disables
	 * timestamps.
	 */
	tp->ts_timebase = tcp_now - 1;
	
	tcp_congctl_select(tp, tcp_congctl_global_name);

	return (tp);
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop(struct tcpcb *tp, int errno)
{
	struct socket *so = NULL;

#ifdef DIAGNOSTIC
	if (tp->t_inpcb && tp->t_in6pcb)
		panic("tcp_drop: both t_inpcb and t_in6pcb are set");
#endif
#ifdef INET
	if (tp->t_inpcb)
		so = tp->t_inpcb->inp_socket;
#endif
#ifdef INET6
	if (tp->t_in6pcb)
		so = tp->t_in6pcb->in6p_socket;
#endif
	if (!so)
		return NULL;

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		(void) tcp_output(tp);
		TCP_STATINC(TCP_STAT_DROPS);
	} else
		TCP_STATINC(TCP_STAT_CONNDROPS);
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */
struct tcpcb *
tcp_close(struct tcpcb *tp)
{
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	struct socket *so;
#ifdef RTV_RTT
	struct rtentry *rt;
#endif
	struct route *ro;
	int j;

	inp = tp->t_inpcb;
#ifdef INET6
	in6p = tp->t_in6pcb;
#endif
	so = NULL;
	ro = NULL;
	if (inp) {
		so = inp->inp_socket;
		ro = &inp->inp_route;
	}
#ifdef INET6
	else if (in6p) {
		so = in6p->in6p_socket;
		ro = (struct route *)&in6p->in6p_route;
	}
#endif

#ifdef RTV_RTT
	/*
	 * If we sent enough data to get some meaningful characteristics,
	 * save them in the routing entry.  'Enough' is arbitrarily
	 * defined as the sendpipesize (default 4K) * 16.  This would
	 * give us 16 rtt samples assuming we only get one sample per
	 * window (the usual case on a long haul net).  16 samples is
	 * enough for the srtt filter to converge to within 5% of the correct
	 * value; fewer samples and we could save a very bogus rtt.
	 *
	 * Don't update the default route's characteristics and don't
	 * update anything that the user "locked".
	 */
	if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
	    ro && (rt = rtcache_validate(ro)) != NULL &&
	    !in_nullhost(satocsin(rt_getkey(rt))->sin_addr)) {
		u_long i = 0;

		if ((rt->rt_rmx.rmx_locks & RTV_RTT) == 0) {
			i = tp->t_srtt *
			    ((RTM_RTTUNIT / PR_SLOWHZ) >> (TCP_RTT_SHIFT + 2));
			if (rt->rt_rmx.rmx_rtt && i)
				/*
				 * filter this update to half the old & half
				 * the new values, converting scale.
				 * See route.h and tcp_var.h for a
				 * description of the scaling constants.
				 */
				rt->rt_rmx.rmx_rtt =
				    (rt->rt_rmx.rmx_rtt + i) / 2;
			else
				rt->rt_rmx.rmx_rtt = i;
		}
		if ((rt->rt_rmx.rmx_locks & RTV_RTTVAR) == 0) {
			i = tp->t_rttvar *
			    ((RTM_RTTUNIT / PR_SLOWHZ) >> (TCP_RTTVAR_SHIFT + 2));
			if (rt->rt_rmx.rmx_rttvar && i)
				rt->rt_rmx.rmx_rttvar =
				    (rt->rt_rmx.rmx_rttvar + i) / 2;
			else
				rt->rt_rmx.rmx_rttvar = i;
		}
		/*
		 * update the pipelimit (ssthresh) if it has been updated
		 * already or if a pipesize was specified & the threshhold
		 * got below half the pipesize.  I.e., wait for bad news
		 * before we start updating, then update on both good
		 * and bad news.
		 */
		if (((rt->rt_rmx.rmx_locks & RTV_SSTHRESH) == 0 &&
		    (i = tp->snd_ssthresh) && rt->rt_rmx.rmx_ssthresh) ||
		    i < (rt->rt_rmx.rmx_sendpipe / 2)) {
			/*
			 * convert the limit from user data bytes to
			 * packets then to packet data bytes.
			 */
			i = (i + tp->t_segsz / 2) / tp->t_segsz;
			if (i < 2)
				i = 2;
			i *= (u_long)(tp->t_segsz + sizeof (struct tcpiphdr));
			if (rt->rt_rmx.rmx_ssthresh)
				rt->rt_rmx.rmx_ssthresh =
				    (rt->rt_rmx.rmx_ssthresh + i) / 2;
			else
				rt->rt_rmx.rmx_ssthresh = i;
		}
	}
#endif /* RTV_RTT */
	/* free the reassembly queue, if any */
	TCP_REASS_LOCK(tp);
	(void) tcp_freeq(tp);
	TCP_REASS_UNLOCK(tp);

	/* free the SACK holes list. */
	tcp_free_sackholes(tp);	
	tcp_congctl_release(tp);
	syn_cache_cleanup(tp);

	if (tp->t_template) {
		m_free(tp->t_template);
		tp->t_template = NULL;
	}

	/*
	 * Detaching the pcb will unlock the socket/tcpcb, and stopping
	 * the timers can also drop the lock.  We need to prevent access
	 * to the tcpcb as it's half torn down.  Flag the pcb as dead
	 * (prevents access by timers) and only then detach it.
	 */
	tp->t_flags |= TF_DEAD;
	if (inp) {
		inp->inp_ppcb = 0;
		soisdisconnected(so);
		in_pcbdetach(inp);
	}
#ifdef INET6
	else if (in6p) {
		in6p->in6p_ppcb = 0;
		soisdisconnected(so);
		in6_pcbdetach(in6p);
	}
#endif
	/*
	 * pcb is no longer visble elsewhere, so we can safely release
	 * the lock in callout_halt() if needed.
	 */
	TCP_STATINC(TCP_STAT_CLOSED);
	for (j = 0; j < TCPT_NTIMERS; j++) {
		callout_halt(&tp->t_timer[j], softnet_lock);
		callout_destroy(&tp->t_timer[j]);
	}
	callout_halt(&tp->t_delack_ch, softnet_lock);
	callout_destroy(&tp->t_delack_ch);
	pool_put(&tcpcb_pool, tp);

	return NULL;
}

int
tcp_freeq(struct tcpcb *tp)
{
	struct ipqent *qe;
	int rv = 0;
#ifdef TCPREASS_DEBUG
	int i = 0;
#endif

	TCP_REASS_LOCK_CHECK(tp);

	while ((qe = TAILQ_FIRST(&tp->segq)) != NULL) {
#ifdef TCPREASS_DEBUG
		printf("tcp_freeq[%p,%d]: %u:%u(%u) 0x%02x\n",
			tp, i++, qe->ipqe_seq, qe->ipqe_seq + qe->ipqe_len,
			qe->ipqe_len, qe->ipqe_flags & (TH_SYN|TH_FIN|TH_RST));
#endif
		TAILQ_REMOVE(&tp->segq, qe, ipqe_q);
		TAILQ_REMOVE(&tp->timeq, qe, ipqe_timeq);
		m_freem(qe->ipqe_m);
		tcpipqent_free(qe);
		rv = 1;
	}
	tp->t_segqlen = 0;
	KASSERT(TAILQ_EMPTY(&tp->timeq));
	return (rv);
}

void
tcp_fasttimo(void)
{
	if (tcp_drainwanted) {
		tcp_drain();
		tcp_drainwanted = 0;
	}
}

void
tcp_drainstub(void)
{
	tcp_drainwanted = 1;
}

/*
 * Protocol drain routine.  Called when memory is in short supply.
 * Called from pr_fasttimo thus a callout context.
 */
void
tcp_drain(void)
{
	struct inpcb_hdr *inph;
	struct tcpcb *tp;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	/*
	 * Free the sequence queue of all TCP connections.
	 */
	TAILQ_FOREACH(inph, &tcbtable.inpt_queue, inph_queue) {
		switch (inph->inph_af) {
		case AF_INET:
			tp = intotcpcb((struct inpcb *)inph);
			break;
#ifdef INET6
		case AF_INET6:
			tp = in6totcpcb((struct in6pcb *)inph);
			break;
#endif
		default:
			tp = NULL;
			break;
		}
		if (tp != NULL) {
			/*
			 * We may be called from a device's interrupt
			 * context.  If the tcpcb is already busy,
			 * just bail out now.
			 */
			if (tcp_reass_lock_try(tp) == 0)
				continue;
			if (tcp_freeq(tp))
				TCP_STATINC(TCP_STAT_CONNSDRAINED);
			TCP_REASS_UNLOCK(tp);
		}
	}

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 */
void
tcp_notify(struct inpcb *inp, int error)
{
	struct tcpcb *tp = (struct tcpcb *)inp->inp_ppcb;
	struct socket *so = inp->inp_socket;

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	     (error == EHOSTUNREACH || error == ENETUNREACH ||
	      error == EHOSTDOWN)) {
		return;
	} else if (TCPS_HAVEESTABLISHED(tp->t_state) == 0 &&
	    tp->t_rxtshift > 3 && tp->t_softerror)
		so->so_error = error;
	else
		tp->t_softerror = error;
	cv_broadcast(&so->so_cv);
	sorwakeup(so);
	sowwakeup(so);
}

#ifdef INET6
void
tcp6_notify(struct in6pcb *in6p, int error)
{
	struct tcpcb *tp = (struct tcpcb *)in6p->in6p_ppcb;
	struct socket *so = in6p->in6p_socket;

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	     (error == EHOSTUNREACH || error == ENETUNREACH ||
	      error == EHOSTDOWN)) {
		return;
	} else if (TCPS_HAVEESTABLISHED(tp->t_state) == 0 &&
	    tp->t_rxtshift > 3 && tp->t_softerror)
		so->so_error = error;
	else
		tp->t_softerror = error;
	cv_broadcast(&so->so_cv);
	sorwakeup(so);
	sowwakeup(so);
}
#endif

#ifdef INET6
void *
tcp6_ctlinput(int cmd, const struct sockaddr *sa, void *d)
{
	struct tcphdr th;
	void (*notify)(struct in6pcb *, int) = tcp6_notify;
	int nmatch;
	struct ip6_hdr *ip6;
	const struct sockaddr_in6 *sa6_src = NULL;
	const struct sockaddr_in6 *sa6 = (const struct sockaddr_in6 *)sa;
	struct mbuf *m;
	int off;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return NULL;
	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	else if (cmd == PRC_QUENCH) {
		/* 
		 * Don't honor ICMP Source Quench messages meant for
		 * TCP connections.
		 */
		return NULL;
	} else if (PRC_IS_REDIRECT(cmd))
		notify = in6_rtchange, d = NULL;
	else if (cmd == PRC_MSGSIZE)
		; /* special code is present, see below */
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (inet6ctlerrmap[cmd] == 0)
		return NULL;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		struct ip6ctlparam *ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		sa6_src = ip6cp->ip6c_src;
	} else {
		m = NULL;
		ip6 = NULL;
		sa6_src = &sa6_any;
		off = 0;
	}

	if (ip6) {
		/*
		 * XXX: We assume that when ip6 is non NULL,
		 * M and OFF are valid.
		 */

		/* check if we can safely examine src and dst ports */
		if (m->m_pkthdr.len < off + sizeof(th)) {
			if (cmd == PRC_MSGSIZE)
				icmp6_mtudisc_update((struct ip6ctlparam *)d, 0);
			return NULL;
		}

		memset(&th, 0, sizeof(th));
		m_copydata(m, off, sizeof(th), (void *)&th);

		if (cmd == PRC_MSGSIZE) {
			int valid = 0;

			/*
			 * Check to see if we have a valid TCP connection
			 * corresponding to the address in the ICMPv6 message
			 * payload.
			 */
			if (in6_pcblookup_connect(&tcbtable, &sa6->sin6_addr,
			    th.th_dport,
			    (const struct in6_addr *)&sa6_src->sin6_addr,
						  th.th_sport, 0, 0))
				valid++;

			/*
			 * Depending on the value of "valid" and routing table
			 * size (mtudisc_{hi,lo}wat), we will:
			 * - recalcurate the new MTU and create the
			 *   corresponding routing entry, or
			 * - ignore the MTU change notification.
			 */
			icmp6_mtudisc_update((struct ip6ctlparam *)d, valid);

			/*
			 * no need to call in6_pcbnotify, it should have been
			 * called via callback if necessary
			 */
			return NULL;
		}

		nmatch = in6_pcbnotify(&tcbtable, sa, th.th_dport,
		    (const struct sockaddr *)sa6_src, th.th_sport, cmd, NULL, notify);
		if (nmatch == 0 && syn_cache_count &&
		    (inet6ctlerrmap[cmd] == EHOSTUNREACH ||
		     inet6ctlerrmap[cmd] == ENETUNREACH ||
		     inet6ctlerrmap[cmd] == EHOSTDOWN))
			syn_cache_unreach((const struct sockaddr *)sa6_src,
					  sa, &th);
	} else {
		(void) in6_pcbnotify(&tcbtable, sa, 0,
		    (const struct sockaddr *)sa6_src, 0, cmd, NULL, notify);
	}

	return NULL;
}
#endif

#ifdef INET
/* assumes that ip header and tcp header are contiguous on mbuf */
void *
tcp_ctlinput(int cmd, const struct sockaddr *sa, void *v)
{
	struct ip *ip = v;
	struct tcphdr *th;
	struct icmp *icp;
	extern const int inetctlerrmap[];
	void (*notify)(struct inpcb *, int) = tcp_notify;
	int errno;
	int nmatch;
	struct tcpcb *tp;
	u_int mtu;
	tcp_seq seq;
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
	struct in6_addr src6, dst6;
#endif

	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return NULL;
	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	errno = inetctlerrmap[cmd];
	if (cmd == PRC_QUENCH)
		/* 
		 * Don't honor ICMP Source Quench messages meant for
		 * TCP connections.
		 */
		return NULL;
	else if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, ip = 0;
	else if (cmd == PRC_MSGSIZE && ip && ip->ip_v == 4) {
		/*
		 * Check to see if we have a valid TCP connection
		 * corresponding to the address in the ICMP message
		 * payload.
		 *
		 * Boundary check is made in icmp_input(), with ICMP_ADVLENMIN.
		 */
		th = (struct tcphdr *)((char *)ip + (ip->ip_hl << 2));
#ifdef INET6
		memset(&src6, 0, sizeof(src6));
		memset(&dst6, 0, sizeof(dst6));
		src6.s6_addr16[5] = dst6.s6_addr16[5] = 0xffff;
		memcpy(&src6.s6_addr32[3], &ip->ip_src, sizeof(struct in_addr));
		memcpy(&dst6.s6_addr32[3], &ip->ip_dst, sizeof(struct in_addr));
#endif
		if ((inp = in_pcblookup_connect(&tcbtable, ip->ip_dst,
						th->th_dport, ip->ip_src, th->th_sport, 0)) != NULL)
#ifdef INET6
			in6p = NULL;
#else
			;
#endif
#ifdef INET6
		else if ((in6p = in6_pcblookup_connect(&tcbtable, &dst6,
						       th->th_dport, &src6, th->th_sport, 0, 0)) != NULL)
			;
#endif
		else
			return NULL;

		/*
		 * Now that we've validated that we are actually communicating
		 * with the host indicated in the ICMP message, locate the
		 * ICMP header, recalculate the new MTU, and create the
		 * corresponding routing entry.
		 */
		icp = (struct icmp *)((char *)ip -
		    offsetof(struct icmp, icmp_ip));
		if (inp) {
			if ((tp = intotcpcb(inp)) == NULL)
				return NULL;
		}
#ifdef INET6
		else if (in6p) {
			if ((tp = in6totcpcb(in6p)) == NULL)
				return NULL;
		}
#endif
		else
			return NULL;
		seq = ntohl(th->th_seq);
		if (SEQ_LT(seq, tp->snd_una) || SEQ_GT(seq, tp->snd_max))
			return NULL;
		/* 
		 * If the ICMP message advertises a Next-Hop MTU
		 * equal or larger than the maximum packet size we have
		 * ever sent, drop the message.
		 */
		mtu = (u_int)ntohs(icp->icmp_nextmtu);
		if (mtu >= tp->t_pmtud_mtu_sent)
			return NULL;
		if (mtu >= tcp_hdrsz(tp) + tp->t_pmtud_mss_acked) {
			/* 
			 * Calculate new MTU, and create corresponding
			 * route (traditional PMTUD).
			 */
			tp->t_flags &= ~TF_PMTUD_PEND;
			icmp_mtudisc(icp, ip->ip_dst);
		} else {
			/*
			 * Record the information got in the ICMP
			 * message; act on it later.
			 * If we had already recorded an ICMP message,
			 * replace the old one only if the new message
			 * refers to an older TCP segment
			 */
			if (tp->t_flags & TF_PMTUD_PEND) {
				if (SEQ_LT(tp->t_pmtud_th_seq, seq))
					return NULL;
			} else
				tp->t_flags |= TF_PMTUD_PEND;
			tp->t_pmtud_th_seq = seq;
			tp->t_pmtud_nextmtu = icp->icmp_nextmtu;
			tp->t_pmtud_ip_len = icp->icmp_ip.ip_len;
			tp->t_pmtud_ip_hl = icp->icmp_ip.ip_hl;
		}
		return NULL;
	} else if (cmd == PRC_HOSTDEAD)
		ip = 0;
	else if (errno == 0)
		return NULL;
	if (ip && ip->ip_v == 4 && sa->sa_family == AF_INET) {
		th = (struct tcphdr *)((char *)ip + (ip->ip_hl << 2));
		nmatch = in_pcbnotify(&tcbtable, satocsin(sa)->sin_addr,
		    th->th_dport, ip->ip_src, th->th_sport, errno, notify);
		if (nmatch == 0 && syn_cache_count &&
		    (inetctlerrmap[cmd] == EHOSTUNREACH ||
		    inetctlerrmap[cmd] == ENETUNREACH ||
		    inetctlerrmap[cmd] == EHOSTDOWN)) {
			struct sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_len = sizeof(sin);
			sin.sin_family = AF_INET;
			sin.sin_port = th->th_sport;
			sin.sin_addr = ip->ip_src;
			syn_cache_unreach((struct sockaddr *)&sin, sa, th);
		}

		/* XXX mapped address case */
	} else
		in_pcbnotifyall(&tcbtable, satocsin(sa)->sin_addr, errno,
		    notify);
	return NULL;
}

/*
 * When a source quench is received, we are being notified of congestion.
 * Close the congestion window down to the Loss Window (one segment).
 * We will gradually open it again as we proceed.
 */
void
tcp_quench(struct inpcb *inp, int errno)
{
	struct tcpcb *tp = intotcpcb(inp);

	if (tp) {
		tp->snd_cwnd = tp->t_segsz;
		tp->t_bytes_acked = 0;
	}
}
#endif

#ifdef INET6
void
tcp6_quench(struct in6pcb *in6p, int errno)
{
	struct tcpcb *tp = in6totcpcb(in6p);

	if (tp) {
		tp->snd_cwnd = tp->t_segsz;
		tp->t_bytes_acked = 0;
	}
}
#endif

#ifdef INET
/*
 * Path MTU Discovery handlers.
 */
void
tcp_mtudisc_callback(struct in_addr faddr)
{
#ifdef INET6
	struct in6_addr in6;
#endif

	in_pcbnotifyall(&tcbtable, faddr, EMSGSIZE, tcp_mtudisc);
#ifdef INET6
	memset(&in6, 0, sizeof(in6));
	in6.s6_addr16[5] = 0xffff;
	memcpy(&in6.s6_addr32[3], &faddr, sizeof(struct in_addr));
	tcp6_mtudisc_callback(&in6);
#endif
}

/*
 * On receipt of path MTU corrections, flush old route and replace it
 * with the new one.  Retransmit all unacknowledged packets, to ensure
 * that all packets will be received.
 */
void
tcp_mtudisc(struct inpcb *inp, int errno)
{
	struct tcpcb *tp = intotcpcb(inp);
	struct rtentry *rt;

	if (tp == NULL)
		return;

	rt = in_pcbrtentry(inp);
	if (rt != NULL) {
		/*
		 * If this was not a host route, remove and realloc.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0) {
			in_rtchange(inp, errno);
			if ((rt = in_pcbrtentry(inp)) == NULL)
				return;
		}

		/*
		 * Slow start out of the error condition.  We
		 * use the MTU because we know it's smaller
		 * than the previously transmitted segment.
		 *
		 * Note: This is more conservative than the
		 * suggestion in draft-floyd-incr-init-win-03.
		 */
		if (rt->rt_rmx.rmx_mtu != 0)
			tp->snd_cwnd =
			    TCP_INITIAL_WINDOW(tcp_init_win,
			    rt->rt_rmx.rmx_mtu);
	}

	/*
	 * Resend unacknowledged packets.
	 */
	tp->snd_nxt = tp->sack_newdata = tp->snd_una;
	tcp_output(tp);
}
#endif /* INET */

#ifdef INET6
/*
 * Path MTU Discovery handlers.
 */
void
tcp6_mtudisc_callback(struct in6_addr *faddr)
{
	struct sockaddr_in6 sin6;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *faddr;
	(void) in6_pcbnotify(&tcbtable, (struct sockaddr *)&sin6, 0,
	    (const struct sockaddr *)&sa6_any, 0, PRC_MSGSIZE, NULL, tcp6_mtudisc);
}

void
tcp6_mtudisc(struct in6pcb *in6p, int errno)
{
	struct tcpcb *tp = in6totcpcb(in6p);
	struct rtentry *rt = in6_pcbrtentry(in6p);

	if (tp != 0) {
		if (rt != 0) {
			/*
			 * If this was not a host route, remove and realloc.
			 */
			if ((rt->rt_flags & RTF_HOST) == 0) {
				in6_rtchange(in6p, errno);
				if ((rt = in6_pcbrtentry(in6p)) == 0)
					return;
			}

			/*
			 * Slow start out of the error condition.  We
			 * use the MTU because we know it's smaller
			 * than the previously transmitted segment.
			 *
			 * Note: This is more conservative than the
			 * suggestion in draft-floyd-incr-init-win-03.
			 */
			if (rt->rt_rmx.rmx_mtu != 0)
				tp->snd_cwnd =
				    TCP_INITIAL_WINDOW(tcp_init_win,
				    rt->rt_rmx.rmx_mtu);
		}

		/*
		 * Resend unacknowledged packets.
		 */
		tp->snd_nxt = tp->sack_newdata = tp->snd_una;
		tcp_output(tp);
	}
}
#endif /* INET6 */

/*
 * Compute the MSS to advertise to the peer.  Called only during
 * the 3-way handshake.  If we are the server (peer initiated
 * connection), we are called with a pointer to the interface
 * on which the SYN packet arrived.  If we are the client (we
 * initiated connection), we are called with a pointer to the
 * interface out which this connection should go.
 *
 * NOTE: Do not subtract IP option/extension header size nor IPsec
 * header size from MSS advertisement.  MSS option must hold the maximum
 * segment size we can accept, so it must always be:
 *	 max(if mtu) - ip header - tcp header
 */
u_long
tcp_mss_to_advertise(const struct ifnet *ifp, int af)
{
	extern u_long in_maxmtu;
	u_long mss = 0;
	u_long hdrsiz;

	/*
	 * In order to avoid defeating path MTU discovery on the peer,
	 * we advertise the max MTU of all attached networks as our MSS,
	 * per RFC 1191, section 3.1.
	 *
	 * We provide the option to advertise just the MTU of
	 * the interface on which we hope this connection will
	 * be receiving.  If we are responding to a SYN, we
	 * will have a pretty good idea about this, but when
	 * initiating a connection there is a bit more doubt.
	 *
	 * We also need to ensure that loopback has a large enough
	 * MSS, as the loopback MTU is never included in in_maxmtu.
	 */

	if (ifp != NULL)
		switch (af) {
		case AF_INET:
			mss = ifp->if_mtu;
			break;
#ifdef INET6
		case AF_INET6:
			mss = IN6_LINKMTU(ifp);
			break;
#endif
		}

	if (tcp_mss_ifmtu == 0)
		switch (af) {
		case AF_INET:
			mss = max(in_maxmtu, mss);
			break;
#ifdef INET6
		case AF_INET6:
			mss = max(in6_maxmtu, mss);
			break;
#endif
		}

	switch (af) {
	case AF_INET:
		hdrsiz = sizeof(struct ip);
		break;
#ifdef INET6
	case AF_INET6:
		hdrsiz = sizeof(struct ip6_hdr);
		break;
#endif
	default:
		hdrsiz = 0;
		break;
	}
	hdrsiz += sizeof(struct tcphdr);
	if (mss > hdrsiz)
		mss -= hdrsiz;

	mss = max(tcp_mssdflt, mss);
	return (mss);
}

/*
 * Set connection variables based on the peer's advertised MSS.
 * We are passed the TCPCB for the actual connection.  If we
 * are the server, we are called by the compressed state engine
 * when the 3-way handshake is complete.  If we are the client,
 * we are called when we receive the SYN,ACK from the server.
 *
 * NOTE: Our advertised MSS value must be initialized in the TCPCB
 * before this routine is called!
 */
void
tcp_mss_from_peer(struct tcpcb *tp, int offer)
{
	struct socket *so;
#if defined(RTV_SPIPE) || defined(RTV_SSTHRESH)
	struct rtentry *rt;
#endif
	u_long bufsize;
	int mss;

#ifdef DIAGNOSTIC
	if (tp->t_inpcb && tp->t_in6pcb)
		panic("tcp_mss_from_peer: both t_inpcb and t_in6pcb are set");
#endif
	so = NULL;
	rt = NULL;
#ifdef INET
	if (tp->t_inpcb) {
		so = tp->t_inpcb->inp_socket;
#if defined(RTV_SPIPE) || defined(RTV_SSTHRESH)
		rt = in_pcbrtentry(tp->t_inpcb);
#endif
	}
#endif
#ifdef INET6
	if (tp->t_in6pcb) {
		so = tp->t_in6pcb->in6p_socket;
#if defined(RTV_SPIPE) || defined(RTV_SSTHRESH)
		rt = in6_pcbrtentry(tp->t_in6pcb);
#endif
	}
#endif

	/*
	 * As per RFC1122, use the default MSS value, unless they
	 * sent us an offer.  Do not accept offers less than 256 bytes.
	 */
	mss = tcp_mssdflt;
	if (offer)
		mss = offer;
	mss = max(mss, 256);		/* sanity */
	tp->t_peermss = mss;
	mss -= tcp_optlen(tp);
#ifdef INET
	if (tp->t_inpcb)
		mss -= ip_optlen(tp->t_inpcb);
#endif
#ifdef INET6
	if (tp->t_in6pcb)
		mss -= ip6_optlen(tp->t_in6pcb);
#endif

	/*
	 * If there's a pipesize, change the socket buffer to that size.
	 * Make the socket buffer an integral number of MSS units.  If
	 * the MSS is larger than the socket buffer, artificially decrease
	 * the MSS.
	 */
#ifdef RTV_SPIPE
	if (rt != NULL && rt->rt_rmx.rmx_sendpipe != 0)
		bufsize = rt->rt_rmx.rmx_sendpipe;
	else
#endif
	{
		KASSERT(so != NULL);
		bufsize = so->so_snd.sb_hiwat;
	}
	if (bufsize < mss)
		mss = bufsize;
	else {
		bufsize = roundup(bufsize, mss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		(void) sbreserve(&so->so_snd, bufsize, so);
	}
	tp->t_segsz = mss;

#ifdef RTV_SSTHRESH
	if (rt != NULL && rt->rt_rmx.rmx_ssthresh) {
		/*
		 * There's some sort of gateway or interface buffer
		 * limit on the path.  Use this to set the slow
		 * start threshold, but set the threshold to no less
		 * than 2 * MSS.
		 */
		tp->snd_ssthresh = max(2 * mss, rt->rt_rmx.rmx_ssthresh);
	}
#endif
}

/*
 * Processing necessary when a TCP connection is established.
 */
void
tcp_established(struct tcpcb *tp)
{
	struct socket *so;
#ifdef RTV_RPIPE
	struct rtentry *rt;
#endif
	u_long bufsize;

#ifdef DIAGNOSTIC
	if (tp->t_inpcb && tp->t_in6pcb)
		panic("tcp_established: both t_inpcb and t_in6pcb are set");
#endif
	so = NULL;
	rt = NULL;
#ifdef INET
	/* This is a while() to reduce the dreadful stairstepping below */
	while (tp->t_inpcb) {
		so = tp->t_inpcb->inp_socket;
#if defined(RTV_RPIPE)
		rt = in_pcbrtentry(tp->t_inpcb);
#endif
		if (__predict_true(tcp_msl_enable)) {
			if (tp->t_inpcb->inp_laddr.s_addr == INADDR_LOOPBACK) {
				tp->t_msl = tcp_msl_loop ? tcp_msl_loop : (TCPTV_MSL >> 2);
				break;
			}

			if (__predict_false(tcp_rttlocal)) {
				/* This may be adjusted by tcp_input */
				tp->t_msl = tcp_msl_local ? tcp_msl_local : (TCPTV_MSL >> 1);
				break;
			}
			if (in_localaddr(tp->t_inpcb->inp_faddr)) {
				tp->t_msl = tcp_msl_local ? tcp_msl_local : (TCPTV_MSL >> 1);
				break;
			}
		}
		tp->t_msl = tcp_msl_remote ? tcp_msl_remote : TCPTV_MSL;
		break;
	}
#endif
#ifdef INET6
	/* The !tp->t_inpcb lets the compiler know it can't be v4 *and* v6 */
	while (!tp->t_inpcb && tp->t_in6pcb) {
		so = tp->t_in6pcb->in6p_socket;
#if defined(RTV_RPIPE)
		rt = in6_pcbrtentry(tp->t_in6pcb);
#endif
		if (__predict_true(tcp_msl_enable)) {
			extern const struct in6_addr in6addr_loopback;
		    
			if (IN6_ARE_ADDR_EQUAL(&tp->t_in6pcb->in6p_laddr,
					       &in6addr_loopback)) {
				tp->t_msl = tcp_msl_loop ? tcp_msl_loop : (TCPTV_MSL >> 2);
				break;
			}

			if (__predict_false(tcp_rttlocal)) {
				/* This may be adjusted by tcp_input */
				tp->t_msl = tcp_msl_local ? tcp_msl_local : (TCPTV_MSL >> 1);
				break;
			}
			if (in6_localaddr(&tp->t_in6pcb->in6p_faddr)) {
				tp->t_msl = tcp_msl_local ? tcp_msl_local : (TCPTV_MSL >> 1);
				break;
			}
		}
		tp->t_msl = tcp_msl_remote ? tcp_msl_remote : TCPTV_MSL;
		break;
	}
#endif

	tp->t_state = TCPS_ESTABLISHED;
	TCP_TIMER_ARM(tp, TCPT_KEEP, tp->t_keepidle);

#ifdef RTV_RPIPE
	if (rt != NULL && rt->rt_rmx.rmx_recvpipe != 0)
		bufsize = rt->rt_rmx.rmx_recvpipe;
	else
#endif
	{
		KASSERT(so != NULL);
		bufsize = so->so_rcv.sb_hiwat;
	}
	if (bufsize > tp->t_ourmss) {
		bufsize = roundup(bufsize, tp->t_ourmss);
		if (bufsize > sb_max)
			bufsize = sb_max;
		(void) sbreserve(&so->so_rcv, bufsize, so);
	}
}

/*
 * Check if there's an initial rtt or rttvar.  Convert from the
 * route-table units to scaled multiples of the slow timeout timer.
 * Called only during the 3-way handshake.
 */
void
tcp_rmx_rtt(struct tcpcb *tp)
{
#ifdef RTV_RTT
	struct rtentry *rt = NULL;
	int rtt;

#ifdef DIAGNOSTIC
	if (tp->t_inpcb && tp->t_in6pcb)
		panic("tcp_rmx_rtt: both t_inpcb and t_in6pcb are set");
#endif
#ifdef INET
	if (tp->t_inpcb)
		rt = in_pcbrtentry(tp->t_inpcb);
#endif
#ifdef INET6
	if (tp->t_in6pcb)
		rt = in6_pcbrtentry(tp->t_in6pcb);
#endif
	if (rt == NULL)
		return;

	if (tp->t_srtt == 0 && (rtt = rt->rt_rmx.rmx_rtt)) {
		/*
		 * XXX The lock bit for MTU indicates that the value
		 * is also a minimum value; this is subject to time.
		 */
		if (rt->rt_rmx.rmx_locks & RTV_RTT)
			TCPT_RANGESET(tp->t_rttmin,
			    rtt / (RTM_RTTUNIT / PR_SLOWHZ),
			    TCPTV_MIN, TCPTV_REXMTMAX);
		tp->t_srtt = rtt /
		    ((RTM_RTTUNIT / PR_SLOWHZ) >> (TCP_RTT_SHIFT + 2));
		if (rt->rt_rmx.rmx_rttvar) {
			tp->t_rttvar = rt->rt_rmx.rmx_rttvar /
			    ((RTM_RTTUNIT / PR_SLOWHZ) >>
				(TCP_RTTVAR_SHIFT + 2));
		} else {
			/* Default variation is +- 1 rtt */
			tp->t_rttvar =
			    tp->t_srtt >> (TCP_RTT_SHIFT - TCP_RTTVAR_SHIFT);
		}
		TCPT_RANGESET(tp->t_rxtcur,
		    ((tp->t_srtt >> 2) + tp->t_rttvar) >> (1 + 2),
		    tp->t_rttmin, TCPTV_REXMTMAX);
	}
#endif
}

tcp_seq	 tcp_iss_seq = 0;	/* tcp initial seq # */

/*
 * Get a new sequence value given a tcp control block
 */
tcp_seq
tcp_new_iss(struct tcpcb *tp, tcp_seq addin)
{

#ifdef INET
	if (tp->t_inpcb != NULL) {
		return (tcp_new_iss1(&tp->t_inpcb->inp_laddr,
		    &tp->t_inpcb->inp_faddr, tp->t_inpcb->inp_lport,
		    tp->t_inpcb->inp_fport, sizeof(tp->t_inpcb->inp_laddr),
		    addin));
	}
#endif
#ifdef INET6
	if (tp->t_in6pcb != NULL) {
		return (tcp_new_iss1(&tp->t_in6pcb->in6p_laddr,
		    &tp->t_in6pcb->in6p_faddr, tp->t_in6pcb->in6p_lport,
		    tp->t_in6pcb->in6p_fport, sizeof(tp->t_in6pcb->in6p_laddr),
		    addin));
	}
#endif
	/* Not possible. */
	panic("tcp_new_iss");
}

static u_int8_t tcp_iss_secret[16];	/* 128 bits; should be plenty */

/*
 * Initialize RFC 1948 ISS Secret
 */
static int
tcp_iss_secret_init(void)
{
	cprng_strong(kern_cprng,
	    tcp_iss_secret, sizeof(tcp_iss_secret), 0);

	return 0;
}

/*
 * This routine actually generates a new TCP initial sequence number.
 */
tcp_seq
tcp_new_iss1(void *laddr, void *faddr, u_int16_t lport, u_int16_t fport,
    size_t addrsz, tcp_seq addin)
{
	tcp_seq tcp_iss;

	if (tcp_do_rfc1948) {
		MD5_CTX ctx;
		u_int8_t hash[16];	/* XXX MD5 knowledge */
		static ONCE_DECL(tcp_iss_secret_control);

		/*
		 * If we haven't been here before, initialize our cryptographic
		 * hash secret.
		 */
		RUN_ONCE(&tcp_iss_secret_control, tcp_iss_secret_init);

		/*
		 * Compute the base value of the ISS.  It is a hash
		 * of (saddr, sport, daddr, dport, secret).
		 */
		MD5Init(&ctx);

		MD5Update(&ctx, (u_char *) laddr, addrsz);
		MD5Update(&ctx, (u_char *) &lport, sizeof(lport));

		MD5Update(&ctx, (u_char *) faddr, addrsz);
		MD5Update(&ctx, (u_char *) &fport, sizeof(fport));

		MD5Update(&ctx, tcp_iss_secret, sizeof(tcp_iss_secret));

		MD5Final(hash, &ctx);

		memcpy(&tcp_iss, hash, sizeof(tcp_iss));

		/*
		 * Now increment our "timer", and add it in to
		 * the computed value.
		 *
		 * XXX Use `addin'?
		 * XXX TCP_ISSINCR too large to use?
		 */
		tcp_iss_seq += TCP_ISSINCR;
#ifdef TCPISS_DEBUG
		printf("ISS hash 0x%08x, ", tcp_iss);
#endif
		tcp_iss += tcp_iss_seq + addin;
#ifdef TCPISS_DEBUG
		printf("new ISS 0x%08x\n", tcp_iss);
#endif
	} else {
		/*
		 * Randomize.
		 */
		tcp_iss = cprng_fast32();

		/*
		 * If we were asked to add some amount to a known value,
		 * we will take a random value obtained above, mask off
		 * the upper bits, and add in the known value.  We also
		 * add in a constant to ensure that we are at least a
		 * certain distance from the original value.
		 *
		 * This is used when an old connection is in timed wait
		 * and we have a new one coming in, for instance.
		 */
		if (addin != 0) {
#ifdef TCPISS_DEBUG
			printf("Random %08x, ", tcp_iss);
#endif
			tcp_iss &= TCP_ISS_RANDOM_MASK;
			tcp_iss += addin + TCP_ISSINCR;
#ifdef TCPISS_DEBUG
			printf("Old ISS %08x, ISS %08x\n", addin, tcp_iss);
#endif
		} else {
			tcp_iss &= TCP_ISS_RANDOM_MASK;
			tcp_iss += tcp_iss_seq;
			tcp_iss_seq += TCP_ISSINCR;
#ifdef TCPISS_DEBUG
			printf("ISS %08x\n", tcp_iss);
#endif
		}
	}

	if (tcp_compat_42) {
		/*
		 * Limit it to the positive range for really old TCP
		 * implementations.
		 * Just AND off the top bit instead of checking if
		 * is set first - saves a branch 50% of the time.
		 */
		tcp_iss &= 0x7fffffff;		/* XXX */
	}

	return (tcp_iss);
}

#if defined(IPSEC)
/* compute ESP/AH header size for TCP, including outer IP header. */
size_t
ipsec4_hdrsiz_tcp(struct tcpcb *tp)
{
	struct inpcb *inp;
	size_t hdrsiz;

	/* XXX mapped addr case (tp->t_in6pcb) */
	if (!tp || !tp->t_template || !(inp = tp->t_inpcb))
		return 0;
	switch (tp->t_family) {
	case AF_INET:
		/* XXX: should use currect direction. */
		hdrsiz = ipsec4_hdrsiz(tp->t_template, IPSEC_DIR_OUTBOUND, inp);
		break;
	default:
		hdrsiz = 0;
		break;
	}

	return hdrsiz;
}

#ifdef INET6
size_t
ipsec6_hdrsiz_tcp(struct tcpcb *tp)
{
	struct in6pcb *in6p;
	size_t hdrsiz;

	if (!tp || !tp->t_template || !(in6p = tp->t_in6pcb))
		return 0;
	switch (tp->t_family) {
	case AF_INET6:
		/* XXX: should use currect direction. */
		hdrsiz = ipsec6_hdrsiz(tp->t_template, IPSEC_DIR_OUTBOUND, in6p);
		break;
	case AF_INET:
		/* mapped address case - tricky */
	default:
		hdrsiz = 0;
		break;
	}

	return hdrsiz;
}
#endif
#endif /*IPSEC*/

/*
 * Determine the length of the TCP options for this connection.
 *
 * XXX:  What do we do for SACK, when we add that?  Just reserve
 *       all of the space?  Otherwise we can't exactly be incrementing
 *       cwnd by an amount that varies depending on the amount we last
 *       had to SACK!
 */

u_int
tcp_optlen(struct tcpcb *tp)
{
	u_int optlen;

	optlen = 0;
	if ((tp->t_flags & (TF_REQ_TSTMP|TF_RCVD_TSTMP|TF_NOOPT)) ==
	    (TF_REQ_TSTMP | TF_RCVD_TSTMP))
		optlen += TCPOLEN_TSTAMP_APPA;

#ifdef TCP_SIGNATURE
	if (tp->t_flags & TF_SIGNATURE)
		optlen += TCPOLEN_SIGNATURE + 2;
#endif /* TCP_SIGNATURE */

	return optlen;
}

u_int
tcp_hdrsz(struct tcpcb *tp)
{
	u_int hlen;

	switch (tp->t_family) {
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif
	case AF_INET:
		hlen = sizeof(struct ip);
		break;
	default:
		hlen = 0;
		break;
	}
	hlen += sizeof(struct tcphdr);

	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	    (tp->t_flags & TF_RCVD_TSTMP) == TF_RCVD_TSTMP)
		hlen += TCPOLEN_TSTAMP_APPA;
#ifdef TCP_SIGNATURE
	if (tp->t_flags & TF_SIGNATURE)
		hlen += TCPOLEN_SIGLEN;
#endif
	return hlen;
}

void
tcp_statinc(u_int stat)
{

	KASSERT(stat < TCP_NSTATS);
	TCP_STATINC(stat);
}

void
tcp_statadd(u_int stat, uint64_t val)
{

	KASSERT(stat < TCP_NSTATS);
	TCP_STATADD(stat, val);
}
