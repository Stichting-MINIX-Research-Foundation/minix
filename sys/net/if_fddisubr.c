/*	$NetBSD: if_fddisubr.c,v 1.94 2015/09/30 06:25:59 ozaki-r Exp $	*/

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
 *
 *	@(#)if_fddisubr.c	8.1 (Berkeley) 6/10/93
 *
 * Id: if_fddisubr.c,v 1.15 1997/03/21 22:35:50 thomas Exp
 */

/*
 * Copyright (c) 1995, 1996
 *	Matt Thomas <matt@3am-software.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of its contributor may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *	@(#)if_fddisubr.c	8.1 (Berkeley) 6/10/93
 *
 * Id: if_fddisubr.c,v 1.15 1997/03/21 22:35:50 thomas Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_fddisubr.c,v 1.94 2015/09/30 06:25:59 ozaki-r Exp $");

#ifdef _KERNEL_OPT
#include "opt_gateway.h"
#include "opt_inet.h"
#include "opt_atalk.h"
#include "opt_mbuftrace.h"
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

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_inarp.h>
#endif
#include <net/if_fddi.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif
#include <netinet6/nd6.h>
#endif


#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

#define llc_snap_org_code llc_un.type_snap.org_code
#define llc_snap_ether_type llc_un.type_snap.ether_type

extern u_char	at_org_code[ 3 ];
extern u_char	aarp_org_code[ 3 ];
#endif /* NETATALK */



#define senderr(e) { error = (e); goto bad;}

/*
 * This really should be defined in if_llc.h but in case it isn't.
 */
#ifndef llc_snap
#define	llc_snap	llc_un.type_snap
#endif

#define	FDDIADDR(ifp)		LLADDR((ifp)->if_sadl)
#define	CFDDIADDR(ifp)		CLLADDR((ifp)->if_sadl)

static	int fddi_output(struct ifnet *, struct mbuf *,
	    const struct sockaddr *, struct rtentry *);
static	void fddi_input(struct ifnet *, struct mbuf *);

/*
 * FDDI output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to ethercom structure.
 */
static int
fddi_output(struct ifnet *ifp0, struct mbuf *m0, const struct sockaddr *dst,
    struct rtentry *rt)
{
	uint16_t etype;
	int error = 0, hdrcmplt = 0;
	uint8_t esrc[6], edst[6];
	struct mbuf *m = m0;
	struct fddi_header *fh;
	struct mbuf *mcopy = NULL;
	struct ifnet *ifp = ifp0;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	MCLAIM(m, ifp->if_mowner);

#if NCARP > 0
	if (ifp->if_type == IFT_CARP) {
		struct ifaddr *ifa;

		/* loop back if this is going to the carp interface */
		if (dst != NULL && ifp0->if_link_state == LINK_STATE_UP &&
		    (ifa = ifa_ifwithaddr(dst)) != NULL &&
		    ifa->ifa_ifp == ifp0)
			return (looutput(ifp0, m, dst, rt));

		ifp = ifp->if_carpdev;
		/* ac = (struct arpcom *)ifp; */

		if ((ifp0->if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			senderr(ENETDOWN);
	}
#endif /* NCARP > 0 */
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	switch (dst->sa_family) {

#ifdef INET
	case AF_INET: {
		if (m->m_flags & M_BCAST)
                	memcpy(edst, fddibroadcastaddr, sizeof(edst));
		else if (m->m_flags & M_MCAST) {
			ETHER_MAP_IP_MULTICAST(&satocsin(dst)->sin_addr,
			    (char *)edst);
		} else if (!arpresolve(ifp, rt, m, dst, edst))
			return (0);	/* if not yet resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		etype = htons(ETHERTYPE_IP);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
		if (!nd6_storelladdr(ifp, rt, m, dst, edst, sizeof(edst))){
			/* something bad happened */
			return (0);
		}
		etype = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef AF_ARP
	case AF_ARP: {
		struct arphdr *ah = mtod(m, struct arphdr *);
		if (m->m_flags & M_BCAST)
                	memcpy(edst, etherbroadcastaddr, sizeof(edst));
		else {
			void *tha = ar_tha(ah);
			if (tha == NULL)
				return 0;
			memcpy(edst, tha, sizeof(edst));
		}

		ah->ar_hrd = htons(ARPHRD_ETHER);

		switch (ntohs(ah->ar_op)) {
		case ARPOP_REVREQUEST:
		case ARPOP_REVREPLY:
			etype = htons(ETHERTYPE_REVARP);
			break;

		case ARPOP_REQUEST:
		case ARPOP_REPLY:
		default:
			etype = htons(ETHERTYPE_ARP);
		}

		break;
	}
#endif /* AF_ARP */
#ifdef NETATALK
	case AF_APPLETALK: {
		struct at_ifaddr *aa;
		if (!aarpresolve(ifp, m, (const struct sockaddr_at *)dst, edst)) {
#ifdef NETATALKDEBUG
			printf("aarpresolv: failed\n");
#endif
			return (0);
		}
		/*
		 * ifaddr is the first thing in at_ifaddr
		 */
		if ((aa = (struct at_ifaddr *)at_ifawithnet(
		    (const struct sockaddr_at *)dst, ifp)) == NULL)
			goto bad;

		/*
		 * In the phase 2 case, we need to prepend an mbuf for the llc
		 * header. Since we must preserve the value of m, which is
		 * passed to us by value, we m_copy() the first mbuf, and use
		 * it for our llc header.
		 */
		if (aa->aa_flags & AFA_PHASE2) {
			struct llc llc;

			M_PREPEND(m, sizeof(struct llc), M_NOWAIT);
			if (m == 0)
				senderr(ENOBUFS);
			llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
			llc.llc_control = LLC_UI;
			memcpy(llc.llc_snap_org_code, at_org_code,
			    sizeof(at_org_code));
			llc.llc_snap_ether_type = htons(ETHERTYPE_ATALK);
			memcpy(mtod(m, void *), &llc, sizeof(struct llc));
			etype = 0;
		} else {
			etype = htons(ETHERTYPE_ATALK);
		}
		break;
	}
#endif /* NETATALK */

	case pseudo_AF_HDRCMPLT:
	{
		const struct fddi_header *fh1 =
		    (const struct fddi_header *)dst->sa_data;
		hdrcmplt = 1;
		memcpy(esrc, fh1->fddi_shost, sizeof (esrc));
		/*FALLTHROUGH*/
	}

	case AF_LINK:
	{
		const struct fddi_header *fh1 =
		    (const struct fddi_header *)dst->sa_data;
 		memcpy(edst, fh1->fddi_dhost, sizeof (edst));
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		etype = 0;
		break;
	}

	case AF_UNSPEC:
	{
		const struct ether_header *eh;
		eh = (const struct ether_header *)dst->sa_data;
 		memcpy(edst, eh->ether_dhost, sizeof(edst));
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		etype = eh->ether_type;
		break;
	}

	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
		       dst->sa_family);
		senderr(EAFNOSUPPORT);
	}


	if (mcopy)
		(void) looutput(ifp, mcopy, dst, rt);
	if (etype != 0) {
		struct llc *l;
		M_PREPEND(m, sizeof (struct llc), M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		l = mtod(m, struct llc *);
		l->llc_control = LLC_UI;
		l->llc_dsap = l->llc_ssap = LLC_SNAP_LSAP;
		l->llc_snap.org_code[0] = l->llc_snap.org_code[1] = l->llc_snap.org_code[2] = 0;
		memcpy(&l->llc_snap.ether_type, &etype, sizeof(uint16_t));
	}
	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, sizeof (struct fddi_header), M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	fh = mtod(m, struct fddi_header *);
	fh->fddi_fc = FDDIFC_LLC_ASYNC|FDDIFC_LLC_PRIO4;
	memcpy(fh->fddi_dhost, edst, sizeof (edst));
	if (hdrcmplt)
		memcpy(fh->fddi_shost, esrc, sizeof(fh->fddi_shost));
	else
		memcpy(fh->fddi_shost, CFDDIADDR(ifp), sizeof(fh->fddi_shost));

#if NCARP > 0
	if (ifp0 != ifp && ifp0->if_type == IFT_CARP) {
		if_set_sadl(ifp0, fh->fddi_shost, sizeof(fh->fddi_shost),
		    false);
	}

	if (ifp != ifp0)
		ifp0->if_obytes += m->m_pkthdr.len;
#endif /* NCARP > 0 */
	return ifq_enqueue(ifp, m ALTQ_COMMA ALTQ_DECL(&pktattr));

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received FDDI packet;
 * the packet is in the mbuf chain m with
 * the fddi header.
 */
static void
fddi_input(struct ifnet *ifp, struct mbuf *m)
{
#if defined(INET) || defined(INET6)
	pktqueue_t *pktq = NULL;
#endif
#if defined(NETATALK)
	struct ifqueue *inq = NULL;
	int isr = 0;
	int s;
#endif

	struct llc *l;
	struct fddi_header *fh;

	MCLAIM(m, &((struct ethercom *)ifp)->ec_rx_mowner);
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	fh = mtod(m, struct fddi_header *);

	ifp->if_ibytes += m->m_pkthdr.len;
	if (fh->fddi_dhost[0] & 1) {
		if (memcmp(fddibroadcastaddr, fh->fddi_dhost,
		    sizeof(fddibroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
		ifp->if_imcasts++;
	} else if ((ifp->if_flags & IFF_PROMISC)
	    && memcmp(CFDDIADDR(ifp), (void *)fh->fddi_dhost,
		    sizeof(fh->fddi_dhost)) != 0) {
		m_freem(m);
		return;
	}

#ifdef M_LINK0
	/*
	 * If this has a LLC priority of 0, then mark it so upper
	 * layers have a hint that it really came via a FDDI/Ethernet
	 * bridge.
	 */
	if ((fh->fddi_fc & FDDIFC_LLC_PRIO7) == FDDIFC_LLC_PRIO0)
		m->m_flags |= M_LINK0;
#endif

	l = (struct llc *)(fh+1);
	switch (l->llc_dsap) {
#if defined(INET) || defined(INET6) || defined(NETATALK)
	case LLC_SNAP_LSAP:
	{
		uint16_t etype;
		if (l->llc_control != LLC_UI || l->llc_ssap != LLC_SNAP_LSAP)
			goto dropanyway;

		/* Strip off the FDDI header. */
		m_adj(m, sizeof(struct fddi_header));

#ifdef NETATALK
		if (memcmp(&(l->llc_snap_org_code)[0], at_org_code,
			 sizeof(at_org_code)) == 0 &&
		 	ntohs(l->llc_snap_ether_type) == ETHERTYPE_ATALK) {
		    inq = &atintrq2;
		    m_adj( m, sizeof( struct llc ));
		    isr = NETISR_ATALK;
		    break;
		}

		if (memcmp(&(l->llc_snap_org_code)[0], aarp_org_code,
			 sizeof(aarp_org_code)) == 0 &&
			ntohs(l->llc_snap_ether_type) == ETHERTYPE_AARP) {
		    m_adj( m, sizeof( struct llc ));
		    aarpinput(ifp, m); /* XXX */
		    return;
		}
#endif /* NETATALK */
		if (l->llc_snap.org_code[0] != 0 || l->llc_snap.org_code[1] != 0|| l->llc_snap.org_code[2] != 0)
			goto dropanyway;
		etype = ntohs(l->llc_snap.ether_type);
		m_adj(m, 8);
#if NCARP > 0
		if (ifp->if_carp && ifp->if_type != IFT_CARP &&
		    (carp_input(m, (uint8_t *)&fh->fddi_shost,
		    (uint8_t *)&fh->fddi_dhost, l->llc_snap.ether_type) == 0))
			return;
#endif

		switch (etype) {
#ifdef INET
		case ETHERTYPE_IP:
#ifdef GATEWAY
			if (ipflow_fastforward(m))
				return;
#endif
			pktq = ip_pktq;
			break;

		case ETHERTYPE_ARP:
#if !defined(__bsdi__) || _BSDI_VERSION >= 199401
#if defined(NETATALK)
			isr = NETISR_ARP;
			inq = &arpintrq;
#endif
			break;
#else
			arpinput(ifp, m);
			return;
#endif
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
#ifdef GATEWAY  
			if (ip6flow_fastforward(&m))
				return;
#endif
			pktq = ip6_pktq;
			break;

#endif
#ifdef NETATALK
		case ETHERTYPE_ATALK:
	                isr = NETISR_ATALK;
			inq = &atintrq1;
			break;
	        case ETHERTYPE_AARP:
			/* probably this should be done with a NETISR as well */
			aarpinput(ifp, m); /* XXX */
			return;
#endif /* NETATALK */
		default:
			ifp->if_noproto++;
			goto dropanyway;
		}
		break;
	}
#endif /* INET || NS */

	default:
		ifp->if_noproto++;
#if defined(INET) || defined(INET6) || defined(NETATALK)
	dropanyway:
#endif
		m_freem(m);
		return;
	}

#if defined(INET) || defined(INET6)
	if (__predict_true(pktq)) {
		if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
			m_freem(m);
		}
		return;
	}
#endif
#if defined(NETATALK)
	if (!inq) {
		m_freem(m);
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
#endif
}

/*
 * Perform common duties while attaching to interface list
 */
void
fddi_ifattach(struct ifnet *ifp, void *lla)
{
	struct ethercom *ec = (struct ethercom *)ifp;

	ifp->if_type = IFT_FDDI;
	ifp->if_hdrlen = 21;
	ifp->if_dlt = DLT_FDDI;
	ifp->if_mtu = FDDIMTU;
	ifp->if_output = fddi_output;
	ifp->if_input = fddi_input;
	ifp->if_baudrate = IF_Mbps(100);
#ifdef IFF_NOTRAILERS
	ifp->if_flags |= IFF_NOTRAILERS;
#endif

	/*
	 * Update the max_linkhdr
	 */
	if (ALIGN(ifp->if_hdrlen) > max_linkhdr)
		max_linkhdr = ALIGN(ifp->if_hdrlen);

	LIST_INIT(&ec->ec_multiaddrs);
	if_set_sadl(ifp, lla, 6, true);

	ifp->if_broadcastaddr = fddibroadcastaddr;
	bpf_attach(ifp, DLT_FDDI, sizeof(struct fddi_header));
#ifdef MBUFTRACE
	strlcpy(ec->ec_tx_mowner.mo_name, ifp->if_xname,
	    sizeof(ec->ec_tx_mowner.mo_name));
	strlcpy(ec->ec_tx_mowner.mo_descr, "tx",
	    sizeof(ec->ec_tx_mowner.mo_descr));
	strlcpy(ec->ec_rx_mowner.mo_name, ifp->if_xname,
	    sizeof(ec->ec_rx_mowner.mo_name));
	strlcpy(ec->ec_rx_mowner.mo_descr, "rx",
	    sizeof(ec->ec_rx_mowner.mo_descr));
	MOWNER_ATTACH(&ec->ec_tx_mowner);
	MOWNER_ATTACH(&ec->ec_rx_mowner);
	ifp->if_mowner = &ec->ec_tx_mowner;
#endif
}
