/*	$NetBSD: if_hippisubr.c,v 1.44 2015/08/24 22:21:26 pooka Exp $	*/

/*
 * Copyright (c) 1982, 1989, 1993
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
__KERNEL_RCSID(0, "$NetBSD: if_hippisubr.c,v 1.44 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <sys/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <net/if_hippi.h>

#include <netinet/in.h>
#if defined(INET) || defined(INET6)
#include <netinet/in_var.h>
#endif

#define senderr(e) { error = (e); goto bad;}

#ifndef llc_snap
#define	llc_snap	llc_un.type_snap
#endif

static int	hippi_output(struct ifnet *, struct mbuf *,
			     const struct sockaddr *, struct rtentry *);
static void	hippi_input(struct ifnet *, struct mbuf *);

/*
 * HIPPI output routine.
 * Encapsulate a packet of type family for the local net.
 * I don't know anything about the mapping of AppleTalk or OSI
 * protocols to HIPPI, so I don't include any code for them.
 */

static int
hippi_output(struct ifnet *ifp, struct mbuf *m0, const struct sockaddr *dst,
    struct rtentry *rt)
{
	uint16_t htype;
	uint32_t ifield = 0;
	int error = 0;
	struct mbuf *m = m0;
	struct hippi_header *hh;
	uint32_t *cci;
	uint32_t d2_len;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);

	/* HIPPI doesn't really do broadcast or multicast right now */
	if (m->m_flags & (M_BCAST | M_MCAST))
		senderr(EOPNOTSUPP);  /* XXX: some other error? */

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		if (rt) {
			const struct sockaddr_dl *sdl =
			    satocsdl(rt->rt_gateway);
			if (sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0)
				memcpy(&ifield, CLLADDR(sdl), sizeof(ifield));
		}
		if (!ifield)  /* XXX:  bogus check, but helps us get going */
			senderr(EHOSTUNREACH);
		htype = htons(ETHERTYPE_IP);
		break;
#endif

#ifdef INET6
	case AF_INET6:
		if (rt) {
			const struct sockaddr_dl *sdl =
			    satocsdl(rt->rt_gateway);
			if (sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0)
				memcpy(&ifield, CLLADDR(sdl), sizeof(ifield));
		}
		if (!ifield)  /* XXX:  bogus check, but helps us get going */
			senderr(EHOSTUNREACH);
		htype = htons(ETHERTYPE_IPV6);
		break;
#endif

	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
		       dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	if (htype != 0) {
		struct llc *l;
		M_PREPEND(m, sizeof (struct llc), M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		l = mtod(m, struct llc *);
		l->llc_control = LLC_UI;
		l->llc_dsap = l->llc_ssap = LLC_SNAP_LSAP;
		l->llc_snap.org_code[0] = l->llc_snap.org_code[1] =
			l->llc_snap.org_code[2] = 0;
		memcpy((void *) &l->llc_snap.ether_type, (void *) &htype,
		      sizeof(uint16_t));
	}

	d2_len = m->m_pkthdr.len;

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */

	M_PREPEND(m, sizeof (struct hippi_header) + 8, M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	cci = mtod(m, uint32_t *);
	memset(cci, 0, sizeof(struct hippi_header) + 8);
	cci[0] = 0;
	cci[1] = ifield;
	hh = (struct hippi_header *) &cci[2];
	hh->hi_fp.fp_ulp = HIPPI_ULP_802;
	hh->hi_fp.fp_flags = HIPPI_FP_D1_PRESENT;
	hh->hi_fp.fp_offsets = htons(sizeof(struct hippi_le));
	hh->hi_fp.fp_d2_len = htonl(d2_len);

	/* Pad out the D2 area to end on a quadword (64-bit) boundry. */

	if (d2_len % 8 != 0) {
		static uint32_t buffer[2] = {0, 0};
		m_copyback(m, m->m_pkthdr.len, 8 - d2_len % 8, (void *) buffer);
	}

	return ifq_enqueue(ifp, m ALTQ_COMMA ALTQ_DECL(&pktattr));

 bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received HIPPI packet;
 * the packet is in the mbuf chain m with
 * the HIPPI header.
 */

static void
hippi_input(struct ifnet *ifp, struct mbuf *m)
{
	pktqueue_t *pktq;
	struct llc *l;
	uint16_t htype;
	struct hippi_header *hh;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	/* XXX:  need to check flags and drop if bogus! */

	hh = mtod(m, struct hippi_header *);

	ifp->if_ibytes += m->m_pkthdr.len;
	if (hh->hi_le.le_dest_addr[0] & 1) {
		if (memcmp(etherbroadcastaddr, hh->hi_le.le_dest_addr,
		    sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	/* Skip past the HIPPI header. */
	m_adj(m, sizeof(struct hippi_header));

	l = mtod(m, struct llc *);
	if (l->llc_dsap != LLC_SNAP_LSAP) {
		m_freem(m);
		return;
	}
	htype = ntohs(l->llc_snap.ether_type);
	m_adj(m, 8);
	switch (htype) {
#ifdef INET
	case ETHERTYPE_IP:
		pktq = ip_pktq;
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		pktq = ip6_pktq;
		break;
#endif
	default:
		m_freem(m);
		return;
	}

	if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
		m_freem(m);
	}
}

/*
 * Handle packet from HIPPI that has no MAC header
 */

#ifdef INET
void
hippi_ip_input(struct ifnet *ifp, struct mbuf *m)
{
	int s;

	s = splnet();
	if (__predict_false(!pktq_enqueue(ip_pktq, m, 0))) {
		m_freem(m);
	}
	splx(s);
}
#endif

/*
 * Perform common duties while attaching to interface list
 */
void
hippi_ifattach(struct ifnet *ifp, void *lla)
{

	ifp->if_type = IFT_HIPPI;
	ifp->if_hdrlen = sizeof(struct hippi_header) + 8; /* add CCI */
	ifp->if_dlt = DLT_HIPPI;
	ifp->if_mtu = HIPPIMTU;
	ifp->if_output = hippi_output;
	ifp->if_input = hippi_input;
	ifp->if_baudrate = IF_Mbps(800);	/* XXX double-check */

	if_set_sadl(ifp, lla, 6, true);

	bpf_attach(ifp, DLT_HIPPI, sizeof(struct hippi_header));
}
