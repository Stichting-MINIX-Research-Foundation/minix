/*	$NetBSD: if_arcsubr.c,v 1.68 2015/08/24 22:21:26 pooka Exp $	*/

/*
 * Copyright (c) 1994, 1995 Ignatios Souvatzis
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
 * from: NetBSD: if_ethersubr.c,v 1.9 1994/06/29 06:36:11 cgd Exp
 *       @(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_arcsubr.c,v 1.68 2015/08/24 22:21:26 pooka Exp $");

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
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_arc.h>
#include <net/if_arp.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_inarp.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif

#define ARCNET_ALLOW_BROKEN_ARP

#ifndef ARC_IPMTU
#define ARC_IPMTU	1500
#endif

static struct mbuf *arc_defrag(struct ifnet *, struct mbuf *);

/*
 * RC1201 requires us to have this configurable. We have it only per
 * machine at the moment... there is no generic "set mtu" ioctl, AFAICS.
 * Anyway, it is possible to binpatch this or set it per kernel config
 * option.
 */
#if ARC_IPMTU > 60480
ERROR: The arc_ipmtu is ARC_IPMTU, but must not exceed 60480.
#endif
int arc_ipmtu = ARC_IPMTU;
uint8_t  arcbroadcastaddr = 0;

#define senderr(e) { error = (e); goto bad;}

static	int arc_output(struct ifnet *, struct mbuf *,
	    const struct sockaddr *, struct rtentry *);
static	void arc_input(struct ifnet *, struct mbuf *);

/*
 * ARCnet output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arccom structure.
 */
static int
arc_output(struct ifnet *ifp, struct mbuf *m0, const struct sockaddr *dst,
    struct rtentry *rt)
{
	struct mbuf		*m, *m1, *mcopy;
	struct arccom		*ac;
	const struct arc_header	*cah;
	struct arc_header	*ah;
	struct arphdr		*arph;
	int			error, newencoding;
	uint8_t			atype, adst, myself;
	int			tfrags, sflag, fsflag, rsflag;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		return (ENETDOWN); /* m, m1 aren't initialized yet */

	error = newencoding = 0;
	ac = (struct arccom *)ifp;
	m = m0;
	mcopy = m1 = NULL;

	myself = *CLLADDR(ifp->if_sadl);

	/*
	 * if the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:

		/*
		 * For now, use the simple IP addr -> ARCnet addr mapping
		 */
		if (m->m_flags & (M_BCAST|M_MCAST))
			adst = arcbroadcastaddr; /* ARCnet broadcast address */
		else if (ifp->if_flags & IFF_NOARP)
			adst = ntohl(satocsin(dst)->sin_addr.s_addr) & 0xFF;
		else if (!arpresolve(ifp, rt, m, dst, &adst))
			return 0;	/* not resolved yet */

		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & (M_BCAST|M_MCAST)) &&
		    (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		if (ifp->if_flags & IFF_LINK0) {
			atype = ARCTYPE_IP;
			newencoding = 1;
		} else {
			atype = ARCTYPE_IP_OLD;
			newencoding = 0;
		}
		break;

	case AF_ARP:
		arph = mtod(m, struct arphdr *);
		if (m->m_flags & M_BCAST)
			adst = arcbroadcastaddr;
		else {
			uint8_t *tha = ar_tha(arph);
			if (tha == NULL)
				return 0;
			adst = *tha;
		}

		arph->ar_hrd = htons(ARPHRD_ARCNET);

		switch (ntohs(arph->ar_op)) {
		case ARPOP_REVREQUEST:
		case ARPOP_REVREPLY:
			if (!(ifp->if_flags & IFF_LINK0)) {
				printf("%s: can't handle af%d\n",
				    ifp->if_xname, dst->sa_family);
				senderr(EAFNOSUPPORT);
			}

			atype = htons(ARCTYPE_REVARP);
			newencoding = 1;
			break;

		case ARPOP_REQUEST:
		case ARPOP_REPLY:
		default:
			if (ifp->if_flags & IFF_LINK0) {
				atype = htons(ARCTYPE_ARP);
				newencoding = 1;
			} else {
				atype = htons(ARCTYPE_ARP_OLD);
				newencoding = 0;
			}
		}
#ifdef ARCNET_ALLOW_BROKEN_ARP
		/*
		 * XXX It's not clear per RFC826 if this is needed, but
		 * "assigned numbers" say this is wrong.
		 * However, e.g., AmiTCP 3.0Beta used it... we make this
		 * switchable for emergency cases. Not perfect, but...
		 */
		if (ifp->if_flags & IFF_LINK2)
			arph->ar_pro = atype - 1;
#endif
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (!nd6_storelladdr(ifp, rt, m, dst, &adst, sizeof(adst)))
			return (0); /* it must be impossible, but... */
		atype = htons(ARCTYPE_INET6);
		newencoding = 1;
		break;
#endif

	case AF_UNSPEC:
		cah = (const struct arc_header *)dst->sa_data;
 		adst = cah->arc_dhost;
		atype = cah->arc_type;
		break;

	default:
		printf("%s: can't handle af%d\n", ifp->if_xname,
		    dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	if (mcopy)
		(void) looutput(ifp, mcopy, dst, rt);

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 *
	 * For ARCnet, this is just symbolic. The header changes
	 * form and position on its way into the hardware and out of
	 * the wire.  At this point, it contains source, destination and
	 * packet type.
	 */
	if (newencoding) {
		++ac->ac_seqid; /* make the seqid unique */

		tfrags = (m->m_pkthdr.len + 503) / 504;
		fsflag = 2 * tfrags - 3;
		sflag = 0;
		rsflag = fsflag;

		while (sflag < fsflag) {
			/* we CAN'T have short packets here */
			m1 = m_split(m, 504, M_DONTWAIT);
			if (m1 == 0)
				senderr(ENOBUFS);

			M_PREPEND(m, ARC_HDRNEWLEN, M_DONTWAIT);
			if (m == 0)
				senderr(ENOBUFS);
			ah = mtod(m, struct arc_header *);
			ah->arc_type = atype;
			ah->arc_dhost = adst;
			ah->arc_shost = myself;
			ah->arc_flag = rsflag;
			ah->arc_seqid = ac->ac_seqid;

			if ((error = ifq_enqueue(ifp, m ALTQ_COMMA
			    ALTQ_DECL(&pktattr))) != 0)
				return (error);

			m = m1;
			sflag += 2;
			rsflag = sflag;
		}
		m1 = NULL;


		/* here we can have small, especially forbidden packets */

		if ((m->m_pkthdr.len >=
		    ARC_MIN_FORBID_LEN - ARC_HDRNEWLEN + 2) &&
		    (m->m_pkthdr.len <=
		    ARC_MAX_FORBID_LEN - ARC_HDRNEWLEN + 2)) {

			M_PREPEND(m, ARC_HDRNEWLEN_EXC, M_DONTWAIT);
			if (m == 0)
				senderr(ENOBUFS);
			ah = mtod(m, struct arc_header *);
			ah->arc_flag = 0xFF;
			ah->arc_seqid = 0xFFFF;
			ah->arc_type2 = atype;
			ah->arc_flag2 = sflag;
			ah->arc_seqid2 = ac->ac_seqid;
		} else {
			M_PREPEND(m, ARC_HDRNEWLEN, M_DONTWAIT);
			if (m == 0)
				senderr(ENOBUFS);
			ah = mtod(m, struct arc_header *);
			ah->arc_flag = sflag;
			ah->arc_seqid = ac->ac_seqid;
		}

		ah->arc_dhost = adst;
		ah->arc_shost = myself;
		ah->arc_type = atype;
	} else {
		M_PREPEND(m, ARC_HDRLEN, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		ah = mtod(m, struct arc_header *);
		ah->arc_type = atype;
		ah->arc_dhost = adst;
		ah->arc_shost = myself;
	}

	return ifq_enqueue(ifp, m ALTQ_COMMA ALTQ_DECL(&pktattr));

bad:
	if (m1)
		m_freem(m1);
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Defragmenter. Returns mbuf if last packet found, else
 * NULL. frees imcoming mbuf as necessary.
 */

static struct mbuf *
arc_defrag(struct ifnet *ifp, struct mbuf *m)
{
	struct arc_header *ah, *ah1;
	struct arccom *ac;
	struct ac_frag *af;
	struct mbuf *m1;
	const char *s;
	int newflen;
	u_char src, dst, typ;

	ac = (struct arccom *)ifp;

	if (m->m_len < ARC_HDRNEWLEN) {
		m = m_pullup(m, ARC_HDRNEWLEN);
		if (m == NULL) {
			++ifp->if_ierrors;
			return NULL;
		}
	}

	ah = mtod(m, struct arc_header *);
	typ = ah->arc_type;

	if (!arc_isphds(typ))
		return m;

	src = ah->arc_shost;
	dst = ah->arc_dhost;

	if (ah->arc_flag == 0xff) {
		m_adj(m, 4);

		if (m->m_len < ARC_HDRNEWLEN) {
			m = m_pullup(m, ARC_HDRNEWLEN);
			if (m == NULL) {
				++ifp->if_ierrors;
				return NULL;
			}
		}

		ah = mtod(m, struct arc_header *);
	}

	af = &ac->ac_fragtab[src];
	m1 = af->af_packet;
	s = "debug code error";

	if (ah->arc_flag & 1) {
		/*
		 * first fragment. We always initialize, which is
		 * about the right thing to do, as we only want to
		 * accept one fragmented packet per src at a time.
		 */
		if (m1 != NULL)
			m_freem(m1);

		af->af_packet = m;
		m1 = m;
		af->af_maxflag = ah->arc_flag;
		af->af_lastseen = 0;
		af->af_seqid = ah->arc_seqid;

		return NULL;
		/* notreached */
	} else {
		/* check for unfragmented packet */
		if (ah->arc_flag == 0)
			return m;

		/* do we have a first packet from that src? */
		if (m1 == NULL) {
			s = "no first frag";
			goto outofseq;
		}

		ah1 = mtod(m1, struct arc_header *);

		if (ah->arc_seqid != ah1->arc_seqid) {
			s = "seqid differs";
			goto outofseq;
		}

		if (typ != ah1->arc_type) {
			s = "type differs";
			goto outofseq;
		}

		if (dst != ah1->arc_dhost) {
			s = "dest host differs";
			goto outofseq;
		}

		/* typ, seqid and dst are ok here. */

		if (ah->arc_flag == af->af_lastseen) {
			m_freem(m);
			return NULL;
		}

		if (ah->arc_flag == af->af_lastseen + 2) {
			/* ok, this is next fragment */
			af->af_lastseen = ah->arc_flag;
			m_adj(m, ARC_HDRNEWLEN);

			/*
			 * m_cat might free the first mbuf (with pkthdr)
			 * in 2nd chain; therefore:
			 */

			newflen = m->m_pkthdr.len;

			m_cat(m1, m);

			m1->m_pkthdr.len += newflen;

			/* is it the last one? */
			if (af->af_lastseen > af->af_maxflag) {
				af->af_packet = NULL;
				return (m1);
			} else
				return NULL;
		}
		s = "other reason";
		/* if all else fails, it is out of sequence, too */
	}
outofseq:
	if (m1) {
		m_freem(m1);
		af->af_packet = NULL;
	}

	if (m)
		m_freem(m);

	log(LOG_INFO,"%s: got out of seq. packet: %s\n",
	    ifp->if_xname, s);

	return NULL;
}

/*
 * return 1 if Packet Header Definition Standard, else 0.
 * For now: old IP, old ARP aren't obviously. Lacking correct information,
 * we guess that besides new IP and new ARP also IPX and APPLETALK are PHDS.
 * (Apple and Novell corporations were involved, among others, in PHDS work).
 * Easiest is to assume that everybody else uses that, too.
 */
int
arc_isphds(uint8_t type)
{
	return (type != ARCTYPE_IP_OLD &&
		type != ARCTYPE_ARP_OLD &&
		type != ARCTYPE_DIAGNOSE);
}

/*
 * Process a received Arcnet packet;
 * the packet is in the mbuf chain m with
 * the ARCnet header.
 */
static void
arc_input(struct ifnet *ifp, struct mbuf *m)
{
	pktqueue_t *pktq = NULL;
	struct arc_header *ah;
	struct ifqueue *inq;
	uint8_t atype;
	int isr = 0;
	int s;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	/* possibly defragment: */
	m = arc_defrag(ifp, m);
	if (m == NULL)
		return;

	ah = mtod(m, struct arc_header *);

	ifp->if_ibytes += m->m_pkthdr.len;

	if (arcbroadcastaddr == ah->arc_dhost) {
		m->m_flags |= M_BCAST|M_MCAST;
		ifp->if_imcasts++;
	}

	atype = ah->arc_type;
	switch (atype) {
#ifdef INET
	case ARCTYPE_IP:
		m_adj(m, ARC_HDRNEWLEN);
		pktq = ip_pktq;
		break;

	case ARCTYPE_IP_OLD:
		m_adj(m, ARC_HDRLEN);
		pktq = ip_pktq;
		break;

	case ARCTYPE_ARP:
		m_adj(m, ARC_HDRNEWLEN);
		isr = NETISR_ARP;
		inq = &arpintrq;
#ifdef ARCNET_ALLOW_BROKEN_ARP
		mtod(m, struct arphdr *)->ar_pro = htons(ETHERTYPE_IP);
#endif
		break;

	case ARCTYPE_ARP_OLD:
		m_adj(m, ARC_HDRLEN);
		isr = NETISR_ARP;
		inq = &arpintrq;
#ifdef ARCNET_ALLOW_BROKEN_ARP
		mtod(m, struct arphdr *)->ar_pro = htons(ETHERTYPE_IP);
#endif
		break;
#endif
#ifdef INET6
	case ARCTYPE_INET6:
		m_adj(m, ARC_HDRNEWLEN);
		pktq = ip6_pktq;
		break;
#endif
	default:
		m_freem(m);
		return;
	}

	s = splnet();
	if (__predict_true(pktq)) {
		if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
			m_freem(m);
		}
		splx(s);
		return;
	}
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else {
		IF_ENQUEUE(inq, m);
		schednetisr(isr);
	}
	splx(s);
}

/*
 * Convert Arcnet address to printable (loggable) representation.
 */
char *
arc_sprintf(uint8_t *ap)
{
	static char arcbuf[3];
	char *cp = arcbuf;

	*cp++ = hexdigits[*ap >> 4];
	*cp++ = hexdigits[*ap++ & 0xf];
	*cp   = 0;
	return (arcbuf);
}

/*
 * Perform common duties while attaching to interface list
 */
void
arc_ifattach(struct ifnet *ifp, uint8_t lla)
{
	struct arccom *ac;

	ifp->if_type = IFT_ARCNET;
	ifp->if_addrlen = 1;
	ifp->if_hdrlen = ARC_HDRLEN;
	ifp->if_dlt = DLT_ARCNET;
	if (ifp->if_flags & IFF_BROADCAST)
		ifp->if_flags |= IFF_MULTICAST|IFF_ALLMULTI;
	if (ifp->if_flags & IFF_LINK0 && arc_ipmtu > ARC_PHDS_MAXMTU)
		log(LOG_ERR,
		    "%s: arc_ipmtu is %d, but must not exceed %d\n",
		    ifp->if_xname, arc_ipmtu, ARC_PHDS_MAXMTU);

	ifp->if_output = arc_output;
	ifp->if_input = arc_input;
	ac = (struct arccom *)ifp;
	ac->ac_seqid = (time_second) & 0xFFFF; /* try to make seqid unique */
	if (lla == 0) {
		/* XXX this message isn't entirely clear, to me -- cgd */
		log(LOG_ERR,"%s: link address 0 reserved for broadcasts.  Please change it and ifconfig %s down up\n",
		   ifp->if_xname, ifp->if_xname);
	}
	if_attach(ifp);
	if_set_sadl(ifp, &lla, sizeof(lla), true);

	ifp->if_broadcastaddr = &arcbroadcastaddr;

	bpf_attach(ifp, DLT_ARCNET, ARC_HDRLEN);
}
