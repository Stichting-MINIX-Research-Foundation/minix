/*	$NetBSD: ip_mroute.c,v 1.132 2015/08/24 22:21:26 pooka Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *      @(#)ip_mroute.c 8.2 (Berkeley) 11/15/93
 */

/*
 * Copyright (c) 1989 Stephen Deering
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      @(#)ip_mroute.c 8.2 (Berkeley) 11/15/93
 */

/*
 * IP multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1994
 * Modified by Charles M. Hannum, NetBSD, May 1995.
 * Modified by Ahmed Helmy, SGI, June 1996
 * Modified by George Edmond Eddy (Rusty), ISI, February 1998
 * Modified by Pavlin Radoslavov, USC/ISI, May 1998, August 1999, October 2000
 * Modified by Hitoshi Asaeda, WIDE, August 2000
 * Modified by Pavlin Radoslavov, ICSI, October 2002
 *
 * MROUTING Revision: 1.2
 * and PIM-SMv2 and PIM-DM support, advanced API support,
 * bandwidth metering and signaling
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_mroute.c,v 1.132 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_ipsec.h"
#include "opt_pim.h"
#endif

#ifdef PIM
#define _PIM_VT 1
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_mroute.h>
#ifdef PIM
#include <netinet/pim.h>
#include <netinet/pim_var.h>
#endif
#include <netinet/ip_encap.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/key.h>
#endif

#define IP_MULTICASTOPTS 0
#define	M_PULLUP(m, len)						 \
	do {								 \
		if ((m) && ((m)->m_flags & M_EXT || (m)->m_len < (len))) \
			(m) = m_pullup((m), (len));			 \
	} while (/*CONSTCOND*/ 0)

/*
 * Globals.  All but ip_mrouter and ip_mrtproto could be static,
 * except for netstat or debugging purposes.
 */
struct socket  *ip_mrouter  = NULL;
int		ip_mrtproto = IGMP_DVMRP;    /* for netstat only */

#define NO_RTE_FOUND 	0x1
#define RTE_FOUND	0x2

#define	MFCHASH(a, g)							\
	((((a).s_addr >> 20) ^ ((a).s_addr >> 10) ^ (a).s_addr ^	\
	  ((g).s_addr >> 20) ^ ((g).s_addr >> 10) ^ (g).s_addr) & mfchash)
LIST_HEAD(mfchashhdr, mfc) *mfchashtbl;
u_long	mfchash;

u_char		nexpire[MFCTBLSIZ];
struct vif	viftable[MAXVIFS];
struct mrtstat	mrtstat;
u_int		mrtdebug = 0;	  /* debug level 	*/
#define		DEBUG_MFC	0x02
#define		DEBUG_FORWARD	0x04
#define		DEBUG_EXPIRE	0x08
#define		DEBUG_XMIT	0x10
#define		DEBUG_PIM	0x20

#define		VIFI_INVALID	((vifi_t) -1)

u_int       	tbfdebug = 0;     /* tbf debug level 	*/
#ifdef RSVP_ISI
u_int		rsvpdebug = 0;	  /* rsvp debug level   */
extern struct socket *ip_rsvpd;
extern int rsvp_on;
#endif /* RSVP_ISI */

/* vif attachment using sys/netinet/ip_encap.c */
static void vif_input(struct mbuf *, ...);
static int vif_encapcheck(struct mbuf *, int, int, void *);

static const struct protosw vif_protosw = {
	.pr_type	= SOCK_RAW,
	.pr_domain	= &inetdomain,
	.pr_protocol	= IPPROTO_IPV4,
	.pr_flags	= PR_ATOMIC|PR_ADDR,
	.pr_input	= vif_input,
	.pr_output	= rip_output,
	.pr_ctloutput	= rip_ctloutput,
	.pr_usrreqs	= &rip_usrreqs,
};

#define		EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second */
#define		UPCALL_EXPIRE	6		/* number of timeouts */

/*
 * Define the token bucket filter structures
 */

#define		TBF_REPROCESS	(hz / 100)	/* 100x / second */

static int get_sg_cnt(struct sioc_sg_req *);
static int get_vif_cnt(struct sioc_vif_req *);
static int ip_mrouter_init(struct socket *, int);
static int set_assert(int);
static int add_vif(struct vifctl *);
static int del_vif(vifi_t *);
static void update_mfc_params(struct mfc *, struct mfcctl2 *);
static void init_mfc_params(struct mfc *, struct mfcctl2 *);
static void expire_mfc(struct mfc *);
static int add_mfc(struct sockopt *);
#ifdef UPCALL_TIMING
static void collate(struct timeval *);
#endif
static int del_mfc(struct sockopt *);
static int set_api_config(struct sockopt *); /* chose API capabilities */
static int socket_send(struct socket *, struct mbuf *, struct sockaddr_in *);
static void expire_upcalls(void *);
#ifdef RSVP_ISI
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *, vifi_t);
#else
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *);
#endif
static void phyint_send(struct ip *, struct vif *, struct mbuf *);
static void encap_send(struct ip *, struct vif *, struct mbuf *);
static void tbf_control(struct vif *, struct mbuf *, struct ip *, u_int32_t);
static void tbf_queue(struct vif *, struct mbuf *);
static void tbf_process_q(struct vif *);
static void tbf_reprocess_q(void *);
static int tbf_dq_sel(struct vif *, struct ip *);
static void tbf_send_packet(struct vif *, struct mbuf *);
static void tbf_update_tokens(struct vif *);
static int priority(struct vif *, struct ip *);

/*
 * Bandwidth monitoring
 */
static void free_bw_list(struct bw_meter *);
static int add_bw_upcall(struct bw_upcall *);
static int del_bw_upcall(struct bw_upcall *);
static void bw_meter_receive_packet(struct bw_meter *, int , struct timeval *);
static void bw_meter_prepare_upcall(struct bw_meter *, struct timeval *);
static void bw_upcalls_send(void);
static void schedule_bw_meter(struct bw_meter *, struct timeval *);
static void unschedule_bw_meter(struct bw_meter *);
static void bw_meter_process(void);
static void expire_bw_upcalls_send(void *);
static void expire_bw_meter_process(void *);

#ifdef PIM
static int pim_register_send(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static int pim_register_send_rp(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static int pim_register_send_upcall(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static struct mbuf *pim_register_prepare(struct ip *, struct mbuf *);
#endif

/*
 * 'Interfaces' associated with decapsulator (so we can tell
 * packets that went through it from ones that get reflected
 * by a broken gateway).  These interfaces are never linked into
 * the system ifnet list & no routes point to them.  I.e., packets
 * can't be sent this way.  They only exist as a placeholder for
 * multicast source verification.
 */
#if 0
struct ifnet multicast_decap_if[MAXVIFS];
#endif

#define	ENCAP_TTL	64
#define	ENCAP_PROTO	IPPROTO_IPIP	/* 4 */

/* prototype IP hdr for encapsulated packets */
struct ip multicast_encap_iphdr = {
	.ip_hl = sizeof(struct ip) >> 2,
	.ip_v = IPVERSION,
	.ip_len = sizeof(struct ip),
	.ip_ttl = ENCAP_TTL,
	.ip_p = ENCAP_PROTO,
};

/*
 * Bandwidth meter variables and constants
 */

/*
 * Pending timeouts are stored in a hash table, the key being the
 * expiration time. Periodically, the entries are analysed and processed.
 */
#define BW_METER_BUCKETS	1024
static struct bw_meter *bw_meter_timers[BW_METER_BUCKETS];
struct callout bw_meter_ch;
#define BW_METER_PERIOD (hz)		/* periodical handling of bw meters */

/*
 * Pending upcalls are stored in a vector which is flushed when
 * full, or periodically
 */
static struct bw_upcall	bw_upcalls[BW_UPCALLS_MAX];
static u_int	bw_upcalls_n; /* # of pending upcalls */
struct callout	bw_upcalls_ch;
#define BW_UPCALLS_PERIOD (hz)		/* periodical flush of bw upcalls */

#ifdef PIM
struct pimstat pimstat;

/*
 * Note: the PIM Register encapsulation adds the following in front of a
 * data packet:
 *
 * struct pim_encap_hdr {
 *    struct ip ip;
 *    struct pim_encap_pimhdr  pim;
 * }
 *
 */

struct pim_encap_pimhdr {
	struct pim pim;
	uint32_t   flags;
};

static struct ip pim_encap_iphdr = {
	.ip_v = IPVERSION,
	.ip_hl = sizeof(struct ip) >> 2,
	.ip_len = sizeof(struct ip),
	.ip_ttl = ENCAP_TTL,
	.ip_p = IPPROTO_PIM,
};

static struct pim_encap_pimhdr pim_encap_pimhdr = {
    {
	PIM_MAKE_VT(PIM_VERSION, PIM_REGISTER), /* PIM vers and message type */
	0,			/* reserved */
	0,			/* checksum */
    },
    0				/* flags */
};

static struct ifnet multicast_register_if;
static vifi_t reg_vif_num = VIFI_INVALID;
#endif /* PIM */


/*
 * Private variables.
 */
static vifi_t	   numvifs = 0;

static struct callout expire_upcalls_ch;

/*
 * whether or not special PIM assert processing is enabled.
 */
static int pim_assert;
/*
 * Rate limit for assert notification messages, in usec
 */
#define ASSERT_MSG_TIME		3000000

/*
 * Kernel multicast routing API capabilities and setup.
 * If more API capabilities are added to the kernel, they should be
 * recorded in `mrt_api_support'.
 */
static const u_int32_t mrt_api_support = (MRT_MFC_FLAGS_DISABLE_WRONGVIF |
					  MRT_MFC_FLAGS_BORDER_VIF |
					  MRT_MFC_RP |
					  MRT_MFC_BW_UPCALL);
static u_int32_t mrt_api_config = 0;

/*
 * Find a route for a given origin IP address and Multicast group address
 * Type of service parameter to be added in the future!!!
 * Statistics are updated by the caller if needed
 * (mrtstat.mrts_mfc_lookups and mrtstat.mrts_mfc_misses)
 */
static struct mfc *
mfc_find(struct in_addr *o, struct in_addr *g)
{
	struct mfc *rt;

	LIST_FOREACH(rt, &mfchashtbl[MFCHASH(*o, *g)], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, *o) &&
		    in_hosteq(rt->mfc_mcastgrp, *g) &&
		    (rt->mfc_stall == NULL))
			break;
	}

	return (rt);
}

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) do {					\
	int xxs;							\
	delta = (a).tv_usec - (b).tv_usec;				\
	xxs = (a).tv_sec - (b).tv_sec;					\
	switch (xxs) {							\
	case 2:								\
		delta += 1000000;					\
		/* fall through */					\
	case 1:								\
		delta += 1000000;					\
		/* fall through */					\
	case 0:								\
		break;							\
	default:							\
		delta += (1000000 * xxs);				\
		break;							\
	}								\
} while (/*CONSTCOND*/ 0)

#ifdef UPCALL_TIMING
u_int32_t upcall_data[51];
#endif /* UPCALL_TIMING */

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
int
ip_mrouter_set(struct socket *so, struct sockopt *sopt)
{
	int error;
	int optval;
	struct vifctl vifc;
	vifi_t vifi;
	struct bw_upcall bwuc;

	if (sopt->sopt_name != MRT_INIT && so != ip_mrouter)
		error = ENOPROTOOPT;
	else {
		switch (sopt->sopt_name) {
		case MRT_INIT:
			error = sockopt_getint(sopt, &optval);
			if (error)
				break;

			error = ip_mrouter_init(so, optval);
			break;
		case MRT_DONE:
			error = ip_mrouter_done();
			break;
		case MRT_ADD_VIF:
			error = sockopt_get(sopt, &vifc, sizeof(vifc));
			if (error)
				break;
			error = add_vif(&vifc);
			break;
		case MRT_DEL_VIF:
			error = sockopt_get(sopt, &vifi, sizeof(vifi));
			if (error)
				break;
			error = del_vif(&vifi);
			break;
		case MRT_ADD_MFC:
			error = add_mfc(sopt);
			break;
		case MRT_DEL_MFC:
			error = del_mfc(sopt);
			break;
		case MRT_ASSERT:
			error = sockopt_getint(sopt, &optval);
			if (error)
				break;
			error = set_assert(optval);
			break;
		case MRT_API_CONFIG:
			error = set_api_config(sopt);
			break;
		case MRT_ADD_BW_UPCALL:
			error = sockopt_get(sopt, &bwuc, sizeof(bwuc));
			if (error)
				break;
			error = add_bw_upcall(&bwuc);
			break;
		case MRT_DEL_BW_UPCALL:
			error = sockopt_get(sopt, &bwuc, sizeof(bwuc));
			if (error)
				break;
			error = del_bw_upcall(&bwuc);
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
	}
	return (error);
}

/*
 * Handle MRT getsockopt commands
 */
int
ip_mrouter_get(struct socket *so, struct sockopt *sopt)
{
	int error;

	if (so != ip_mrouter)
		error = ENOPROTOOPT;
	else {
		switch (sopt->sopt_name) {
		case MRT_VERSION:
			error = sockopt_setint(sopt, 0x0305); /* XXX !!!! */
			break;
		case MRT_ASSERT:
			error = sockopt_setint(sopt, pim_assert);
			break;
		case MRT_API_SUPPORT:
			error = sockopt_set(sopt, &mrt_api_support,
			    sizeof(mrt_api_support));
			break;
		case MRT_API_CONFIG:
			error = sockopt_set(sopt, &mrt_api_config,
			    sizeof(mrt_api_config));
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
	}
	return (error);
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
int
mrt_ioctl(struct socket *so, u_long cmd, void *data)
{
	int error;

	if (so != ip_mrouter)
		error = EINVAL;
	else
		switch (cmd) {
		case SIOCGETVIFCNT:
			error = get_vif_cnt((struct sioc_vif_req *)data);
			break;
		case SIOCGETSGCNT:
			error = get_sg_cnt((struct sioc_sg_req *)data);
			break;
		default:
			error = EINVAL;
			break;
		}

	return (error);
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
static int
get_sg_cnt(struct sioc_sg_req *req)
{
	int s;
	struct mfc *rt;

	s = splsoftnet();
	rt = mfc_find(&req->src, &req->grp);
	if (rt == NULL) {
		splx(s);
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
		return (EADDRNOTAVAIL);
	}
	req->pktcnt = rt->mfc_pkt_cnt;
	req->bytecnt = rt->mfc_byte_cnt;
	req->wrong_if = rt->mfc_wrong_if;
	splx(s);

	return (0);
}

/*
 * returns the input and output packet and byte counts on the vif provided
 */
static int
get_vif_cnt(struct sioc_vif_req *req)
{
	vifi_t vifi = req->vifi;

	if (vifi >= numvifs)
		return (EINVAL);

	req->icount = viftable[vifi].v_pkt_in;
	req->ocount = viftable[vifi].v_pkt_out;
	req->ibytes = viftable[vifi].v_bytes_in;
	req->obytes = viftable[vifi].v_bytes_out;

	return (0);
}

/*
 * Enable multicast routing
 */
static int
ip_mrouter_init(struct socket *so, int v)
{
	if (mrtdebug)
		log(LOG_DEBUG,
		    "ip_mrouter_init: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_IGMP)
		return (EOPNOTSUPP);

	if (v != 1)
		return (EINVAL);

	if (ip_mrouter != NULL)
		return (EADDRINUSE);

	ip_mrouter = so;

	mfchashtbl = hashinit(MFCTBLSIZ, HASH_LIST, true, &mfchash);
	memset((void *)nexpire, 0, sizeof(nexpire));

	pim_assert = 0;

	callout_init(&expire_upcalls_ch, 0);
	callout_reset(&expire_upcalls_ch, EXPIRE_TIMEOUT,
		      expire_upcalls, NULL);

	callout_init(&bw_upcalls_ch, 0);
	callout_reset(&bw_upcalls_ch, BW_UPCALLS_PERIOD,
		      expire_bw_upcalls_send, NULL);

	callout_init(&bw_meter_ch, 0);
	callout_reset(&bw_meter_ch, BW_METER_PERIOD,
		      expire_bw_meter_process, NULL);

	if (mrtdebug)
		log(LOG_DEBUG, "ip_mrouter_init\n");

	return (0);
}

/*
 * Disable multicast routing
 */
int
ip_mrouter_done(void)
{
	vifi_t vifi;
	struct vif *vifp;
	int i;
	int s;

	s = splsoftnet();

	/* Clear out all the vifs currently in use. */
	for (vifi = 0; vifi < numvifs; vifi++) {
		vifp = &viftable[vifi];
		if (!in_nullhost(vifp->v_lcl_addr))
			reset_vif(vifp);
	}

	numvifs = 0;
	pim_assert = 0;
	mrt_api_config = 0;

	callout_stop(&expire_upcalls_ch);
	callout_stop(&bw_upcalls_ch);
	callout_stop(&bw_meter_ch);

	/*
	 * Free all multicast forwarding cache entries.
	 */
	for (i = 0; i < MFCTBLSIZ; i++) {
		struct mfc *rt, *nrt;

		for (rt = LIST_FIRST(&mfchashtbl[i]); rt; rt = nrt) {
			nrt = LIST_NEXT(rt, mfc_hash);

			expire_mfc(rt);
		}
	}

	memset((void *)nexpire, 0, sizeof(nexpire));
	hashdone(mfchashtbl, HASH_LIST, mfchash);
	mfchashtbl = NULL;

	bw_upcalls_n = 0;
	memset(bw_meter_timers, 0, sizeof(bw_meter_timers));

	/* Reset de-encapsulation cache. */

	ip_mrouter = NULL;

	splx(s);

	if (mrtdebug)
		log(LOG_DEBUG, "ip_mrouter_done\n");

	return (0);
}

void
ip_mrouter_detach(struct ifnet *ifp)
{
	int vifi, i;
	struct vif *vifp;
	struct mfc *rt;
	struct rtdetq *rte;

	/* XXX not sure about side effect to userland routing daemon */
	for (vifi = 0; vifi < numvifs; vifi++) {
		vifp = &viftable[vifi];
		if (vifp->v_ifp == ifp)
			reset_vif(vifp);
	}
	for (i = 0; i < MFCTBLSIZ; i++) {
		if (nexpire[i] == 0)
			continue;
		LIST_FOREACH(rt, &mfchashtbl[i], mfc_hash) {
			for (rte = rt->mfc_stall; rte; rte = rte->next) {
				if (rte->ifp == ifp)
					rte->ifp = NULL;
			}
		}
	}
}

/*
 * Set PIM assert processing global
 */
static int
set_assert(int i)
{
	pim_assert = !!i;
	return (0);
}

/*
 * Configure API capabilities
 */
static int
set_api_config(struct sockopt *sopt)
{
	u_int32_t apival;
	int i, error;

	/*
	 * We can set the API capabilities only if it is the first operation
	 * after MRT_INIT. I.e.:
	 *  - there are no vifs installed
	 *  - pim_assert is not enabled
	 *  - the MFC table is empty
	 */
	error = sockopt_get(sopt, &apival, sizeof(apival));
	if (error)
		return (error);
	if (numvifs > 0)
		return (EPERM);
	if (pim_assert)
		return (EPERM);
	for (i = 0; i < MFCTBLSIZ; i++) {
		if (LIST_FIRST(&mfchashtbl[i]) != NULL)
			return (EPERM);
	}

	mrt_api_config = apival & mrt_api_support;
	return (0);
}

/*
 * Add a vif to the vif table
 */
static int
add_vif(struct vifctl *vifcp)
{
	struct vif *vifp;
	struct ifaddr *ifa;
	struct ifnet *ifp;
	int error, s;
	struct sockaddr_in sin;

	if (vifcp->vifc_vifi >= MAXVIFS)
		return (EINVAL);
	if (in_nullhost(vifcp->vifc_lcl_addr))
		return (EADDRNOTAVAIL);

	vifp = &viftable[vifcp->vifc_vifi];
	if (!in_nullhost(vifp->v_lcl_addr))
		return (EADDRINUSE);

	/* Find the interface with an address in AF_INET family. */
#ifdef PIM
	if (vifcp->vifc_flags & VIFF_REGISTER) {
		/*
		 * XXX: Because VIFF_REGISTER does not really need a valid
		 * local interface (e.g. it could be 127.0.0.2), we don't
		 * check its address.
		 */
	    ifp = NULL;
	} else
#endif
	{
		sockaddr_in_init(&sin, &vifcp->vifc_lcl_addr, 0);
		ifa = ifa_ifwithaddr(sintosa(&sin));
		if (ifa == NULL)
			return (EADDRNOTAVAIL);
		ifp = ifa->ifa_ifp;
	}

	if (vifcp->vifc_flags & VIFF_TUNNEL) {
		if (vifcp->vifc_flags & VIFF_SRCRT) {
			log(LOG_ERR, "source routed tunnels not supported\n");
			return (EOPNOTSUPP);
		}

		/* attach this vif to decapsulator dispatch table */
		/*
		 * XXX Use addresses in registration so that matching
		 * can be done with radix tree in decapsulator.  But,
		 * we need to check inner header for multicast, so
		 * this requires both radix tree lookup and then a
		 * function to check, and this is not supported yet.
		 */
		vifp->v_encap_cookie = encap_attach_func(AF_INET, IPPROTO_IPV4,
		    vif_encapcheck, &vif_protosw, vifp);
		if (!vifp->v_encap_cookie)
			return (EINVAL);

		/* Create a fake encapsulation interface. */
		ifp = malloc(sizeof(*ifp), M_MRTABLE, M_WAITOK|M_ZERO);
		snprintf(ifp->if_xname, sizeof(ifp->if_xname),
			 "mdecap%d", vifcp->vifc_vifi);

		/* Prepare cached route entry. */
		memset(&vifp->v_route, 0, sizeof(vifp->v_route));
#ifdef PIM
	} else if (vifcp->vifc_flags & VIFF_REGISTER) {
		ifp = &multicast_register_if;
		if (mrtdebug)
			log(LOG_DEBUG, "Adding a register vif, ifp: %p\n",
			    (void *)ifp);
		if (reg_vif_num == VIFI_INVALID) {
			memset(ifp, 0, sizeof(*ifp));
			snprintf(ifp->if_xname, sizeof(ifp->if_xname),
				 "register_vif");
			ifp->if_flags = IFF_LOOPBACK;
			memset(&vifp->v_route, 0, sizeof(vifp->v_route));
			reg_vif_num = vifcp->vifc_vifi;
		}
#endif
	} else {
		/* Make sure the interface supports multicast. */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return (EOPNOTSUPP);

		/* Enable promiscuous reception of all IP multicasts. */
		sockaddr_in_init(&sin, &zeroin_addr, 0);
		error = if_mcast_op(ifp, SIOCADDMULTI, sintosa(&sin));
		if (error)
			return (error);
	}

	s = splsoftnet();

	/* Define parameters for the tbf structure. */
	vifp->tbf_q = NULL;
	vifp->tbf_t = &vifp->tbf_q;
	microtime(&vifp->tbf_last_pkt_t);
	vifp->tbf_n_tok = 0;
	vifp->tbf_q_len = 0;
	vifp->tbf_max_q_len = MAXQSIZE;

	vifp->v_flags = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	/* scaling up here allows division by 1024 in critical code */
	vifp->v_rate_limit = vifcp->vifc_rate_limit * 1024 / 1000;
	vifp->v_lcl_addr = vifcp->vifc_lcl_addr;
	vifp->v_rmt_addr = vifcp->vifc_rmt_addr;
	vifp->v_ifp = ifp;
	/* Initialize per vif pkt counters. */
	vifp->v_pkt_in = 0;
	vifp->v_pkt_out = 0;
	vifp->v_bytes_in = 0;
	vifp->v_bytes_out = 0;

	callout_init(&vifp->v_repq_ch, 0);

#ifdef RSVP_ISI
	vifp->v_rsvp_on = 0;
	vifp->v_rsvpd = NULL;
#endif /* RSVP_ISI */

	splx(s);

	/* Adjust numvifs up if the vifi is higher than numvifs. */
	if (numvifs <= vifcp->vifc_vifi)
		numvifs = vifcp->vifc_vifi + 1;

	if (mrtdebug)
		log(LOG_DEBUG, "add_vif #%d, lcladdr %x, %s %x, thresh %x, rate %d\n",
		    vifcp->vifc_vifi,
		    ntohl(vifcp->vifc_lcl_addr.s_addr),
		    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
		    ntohl(vifcp->vifc_rmt_addr.s_addr),
		    vifcp->vifc_threshold,
		    vifcp->vifc_rate_limit);

	return (0);
}

void
reset_vif(struct vif *vifp)
{
	struct mbuf *m, *n;
	struct ifnet *ifp;
	struct sockaddr_in sin;

	callout_stop(&vifp->v_repq_ch);

	/* detach this vif from decapsulator dispatch table */
	encap_detach(vifp->v_encap_cookie);
	vifp->v_encap_cookie = NULL;

	/*
	 * Free packets queued at the interface
	 */
	for (m = vifp->tbf_q; m != NULL; m = n) {
		n = m->m_nextpkt;
		m_freem(m);
	}

	if (vifp->v_flags & VIFF_TUNNEL)
		free(vifp->v_ifp, M_MRTABLE);
	else if (vifp->v_flags & VIFF_REGISTER) {
#ifdef PIM
		reg_vif_num = VIFI_INVALID;
#endif
	} else {
		sockaddr_in_init(&sin, &zeroin_addr, 0);
		ifp = vifp->v_ifp;
		if_mcast_op(ifp, SIOCDELMULTI, sintosa(&sin));
	}
	memset((void *)vifp, 0, sizeof(*vifp));
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif(vifi_t *vifip)
{
	struct vif *vifp;
	vifi_t vifi;
	int s;

	if (*vifip >= numvifs)
		return (EINVAL);

	vifp = &viftable[*vifip];
	if (in_nullhost(vifp->v_lcl_addr))
		return (EADDRNOTAVAIL);

	s = splsoftnet();

	reset_vif(vifp);

	/* Adjust numvifs down */
	for (vifi = numvifs; vifi > 0; vifi--)
		if (!in_nullhost(viftable[vifi - 1].v_lcl_addr))
			break;
	numvifs = vifi;

	splx(s);

	if (mrtdebug)
		log(LOG_DEBUG, "del_vif %d, numvifs %d\n", *vifip, numvifs);

	return (0);
}

/*
 * update an mfc entry without resetting counters and S,G addresses.
 */
static void
update_mfc_params(struct mfc *rt, struct mfcctl2 *mfccp)
{
	int i;

	rt->mfc_parent = mfccp->mfcc_parent;
	for (i = 0; i < numvifs; i++) {
		rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
		rt->mfc_flags[i] = mfccp->mfcc_flags[i] & mrt_api_config &
			MRT_MFC_FLAGS_ALL;
	}
	/* set the RP address */
	if (mrt_api_config & MRT_MFC_RP)
		rt->mfc_rp = mfccp->mfcc_rp;
	else
		rt->mfc_rp = zeroin_addr;
}

/*
 * fully initialize an mfc entry from the parameter.
 */
static void
init_mfc_params(struct mfc *rt, struct mfcctl2 *mfccp)
{
	rt->mfc_origin     = mfccp->mfcc_origin;
	rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;

	update_mfc_params(rt, mfccp);

	/* initialize pkt counters per src-grp */
	rt->mfc_pkt_cnt    = 0;
	rt->mfc_byte_cnt   = 0;
	rt->mfc_wrong_if   = 0;
	timerclear(&rt->mfc_last_assert);
}

static void
expire_mfc(struct mfc *rt)
{
	struct rtdetq *rte, *nrte;

	free_bw_list(rt->mfc_bw_meter);

	for (rte = rt->mfc_stall; rte != NULL; rte = nrte) {
		nrte = rte->next;
		m_freem(rte->m);
		free(rte, M_MRTABLE);
	}

	LIST_REMOVE(rt, mfc_hash);
	free(rt, M_MRTABLE);
}

/*
 * Add an mfc entry
 */
static int
add_mfc(struct sockopt *sopt)
{
	struct mfcctl2 mfcctl2;
	struct mfcctl2 *mfccp;
	struct mfc *rt;
	u_int32_t hash = 0;
	struct rtdetq *rte, *nrte;
	u_short nstl;
	int s;
	int error;

	/*
	 * select data size depending on API version.
	 */
	mfccp = &mfcctl2;
	memset(&mfcctl2, 0, sizeof(mfcctl2));

	if (mrt_api_config & MRT_API_FLAGS_ALL)
		error = sockopt_get(sopt, mfccp, sizeof(struct mfcctl2));
	else
		error = sockopt_get(sopt, mfccp, sizeof(struct mfcctl));

	if (error)
		return (error);

	s = splsoftnet();
	rt = mfc_find(&mfccp->mfcc_origin, &mfccp->mfcc_mcastgrp);

	/* If an entry already exists, just update the fields */
	if (rt) {
		if (mrtdebug & DEBUG_MFC)
			log(LOG_DEBUG, "add_mfc update o %x g %x p %x\n",
			    ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);

		update_mfc_params(rt, mfccp);

		splx(s);
		return (0);
	}

	/*
	 * Find the entry for which the upcall was made and update
	 */
	nstl = 0;
	hash = MFCHASH(mfccp->mfcc_origin, mfccp->mfcc_mcastgrp);
	LIST_FOREACH(rt, &mfchashtbl[hash], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
		    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp) &&
		    rt->mfc_stall != NULL) {
			if (nstl++)
				log(LOG_ERR, "add_mfc %s o %x g %x p %x dbx %p\n",
				    "multiple kernel entries",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);

			if (mrtdebug & DEBUG_MFC)
				log(LOG_DEBUG, "add_mfc o %x g %x p %x dbg %p\n",
				    ntohl(mfccp->mfcc_origin.s_addr),
				    ntohl(mfccp->mfcc_mcastgrp.s_addr),
				    mfccp->mfcc_parent, rt->mfc_stall);

			rte = rt->mfc_stall;
			init_mfc_params(rt, mfccp);
			rt->mfc_stall = NULL;

			rt->mfc_expire = 0; /* Don't clean this guy up */
			nexpire[hash]--;

			/* free packets Qed at the end of this entry */
			for (; rte != NULL; rte = nrte) {
				nrte = rte->next;
				if (rte->ifp) {
#ifdef RSVP_ISI
					ip_mdq(rte->m, rte->ifp, rt, -1);
#else
					ip_mdq(rte->m, rte->ifp, rt);
#endif /* RSVP_ISI */
				}
				m_freem(rte->m);
#ifdef UPCALL_TIMING
				collate(&rte->t);
#endif /* UPCALL_TIMING */
				free(rte, M_MRTABLE);
			}
		}
	}

	/*
	 * It is possible that an entry is being inserted without an upcall
	 */
	if (nstl == 0) {
		/*
		 * No mfc; make a new one
		 */
		if (mrtdebug & DEBUG_MFC)
			log(LOG_DEBUG, "add_mfc no upcall o %x g %x p %x\n",
			    ntohl(mfccp->mfcc_origin.s_addr),
			    ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent);

		LIST_FOREACH(rt, &mfchashtbl[hash], mfc_hash) {
			if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
			    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp)) {
				init_mfc_params(rt, mfccp);
				if (rt->mfc_expire)
					nexpire[hash]--;
				rt->mfc_expire = 0;
				break; /* XXX */
			}
		}
		if (rt == NULL) {	/* no upcall, so make a new entry */
			rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE,
						  M_NOWAIT);
			if (rt == NULL) {
				splx(s);
				return (ENOBUFS);
			}

			init_mfc_params(rt, mfccp);
			rt->mfc_expire	= 0;
			rt->mfc_stall	= NULL;
			rt->mfc_bw_meter = NULL;

			/* insert new entry at head of hash chain */
			LIST_INSERT_HEAD(&mfchashtbl[hash], rt, mfc_hash);
		}
	}

	splx(s);
	return (0);
}

#ifdef UPCALL_TIMING
/*
 * collect delay statistics on the upcalls
 */
static void
collate(struct timeval *t)
{
	u_int32_t d;
	struct timeval tp;
	u_int32_t delta;

	microtime(&tp);

	if (timercmp(t, &tp, <)) {
		TV_DELTA(tp, *t, delta);

		d = delta >> 10;
		if (d > 50)
			d = 50;

		++upcall_data[d];
	}
}
#endif /* UPCALL_TIMING */

/*
 * Delete an mfc entry
 */
static int
del_mfc(struct sockopt *sopt)
{
	struct mfcctl2 mfcctl2;
	struct mfcctl2 *mfccp;
	struct mfc *rt;
	int s;
	int error;

	/*
	 * XXX: for deleting MFC entries the information in entries
	 * of size "struct mfcctl" is sufficient.
	 */

	mfccp = &mfcctl2;
	memset(&mfcctl2, 0, sizeof(mfcctl2));

	error = sockopt_get(sopt, mfccp, sizeof(struct mfcctl));
	if (error) {
		/* Try with the size of mfcctl2. */
		error = sockopt_get(sopt, mfccp, sizeof(struct mfcctl2));
		if (error)
			return (error);
	}

	if (mrtdebug & DEBUG_MFC)
		log(LOG_DEBUG, "del_mfc origin %x mcastgrp %x\n",
		    ntohl(mfccp->mfcc_origin.s_addr),
		    ntohl(mfccp->mfcc_mcastgrp.s_addr));

	s = splsoftnet();

	rt = mfc_find(&mfccp->mfcc_origin, &mfccp->mfcc_mcastgrp);
	if (rt == NULL) {
		splx(s);
		return (EADDRNOTAVAIL);
	}

	/*
	 * free the bw_meter entries
	 */
	free_bw_list(rt->mfc_bw_meter);
	rt->mfc_bw_meter = NULL;

	LIST_REMOVE(rt, mfc_hash);
	free(rt, M_MRTABLE);

	splx(s);
	return (0);
}

static int
socket_send(struct socket *s, struct mbuf *mm, struct sockaddr_in *src)
{
	if (s) {
		if (sbappendaddr(&s->so_rcv, sintosa(src), mm, NULL) != 0) {
			sorwakeup(s);
			return (0);
		}
	}
	m_freem(mm);
	return (-1);
}

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

#define IP_HDR_LEN  20	/* # bytes of fixed IP header (excluding options) */
#define TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

int
#ifdef RSVP_ISI
ip_mforward(struct mbuf *m, struct ifnet *ifp, struct ip_moptions *imo)
#else
ip_mforward(struct mbuf *m, struct ifnet *ifp)
#endif /* RSVP_ISI */
{
	struct ip *ip = mtod(m, struct ip *);
	struct mfc *rt;
	static int srctun = 0;
	struct mbuf *mm;
	struct sockaddr_in sin;
	int s;
	vifi_t vifi;

	if (mrtdebug & DEBUG_FORWARD)
		log(LOG_DEBUG, "ip_mforward: src %x, dst %x, ifp %p\n",
		    ntohl(ip->ip_src.s_addr), ntohl(ip->ip_dst.s_addr), ifp);

	if (ip->ip_hl < (IP_HDR_LEN + TUNNEL_LEN) >> 2 ||
	    ((u_char *)(ip + 1))[1] != IPOPT_LSRR) {
		/*
		 * Packet arrived via a physical interface or
		 * an encapsulated tunnel or a register_vif.
		 */
	} else {
		/*
		 * Packet arrived through a source-route tunnel.
		 * Source-route tunnels are no longer supported.
		 */
		if ((srctun++ % 1000) == 0)
			log(LOG_ERR,
			    "ip_mforward: received source-routed packet from %x\n",
			    ntohl(ip->ip_src.s_addr));

		return (1);
	}

	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m->m_pkthdr.csum_flags = 0;

#ifdef RSVP_ISI
	if (imo && ((vifi = imo->imo_multicast_vif) < numvifs)) {
		if (ip->ip_ttl < MAXTTL)
			ip->ip_ttl++;	/* compensate for -1 in *_send routines */
		if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
			struct vif *vifp = viftable + vifi;
			printf("Sending IPPROTO_RSVP from %x to %x on vif %d (%s%s)\n",
			    ntohl(ip->ip_src), ntohl(ip->ip_dst), vifi,
			    (vifp->v_flags & VIFF_TUNNEL) ? "tunnel on " : "",
			    vifp->v_ifp->if_xname);
		}
		return (ip_mdq(m, ifp, NULL, vifi));
	}
	if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
		printf("Warning: IPPROTO_RSVP from %x to %x without vif option\n",
		    ntohl(ip->ip_src), ntohl(ip->ip_dst));
	}
#endif /* RSVP_ISI */

	/*
	 * Don't forward a packet with time-to-live of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip->ip_ttl <= 1 || IN_LOCAL_GROUP(ip->ip_dst.s_addr))
		return (0);

	/*
	 * Determine forwarding vifs from the forwarding cache table
	 */
	s = splsoftnet();
	++mrtstat.mrts_mfc_lookups;
	rt = mfc_find(&ip->ip_src, &ip->ip_dst);

	/* Entry exists, so forward if necessary */
	if (rt != NULL) {
		splx(s);
#ifdef RSVP_ISI
		return (ip_mdq(m, ifp, rt, -1));
#else
		return (ip_mdq(m, ifp, rt));
#endif /* RSVP_ISI */
	} else {
		/*
		 * If we don't have a route for packet's origin,
		 * Make a copy of the packet & send message to routing daemon
		 */

		struct mbuf *mb0;
		struct rtdetq *rte;
		u_int32_t hash;
		int hlen = ip->ip_hl << 2;
#ifdef UPCALL_TIMING
		struct timeval tp;

		microtime(&tp);
#endif /* UPCALL_TIMING */

		++mrtstat.mrts_mfc_misses;

		mrtstat.mrts_no_route++;
		if (mrtdebug & (DEBUG_FORWARD | DEBUG_MFC))
			log(LOG_DEBUG, "ip_mforward: no rte s %x g %x\n",
			    ntohl(ip->ip_src.s_addr),
			    ntohl(ip->ip_dst.s_addr));

		/*
		 * Allocate mbufs early so that we don't do extra work if we are
		 * just going to fail anyway.  Make sure to pullup the header so
		 * that other people can't step on it.
		 */
		rte = (struct rtdetq *)malloc(sizeof(*rte), M_MRTABLE,
					      M_NOWAIT);
		if (rte == NULL) {
			splx(s);
			return (ENOBUFS);
		}
		mb0 = m_copypacket(m, M_DONTWAIT);
		M_PULLUP(mb0, hlen);
		if (mb0 == NULL) {
			free(rte, M_MRTABLE);
			splx(s);
			return (ENOBUFS);
		}

		/* is there an upcall waiting for this flow? */
		hash = MFCHASH(ip->ip_src, ip->ip_dst);
		LIST_FOREACH(rt, &mfchashtbl[hash], mfc_hash) {
			if (in_hosteq(ip->ip_src, rt->mfc_origin) &&
			    in_hosteq(ip->ip_dst, rt->mfc_mcastgrp) &&
			    rt->mfc_stall != NULL)
				break;
		}

		if (rt == NULL) {
			int i;
			struct igmpmsg *im;

			/*
			 * Locate the vifi for the incoming interface for
			 * this packet.
			 * If none found, drop packet.
			 */
			for (vifi = 0; vifi < numvifs &&
				 viftable[vifi].v_ifp != ifp; vifi++)
				;
			if (vifi >= numvifs) /* vif not found, drop packet */
				goto non_fatal;

			/* no upcall, so make a new entry */
			rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE,
						  M_NOWAIT);
			if (rt == NULL)
				goto fail;

			/*
			 * Make a copy of the header to send to the user level
			 * process
			 */
			mm = m_copym(m, 0, hlen, M_DONTWAIT);
			M_PULLUP(mm, hlen);
			if (mm == NULL)
				goto fail1;

			/*
			 * Send message to routing daemon to install
			 * a route into the kernel table
			 */

			im = mtod(mm, struct igmpmsg *);
			im->im_msgtype = IGMPMSG_NOCACHE;
			im->im_mbz = 0;
			im->im_vif = vifi;

			mrtstat.mrts_upcalls++;

			sockaddr_in_init(&sin, &ip->ip_src, 0);
			if (socket_send(ip_mrouter, mm, &sin) < 0) {
				log(LOG_WARNING,
				    "ip_mforward: ip_mrouter socket queue full\n");
				++mrtstat.mrts_upq_sockfull;
			fail1:
				free(rt, M_MRTABLE);
			fail:
				free(rte, M_MRTABLE);
				m_freem(mb0);
				splx(s);
				return (ENOBUFS);
			}

			/* insert new entry at head of hash chain */
			rt->mfc_origin = ip->ip_src;
			rt->mfc_mcastgrp = ip->ip_dst;
			rt->mfc_pkt_cnt = 0;
			rt->mfc_byte_cnt = 0;
			rt->mfc_wrong_if = 0;
			rt->mfc_expire = UPCALL_EXPIRE;
			nexpire[hash]++;
			for (i = 0; i < numvifs; i++) {
				rt->mfc_ttls[i] = 0;
				rt->mfc_flags[i] = 0;
			}
			rt->mfc_parent = -1;

			/* clear the RP address */
			rt->mfc_rp = zeroin_addr;

			rt->mfc_bw_meter = NULL;

			/* link into table */
			LIST_INSERT_HEAD(&mfchashtbl[hash], rt, mfc_hash);
			/* Add this entry to the end of the queue */
			rt->mfc_stall = rte;
		} else {
			/* determine if q has overflowed */
			struct rtdetq **p;
			int npkts = 0;

			/*
			 * XXX ouch! we need to append to the list, but we
			 * only have a pointer to the front, so we have to
			 * scan the entire list every time.
			 */
			for (p = &rt->mfc_stall; *p != NULL; p = &(*p)->next)
				if (++npkts > MAX_UPQ) {
					mrtstat.mrts_upq_ovflw++;
				non_fatal:
					free(rte, M_MRTABLE);
					m_freem(mb0);
					splx(s);
					return (0);
				}

			/* Add this entry to the end of the queue */
			*p = rte;
		}

		rte->next = NULL;
		rte->m = mb0;
		rte->ifp = ifp;
#ifdef UPCALL_TIMING
		rte->t = tp;
#endif /* UPCALL_TIMING */

		splx(s);

		return (0);
	}
}


/*ARGSUSED*/
static void
expire_upcalls(void *v)
{
	int i;
	int s;

	s = splsoftnet();

	for (i = 0; i < MFCTBLSIZ; i++) {
		struct mfc *rt, *nrt;

		if (nexpire[i] == 0)
			continue;

		for (rt = LIST_FIRST(&mfchashtbl[i]); rt; rt = nrt) {
			nrt = LIST_NEXT(rt, mfc_hash);

			if (rt->mfc_expire == 0 || --rt->mfc_expire > 0)
				continue;
			nexpire[i]--;

			/*
			 * free the bw_meter entries
			 */
			while (rt->mfc_bw_meter != NULL) {
				struct bw_meter *x = rt->mfc_bw_meter;

				rt->mfc_bw_meter = x->bm_mfc_next;
				kmem_free(x, sizeof(*x));
			}

			++mrtstat.mrts_cache_cleanups;
			if (mrtdebug & DEBUG_EXPIRE)
				log(LOG_DEBUG,
				    "expire_upcalls: expiring (%x %x)\n",
				    ntohl(rt->mfc_origin.s_addr),
				    ntohl(rt->mfc_mcastgrp.s_addr));

			expire_mfc(rt);
		}
	}

	splx(s);
	callout_reset(&expire_upcalls_ch, EXPIRE_TIMEOUT,
	    expire_upcalls, NULL);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
#ifdef RSVP_ISI
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt, vifi_t xmt_vif)
#else
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt)
#endif /* RSVP_ISI */
{
	struct ip  *ip = mtod(m, struct ip *);
	vifi_t vifi;
	struct vif *vifp;
	struct sockaddr_in sin;
	int plen = ntohs(ip->ip_len) - (ip->ip_hl << 2);

/*
 * Macro to send packet on vif.  Since RSVP packets don't get counted on
 * input, they shouldn't get counted on output, so statistics keeping is
 * separate.
 */
#define MC_SEND(ip, vifp, m) do {					\
	if ((vifp)->v_flags & VIFF_TUNNEL)				\
		encap_send((ip), (vifp), (m));				\
	else								\
		phyint_send((ip), (vifp), (m));				\
} while (/*CONSTCOND*/ 0)

#ifdef RSVP_ISI
	/*
	 * If xmt_vif is not -1, send on only the requested vif.
	 *
	 * (since vifi_t is u_short, -1 becomes MAXUSHORT, which > numvifs.
	 */
	if (xmt_vif < numvifs) {
#ifdef PIM
		if (viftable[xmt_vif].v_flags & VIFF_REGISTER)
			pim_register_send(ip, viftable + xmt_vif, m, rt);
		else
#endif
		MC_SEND(ip, viftable + xmt_vif, m);
		return (1);
	}
#endif /* RSVP_ISI */

	/*
	 * Don't forward if it didn't arrive from the parent vif for its origin.
	 */
	vifi = rt->mfc_parent;
	if ((vifi >= numvifs) || (viftable[vifi].v_ifp != ifp)) {
		/* came in the wrong interface */
		if (mrtdebug & DEBUG_FORWARD)
			log(LOG_DEBUG, "wrong if: ifp %p vifi %d vififp %p\n",
			    ifp, vifi,
			    vifi >= numvifs ? 0 : viftable[vifi].v_ifp);
		++mrtstat.mrts_wrong_if;
		++rt->mfc_wrong_if;
		/*
		 * If we are doing PIM assert processing, send a message
		 * to the routing daemon.
		 *
		 * XXX: A PIM-SM router needs the WRONGVIF detection so it
		 * can complete the SPT switch, regardless of the type
		 * of the iif (broadcast media, GRE tunnel, etc).
		 */
		if (pim_assert && (vifi < numvifs) && viftable[vifi].v_ifp) {
			struct timeval now;
			u_int32_t delta;

#ifdef PIM
			if (ifp == &multicast_register_if)
				pimstat.pims_rcv_registers_wrongiif++;
#endif

			/* Get vifi for the incoming packet */
			for (vifi = 0;
			     vifi < numvifs && viftable[vifi].v_ifp != ifp;
			     vifi++)
			    ;
			if (vifi >= numvifs) {
				/* The iif is not found: ignore the packet. */
				return (0);
			}

			if (rt->mfc_flags[vifi] &
			    MRT_MFC_FLAGS_DISABLE_WRONGVIF) {
				/* WRONGVIF disabled: ignore the packet */
				return (0);
			}

			microtime(&now);

			TV_DELTA(rt->mfc_last_assert, now, delta);

			if (delta > ASSERT_MSG_TIME) {
				struct igmpmsg *im;
				int hlen = ip->ip_hl << 2;
				struct mbuf *mm =
				    m_copym(m, 0, hlen, M_DONTWAIT);

				M_PULLUP(mm, hlen);
				if (mm == NULL)
					return (ENOBUFS);

				rt->mfc_last_assert = now;

				im = mtod(mm, struct igmpmsg *);
				im->im_msgtype	= IGMPMSG_WRONGVIF;
				im->im_mbz	= 0;
				im->im_vif	= vifi;

				mrtstat.mrts_upcalls++;

				sockaddr_in_init(&sin, &im->im_src, 0);
				if (socket_send(ip_mrouter, mm, &sin) < 0) {
					log(LOG_WARNING,
					    "ip_mforward: ip_mrouter socket queue full\n");
					++mrtstat.mrts_upq_sockfull;
					return (ENOBUFS);
				}
			}
		}
		return (0);
	}

	/* If I sourced this packet, it counts as output, else it was input. */
	if (in_hosteq(ip->ip_src, viftable[vifi].v_lcl_addr)) {
		viftable[vifi].v_pkt_out++;
		viftable[vifi].v_bytes_out += plen;
	} else {
		viftable[vifi].v_pkt_in++;
		viftable[vifi].v_bytes_in += plen;
	}
	rt->mfc_pkt_cnt++;
	rt->mfc_byte_cnt += plen;

	/*
	 * For each vif, decide if a copy of the packet should be forwarded.
	 * Forward if:
	 *		- the ttl exceeds the vif's threshold
	 *		- there are group members downstream on interface
	 */
	for (vifp = viftable, vifi = 0; vifi < numvifs; vifp++, vifi++)
		if ((rt->mfc_ttls[vifi] > 0) &&
			(ip->ip_ttl > rt->mfc_ttls[vifi])) {
			vifp->v_pkt_out++;
			vifp->v_bytes_out += plen;
#ifdef PIM
			if (vifp->v_flags & VIFF_REGISTER)
				pim_register_send(ip, vifp, m, rt);
			else
#endif
			MC_SEND(ip, vifp, m);
		}

	/*
	 * Perform upcall-related bw measuring.
	 */
	if (rt->mfc_bw_meter != NULL) {
		struct bw_meter *x;
		struct timeval now;

		microtime(&now);
		for (x = rt->mfc_bw_meter; x != NULL; x = x->bm_mfc_next)
			bw_meter_receive_packet(x, plen, &now);
	}

	return (0);
}

#ifdef RSVP_ISI
/*
 * check if a vif number is legal/ok. This is used by ip_output.
 */
int
legal_vif_num(int vif)
{
	if (vif >= 0 && vif < numvifs)
		return (1);
	else
		return (0);
}
#endif /* RSVP_ISI */

static void
phyint_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
	struct mbuf *mb_copy;
	int hlen = ip->ip_hl << 2;

	/*
	 * Make a new reference to the packet; make sure that
	 * the IP header is actually copied, not just referenced,
	 * so that ip_output() only scribbles on the copy.
	 */
	mb_copy = m_copypacket(m, M_DONTWAIT);
	M_PULLUP(mb_copy, hlen);
	if (mb_copy == NULL)
		return;

	if (vifp->v_rate_limit <= 0)
		tbf_send_packet(vifp, mb_copy);
	else
		tbf_control(vifp, mb_copy, mtod(mb_copy, struct ip *),
		    ntohs(ip->ip_len));
}

static void
encap_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
	struct mbuf *mb_copy;
	struct ip *ip_copy;
	int i, len = ntohs(ip->ip_len) + sizeof(multicast_encap_iphdr);

	/* Take care of delayed checksums */
	if (m->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~(M_CSUM_TCPv4|M_CSUM_UDPv4);
	}

	/*
	 * copy the old packet & pullup its IP header into the
	 * new mbuf so we can modify it.  Try to fill the new
	 * mbuf since if we don't the ethernet driver will.
	 */
	MGETHDR(mb_copy, M_DONTWAIT, MT_DATA);
	if (mb_copy == NULL)
		return;
	mb_copy->m_data += max_linkhdr;
	mb_copy->m_pkthdr.len = len;
	mb_copy->m_len = sizeof(multicast_encap_iphdr);

	if ((mb_copy->m_next = m_copypacket(m, M_DONTWAIT)) == NULL) {
		m_freem(mb_copy);
		return;
	}
	i = MHLEN - max_linkhdr;
	if (i > len)
		i = len;
	mb_copy = m_pullup(mb_copy, i);
	if (mb_copy == NULL)
		return;

	/*
	 * fill in the encapsulating IP header.
	 */
	ip_copy = mtod(mb_copy, struct ip *);
	*ip_copy = multicast_encap_iphdr;
	if (len < IP_MINFRAGSIZE)
		ip_copy->ip_id = 0;
	else
		ip_copy->ip_id = ip_newid(NULL);
	ip_copy->ip_len = htons(len);
	ip_copy->ip_src = vifp->v_lcl_addr;
	ip_copy->ip_dst = vifp->v_rmt_addr;

	/*
	 * turn the encapsulated IP header back into a valid one.
	 */
	ip = (struct ip *)((char *)ip_copy + sizeof(multicast_encap_iphdr));
	--ip->ip_ttl;
	ip->ip_sum = 0;
	mb_copy->m_data += sizeof(multicast_encap_iphdr);
	ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
	mb_copy->m_data -= sizeof(multicast_encap_iphdr);

	if (vifp->v_rate_limit <= 0)
		tbf_send_packet(vifp, mb_copy);
	else
		tbf_control(vifp, mb_copy, ip, ntohs(ip_copy->ip_len));
}

/*
 * De-encapsulate a packet and feed it back through ip input.
 */
static void
vif_input(struct mbuf *m, ...)
{
	int off, proto;
	va_list ap;
	struct vif *vifp;

	va_start(ap, m);
	off = va_arg(ap, int);
	proto = va_arg(ap, int);
	va_end(ap);

	vifp = (struct vif *)encap_getarg(m);
	if (!vifp || proto != ENCAP_PROTO) {
		m_freem(m);
		mrtstat.mrts_bad_tunnel++;
		return;
	}

	m_adj(m, off);
	m->m_pkthdr.rcvif = vifp->v_ifp;

	if (__predict_false(!pktq_enqueue(ip_pktq, m, 0))) {
		m_freem(m);
	}
}

/*
 * Check if the packet should be received on the vif denoted by arg.
 * (The encap selection code will call this once per vif since each is
 * registered separately.)
 */
static int
vif_encapcheck(struct mbuf *m, int off, int proto, void *arg)
{
	struct vif *vifp;
	struct ip ip;

#ifdef DIAGNOSTIC
	if (!arg || proto != IPPROTO_IPV4)
		panic("unexpected arg in vif_encapcheck");
#endif

	/*
	 * Accept the packet only if the inner heaader is multicast
	 * and the outer header matches a tunnel-mode vif.  Order
	 * checks in the hope that common non-matching packets will be
	 * rejected quickly.  Assume that unicast IPv4 traffic in a
	 * parallel tunnel (e.g. gif(4)) is unlikely.
	 */

	/* Obtain the outer IP header and the vif pointer. */
	m_copydata((struct mbuf *)m, 0, sizeof(ip), (void *)&ip);
	vifp = (struct vif *)arg;

	/*
	 * The outer source must match the vif's remote peer address.
	 * For a multicast router with several tunnels, this is the
	 * only check that will fail on packets in other tunnels,
	 * assuming the local address is the same.	   
	 */
	if (!in_hosteq(vifp->v_rmt_addr, ip.ip_src))
		return 0;

	/* The outer destination must match the vif's local address. */
	if (!in_hosteq(vifp->v_lcl_addr, ip.ip_dst))
		return 0;

	/* The vif must be of tunnel type. */
	if ((vifp->v_flags & VIFF_TUNNEL) == 0)
		return 0;

	/* Check that the inner destination is multicast. */
	m_copydata((struct mbuf *)m, off, sizeof(ip), (void *)&ip);
	if (!IN_MULTICAST(ip.ip_dst.s_addr))
		return 0;

	/*
	 * We have checked that both the outer src and dst addresses
	 * match the vif, and that the inner destination is multicast
	 * (224/5).  By claiming more than 64, we intend to
	 * preferentially take packets that also match a parallel
	 * gif(4).
	 */
	return 32 + 32 + 5;
}

/*
 * Token bucket filter module
 */
static void
tbf_control(struct vif *vifp, struct mbuf *m, struct ip *ip, u_int32_t len)
{

	if (len > MAX_BKT_SIZE) {
		/* drop if packet is too large */
		mrtstat.mrts_pkt2large++;
		m_freem(m);
		return;
	}

	tbf_update_tokens(vifp);

	/*
	 * If there are enough tokens, and the queue is empty, send this packet
	 * out immediately.  Otherwise, try to insert it on this vif's queue.
	 */
	if (vifp->tbf_q_len == 0) {
		if (len <= vifp->tbf_n_tok) {
			vifp->tbf_n_tok -= len;
			tbf_send_packet(vifp, m);
		} else {
			/* queue packet and timeout till later */
			tbf_queue(vifp, m);
			callout_reset(&vifp->v_repq_ch, TBF_REPROCESS,
			    tbf_reprocess_q, vifp);
		}
	} else {
		if (vifp->tbf_q_len >= vifp->tbf_max_q_len &&
		    !tbf_dq_sel(vifp, ip)) {
			/* queue full, and couldn't make room */
			mrtstat.mrts_q_overflow++;
			m_freem(m);
		} else {
			/* queue length low enough, or made room */
			tbf_queue(vifp, m);
			tbf_process_q(vifp);
		}
	}
}

/*
 * adds a packet to the queue at the interface
 */
static void
tbf_queue(struct vif *vifp, struct mbuf *m)
{
	int s = splsoftnet();

	/* insert at tail */
	*vifp->tbf_t = m;
	vifp->tbf_t = &m->m_nextpkt;
	vifp->tbf_q_len++;

	splx(s);
}


/*
 * processes the queue at the interface
 */
static void
tbf_process_q(struct vif *vifp)
{
	struct mbuf *m;
	int len;
	int s = splsoftnet();

	/*
	 * Loop through the queue at the interface and send as many packets
	 * as possible.
	 */
	for (m = vifp->tbf_q; m != NULL; m = vifp->tbf_q) {
		len = ntohs(mtod(m, struct ip *)->ip_len);

		/* determine if the packet can be sent */
		if (len <= vifp->tbf_n_tok) {
			/* if so,
			 * reduce no of tokens, dequeue the packet,
			 * send the packet.
			 */
			if ((vifp->tbf_q = m->m_nextpkt) == NULL)
				vifp->tbf_t = &vifp->tbf_q;
			--vifp->tbf_q_len;

			m->m_nextpkt = NULL;
			vifp->tbf_n_tok -= len;
			tbf_send_packet(vifp, m);
		} else
			break;
	}
	splx(s);
}

static void
tbf_reprocess_q(void *arg)
{
	struct vif *vifp = arg;

	if (ip_mrouter == NULL)
		return;

	tbf_update_tokens(vifp);
	tbf_process_q(vifp);

	if (vifp->tbf_q_len != 0)
		callout_reset(&vifp->v_repq_ch, TBF_REPROCESS,
		    tbf_reprocess_q, vifp);
}

/* function that will selectively discard a member of the queue
 * based on the precedence value and the priority
 */
static int
tbf_dq_sel(struct vif *vifp, struct ip *ip)
{
	u_int p;
	struct mbuf **mp, *m;
	int s = splsoftnet();

	p = priority(vifp, ip);

	for (mp = &vifp->tbf_q, m = *mp;
	    m != NULL;
	    mp = &m->m_nextpkt, m = *mp) {
		if (p > priority(vifp, mtod(m, struct ip *))) {
			if ((*mp = m->m_nextpkt) == NULL)
				vifp->tbf_t = mp;
			--vifp->tbf_q_len;

			m_freem(m);
			mrtstat.mrts_drop_sel++;
			splx(s);
			return (1);
		}
	}
	splx(s);
	return (0);
}

static void
tbf_send_packet(struct vif *vifp, struct mbuf *m)
{
	int error;
	int s = splsoftnet();

	if (vifp->v_flags & VIFF_TUNNEL) {
		/* If tunnel options */
		ip_output(m, NULL, &vifp->v_route, IP_FORWARDING, NULL, NULL);
	} else {
		/* if physical interface option, extract the options and then send */
		struct ip_moptions imo;

		imo.imo_multicast_ifp = vifp->v_ifp;
		imo.imo_multicast_ttl = mtod(m, struct ip *)->ip_ttl - 1;
		imo.imo_multicast_loop = 1;
#ifdef RSVP_ISI
		imo.imo_multicast_vif = -1;
#endif

		error = ip_output(m, NULL, NULL, IP_FORWARDING|IP_MULTICASTOPTS,
		    &imo, NULL);

		if (mrtdebug & DEBUG_XMIT)
			log(LOG_DEBUG, "phyint_send on vif %ld err %d\n",
			    (long)(vifp - viftable), error);
	}
	splx(s);
}

/* determine the current time and then
 * the elapsed time (between the last time and time now)
 * in milliseconds & update the no. of tokens in the bucket
 */
static void
tbf_update_tokens(struct vif *vifp)
{
	struct timeval tp;
	u_int32_t tm;
	int s = splsoftnet();

	microtime(&tp);

	TV_DELTA(tp, vifp->tbf_last_pkt_t, tm);

	/*
	 * This formula is actually
	 * "time in seconds" * "bytes/second".
	 *
	 * (tm / 1000000) * (v_rate_limit * 1000 * (1000/1024) / 8)
	 *
	 * The (1000/1024) was introduced in add_vif to optimize
	 * this divide into a shift.
	 */
	vifp->tbf_n_tok += tm * vifp->v_rate_limit / 8192;
	vifp->tbf_last_pkt_t = tp;

	if (vifp->tbf_n_tok > MAX_BKT_SIZE)
		vifp->tbf_n_tok = MAX_BKT_SIZE;

	splx(s);
}

static int
priority(struct vif *vifp, struct ip *ip)
{
	int prio = 50;	/* the lowest priority -- default case */

	/* temporary hack; may add general packet classifier some day */

	/*
	 * The UDP port space is divided up into four priority ranges:
	 * [0, 16384)     : unclassified - lowest priority
	 * [16384, 32768) : audio - highest priority
	 * [32768, 49152) : whiteboard - medium priority
	 * [49152, 65536) : video - low priority
	 */
	if (ip->ip_p == IPPROTO_UDP) {
		struct udphdr *udp = (struct udphdr *)(((char *)ip) + (ip->ip_hl << 2));

		switch (ntohs(udp->uh_dport) & 0xc000) {
		case 0x4000:
			prio = 70;
			break;
		case 0x8000:
			prio = 60;
			break;
		case 0xc000:
			prio = 55;
			break;
		}

		if (tbfdebug > 1)
			log(LOG_DEBUG, "port %x prio %d\n",
			    ntohs(udp->uh_dport), prio);
	}

	return (prio);
}

/*
 * End of token bucket filter modifications
 */
#ifdef RSVP_ISI
int
ip_rsvp_vif_init(struct socket *so, struct mbuf *m)
{
	int vifi, s;

	if (rsvpdebug)
		printf("ip_rsvp_vif_init: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
		return (EOPNOTSUPP);

	/* Check mbuf. */
	if (m == NULL || m->m_len != sizeof(int)) {
		return (EINVAL);
	}
	vifi = *(mtod(m, int *));

	if (rsvpdebug)
		printf("ip_rsvp_vif_init: vif = %d rsvp_on = %d\n",
		       vifi, rsvp_on);

	s = splsoftnet();

	/* Check vif. */
	if (!legal_vif_num(vifi)) {
		splx(s);
		return (EADDRNOTAVAIL);
	}

	/* Check if socket is available. */
	if (viftable[vifi].v_rsvpd != NULL) {
		splx(s);
		return (EADDRINUSE);
	}

	viftable[vifi].v_rsvpd = so;
	/*
	 * This may seem silly, but we need to be sure we don't over-increment
	 * the RSVP counter, in case something slips up.
	 */
	if (!viftable[vifi].v_rsvp_on) {
		viftable[vifi].v_rsvp_on = 1;
		rsvp_on++;
	}

	splx(s);
	return (0);
}

int
ip_rsvp_vif_done(struct socket *so, struct mbuf *m)
{
	int vifi, s;

	if (rsvpdebug)
		printf("ip_rsvp_vif_done: so_type = %d, pr_protocol = %d\n",
		    so->so_type, so->so_proto->pr_protocol);

	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
		return (EOPNOTSUPP);

	/* Check mbuf. */
	if (m == NULL || m->m_len != sizeof(int)) {
		return (EINVAL);
	}
	vifi = *(mtod(m, int *));

	s = splsoftnet();

	/* Check vif. */
	if (!legal_vif_num(vifi)) {
		splx(s);
		return (EADDRNOTAVAIL);
	}

	if (rsvpdebug)
		printf("ip_rsvp_vif_done: v_rsvpd = %x so = %x\n",
		    viftable[vifi].v_rsvpd, so);

	viftable[vifi].v_rsvpd = NULL;
	/*
	 * This may seem silly, but we need to be sure we don't over-decrement
	 * the RSVP counter, in case something slips up.
	 */
	if (viftable[vifi].v_rsvp_on) {
		viftable[vifi].v_rsvp_on = 0;
		rsvp_on--;
	}

	splx(s);
	return (0);
}

void
ip_rsvp_force_done(struct socket *so)
{
	int vifi, s;

	/* Don't bother if it is not the right type of socket. */
	if (so->so_type != SOCK_RAW ||
	    so->so_proto->pr_protocol != IPPROTO_RSVP)
		return;

	s = splsoftnet();

	/*
	 * The socket may be attached to more than one vif...this
	 * is perfectly legal.
	 */
	for (vifi = 0; vifi < numvifs; vifi++) {
		if (viftable[vifi].v_rsvpd == so) {
			viftable[vifi].v_rsvpd = NULL;
			/*
			 * This may seem silly, but we need to be sure we don't
			 * over-decrement the RSVP counter, in case something
			 * slips up.
			 */
			if (viftable[vifi].v_rsvp_on) {
				viftable[vifi].v_rsvp_on = 0;
				rsvp_on--;
			}
		}
	}

	splx(s);
	return;
}

void
rsvp_input(struct mbuf *m, struct ifnet *ifp)
{
	int vifi, s;
	struct ip *ip = mtod(m, struct ip *);
	struct sockaddr_in rsvp_src;

	if (rsvpdebug)
		printf("rsvp_input: rsvp_on %d\n", rsvp_on);

	/*
	 * Can still get packets with rsvp_on = 0 if there is a local member
	 * of the group to which the RSVP packet is addressed.  But in this
	 * case we want to throw the packet away.
	 */
	if (!rsvp_on) {
		m_freem(m);
		return;
	}

	/*
	 * If the old-style non-vif-associated socket is set, then use
	 * it and ignore the new ones.
	 */
	if (ip_rsvpd != NULL) {
		if (rsvpdebug)
			printf("rsvp_input: "
			    "Sending packet up old-style socket\n");
		rip_input(m);	/*XXX*/
		return;
	}

	s = splsoftnet();

	if (rsvpdebug)
		printf("rsvp_input: check vifs\n");

	/* Find which vif the packet arrived on. */
	for (vifi = 0; vifi < numvifs; vifi++) {
		if (viftable[vifi].v_ifp == ifp)
			break;
	}

	if (vifi == numvifs) {
		/* Can't find vif packet arrived on. Drop packet. */
		if (rsvpdebug)
			printf("rsvp_input: "
			    "Can't find vif for packet...dropping it.\n");
		m_freem(m);
		splx(s);
		return;
	}

	if (rsvpdebug)
		printf("rsvp_input: check socket\n");

	if (viftable[vifi].v_rsvpd == NULL) {
		/*
		 * drop packet, since there is no specific socket for this
		 * interface
		 */
		if (rsvpdebug)
			printf("rsvp_input: No socket defined for vif %d\n",
			    vifi);
		m_freem(m);
		splx(s);
		return;
	}

	sockaddr_in_init(&rsvp_src, &ip->ip_src, 0);

	if (rsvpdebug && m)
		printf("rsvp_input: m->m_len = %d, sbspace() = %d\n",
		    m->m_len, sbspace(&viftable[vifi].v_rsvpd->so_rcv));

	if (socket_send(viftable[vifi].v_rsvpd, m, &rsvp_src) < 0)
		if (rsvpdebug)
			printf("rsvp_input: Failed to append to socket\n");
	else
		if (rsvpdebug)
			printf("rsvp_input: send packet up\n");

	splx(s);
}
#endif /* RSVP_ISI */

/*
 * Code for bandwidth monitors
 */

/*
 * Define common interface for timeval-related methods
 */
#define	BW_TIMEVALCMP(tvp, uvp, cmp) timercmp((tvp), (uvp), cmp)
#define	BW_TIMEVALDECR(vvp, uvp) timersub((vvp), (uvp), (vvp))
#define	BW_TIMEVALADD(vvp, uvp) timeradd((vvp), (uvp), (vvp))

static uint32_t
compute_bw_meter_flags(struct bw_upcall *req)
{
    uint32_t flags = 0;

    if (req->bu_flags & BW_UPCALL_UNIT_PACKETS)
	flags |= BW_METER_UNIT_PACKETS;
    if (req->bu_flags & BW_UPCALL_UNIT_BYTES)
	flags |= BW_METER_UNIT_BYTES;
    if (req->bu_flags & BW_UPCALL_GEQ)
	flags |= BW_METER_GEQ;
    if (req->bu_flags & BW_UPCALL_LEQ)
	flags |= BW_METER_LEQ;

    return flags;
}

/*
 * Add a bw_meter entry
 */
static int
add_bw_upcall(struct bw_upcall *req)
{
    int s;
    struct mfc *mfc;
    struct timeval delta = { BW_UPCALL_THRESHOLD_INTERVAL_MIN_SEC,
		BW_UPCALL_THRESHOLD_INTERVAL_MIN_USEC };
    struct timeval now;
    struct bw_meter *x;
    uint32_t flags;

    if (!(mrt_api_config & MRT_MFC_BW_UPCALL))
	return EOPNOTSUPP;

    /* Test if the flags are valid */
    if (!(req->bu_flags & (BW_UPCALL_UNIT_PACKETS | BW_UPCALL_UNIT_BYTES)))
	return EINVAL;
    if (!(req->bu_flags & (BW_UPCALL_GEQ | BW_UPCALL_LEQ)))
	return EINVAL;
    if ((req->bu_flags & (BW_UPCALL_GEQ | BW_UPCALL_LEQ))
	    == (BW_UPCALL_GEQ | BW_UPCALL_LEQ))
	return EINVAL;

    /* Test if the threshold time interval is valid */
    if (BW_TIMEVALCMP(&req->bu_threshold.b_time, &delta, <))
	return EINVAL;

    flags = compute_bw_meter_flags(req);

    /*
     * Find if we have already same bw_meter entry
     */
    s = splsoftnet();
    mfc = mfc_find(&req->bu_src, &req->bu_dst);
    if (mfc == NULL) {
	splx(s);
	return EADDRNOTAVAIL;
    }
    for (x = mfc->mfc_bw_meter; x != NULL; x = x->bm_mfc_next) {
	if ((BW_TIMEVALCMP(&x->bm_threshold.b_time,
			   &req->bu_threshold.b_time, ==)) &&
	    (x->bm_threshold.b_packets == req->bu_threshold.b_packets) &&
	    (x->bm_threshold.b_bytes == req->bu_threshold.b_bytes) &&
	    (x->bm_flags & BW_METER_USER_FLAGS) == flags)  {
	    splx(s);
	    return 0;		/* XXX Already installed */
	}
    }

    /* Allocate the new bw_meter entry */
    x = kmem_intr_alloc(sizeof(*x), KM_NOSLEEP);
    if (x == NULL) {
	splx(s);
	return ENOBUFS;
    }

    /* Set the new bw_meter entry */
    x->bm_threshold.b_time = req->bu_threshold.b_time;
    microtime(&now);
    x->bm_start_time = now;
    x->bm_threshold.b_packets = req->bu_threshold.b_packets;
    x->bm_threshold.b_bytes = req->bu_threshold.b_bytes;
    x->bm_measured.b_packets = 0;
    x->bm_measured.b_bytes = 0;
    x->bm_flags = flags;
    x->bm_time_next = NULL;
    x->bm_time_hash = BW_METER_BUCKETS;

    /* Add the new bw_meter entry to the front of entries for this MFC */
    x->bm_mfc = mfc;
    x->bm_mfc_next = mfc->mfc_bw_meter;
    mfc->mfc_bw_meter = x;
    schedule_bw_meter(x, &now);
    splx(s);

    return 0;
}

static void
free_bw_list(struct bw_meter *list)
{
    while (list != NULL) {
	struct bw_meter *x = list;

	list = list->bm_mfc_next;
	unschedule_bw_meter(x);
	kmem_free(x, sizeof(*x));
    }
}

/*
 * Delete one or multiple bw_meter entries
 */
static int
del_bw_upcall(struct bw_upcall *req)
{
    int s;
    struct mfc *mfc;
    struct bw_meter *x;

    if (!(mrt_api_config & MRT_MFC_BW_UPCALL))
	return EOPNOTSUPP;

    s = splsoftnet();
    /* Find the corresponding MFC entry */
    mfc = mfc_find(&req->bu_src, &req->bu_dst);
    if (mfc == NULL) {
	splx(s);
	return EADDRNOTAVAIL;
    } else if (req->bu_flags & BW_UPCALL_DELETE_ALL) {
	/*
	 * Delete all bw_meter entries for this mfc
	 */
	struct bw_meter *list;

	list = mfc->mfc_bw_meter;
	mfc->mfc_bw_meter = NULL;
	free_bw_list(list);
	splx(s);
	return 0;
    } else {			/* Delete a single bw_meter entry */
	struct bw_meter *prev;
	uint32_t flags = 0;

	flags = compute_bw_meter_flags(req);

	/* Find the bw_meter entry to delete */
	for (prev = NULL, x = mfc->mfc_bw_meter; x != NULL;
	     prev = x, x = x->bm_mfc_next) {
	    if ((BW_TIMEVALCMP(&x->bm_threshold.b_time,
			       &req->bu_threshold.b_time, ==)) &&
		(x->bm_threshold.b_packets == req->bu_threshold.b_packets) &&
		(x->bm_threshold.b_bytes == req->bu_threshold.b_bytes) &&
		(x->bm_flags & BW_METER_USER_FLAGS) == flags)
		break;
	}
	if (x != NULL) { /* Delete entry from the list for this MFC */
	    if (prev != NULL)
		prev->bm_mfc_next = x->bm_mfc_next;	/* remove from middle*/
	    else
		x->bm_mfc->mfc_bw_meter = x->bm_mfc_next;/* new head of list */

	    unschedule_bw_meter(x);
	    splx(s);
	    /* Free the bw_meter entry */
	    kmem_free(x, sizeof(*x));
	    return 0;
	} else {
	    splx(s);
	    return EINVAL;
	}
    }
    /* NOTREACHED */
}

/*
 * Perform bandwidth measurement processing that may result in an upcall
 */
static void
bw_meter_receive_packet(struct bw_meter *x, int plen, struct timeval *nowp)
{
    struct timeval delta;

    delta = *nowp;
    BW_TIMEVALDECR(&delta, &x->bm_start_time);

    if (x->bm_flags & BW_METER_GEQ) {
	/*
	 * Processing for ">=" type of bw_meter entry
	 */
	if (BW_TIMEVALCMP(&delta, &x->bm_threshold.b_time, >)) {
	    /* Reset the bw_meter entry */
	    x->bm_start_time = *nowp;
	    x->bm_measured.b_packets = 0;
	    x->bm_measured.b_bytes = 0;
	    x->bm_flags &= ~BW_METER_UPCALL_DELIVERED;
	}

	/* Record that a packet is received */
	x->bm_measured.b_packets++;
	x->bm_measured.b_bytes += plen;

	/*
	 * Test if we should deliver an upcall
	 */
	if (!(x->bm_flags & BW_METER_UPCALL_DELIVERED)) {
	    if (((x->bm_flags & BW_METER_UNIT_PACKETS) &&
		 (x->bm_measured.b_packets >= x->bm_threshold.b_packets)) ||
		((x->bm_flags & BW_METER_UNIT_BYTES) &&
		 (x->bm_measured.b_bytes >= x->bm_threshold.b_bytes))) {
		/* Prepare an upcall for delivery */
		bw_meter_prepare_upcall(x, nowp);
		x->bm_flags |= BW_METER_UPCALL_DELIVERED;
	    }
	}
    } else if (x->bm_flags & BW_METER_LEQ) {
	/*
	 * Processing for "<=" type of bw_meter entry
	 */
	if (BW_TIMEVALCMP(&delta, &x->bm_threshold.b_time, >)) {
	    /*
	     * We are behind time with the multicast forwarding table
	     * scanning for "<=" type of bw_meter entries, so test now
	     * if we should deliver an upcall.
	     */
	    if (((x->bm_flags & BW_METER_UNIT_PACKETS) &&
		 (x->bm_measured.b_packets <= x->bm_threshold.b_packets)) ||
		((x->bm_flags & BW_METER_UNIT_BYTES) &&
		 (x->bm_measured.b_bytes <= x->bm_threshold.b_bytes))) {
		/* Prepare an upcall for delivery */
		bw_meter_prepare_upcall(x, nowp);
	    }
	    /* Reschedule the bw_meter entry */
	    unschedule_bw_meter(x);
	    schedule_bw_meter(x, nowp);
	}

	/* Record that a packet is received */
	x->bm_measured.b_packets++;
	x->bm_measured.b_bytes += plen;

	/*
	 * Test if we should restart the measuring interval
	 */
	if ((x->bm_flags & BW_METER_UNIT_PACKETS &&
	     x->bm_measured.b_packets <= x->bm_threshold.b_packets) ||
	    (x->bm_flags & BW_METER_UNIT_BYTES &&
	     x->bm_measured.b_bytes <= x->bm_threshold.b_bytes)) {
	    /* Don't restart the measuring interval */
	} else {
	    /* Do restart the measuring interval */
	    /*
	     * XXX: note that we don't unschedule and schedule, because this
	     * might be too much overhead per packet. Instead, when we process
	     * all entries for a given timer hash bin, we check whether it is
	     * really a timeout. If not, we reschedule at that time.
	     */
	    x->bm_start_time = *nowp;
	    x->bm_measured.b_packets = 0;
	    x->bm_measured.b_bytes = 0;
	    x->bm_flags &= ~BW_METER_UPCALL_DELIVERED;
	}
    }
}

/*
 * Prepare a bandwidth-related upcall
 */
static void
bw_meter_prepare_upcall(struct bw_meter *x, struct timeval *nowp)
{
    struct timeval delta;
    struct bw_upcall *u;

    /*
     * Compute the measured time interval
     */
    delta = *nowp;
    BW_TIMEVALDECR(&delta, &x->bm_start_time);

    /*
     * If there are too many pending upcalls, deliver them now
     */
    if (bw_upcalls_n >= BW_UPCALLS_MAX)
	bw_upcalls_send();

    /*
     * Set the bw_upcall entry
     */
    u = &bw_upcalls[bw_upcalls_n++];
    u->bu_src = x->bm_mfc->mfc_origin;
    u->bu_dst = x->bm_mfc->mfc_mcastgrp;
    u->bu_threshold.b_time = x->bm_threshold.b_time;
    u->bu_threshold.b_packets = x->bm_threshold.b_packets;
    u->bu_threshold.b_bytes = x->bm_threshold.b_bytes;
    u->bu_measured.b_time = delta;
    u->bu_measured.b_packets = x->bm_measured.b_packets;
    u->bu_measured.b_bytes = x->bm_measured.b_bytes;
    u->bu_flags = 0;
    if (x->bm_flags & BW_METER_UNIT_PACKETS)
	u->bu_flags |= BW_UPCALL_UNIT_PACKETS;
    if (x->bm_flags & BW_METER_UNIT_BYTES)
	u->bu_flags |= BW_UPCALL_UNIT_BYTES;
    if (x->bm_flags & BW_METER_GEQ)
	u->bu_flags |= BW_UPCALL_GEQ;
    if (x->bm_flags & BW_METER_LEQ)
	u->bu_flags |= BW_UPCALL_LEQ;
}

/*
 * Send the pending bandwidth-related upcalls
 */
static void
bw_upcalls_send(void)
{
    struct mbuf *m;
    int len = bw_upcalls_n * sizeof(bw_upcalls[0]);
    struct sockaddr_in k_igmpsrc = { 
	    .sin_len = sizeof(k_igmpsrc),
	    .sin_family = AF_INET,
    };
    static struct igmpmsg igmpmsg = { 0,		/* unused1 */
				      0,		/* unused2 */
				      IGMPMSG_BW_UPCALL,/* im_msgtype */
				      0,		/* im_mbz  */
				      0,		/* im_vif  */
				      0,		/* unused3 */
				      { 0 },		/* im_src  */
				      { 0 } };		/* im_dst  */

    if (bw_upcalls_n == 0)
	return;			/* No pending upcalls */

    bw_upcalls_n = 0;

    /*
     * Allocate a new mbuf, initialize it with the header and
     * the payload for the pending calls.
     */
    MGETHDR(m, M_DONTWAIT, MT_HEADER);
    if (m == NULL) {
	log(LOG_WARNING, "bw_upcalls_send: cannot allocate mbuf\n");
	return;
    }

    m->m_len = m->m_pkthdr.len = 0;
    m_copyback(m, 0, sizeof(struct igmpmsg), (void *)&igmpmsg);
    m_copyback(m, sizeof(struct igmpmsg), len, (void *)&bw_upcalls[0]);

    /*
     * Send the upcalls
     * XXX do we need to set the address in k_igmpsrc ?
     */
    mrtstat.mrts_upcalls++;
    if (socket_send(ip_mrouter, m, &k_igmpsrc) < 0) {
	log(LOG_WARNING, "bw_upcalls_send: ip_mrouter socket queue full\n");
	++mrtstat.mrts_upq_sockfull;
    }
}

/*
 * Compute the timeout hash value for the bw_meter entries
 */
#define	BW_METER_TIMEHASH(bw_meter, hash)				\
    do {								\
	struct timeval next_timeval = (bw_meter)->bm_start_time;	\
									\
	BW_TIMEVALADD(&next_timeval, &(bw_meter)->bm_threshold.b_time); \
	(hash) = next_timeval.tv_sec;					\
	if (next_timeval.tv_usec)					\
	    (hash)++; /* XXX: make sure we don't timeout early */	\
	(hash) %= BW_METER_BUCKETS;					\
    } while (/*CONSTCOND*/ 0)

/*
 * Schedule a timer to process periodically bw_meter entry of type "<="
 * by linking the entry in the proper hash bucket.
 */
static void
schedule_bw_meter(struct bw_meter *x, struct timeval *nowp)
{
    int time_hash;

    if (!(x->bm_flags & BW_METER_LEQ))
	return;		/* XXX: we schedule timers only for "<=" entries */

    /*
     * Reset the bw_meter entry
     */
    x->bm_start_time = *nowp;
    x->bm_measured.b_packets = 0;
    x->bm_measured.b_bytes = 0;
    x->bm_flags &= ~BW_METER_UPCALL_DELIVERED;

    /*
     * Compute the timeout hash value and insert the entry
     */
    BW_METER_TIMEHASH(x, time_hash);
    x->bm_time_next = bw_meter_timers[time_hash];
    bw_meter_timers[time_hash] = x;
    x->bm_time_hash = time_hash;
}

/*
 * Unschedule the periodic timer that processes bw_meter entry of type "<="
 * by removing the entry from the proper hash bucket.
 */
static void
unschedule_bw_meter(struct bw_meter *x)
{
    int time_hash;
    struct bw_meter *prev, *tmp;

    if (!(x->bm_flags & BW_METER_LEQ))
	return;		/* XXX: we schedule timers only for "<=" entries */

    /*
     * Compute the timeout hash value and delete the entry
     */
    time_hash = x->bm_time_hash;
    if (time_hash >= BW_METER_BUCKETS)
	return;		/* Entry was not scheduled */

    for (prev = NULL, tmp = bw_meter_timers[time_hash];
	     tmp != NULL; prev = tmp, tmp = tmp->bm_time_next)
	if (tmp == x)
	    break;

    if (tmp == NULL)
	panic("unschedule_bw_meter: bw_meter entry not found");

    if (prev != NULL)
	prev->bm_time_next = x->bm_time_next;
    else
	bw_meter_timers[time_hash] = x->bm_time_next;

    x->bm_time_next = NULL;
    x->bm_time_hash = BW_METER_BUCKETS;
}

/*
 * Process all "<=" type of bw_meter that should be processed now,
 * and for each entry prepare an upcall if necessary. Each processed
 * entry is rescheduled again for the (periodic) processing.
 *
 * This is run periodically (once per second normally). On each round,
 * all the potentially matching entries are in the hash slot that we are
 * looking at.
 */
static void
bw_meter_process(void)
{
    int s;
    static uint32_t last_tv_sec;	/* last time we processed this */

    uint32_t loops;
    int i;
    struct timeval now, process_endtime;

    microtime(&now);
    if (last_tv_sec == now.tv_sec)
	return;		/* nothing to do */

    loops = now.tv_sec - last_tv_sec;
    last_tv_sec = now.tv_sec;
    if (loops > BW_METER_BUCKETS)
	loops = BW_METER_BUCKETS;

    s = splsoftnet();
    /*
     * Process all bins of bw_meter entries from the one after the last
     * processed to the current one. On entry, i points to the last bucket
     * visited, so we need to increment i at the beginning of the loop.
     */
    for (i = (now.tv_sec - loops) % BW_METER_BUCKETS; loops > 0; loops--) {
	struct bw_meter *x, *tmp_list;

	if (++i >= BW_METER_BUCKETS)
	    i = 0;

	/* Disconnect the list of bw_meter entries from the bin */
	tmp_list = bw_meter_timers[i];
	bw_meter_timers[i] = NULL;

	/* Process the list of bw_meter entries */
	while (tmp_list != NULL) {
	    x = tmp_list;
	    tmp_list = tmp_list->bm_time_next;

	    /* Test if the time interval is over */
	    process_endtime = x->bm_start_time;
	    BW_TIMEVALADD(&process_endtime, &x->bm_threshold.b_time);
	    if (BW_TIMEVALCMP(&process_endtime, &now, >)) {
		/* Not yet: reschedule, but don't reset */
		int time_hash;

		BW_METER_TIMEHASH(x, time_hash);
		if (time_hash == i && process_endtime.tv_sec == now.tv_sec) {
		    /*
		     * XXX: somehow the bin processing is a bit ahead of time.
		     * Put the entry in the next bin.
		     */
		    if (++time_hash >= BW_METER_BUCKETS)
			time_hash = 0;
		}
		x->bm_time_next = bw_meter_timers[time_hash];
		bw_meter_timers[time_hash] = x;
		x->bm_time_hash = time_hash;

		continue;
	    }

	    /*
	     * Test if we should deliver an upcall
	     */
	    if (((x->bm_flags & BW_METER_UNIT_PACKETS) &&
		 (x->bm_measured.b_packets <= x->bm_threshold.b_packets)) ||
		((x->bm_flags & BW_METER_UNIT_BYTES) &&
		 (x->bm_measured.b_bytes <= x->bm_threshold.b_bytes))) {
		/* Prepare an upcall for delivery */
		bw_meter_prepare_upcall(x, &now);
	    }

	    /*
	     * Reschedule for next processing
	     */
	    schedule_bw_meter(x, &now);
	}
    }

    /* Send all upcalls that are pending delivery */
    bw_upcalls_send();

    splx(s);
}

/*
 * A periodic function for sending all upcalls that are pending delivery
 */
static void
expire_bw_upcalls_send(void *unused)
{
    int s;

    s = splsoftnet();
    bw_upcalls_send();
    splx(s);

    callout_reset(&bw_upcalls_ch, BW_UPCALLS_PERIOD,
		  expire_bw_upcalls_send, NULL);
}

/*
 * A periodic function for periodic scanning of the multicast forwarding
 * table for processing all "<=" bw_meter entries.
 */
static void
expire_bw_meter_process(void *unused)
{
    if (mrt_api_config & MRT_MFC_BW_UPCALL)
	bw_meter_process();

    callout_reset(&bw_meter_ch, BW_METER_PERIOD,
		  expire_bw_meter_process, NULL);
}

/*
 * End of bandwidth monitoring code
 */

#ifdef PIM
/*
 * Send the packet up to the user daemon, or eventually do kernel encapsulation
 */
static int
pim_register_send(struct ip *ip, struct vif *vifp,
	struct mbuf *m, struct mfc *rt)
{
    struct mbuf *mb_copy, *mm;

    if (mrtdebug & DEBUG_PIM)
        log(LOG_DEBUG, "pim_register_send: \n");

    mb_copy = pim_register_prepare(ip, m);
    if (mb_copy == NULL)
	return ENOBUFS;

    /*
     * Send all the fragments. Note that the mbuf for each fragment
     * is freed by the sending machinery.
     */
    for (mm = mb_copy; mm; mm = mb_copy) {
	mb_copy = mm->m_nextpkt;
	mm->m_nextpkt = NULL;
	mm = m_pullup(mm, sizeof(struct ip));
	if (mm != NULL) {
	    ip = mtod(mm, struct ip *);
	    if ((mrt_api_config & MRT_MFC_RP) &&
		!in_nullhost(rt->mfc_rp)) {
		pim_register_send_rp(ip, vifp, mm, rt);
	    } else {
		pim_register_send_upcall(ip, vifp, mm, rt);
	    }
	}
    }

    return 0;
}

/*
 * Return a copy of the data packet that is ready for PIM Register
 * encapsulation.
 * XXX: Note that in the returned copy the IP header is a valid one.
 */
static struct mbuf *
pim_register_prepare(struct ip *ip, struct mbuf *m)
{
    struct mbuf *mb_copy = NULL;
    int mtu;

    /* Take care of delayed checksums */
    if (m->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
	in_delayed_cksum(m);
	m->m_pkthdr.csum_flags &= ~(M_CSUM_TCPv4|M_CSUM_UDPv4);
    }

    /*
     * Copy the old packet & pullup its IP header into the
     * new mbuf so we can modify it.
     */
    mb_copy = m_copypacket(m, M_DONTWAIT);
    if (mb_copy == NULL)
	return NULL;
    mb_copy = m_pullup(mb_copy, ip->ip_hl << 2);
    if (mb_copy == NULL)
	return NULL;

    /* take care of the TTL */
    ip = mtod(mb_copy, struct ip *);
    --ip->ip_ttl;

    /* Compute the MTU after the PIM Register encapsulation */
    mtu = 0xffff - sizeof(pim_encap_iphdr) - sizeof(pim_encap_pimhdr);

    if (ntohs(ip->ip_len) <= mtu) {
	/* Turn the IP header into a valid one */
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
    } else {
	/* Fragment the packet */
	if (ip_fragment(mb_copy, NULL, mtu) != 0) {
	    /* XXX: mb_copy was freed by ip_fragment() */
	    return NULL;
	}
    }
    return mb_copy;
}

/*
 * Send an upcall with the data packet to the user-level process.
 */
static int
pim_register_send_upcall(struct ip *ip, struct vif *vifp,
    struct mbuf *mb_copy, struct mfc *rt)
{
    struct mbuf *mb_first;
    int len = ntohs(ip->ip_len);
    struct igmpmsg *im;
    struct sockaddr_in k_igmpsrc = {
	    .sin_len = sizeof(k_igmpsrc),
	    .sin_family = AF_INET,
    };

    /*
     * Add a new mbuf with an upcall header
     */
    MGETHDR(mb_first, M_DONTWAIT, MT_HEADER);
    if (mb_first == NULL) {
	m_freem(mb_copy);
	return ENOBUFS;
    }
    mb_first->m_data += max_linkhdr;
    mb_first->m_pkthdr.len = len + sizeof(struct igmpmsg);
    mb_first->m_len = sizeof(struct igmpmsg);
    mb_first->m_next = mb_copy;

    /* Send message to routing daemon */
    im = mtod(mb_first, struct igmpmsg *);
    im->im_msgtype	= IGMPMSG_WHOLEPKT;
    im->im_mbz		= 0;
    im->im_vif		= vifp - viftable;
    im->im_src		= ip->ip_src;
    im->im_dst		= ip->ip_dst;

    k_igmpsrc.sin_addr	= ip->ip_src;

    mrtstat.mrts_upcalls++;

    if (socket_send(ip_mrouter, mb_first, &k_igmpsrc) < 0) {
	if (mrtdebug & DEBUG_PIM)
	    log(LOG_WARNING,
		"mcast: pim_register_send_upcall: ip_mrouter socket queue full\n");
	++mrtstat.mrts_upq_sockfull;
	return ENOBUFS;
    }

    /* Keep statistics */
    pimstat.pims_snd_registers_msgs++;
    pimstat.pims_snd_registers_bytes += len;

    return 0;
}

/*
 * Encapsulate the data packet in PIM Register message and send it to the RP.
 */
static int
pim_register_send_rp(struct ip *ip, struct vif *vifp,
	struct mbuf *mb_copy, struct mfc *rt)
{
    struct mbuf *mb_first;
    struct ip *ip_outer;
    struct pim_encap_pimhdr *pimhdr;
    int len = ntohs(ip->ip_len);
    vifi_t vifi = rt->mfc_parent;

    if ((vifi >= numvifs) || in_nullhost(viftable[vifi].v_lcl_addr)) {
	m_freem(mb_copy);
	return EADDRNOTAVAIL;		/* The iif vif is invalid */
    }

    /*
     * Add a new mbuf with the encapsulating header
     */
    MGETHDR(mb_first, M_DONTWAIT, MT_HEADER);
    if (mb_first == NULL) {
	m_freem(mb_copy);
	return ENOBUFS;
    }
    mb_first->m_data += max_linkhdr;
    mb_first->m_len = sizeof(pim_encap_iphdr) + sizeof(pim_encap_pimhdr);
    mb_first->m_next = mb_copy;

    mb_first->m_pkthdr.len = len + mb_first->m_len;

    /*
     * Fill in the encapsulating IP and PIM header
     */
    ip_outer = mtod(mb_first, struct ip *);
    *ip_outer = pim_encap_iphdr;
     if (mb_first->m_pkthdr.len < IP_MINFRAGSIZE)
	ip_outer->ip_id = 0;
    else
	ip_outer->ip_id = ip_newid(NULL);
    ip_outer->ip_len = htons(len + sizeof(pim_encap_iphdr) +
			     sizeof(pim_encap_pimhdr));
    ip_outer->ip_src = viftable[vifi].v_lcl_addr;
    ip_outer->ip_dst = rt->mfc_rp;
    /*
     * Copy the inner header TOS to the outer header, and take care of the
     * IP_DF bit.
     */
    ip_outer->ip_tos = ip->ip_tos;
    if (ntohs(ip->ip_off) & IP_DF)
	ip_outer->ip_off |= htons(IP_DF);
    pimhdr = (struct pim_encap_pimhdr *)((char *)ip_outer
					 + sizeof(pim_encap_iphdr));
    *pimhdr = pim_encap_pimhdr;
    /* If the iif crosses a border, set the Border-bit */
    if (rt->mfc_flags[vifi] & MRT_MFC_FLAGS_BORDER_VIF & mrt_api_config)
	pimhdr->flags |= htonl(PIM_BORDER_REGISTER);

    mb_first->m_data += sizeof(pim_encap_iphdr);
    pimhdr->pim.pim_cksum = in_cksum(mb_first, sizeof(pim_encap_pimhdr));
    mb_first->m_data -= sizeof(pim_encap_iphdr);

    if (vifp->v_rate_limit == 0)
	tbf_send_packet(vifp, mb_first);
    else
	tbf_control(vifp, mb_first, ip, ntohs(ip_outer->ip_len));

    /* Keep statistics */
    pimstat.pims_snd_registers_msgs++;
    pimstat.pims_snd_registers_bytes += len;

    return 0;
}

/*
 * PIM-SMv2 and PIM-DM messages processing.
 * Receives and verifies the PIM control messages, and passes them
 * up to the listening socket, using rip_input().
 * The only message with special processing is the PIM_REGISTER message
 * (used by PIM-SM): the PIM header is stripped off, and the inner packet
 * is passed to if_simloop().
 */
void
pim_input(struct mbuf *m, ...)
{
    struct ip *ip = mtod(m, struct ip *);
    struct pim *pim;
    int minlen;
    int datalen;
    int ip_tos;
    int proto;
    int iphlen;
    va_list ap;

    va_start(ap, m);
    iphlen = va_arg(ap, int);
    proto = va_arg(ap, int);
    va_end(ap);

    datalen = ntohs(ip->ip_len) - iphlen;

    /* Keep statistics */
    pimstat.pims_rcv_total_msgs++;
    pimstat.pims_rcv_total_bytes += datalen;

    /*
     * Validate lengths
     */
    if (datalen < PIM_MINLEN) {
	pimstat.pims_rcv_tooshort++;
	log(LOG_ERR, "pim_input: packet size too small %d from %lx\n",
	    datalen, (u_long)ip->ip_src.s_addr);
	m_freem(m);
	return;
    }

    /*
     * If the packet is at least as big as a REGISTER, go agead
     * and grab the PIM REGISTER header size, to avoid another
     * possible m_pullup() later.
     *
     * PIM_MINLEN       == pimhdr + u_int32_t == 4 + 4 = 8
     * PIM_REG_MINLEN   == pimhdr + reghdr + encap_iphdr == 4 + 4 + 20 = 28
     */
    minlen = iphlen + (datalen >= PIM_REG_MINLEN ? PIM_REG_MINLEN : PIM_MINLEN);
    /*
     * Get the IP and PIM headers in contiguous memory, and
     * possibly the PIM REGISTER header.
     */
    if ((m->m_flags & M_EXT || m->m_len < minlen) &&
	(m = m_pullup(m, minlen)) == NULL) {
	log(LOG_ERR, "pim_input: m_pullup failure\n");
	return;
    }
    /* m_pullup() may have given us a new mbuf so reset ip. */
    ip = mtod(m, struct ip *);
    ip_tos = ip->ip_tos;

    /* adjust mbuf to point to the PIM header */
    m->m_data += iphlen;
    m->m_len  -= iphlen;
    pim = mtod(m, struct pim *);

    /*
     * Validate checksum. If PIM REGISTER, exclude the data packet.
     *
     * XXX: some older PIMv2 implementations don't make this distinction,
     * so for compatibility reason perform the checksum over part of the
     * message, and if error, then over the whole message.
     */
    if (PIM_VT_T(pim->pim_vt) == PIM_REGISTER && in_cksum(m, PIM_MINLEN) == 0) {
	/* do nothing, checksum okay */
    } else if (in_cksum(m, datalen)) {
	pimstat.pims_rcv_badsum++;
	if (mrtdebug & DEBUG_PIM)
	    log(LOG_DEBUG, "pim_input: invalid checksum\n");
	m_freem(m);
	return;
    }

    /* PIM version check */
    if (PIM_VT_V(pim->pim_vt) < PIM_VERSION) {
	pimstat.pims_rcv_badversion++;
	log(LOG_ERR, "pim_input: incorrect version %d, expecting %d\n",
	    PIM_VT_V(pim->pim_vt), PIM_VERSION);
	m_freem(m);
	return;
    }

    /* restore mbuf back to the outer IP */
    m->m_data -= iphlen;
    m->m_len  += iphlen;

    if (PIM_VT_T(pim->pim_vt) == PIM_REGISTER) {
	/*
	 * Since this is a REGISTER, we'll make a copy of the register
	 * headers ip + pim + u_int32 + encap_ip, to be passed up to the
	 * routing daemon.
	 */
	int s;
	struct sockaddr_in dst = {
		.sin_len = sizeof(dst),
		.sin_family = AF_INET,
	};
	struct mbuf *mcp;
	struct ip *encap_ip;
	u_int32_t *reghdr;
	struct ifnet *vifp;

	s = splsoftnet();
	if ((reg_vif_num >= numvifs) || (reg_vif_num == VIFI_INVALID)) {
	    splx(s);
	    if (mrtdebug & DEBUG_PIM)
		log(LOG_DEBUG,
		    "pim_input: register vif not set: %d\n", reg_vif_num);
	    m_freem(m);
	    return;
	}
	/* XXX need refcnt? */
	vifp = viftable[reg_vif_num].v_ifp;
	splx(s);

	/*
	 * Validate length
	 */
	if (datalen < PIM_REG_MINLEN) {
	    pimstat.pims_rcv_tooshort++;
	    pimstat.pims_rcv_badregisters++;
	    log(LOG_ERR,
		"pim_input: register packet size too small %d from %lx\n",
		datalen, (u_long)ip->ip_src.s_addr);
	    m_freem(m);
	    return;
	}

	reghdr = (u_int32_t *)(pim + 1);
	encap_ip = (struct ip *)(reghdr + 1);

	if (mrtdebug & DEBUG_PIM) {
	    log(LOG_DEBUG,
		"pim_input[register], encap_ip: %lx -> %lx, encap_ip len %d\n",
		(u_long)ntohl(encap_ip->ip_src.s_addr),
		(u_long)ntohl(encap_ip->ip_dst.s_addr),
		ntohs(encap_ip->ip_len));
	}

	/* verify the version number of the inner packet */
	if (encap_ip->ip_v != IPVERSION) {
	    pimstat.pims_rcv_badregisters++;
	    if (mrtdebug & DEBUG_PIM) {
		log(LOG_DEBUG, "pim_input: invalid IP version (%d) "
		    "of the inner packet\n", encap_ip->ip_v);
	    }
	    m_freem(m);
	    return;
	}

	/* verify the inner packet is destined to a mcast group */
	if (!IN_MULTICAST(encap_ip->ip_dst.s_addr)) {
	    pimstat.pims_rcv_badregisters++;
	    if (mrtdebug & DEBUG_PIM)
		log(LOG_DEBUG,
		    "pim_input: inner packet of register is not "
		    "multicast %lx\n",
		    (u_long)ntohl(encap_ip->ip_dst.s_addr));
	    m_freem(m);
	    return;
	}

	/* If a NULL_REGISTER, pass it to the daemon */
	if ((ntohl(*reghdr) & PIM_NULL_REGISTER))
	    goto pim_input_to_daemon;

	/*
	 * Copy the TOS from the outer IP header to the inner IP header.
	 */
	if (encap_ip->ip_tos != ip_tos) {
	    /* Outer TOS -> inner TOS */
	    encap_ip->ip_tos = ip_tos;
	    /* Recompute the inner header checksum. Sigh... */

	    /* adjust mbuf to point to the inner IP header */
	    m->m_data += (iphlen + PIM_MINLEN);
	    m->m_len  -= (iphlen + PIM_MINLEN);

	    encap_ip->ip_sum = 0;
	    encap_ip->ip_sum = in_cksum(m, encap_ip->ip_hl << 2);

	    /* restore mbuf to point back to the outer IP header */
	    m->m_data -= (iphlen + PIM_MINLEN);
	    m->m_len  += (iphlen + PIM_MINLEN);
	}

	/*
	 * Decapsulate the inner IP packet and loopback to forward it
	 * as a normal multicast packet. Also, make a copy of the
	 *     outer_iphdr + pimhdr + reghdr + encap_iphdr
	 * to pass to the daemon later, so it can take the appropriate
	 * actions (e.g., send back PIM_REGISTER_STOP).
	 * XXX: here m->m_data points to the outer IP header.
	 */
	mcp = m_copym(m, 0, iphlen + PIM_REG_MINLEN, M_DONTWAIT);
	if (mcp == NULL) {
	    log(LOG_ERR,
		"pim_input: pim register: could not copy register head\n");
	    m_freem(m);
	    return;
	}

	/* Keep statistics */
	/* XXX: registers_bytes include only the encap. mcast pkt */
	pimstat.pims_rcv_registers_msgs++;
	pimstat.pims_rcv_registers_bytes += ntohs(encap_ip->ip_len);

	/*
	 * forward the inner ip packet; point m_data at the inner ip.
	 */
	m_adj(m, iphlen + PIM_MINLEN);

	if (mrtdebug & DEBUG_PIM) {
	    log(LOG_DEBUG,
		"pim_input: forwarding decapsulated register: "
		"src %lx, dst %lx, vif %d\n",
		(u_long)ntohl(encap_ip->ip_src.s_addr),
		(u_long)ntohl(encap_ip->ip_dst.s_addr),
		reg_vif_num);
	}
	/* NB: vifp was collected above; can it change on us? */
	looutput(vifp, m, (struct sockaddr *)&dst, NULL);

	/* prepare the register head to send to the mrouting daemon */
	m = mcp;
    }

pim_input_to_daemon:
    /*
     * Pass the PIM message up to the daemon; if it is a Register message,
     * pass the 'head' only up to the daemon. This includes the
     * outer IP header, PIM header, PIM-Register header and the
     * inner IP header.
     * XXX: the outer IP header pkt size of a Register is not adjust to
     * reflect the fact that the inner multicast data is truncated.
     */
    rip_input(m, iphlen, proto);

    return;
}
#endif /* PIM */
