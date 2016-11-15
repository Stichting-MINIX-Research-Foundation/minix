/*	$NetBSD: ddp_output.c,v 1.17 2013/09/12 19:47:58 martin Exp $	 */

/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
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
__KERNEL_RCSID(0, "$NetBSD: ddp_output.c,v 1.17 2013/09/12 19:47:58 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#undef s_net

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

int ddp_cksum = 1;

int
ddp_output(struct mbuf *m,...)
{
	struct ddpcb   *ddp;
	struct ddpehdr *deh;
	va_list         ap;

	va_start(ap, m);
	ddp = va_arg(ap, struct ddpcb *);
	va_end(ap);

	M_PREPEND(m, sizeof(struct ddpehdr), M_DONTWAIT);
	if (!m)
		return (ENOBUFS);

	deh = mtod(m, struct ddpehdr *);
	deh->deh_pad = 0;
	deh->deh_hops = 0;

	deh->deh_len = m->m_pkthdr.len;

	deh->deh_dnet = ddp->ddp_fsat.sat_addr.s_net;
	deh->deh_dnode = ddp->ddp_fsat.sat_addr.s_node;
	deh->deh_dport = ddp->ddp_fsat.sat_port;
	deh->deh_snet = ddp->ddp_lsat.sat_addr.s_net;
	deh->deh_snode = ddp->ddp_lsat.sat_addr.s_node;
	deh->deh_sport = ddp->ddp_lsat.sat_port;

	/*
         * The checksum calculation is done after all of the other bytes have
         * been filled in.
         */
	if (ddp_cksum)
		deh->deh_sum = at_cksum(m, sizeof(int));
	else
		deh->deh_sum = 0;
	deh->deh_bytes = htonl(deh->deh_bytes);

	return ddp_route(m, &ddp->ddp_route);
}

u_short
at_cksum(struct mbuf *m, int skip)
{
	u_char         *data, *end;
	u_long          cksum = 0;

	for (; m; m = m->m_next) {
		for (data = mtod(m, u_char *), end = data + m->m_len;
		    data < end; data++) {
			if (skip) {
				skip--;
				continue;
			}
			cksum = (cksum + *data) << 1;
			if (cksum & 0x00010000)
				cksum++;
			cksum &= 0x0000ffff;
		}
	}

	if (cksum == 0) {
		cksum = 0x0000ffff;
	}
	return (u_short)cksum;
}

int
ddp_route(struct mbuf *m, struct route *ro)
{
	struct rtentry *rt;
	struct sockaddr_at gate;
	struct elaphdr *elh;
	struct at_ifaddr *aa = NULL;
	struct ifnet   *ifp = NULL;
	uint16_t        net;
	uint8_t         loopback = 0;

	if ((rt = rtcache_validate(ro)) != NULL && (ifp = rt->rt_ifp) != NULL) {
		const struct sockaddr_at *dst = satocsat(rtcache_getdst(ro));
		uint16_t dnet = dst->sat_addr.s_net;
		uint8_t dnode = dst->sat_addr.s_node;
		net = satosat(rt->rt_gateway)->sat_addr.s_net;

		TAILQ_FOREACH(aa, &at_ifaddr, aa_list) {
			if (ntohs(net) >= ntohs(aa->aa_firstnet) &&
			    ntohs(net) <= ntohs(aa->aa_lastnet)) {
				/* Are we talking to ourselves? */
				if (dnet == aa->aa_addr.sat_addr.s_net &&
				    dnode == aa->aa_addr.sat_addr.s_node) {
					/* If to us, redirect to lo0. */
					ifp = lo0ifp;
				}
				/* Or is it a broadcast? */
				else if (dnet == aa->aa_addr.sat_addr.s_net &&
					dnode == 255) {
					/* If broadcast, loop back a copy. */
					loopback = 1;
				}
				break;
			}
		}
	}
	if (aa == NULL) {
#ifdef NETATALKDEBUG
		printf("%s: no address found\n", __func__);
#endif
		m_freem(m);
		return EINVAL;
	}
	/*
         * There are several places in the kernel where data is added to
         * an mbuf without ensuring that the mbuf pointer is aligned.
         * This is bad for transition routing, since phase 1 and phase 2
         * packets end up poorly aligned due to the three byte elap header.
         */
	if (!(aa->aa_flags & AFA_PHASE2)) {
		M_PREPEND(m, SZ_ELAPHDR, M_DONTWAIT);
		if (m == NULL)
			return ENOBUFS;

		elh = mtod(m, struct elaphdr *);
		elh->el_snode = satosat(&aa->aa_addr)->sat_addr.s_node;
		elh->el_type = ELAP_DDPEXTEND;
		if (ntohs(satocsat(rtcache_getdst(ro))->sat_addr.s_net) >=
		    ntohs(aa->aa_firstnet) &&
		    ntohs(satocsat(rtcache_getdst(ro))->sat_addr.s_net) <=
		    ntohs(aa->aa_lastnet)) {
			elh->el_dnode =
			    satocsat(rtcache_getdst(ro))->sat_addr.s_node;
		} else {
			elh->el_dnode =
			    satosat(rt->rt_gateway)->sat_addr.s_node;
		}
	}
	if (ntohs(satocsat(rtcache_getdst(ro))->sat_addr.s_net) >=
	    ntohs(aa->aa_firstnet) &&
	    ntohs(satocsat(rtcache_getdst(ro))->sat_addr.s_net) <=
	    ntohs(aa->aa_lastnet)) {
		gate = *satocsat(rtcache_getdst(ro));
	} else {
		gate = *satosat(rt->rt_gateway);
	}
	rt->rt_use++;

#if IFA_STATS
	aa->aa_ifa.ifa_data.ifad_outbytes += m->m_pkthdr.len;
#endif

	/* XXX */
	if (loopback && rtcache_getdst(ro)->sa_family == AF_APPLETALK) {
		struct mbuf *copym = m_copypacket(m, M_DONTWAIT);
		
#ifdef NETATALKDEBUG
		printf("Looping back (not AARP).\n");
#endif
		looutput(lo0ifp, copym, rtcache_getdst(ro), NULL);
	}
	return (*ifp->if_output)(ifp, m, (struct sockaddr *)&gate, NULL);
}
