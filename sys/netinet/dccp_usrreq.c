/*	$KAME: dccp_usrreq.c,v 1.67 2005/11/03 16:05:04 nishida Exp $	*/
/*	$NetBSD: dccp_usrreq.c,v 1.7 2015/08/24 22:21:26 pooka Exp $ */

/*
 * Copyright (c) 2003 Joacim Häggmark, Magnus Erixzon, Nils-Erik Mattsson 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Id: dccp_usrreq.c,v 1.47 2003/07/31 11:23:08 joahag-9 Exp
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)udp_usrreq.c	8.6 (Berkeley) 5/23/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dccp_usrreq.c,v 1.7 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_dccp.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/dccp6_var.h>
#endif
#include <netinet/dccp.h>
#include <netinet/dccp_var.h>
#include <netinet/dccp_cc_sw.h>

#define DEFAULT_CCID 2

#define INP_INFO_LOCK_INIT(x,y)
#define INP_INFO_WLOCK(x)
#define INP_INFO_WUNLOCK(x)
#define INP_INFO_RLOCK(x)
#define INP_INFO_RUNLOCK(x)
#define INP_LOCK(x)
#define IN6P_LOCK(x)
#define INP_UNLOCK(x)
#define IN6P_UNLOCK(x)

/* Congestion control switch table */
extern struct dccp_cc_sw cc_sw[];

int	dccp_log_in_vain = 1;
int	dccp_do_feature_nego = 1;

struct	inpcbhead dccpb;		/* from dccp_var.h */
#ifdef __FreeBSD__
struct	inpcbinfo dccpbinfo;
#else
struct	inpcbtable dccpbtable;
#endif

#ifndef DCCPBHASHSIZE
#define DCCPBHASHSIZE 16
#endif

struct	pool dccpcb_pool;

u_long	dccp_sendspace = 32768;
u_long	dccp_recvspace = 65536;

struct	dccpstat dccpstat;	/* from dccp_var.h */

static struct dccpcb * dccp_close(struct dccpcb *);
static int dccp_disconnect2(struct dccpcb *);
int dccp_get_option(char *, int, int, char *,int);
void dccp_parse_options(struct dccpcb *, char *, int);
int dccp_remove_feature(struct dccpcb *, u_int8_t, u_int8_t);
int dccp_add_feature_option(struct dccpcb *, u_int8_t, u_int8_t, char *, u_int8_t);
void dccp_feature_neg(struct dccpcb *, u_int8_t, u_int8_t, u_int8_t, char *);
void dccp_close_t(void *);
void dccp_timewait_t(void *);

/* Ack Vector functions */
#define DCCP_VECTORSIZE 512 /* initial ack and cwnd-vector size. Multiple of 8 ! */
void dccp_use_ackvector(struct dccpcb *);
void dccp_update_ackvector(struct dccpcb *, u_int64_t);
void dccp_increment_ackvector(struct dccpcb *, u_int64_t);
u_int16_t dccp_generate_ackvector(struct dccpcb *, u_char *);
u_char dccp_ackvector_state(struct dccpcb *, u_int64_t);

/*
 * DCCP initialization
 */
void
dccp_init(void)
{
	pool_init(&dccpcb_pool, sizeof(struct dccpcb), 0, 0, 0, "dccpcbpl",
		  NULL, IPL_SOFTNET);

	in_pcbinit(&dccpbtable, DCCPBHASHSIZE, DCCPBHASHSIZE);
}

void
dccp_input(struct mbuf *m, ...)
{
	int iphlen;
	struct ip *ip = NULL;
	struct dccphdr *dh;
	struct dccplhdr *dlh;
	struct inpcb *inp = NULL, *oinp = NULL;
	struct in6pcb *in6p = NULL, *oin6p = NULL;
	struct dccpcb *dp;
	struct ipovly *ipov = NULL;
	struct dccp_requesthdr *drqh;
	struct dccp_ackhdr *dah = NULL;
	struct dccp_acklhdr *dalh = NULL;
	struct dccp_resethdr *drth;
	struct socket *so;
	u_char *optp = NULL;
	struct mbuf *opts = 0;
	int len, data_off, extrah_len, optlen;
	/*struct ip save_ip;*/
	char options[DCCP_MAX_OPTIONS];
	char test[2];
	u_int32_t cslen;
	dccp_seq seqnr, low_seqnr, high_seqnr;
	int isipv6 = 0;
	int is_shortseq; /* Is this shortseq packet? */ 
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif

	int off;
	va_list ap;
	
	va_start(ap, m);
	iphlen = off = va_arg(ap, int);
	va_end(ap);

	DCCP_DEBUG((LOG_INFO, "Got DCCP packet!\n"));

	dccpstat.dccps_ipackets++;
	dccpstat.dccps_ibytes += m->m_pkthdr.len;

#ifdef INET6
	isipv6 = (mtod(m, struct ip *)->ip_v == 6) ? 1 : 0;
#endif

#ifdef INET6
	if (isipv6) {
		DCCP_DEBUG((LOG_INFO, "Got DCCP ipv6 packet, iphlen = %u!\n", iphlen));
		ip6 = mtod(m, struct ip6_hdr *);
		IP6_EXTHDR_GET(dh, struct dccphdr *, m, iphlen, sizeof(*dh));
		if (dh == NULL) {
			dccpstat.dccps_badlen++;
			return;
		}
	} else
#endif
	{
		/*
		 * Strip IP options, if any; should skip this,
		 * make available to user, and use on returned packets,
		 * but we don't yet have a way to check the checksum
		 * with options still present.
		 */
		if (iphlen > sizeof (struct ip)) {
			DCCP_DEBUG((LOG_INFO, "Need to strip options\n"));
#if 0				/* XXX */
			ip_stripoptions(m, (struct mbuf *)0);
#endif
			iphlen = sizeof(struct ip);
		}

		/*
		 * Get IP and DCCP header together in first mbuf.
		 */
		ip = mtod(m, struct ip *);
		IP6_EXTHDR_GET(dh, struct dccphdr *, m, iphlen, sizeof(*dh));
		if (dh == NULL) {
			dccpstat.dccps_badlen++;
			return;
		}
	}
	dlh = (struct dccplhdr*)dh;	
	is_shortseq = !dh->dh_x;
	
	if (!is_shortseq) {
		DCCP_DEBUG((LOG_INFO, 
		"Header info: cslen = %u, off = %u, type = %u, reserved = %u, seq = %u.%lu\n",
			 dlh->dh_cscov, dlh->dh_off, dlh->dh_type, dlh->dh_res, ntohs(dlh->dh_seq),
			 (unsigned long)ntohl(dlh->dh_seq2)));
	} else {
		DCCP_DEBUG((LOG_INFO, 
		"Header info(short): cslen = %u, off = %u, type = %u, reserved = %u, seq = %u\n", 
			dh->dh_cscov, dh->dh_off, dh->dh_type, dh->dh_res, ntohl(dh->dh_seq)));
	}

	/*
	 * Make mbuf data length reflect DCCP length.
	 * If not enough data to reflect DCCP length, drop.
	 */

#ifdef INET6
	if (isipv6)
		len = m->m_pkthdr.len - off;
	else
#endif
	{
		len = ntohs(ip->ip_len);
		len -= ip->ip_hl << 2;
	}

	if (len < sizeof(struct dccphdr)) {
		DCCP_DEBUG((LOG_INFO, "Dropping DCCP packet!\n"));
		dccpstat.dccps_badlen++;
		goto badunlocked;
	}
	/*
	 * Save a copy of the IP header in case we want restore it
	 * for sending a DCCP reset packet in response.
	 */
	if (!isipv6) {
		/*save_ip = *ip;*/
		ipov = (struct ipovly *)ip;
	}

	if (dh->dh_cscov == 0) {
		cslen = len;
	} else {
		cslen = dh->dh_off * 4 + (dh->dh_cscov - 1) * 4;
		if (cslen > len)
			cslen = len;
	}
	
	/*
	 * Checksum extended DCCP header and data.
	 */
	
#ifdef INET6
	if (isipv6) {
		if (in6_cksum(m, IPPROTO_DCCP, off, cslen) != 0) {
			dccpstat.dccps_badsum++;
			goto badunlocked;
		}
	} else
#endif
	{
		bzero(ipov->ih_x1, sizeof(ipov->ih_x1));
		ip->ip_len = htons(m->m_pkthdr.len);
		dh->dh_sum = in4_cksum(m, IPPROTO_DCCP, off, cslen);

		if (dh->dh_sum) {
			dccpstat.dccps_badsum++;
			goto badunlocked;
		}
	}

	INP_INFO_WLOCK(&dccpbinfo);

	/*
	 * Locate pcb for datagram.
	 */
#ifdef INET6
	if (isipv6) {
		in6p = in6_pcblookup_connect(&dccpbtable, &ip6->ip6_src,
		    dh->dh_sport, &ip6->ip6_dst, dh->dh_dport, 0, 0);
		if (in6p == 0) {
			/* XXX stats increment? */
			in6p = in6_pcblookup_bind(&dccpbtable, &ip6->ip6_dst,
			    dh->dh_dport, 0);
		}
	} else
#endif
	{
		inp = in_pcblookup_connect(&dccpbtable, ip->ip_src,
		    dh->dh_sport, ip->ip_dst, dh->dh_dport, 0);
		if (inp == NULL) {
			/* XXX stats increment? */
			inp = in_pcblookup_bind(&dccpbtable, ip->ip_dst,
			    dh->dh_dport);
		}
	}
	if (isipv6) {
		DCCP_DEBUG((LOG_INFO, "in6p=%p\n", in6p));
	} else {
		DCCP_DEBUG((LOG_INFO, "inp=%p\n", inp));
	}

	if (isipv6 ? in6p == NULL : inp == NULL) {
		if (dccp_log_in_vain) {
#ifdef INET6
			char dbuf[INET6_ADDRSTRLEN+2], sbuf[INET6_ADDRSTRLEN+2];
#else
			char dbuf[4*sizeof "123"], sbuf[4*sizeof "123"];
#endif

#ifdef INET6
			if (isipv6) {
				strlcpy(dbuf, "[", sizeof dbuf);
				strlcat(dbuf, ip6_sprintf(&ip6->ip6_dst), sizeof dbuf);
				strlcat(dbuf, "]", sizeof dbuf);
				strlcpy(sbuf, "[", sizeof sbuf);
				strlcat(sbuf, ip6_sprintf(&ip6->ip6_src), sizeof sbuf);
				strlcat(sbuf, "]", sizeof sbuf);
			} else
#endif
			{
				strlcpy(dbuf, inet_ntoa(ip->ip_dst), sizeof dbuf);
				strlcpy(sbuf, inet_ntoa(ip->ip_src), sizeof sbuf);
			}
			log(LOG_INFO,
			    "Connection attempt to DCCP %s:%d from %s:%d\n",
			    dbuf, ntohs(dh->dh_dport), sbuf,
			    ntohs(dh->dh_sport));
		}
		dccpstat.dccps_noport++;

		/*
		 * We should send DCCP reset here but we can't call dccp_output
		 * since we have no dccpcb. A icmp unreachable works great but
		 * the specs says DCCP reset :(
		 *
		 * if (!isipv6) {
		 *	*ip = save_ip;
		 *	ip->ip_len += iphlen;
		 *	icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0); 
		 * } 
		 */

		INP_INFO_WUNLOCK(&dccpbinfo);
		goto badunlocked;
	}
	INP_LOCK(inp);

#ifdef INET6
	if (isipv6)
		dp = in6todccpcb(in6p);
	else
#endif
		dp = intodccpcb(inp);

	if (dp == 0) {
		INP_UNLOCK(inp);
		INP_INFO_WUNLOCK(&dccpbinfo);
		goto badunlocked;
	}

	if (dp->state == DCCPS_CLOSED) {
		DCCP_DEBUG((LOG_INFO, "We are in closed state, dropping packet and sending reset!\n"));
		if (dh->dh_type != DCCP_TYPE_RESET)
			dccp_output(dp, DCCP_TYPE_RESET + 2);
		INP_UNLOCK(inp);
		INP_INFO_WUNLOCK(&dccpbinfo);
		goto badunlocked;
	}

#if defined(INET6)
	if (isipv6)
		so = in6p->in6p_socket;
	else
#endif
		so = inp->inp_socket;

	if (so->so_options & SO_ACCEPTCONN) {
		DCCP_DEBUG((LOG_INFO, "so->options & SO_ACCEPTCONN! dp->state = %i\n", dp->state));
		so = sonewconn(so, SS_ISCONNECTED);
		if (so == 0) {
			DCCP_DEBUG((LOG_INFO, "Error, sonewconn failed!\n"));
			INP_UNLOCK(inp);
			INP_INFO_WUNLOCK(&dccpbinfo);
			goto badunlocked;
		}

		/* INP_LOCK(inp); XXX */

#if defined(INET6)
		if (isipv6)
			oin6p = in6p;
		else
#endif
			oinp = inp;

#ifdef INET6
		if (isipv6) {
			in6p = sotoin6pcb(so);
			in6p->in6p_laddr = ip6->ip6_dst;
			in6p->in6p_faddr = ip6->ip6_src;
			in6p->in6p_lport = dh->dh_dport;
			in6p->in6p_fport = dh->dh_sport;
			in6_pcbstate(in6p, IN6P_CONNECTED);
		} else 
#endif
		{
			inp = sotoinpcb(so);
			inp->inp_laddr = ip->ip_dst;
			inp->inp_faddr = ip->ip_src;
			inp->inp_lport = dh->dh_dport;
			inp->inp_fport = dh->dh_sport;
		}

		if (!isipv6)
			in_pcbstate(inp, INP_BOUND);

#if defined(INET6)
		if (isipv6)
			dp = (struct dccpcb *)in6p->in6p_ppcb;
		else
#endif
			dp = (struct dccpcb *)inp->inp_ppcb;

		dp->state = DCCPS_LISTEN;
		dp->who = DCCP_SERVER;
#if defined(INET6)
		if (isipv6) {
			dp->cslen = ((struct dccpcb *)oin6p->in6p_ppcb)->cslen;
			dp->avgpsize = ((struct dccpcb *)oin6p->in6p_ppcb)->avgpsize;
			dp->scode = ((struct dccpcb *)oin6p->in6p_ppcb)->scode;
		} else 
#endif
		{
			dp->cslen = ((struct dccpcb *)oinp->inp_ppcb)->cslen;
			dp->avgpsize = ((struct dccpcb *)oinp->inp_ppcb)->avgpsize;
			dp->scode = ((struct dccpcb *)oinp->inp_ppcb)->scode;
		}
		dp->seq_snd = (((u_int64_t)random() << 32) | random()) % 281474976710656LL;
		dp->ref_seq.hi = dp->seq_snd >> 24;
		dp->ref_seq.lo = (u_int64_t)(dp->seq_snd & 0xffffff);
		INP_UNLOCK(oinp);
		DCCP_DEBUG((LOG_INFO, "New dp = %u, dp->state = %u!\n", (int)dp, dp->state));
	}

	INP_INFO_WUNLOCK(&dccpbinfo);

	/*
	 * Check if sequence number is inside the loss window 
	 */
	if (!is_shortseq) { 
		DHDR_TO_DSEQ(seqnr, dlh);
	} else {
		/* shortseq */
		seqnr = CONVERT_TO_LONGSEQ((ntohl(dh->dh_seq) >> 8), dp->ref_pseq);
		DCCP_DEBUG((LOG_INFO, "short seq conversion %x,  %u %u\n", 
			ntohl(dh->dh_seq) >> 8, dp->ref_pseq.hi, dp->ref_pseq.lo));
	}

	DCCP_DEBUG((LOG_INFO, "Received DCCP packet with sequence number = %llu , gsn_rcv %llu\n", seqnr, dp->gsn_rcv));

	/* store ccval */
	dp->ccval = dh->dh_ccval;

	if (dp->gsn_rcv == 281474976710656LL) dp->gsn_rcv = seqnr;
	if (dp->gsn_rcv > (dp->loss_window / 4))
		low_seqnr = (dp->gsn_rcv - (dp->loss_window / 4)) % 281474976710656LL;
	else
		low_seqnr = 0ll;
	high_seqnr = (dp->gsn_rcv + (dp->loss_window / 4 * 3)) % 281474976710656LL;

	if (! (DCCP_SEQ_GT(seqnr, low_seqnr) && DCCP_SEQ_LT(seqnr, high_seqnr))) {
		dccpstat.dccps_badseq++;
		DCCP_DEBUG((LOG_INFO, "Recieved DCCP packet with bad sequence number = %llu (low_seqnr = %llu, high_seqnr = %llu)\n", seqnr, low_seqnr, high_seqnr));
		INP_UNLOCK(inp);
		goto badunlocked;
	}

	/* dp->gsn_rcv should always be the highest received valid sequence number */
	if (DCCP_SEQ_GT(seqnr, dp->gsn_rcv))
		dp->gsn_rcv = seqnr;

	/* Just ignore DCCP-Move for now */
	if (dlh->dh_type == DCCP_TYPE_DATA) {
		extrah_len = 0;
		if (!is_shortseq)
			optp = (u_char *)(dlh + 1);
		else
			optp = (u_char *)(dh + 1);
	} else if (dh->dh_type == DCCP_TYPE_REQUEST) {
		drqh = (struct dccp_requesthdr *)(dlh + 1);
		if (drqh->drqh_scode != dp->scode){
			DCCP_DEBUG((LOG_INFO, "service code in request packet doesn't match! %x %x\n", drqh->drqh_scode, dp->scode));
			INP_UNLOCK(inp);
			dp->state = DCCPS_SERVER_CLOSE; /* So disconnect2 doesn't send CLOSEREQ */
			dccp_disconnect2(dp);
			dccp_output(dp, DCCP_TYPE_RESET + 2);
			dccp_close(dp);
			goto badunlocked;
		}
		optp = (u_char *)(drqh + 1);
		extrah_len = 4;

		/* store reference peer sequence number */
		dp->ref_pseq.hi = seqnr >> 24;
		dp->ref_pseq.lo = (u_int64_t)(seqnr & 0xffffff);

	} else if (dh->dh_type == DCCP_TYPE_RESET) {
		extrah_len = 8 ;
		drth = (struct dccp_resethdr *)(dlh + 1);
		optp = (u_char *)(drth + 1);
	} else {
		if (!is_shortseq){
			extrah_len = 8;
			dalh = (struct dccp_acklhdr *)(dlh + 1);
			if (dh->dh_type == DCCP_TYPE_RESPONSE) {
				extrah_len += 4;
				drqh = (struct dccp_requesthdr *)(dalh + 1);
				if (drqh->drqh_scode != dp->scode){
					DCCP_DEBUG((LOG_INFO, "service code in response packet doesn't match! %x %x\n", drqh->drqh_scode, dp->scode));
					INP_UNLOCK(inp);
					dp->state = DCCPS_CLIENT_CLOSE; /* So disconnect2 doesn't send CLOSEREQ */
					dccp_disconnect2(dp);
					dccp_output(dp, DCCP_TYPE_RESET + 2);
					dccp_close(dp);
					goto badunlocked;
				}
				optp = (u_char *)(drqh + 1);

				/* store reference peer sequence number */
				dp->ref_pseq.hi = seqnr >> 24;
				dp->ref_pseq.lo = (u_int64_t)(seqnr & 0xffffff);
			} else 
				optp = (u_char *)(dalh + 1);
		} else {
			extrah_len = 4;
			dah = (struct dccp_ackhdr *)(dh + 1);
			optp = (u_char *)(dah + 1);
		}

	}

	data_off = (dh->dh_off << 2);

	dp->seq_rcv = seqnr;
	dp->ack_rcv = 0; /* Clear it for now */
	dp->type_rcv = dh->dh_type;
	dp->len_rcv = m->m_len - data_off - iphlen; /* Correct length ? */
	
	if (!is_shortseq)
		optlen = data_off - (sizeof(struct dccplhdr) + extrah_len);
	else
		optlen = data_off - (sizeof(struct dccphdr) + extrah_len);

	if (optlen < 0) {
		DCCP_DEBUG((LOG_INFO, "Data offset is smaller then it could be, optlen = %i data_off = %i, m_len = %i, iphlen = %i extrah_len = %i !\n", optlen, data_off, m->m_len, iphlen, extrah_len));
		INP_UNLOCK(inp);
		goto badunlocked;
	}

	if (optlen > 0) {
		if (optlen > DCCP_MAX_OPTIONS) {
			DCCP_DEBUG((LOG_INFO, "Error, more options (%i) then DCCP_MAX_OPTIONS options!\n", optlen));
			INP_UNLOCK(inp);
			goto badunlocked;
		}

		DCCP_DEBUG((LOG_INFO, "Parsing DCCP options, optlen = %i\n", optlen));
		bcopy(optp, options, optlen);
		dccp_parse_options(dp, options, optlen);
	}

	DCCP_DEBUG((LOG_INFO, "BEFORE state check, Got a %u packet while in %u state, who = %u!\n", dh->dh_type, dp->state, dp->who));

	if (dp->state == DCCPS_LISTEN) {
		switch (dh->dh_type) {

		case DCCP_TYPE_REQUEST:
			DCCP_DEBUG((LOG_INFO, "Got DCCP REQUEST\n"));
			dp->state = DCCPS_REQUEST;
			if (dp->cc_in_use[1] < 0) {
				test[0] = DEFAULT_CCID;
				test[1] = 3;
				dccp_add_feature(dp, DCCP_OPT_CHANGE_R, DCCP_FEATURE_CC, test, 2);
			}
			if (len > data_off) {
				DCCP_DEBUG((LOG_INFO, "XXX: len=%d, data_off=%d\n", len, data_off));
				dccp_add_option(dp, DCCP_OPT_DATA_DISCARD, test, 0);
			}
			callout_reset(&dp->connect_timer, DCCP_CONNECT_TIMER,
			    dccp_connect_t, dp);
			dccp_output(dp, 0);
			break;


		/* These are ok if the sender has a valid init Cookie */
		case DCCP_TYPE_ACK:
		case DCCP_TYPE_DATAACK:
		case DCCP_TYPE_DATA:
			DCCP_DEBUG((LOG_INFO, "Got DCCP ACK/DATAACK/DATA, should check init cookie...\n"));
			dccp_output(dp, DCCP_TYPE_RESET + 2);
			break;

		case DCCP_TYPE_RESET:
			DCCP_DEBUG((LOG_INFO, "Got DCCP RESET\n"));
			dp->state = DCCPS_TIME_WAIT;
			dp = dccp_close(dp);
			return;

		default:
			DCCP_DEBUG((LOG_INFO, "Got a %u packet while in listen stage!\n", dh->dh_type));
			/* Force send reset. */
			dccp_output(dp, DCCP_TYPE_RESET + 2);
		}
	} else if (dp->state == DCCPS_REQUEST) {
		switch (dh->dh_type) {
		case DCCP_TYPE_RESPONSE:
			DAHDR_TO_DSEQ(dp->ack_rcv, ((struct dccp_acklhdr*)dalh)->dash);
			dp->ack_snd = dp->seq_rcv;
			DCCP_DEBUG((LOG_INFO, "Got DCCP REPSONSE %x %llx\n", dp, dp->ack_snd));

			callout_stop(&dp->retrans_timer);
			callout_stop(&dp->connect_timer);

			/* First check if we have negotiated a cc */
			if (dp->cc_in_use[0] > 0 && dp->cc_in_use[1] > 0) {
				DCCP_DEBUG((LOG_INFO, "Setting DCCPS_ESTAB & soisconnected\n"));
				dp->state = DCCPS_ESTAB;
				dccpstat.dccps_connects++;
#if defined(INET6)
				if (isipv6)
					soisconnected(in6p->in6p_socket);
				else
#endif
					soisconnected(inp->inp_socket);
			} else {
				dp->state = DCCPS_RESPOND;
				DCCP_DEBUG((LOG_INFO, "CC negotiation is not finished, cc_in_use[0] = %u, cc_in_use[1] = %u\n",dp->cc_in_use[0], dp->cc_in_use[1]));

			}
			dccp_output(dp, 0);
			break;

		case DCCP_TYPE_RESET:
			DCCP_DEBUG((LOG_INFO, "Got DCCP RESET\n"));
			dp->state = DCCPS_TIME_WAIT;
			dp = dccp_close(dp);
			return;

		default:
			DCCP_DEBUG((LOG_INFO, "Got a %u packet while in REQUEST stage!\n", dh->dh_type));
			/* Force send reset. */
			dccp_output(dp, DCCP_TYPE_RESET + 2);
			if (dh->dh_type == DCCP_TYPE_CLOSE) {
				dp = dccp_close(dp);
				return;
			} else {
				callout_stop(&dp->retrans_timer);
				dp->state = DCCPS_TIME_WAIT;
			}
		}
	} else if (dp->state == DCCPS_RESPOND) {
		switch (dh->dh_type) {

		case DCCP_TYPE_REQUEST:
			break;
		case DCCP_TYPE_ACK:
		case DCCP_TYPE_DATAACK:
			DCCP_DEBUG((LOG_INFO, "Got DCCP ACK/DATAACK\n"));

			callout_stop(&dp->connect_timer);

			if (!is_shortseq) {
				DAHDR_TO_DSEQ(dp->ack_rcv, ((struct dccp_acklhdr*)dalh)->dash);
			} else {
				/* shortseq XXX */
				dp->ack_rcv = CONVERT_TO_LONGSEQ((ntohl(dah->dash.dah_ack) >> 8), dp->ref_seq);
			} 

			if (dp->cc_in_use[0] > 0 && dp->cc_in_use[1] > 0) {
				DCCP_DEBUG((LOG_INFO, "Setting DCCPS_ESTAB & soisconnected\n"));
				dp->state = DCCPS_ESTAB;
				dccpstat.dccps_connects++;
#if defined(INET6)
				if (isipv6)
					soisconnected(in6p->in6p_socket);
				else
#endif
					soisconnected(inp->inp_socket);
			} else {
				DCCP_DEBUG((LOG_INFO, "CC negotiation is not finished, cc_in_use[0] = %u, cc_in_use[1] = %u\n",dp->cc_in_use[0], dp->cc_in_use[1]));
				/* Force an output!!! */
				dp->ack_snd = dp->seq_rcv;
				dccp_output(dp, 0);
			}

			if (dh->dh_type == DCCP_TYPE_DATAACK && dp->cc_in_use[1] > 0) {
				if (!dp->ack_snd) dp->ack_snd = dp->seq_rcv;
				DCCP_DEBUG((LOG_INFO, "Calling *cc_sw[%u].cc_recv_packet_recv!\n", dp->cc_in_use[1]));
				(*cc_sw[dp->cc_in_use[1]].cc_recv_packet_recv)(dp->cc_state[1], options, optlen); 
			}
			break;

		case DCCP_TYPE_CLOSE:
			dccp_output(dp, DCCP_TYPE_CLOSE + 1);
			dp = dccp_close(dp);
			goto badunlocked;

		case DCCP_TYPE_RESET:
			dp->state = DCCPS_TIME_WAIT;
			callout_stop(&dp->retrans_timer);
			break;

		default:
			DCCP_DEBUG((LOG_INFO, "Got a %u packet while in response stage!\n", dh->dh_type));
			/* Force send reset. */
			dccp_output(dp, DCCP_TYPE_RESET + 2);
		}
	} else if (dp->state == DCCPS_ESTAB) {
		switch (dh->dh_type) {

		case DCCP_TYPE_DATA:
			DCCP_DEBUG((LOG_INFO, "Got DCCP DATA, state = %i, cc_in_use[1] = %u\n", dp->state, dp->cc_in_use[1]));
			
			if (dp->cc_in_use[1] > 0) {
				if (!dp->ack_snd) dp->ack_snd = dp->seq_rcv;
				DCCP_DEBUG((LOG_INFO, "Calling data *cc_sw[%u].cc_recv_packet_recv! %llx %llx dp=%x\n", dp->cc_in_use[1], dp->ack_snd, dp->seq_rcv, dp));
				(*cc_sw[dp->cc_in_use[1]].cc_recv_packet_recv)(dp->cc_state[1], options, optlen);
			}
			break;
	
		case DCCP_TYPE_ACK:
			DCCP_DEBUG((LOG_INFO, "Got DCCP ACK\n"));
			if (!is_shortseq) {
				DAHDR_TO_DSEQ(dp->ack_rcv, ((struct dccp_acklhdr*)dalh)->dash);
			} else {
				/* shortseq */
				dp->ack_rcv = CONVERT_TO_LONGSEQ((ntohl(dah->dash.dah_ack) >> 8), dp->ref_seq);
			} 

			if (dp->cc_in_use[1] > 0) {
				/* This is called so Acks on Acks can be handled */
				if (!dp->ack_snd) dp->ack_snd = dp->seq_rcv;
				DCCP_DEBUG((LOG_INFO, "Calling ACK *cc_sw[%u].cc_recv_packet_recv! %llx %llx\n", dp->cc_in_use[1], dp->ack_snd, dp->seq_rcv));
				(*cc_sw[dp->cc_in_use[1]].cc_recv_packet_recv)(dp->cc_state[1], options, optlen); 
			}
			break;
	
		case DCCP_TYPE_DATAACK:
			DCCP_DEBUG((LOG_INFO, "Got DCCP DATAACK\n"));

			if (!is_shortseq) {
				DAHDR_TO_DSEQ(dp->ack_rcv, ((struct dccp_acklhdr*)dalh)->dash);
			} else {
				/* shortseq */
				dp->ack_rcv = CONVERT_TO_LONGSEQ((ntohl(dah->dash.dah_ack) >> 8), dp->ref_seq);
			} 

			if (dp->cc_in_use[1] > 0) {
				if (!dp->ack_snd) dp->ack_snd = dp->seq_rcv;
				DCCP_DEBUG((LOG_INFO, "Calling *cc_sw[%u].cc_recv_packet_recv! %llx %llx\n", dp->cc_in_use[1], dp->ack_snd, dp->seq_rcv));
				(*cc_sw[dp->cc_in_use[1]].cc_recv_packet_recv)(dp->cc_state[1], options, optlen); 
			}
			break;
	
		case DCCP_TYPE_CLOSEREQ:
			DCCP_DEBUG((LOG_INFO, "Got DCCP CLOSEREQ, state = estab\n"));
			if (dp->who == DCCP_CLIENT) {
				dccp_disconnect2(dp);
			} else {
				dccp_output(dp, DCCP_TYPE_RESET + 2);
			}
			break;
	
		case DCCP_TYPE_CLOSE:
			DCCP_DEBUG((LOG_INFO, "Got DCCP CLOSE, state = estab\n"));
			dp->state = DCCPS_SERVER_CLOSE; /* So disconnect2 doesn't send CLOSEREQ */
			dccp_disconnect2(dp);
			dccp_output(dp, DCCP_TYPE_RESET + 2);
			dccp_close(dp);
			goto badunlocked;
			break;
	
		case DCCP_TYPE_RESET:
			DCCP_DEBUG((LOG_INFO, "Got DCCP RESET\n"));
			dp->state = DCCPS_TIME_WAIT;
			callout_stop(&dp->retrans_timer);
			callout_reset(&dp->timewait_timer, DCCP_TIMEWAIT_TIMER,
			    dccp_timewait_t, dp);
			break;
	
		case DCCP_TYPE_MOVE:
			DCCP_DEBUG((LOG_INFO, "Got DCCP MOVE\n"));
			break;

		default:
			DCCP_DEBUG((LOG_INFO, "Got a %u packet while in established stage!\n", dh->dh_type));
		}

	} else if (dp->state == DCCPS_SERVER_CLOSE) {
		/* Server */
		switch (dh->dh_type) {
		case DCCP_TYPE_CLOSE:
			DCCP_DEBUG((LOG_INFO, "Got DCCP CLOSE (State DCCPS_SERVER_CLOSE)\n"));
			callout_stop(&dp->retrans_timer);
			dccp_output(dp, DCCP_TYPE_RESET + 2);
			dp = dccp_close(dp);
			goto badunlocked;

		case DCCP_TYPE_RESET:
			DCCP_DEBUG((LOG_INFO, "Got DCCP RESET\n"));
			callout_stop(&dp->retrans_timer);
			dccp_output(dp, DCCP_TYPE_RESET + 2);
			dp->state = DCCPS_TIME_WAIT;
			break;
		default:
			DCCP_DEBUG((LOG_INFO, "Got a %u packet while in server_close stage!\n", dh->dh_type));
		}

	} else if (dp->state == DCCPS_CLIENT_CLOSE) {
		/* Client */
		switch (dh->dh_type) {
		case DCCP_TYPE_CLOSE:
			/* Ignore */
			break;
		case DCCP_TYPE_CLOSEREQ:
			DCCP_DEBUG((LOG_INFO, "Got DCCP CLOSEREQ, state = DCCPS_CLIENT_CLOSE\n"));
			/* Just resend close */
			dccp_output(dp, 0);
			break;
		case DCCP_TYPE_RESET:
			DCCP_DEBUG((LOG_INFO, "Got DCCP RESET\n"));
			callout_stop(&dp->retrans_timer);
			dp->state = DCCPS_TIME_WAIT;
			callout_reset(&dp->timewait_timer, DCCP_TIMEWAIT_TIMER,
			    dccp_timewait_t, dp);
			break;
		default:
			DCCP_DEBUG((LOG_INFO, "Got a %u packet while in client_close stage!\n", dh->dh_type));

		}
	} else {
		DCCP_DEBUG((LOG_INFO, "Got a %u packet while in %u state!\n", dh->dh_type, dp->state));
		if (dh->dh_type != DCCP_TYPE_RESET) {
			/* Force send reset. */
			DCCP_DEBUG((LOG_INFO, "Force sending a request!\n"));
			dccp_output(dp, DCCP_TYPE_RESET + 2);
		}
	}

	if (dh->dh_type == DCCP_TYPE_DATA ||
	    dh->dh_type == DCCP_TYPE_ACK  ||
	    dh->dh_type == DCCP_TYPE_DATAACK) {
		if (dp->cc_in_use[0] > 0) {
			(*cc_sw[dp->cc_in_use[0]].cc_send_packet_recv)(dp->cc_state[0],options, optlen);
		}
		
	}

	if (dh->dh_type == DCCP_TYPE_DATA || dh->dh_type == DCCP_TYPE_DATAACK) {
#if defined(__FreeBSD__) && __FreeBSD_version >= 503000
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE) 
#else
		if (so->so_state & SS_CANTRCVMORE) 
#endif
		{
			DCCP_DEBUG((LOG_INFO, "state & SS_CANTRCVMORE...!\n"));
			m_freem(m);
			if (opts)
				m_freem(opts);
		} else {
			m_adj(m, (iphlen + data_off));
			DCCP_DEBUG((LOG_INFO, "Calling sbappend!\n"));
			sbappend(&so->so_rcv, m);
		}
		DCCP_DEBUG((LOG_INFO, "Calling sorwakeup...!\n"));
		sorwakeup(so);
	} else {
		m_freem(m);
		if (opts)
			m_freem(opts);
	}
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	if (dp)
		INP_UNLOCK(inp);
#endif

	return;

badunlocked:
	m_freem(m);
	if (opts)
		m_freem(opts);
	return;
}

/*
 * Notify a dccp user of an asynchronous error;
 * just wake up so that he can collect error status.
 */
void
dccp_notify(struct inpcb *inp, int errno)
{
	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
	return;
}

/*
 * Called when we get ICMP errors (destination unrechable,
 * parameter problem, source quench, time exceeded and redirects)
 */
void *
dccp_ctlinput(int cmd, const struct sockaddr *sa, void *vip)
{
	struct ip *ip = vip;
	struct dccphdr *dh;
	void (*notify)(struct inpcb *, int) = dccp_notify;
	struct in_addr faddr;
	struct inpcb *inp = NULL;

	faddr = ((const struct sockaddr_in *)sa)->sin_addr;
	if (sa->sa_family != AF_INET || faddr.s_addr == INADDR_ANY)
		return NULL;

	if (PRC_IS_REDIRECT(cmd)) {
		ip = 0;
		notify = in_rtchange;
	} else if (cmd == PRC_HOSTDEAD)
		ip = 0;
	else if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0)
		return NULL;
	if (ip) {
		/*s = splsoftnet();*/
		dh = (struct dccphdr *)((vaddr_t)ip + (ip->ip_hl << 2));
		INP_INFO_RLOCK(&dccpbinfo);
		in_pcbnotify(&dccpbtable, faddr, dh->dh_dport,
		    ip->ip_src, dh->dh_sport, inetctlerrmap[cmd], notify); 
		if (inp != NULL) {
			INP_LOCK(inp);
			if (inp->inp_socket != NULL) {
				(*notify)(inp, inetctlerrmap[cmd]);
			}
			INP_UNLOCK(inp);
		}
		INP_INFO_RUNLOCK(&dccpbinfo);
		/*splx(s);*/
	} else
		in_pcbnotifyall(&dccpbtable, faddr, inetctlerrmap[cmd], notify);

	return NULL;
}

static int
dccp_optsset(struct dccpcb *dp, struct sockopt *sopt)
{
	int optval;
	int error = 0;

	switch (sopt->sopt_name) {
	case DCCP_CCID:
		error = sockopt_getint(sopt, &optval);
		/* Add check that optval is a CCID we support!!! */
		if (optval == 2 || optval == 3 || optval == 0) {
			dp->pref_cc = optval;
		} else {
			error = EINVAL;
		}
		break;
	case DCCP_CSLEN:
		error = sockopt_getint(sopt, &optval);
		if (optval > 15 || optval < 0) {
			error = EINVAL;
		} else {
			dp->cslen = optval;
		}
		break;
	case DCCP_MAXSEG:
		error = sockopt_getint(sopt, &optval);
		if (optval > 0 && optval <= dp->d_maxseg) {
			dp->d_maxseg = optval;
		} else {
			error = EINVAL;
		}
		break;
	case DCCP_SERVICE:
		error = sockopt_getint(sopt, &optval);
		dp->scode = optval;
		break;
		
	default:
		error = ENOPROTOOPT;
	}

	return error;
}

static int	
dccp_optsget(struct dccpcb *dp, struct sockopt *sopt)
{
	int optval = 0;
	int error = 0;

	switch (sopt->sopt_name) {
	case DCCP_CCID:
		optval = dp->pref_cc;
		error = sockopt_set(sopt, &optval, sizeof(optval));
		break;
	case DCCP_CSLEN:
		optval = dp->cslen;
		error = sockopt_set(sopt, &optval, sizeof(optval));
		break;
	case DCCP_MAXSEG:
		optval = dp->d_maxseg;
		error = sockopt_set(sopt, &optval, sizeof(optval));
		break;
	case DCCP_SERVICE:
		optval = dp->scode;
		error = sockopt_set(sopt, &optval, sizeof(optval));
		break;
	default:
		error = ENOPROTOOPT;
	}

	return error;
}

/* 
 * Called by getsockopt and setsockopt.
 *
 */
int
dccp_ctloutput(int op, struct socket *so, struct sockopt *sopt)
{
	int s, error = 0;
	struct inpcb	*inp;
#if defined(INET6)
	struct in6pcb *in6p;
#endif
	struct dccpcb	*dp;
	int family;	/* family of the socket */

	family = so->so_proto->pr_domain->dom_family;
	error = 0;

	s = splsoftnet();
	INP_INFO_RLOCK(&dccpbinfo);
	switch (family) {
	case PF_INET:
		inp = sotoinpcb(so);
#if defined(INET6)
		in6p = NULL;
#endif
		break;
#if defined(INET6)
	case PF_INET6:
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		INP_INFO_RUNLOCK(&dccpbinfo);
		splx(s);
		return EAFNOSUPPORT;
	}
#if defined(INET6)
	if (inp == NULL && in6p == NULL)
#else
	if (inp == NULL)
#endif
	{
		INP_INFO_RUNLOCK(&dccpbinfo);
		splx(s);
		return (ECONNRESET);
	}
/*
	if (inp)
		INP_LOCK(inp);
	else
		IN6P_LOCK(in6p);
	INP_INFO_RUNLOCK(&dccpbinfo);
*/
	if (sopt->sopt_level != IPPROTO_DCCP) {
		switch (family) {
		case PF_INET:
			error = ip_ctloutput(op, so, sopt);
			break;
#if defined(INET6)
		case PF_INET6:
			error = ip6_ctloutput(op, so, sopt);
			break;
#endif
		}
		splx(s);
		return (error);
	}

	if (inp)
		dp = intodccpcb(inp);
#if defined(INET6)
	else if (in6p)
		dp = in6todccpcb(in6p);
#endif
	else
		dp = NULL;

	if (op == PRCO_SETOPT) {
		error = dccp_optsset(dp, sopt);
	} else if (op ==  PRCO_GETOPT) {
		error = dccp_optsget(dp, sopt);
	} else {
		error = EINVAL;
	}
/*
	if (inp)
		INP_UNLOCK(inp);
	else
		IN6P_UNLOCK(in6p);
*/
	splx(s);
	return error;
}

int
dccp_output(struct dccpcb *dp, u_int8_t extra)
{
	struct inpcb *inp;
	struct in6pcb *in6p = NULL;
	struct socket *so;
	struct mbuf *m;

	struct ip *ip = NULL;
	struct dccphdr *dh;
	struct dccplhdr *dlh;
	struct dccp_requesthdr *drqh;
	struct dccp_ackhdr *dah;
	struct dccp_acklhdr *dalh;
	struct dccp_resethdr *drth;
	u_char *optp = NULL;
	int error = 0;
	int off, sendalot, t, i;
	u_int32_t hdrlen, optlen, extrah_len, cslen;
	u_int8_t type;
	char options[DCCP_MAX_OPTIONS *2];
	long len, pktlen;
	int isipv6 = 0;
	int use_shortseq = 0;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;
#endif

	DCCP_DEBUG((LOG_INFO, "dccp_output start!\n"));

	isipv6 = (dp->inp_vflag & INP_IPV6) != 0;

	DCCP_DEBUG((LOG_INFO, "Going to send a DCCP packet!\n"));
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	KASSERT(mutex_assert(&dp->d_inpcb->inp_mtx, MA_OWNED));
#endif

#if defined(INET6)
	if (isipv6) {
		inp = 0;
		in6p = dp->d_in6pcb;
		so = in6p->in6p_socket;
	} else
#endif
	{
		inp = dp->d_inpcb;
		so = inp->inp_socket;
	}

	if (dp->state != DCCPS_ESTAB && extra == 1) {
		/* Only let cc decide when to resend if we are in establised state */
		return 0;
	}

	if (so->so_snd.sb_cc){
		pktlen = dp->pktlen[dp->pktlenidx];
	} else 
		pktlen = 0;

	/* Check with CC if we can send... */
	if (pktlen && dp->cc_in_use[0] > 0 && dp->state == DCCPS_ESTAB) {
		if (!(*cc_sw[dp->cc_in_use[0]].cc_send_packet)(dp->cc_state[0], pktlen)) {
			DCCP_DEBUG((LOG_INFO, "Not allowed to send right now\n"));
			return 0;
		}
	}

	if (pktlen) {
		dp->pktcnt --;
		dp->pktlenidx = (dp->pktlenidx +1) % DCCP_MAX_PKTS; 
	}

again:
	sendalot = 0;

	/*
	 * off not needed for dccp because we do not need to wait for ACK
	 * before removing the packet
	 */
	off = 0;
	optlen = 0;

	if (pktlen > dp->d_maxseg) {
		/* this should not happen */
		DCCP_DEBUG((LOG_INFO, "packet will be fragmented! maxseg %d\n", dp->d_maxseg));
		len = dp->d_maxseg;
		pktlen -= len;
		sendalot = 1;
	} else
		len = pktlen;

	if (extra == DCCP_TYPE_RESET + 2) {
		DCCP_DEBUG((LOG_INFO, "Force sending of DCCP TYPE_RESET! seq=%llu\n", dp->seq_snd));
		type = DCCP_TYPE_RESET;
		extrah_len = 12; 
	} else if (dp->state <= DCCPS_REQUEST && dp->who == DCCP_CLIENT) {
		DCCP_DEBUG((LOG_INFO, "Sending DCCP TYPE_REQUEST!\n"));
		type = DCCP_TYPE_REQUEST;
		dp->state = DCCPS_REQUEST;
		extrah_len = 4; 
	} else if (dp->state == DCCPS_REQUEST && dp->who == DCCP_SERVER) {
		DCCP_DEBUG((LOG_INFO, "Sending DCCP TYPE_RESPONSE!\n"));
		type = DCCP_TYPE_RESPONSE;
		dp->state = DCCPS_RESPOND;
		extrah_len = 12; 
	} else if (dp->state == DCCPS_RESPOND) {
		DCCP_DEBUG((LOG_INFO, "Still in feature neg, sending DCCP TYPE_ACK!\n"));
		type = DCCP_TYPE_ACK;
		if (!dp->shortseq)
			extrah_len = 8;
		else {
			extrah_len = 4;
			use_shortseq = 1;
		}
	} else if (dp->state == DCCPS_ESTAB) {
		if (dp->ack_snd && len) {
			DCCP_DEBUG((LOG_INFO, "Sending DCCP TYPE_DATAACK!\n"));
			type = DCCP_TYPE_DATAACK;
			/*(u_int32_t *)&extrah = dp->seq_rcv; */
			if (!dp->shortseq)
				extrah_len = 8;
			else {
				extrah_len = 4;
				use_shortseq = 1;
			}
		} else if (dp->ack_snd) {
			DCCP_DEBUG((LOG_INFO, "Sending DCCP TYPE_ACK!\n"));
			type = DCCP_TYPE_ACK;
			if (!dp->shortseq)
				extrah_len = 8;
			else {
				extrah_len = 4;
				use_shortseq = 1;
			}
		} else if (len) {
			DCCP_DEBUG((LOG_INFO, "Sending DCCP TYPE_DATA!\n"));
			type = DCCP_TYPE_DATA;
			extrah_len = 0;
		} else {
			DCCP_DEBUG((LOG_INFO, "No ack or data to send!\n"));
			return 0;
		}
	} else if (dp->state == DCCPS_CLIENT_CLOSE) {
		DCCP_DEBUG((LOG_INFO, "Sending DCCP TYPE_CLOSE!\n"));
		type = DCCP_TYPE_CLOSE;
		extrah_len = 8;
	} else if (dp->state == DCCPS_SERVER_CLOSE) {
		DCCP_DEBUG((LOG_INFO, "Sending DCCP TYPE_CLOSEREQ!\n"));
		type = DCCP_TYPE_CLOSEREQ;
		extrah_len = 8;
	} else {
		DCCP_DEBUG((LOG_INFO, "Hey, we should never get here, state = %u\n", dp->state));
		return 1;
	}

	/* Adding options. */
	if (dp->optlen) {
		DCCP_DEBUG((LOG_INFO, "Copying options from dp->options! %u\n", dp->optlen));
		bcopy(dp->options, options , dp->optlen);
		optlen = dp->optlen;
		dp->optlen = 0;
	}

	if (dp->featlen && (optlen + dp->featlen < DCCP_MAX_OPTIONS)) {
		DCCP_DEBUG((LOG_INFO, "Copying options from dp->features! %u\n", dp->featlen));
		bcopy(dp->features, options + optlen, dp->featlen);
		optlen += dp->featlen;
	}

	t = optlen % 4;

	if (t) {
		t = 4 - t;
		for (i = 0 ; i<t; i++) {
			options[optlen] = 0;
			optlen++;
		}
	}

#ifdef INET6
	if (isipv6) {
		DCCP_DEBUG((LOG_INFO, "Sending ipv6 packet...\n"));
		if (!use_shortseq)
			hdrlen = sizeof(struct ip6_hdr) + sizeof(struct dccplhdr) +
			    extrah_len + optlen;
		else 
			hdrlen = sizeof(struct ip6_hdr) + sizeof(struct dccphdr) +
			    extrah_len + optlen;
	} else
#endif
	{
		if (!use_shortseq)
			hdrlen = sizeof(struct ip) + sizeof(struct dccplhdr) +
		   		extrah_len + optlen;
		else
			hdrlen = sizeof(struct ip) + sizeof(struct dccphdr) +
		   		extrah_len + optlen;
	}
	DCCP_DEBUG((LOG_INFO, "Pkt headerlen %u\n", hdrlen));

	if (len > (dp->d_maxseg - extrah_len - optlen)) {
		len = dp->d_maxseg - extrah_len - optlen;
		sendalot = 1;
	}
	
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}
	if (MHLEN < hdrlen + max_linkhdr) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			error = ENOBUFS;
			goto release;
		}
	}

	m->m_data += max_linkhdr;
	m->m_len = hdrlen;

	if (len) { /* We have data to send */
		if (len <= M_TRAILINGSPACE(m) - hdrlen) {
			m_copydata(so->so_snd.sb_mb, off, (int) len,
			mtod(m, char *) + hdrlen);
			m->m_len += len;
		} else {
			m->m_next = m_copy(so->so_snd.sb_mb, off, (int) len);
			if (m->m_next == 0) {
				error = ENOBUFS;
				goto release;
			}
		}
	} else {
		dp->ndp++;
	}

	m->m_pkthdr.rcvif = (struct ifnet *)0;

	if (!isipv6 && (len + hdrlen) > IP_MAXPACKET) {
		error = EMSGSIZE;
		goto release;
	}

	/*
	 * Fill in mbuf with extended DCCP header
	 * and addresses and length put into network format.
	 */
#ifdef INET6
	if (isipv6) {
		ip6 = mtod(m, struct ip6_hdr *);
		dh = (struct dccphdr *)(ip6 + 1);
		ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
			(in6p->in6p_flowinfo & IPV6_FLOWINFO_MASK);
		ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
			 (IPV6_VERSION & IPV6_VERSION_MASK);
		ip6->ip6_nxt = IPPROTO_DCCP;
		ip6->ip6_src = in6p->in6p_laddr;
		ip6->ip6_dst = in6p->in6p_faddr;
	} else 
#endif
	{
		ip = mtod(m, struct ip *);
		dh = (struct dccphdr *)(ip + 1);
		bzero(ip, sizeof(struct ip));
		ip->ip_p = IPPROTO_DCCP;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst = inp->inp_faddr;
	}
	dlh = (struct dccplhdr *)dh;

	if (inp) {
		dh->dh_sport = inp->inp_lport;
		dh->dh_dport = inp->inp_fport;
	}
#ifdef INET6
	else if (in6p) {
		dh->dh_sport = in6p->in6p_lport;
		dh->dh_dport = in6p->in6p_fport;
	}
#endif
	dh->dh_cscov = dp->cslen;
	dh->dh_ccval = dp->ccval;
	dh->dh_type = type;
	dh->dh_res = 0; /* Reserved field should be zero */
	if (!use_shortseq) { 
		dlh->dh_res2 = 0; /* Reserved field should be zero */
		dh->dh_off = 4 + (extrah_len / 4) + (optlen / 4);
	} else		
		dh->dh_off = 3 + (extrah_len / 4) + (optlen / 4);

	dp->seq_snd = (dp->seq_snd +1) % 281474976710656LL;
	if (!use_shortseq) { 
		DSEQ_TO_DHDR(dlh, dp->seq_snd);
		dlh->dh_x = 1;
	} else {
		/* short sequene number */
		dh->dh_seq = htonl(dp->seq_snd) >> 8;
		dh->dh_x = 0;
	}       

	if (!use_shortseq) {
		DCCP_DEBUG((LOG_INFO, "Sending with seq %x.%x, (dp->seq_snd = %llu)\n\n", dlh->dh_seq, dlh->dh_seq2, dp->seq_snd));
	} else {
		DCCP_DEBUG((LOG_INFO, "Sending with seq %x, (dp->seq_snd = %llu)\n\n", dh->dh_seq, dp->seq_snd));
	}

	if (dh->dh_type == DCCP_TYPE_REQUEST) {
		drqh = (struct dccp_requesthdr *)(dlh + 1);
		drqh->drqh_scode = dp->scode;
		optp = (u_char *)(drqh + 1);
	} else if (dh->dh_type == DCCP_TYPE_RESET) {
		drth = (struct dccp_resethdr *)(dlh + 1);
		drth->drth_dash.dah_res = 0;
		DSEQ_TO_DAHDR(drth->drth_dash, dp->seq_rcv);
		if (dp->state == DCCPS_SERVER_CLOSE) 
			drth->drth_reason = 1; 
		else
			drth->drth_reason = 2; 
		drth->drth_data1 = 0;
		drth->drth_data2 = 0;
		drth->drth_data3 = 0;
		optp = (u_char *)(drth + 1);
	} else if (extrah_len) {
		if (!use_shortseq){
			dalh = (struct dccp_acklhdr *)(dlh + 1);
			dalh->dash.dah_res = 0; /* Reserved field should be zero */

			if (dp->state == DCCPS_ESTAB) {
				DSEQ_TO_DAHDR(dalh->dash, dp->ack_snd);
				dp->ack_snd = 0;
			} else {
				DSEQ_TO_DAHDR(dalh->dash, dp->seq_rcv);
			}

			if (dh->dh_type == DCCP_TYPE_RESPONSE) {
				DCCP_DEBUG((LOG_INFO, "Sending dccp type response\n"));
				drqh = (struct dccp_requesthdr *)(dalh + 1);
				drqh->drqh_scode = dp->scode; 
				optp = (u_char *)(drqh + 1);
			} else 
				optp = (u_char *)(dalh + 1);
		} else {
			/* XXX shortseq */
			dah = (struct dccp_ackhdr *)(dh + 1);
			dah->dash.dah_res = 0; /* Reserved field should be zero */
			dah->dash.dah_ack = htonl(dp->seq_rcv) >> 8;
			optp = (u_char *)(dah + 1);
		}
		
	} else {
		optp = (u_char *)(dlh + 1);
	}

	if (optlen)
		bcopy(options, optp, optlen);

	m->m_pkthdr.len = hdrlen + len;

	if (dh->dh_cscov == 0) {
#ifdef INET6
		if (isipv6)
			cslen = (hdrlen - sizeof(struct ip6_hdr)) + len;
		else
			cslen = (hdrlen - sizeof(struct ip)) + len;
#else
		cslen = (hdrlen - sizeof(struct ip)) + len;
#endif
	} else {
		cslen = dh->dh_off * 4 + (dh->dh_cscov - 1) * 4;
#ifdef INET6
		if (isipv6) {
			if (cslen > (hdrlen - sizeof(struct ip6_hdr)) + len)
				cslen = (hdrlen - sizeof(struct ip6_hdr)) + len;
		} else {
			if (cslen > (hdrlen - sizeof(struct ip)) + len)
				cslen = (hdrlen - sizeof(struct ip)) + len;
		}
#else
		if (cslen > (hdrlen - sizeof(struct ip)) + len)
			cslen = (hdrlen - sizeof(struct ip)) + len;
#endif
	}

	/*
	 * Set up checksum 
	 */
#ifdef __FreeBSD__
	m->m_pkthdr.csum_flags = CSUM_IP; /* Do not allow the network card to calculate the checksum */
#elif defined(__NetBSD__)
	m->m_pkthdr.csum_flags = 0;
#else
	m->m_pkthdr.csum = 0;
#endif

	dh->dh_sum = 0;
#ifdef INET6
	if (isipv6) {
		dh->dh_sum = in6_cksum(m, IPPROTO_DCCP, sizeof(struct ip6_hdr),
		    cslen);
	} else
#endif
	{
		ip->ip_len = htons(hdrlen + len);
		ip->ip_ttl = dp->inp_ip_ttl;	/* XXX */
		ip->ip_tos = dp->inp_ip_tos;	/* XXX */

		dh->dh_sum = in4_cksum(m, IPPROTO_DCCP, sizeof(struct ip),
		    cslen);
#ifndef __OpenBSD__
		m->m_pkthdr.csum_data = offsetof(struct dccphdr, dh_sum);
#endif
	}

	dccpstat.dccps_opackets++;
	dccpstat.dccps_obytes += m->m_pkthdr.len;

#ifdef INET6
	if (isipv6) {
		DCCP_DEBUG((LOG_INFO, "Calling ip_output6, mbuf->m_len = %u, mbuf->m_pkthdr.len = %u\n", m->m_len, m->m_pkthdr.len));

		error = ip6_output(m, in6p->in6p_outputopts, &in6p->in6p_route,
		    (in6p->in6p_socket->so_options & SO_DONTROUTE), NULL, NULL,
		    NULL);
	} else
#endif
	{
		DCCP_DEBUG((LOG_INFO, "Calling ip_output, mbuf->m_len = %u, mbuf->m_pkthdr.len = %u\n", m->m_len, m->m_pkthdr.len));
		error = ip_output(m, inp->inp_options, &inp->inp_route,
		    (inp->inp_socket->so_options & SO_DONTROUTE), 0, 
				  inp->inp_socket); 
	}

	if (error) {
		DCCP_DEBUG((LOG_INFO, "IP output failed! %d\n", error));
		return (error);
	}

#if defined(INET6)
	if (isipv6) {
		sbdrop(&in6p->in6p_socket->so_snd, len);
		sowwakeup(in6p->in6p_socket);
	} else
#endif
	{
		sbdrop(&inp->inp_socket->so_snd, len);
		sowwakeup(inp->inp_socket);
	}

	if (dp->cc_in_use[0] > 0  && dp->state == DCCPS_ESTAB) {
		DCCP_DEBUG((LOG_INFO, "Calling *cc_sw[%u].cc_send_packet_sent!\n", dp->cc_in_use[0]));
		if (sendalot) {
			(*cc_sw[dp->cc_in_use[0]].cc_send_packet_sent)(dp->cc_state[0], 1,len);
			goto again;
		} else {
			(*cc_sw[dp->cc_in_use[0]].cc_send_packet_sent)(dp->cc_state[0], 0,len);
		}
	} else {
		if (sendalot)
			goto again;
	}

	DCCP_DEBUG((LOG_INFO, "dccp_output finished\n"));

	return (0);

release:
	m_freem(m);
	return (error);
}

int
dccp_abort(struct socket *so)
{
	struct inpcb *inp = 0;
	struct in6pcb *in6p;
	struct dccpcb *dp;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_abort!\n"));
	INP_INFO_WLOCK(&dccpbinfo);
	if (so->so_proto->pr_domain->dom_family == PF_INET6) {
		in6p = sotoin6pcb(so);
		if (in6p == 0) {
			return EINVAL;
		}
		inp = 0;
		dp = (struct dccpcb *)in6p->in6p_ppcb;
	} else {
		inp = sotoinpcb(so);
		if (inp == 0) {
			INP_INFO_WUNLOCK(&dccpbinfo);
			return EINVAL;
		}
		dp = (struct dccpcb *)inp->inp_ppcb;
	}

	dccp_disconnect2(dp);

	INP_INFO_WUNLOCK(&dccpbinfo);
	return 0;
}

static struct dccpcb *
dccp_close(struct dccpcb *dp)
{
	struct socket *so;
	struct inpcb *inp = dp->d_inpcb;
	struct in6pcb *in6p = dp->d_in6pcb;
	so = dptosocket(dp);

	DCCP_DEBUG((LOG_INFO, "Entering dccp_close!\n"));

	/* Stop all timers */
	callout_stop(&dp->connect_timer);
	callout_stop(&dp->retrans_timer);
	callout_stop(&dp->close_timer);
	callout_stop(&dp->timewait_timer);

	if (dp->cc_in_use[0] > 0)
		(*cc_sw[dp->cc_in_use[0]].cc_send_free)(dp->cc_state[0]);
	if (dp->cc_in_use[1] > 0)
		(*cc_sw[dp->cc_in_use[1]].cc_recv_free)(dp->cc_state[1]);

	pool_put(&dccpcb_pool, dp);
	if (inp) {
		inp->inp_ppcb = NULL;
		soisdisconnected(so);
		in_pcbdetach(inp);
	}
#if defined(INET6)
	else if (in6p) {
		in6p->in6p_ppcb = 0;
		soisdisconnected(so);
		in6_pcbdetach(in6p);
	}
#endif
	return ((struct dccpcb *)0);
}

/*
 * Runs when a new socket is created with the
 * socket system call or sonewconn.
 */
int
dccp_attach(struct socket *so, int proto)
{
	struct inpcb *inp = 0;
	struct in6pcb *in6p = 0;
	struct dccpcb *dp;
	int s, family, error = 0;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_attach(proto=%d)!\n", proto));
	INP_INFO_WLOCK(&dccpbinfo);
	s = splsoftnet();
	sosetlock(so);

	family = so->so_proto->pr_domain->dom_family;
	switch (family) {
	case PF_INET:
		inp = sotoinpcb(so);
		if (inp != 0) {
			error = EINVAL;
			goto out;
		}
		error = soreserve(so, dccp_sendspace, dccp_recvspace);
		if (error)
			goto out;
		error = in_pcballoc(so, &dccpbtable);
		if (error)
			goto out;
		inp = sotoinpcb(so);
		break;
#if defined(INET6)
	case PF_INET6:
		in6p = sotoin6pcb(so);
		if (in6p != 0) {
			error = EINVAL;
			goto out;
		}
		error = soreserve(so, dccp_sendspace, dccp_recvspace);
		if (error)
			goto out;
		error = in6_pcballoc(so, &dccpbtable);
		if (error)
			goto out;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		goto out;
	}

	if (inp)
		dp = dccp_newdccpcb(PF_INET, (void *)inp);
	else if (in6p)
		dp = dccp_newdccpcb(PF_INET6, (void *)in6p);
	else
		dp = NULL;

	if (dp == 0) {
		int nofd = so->so_state & SS_NOFDREF;
		so->so_state &= ~SS_NOFDREF;
#if defined(INET6)
		if (proto == PF_INET6) {
			in6_pcbdetach(in6p);
		} else
#endif
			in_pcbdetach(inp);
		so->so_state |= nofd;
		error = ENOBUFS;
		goto out;
	}

#ifdef INET6
	if (proto == PF_INET6) {
		DCCP_DEBUG((LOG_INFO, "We are a ipv6 socket!!!\n"));
		dp->inp_vflag |= INP_IPV6;
	} else 
#endif
		dp->inp_vflag |= INP_IPV4;
	dp->inp_ip_ttl = ip_defttl;

	dp->state = DCCPS_CLOSED;
out:
	splx(s);
	INP_INFO_WUNLOCK(&dccpbinfo);
	return error;
}

static int
dccp_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct inpcb *inp;
	int error;
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_bind!\n"));
	INP_INFO_WLOCK(&dccpbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EINVAL;
	}

	/* Do not bind to multicast addresses! */
	if (sin->sin_family == AF_INET &&
	    IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EAFNOSUPPORT;
	}
	INP_LOCK(inp);
	error = in_pcbbind(inp, sin, l);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&dccpbinfo);
	return error;
}

/*
 * Initiates a connection to a server
 * Called by the connect system call.
 */
static int
dccp_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct inpcb *inp;
	struct dccpcb *dp;
	int error;
	struct sockaddr_in *sin;
	char test[2];

	DCCP_DEBUG((LOG_INFO, "Entering dccp_connect!\n"));

	INP_INFO_WLOCK(&dccpbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	if (inp->inp_faddr.s_addr != INADDR_ANY) {
		INP_UNLOCK(inp);
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EISCONN;
	}

	dp = (struct dccpcb *)inp->inp_ppcb;

	if (dp->state == DCCPS_ESTAB) {
		DCCP_DEBUG((LOG_INFO, "Why are we in connect when we already have a established connection?\n"));
	}

	dp->who = DCCP_CLIENT;
	dp->seq_snd = (((u_int64_t)random() << 32) | random()) % 281474976710656LL;
	dp->ref_seq.hi = dp->seq_snd >> 24;
	dp->ref_seq.lo = (u_int64_t)(dp->seq_snd & 0xffffff);
	DCCP_DEBUG((LOG_INFO, "dccp_connect seq_snd %llu\n", dp->seq_snd));

	dccpstat.dccps_connattempt++;

	sin = (struct sockaddr_in *)nam;
	if (sin->sin_family == AF_INET
	    && IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
		error = EAFNOSUPPORT;
		goto bad;
	}

	error = dccp_doconnect(so, nam, l, 0);

	if (error != 0)
		goto bad;

	callout_reset(&dp->retrans_timer, dp->retrans, dccp_retrans_t, dp);
	callout_reset(&dp->connect_timer, DCCP_CONNECT_TIMER, dccp_connect_t, dp);

	if (dccp_do_feature_nego){
		test[0] = dp->pref_cc;
		dccp_add_feature(dp, DCCP_OPT_CHANGE_R, DCCP_FEATURE_CC, test, 1);
	}

	error = dccp_output(dp, 0);

bad:
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&dccpbinfo);
	return error;
}

static int
dccp_connect2(struct socket *so, struct socket *so2)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

/*
 *
 *
 */
int
dccp_doconnect(struct socket *so, struct sockaddr *nam,
    struct lwp *l, int isipv6)
{ 
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	int error = 0;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_doconnect!\n"));

#if defined(INET6)
	if (isipv6) {
		in6p = sotoin6pcb(so);
		inp = 0;
	} else
#endif
	{
		inp = sotoinpcb(so);
		in6p = 0;
	}

#if !defined(__NetBSD__) || !defined(INET6)
	if (inp->inp_lport == 0) {
#else
	if (isipv6 ? in6p->in6p_lport == 0 : inp->inp_lport == 0) {
#endif
#ifdef INET6
		if (isipv6) {
			DCCP_DEBUG((LOG_INFO, "Running in6_pcbbind!\n"));
			error = in6_pcbbind(in6p, NULL, l);
		} else
#endif /* INET6 */
		{
			error = in_pcbbind(inp, NULL, l);
		}
		if (error) {
			DCCP_DEBUG((LOG_INFO, "in_pcbbind=%d\n",error));
			return error;
		}
	}

#ifdef INET6
	if (isipv6) {
		error = in6_pcbconnect(in6p, (struct sockaddr_in6 *)nam, l);
		DCCP_DEBUG((LOG_INFO, "in6_pcbconnect=%d\n",error));
	} else
#endif
		error = in_pcbconnect(inp, (struct sockaddr_in *)nam, l);
	if (error) {
		DCCP_DEBUG((LOG_INFO, "in_pcbconnect=%d\n",error));
		return error;
	}

	soisconnecting(so);
	return error;
}

/*
 * Detaches the DCCP protocol from the socket.
 *
 */
int
dccp_detach(struct socket *so)
{
	struct inpcb *inp;
	struct in6pcb *in6p;
	struct dccpcb *dp;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_detach!\n"));
#ifdef INET6
	if (so->so_proto->pr_domain->dom_family == AF_INET6) {
		in6p = sotoin6pcb(so);
		if (in6p == 0) {
			return EINVAL;
		}
		dp = (struct dccpcb *)in6p->in6p_ppcb;
	} else
#endif
	{
		inp = sotoinpcb(so);
		if (inp == 0) {
			return EINVAL;
		}
		dp = (struct dccpcb *)inp->inp_ppcb;
	}
	if (! dccp_disconnect2(dp)) {
		INP_UNLOCK(inp);
	}
	INP_INFO_WUNLOCK(&dccpbinfo);
	return 0;
}

/*
 * 
 *
 */
int
dccp_disconnect(struct socket *so)
{
	struct inpcb *inp;
	struct in6pcb *in6p;
	struct dccpcb *dp;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_disconnect!\n"));
	INP_INFO_WLOCK(&dccpbinfo);
#ifndef __NetBSD__
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	if (inp->inp_faddr.s_addr == INADDR_ANY) {
		INP_INFO_WUNLOCK(&dccpbinfo);
		INP_UNLOCK(inp);
		return ENOTCONN;
	}

	dp = (struct dccpcb *)inp->inp_ppcb;
#else /* NetBSD */
#ifdef INET6
	if (so->so_proto->pr_domain->dom_family == AF_INET6) {
		in6p = sotoin6pcb(so);
		if (in6p == 0) {
			INP_INFO_WUNLOCK(&dccpbinfo);
			return EINVAL;
		}
		dp = (struct dccpcb *)in6p->in6p_ppcb;
	} else
#endif
	{
		inp = sotoinpcb(so);
		if (inp == 0) {
			return EINVAL;
		}
		dp = (struct dccpcb *)inp->inp_ppcb;
	}
#endif
	if (!dccp_disconnect2(dp)) {
		INP_UNLOCK(inp);
	}
	INP_INFO_WUNLOCK(&dccpbinfo);
	return 0;
}

/*
 * If we have don't have a established connection
 * we can call dccp_close, otherwise we can just
 * set SS_ISDISCONNECTED and flush the receive queue.
 */
static int
dccp_disconnect2(struct dccpcb *dp)
{
	struct socket *so = dptosocket(dp);

	DCCP_DEBUG((LOG_INFO, "Entering dccp_disconnect2!\n"));

	if (dp->state < DCCPS_ESTAB) {
		dccp_close(dp);
		return 1;
	} else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		if (dp->state == DCCPS_ESTAB) {
			dp->retrans = 100;
			callout_reset(&dp->retrans_timer, dp->retrans,
			    dccp_retrans_t, dp);
			callout_reset(&dp->close_timer, DCCP_CLOSE_TIMER,
			    dccp_close_t, dp);
			if (dp->who == DCCP_CLIENT) {
				dp->state = DCCPS_CLIENT_CLOSE;
			} else {
				dp->state = DCCPS_SERVER_CLOSE;
			}
			dccp_output(dp, 0);
		}
	}
	return 0;
}

int
dccp_send(struct socket *so, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct lwp *l)
{
	struct inpcb	*inp;
	struct dccpcb	*dp;
	int		error = 0;
	int		isipv6 = 0;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_send!\n"));
	KASSERT(solocked(so));
	KASSERT(m != NULL);

	if (control && control->m_len) {
		m_freem(control);
		m_freem(m);
		return EINVAL;
	}

#ifdef INET6
	isipv6 = addr && addr->sa_family == AF_INET6;
#endif

#if defined(INET6)
	if (so->so_proto->pr_domain->dom_family == AF_INET6) {
		struct in6pcb	*in6p;
		in6p = sotoin6pcb(so);
		if (in6p == 0) {
			error = EINVAL;
			goto release;
		}
		dp = (struct dccpcb *)in6p->in6p_ppcb;
	} else
#endif
	{
		INP_INFO_WLOCK(&dccpbinfo);
		inp = sotoinpcb(so);
		if (inp == 0) {
			error = EINVAL;
			goto release;
		}
		INP_LOCK(inp);
		dp = (struct dccpcb *)inp->inp_ppcb;
	}
	if (dp->state != DCCPS_ESTAB) {
		DCCP_DEBUG((LOG_INFO, "We have no established connection!\n"));
	}

	if (control != NULL) {
		DCCP_DEBUG((LOG_INFO, "We got a control message!\n"));
		/* Are we going to use control messages??? */
		if (control->m_len) {
			m_freem(control);
		}
	}

	if (sbspace(&so->so_snd) < -512) {
		INP_UNLOCK(inp);
		error = ENOBUFS;
		goto release;
	}

	if (m->m_pkthdr.len > dp->d_maxseg) {
		/* XXX we should calculate packet size more carefully */
		INP_UNLOCK(inp);
		error = EINVAL;
		goto release;
	}

	if (dp->pktcnt >= DCCP_MAX_PKTS) {
		INP_UNLOCK(inp);
		error = ENOBUFS;
		goto release;
	}

	sbappend(&so->so_snd, m);
	dp->pktlen[(dp->pktlenidx + dp->pktcnt) % DCCP_MAX_PKTS] = m->m_pkthdr.len; 
	dp->pktcnt ++;

	if (addr && dp->state == DCCPS_CLOSED) {
		error = dccp_doconnect(so, addr, l, isipv6);
		if (error)
			goto out;
	}

	error = dccp_output(dp, 0);

out:
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&dccpbinfo);
	return error; 

release:
	INP_INFO_WUNLOCK(&dccpbinfo);
	m_freem(m);
	return (error);
}

/*
 * Sets socket to SS_CANTSENDMORE 
 */
int
dccp_shutdown(struct socket *so)
{
	struct inpcb *inp;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_shutdown!\n"));
	INP_INFO_RLOCK(&dccpbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_RUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&dccpbinfo);
	socantsendmore(so);
	INP_UNLOCK(inp);
	return 0;
}

static int
dccp_listen(struct socket *so, struct lwp *td)
{
	struct inpcb *inp;
	struct dccpcb *dp;
	int error = 0;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_listen!\n"));

	INP_INFO_RLOCK(&dccpbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_RUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&dccpbinfo);
	dp = (struct dccpcb *)inp->inp_ppcb;
	if (inp->inp_lport == 0)
		error = in_pcbbind(inp, NULL, td);
	if (error == 0) {
		dp->state = DCCPS_LISTEN;
		dp->who = DCCP_LISTENER;
	}
	INP_UNLOCK(inp);
	return error;
}

/*
 * Accepts a connection (accept system call)
 */
static int
dccp_accept(struct socket *so, struct sockaddr *nam)
{
	struct inpcb *inp = NULL;
	int error = 0;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_accept!\n"));

	if (nam == NULL) {
		return EINVAL;
	}
	if (so->so_state & SS_ISDISCONNECTED) {
		DCCP_DEBUG((LOG_INFO, "so_state && SS_ISDISCONNECTED!, so->state = %i\n", so->so_state));
		return ECONNABORTED;
	}

	INP_INFO_RLOCK(&dccpbinfo);
	inp = sotoinpcb(so);
	if (inp == 0) {
		INP_INFO_RUNLOCK(&dccpbinfo);
		return EINVAL;
	}
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&dccpbinfo);
	in_setpeeraddr(inp, (struct sockaddr_in *)nam);

	return error;
}

/*
 * Initializes a new DCCP control block
 * (in_pcballoc in attach has already allocated memory for it)
 */
struct dccpcb *
dccp_newdccpcb(int family, void *aux)
{
	struct inpcb *inp;
	struct in6pcb *in6p;
	struct dccpcb	*dp;

	DCCP_DEBUG((LOG_INFO, "Creating a new dccpcb!\n"));

	dp = pool_get(&dccpcb_pool, PR_NOWAIT);
	if (dp == NULL)
		return NULL;
	bzero((char *) dp, sizeof(struct dccpcb));

	callout_init(&dp->connect_timer, 0);
	callout_init(&dp->retrans_timer, 0);
	callout_init(&dp->close_timer, 0);
	callout_init(&dp->timewait_timer, 0);

	dp->ndp = 0;
	dp->loss_window = 1000;
	dp->cslen = 0;
	dp->pref_cc = DEFAULT_CCID;
	dp->who = DCCP_UNDEF;
	dp->seq_snd = 0;
	dp->seq_rcv = 0;
	dp->shortseq = 0;
	dp->gsn_rcv = 281474976710656LL;
	dp->optlen = 0;
	if (dccp_do_feature_nego){
		dp->cc_in_use[0] = -1;
		dp->cc_in_use[1] = -1;
	} else {
		/* for compatibility with linux */
		dp->cc_in_use[0] = 4; 
		dp->cc_in_use[1] = 4;
	}
	dp->av_size = 0; /* no ack vector initially */
	dp->remote_ackvector = 0; /* no ack vector on remote side initially */
	dp->retrans = 200;
	dp->avgpsize = 0;
	dp->d_maxseg = 1400;
	dp->ref_pseq.hi = 0;
	dp->ref_pseq.lo = 0;
	dp->pktlenidx = 0;
	dp->pktcnt = 0;

	switch (family) {
	case PF_INET:
		inp = (struct inpcb *)aux;
		dp->d_inpcb = inp;
		inp->inp_ip.ip_ttl = ip_defttl;
		inp->inp_ppcb = dp;
		break;
	case PF_INET6:
		in6p = (struct in6pcb *)aux;
		dp->d_in6pcb = in6p;
		in6p->in6p_ip6.ip6_hlim = in6_selecthlim_rt(in6p);
		in6p->in6p_ppcb = dp;
		break;
	}
	
	if (!dccp_do_feature_nego){
		dp->cc_state[0] = (*cc_sw[4].cc_send_init)(dp);
		dp->cc_state[1] = (*cc_sw[4].cc_recv_init)(dp);
	}

	return dp;
}

int
dccp_add_option(struct dccpcb *dp, u_int8_t opt, char *val, u_int8_t val_len)
{
	return dccp_add_feature_option(dp, opt, 0, val, val_len);
}

int
dccp_add_feature_option(struct dccpcb *dp, u_int8_t opt, u_int8_t feature, char *val, u_int8_t val_len)
{
	int i;
	DCCP_DEBUG((LOG_INFO, "Entering dccp_add_feature_option, opt = %u, val_len = %u optlen %u\n", opt, val_len, dp->optlen));

	if (DCCP_MAX_OPTIONS > (dp->optlen + val_len + 2)) {
		dp->options[dp->optlen] = opt;
		if (opt < 32) {
			dp->optlen++;
		} else {
			if (opt == DCCP_OPT_CONFIRM_L && val_len) {
				dp->options[dp->optlen + 1] = val_len + 3;
				dp->options[dp->optlen +2] = feature;
				dp->optlen += 3;
			} else {
				dp->options[dp->optlen + 1] = val_len + 2;
				dp->optlen += 2;
			}
	
			for (i = 0; i<val_len; i++) {
				dp->options[dp->optlen] = val[i];
				dp->optlen++;
			}
		}
	} else {
		DCCP_DEBUG((LOG_INFO, "No room for more options, optlen = %u\n", dp->optlen));
		return -1;
	}

	return 0;
}

/*
 * Searches "options" for given option type. if found, the data is copied to buffer
 * and returns the data length.
 * Returns 0 if option type not found
 */
int
dccp_get_option(char *options, int optlen, int type, char *buffer, int buflen)
{
	int i, j, size;
	u_int8_t t;
	
	for (i=0; i < optlen;) {
		t = options[i++];
		if (t >= 32) {		  
			size = options[i++] - 2;
			if (t == type) {
				if (size > buflen)
					return 0;
				for (j = 0; j < size; j++)
					buffer[j] = options[i++];
				return size;
			}
			i += size;
		}
	}
	/* If we get here the options was not found */
	DCCP_DEBUG((LOG_INFO, "dccp_get_option option(%d) not found\n", type));
	return 0;
}

void
dccp_parse_options(struct dccpcb *dp, char *options, int optlen)
{
	u_int8_t opt, size, i, j;
	char val[8];

	for (i = 0; i < optlen; i++) {
		opt = options[i];

		DCCP_DEBUG((LOG_INFO, "Parsing opt: 0x%02x\n", opt));

		if (opt < 32) {
			switch (opt) {
			    case DCCP_OPT_PADDING:
				DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_PADDING!\n"));
				break;
			    case DCCP_OPT_DATA_DISCARD:
				DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_DATA_DISCARD!\n"));
				break;
			    case DCCP_OPT_SLOW_RECV:
				DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_SLOW_RECV!\n"));
				break;
			    case DCCP_OPT_BUF_CLOSED:
				DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_BUF_CLOSED!\n"));
				break;
			    default:
				DCCP_DEBUG((LOG_INFO, "Got an unknown option, option = %u!\n", opt));
			}
		} else if (opt > 32 && opt < 36) {
			size = options[i+ 1];
			if (size < 3 || size > 10) {
				DCCP_DEBUG((LOG_INFO, "Error, option size = %u\n", size));
				return;
			}
			/* Feature negotiations are options 33 to 35 */ 
			DCCP_DEBUG((LOG_INFO, "Got option %u, size = %u, feature = %u\n", opt, size, options[i+2]));
			bcopy(options + i + 3, val, size -3);
			DCCP_DEBUG((LOG_INFO, "Calling dccp_feature neg(%u, %u, options[%u + 1], %u)!\n", (u_int)dp, opt, i+ 1, (size - 3)));
			dccp_feature_neg(dp, opt, options[i+2], (size -3) , val);
			i += size - 1;

		} else if (opt < 128) {
			size = options[i+ 1];
			if (size < 3 || size > 10) {
				DCCP_DEBUG((LOG_INFO, "Error, option size = %u\n", size));
				return;
			}

			switch (opt) {
			    case DCCP_OPT_RECV_BUF_DROPS:
					DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_RECV_BUF_DROPS, size = %u!\n", size));
					for (j=2; j < size; j++) {
						DCCP_DEBUG((LOG_INFO, "val[%u] = %u ", j-1, options[i+j]));
					}
					DCCP_DEBUG((LOG_INFO, "\n"));
				break;

			    case DCCP_OPT_TIMESTAMP:
					DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_TIMESTAMP, size = %u\n", size));

					/* Adding TimestampEcho to next outgoing */
					bcopy(options + i + 2, val, 4);
					bzero(val + 4, 4);
					dccp_add_option(dp, DCCP_OPT_TIMESTAMP_ECHO, val, 8);
				break;
				
			    case DCCP_OPT_TIMESTAMP_ECHO:
					DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_TIMESTAMP_ECHO, size = %u\n",size));
					for (j=2; j < size; j++) {
						DCCP_DEBUG((LOG_INFO, "val[%u] = %u ", j-1, options[i+j]));
					}
					DCCP_DEBUG((LOG_INFO, "\n"));

					/*
						bcopy(options + i + 2, &(dp->timestamp_echo), 4);
						bcopy(options + i + 6, &(dp->timestamp_elapsed), 4);
						ACK_DEBUG((LOG_INFO, "DATA; echo = %u , elapsed = %u\n",
						   dp->timestamp_echo, dp->timestamp_elapsed));
					*/
				
				break;

			case DCCP_OPT_ACK_VECTOR0:
			case DCCP_OPT_ACK_VECTOR1:
			case DCCP_OPT_ELAPSEDTIME:
				/* Dont do nothing here. Let the CC deal with it */
				break;
				
			default:
				DCCP_DEBUG((LOG_INFO, "Got an unknown option, option = %u, size = %u!\n", opt, size));
				break;

			}
			i += size - 1;

		} else {
			DCCP_DEBUG((LOG_INFO, "Got a CCID option (%d), do nothing!\n", opt));
			size = options[i+ 1];
			if (size < 3 || size > 10) {
				DCCP_DEBUG((LOG_INFO, "Error, option size = %u\n", size));
				return;
			}
			i += size - 1;
		}
	}

}

int
dccp_add_feature(struct dccpcb *dp, u_int8_t opt, u_int8_t feature, char *val, u_int8_t val_len)
{
	int i;
	DCCP_DEBUG((LOG_INFO, "Entering dccp_add_feature, opt = %u, feature = %u, val_len = %u\n", opt, feature, val_len));

	if (DCCP_MAX_OPTIONS > (dp->featlen + val_len + 3)) {
		dp->features[dp->featlen] = opt;
		dp->features[dp->featlen + 1] = val_len + 3;
		dp->features[dp->featlen +2] = feature;
		dp->featlen += 3;
		for (i = 0; i<val_len; i++) {
			dp->features[dp->featlen] = val[i];
			dp->featlen++;
		}
	} else {
		DCCP_DEBUG((LOG_INFO, "No room for more features, featlen = %u\n", dp->featlen));
		return -1;
	}

	return 0;
}

int
dccp_remove_feature(struct dccpcb *dp, u_int8_t opt, u_int8_t feature)
{
	int i = 0, j = 0, k;
	u_int8_t t_opt, t_feature, len;
	DCCP_DEBUG((LOG_INFO, "Entering dccp_remove_feature, featlen = %u, opt = %u, feature = %u\n", dp->featlen, opt, feature));

	while (i < dp->featlen) {
		t_opt = dp->features[i];
		len = dp->features[i+ 1];

		if (i + len > dp->featlen) {
			DCCP_DEBUG((LOG_INFO, "Error, len = %u and i(%u) + len > dp->featlen (%u)\n", len, i, dp->featlen));
			return 1;
		}
		t_feature = dp->features[i+2];

		if (t_opt == opt && t_feature == feature) {
			i += len;
		} else {
			if (i != j) {
				for (k = 0; k < len; k++) {
					dp->features[j+k] = dp->features[i+k];
				}
			}
			i += len;
			j += len;
		}
	}
	dp->featlen = j;
	DCCP_DEBUG((LOG_INFO, "Exiting dccp_remove_feature, featlen = %u\n", dp->featlen));
	return 0;
}

void
dccp_feature_neg(struct dccpcb *dp, u_int8_t opt, u_int8_t feature, u_int8_t val_len, char *val)
{
	DCCP_DEBUG((LOG_INFO, "Running dccp_feature_neg, opt = %u, feature = %u len = %u ", opt, feature, val_len));

	switch (feature) {
		case DCCP_FEATURE_CC:
			DCCP_DEBUG((LOG_INFO, "Got CCID negotiation, opt = %u, val[0] = %u\n", opt, val[0]));
			if (opt == DCCP_OPT_CHANGE_R) {
				if (val[0] == 2 || val[0] == 3 || val[0] == 0) {
					/* try to use preferable CCID */
					int i;
					for (i = 1; i < val_len; i ++) if (val[i] == dp->pref_cc) val[0] = dp->pref_cc;
					DCCP_DEBUG((LOG_INFO, "Sending DCCP_OPT_CONFIRM_L on CCID %u\n", val[0]));
					dccp_remove_feature(dp, DCCP_OPT_CONFIRM_L, DCCP_FEATURE_CC);
					dccp_add_feature_option(dp, DCCP_OPT_CONFIRM_L, DCCP_FEATURE_CC , val, 1);
					if (dp->cc_in_use[0] < 1) {
						dp->cc_state[0] = (*cc_sw[val[0] + 1].cc_send_init)(dp);
						dp->cc_in_use[0] = val[0] + 1;
					} else {
						DCCP_DEBUG((LOG_INFO, "We already have negotiated a CC!!!\n"));
					}
				}
			} else if (opt == DCCP_OPT_CONFIRM_L) {
				DCCP_DEBUG((LOG_INFO, "Got DCCP_OPT_CONFIRM_L on CCID %u\n", val[0]));
				dccp_remove_feature(dp, DCCP_OPT_CHANGE_R, DCCP_FEATURE_CC);
				if (dp->cc_in_use[1] < 1) {
					dp->cc_state[1] = (*cc_sw[val[0] + 1].cc_recv_init)(dp);
					dp->cc_in_use[1] = val[0] + 1;
					DCCP_DEBUG((LOG_INFO, "confirmed cc_in_use[1] = %d\n", dp->cc_in_use[1]));
				} else {
					DCCP_DEBUG((LOG_INFO, "We already have negotiated a CC!!! (confirm) %d\n", dp->cc_in_use[1]));
				}
			}
		
		break;

		case DCCP_FEATURE_ACKVECTOR:
			ACK_DEBUG((LOG_INFO, "Got _Use Ack Vector_\n"));
			if (opt == DCCP_OPT_CHANGE_R) {
				if (val[0] == 1) {
					dccp_use_ackvector(dp);
					dccp_remove_feature(dp, DCCP_OPT_CONFIRM_L, DCCP_FEATURE_ACKVECTOR);
					dccp_add_feature_option(dp, DCCP_OPT_CONFIRM_L, DCCP_FEATURE_ACKVECTOR , val, 1);
				} else {
					ACK_DEBUG((LOG_INFO, "ERROR. Strange val %u\n", val[0]));
				}
			} else if (opt == DCCP_OPT_CONFIRM_L) {
					dccp_remove_feature(dp, DCCP_OPT_CONFIRM_L, DCCP_FEATURE_ACKVECTOR);
			if (val[0] == 1) {
					dp->remote_ackvector = 1;
					ACK_DEBUG((LOG_INFO,"Remote side confirmed AckVector usage\n"));
				} else {
					ACK_DEBUG((LOG_INFO, "ERROR. Strange val %u\n", val[0]));
				}
			}
			break;
			
		case DCCP_FEATURE_ACKRATIO:
			if (opt == DCCP_OPT_CHANGE_R) {
				bcopy(val , &(dp->ack_ratio), 1);
				ACK_DEBUG((LOG_INFO, "Feature: Change Ack Ratio to %u\n", dp->ack_ratio));
			}
			break;
			
		case DCCP_FEATURE_ECN:
		case DCCP_FEATURE_MOBILITY:
		default:
			/* we should send back empty CONFIRM_L for unknown feature unless it's not mandatory */
			dccp_add_option(dp, DCCP_OPT_CONFIRM_L, NULL, 0);
		break;

	}
}

#ifdef __FreeBSD__
static int
dccp_pcblist(SYSCTL_HANDLER_ARGS)
{

	int error, i, n, s;
	struct inpcb *inp, **inp_list;
	inp_gen_t gencnt;
	struct xinpgen xig;

	/*
	 * The process of preparing the TCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == 0) {
		n = dccpbinfo.ipi_count;
		req->oldidx = 2 * (sizeof xig)
			+ (n + n/8) * sizeof(struct xdccpcb);
		return 0;
	}


	if (req->newptr != 0)
		return EPERM;


	/*
	 * OK, now we're committed to doing something.
	 */
	s = splnet();
	gencnt = dccpbinfo.ipi_gencnt;
	n = dccpbinfo.ipi_count;
	splx(s);

#if __FreeBSD_version >= 500000
	sysctl_wire_old_buffer(req, 2 * (sizeof xig)
		+ n * sizeof(struct xdccpcb));
#endif

	xig.xig_len = sizeof xig;
	xig.xig_count = n;
	xig.xig_gen = gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return error;

	inp_list = malloc(n * sizeof *inp_list, M_TEMP, M_WAITOK);
	if (inp_list == 0)
		return ENOMEM;
	
	s = splsoftnet();
	INP_INFO_RLOCK(&dccpbinfo);

	for (inp = LIST_FIRST(dccpbinfo.listhead), i = 0; inp && i < n;
	     inp = LIST_NEXT(inp, inp_list)) {
		INP_LOCK(inp);
		if (inp->inp_gencnt <= gencnt &&
#if __FreeBSD_version >= 500000
		    cr_canseesocket(req->td->td_ucred, inp->inp_socket) == 0)
#else
		    !prison_xinpcb(req->p, inp))
#endif
			inp_list[i++] = inp;
		INP_UNLOCK(inp);
	}
	INP_INFO_RUNLOCK(&dccpbinfo);
	splx(s);
	n = i;

	error = 0;
	for (i = 0; i < n; i++) {
		inp = inp_list[i];
		INP_LOCK(inp);

		if (inp->inp_gencnt <= gencnt) {
			struct xdccpcb xd;
			vaddr_t inp_ppcb;
			xd.xd_len = sizeof xd;
			/* XXX should avoid extra copy */
			bcopy(inp, &xd.xd_inp, sizeof *inp);
			inp_ppcb = inp->inp_ppcb;
			if (inp_ppcb != NULL)
				bcopy(inp_ppcb, &xd.xd_dp, sizeof xd.xd_dp);
			else
				bzero((char *) &xd.xd_dp, sizeof xd.xd_dp);
			if (inp->inp_socket)
				 sotoxsocket(inp->inp_socket, &xd.xd_socket);
			error = SYSCTL_OUT(req, &xd, sizeof xd);
		}
		INP_UNLOCK(inp);
	}
	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		s = splnet();
		INP_INFO_RLOCK(&dccpbinfo);
		xig.xig_gen = dccpbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = dccpbinfo.ipi_count;


		INP_INFO_RUNLOCK(&dccpbinfo);
		splx(s);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}
	free(inp_list, M_TEMP);
	return error;
}
#endif

#ifdef __FreeBSD__
SYSCTL_PROC(_net_inet_dccp, DCCPCTL_PCBLIST, pcblist, CTLFLAG_RD, 0, 0,
    dccp_pcblist, "S,xdccpcb", "List of active DCCP sockets");
#endif

void
dccp_timewait_t(void *dcb)
{
	struct dccpcb *dp = dcb;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_timewait_t!\n"));
	mutex_enter(softnet_lock);
	INP_INFO_WLOCK(&dccpbinfo);
	INP_LOCK(dp->d_inpcb);
	dccp_close(dp);
	INP_INFO_WUNLOCK(&dccpbinfo);
	mutex_exit(softnet_lock);
}

void
dccp_connect_t(void *dcb)
{
	struct dccpcb *dp = dcb;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_connect_t!\n"));
	mutex_enter(softnet_lock);
	INP_INFO_WLOCK(&dccpbinfo);
	INP_LOCK(dp->d_inpcb);
	dccp_close(dp);
	INP_INFO_WUNLOCK(&dccpbinfo);
	mutex_exit(softnet_lock);
}

void
dccp_close_t(void *dcb)
{
	struct dccpcb *dp = dcb;

	DCCP_DEBUG((LOG_INFO, "Entering dccp_close_t!\n"));
	mutex_enter(softnet_lock);
	INP_INFO_WLOCK(&dccpbinfo);
	dp->state = DCCPS_TIME_WAIT; /* HMM */
	if (dp->who == DCCP_SERVER) {
		INP_LOCK(dp->d_inpcb);
		KERNEL_LOCK(1, NULL);
		dccp_output(dp, DCCP_TYPE_RESET + 2);
		KERNEL_UNLOCK_ONE(NULL);
		dccp_close(dp);
	} else {
		INP_LOCK(dp->d_inpcb);
		dccp_output(dp, DCCP_TYPE_RESET + 2);
		/*dp->state = DCCPS_TIME_WAIT; */
		callout_reset(&dp->timewait_timer, DCCP_TIMEWAIT_TIMER,
		    dccp_timewait_t, dp);
		INP_UNLOCK(dp->d_inpcb);
	}
	INP_INFO_WUNLOCK(&dccpbinfo);
	mutex_exit(softnet_lock);
}

void
dccp_retrans_t(void *dcb)
{
	struct dccpcb *dp = dcb;
	/*struct inpcb *inp;*/

	DCCP_DEBUG((LOG_INFO, "Entering dccp_retrans_t!\n"));
	mutex_enter(softnet_lock);
	INP_INFO_RLOCK(&dccpbinfo);
	/*inp = dp->d_inpcb;*/
	INP_LOCK(inp);
	INP_INFO_RUNLOCK(&dccpbinfo);
	callout_stop(&dp->retrans_timer);
	KERNEL_LOCK(1, NULL);
	dccp_output(dp, 0);
	KERNEL_UNLOCK_ONE(NULL);
	dp->retrans = dp->retrans * 2;
	callout_reset(&dp->retrans_timer, dp->retrans, dccp_retrans_t, dp);
	INP_UNLOCK(inp);
	mutex_exit(softnet_lock);
}

static int
dccp_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	int error = 0;
	int family;

	family = so->so_proto->pr_domain->dom_family;
	switch (family) {
	case PF_INET:
		error = in_control(so, cmd, nam, ifp);
		break;
#ifdef INET6
	case PF_INET6:
		error = in6_control(so, cmd, nam, ifp);
		break;
#endif
	default:
		error =	 EAFNOSUPPORT;
	}
	return (error);
}

static int
dccp_stat(struct socket *so, struct stat *ub)
{
	return 0;
}

static int
dccp_peeraddr(struct socket *so, struct sockaddr *nam)
{

	KASSERT(solocked(so));
	KASSERT(sotoinpcb(so) != NULL);
	KASSERT(nam != NULL);

	in_setpeeraddr(sotoinpcb(so), (struct sockaddr_in *)nam);
	return 0;
}

static int
dccp_sockaddr(struct socket *so, struct sockaddr *nam)
{

	KASSERT(solocked(so));
	KASSERT(sotoinpcb(so) != NULL);
	KASSERT(nam != NULL);

	in_setsockaddr(sotoinpcb(so), (struct sockaddr_in *)nam);
	return 0;
}

static int
dccp_rcvd(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
dccp_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
dccp_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	m_freem(m);
	m_freem(control);

	return EOPNOTSUPP;
}

static int
dccp_purgeif(struct socket *so, struct ifnet *ifp)
{
	int s;

	s = splsoftnet();
	mutex_enter(softnet_lock);
	in_pcbpurgeif0(&dccpbtable, ifp);
	in_purgeif(ifp);
	in_pcbpurgeif(&dccpbtable, ifp);
	mutex_exit(softnet_lock);
	splx(s);

	return 0;
}

/****** Ack Vector functions *********/

/**
 * Initialize and allocate mem for Ack Vector
 **/
void
dccp_use_ackvector(struct dccpcb *dp)
{
	DCCP_DEBUG((LOG_INFO,"Initializing AckVector\n"));
	if (dp->ackvector != 0) {
		DCCP_DEBUG((LOG_INFO, "It was already initialized!!!\n"));
		return;
	}
	dp->av_size = DCCP_VECTORSIZE;
	/* need 2 bits per entry */
	dp->ackvector = malloc(dp->av_size/4, M_PCB, M_NOWAIT | M_ZERO);
	if (dp->ackvector == 0) {
		DCCP_DEBUG((LOG_INFO, "Unable to allocate memory for ackvector\n"));
		/* What to do now? */
		dp->av_size = 0;
		return;
	}
	memset(dp->ackvector, 0xff, dp->av_size/4);
	dp->av_hs = dp->av_ts = 0;
	dp->av_hp = dp->ackvector;
}

/**
 * Set 'seqnr' as the new head in ackvector
 **/
void
dccp_update_ackvector(struct dccpcb *dp, u_int64_t seqnr)
{
	int64_t gap;
	u_char *t;

	/* Ignore wrapping for now */
	
	ACK_DEBUG((LOG_INFO,"New head in ackvector: %u\n", seqnr));
	
	if (dp->av_size == 0) {
		ACK_DEBUG((LOG_INFO, "Update: AckVector NOT YET INITIALIZED!!!\n"));
		dccp_use_ackvector(dp);
	}
	
	if (seqnr > dp->av_hs) {
		gap = seqnr - dp->av_hs;
	} else {
		/* We received obsolete information */
		return;
	}
	
	t = dp->av_hp + (gap/4);
	if (t >= (dp->ackvector + (dp->av_size/4)))
		t -= (dp->av_size / 4); /* ackvector wrapped */
	dp->av_hp = t;
	dp->av_hs = seqnr;
}

/**
 * We've received a packet. store in local av so it's included in
 * next Ack Vector sent
 **/
void
dccp_increment_ackvector(struct dccpcb *dp, u_int64_t seqnr)
{
	u_int64_t offset, dc;
	int64_t gap;
	u_char *t, *n;
	
	DCCP_DEBUG((LOG_INFO, "Entering dccp_increment_ackvecktor %d\n", dp->av_size));
	if (dp->av_size == 0) {
		DCCP_DEBUG((LOG_INFO, "Increment: AckVector NOT YET INITIALIZED!!!\n"));
		dccp_use_ackvector(dp);
	}
	
	if (dp->av_hs == dp->av_ts) {
		/* Empty ack vector */
		dp->av_hs = dp->av_ts = seqnr;
	}

	/* Check for wrapping */
	if (seqnr >= dp->av_hs) {
		/* Not wrapped */
		gap = seqnr - dp->av_hs;
	} else {
		/* Wrapped */
		gap = seqnr + 0x1000000000000LL - dp->av_hs; /* seqnr = 48 bits */
	}
	DCCP_DEBUG((LOG_INFO, "dccp_increment_ackvecktor gap=%llu av_size %d\n", gap, dp->av_size));

	if (gap >= dp->av_size) {
		/* gap is bigger than ackvector size? baaad */
		/* maybe we should increase the ackvector here */
		DCCP_DEBUG((LOG_INFO, "increment_ackvector error. gap: %llu, av_size: %d, seqnr: %d\n",
			    gap, dp->av_size, seqnr));
		return;
	}
	
	offset = gap % 4; /* hi or low 2 bits to mark */
	t = dp->av_hp + (gap/4);
	if (t >= (dp->ackvector + (dp->av_size/4)))
		t -= (dp->av_size / 4); /* ackvector wrapped */
	
	*t = *t & (~(0x03 << (offset *2))); /* turn off bits, 00 is rcvd, 11 is missing */

	dp->av_ts = seqnr + 1;
	if (dp->av_ts == 0x1000000000000LL)
		dp->av_ts = 0;

	if (gap > (dp->av_size - 128)) {
		n = malloc(dp->av_size/2, M_PCB, M_NOWAIT | M_ZERO); /* old size * 2 */
		memset (n + dp->av_size / 4, 0xff, dp->av_size / 4); /* new half all missing */
		dc = (dp->ackvector + (dp->av_size/4)) - dp->av_hp;
		memcpy (n, dp->av_hp, dc); /* tail to end */
		memcpy (n+dc, dp->ackvector, dp->av_hp - dp->ackvector); /* start to tail */
		dp->av_size = dp->av_size * 2; /* counted in items, so it';s a doubling */
		free (dp->ackvector, M_PCB);
		dp->av_hp = dp->ackvector = n;
	}
}

/**
 * Generates the ack vector to send in outgoing packet.
 * These are backwards (first packet in ack vector is packet indicated by Ack Number,
 * subsequent are older packets).
 **/

u_int16_t
dccp_generate_ackvector(struct dccpcb *dp, u_char *buf)
{
	int64_t j;
	u_int64_t i;
	u_int16_t cnt, oldlen, bufsize;
	u_char oldstate, st;

	bufsize = 16;
	cnt = 0;

	oldstate = 0x04; /* bad value */
	oldlen = 0;
	
	if (dp->av_size == 0) {
		ACK_DEBUG((LOG_INFO, "Generate: AckVector NOT YET INITIALIZED!!!\n"));
		return 0;
	}

	if (dp->seq_rcv > dp->av_ts) {
		/* AckNum is beyond our av-list , so we'll start with some
		 * 0x3 (Packet not yet received) */
		j = dp->seq_rcv - dp->av_ts -1;
		do {
			/* state | length */
			oldstate = 0x03;
			if (j > 63)
				oldlen = 63;
			else
				oldlen = j;
			
			buf[cnt] = (0x03 << 6) | oldlen;
			cnt++;
			if (cnt == bufsize) {
				/* I've skipped the realloc bshit */
				/* PANIC */
			}
			j-=63;
		} while (j > 0);
	}
	
	/* Ok now we're at dp->av_ts (unless AckNum is lower) */
	i = (dp->seq_rcv < dp->av_ts) ? dp->seq_rcv : dp->av_ts;
	st = dccp_ackvector_state(dp, i);

	if (st == oldstate) {
		cnt--;
		oldlen++;
	} else {
		oldlen = 0;
		oldstate = st;
	}

	if (dp->av_ts > dp->av_hs) {
		do {
			i--;
			st = dccp_ackvector_state(dp, i);
			if (st == oldstate && oldlen < 64) {
				oldlen++;
			} else {
				buf[cnt] = (oldstate << 6) | (oldlen & 0x3f);
				cnt++;
				oldlen = 0;
				oldstate = st;
				if (cnt == bufsize) {
					/* PANIC */
				}
			}
			
		} while (i > dp->av_hs);
	} else {
		/* It's wrapped */
		do {
			i--;
			st = dccp_ackvector_state(dp, i);
			if (st == oldstate && oldlen < 64) {
				oldlen++;
			} else {
				buf[cnt] = (oldstate << 6) | (oldlen & 0x3f);
				cnt++;
				oldlen = 0;
				oldstate = st;
				if (cnt == bufsize) {
					/* PANIC */
				}
			}
			
		} while (i > 0);
		i = 0x1000000;
		do {
			i--;
			st = dccp_ackvector_state(dp, i);
			if (st == oldstate && oldlen < 64) {
				oldlen++;
			} else {
				buf[cnt] = (oldstate << 6) | (oldlen & 0x3f);
				cnt++;
				oldlen = 0;
				oldstate = st;
				if (cnt == bufsize) {
					/* PANIC */
				}
			}
		} while (i > dp->av_hs);
	}
	
	/* add the last one */
	buf[cnt] = (oldstate << 6) | (oldlen & 0x3f);
	cnt++;

	return cnt;
}

u_char
dccp_ackvector_state(struct dccpcb *dp, u_int64_t seqnr)
{
	u_int64_t gap, offset;
	u_char *t;

	/* Check for wrapping */
	if (seqnr >= dp->av_hs) {
		/* Not wrapped */
		gap = seqnr - dp->av_hs;
	} else {
		/* Wrapped */
		gap = seqnr + 0x1000000000000LL - dp->av_hs; /* seq nr = 48 bits */
	}

	if (gap >= dp->av_size) {
		/* gap is bigger than ackvector size? baaad */
		return 0x03;
	}

	offset = gap % 4 *2;
	t = dp->av_hp + (gap/4);
	if (t >= (dp->ackvector + (dp->av_size/4)))
		t -= (dp->av_size / 4); /* wrapped */
	
	return ((*t & (0x03 << offset)) >> offset);
}

/****** End of Ack Vector functions *********/

/* No cc functions */
void *
dccp_nocc_init(struct dccpcb *pcb)
{
  return (void*) 1;
}

void
dccp_nocc_free(void *ccb)
{
}

int
dccp_nocc_send_packet(void *ccb, long size)
{
  return 1;
}

void
dccp_nocc_send_packet_sent(void *ccb, int moreToSend, long size)
{
}

void
dccp_nocc_packet_recv(void *ccb, char* options ,int optlen)
{
}

void
dccp_log(int level, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vprintf(format,  ap);
	va_end(ap);
	return;
}

/*
 * Sysctl for dccp variables.
 */
SYSCTL_SETUP(sysctl_net_inet_dccp_setup, "sysctl net.inet.dccp subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "net", NULL,
		NULL, 0, NULL, 0,
		CTL_NET, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "inet", NULL,
		NULL, 0, NULL, 0,
		CTL_NET, PF_INET, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "dccp",
		SYSCTL_DESCR("DCCPv4 related settings"),
		NULL, 0, NULL, 0,
		CTL_NET, PF_INET, IPPROTO_DCCP, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		CTLTYPE_INT, "dccp_log_in_vain",
		SYSCTL_DESCR("log all connection attempt"),
		NULL, 0, &dccp_log_in_vain, 0,
		CTL_NET, PF_INET, IPPROTO_DCCP, DCCPCTL_LOGINVAIN,
		CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		CTLTYPE_INT, "do_feature_nego",
		SYSCTL_DESCR("enable feature negotiation"),
		NULL, 0, &dccp_do_feature_nego, 0,
		CTL_NET, PF_INET, IPPROTO_DCCP, DCCPCTL_DOFEATURENEGO,
		CTL_EOL);
}

PR_WRAP_USRREQS(dccp)
#define	dccp_attach	dccp_attach_wrapper
#define	dccp_detach	dccp_detach_wrapper
#define dccp_accept	dccp_accept_wrapper
#define	dccp_bind	dccp_bind_wrapper
#define	dccp_listen	dccp_listen_wrapper
#define	dccp_connect	dccp_connect_wrapper
#define	dccp_connect2	dccp_connect2_wrapper
#define	dccp_disconnect	dccp_disconnect_wrapper
#define	dccp_shutdown	dccp_shutdown_wrapper
#define	dccp_abort	dccp_abort_wrapper
#define	dccp_ioctl	dccp_ioctl_wrapper
#define	dccp_stat	dccp_stat_wrapper
#define	dccp_peeraddr	dccp_peeraddr_wrapper
#define	dccp_sockaddr	dccp_sockaddr_wrapper
#define	dccp_rcvd	dccp_rcvd_wrapper
#define	dccp_recvoob	dccp_recvoob_wrapper
#define	dccp_send	dccp_send_wrapper
#define	dccp_sendoob	dccp_sendoob_wrapper
#define	dccp_purgeif	dccp_purgeif_wrapper

const struct pr_usrreqs dccp_usrreqs = {
	.pr_attach	= dccp_attach,
	.pr_detach	= dccp_detach,
	.pr_accept	= dccp_accept,
	.pr_bind	= dccp_bind,
	.pr_listen	= dccp_listen,
	.pr_connect	= dccp_connect,
	.pr_connect2	= dccp_connect2,
	.pr_disconnect	= dccp_disconnect,
	.pr_shutdown	= dccp_shutdown,
	.pr_abort	= dccp_abort,
	.pr_ioctl	= dccp_ioctl,
	.pr_stat	= dccp_stat,
	.pr_peeraddr	= dccp_peeraddr,
	.pr_sockaddr	= dccp_sockaddr,
	.pr_rcvd	= dccp_rcvd,
	.pr_recvoob	= dccp_recvoob,
	.pr_send	= dccp_send,
	.pr_sendoob	= dccp_sendoob,
	.pr_purgeif	= dccp_purgeif,
};
