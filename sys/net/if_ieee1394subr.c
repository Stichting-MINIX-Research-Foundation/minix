/*	$NetBSD: if_ieee1394subr.c,v 1.50 2015/08/24 22:21:26 pooka Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ieee1394subr.c,v 1.50 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/select.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ieee1394.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/ethertypes.h>
#include <net/netisr.h>
#include <net/route.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_inarp.h>
#endif /* INET */
#ifdef INET6
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif /* INET6 */

#include <dev/ieee1394/firewire.h>

#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/iec13213.h>
#include <dev/ieee1394/if_fwipvar.h>

#define	IEEE1394_REASS_TIMEOUT	3	/* 3 sec */

#define	senderr(e)	do { error = (e); goto bad; } while(0/*CONSTCOND*/)

static int  ieee1394_output(struct ifnet *, struct mbuf *,
		const struct sockaddr *, struct rtentry *);
static struct mbuf *ieee1394_reass(struct ifnet *, struct mbuf *, uint16_t);

static int
ieee1394_output(struct ifnet *ifp, struct mbuf *m0, const struct sockaddr *dst,
    struct rtentry *rt)
{
	uint16_t etype = 0;
	struct mbuf *m;
	int s, hdrlen, error = 0;
	struct mbuf *mcopy = NULL;
	struct ieee1394_hwaddr *hwdst, baddr;
	const struct ieee1394_hwaddr *myaddr;
	ALTQ_DECL(struct altq_pktattr pktattr;)
#ifdef INET
	struct arphdr *ah;
#endif /* INET */
	struct m_tag *mtag;
	int unicast;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m0, dst->sa_family, &pktattr);

	/*
	 * For unicast, we make a tag to store the lladdr of the
	 * destination. This might not be the first time we have seen
	 * the packet (for instance, the arp code might be trying to
	 * re-send it after receiving an arp reply) so we only
	 * allocate a tag if there isn't one there already. For
	 * multicast, we will eventually use a different tag to store
	 * the channel number.
	 */
	unicast = !(m0->m_flags & (M_BCAST | M_MCAST));
	if (unicast) {
		mtag =
		    m_tag_find(m0, MTAG_FIREWIRE_HWADDR, NULL);
		if (!mtag) {
			mtag = m_tag_get(MTAG_FIREWIRE_HWADDR,
			    sizeof (struct ieee1394_hwaddr), M_NOWAIT);
			if (!mtag) {
				error = ENOMEM;
				goto bad;
			}
			m_tag_prepend(m0, mtag);
		}
		hwdst = (struct ieee1394_hwaddr *)(mtag + 1);
	} else {
		hwdst = &baddr;
	}

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		if (unicast && (!arpresolve(ifp, rt, m0, dst, (u_char *)hwdst)))
			return 0;	/* if not yet resolved */
		/* if broadcasting on a simplex interface, loopback a copy */
		if ((m0->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m0, 0, M_COPYALL);
		etype = htons(ETHERTYPE_IP);
		break;
	case AF_ARP:
		ah = mtod(m0, struct arphdr *);
		ah->ar_hrd = htons(ARPHRD_IEEE1394);
		etype = htons(ETHERTYPE_ARP);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (unicast && (!nd6_storelladdr(ifp, rt, m0, dst,
		    hwdst->iha_uid, IEEE1394_ADDR_LEN))) {
			/* something bad happened */
			return 0;
		}
		etype = htons(ETHERTYPE_IPV6);
		break;
#endif /* INET6 */

	case pseudo_AF_HDRCMPLT:
	case AF_UNSPEC:
		/* TODO? */
	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
		    dst->sa_family);
		senderr(EAFNOSUPPORT);
		break;
	}

	if (mcopy)
		looutput(ifp, mcopy, dst, rt);
	myaddr = (const struct ieee1394_hwaddr *)CLLADDR(ifp->if_sadl);
	if (ifp->if_bpf) {
		struct ieee1394_bpfhdr h;
		if (unicast)
			memcpy(h.ibh_dhost, hwdst->iha_uid, 8);
		else
			memcpy(h.ibh_dhost,
			    ((const struct ieee1394_hwaddr *)
			    ifp->if_broadcastaddr)->iha_uid, 8);
		memcpy(h.ibh_shost, myaddr->iha_uid, 8);
		h.ibh_type = etype;
		bpf_mtap2(ifp->if_bpf, &h, sizeof(h), m0);
	}
	if ((ifp->if_flags & IFF_SIMPLEX) &&
	    unicast &&
	    memcmp(hwdst, myaddr, IEEE1394_ADDR_LEN) == 0)
		return looutput(ifp, m0, dst, rt);

	/*
	 * XXX:
	 * The maximum possible rate depends on the topology.
	 * So the determination of maxrec and fragmentation should be
	 * called from the driver after probing the topology map.
	 */
	if (unicast) {
		hdrlen = IEEE1394_GASP_LEN;
		hwdst->iha_speed = 0;	/* XXX */
	} else
		hdrlen = 0;

	if (hwdst->iha_speed > myaddr->iha_speed)
		hwdst->iha_speed = myaddr->iha_speed;
	if (hwdst->iha_maxrec > myaddr->iha_maxrec)
		hwdst->iha_maxrec = myaddr->iha_maxrec;
	if (hwdst->iha_maxrec > (8 + hwdst->iha_speed))
		hwdst->iha_maxrec = 8 + hwdst->iha_speed;
	if (hwdst->iha_maxrec < 8)
			hwdst->iha_maxrec = 8;

	m0 = ieee1394_fragment(ifp, m0, (2<<hwdst->iha_maxrec) - hdrlen, etype);
	if (m0 == NULL)
		senderr(ENOBUFS);

	s = splnet();
	ifp->if_obytes += m0->m_pkthdr.len;
	if (m0->m_flags & M_MCAST)
		ifp->if_omcasts++;
	while ((m = m0) != NULL) {
		m0 = m->m_nextpkt;
		if (m == NULL) {
			splx(s);
			senderr(ENOBUFS);
		}
		IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, error);
		if (error) {
			/* mbuf is already freed */
			splx(s);
			goto bad;
		}
	}
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	return 0;

  bad:
	while (m0 != NULL) {
		m = m0->m_nextpkt;
		m_freem(m0);
		m0 = m;
	}

	return error;
}

struct mbuf *
ieee1394_fragment(struct ifnet *ifp, struct mbuf *m0, int maxsize,
    uint16_t etype)
{
	struct ieee1394com *ic = (struct ieee1394com *)ifp;
	int totlen, fraglen, off;
	struct mbuf *m, **mp;
	struct ieee1394_fraghdr *ifh;
	struct ieee1394_unfraghdr *iuh;

	totlen = m0->m_pkthdr.len;
	if (totlen + sizeof(struct ieee1394_unfraghdr) <= maxsize) {
		M_PREPEND(m0, sizeof(struct ieee1394_unfraghdr), M_DONTWAIT);
		if (m0 == NULL)
			goto bad;
		iuh = mtod(m0, struct ieee1394_unfraghdr *);
		iuh->iuh_ft = 0;
		iuh->iuh_etype = etype;
		return m0;
	}

	fraglen = maxsize - sizeof(struct ieee1394_fraghdr);

	M_PREPEND(m0, sizeof(struct ieee1394_fraghdr), M_DONTWAIT);
	if (m0 == NULL)
		goto bad;
	ifh = mtod(m0, struct ieee1394_fraghdr *);
	ifh->ifh_ft_size = htons(IEEE1394_FT_MORE | (totlen - 1));
	ifh->ifh_etype_off = etype;
	ifh->ifh_dgl = htons(ic->ic_dgl);
	ifh->ifh_reserved = 0;
	off = fraglen;
	mp = &m0->m_nextpkt;
	while (off < totlen) {
		if (off + fraglen > totlen)
			fraglen = totlen - off;
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			goto bad;
		m->m_flags |= m0->m_flags & (M_BCAST|M_MCAST);	/* copy bcast */
		MH_ALIGN(m, sizeof(struct ieee1394_fraghdr));
		m->m_len = sizeof(struct ieee1394_fraghdr);
		ifh = mtod(m, struct ieee1394_fraghdr *);
		ifh->ifh_ft_size =
		    htons(IEEE1394_FT_SUBSEQ | IEEE1394_FT_MORE | (totlen - 1));
		ifh->ifh_etype_off = htons(off);
		ifh->ifh_dgl = htons(ic->ic_dgl);
		ifh->ifh_reserved = 0;
		m->m_next = m_copy(m0, sizeof(*ifh) + off, fraglen);
		if (m->m_next == NULL)
			goto bad;
		m->m_pkthdr.len = sizeof(*ifh) + fraglen;
		off += fraglen;
		*mp = m;
		mp = &m->m_nextpkt;
	}
	ifh->ifh_ft_size &= ~htons(IEEE1394_FT_MORE);	/* last fragment */
	m_adj(m0, -(m0->m_pkthdr.len - maxsize));

	ic->ic_dgl++;
	return m0;

  bad:
	while ((m = m0) != NULL) {
		m0 = m->m_nextpkt;
		m->m_nextpkt = NULL;
		m_freem(m);
	}
	return NULL;
}

void
ieee1394_input(struct ifnet *ifp, struct mbuf *m, uint16_t src)
{
	pktqueue_t *pktq = NULL;
	struct ifqueue *inq;
	uint16_t etype;
	int s;
	struct ieee1394_unfraghdr *iuh;
	int isr = 0;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	if (m->m_len < sizeof(*iuh)) {
		if ((m = m_pullup(m, sizeof(*iuh))) == NULL)
			return;
	}

	iuh = mtod(m, struct ieee1394_unfraghdr *);

	if (ntohs(iuh->iuh_ft) & (IEEE1394_FT_SUBSEQ | IEEE1394_FT_MORE)) {
		if ((m = ieee1394_reass(ifp, m, src)) == NULL)
			return;
		iuh = mtod(m, struct ieee1394_unfraghdr *);
	}
	etype = ntohs(iuh->iuh_etype);

	/* strip off the ieee1394 header */
	m_adj(m, sizeof(*iuh));
	if (ifp->if_bpf) {
		struct ieee1394_bpfhdr h;
		struct m_tag *mtag;
		const struct ieee1394_hwaddr *myaddr;

		mtag = m_tag_find(m, MTAG_FIREWIRE_SENDER_EUID, 0);
		if (mtag)
			memcpy(h.ibh_shost, mtag + 1, 8);
		else
			memset(h.ibh_shost, 0, 8);
		if (m->m_flags & M_BCAST)
			memcpy(h.ibh_dhost,
			    ((const struct ieee1394_hwaddr *)
			    ifp->if_broadcastaddr)->iha_uid, 8);
		else {
			myaddr =
			  (const struct ieee1394_hwaddr *)CLLADDR(ifp->if_sadl);
			memcpy(h.ibh_dhost, myaddr->iha_uid, 8);
		}
		h.ibh_type = htons(etype);
		bpf_mtap2(ifp->if_bpf, &h, sizeof(h), m);
	}

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		pktq = ip_pktq;
		break;

	case ETHERTYPE_ARP:
		isr = NETISR_ARP;
		inq = &arpintrq;
		break;
#endif /* INET */

#ifdef INET6
	case ETHERTYPE_IPV6:
		pktq = ip6_pktq;
		break;
#endif /* INET6 */

	default:
		m_freem(m);
		return;
	}

	if (__predict_true(pktq)) {
		if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
			m_freem(m);
		}
		return;
	}

	s = splnet();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else {
		IF_ENQUEUE(inq, m);
		schednetisr(isr);
	}
	splx(s);
}

static struct mbuf *
ieee1394_reass(struct ifnet *ifp, struct mbuf *m0, uint16_t src)
{
	struct ieee1394com *ic = (struct ieee1394com *)ifp;
	struct ieee1394_fraghdr *ifh;
	struct ieee1394_unfraghdr *iuh;
	struct ieee1394_reassq *rq;
	struct ieee1394_reass_pkt *rp, *trp, *nrp = NULL;
	int len;
	uint16_t etype, off, ftype, size, dgl;
	uint32_t id;

	if (m0->m_len < sizeof(*ifh)) {
		if ((m0 = m_pullup(m0, sizeof(*ifh))) == NULL)
			return NULL;
	}
	ifh = mtod(m0, struct ieee1394_fraghdr *);
	m_adj(m0, sizeof(*ifh));
	size = ntohs(ifh->ifh_ft_size);
	ftype = size & (IEEE1394_FT_SUBSEQ | IEEE1394_FT_MORE);
	size = (size & ~ftype) + 1;
	dgl = ntohs(ifh->ifh_dgl);
	len = m0->m_pkthdr.len;
	id = dgl | (src << 16);
	if (ftype & IEEE1394_FT_SUBSEQ) {
		m_tag_delete_chain(m0, NULL);
		m0->m_flags &= ~M_PKTHDR;
		etype = 0;
		off = ntohs(ifh->ifh_etype_off);
	} else {
		etype = ifh->ifh_etype_off;
		off = 0;
	}

	for (rq = LIST_FIRST(&ic->ic_reassq); ; rq = LIST_NEXT(rq, rq_node)) {
		if (rq == NULL) {
			/*
			 * Create a new reassemble queue head for the node.
			 */
			rq = malloc(sizeof(*rq), M_FTABLE, M_NOWAIT);
			if (rq == NULL) {
				m_freem(m0);
				return NULL;
			}
			rq->fr_id = id;
			LIST_INIT(&rq->rq_pkt);
			LIST_INSERT_HEAD(&ic->ic_reassq, rq, rq_node);
			break;
		}
		if (rq->fr_id == id)
			break;
	}
	for (rp = LIST_FIRST(&rq->rq_pkt); rp != NULL; rp = nrp) {
		nrp = LIST_NEXT(rp, rp_next);
		if (rp->rp_dgl != dgl)
			continue;
		/*
		 * sanity check:
		 * datagram size must be same for all fragments, and
		 * no overlap is allowed.
		 */
		if (rp->rp_size != size ||
		    (off < rp->rp_off + rp->rp_len && off + len > rp->rp_off)) {
			/*
			 * This happens probably due to wrapping dgl value.
			 * Destroy all previously received fragment and
			 * enqueue current fragment.
			 */
			for (rp = LIST_FIRST(&rq->rq_pkt); rp != NULL;
			    rp = nrp) {
				nrp = LIST_NEXT(rp, rp_next);
				if (rp->rp_dgl == dgl) {
					LIST_REMOVE(rp, rp_next);
					m_freem(rp->rp_m);
					free(rp, M_FTABLE);
				}
			}
			break;
		}
		if (rp->rp_off + rp->rp_len == off) {
			/*
			 * All the subsequent fragments received in sequence
			 * come here.
			 * Concatinate mbuf to previous one instead of
			 * allocating new reassemble queue structure,
			 * and try to merge more with the subsequent fragment
			 * in the queue.
			 */
			m_cat(rp->rp_m, m0);
			rp->rp_len += len;
			while (rp->rp_off + rp->rp_len < size &&
			    nrp != NULL && nrp->rp_dgl == dgl &&
			    nrp->rp_off == rp->rp_off + rp->rp_len) {
				LIST_REMOVE(nrp, rp_next);
				m_cat(rp->rp_m, nrp->rp_m);
				rp->rp_len += nrp->rp_len;
				free(nrp, M_FTABLE);
				nrp = LIST_NEXT(rp, rp_next);
			}
			m0 = NULL;	/* mark merged */
			break;
		}
		if (off + m0->m_pkthdr.len == rp->rp_off) {
			m_cat(m0, rp->rp_m);
			rp->rp_m = m0;
			rp->rp_off = off;
			rp->rp_etype = etype;	 /* over writing trust etype */
			rp->rp_len += len;
			m0 = NULL;	/* mark merged */
			break;
		}
		if (rp->rp_off > off) {
			/* insert before rp */
			nrp = rp;
			break;
		}
		if (nrp == NULL || nrp->rp_dgl != dgl) {
			/* insert after rp */
			nrp = NULL;
			break;
		}
	}
	if (m0 == NULL) {
		if (rp->rp_off != 0 || rp->rp_len != size)
			return NULL;
		/* fragment done */
		LIST_REMOVE(rp, rp_next);
		m0 = rp->rp_m;
		m0->m_pkthdr.len = rp->rp_len;
		M_PREPEND(m0, sizeof(*iuh), M_DONTWAIT);
		if (m0 != NULL) {
			iuh = mtod(m0, struct ieee1394_unfraghdr *);
			iuh->iuh_ft = 0;
			iuh->iuh_etype = rp->rp_etype;
		}
		free(rp, M_FTABLE);
		return m0;
	}

	/*
	 * New fragment received.  Allocate reassemble queue structure.
	 */
	trp = malloc(sizeof(*trp), M_FTABLE, M_NOWAIT);
	if (trp == NULL) {
		m_freem(m0);
		return NULL;
	}
	trp->rp_m = m0;
	trp->rp_size = size;
	trp->rp_etype = etype;		 /* valid only if off==0 */
	trp->rp_off = off;
	trp->rp_dgl = dgl;
	trp->rp_len = len;
	trp->rp_ttl = IEEE1394_REASS_TIMEOUT;
	if (trp->rp_ttl <= ifp->if_timer)
		trp->rp_ttl = ifp->if_timer + 1;

	if (rp == NULL) {
		/* first fragment for the dgl */
		LIST_INSERT_HEAD(&rq->rq_pkt, trp, rp_next);
	} else if (nrp == NULL) {
		/* no next fragment for the dgl */
		LIST_INSERT_AFTER(rp, trp, rp_next);
	} else {
		/* there is a hole */
		LIST_INSERT_BEFORE(nrp, trp, rp_next);
	}
	return NULL;
}

void
ieee1394_drain(struct ifnet *ifp)
{
	struct ieee1394com *ic = (struct ieee1394com *)ifp;
	struct ieee1394_reassq *rq;
	struct ieee1394_reass_pkt *rp;

	while ((rq = LIST_FIRST(&ic->ic_reassq)) != NULL) {
		LIST_REMOVE(rq, rq_node);
		while ((rp = LIST_FIRST(&rq->rq_pkt)) != NULL) {
			LIST_REMOVE(rp, rp_next);
			m_freem(rp->rp_m);
			free(rp, M_FTABLE);
		}
		free(rq, M_FTABLE);
	}
}

void
ieee1394_watchdog(struct ifnet *ifp)
{
	struct ieee1394com *ic = (struct ieee1394com *)ifp;
	struct ieee1394_reassq *rq;
	struct ieee1394_reass_pkt *rp, *nrp;
	int dec;

	dec = (ifp->if_timer > 0) ? ifp->if_timer : 1;
	for (rq = LIST_FIRST(&ic->ic_reassq); rq != NULL;
	    rq = LIST_NEXT(rq, rq_node)) {
		for (rp = LIST_FIRST(&rq->rq_pkt); rp != NULL; rp = nrp) {
			nrp = LIST_NEXT(rp, rp_next);
			if (rp->rp_ttl >= dec)
				rp->rp_ttl -= dec;
			else {
				LIST_REMOVE(rp, rp_next);
				m_freem(rp->rp_m);
				free(rp, M_FTABLE);
			}
		}
	}
}

const char *
ieee1394_sprintf(const uint8_t *laddr)
{
	static char buf[3*8];

	snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
	    laddr[0], laddr[1], laddr[2], laddr[3],
	    laddr[4], laddr[5], laddr[6], laddr[7]);
	return buf;
}

void
ieee1394_ifattach(struct ifnet *ifp, const struct ieee1394_hwaddr *hwaddr)
{
	struct ieee1394_hwaddr *baddr;
	struct ieee1394com *ic = (struct ieee1394com *)ifp;

	ifp->if_type = IFT_IEEE1394;
	ifp->if_hdrlen = sizeof(struct ieee1394_header);
	ifp->if_dlt = DLT_EN10MB;	/* XXX */
	ifp->if_mtu = IEEE1394MTU;
	ifp->if_output = ieee1394_output;
	ifp->if_drain = ieee1394_drain;
	ifp->if_watchdog = ieee1394_watchdog;
	ifp->if_timer = 1;
	if (ifp->if_baudrate == 0)
		ifp->if_baudrate = IF_Mbps(100);

	if_set_sadl(ifp, hwaddr, sizeof(struct ieee1394_hwaddr), true);

	baddr = malloc(ifp->if_addrlen, M_DEVBUF, M_WAITOK);
	memset(baddr->iha_uid, 0xff, IEEE1394_ADDR_LEN);
	baddr->iha_speed = 0;	/*XXX: how to determine the speed for bcast? */
	baddr->iha_maxrec = 512 << baddr->iha_speed;
	memset(baddr->iha_offset, 0, sizeof(baddr->iha_offset));
	ifp->if_broadcastaddr = (uint8_t *)baddr;
	LIST_INIT(&ic->ic_reassq);
	bpf_attach(ifp, DLT_APPLE_IP_OVER_IEEE1394,
	    sizeof(struct ieee1394_hwaddr));
}

void
ieee1394_ifdetach(struct ifnet *ifp)
{
	ieee1394_drain(ifp);
	bpf_detach(ifp);
	free(__UNCONST(ifp->if_broadcastaddr), M_DEVBUF);
	ifp->if_broadcastaddr = NULL;
}

int
ieee1394_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int error = 0;

	switch (cmd) {
	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			if ((error = (*ifp->if_init)(ifp)) != 0)
				break;
			arp_ifinit(ifp, ifa);
			break;
#endif /* INET */
		default:
			error = (*ifp->if_init)(ifp);
			break;
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > IEEE1394MTU)
			error = EINVAL;
		else if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}

	return error;
}
