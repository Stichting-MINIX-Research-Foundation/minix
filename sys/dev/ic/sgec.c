/*      $NetBSD: sgec.c,v 1.41 2015/08/30 04:02:06 dholland Exp $ */
/*
 * Copyright (c) 1999 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

/*
 * Driver for the SGEC (Second Generation Ethernet Controller), sitting
 * on for example the VAX 4000/300 (KA670).
 *
 * The SGEC looks like a mixture of the DEQNA and the TULIP. Fun toy.
 *
 * Even though the chip is capable to use virtual addresses (read the
 * System Page Table directly) this driver doesn't do so, and there
 * is no benefit in doing it either in NetBSD of today.
 *
 * Things that is still to do:
 *	Collect statistics.
 *	Use imperfect filtering when many multicast addresses.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sgec.c,v 1.41 2015/08/30 04:02:06 dholland Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/if_inarp.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/bus.h>

#include <dev/ic/sgecreg.h>
#include <dev/ic/sgecvar.h>

static	void	zeinit(struct ze_softc *);
static	void	zestart(struct ifnet *);
static	int	zeioctl(struct ifnet *, u_long, void *);
static	int	ze_add_rxbuf(struct ze_softc *, int);
static	void	ze_setup(struct ze_softc *);
static	void	zetimeout(struct ifnet *);
static	bool	zereset(struct ze_softc *);

#define	ZE_WCSR(csr, val) \
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, csr, val)
#define	ZE_RCSR(csr) \
	bus_space_read_4(sc->sc_iot, sc->sc_ioh, csr)

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
sgec_attach(struct ze_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ze_tdes *tp;
	struct ze_rdes *rp;
	bus_dma_segment_t seg;
	int i, rseg, error;

        /*
         * Allocate DMA safe memory for descriptors and setup memory.
         */
	error = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct ze_cdata),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error(": unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, sizeof(struct ze_cdata),
	    (void **)&sc->sc_zedata, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		aprint_error(
		    ": unable to map control data, error = %d\n", error);
		goto fail_1;
	}

	error = bus_dmamap_create(sc->sc_dmat, sizeof(struct ze_cdata), 1,
	    sizeof(struct ze_cdata), 0, BUS_DMA_NOWAIT, &sc->sc_cmap);
	if (error) {
		aprint_error(
		    ": unable to create control data DMA map, error = %d\n",
		    error);
		goto fail_2;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_cmap, sc->sc_zedata,
	    sizeof(struct ze_cdata), NULL, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error(
		    ": unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}

	/*
	 * Zero the newly allocated memory.
	 */
	memset(sc->sc_zedata, 0, sizeof(struct ze_cdata));

	/*
	 * Create the transmit descriptor DMA maps.
	 */
	for (i = 0; error == 0 && i < TXDESCS; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    TXDESCS - 1, MCLBYTES, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
		    &sc->sc_xmtmap[i]);
	}
	if (error) {
		aprint_error(": unable to create tx DMA map %d, error = %d\n",
		    i, error);
		goto fail_4;
	}

	/*
	 * Create receive buffer DMA maps.
	 */
	for (i = 0; error == 0 && i < RXDESCS; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &sc->sc_rcvmap[i]);
	}
	if (error) {
		aprint_error(": unable to create rx DMA map %d, error = %d\n",
		    i, error);
		goto fail_5;
	}

	/*
	 * Pre-allocate the receive buffers.
	 */
	for (i = 0; error == 0 && i < RXDESCS; i++) {
		error = ze_add_rxbuf(sc, i);
	}

	if (error) {
		aprint_error(
		    ": unable to allocate or map rx buffer %d, error = %d\n",
		    i, error);
		goto fail_6;
	}

	/* For vmstat -i
	 */
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(sc->sc_dev), "intr");
	evcnt_attach_dynamic(&sc->sc_rxintrcnt, EVCNT_TYPE_INTR,
	    &sc->sc_intrcnt, device_xname(sc->sc_dev), "rx intr");
	evcnt_attach_dynamic(&sc->sc_txintrcnt, EVCNT_TYPE_INTR,
	    &sc->sc_intrcnt, device_xname(sc->sc_dev), "tx intr");
	evcnt_attach_dynamic(&sc->sc_txdraincnt, EVCNT_TYPE_INTR,
	    &sc->sc_intrcnt, device_xname(sc->sc_dev), "tx drain");
	evcnt_attach_dynamic(&sc->sc_nobufintrcnt, EVCNT_TYPE_INTR,
	    &sc->sc_intrcnt, device_xname(sc->sc_dev), "nobuf intr");
	evcnt_attach_dynamic(&sc->sc_nointrcnt, EVCNT_TYPE_INTR,
	    &sc->sc_intrcnt, device_xname(sc->sc_dev), "no intr");

	/*
	 * Create ring loops of the buffer chains.
	 * This is only done once.
	 */
	sc->sc_pzedata = (struct ze_cdata *)sc->sc_cmap->dm_segs[0].ds_addr;

	rp = sc->sc_zedata->zc_recv;
	rp[RXDESCS].ze_framelen = ZE_FRAMELEN_OW;
	rp[RXDESCS].ze_rdes1 = ZE_RDES1_CA;
	rp[RXDESCS].ze_bufaddr = (char *)sc->sc_pzedata->zc_recv;

	tp = sc->sc_zedata->zc_xmit;
	tp[TXDESCS].ze_tdr = ZE_TDR_OW;
	tp[TXDESCS].ze_tdes1 = ZE_TDES1_CA;
	tp[TXDESCS].ze_bufaddr = (char *)sc->sc_pzedata->zc_xmit;

	if (zereset(sc))
		return;

	strcpy(ifp->if_xname, device_xname(sc->sc_dev));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = zestart;
	ifp->if_ioctl = zeioctl;
	ifp->if_watchdog = zetimeout;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	aprint_normal("\n");
	aprint_normal_dev(sc->sc_dev, "hardware address %s\n",
	    ether_sprintf(sc->sc_enaddr));
	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_6:
	for (i = 0; i < RXDESCS; i++) {
		if (sc->sc_rxmbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_rcvmap[i]);
			m_freem(sc->sc_rxmbuf[i]);
		}
	}
 fail_5:
	for (i = 0; i < TXDESCS; i++) {
		if (sc->sc_xmtmap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_xmtmap[i]);
	}
 fail_4:
	for (i = 0; i < RXDESCS; i++) {
		if (sc->sc_rcvmap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_rcvmap[i]);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cmap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cmap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_zedata,
	    sizeof(struct ze_cdata));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * Initialization of interface.
 */
void
zeinit(struct ze_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ze_cdata *zc = sc->sc_zedata;
	int i;

	/*
	 * Reset the interface.
	 */
	if (zereset(sc))
		return;

	sc->sc_nexttx = sc->sc_inq = sc->sc_lastack = sc->sc_txcnt = 0;
	/*
	 * Release and init transmit descriptors.
	 */
	for (i = 0; i < TXDESCS; i++) {
		if (sc->sc_xmtmap[i]->dm_nsegs > 0)
			bus_dmamap_unload(sc->sc_dmat, sc->sc_xmtmap[i]);
		if (sc->sc_txmbuf[i]) {
			m_freem(sc->sc_txmbuf[i]);
			sc->sc_txmbuf[i] = 0;
		}
		zc->zc_xmit[i].ze_tdr = 0; /* Clear valid bit */
	}


	/*
	 * Init receive descriptors.
	 */
	for (i = 0; i < RXDESCS; i++)
		zc->zc_recv[i].ze_framelen = ZE_FRAMELEN_OW;
	sc->sc_nextrx = 0;

	ZE_WCSR(ZE_CSR6, ZE_NICSR6_IE|ZE_NICSR6_BL_8|ZE_NICSR6_ST|
	    ZE_NICSR6_SR|ZE_NICSR6_DC);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Send a setup frame.
	 * This will start the transmit machinery as well.
	 */
	ze_setup(sc);

}

/*
 * Start output on interface.
 */
void
zestart(struct ifnet *ifp)
{
	struct ze_softc *sc = ifp->if_softc;
	struct ze_cdata *zc = sc->sc_zedata;
	paddr_t	buffer;
	struct mbuf *m;
	int nexttx, starttx;
	int len, i, totlen, error;
	int old_inq = sc->sc_inq;
	uint16_t orword, tdr = 0;
	bus_dmamap_t map;

	while (sc->sc_inq < (TXDESCS - 1)) {

		if (sc->sc_setup) {
			ze_setup(sc);
			continue;
		}
		nexttx = sc->sc_nexttx;
		IFQ_POLL(&sc->sc_if.if_snd, m);
		if (m == 0)
			goto out;
		/*
		 * Count number of mbufs in chain.
		 * Always do DMA directly from mbufs, therefore the transmit
		 * ring is really big.
		 */
		map = sc->sc_xmtmap[nexttx];
		error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
		    BUS_DMA_WRITE);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "zestart: load_mbuf failed: %d", error);
			goto out;
		}

		if (map->dm_nsegs >= TXDESCS)
			panic("zestart"); /* XXX */

		if ((map->dm_nsegs + sc->sc_inq) >= (TXDESCS - 1)) {
			bus_dmamap_unload(sc->sc_dmat, map);
			ifp->if_flags |= IFF_OACTIVE;
			goto out;
		}

		/*
		 * m now points to a mbuf chain that can be loaded.
		 * Loop around and set it.
		 */
		totlen = 0;
		orword = ZE_TDES1_FS;
		starttx = nexttx;
		for (i = 0; i < map->dm_nsegs; i++) {
			buffer = map->dm_segs[i].ds_addr;
			len = map->dm_segs[i].ds_len;

			KASSERT(len > 0);

			totlen += len;
			/* Word alignment calc */
			if (totlen == m->m_pkthdr.len) {
				sc->sc_txcnt += map->dm_nsegs;
				if (sc->sc_txcnt >= TXDESCS * 3 / 4) {
					orword |= ZE_TDES1_IC;
					sc->sc_txcnt = 0;
				}
				orword |= ZE_TDES1_LS;
				sc->sc_txmbuf[nexttx] = m;
			}
			zc->zc_xmit[nexttx].ze_bufsize = len;
			zc->zc_xmit[nexttx].ze_bufaddr = (char *)buffer;
			zc->zc_xmit[nexttx].ze_tdes1 = orword;
			zc->zc_xmit[nexttx].ze_tdr = tdr;

			if (++nexttx == TXDESCS)
				nexttx = 0;
			orword = 0;
			tdr = ZE_TDR_OW;
		}

		sc->sc_inq += map->dm_nsegs;

		IFQ_DEQUEUE(&ifp->if_snd, m);
#ifdef DIAGNOSTIC
		if (totlen != m->m_pkthdr.len)
			panic("zestart: len fault");
#endif
		/*
		 * Turn ownership of the packet over to the device.
		 */
		zc->zc_xmit[starttx].ze_tdr = ZE_TDR_OW;

		/*
		 * Kick off the transmit logic, if it is stopped.
		 */
		if ((ZE_RCSR(ZE_CSR5) & ZE_NICSR5_TS) != ZE_NICSR5_TS_RUN)
			ZE_WCSR(ZE_CSR1, -1);
		sc->sc_nexttx = nexttx;
	}
	if (sc->sc_inq == (TXDESCS - 1))
		ifp->if_flags |= IFF_OACTIVE;

out:	if (old_inq < sc->sc_inq)
		ifp->if_timer = 5; /* If transmit logic dies */
}

int
sgec_intr(struct ze_softc *sc)
{
	struct ze_cdata *zc = sc->sc_zedata;
	struct ifnet *ifp = &sc->sc_if;
	struct mbuf *m;
	int csr, len;

	csr = ZE_RCSR(ZE_CSR5);
	if ((csr & ZE_NICSR5_IS) == 0) { /* Wasn't we */
		sc->sc_nointrcnt.ev_count++;
		return 0;
	}
	ZE_WCSR(ZE_CSR5, csr);

	if (csr & ZE_NICSR5_RU)
		sc->sc_nobufintrcnt.ev_count++;

	if (csr & ZE_NICSR5_RI) {
		sc->sc_rxintrcnt.ev_count++;
		while ((zc->zc_recv[sc->sc_nextrx].ze_framelen &
		    ZE_FRAMELEN_OW) == 0) {

			ifp->if_ipackets++;
			m = sc->sc_rxmbuf[sc->sc_nextrx];
			len = zc->zc_recv[sc->sc_nextrx].ze_framelen;
			ze_add_rxbuf(sc, sc->sc_nextrx);
			if (++sc->sc_nextrx == RXDESCS)
				sc->sc_nextrx = 0;
			if (len < ETHER_MIN_LEN) {
				ifp->if_ierrors++;
				m_freem(m);
			} else {
				m->m_pkthdr.rcvif = ifp;
				m->m_pkthdr.len = m->m_len =
				    len - ETHER_CRC_LEN;
				bpf_mtap(ifp, m);
				(*ifp->if_input)(ifp, m);
			}
		}
	}

	if (csr & ZE_NICSR5_TI)
		sc->sc_txintrcnt.ev_count++;
	if (sc->sc_lastack != sc->sc_nexttx) {
		int lastack;
		for (lastack = sc->sc_lastack; lastack != sc->sc_nexttx; ) {
			bus_dmamap_t map;
			int nlastack;

			if ((zc->zc_xmit[lastack].ze_tdr & ZE_TDR_OW) != 0)
				break;

			if ((zc->zc_xmit[lastack].ze_tdes1 & ZE_TDES1_DT) ==
			    ZE_TDES1_DT_SETUP) {
				if (++lastack == TXDESCS)
					lastack = 0;
				sc->sc_inq--;
				continue;
			}

			KASSERT(zc->zc_xmit[lastack].ze_tdes1 & ZE_TDES1_FS);
			map = sc->sc_xmtmap[lastack];
			KASSERT(map->dm_nsegs > 0);
			nlastack = (lastack + map->dm_nsegs - 1) % TXDESCS;
			if (zc->zc_xmit[nlastack].ze_tdr & ZE_TDR_OW)
				break;
			lastack = nlastack;
			if (sc->sc_txcnt > map->dm_nsegs)
			    sc->sc_txcnt -= map->dm_nsegs;
			else
			    sc->sc_txcnt = 0;
			sc->sc_inq -= map->dm_nsegs;
			KASSERT(zc->zc_xmit[lastack].ze_tdes1 & ZE_TDES1_LS);
			ifp->if_opackets++;
			bus_dmamap_unload(sc->sc_dmat, map);
			KASSERT(sc->sc_txmbuf[lastack]);
			bpf_mtap(ifp, sc->sc_txmbuf[lastack]);
			m_freem(sc->sc_txmbuf[lastack]);
			sc->sc_txmbuf[lastack] = 0;
			if (++lastack == TXDESCS)
				lastack = 0;
		}
		if (lastack != sc->sc_lastack) {
			sc->sc_txdraincnt.ev_count++;
			sc->sc_lastack = lastack;
			if (sc->sc_inq == 0)
				ifp->if_timer = 0;
			ifp->if_flags &= ~IFF_OACTIVE;
			zestart(ifp); /* Put in more in queue */
		}
	}
	return 1;
}

/*
 * Process an ioctl request.
 */
int
zeioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ze_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = data;
	int s = splnet(), error = 0;

	switch (cmd) {

	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			zeinit(sc);
			arp_ifinit(ifp, ifa);
			break;
#endif
		}
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		/* XXX re-use ether_ioctl() */
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			/*
			 * If interface is marked down and it is running,
			 * stop it. (by disabling receive mechanism).
			 */
			ZE_WCSR(ZE_CSR6, ZE_RCSR(ZE_CSR6) &
			    ~(ZE_NICSR6_ST|ZE_NICSR6_SR));
			ifp->if_flags &= ~IFF_RUNNING;
			break;
		case IFF_UP:
			/*
			 * If interface it marked up and it is stopped, then
			 * start it.
			 */
			zeinit(sc);
			break;
		case IFF_UP|IFF_RUNNING:
			/*
			 * Send a new setup packet to match any new changes.
			 * (Like IFF_PROMISC etc)
			 */
			ze_setup(sc);
			break;
		case 0:
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * Update our multicast list.
		 */
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				ze_setup(sc);
			error = 0;
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);

	}
	splx(s);
	return (error);
}

/*
 * Add a receive buffer to the indicated descriptor.
 */
int
ze_add_rxbuf(struct ze_softc *sc, int i)
{
	struct mbuf *m;
	struct ze_rdes *rp;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLAIM(m, &sc->sc_ec.ec_rx_mowner);
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (sc->sc_rxmbuf[i] != NULL)
		bus_dmamap_unload(sc->sc_dmat, sc->sc_rcvmap[i]);

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_rcvmap[i],
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error)
		panic("%s: can't load rx DMA map %d, error = %d",
		    device_xname(sc->sc_dev), i, error);
	sc->sc_rxmbuf[i] = m;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_rcvmap[i], 0,
	    sc->sc_rcvmap[i]->dm_mapsize, BUS_DMASYNC_PREREAD);

	/*
	 * We know that the mbuf cluster is page aligned. Also, be sure
	 * that the IP header will be longword aligned.
	 */
	m->m_data += 2;
	rp = &sc->sc_zedata->zc_recv[i];
	rp->ze_bufsize = (m->m_ext.ext_size - 2);
	rp->ze_bufaddr = (char *)sc->sc_rcvmap[i]->dm_segs[0].ds_addr + 2;
	rp->ze_framelen = ZE_FRAMELEN_OW;

	return (0);
}

/*
 * Create a setup packet and put in queue for sending.
 */
void
ze_setup(struct ze_softc *sc)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	struct ze_cdata *zc = sc->sc_zedata;
	struct ifnet *ifp = &sc->sc_if;
	const u_int8_t *enaddr = CLLADDR(ifp->if_sadl);
	int j, idx, reg;

	if (sc->sc_inq == (TXDESCS - 1)) {
		sc->sc_setup = 1;
		return;
	}
	sc->sc_setup = 0;
	/*
	 * Init the setup packet with valid info.
	 */
	memset(zc->zc_setup, 0xff, sizeof(zc->zc_setup)); /* Broadcast */
	memcpy(zc->zc_setup, enaddr, ETHER_ADDR_LEN);

	/*
	 * Multicast handling. The SGEC can handle up to 16 direct
	 * ethernet addresses.
	 */
	j = 16;
	ifp->if_flags &= ~IFF_ALLMULTI;
	ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6)) {
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}
		memcpy(&zc->zc_setup[j], enm->enm_addrlo, ETHER_ADDR_LEN);
		j += 8;
		ETHER_NEXT_MULTI(step, enm);
		if ((enm != NULL)&& (j == 128)) {
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}
	}

	/*
	 * ALLMULTI implies PROMISC in this driver.
	 */
	if (ifp->if_flags & IFF_ALLMULTI)
		ifp->if_flags |= IFF_PROMISC;
	else if (ifp->if_pcount == 0)
		ifp->if_flags &= ~IFF_PROMISC;

	/*
	 * Fiddle with the receive logic.
	 */
	reg = ZE_RCSR(ZE_CSR6);
	DELAY(10);
	ZE_WCSR(ZE_CSR6, reg & ~ZE_NICSR6_SR); /* Stop rx */
	reg &= ~ZE_NICSR6_AF;
	if (ifp->if_flags & IFF_PROMISC)
		reg |= ZE_NICSR6_AF_PROM;
	else if (ifp->if_flags & IFF_ALLMULTI)
		reg |= ZE_NICSR6_AF_ALLM;
	DELAY(10);
	ZE_WCSR(ZE_CSR6, reg);
	/*
	 * Only send a setup packet if needed.
	 */
	if ((ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)) == 0) {
		idx = sc->sc_nexttx;
		zc->zc_xmit[idx].ze_tdes1 = ZE_TDES1_DT_SETUP;
		zc->zc_xmit[idx].ze_bufsize = 128;
		zc->zc_xmit[idx].ze_bufaddr = sc->sc_pzedata->zc_setup;
		zc->zc_xmit[idx].ze_tdr = ZE_TDR_OW;

		if ((ZE_RCSR(ZE_CSR5) & ZE_NICSR5_TS) != ZE_NICSR5_TS_RUN)
			ZE_WCSR(ZE_CSR1, -1);

		sc->sc_inq++;
		if (++sc->sc_nexttx == TXDESCS)
			sc->sc_nexttx = 0;
	}
}

/*
 * Check for dead transmit logic.
 */
void
zetimeout(struct ifnet *ifp)
{
	struct ze_softc *sc = ifp->if_softc;

	if (sc->sc_inq == 0)
		return;

	aprint_error_dev(sc->sc_dev, "xmit logic died, resetting...\n");
	/*
	 * Do a reset of interface, to get it going again.
	 * Will it work by just restart the transmit logic?
	 */
	zeinit(sc);
}

/*
 * Reset chip:
 * Set/reset the reset flag.
 *  Write interrupt vector.
 *  Write ring buffer addresses.
 *  Write SBR.
 */
bool
zereset(struct ze_softc *sc)
{
	int reg, i;

	ZE_WCSR(ZE_CSR6, ZE_NICSR6_RE);
	DELAY(50000);
	if (ZE_RCSR(ZE_CSR6) & ZE_NICSR5_SF) {
		aprint_error_dev(sc->sc_dev, "selftest failed\n");
		return true;
	}

	/*
	 * Get the vector that were set at match time, and remember it.
	 * WHICH VECTOR TO USE? Take one unused. XXX
	 * Funny way to set vector described in the programmers manual.
	 */
	reg = ZE_NICSR0_IPL14 | sc->sc_intvec | 0x1fff0003; /* SYNC/ASYNC??? */
	i = 10;
	do {
		if (i-- == 0) {
			aprint_error_dev(sc->sc_dev,
			    "failing SGEC CSR0 init\n");
			return true;
		}
		ZE_WCSR(ZE_CSR0, reg);
	} while (ZE_RCSR(ZE_CSR0) != reg);

	ZE_WCSR(ZE_CSR3, (vaddr_t)sc->sc_pzedata->zc_recv);
	ZE_WCSR(ZE_CSR4, (vaddr_t)sc->sc_pzedata->zc_xmit);
	return false;
}
