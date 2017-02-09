/*	$NetBSD: pdq_ifsubr.c,v 1.55 2012/10/27 17:18:22 chs Exp $	*/

/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Id: pdq_ifsubr.c,v 1.12 1997/06/05 01:56:35 thomas Exp
 *
 */

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 *	This module provide bus independent BSD specific O/S functions.
 *	(ie. it provides an ifnet interface to the rest of the system)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pdq_ifsubr.c,v 1.55 2012/10/27 17:18:22 chs Exp $");

#ifdef __NetBSD__
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#if defined(__FreeBSD__) && BSD < 199401
#include <sys/devconf.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#if !defined(__NetBSD__)
#include <net/route.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#if defined(__NetBSD__)
#include <netinet/if_inarp.h>
#endif
#endif
#if defined(__FreeBSD__)
#include <netinet/if_ether.h>
#include <netinet/if_fddi.h>
#else
#include <net/if_fddi.h>
#endif

#if defined(__bsdi__)
#include <netinet/if_ether.h>
#include <i386/isa/isavar.h>
#endif


#ifndef __NetBSD__
#include <vm/vm.h>
#endif

#if defined(__FreeBSD__)
/*
 * Yet another specific ifdef for FreeBSD as it diverges...
 */
#include <dev/pdq/pdqvar.h>
#include <dev/pdq/pdqreg.h>
#else
#include "pdqvar.h"
#include "pdqreg.h"
#endif

void
pdq_ifinit(
    pdq_softc_t *sc)
{
    if (sc->sc_if.if_flags & IFF_UP) {
	sc->sc_if.if_flags |= IFF_RUNNING;
	if (sc->sc_if.if_flags & IFF_PROMISC) {
	    sc->sc_pdq->pdq_flags |= PDQ_PROMISC;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PROMISC;
	}
	if (sc->sc_if.if_flags & IFF_LINK1) {
	    sc->sc_pdq->pdq_flags |= PDQ_PASS_SMT;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PASS_SMT;
	}
	sc->sc_pdq->pdq_flags |= PDQ_RUNNING;
	pdq_run(sc->sc_pdq);
    } else {
	sc->sc_if.if_flags &= ~IFF_RUNNING;
	sc->sc_pdq->pdq_flags &= ~PDQ_RUNNING;
	pdq_stop(sc->sc_pdq);
    }
}

void
pdq_ifwatchdog(
    struct ifnet *ifp)
{
    /*
     * No progress was made on the transmit queue for PDQ_OS_TX_TRANSMIT
     * seconds.  Remove all queued packets.
     */

    ifp->if_flags &= ~IFF_OACTIVE;
    ifp->if_timer = 0;
    for (;;) {
	struct mbuf *m;
	IFQ_DEQUEUE(&ifp->if_snd, m);
	if (m == NULL)
	    return;
	PDQ_OS_DATABUF_FREE(PDQ_OS_IFP_TO_SOFTC(ifp)->sc_pdq, m);
    }
}

ifnet_ret_t
pdq_ifstart(
    struct ifnet *ifp)
{
    pdq_softc_t * const sc = PDQ_OS_IFP_TO_SOFTC(ifp);
    struct mbuf *m;
    int tx = 0;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    if (sc->sc_if.if_timer == 0)
	sc->sc_if.if_timer = PDQ_OS_TX_TIMEOUT;

    if ((sc->sc_pdq->pdq_flags & PDQ_TXOK) == 0) {
	sc->sc_if.if_flags |= IFF_OACTIVE;
	return;
    }
    sc->sc_flags |= PDQIF_DOWNCALL;
    for (;; tx = 1) {
	IFQ_POLL(&ifp->if_snd, m);
	if (m == NULL)
	    break;
#if defined(PDQ_BUS_DMA) && !defined(PDQ_BUS_DMA_NOTX)
	if ((m->m_flags & M_HASTXDMAMAP) == 0) {
	    bus_dmamap_t map;
	    if (PDQ_OS_HDR_OFFSET != PDQ_RX_FC_OFFSET) {
		m->m_data[0] = PDQ_FDDI_PH0;
		m->m_data[1] = PDQ_FDDI_PH1;
		m->m_data[2] = PDQ_FDDI_PH2;
	    }
	    if (!bus_dmamap_create(sc->sc_dmatag, m->m_pkthdr.len, 255,
				   m->m_pkthdr.len, 0, BUS_DMA_NOWAIT, &map)) {
		if (!bus_dmamap_load_mbuf(sc->sc_dmatag, map, m,
					  BUS_DMA_WRITE|BUS_DMA_NOWAIT)) {
		    bus_dmamap_sync(sc->sc_dmatag, map, 0, m->m_pkthdr.len,
				    BUS_DMASYNC_PREWRITE);
		    M_SETCTX(m, map);
		    m->m_flags |= M_HASTXDMAMAP;
		}
	    }
	    if ((m->m_flags & M_HASTXDMAMAP) == 0)
		break;
	}
#else
	if (PDQ_OS_HDR_OFFSET != PDQ_RX_FC_OFFSET) {
	    m->m_data[0] = PDQ_FDDI_PH0;
	    m->m_data[1] = PDQ_FDDI_PH1;
	    m->m_data[2] = PDQ_FDDI_PH2;
	}
#endif

	if (pdq_queue_transmit_data(sc->sc_pdq, m) == PDQ_FALSE)
	    break;
	IFQ_DEQUEUE(&ifp->if_snd, m);
    }
    if (m != NULL)
	ifp->if_flags |= IFF_OACTIVE;
    if (tx)
	PDQ_DO_TYPE2_PRODUCER(sc->sc_pdq);
    sc->sc_flags &= ~PDQIF_DOWNCALL;
}

void
pdq_os_receive_pdu(
    pdq_t *pdq,
    struct mbuf *m,
    size_t pktlen,
    int drop)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
    struct fddi_header *fh;

    sc->sc_if.if_ipackets++;
#if defined(PDQ_BUS_DMA)
    {
	/*
	 * Even though the first mbuf start at the first fddi header octet,
	 * the dmamap starts PDQ_OS_HDR_OFFSET octets earlier.  Any additional
	 * mbufs will start normally.
	 */
	int offset = PDQ_OS_HDR_OFFSET;
	struct mbuf *m0;
	for (m0 = m; m0 != NULL; m0 = m0->m_next, offset = 0) {
	    pdq_os_databuf_sync(sc, m0, offset, m0->m_len, BUS_DMASYNC_POSTREAD);
	    bus_dmamap_unload(sc->sc_dmatag, M_GETCTX(m0, bus_dmamap_t));
	    bus_dmamap_destroy(sc->sc_dmatag, M_GETCTX(m0, bus_dmamap_t));
	    m0->m_flags &= ~M_HASRXDMAMAP;
	    M_SETCTX(m0, NULL);
	}
    }
#endif
    m->m_pkthdr.len = pktlen;
    if (sc->sc_bpf != NULL)
	PDQ_BPF_MTAP(sc, m);
    fh = mtod(m, struct fddi_header *);
    if (drop || (fh->fddi_fc & (FDDIFC_L|FDDIFC_F)) != FDDIFC_LLC_ASYNC) {
	PDQ_OS_DATABUF_FREE(pdq, m);
	return;
    }

    m->m_pkthdr.rcvif = &sc->sc_if;
    (*sc->sc_if.if_input)(&sc->sc_if, m);
}

void
pdq_os_restart_transmitter(
    pdq_t *pdq)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
    sc->sc_if.if_flags &= ~IFF_OACTIVE;
    if (IFQ_IS_EMPTY(&sc->sc_if.if_snd) == 0) {
	sc->sc_if.if_timer = PDQ_OS_TX_TIMEOUT;
	if ((sc->sc_flags & PDQIF_DOWNCALL) == 0)
	    pdq_ifstart(&sc->sc_if);
    } else {
	sc->sc_if.if_timer = 0;
    }
}

void
pdq_os_transmit_done(
    pdq_t *pdq,
    struct mbuf *m)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
    if (sc->sc_bpf != NULL)
	PDQ_BPF_MTAP(sc, m);
    PDQ_OS_DATABUF_FREE(pdq, m);
    sc->sc_if.if_opackets++;
}

void
pdq_os_addr_fill(
    pdq_t *pdq,
    pdq_lanaddr_t *addr,
    size_t num_addrs)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
    struct ether_multistep step;
    struct ether_multi *enm;

    /*
     * ADDR_FILTER_SET is always issued before FILTER_SET so
     * we can play with PDQ_ALLMULTI and not worry about
     * queueing a FILTER_SET ourselves.
     */

    pdq->pdq_flags &= ~PDQ_ALLMULTI;
#if defined(IFF_ALLMULTI)
    sc->sc_if.if_flags &= ~IFF_ALLMULTI;
#endif

    ETHER_FIRST_MULTI(step, PDQ_FDDICOM(sc), enm);
    while (enm != NULL && num_addrs > 0) {
	if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6) == 0) {
	    ((u_short *) addr->lanaddr_bytes)[0] = ((u_short *) enm->enm_addrlo)[0];
	    ((u_short *) addr->lanaddr_bytes)[1] = ((u_short *) enm->enm_addrlo)[1];
	    ((u_short *) addr->lanaddr_bytes)[2] = ((u_short *) enm->enm_addrlo)[2];
	    addr++;
	    num_addrs--;
	} else {
	    pdq->pdq_flags |= PDQ_ALLMULTI;
#if defined(IFF_ALLMULTI)
	    sc->sc_if.if_flags |= IFF_ALLMULTI;
#endif
	}
	ETHER_NEXT_MULTI(step, enm);
    }
    /*
     * If not all the address fit into the CAM, turn on all-multicast mode.
     */
    if (enm != NULL) {
	pdq->pdq_flags |= PDQ_ALLMULTI;
#if defined(IFF_ALLMULTI)
	sc->sc_if.if_flags |= IFF_ALLMULTI;
#endif
    }
}

#if defined(IFM_FDDI)
static int
pdq_ifmedia_change(
    struct ifnet *ifp)
{
    pdq_softc_t * const sc = PDQ_OS_IFP_TO_SOFTC(ifp);

    if (sc->sc_ifmedia.ifm_media & IFM_FDX) {
	if ((sc->sc_pdq->pdq_flags & PDQ_WANT_FDX) == 0) {
	    sc->sc_pdq->pdq_flags |= PDQ_WANT_FDX;
	    if (sc->sc_pdq->pdq_flags & PDQ_RUNNING)
		pdq_run(sc->sc_pdq);
	}
    } else if (sc->sc_pdq->pdq_flags & PDQ_WANT_FDX) {
	sc->sc_pdq->pdq_flags &= ~PDQ_WANT_FDX;
	if (sc->sc_pdq->pdq_flags & PDQ_RUNNING)
	    pdq_run(sc->sc_pdq);
    }

    return 0;
}

static void
pdq_ifmedia_status(
    struct ifnet *ifp,
    struct ifmediareq *ifmr)
{
    pdq_softc_t * const sc = PDQ_OS_IFP_TO_SOFTC(ifp);

    ifmr->ifm_status = IFM_AVALID;
    if (sc->sc_pdq->pdq_flags & PDQ_IS_ONRING)
	ifmr->ifm_status |= IFM_ACTIVE;

    ifmr->ifm_active = (ifmr->ifm_current & ~IFM_FDX);
    if (sc->sc_pdq->pdq_flags & PDQ_IS_FDX)
	ifmr->ifm_active |= IFM_FDX;
}

void
pdq_os_update_status(
    pdq_t *pdq,
    const void *arg)
{
    pdq_softc_t * const sc = pdq->pdq_os_ctx;
    const pdq_response_status_chars_get_t *rsp = arg;
    int media = 0;

    switch (rsp->status_chars_get.pmd_type[0]) {
	case PDQ_PMD_TYPE_ANSI_MUTLI_MODE:         media = IFM_FDDI_MMF; break;
	case PDQ_PMD_TYPE_ANSI_SINGLE_MODE_TYPE_1: media = IFM_FDDI_SMF; break;
	case PDQ_PMD_TYPE_ANSI_SIGNLE_MODE_TYPE_2: media = IFM_FDDI_SMF; break;
	case PDQ_PMD_TYPE_UNSHIELDED_TWISTED_PAIR: media = IFM_FDDI_UTP; break;
	default: media |= IFM_MANUAL;
    }

    if (rsp->status_chars_get.station_type == PDQ_STATION_TYPE_DAS)
	media |= IFM_FDDI_DA;

    sc->sc_ifmedia.ifm_media = media | IFM_FDDI;
}
#endif /* defined(IFM_FDDI) */

int
pdq_ifioctl(
    struct ifnet *ifp,
    ioctl_cmd_t cmd,
    void *data)
{
    pdq_softc_t *sc = PDQ_OS_IFP_TO_SOFTC(ifp);
    int s, error = 0;

    s = PDQ_OS_SPL_RAISE();

    switch (cmd) {
	case SIOCINITIFADDR: {
	    struct ifaddr *ifa = (struct ifaddr *)data;

	    ifp->if_flags |= IFF_UP;
	    pdq_ifinit(sc);
	    switch(ifa->ifa_addr->sa_family) {
#if defined(INET)
		case AF_INET:
		    PDQ_ARP_IFINIT(sc, ifa);
		    break;
#endif /* INET */
		default:
		    break;
	    }
	    break;
	}
	case SIOCSIFFLAGS: {
	    if ((error = ifioctl_common(ifp, cmd, data)) != 0)
		break;
	    pdq_ifinit(sc);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    /*
	     * Update multicast listeners
	     */
	    if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
		if (sc->sc_if.if_flags & IFF_RUNNING)
		    pdq_run(sc->sc_pdq);
		error = 0;
	    }
	    break;
	}

#if defined(SIOCSIFMTU)
#if !defined(ifr_mtu)
#define ifr_mtu ifr_metric
#endif
	case SIOCSIFMTU: {
	    struct ifreq *ifr = (struct ifreq *)data;
	    /*
	     * Set the interface MTU.
	     */
	    if (ifr->ifr_mtu > FDDIMTU) {
		error = EINVAL;
		break;
	    }
	    if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
		error = 0;
	    break;
	}
#endif /* SIOCSIFMTU */

#if defined(IFM_FDDI) && defined(SIOCSIFMEDIA)
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA: {
	    struct ifreq *ifr = (struct ifreq *)data;
	    error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, cmd);
	    break;
	}
#endif

	default: {
	    error = ether_ioctl(ifp, cmd, data);
	    break;
	}
    }

    PDQ_OS_SPL_LOWER(s);
    return error;
}

#ifndef IFF_NOTRAILERS
#define	IFF_NOTRAILERS	0
#endif

void
pdq_ifattach(
    pdq_softc_t *sc,
    ifnet_ret_t (*ifwatchdog)(int unit))
{
    struct ifnet *ifp = &sc->sc_if;

    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;

#if (defined(__FreeBSD__) && BSD >= 199506) || defined(__NetBSD__)
    ifp->if_watchdog = pdq_ifwatchdog;
#else
    ifp->if_watchdog = ifwatchdog;
#endif

    ifp->if_ioctl = pdq_ifioctl;
#if !defined(__NetBSD__)
    ifp->if_output = fddi_output;
#endif
    ifp->if_start = pdq_ifstart;
    IFQ_SET_READY(&ifp->if_snd);

#if defined(IFM_FDDI)
    {
	const int media = sc->sc_ifmedia.ifm_media;
	ifmedia_init(&sc->sc_ifmedia, IFM_FDX,
		     pdq_ifmedia_change, pdq_ifmedia_status);
	ifmedia_add(&sc->sc_ifmedia, media, 0, 0);
	ifmedia_set(&sc->sc_ifmedia, media);
    }
#endif

    if_attach(ifp);
#if defined(__NetBSD__)
    fddi_ifattach(ifp, (void *)&sc->sc_pdq->pdq_hwaddr);
#else
    fddi_ifattach(ifp);
#endif
}

#if defined(PDQ_BUS_DMA)
int
pdq_os_memalloc_contig(
    pdq_t *pdq)
{
    pdq_softc_t * const sc = pdq->pdq_os_ctx;
    bus_dma_segment_t db_segs[1], ui_segs[1], cb_segs[1];
    int db_nsegs = 0, ui_nsegs = 0;
    int steps = 0;
    int not_ok;

    not_ok = bus_dmamem_alloc(sc->sc_dmatag,
			 sizeof(*pdq->pdq_dbp), sizeof(*pdq->pdq_dbp),
			 sizeof(*pdq->pdq_dbp), db_segs, 1, &db_nsegs,
#if defined(__sparc__) || defined(__sparc64__)
			BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
#else
			BUS_DMA_NOWAIT);
#endif
    if (!not_ok) {
	steps = 1;
	not_ok = bus_dmamem_map(sc->sc_dmatag, db_segs, db_nsegs,
				sizeof(*pdq->pdq_dbp), (void **) &pdq->pdq_dbp,
				BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 2;
	not_ok = bus_dmamap_create(sc->sc_dmatag, db_segs[0].ds_len, 1,
				   0x2000, 0, BUS_DMA_NOWAIT, &sc->sc_dbmap);
    }
    if (!not_ok) {
	steps = 3;
	not_ok = bus_dmamap_load(sc->sc_dmatag, sc->sc_dbmap,
				 pdq->pdq_dbp, sizeof(*pdq->pdq_dbp),
				 NULL, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 4;
	pdq->pdq_pa_descriptor_block = sc->sc_dbmap->dm_segs[0].ds_addr;
	not_ok = bus_dmamem_alloc(sc->sc_dmatag,
			 PDQ_OS_PAGESIZE, PDQ_OS_PAGESIZE, PDQ_OS_PAGESIZE,
			 ui_segs, 1, &ui_nsegs, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 5;
	not_ok = bus_dmamem_map(sc->sc_dmatag, ui_segs, ui_nsegs,
			    PDQ_OS_PAGESIZE,
			    (void **) &pdq->pdq_unsolicited_info.ui_events,
			    BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 6;
	not_ok = bus_dmamap_create(sc->sc_dmatag, ui_segs[0].ds_len, 1,
				   PDQ_OS_PAGESIZE, 0, BUS_DMA_NOWAIT,
				   &sc->sc_uimap);
    }
    if (!not_ok) {
	steps = 7;
	not_ok = bus_dmamap_load(sc->sc_dmatag, sc->sc_uimap,
				 pdq->pdq_unsolicited_info.ui_events,
				 PDQ_OS_PAGESIZE, NULL, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 8;
	pdq->pdq_unsolicited_info.ui_pa_bufstart = sc->sc_uimap->dm_segs[0].ds_addr;
	cb_segs[0] = db_segs[0];
	cb_segs[0].ds_addr += offsetof(pdq_descriptor_block_t, pdqdb_consumer);
	cb_segs[0].ds_len = sizeof(pdq_consumer_block_t);
#if defined(__sparc__) || defined(__sparc64__)
	pdq->pdq_cbp = (pdq_consumer_block_t*)((unsigned long int)pdq->pdq_dbp +
	    (unsigned long int)offsetof(pdq_descriptor_block_t,pdqdb_consumer));
#else
	not_ok = bus_dmamem_map(sc->sc_dmatag, cb_segs, 1,
				sizeof(*pdq->pdq_cbp),
				(void **)&pdq->pdq_cbp,
				BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
#endif
    }
    if (!not_ok) {
	steps = 9;
	not_ok = bus_dmamap_create(sc->sc_dmatag, cb_segs[0].ds_len, 1,
				   0x2000, 0, BUS_DMA_NOWAIT, &sc->sc_cbmap);
    }
    if (!not_ok) {
	steps = 10;
	not_ok = bus_dmamap_load(sc->sc_dmatag, sc->sc_cbmap,
				 pdq->pdq_cbp, sizeof(*pdq->pdq_cbp),
				 NULL, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	pdq->pdq_pa_consumer_block = sc->sc_cbmap->dm_segs[0].ds_addr;
	return not_ok;
    }

    switch (steps) {
	case 11: {
	    bus_dmamap_unload(sc->sc_dmatag, sc->sc_cbmap);
	    /* FALL THROUGH */
	}
	case 10: {
	    bus_dmamap_destroy(sc->sc_dmatag, sc->sc_cbmap);
	    /* FALL THROUGH */
	}
	case 9: {
	    bus_dmamem_unmap(sc->sc_dmatag,
			     (void *)pdq->pdq_cbp, sizeof(*pdq->pdq_cbp));
	    /* FALL THROUGH */
	}
	case 8: {
	    bus_dmamap_unload(sc->sc_dmatag, sc->sc_uimap);
	    /* FALL THROUGH */
	}
	case 7: {
	    bus_dmamap_destroy(sc->sc_dmatag, sc->sc_uimap);
	    /* FALL THROUGH */
	}
	case 6: {
	    bus_dmamem_unmap(sc->sc_dmatag,
			     (void *) pdq->pdq_unsolicited_info.ui_events,
			     PDQ_OS_PAGESIZE);
	    /* FALL THROUGH */
	}
	case 5: {
	    bus_dmamem_free(sc->sc_dmatag, ui_segs, ui_nsegs);
	    /* FALL THROUGH */
	}
	case 4: {
	    bus_dmamap_unload(sc->sc_dmatag, sc->sc_dbmap);
	    /* FALL THROUGH */
	}
	case 3: {
	    bus_dmamap_destroy(sc->sc_dmatag, sc->sc_dbmap);
	    /* FALL THROUGH */
	}
	case 2: {
	    bus_dmamem_unmap(sc->sc_dmatag,
			     (void *) pdq->pdq_dbp,
			     sizeof(*pdq->pdq_dbp));
	    /* FALL THROUGH */
	}
	case 1: {
	    bus_dmamem_free(sc->sc_dmatag, db_segs, db_nsegs);
	    /* FALL THROUGH */
	}
    }

    return not_ok;
}

extern void
pdq_os_descriptor_block_sync(
    pdq_os_ctx_t *sc,
    size_t offset,
    size_t length,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, sc->sc_dbmap, offset, length, ops);
}

extern void
pdq_os_consumer_block_sync(
    pdq_os_ctx_t *sc,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, sc->sc_cbmap, 0, sizeof(pdq_consumer_block_t), ops);
}

extern void
pdq_os_unsolicited_event_sync(
    pdq_os_ctx_t *sc,
    size_t offset,
    size_t length,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, sc->sc_uimap, offset, length, ops);
}

extern void
pdq_os_databuf_sync(
    pdq_os_ctx_t *sc,
    struct mbuf *m,
    size_t offset,
    size_t length,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, M_GETCTX(m, bus_dmamap_t), offset, length, ops);
}

extern void
pdq_os_databuf_free(
    pdq_os_ctx_t *sc,
    struct mbuf *m)
{
    if (m->m_flags & (M_HASRXDMAMAP|M_HASTXDMAMAP)) {
	bus_dmamap_t map = M_GETCTX(m, bus_dmamap_t);
	bus_dmamap_unload(sc->sc_dmatag, map);
	bus_dmamap_destroy(sc->sc_dmatag, map);
	m->m_flags &= ~(M_HASRXDMAMAP|M_HASTXDMAMAP);
    }
    m_freem(m);
}

extern struct mbuf *
pdq_os_databuf_alloc(
    pdq_os_ctx_t *sc)
{
    struct mbuf *m;
    bus_dmamap_t map;

    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (m == NULL) {
	aprint_error_dev(sc->sc_dev, "can't alloc small buf\n");
	return NULL;
    }
    MCLGET(m, M_DONTWAIT);
    if ((m->m_flags & M_EXT) == 0) {
	aprint_error_dev(sc->sc_dev, "can't alloc cluster\n");
        m_free(m);
	return NULL;
    }
    MCLAIM(m, &PDQ_FDDICOM(sc)->ec_rx_mowner);
    m->m_pkthdr.len = m->m_len = PDQ_OS_DATABUF_SIZE;

    if (bus_dmamap_create(sc->sc_dmatag, PDQ_OS_DATABUF_SIZE,
			   1, PDQ_OS_DATABUF_SIZE, 0, BUS_DMA_NOWAIT, &map)) {
	aprint_error_dev(sc->sc_dev, "can't create dmamap\n");
	m_free(m);
	return NULL;
    }
    if (bus_dmamap_load_mbuf(sc->sc_dmatag, map, m,
    			     BUS_DMA_READ|BUS_DMA_NOWAIT)) {
	aprint_error_dev(sc->sc_dev, "can't load dmamap\n");
	bus_dmamap_destroy(sc->sc_dmatag, map);
	m_free(m);
	return NULL;
    }
    m->m_flags |= M_HASRXDMAMAP;
    M_SETCTX(m, map);
    return m;
}
#endif
