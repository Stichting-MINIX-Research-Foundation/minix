/*	$NetBSD: ddp_input.c,v 1.26 2011/08/31 18:31:03 plunky Exp $	 */

/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ddp_input.c,v 1.26 2011/08/31 18:31:03 plunky Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <net/netisr.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_ether.h>
#include <netinet/in.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/ddp_private.h>
#include <netatalk/at_extern.h>

int             ddp_forward = 1;
int             ddp_firewall = 0;
extern int      ddp_cksum;
void            ddp_input(struct mbuf *, struct ifnet *,
    struct elaphdr *, int);

/*
 * Could probably merge these two code segments a little better...
 */
void
atintr(void)
{
	struct elaphdr *elhp, elh;
	struct ifnet   *ifp;
	struct mbuf    *m;
	struct at_ifaddr *aa;
	int             s;

	mutex_enter(softnet_lock);
	for (;;) {
		s = splnet();

		IF_DEQUEUE(&atintrq2, m);

		splx(s);

		if (m == 0)	/* no more queued packets */
			break;

		m_claimm(m, &atalk_rx_mowner);
		ifp = m->m_pkthdr.rcvif;
		for (aa = at_ifaddr.tqh_first; aa; aa = aa->aa_list.tqe_next) {
			if (aa->aa_ifp == ifp && (aa->aa_flags & AFA_PHASE2))
				break;
		}
		if (aa == NULL) {	/* ifp not an appletalk interface */
			m_freem(m);
			continue;
		}
		ddp_input(m, ifp, NULL, 2);
	}

	for (;;) {
		s = splnet();

		IF_DEQUEUE(&atintrq1, m);

		splx(s);

		if (m == 0)	/* no more queued packets */

			break;

		m_claimm(m, &atalk_rx_mowner);
		ifp = m->m_pkthdr.rcvif;
		for (aa = at_ifaddr.tqh_first; aa; aa = aa->aa_list.tqe_next) {
			if (aa->aa_ifp == ifp &&
			    (aa->aa_flags & AFA_PHASE2) == 0)
				break;
		}
		if (aa == NULL) {	/* ifp not an appletalk interface */
			m_freem(m);
			continue;
		}
		if (m->m_len < SZ_ELAPHDR &&
		    ((m = m_pullup(m, SZ_ELAPHDR)) == 0)) {
			DDP_STATINC(DDP_STAT_TOOSHORT);
			continue;
		}
		elhp = mtod(m, struct elaphdr *);
		m_adj(m, SZ_ELAPHDR);

		if (elhp->el_type == ELAP_DDPEXTEND) {
			ddp_input(m, ifp, NULL, 1);
		} else {
			memcpy((void *) & elh, (void *) elhp, SZ_ELAPHDR);
			ddp_input(m, ifp, &elh, 1);
		}
	}
	mutex_exit(softnet_lock);
}

struct route    forwro;

void
ddp_input(struct mbuf *m, struct ifnet *ifp, struct elaphdr *elh, int phase)
{
	struct rtentry *rt;
	struct sockaddr_at from, to;
	struct ddpshdr *dsh, ddps;
	struct at_ifaddr *aa;
	struct ddpehdr *deh = NULL, ddpe;
	struct ddpcb   *ddp;
	int             dlen, mlen;
	u_short         cksum = 0;
	union {
		struct sockaddr		dst;
		struct sockaddr_at	dsta;
	} u;

	memset((void *) & from, 0, sizeof(struct sockaddr_at));
	if (elh) {
		DDP_STATINC(DDP_STAT_SHORT);

		if (m->m_len < sizeof(struct ddpshdr) &&
		    ((m = m_pullup(m, sizeof(struct ddpshdr))) == 0)) {
			DDP_STATINC(DDP_STAT_TOOSHORT);
			return;
		}
		dsh = mtod(m, struct ddpshdr *);
		memcpy((void *) & ddps, (void *) dsh, sizeof(struct ddpshdr));
		ddps.dsh_bytes = ntohl(ddps.dsh_bytes);
		dlen = ddps.dsh_len;

		to.sat_addr.s_net = ATADDR_ANYNET;
		to.sat_addr.s_node = elh->el_dnode;
		to.sat_port = ddps.dsh_dport;
		from.sat_addr.s_net = ATADDR_ANYNET;
		from.sat_addr.s_node = elh->el_snode;
		from.sat_port = ddps.dsh_sport;

		for (aa = at_ifaddr.tqh_first; aa; aa = aa->aa_list.tqe_next) {
			if (aa->aa_ifp == ifp &&
			    (aa->aa_flags & AFA_PHASE2) == 0 &&
			    (AA_SAT(aa)->sat_addr.s_node ==
			     to.sat_addr.s_node ||
			     to.sat_addr.s_node == ATADDR_BCAST))
				break;
		}
		if (aa == NULL) {
			m_freem(m);
			return;
		}
	} else {
		DDP_STATINC(DDP_STAT_LONG);

		if (m->m_len < sizeof(struct ddpehdr) &&
		    ((m = m_pullup(m, sizeof(struct ddpehdr))) == 0)) {
			DDP_STATINC(DDP_STAT_TOOSHORT);
			return;
		}
		deh = mtod(m, struct ddpehdr *);
		memcpy((void *) & ddpe, (void *) deh, sizeof(struct ddpehdr));
		ddpe.deh_bytes = ntohl(ddpe.deh_bytes);
		dlen = ddpe.deh_len;

		if ((cksum = ddpe.deh_sum) == 0) {
			DDP_STATINC(DDP_STAT_NOSUM);
		}
		from.sat_addr.s_net = ddpe.deh_snet;
		from.sat_addr.s_node = ddpe.deh_snode;
		from.sat_port = ddpe.deh_sport;
		to.sat_addr.s_net = ddpe.deh_dnet;
		to.sat_addr.s_node = ddpe.deh_dnode;
		to.sat_port = ddpe.deh_dport;

		if (to.sat_addr.s_net == ATADDR_ANYNET) {
			for (aa = at_ifaddr.tqh_first; aa;
			    aa = aa->aa_list.tqe_next) {
				if (phase == 1 && (aa->aa_flags & AFA_PHASE2))
					continue;

				if (phase == 2 &&
				    (aa->aa_flags & AFA_PHASE2) == 0)
					continue;

				if (aa->aa_ifp == ifp &&
				    (AA_SAT(aa)->sat_addr.s_node ==
				     to.sat_addr.s_node ||
				     to.sat_addr.s_node == ATADDR_BCAST ||
				     (ifp->if_flags & IFF_LOOPBACK)))
					break;
			}
		} else {
			for (aa = at_ifaddr.tqh_first; aa;
			    aa = aa->aa_list.tqe_next) {
				if (to.sat_addr.s_net == aa->aa_firstnet &&
				    to.sat_addr.s_node == 0)
					break;

				if ((ntohs(to.sat_addr.s_net) <
				     ntohs(aa->aa_firstnet) ||
				     ntohs(to.sat_addr.s_net) >
				     ntohs(aa->aa_lastnet)) &&
				    (ntohs(to.sat_addr.s_net) < 0xff00 ||
				     ntohs(to.sat_addr.s_net) > 0xfffe))
					continue;

				if (to.sat_addr.s_node !=
				    AA_SAT(aa)->sat_addr.s_node &&
				    to.sat_addr.s_node != ATADDR_BCAST)
					continue;

				break;
			}
		}
	}

	/*
         * Adjust the length, removing any padding that may have been added
         * at a link layer.  We do this before we attempt to forward a packet,
         * possibly on a different media.
         */
	mlen = m->m_pkthdr.len;
	if (mlen < dlen) {
		DDP_STATINC(DDP_STAT_TOOSMALL);
		m_freem(m);
		return;
	}
	if (mlen > dlen) {
		m_adj(m, dlen - mlen);
	}
	/*
         * XXX Should we deliver broadcasts locally, also, or rely on the
         * link layer to give us a copy?  For the moment, the latter.
         */
	if (aa == NULL || (to.sat_addr.s_node == ATADDR_BCAST &&
		aa->aa_ifp != ifp && (ifp->if_flags & IFF_LOOPBACK) == 0)) {
		if (ddp_forward == 0) {
			m_freem(m);
			return;
		}
		sockaddr_at_init(&u.dsta, &to.sat_addr, 0);
		rt = rtcache_lookup(&forwro, &u.dst);
#if 0		/* XXX The if-condition is always false.  What was this
		 * actually trying to test?
		 */
		if (to.sat_addr.s_net !=
		    satocsat(rtcache_getdst(&forwro))->sat_addr.s_net &&
		    ddpe.deh_hops == DDP_MAXHOPS) {
			m_freem(m);
			return;
		}
#endif
		if (ddp_firewall && (rt == NULL || rt->rt_ifp != ifp)) {
			m_freem(m);
			return;
		}
		ddpe.deh_hops++;
		ddpe.deh_bytes = htonl(ddpe.deh_bytes);
		memcpy((void *) deh, (void *) & ddpe, sizeof(u_short));/*XXX*/
		if (ddp_route(m, &forwro)) {
			DDP_STATINC(DDP_STAT_CANTFORWARD);
		} else {
			DDP_STATINC(DDP_STAT_FORWARD);
		}
		return;
	}
	from.sat_len = sizeof(struct sockaddr_at);
	from.sat_family = AF_APPLETALK;

	if (elh) {
		m_adj(m, sizeof(struct ddpshdr));
	} else {
		if (ddp_cksum && cksum && cksum != at_cksum(m, sizeof(int))) {
			DDP_STATINC(DDP_STAT_BADSUM);
			m_freem(m);
			return;
		}
		m_adj(m, sizeof(struct ddpehdr));
	}

	if ((ddp = ddp_search(&from, &to, aa)) == NULL) {
		m_freem(m);
		return;
	}
	if (sbappendaddr(&ddp->ddp_socket->so_rcv, (struct sockaddr *) & from,
			 m, (struct mbuf *) 0) == 0) {
		DDP_STATINC(DDP_STAT_NOSOCKSPACE);
		m_freem(m);
		return;
	}
#if IFA_STATS
	if (aa)
		aa->aa_ifa.ifa_data.ifad_inbytes += dlen;
#endif
	sorwakeup(ddp->ddp_socket);
}

#if 0

#define BPXLEN	48
#define BPALEN	16
#include <ctype.h>

static void
bprint(char *data, int len)
{
	char            xout[BPXLEN], aout[BPALEN];
	int             i = 0;

	memset(xout, 0, BPXLEN);
	memset(aout, 0, BPALEN);

	for (;;) {
		if (len < 1) {
			if (i != 0) {
				printf("%s\t%s\n", xout, aout);
			}
			printf("%s\n", "(end)");
			break;
		}
		xout[(i * 3)] = hexdigits[(*data & 0xf0) >> 4];
		xout[(i * 3) + 1] = hexdigits[*data & 0x0f];

		if ((u_char) * data < 0x7f && (u_char) * data > 0x20) {
			aout[i] = *data;
		} else {
			aout[i] = '.';
		}

		xout[(i * 3) + 2] = ' ';

		i++;
		len--;
		data++;

		if (i > BPALEN - 2) {
			printf("%s\t%s\n", xout, aout);
			memset(xout, 0, BPXLEN);
			memset(aout, 0, BPALEN);
			i = 0;
			continue;
		}
	}
}

static void
m_printm(struct mbuf *m)
{
	for (; m; m = m->m_next)
		bprint(mtod(m, char *), m->m_len);
}
#endif
