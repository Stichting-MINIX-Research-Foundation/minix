/*	$NetBSD: in6_pcb.h,v 1.46 2015/05/24 15:43:45 rtr Exp $	*/
/*	$KAME: in6_pcb.h,v 1.45 2001/02/09 05:59:46 itojun Exp $	*/

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

/*
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)in_pcb.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET6_IN6_PCB_H_
#define _NETINET6_IN6_PCB_H_

#include <sys/queue.h>
#include <netinet/in_pcb_hdr.h>
#include <netinet/ip6.h>

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct icmp6_filter;

struct	in6pcb {
	struct inpcb_hdr in6p_head;
#define in6p_hash	 in6p_head.inph_hash
#define in6p_queue	 in6p_head.inph_queue
#define in6p_af		 in6p_head.inph_af
#define in6p_ppcb	 in6p_head.inph_ppcb
#define in6p_state	 in6p_head.inph_state
#define in6p_portalgo	 in6p_head.inph_portalgo
#define in6p_socket	 in6p_head.inph_socket
#define in6p_table	 in6p_head.inph_table
#define in6p_sp		 in6p_head.inph_sp
	struct	route in6p_route;	/* placeholder for routing entry */
	u_int16_t in6p_fport;		/* foreign port */
	u_int16_t in6p_lport;		/* local port */
	u_int32_t in6p_flowinfo;	/* priority and flowlabel */
	int	in6p_flags;		/* generic IP6/datagram flags */
	int	in6p_hops;		/* default hop limit */
	struct	ip6_hdr in6p_ip6;	/* header prototype */
	struct	mbuf *in6p_options;   /* IP6 options */
	struct	ip6_pktopts *in6p_outputopts; /* IP6 options for outgoing packets */
	struct	ip6_moptions *in6p_moptions; /* IP6 multicast options */
	struct icmp6_filter *in6p_icmp6filt;
	int	in6p_cksum;		/* IPV6_CHECKSUM setsockopt */
	bool    in6p_bindportonsend;
	struct ip_moptions *in6p_v4moptions;/* IP4 multicast options */
};

#define in6p_faddr	in6p_ip6.ip6_dst
#define in6p_laddr	in6p_ip6.ip6_src

/* states in inp_state: */
#define	IN6P_ATTACHED		INP_ATTACHED
#define	IN6P_BOUND		INP_BOUND
#define	IN6P_CONNECTED		INP_CONNECTED

/*
 * Flags in in6p_flags
 * We define KAME's original flags in higher 16 bits as much as possible
 * for compatibility with *bsd*s.
 */
#define IN6P_RECVOPTS		0x001000 /* receive incoming IP6 options */
#define IN6P_RECVRETOPTS	0x002000 /* receive IP6 options for reply */
#define IN6P_RECVDSTADDR	0x004000 /* receive IP6 dst address */
#define IN6P_IPV6_V6ONLY	0x008000 /* restrict AF_INET6 socket for v6 */
#define IN6P_PKTINFO		0x010000 /* receive IP6 dst and I/F */
#define IN6P_HOPLIMIT		0x020000 /* receive hoplimit */
#define IN6P_HOPOPTS		0x040000 /* receive hop-by-hop options */
#define IN6P_DSTOPTS		0x080000 /* receive dst options after rthdr */
#define IN6P_RTHDR		0x100000 /* receive routing header */
#define IN6P_RTHDRDSTOPTS	0x200000 /* receive dstoptions before rthdr */
#define IN6P_TCLASS		0x400000 /* traffic class */

#define IN6P_HIGHPORT		0x1000000 /* user wants "high" port binding */
#define IN6P_LOWPORT		0x2000000 /* user wants "low" port binding */
#define IN6P_ANONPORT		0x4000000 /* port chosen for user */
#define IN6P_FAITH		0x8000000 /* accept FAITH'ed connections */

#define IN6P_RFC2292		0x40000000 /* RFC2292 */
#define IN6P_MTU		0x80000000 /* use minimum MTU */

#define IN6P_CONTROLOPTS	(IN6P_PKTINFO|IN6P_HOPLIMIT|IN6P_HOPOPTS|\
				 IN6P_DSTOPTS|IN6P_RTHDR|IN6P_RTHDRDSTOPTS|\
				 IN6P_TCLASS|IN6P_RFC2292|\
				 IN6P_MTU)

#ifdef _KERNEL
/* compute hash value for foreign and local in6_addr and port */
#define IN6_HASH(faddr, fport, laddr, lport) 			\
	(((faddr)->s6_addr32[0] ^ (faddr)->s6_addr32[1] ^	\
	  (faddr)->s6_addr32[2] ^ (faddr)->s6_addr32[3] ^	\
	  (laddr)->s6_addr32[0] ^ (laddr)->s6_addr32[1] ^	\
	  (laddr)->s6_addr32[2] ^ (laddr)->s6_addr32[3])	\
	 + (fport) + (lport))

#define sotoin6pcb(so)	((struct in6pcb *)(so)->so_pcb)

void	in6_losing(struct in6pcb *);
void	in6_pcbinit(struct inpcbtable *, int, int);
int	in6_pcballoc(struct socket *, void *);
int	in6_pcbbind(void *, struct sockaddr_in6 *, struct lwp *);
int	in6_pcbconnect(void *, struct sockaddr_in6 *, struct lwp *);
void	in6_pcbdetach(struct in6pcb *);
void	in6_pcbdisconnect(struct in6pcb *);
struct	in6pcb *in6_pcblookup_port(struct inpcbtable *, struct in6_addr *,
				   u_int, int, struct vestigial_inpcb *);
int	in6_pcbnotify(struct inpcbtable *, const struct sockaddr *,
	u_int, const struct sockaddr *, u_int, int, void *,
	void (*)(struct in6pcb *, int));
void	in6_pcbpurgeif0(struct inpcbtable *, struct ifnet *);
void	in6_pcbpurgeif(struct inpcbtable *, struct ifnet *);
void	in6_pcbstate(struct in6pcb *, int);
void	in6_rtchange(struct in6pcb *, int);
void	in6_setpeeraddr(struct in6pcb *, struct sockaddr_in6 *);
void	in6_setsockaddr(struct in6pcb *, struct sockaddr_in6 *);

/* in in6_src.c */
int	in6_selecthlim(struct in6pcb *, struct ifnet *);
int	in6_selecthlim_rt(struct in6pcb *);
int	in6_pcbsetport(struct sockaddr_in6 *, struct in6pcb *, struct lwp *);

extern struct rtentry *
	in6_pcbrtentry(struct in6pcb *);
extern struct in6pcb *in6_pcblookup_connect(struct inpcbtable *,
					    const struct in6_addr *, u_int, const struct in6_addr *, u_int, int,
					    struct vestigial_inpcb *);
extern struct in6pcb *in6_pcblookup_bind(struct inpcbtable *,
	const struct in6_addr *, u_int, int);
#endif /* _KERNEL */

#endif /* !_NETINET6_IN6_PCB_H_ */
