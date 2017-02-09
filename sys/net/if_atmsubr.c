/*      $NetBSD: if_atmsubr.c,v 1.54 2015/08/24 22:21:26 pooka Exp $       */

/*
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 */

/*
 * if_atmsubr.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_atmsubr.c,v 1.54 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_gateway.h"
#include "opt_natm.h"
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
#include <net/if_atm.h>
#include <net/ethertypes.h> /* XXX: for ETHERTYPE_* */

#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_atm.h>

#if defined(INET) || defined(INET6)
#include <netinet/in_var.h>
#endif
#ifdef NATM
#include <netnatm/natm.h>
#endif

#define senderr(e) { error = (e); goto bad;}

/*
 * atm_output: ATM output routine
 *   inputs:
 *     "ifp" = ATM interface to output to
 *     "m0" = the packet to output
 *     "dst" = the sockaddr to send to (either IP addr, or raw VPI/VCI)
 *     "rt0" = the route to use
 *   returns: error code   [0 == ok]
 *
 *   note: special semantic: if (dst == NULL) then we assume "m" already
 *		has an atm_pseudohdr on it and just send it directly.
 *		[for native mode ATM output]   if dst is null, then
 *		rt0 must also be NULL.
 */

int
atm_output(struct ifnet *ifp, struct mbuf *m0, const struct sockaddr *dst,
    struct rtentry *rt)
{
	uint16_t etype = 0;			/* if using LLC/SNAP */
	int error = 0, sz;
	struct atm_pseudohdr atmdst, *ad;
	struct mbuf *m = m0;
	struct atmllc *atmllc;
	uint32_t atm_flags;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m,
	     (dst != NULL ? dst->sa_family : AF_UNSPEC), &pktattr);

	/*
	 * check for non-native ATM traffic   (dst != NULL)
	 */
	if (dst) {
		switch (dst->sa_family) {
#ifdef INET
		case AF_INET:
#endif
#ifdef INET6
		case AF_INET6:
#endif
#if defined(INET) || defined(INET6)
			if (dst->sa_family == AF_INET)
				etype = ETHERTYPE_IP;
			else
				etype = ETHERTYPE_IPV6;
# ifdef ATM_PVCEXT
			if (ifp->if_flags & IFF_POINTOPOINT) {
				/* pvc subinterface */
				struct pvcsif *pvcsif = (struct pvcsif *)ifp;
				atmdst = pvcsif->sif_aph;
				break;
			}
# endif
			if (!atmresolve(rt, m, dst, &atmdst)) {
				m = NULL;
				/* XXX: atmresolve already free'd it */
				senderr(EHOSTUNREACH);
				/* XXX: put ATMARP stuff here */
				/* XXX: watch who frees m on failure */
			}
			break;
#endif

		case AF_UNSPEC:
			/*
			 * XXX: bpfwrite or output from a pvc shadow if.
			 * assuming dst contains 12 bytes (atm pseudo
			 * header (4) + LLC/SNAP (8))
			 */
			memcpy(&atmdst, dst->sa_data, sizeof(atmdst));
			break;

		default:
#if defined(__NetBSD__) || defined(__OpenBSD__)
			printf("%s: can't handle af%d\n", ifp->if_xname,
			    dst->sa_family);
#elif defined(__FreeBSD__) || defined(__bsdi__)
			printf("%s%d: can't handle af%d\n", ifp->if_name,
			    ifp->if_unit, dst->sa_family);
#endif
			senderr(EAFNOSUPPORT);
		}

		/*
		 * must add atm_pseudohdr to data
		 */
		sz = sizeof(atmdst);
		atm_flags = ATM_PH_FLAGS(&atmdst);
		if (atm_flags & ATM_PH_LLCSNAP) sz += 8; /* sizeof snap == 8 */
		M_PREPEND(m, sz, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		ad = mtod(m, struct atm_pseudohdr *);
		*ad = atmdst;
		if (atm_flags & ATM_PH_LLCSNAP) {
			atmllc = (struct atmllc *)(ad + 1);
			memcpy(atmllc->llchdr, ATMLLC_HDR,
						sizeof(atmllc->llchdr));
			ATM_LLC_SETTYPE(atmllc, etype);
		}
	}

	return ifq_enqueue(ifp, m ALTQ_COMMA ALTQ_DECL(&pktattr));

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received ATM packet;
 * the packet is in the mbuf chain m.
 */
void
atm_input(struct ifnet *ifp, struct atm_pseudohdr *ah, struct mbuf *m,
    void *rxhand)
{
	pktqueue_t *pktq = NULL;
	uint16_t etype = ETHERTYPE_IP; /* default */

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_ibytes += m->m_pkthdr.len;

	if (rxhand) {
#ifdef NATM
	  struct natmpcb *npcb = rxhand;
	  struct ifqueue *inq;
	  int s, isr = 0;

	  s = splnet();			/* in case 2 atm cards @ diff lvls */
	  npcb->npcb_inq++;			/* count # in queue */
	  splx(s);
	  isr = NETISR_NATM;
	  inq = &natmintrq;
	  m->m_pkthdr.rcvif = rxhand; /* XXX: overload */

	  s = splnet();
	  if (IF_QFULL(inq)) {
	  	IF_DROP(inq);
	  	m_freem(m);
	  } else {
	  	IF_ENQUEUE(inq, m);
	  	schednetisr(isr);
	  }
	  splx(s);
#else
	  printf("atm_input: NATM detected but not configured in kernel\n");
	  m_freem(m);
#endif
	  return;

	} else {
	  /*
	   * handle LLC/SNAP header, if present
	   */
	  if (ATM_PH_FLAGS(ah) & ATM_PH_LLCSNAP) {
	    struct atmllc *alc;
	    if (m->m_len < sizeof(*alc) && (m = m_pullup(m, sizeof(*alc))) == 0)
		  return; /* failed */
	    alc = mtod(m, struct atmllc *);
	    if (memcmp(alc, ATMLLC_HDR, 6)) {
#if defined(__NetBSD__) || defined(__OpenBSD__)
	      printf("%s: recv'd invalid LLC/SNAP frame [vp=%d,vc=%d]\n",
		  ifp->if_xname, ATM_PH_VPI(ah), ATM_PH_VCI(ah));
#elif defined(__FreeBSD__) || defined(__bsdi__)
	      printf("%s%d: recv'd invalid LLC/SNAP frame [vp=%d,vc=%d]\n",
		  ifp->if_name, ifp->if_unit, ATM_PH_VPI(ah), ATM_PH_VCI(ah));
#endif
	      m_freem(m);
              return;
	    }
	    etype = ATM_LLC_TYPE(alc);
	    m_adj(m, sizeof(*alc));
	  }

	  switch (etype) {
#ifdef INET
	  case ETHERTYPE_IP:
#ifdef GATEWAY
		if (ipflow_fastforward(m))
			return;
#endif
		pktq = ip_pktq;
		break;
#endif /* INET */
#ifdef INET6
	  case ETHERTYPE_IPV6:
#ifdef GATEWAY  
		if (ip6flow_fastforward(&m))
			return;
#endif
		pktq = ip6_pktq;
		break;
#endif
	  default:
	      m_freem(m);
	      return;
	  }
	}

	if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
		m_freem(m);
	}
}

/*
 * Perform common duties while attaching to interface list
 */
void
atm_ifattach(struct ifnet *ifp)
{

	ifp->if_type = IFT_ATM;
	ifp->if_addrlen = 0;
	ifp->if_hdrlen = 0;
	ifp->if_dlt = DLT_ATM_RFC1483;
	ifp->if_mtu = ATMMTU;
	ifp->if_output = atm_output;
#if 0 /* XXX XXX XXX */
	ifp->if_input = atm_input;
#endif

	if_alloc_sadl(ifp);
	/* XXX Store LLADDR for ATMARP. */

	bpf_attach(ifp, DLT_ATM_RFC1483, sizeof(struct atmllc));
}

#ifdef ATM_PVCEXT

static int pvc_max_number = 16;	/* max number of PVCs */
static int pvc_number = 0;	/* pvc unit number */

struct ifnet *
pvcsif_alloc(void)
{
	struct pvcsif *pvcsif;

	if (pvc_number >= pvc_max_number)
		return (NULL);
	pvcsif = malloc(sizeof(struct pvcsif),
	       M_DEVBUF, M_WAITOK|M_ZERO);
	if (pvcsif == NULL)
		return (NULL);

#ifdef __NetBSD__
	snprintf(pvcsif->sif_if.if_xname, sizeof(pvcsif->sif_if.if_xname),
	    "pvc%d", pvc_number++);
#else
	pvcsif->sif_if.if_name = "pvc";
	pvcsif->sif_if.if_unit = pvc_number++;
#endif
	return (&pvcsif->sif_if);
}
#endif /* ATM_PVCEXT */
