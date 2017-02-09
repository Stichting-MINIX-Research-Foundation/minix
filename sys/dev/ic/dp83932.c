/*	$NetBSD: dp83932.c,v 1.36 2013/10/25 21:29:28 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * Device driver for the National Semiconductor DP83932
 * Systems-Oriented Network Interface Controller (SONIC).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dp83932.c,v 1.36 2013/10/25 21:29:28 martin Exp $");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/dp83932reg.h>
#include <dev/ic/dp83932var.h>

static void	sonic_start(struct ifnet *);
static void	sonic_watchdog(struct ifnet *);
static int	sonic_ioctl(struct ifnet *, u_long, void *);
static int	sonic_init(struct ifnet *);
static void	sonic_stop(struct ifnet *, int);

static bool	sonic_shutdown(device_t, int);

static void	sonic_reset(struct sonic_softc *);
static void	sonic_rxdrain(struct sonic_softc *);
static int	sonic_add_rxbuf(struct sonic_softc *, int);
static void	sonic_set_filter(struct sonic_softc *);

static uint16_t sonic_txintr(struct sonic_softc *);
static void	sonic_rxintr(struct sonic_softc *);

int	sonic_copy_small = 0;

#define ETHER_PAD_LEN (ETHER_MIN_LEN - ETHER_CRC_LEN)

/*
 * sonic_attach:
 *
 *	Attach a SONIC interface to the system.
 */
void
sonic_attach(struct sonic_softc *sc, const uint8_t *enaddr)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int i, rseg, error;
	bus_dma_segment_t seg;
	size_t cdatasize;
	uint8_t *nullbuf;

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if (sc->sc_32bit)
		cdatasize = sizeof(struct sonic_control_data32);
	else
		cdatasize = sizeof(struct sonic_control_data16);

	if ((error = bus_dmamem_alloc(sc->sc_dmat, cdatasize + ETHER_PAD_LEN,
	     PAGE_SIZE, (64 * 1024), &seg, 1, &rseg,
	     BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate control data, error = %d\n", error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    cdatasize + ETHER_PAD_LEN, (void **) &sc->sc_cdata16,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map control data, error = %d\n", error);
		goto fail_1;
	}
	nullbuf = (uint8_t *)sc->sc_cdata16 + cdatasize;
	memset(nullbuf, 0, ETHER_PAD_LEN);

	if ((error = bus_dmamap_create(sc->sc_dmat,
	     cdatasize, 1, cdatasize, 0, BUS_DMA_NOWAIT,
	     &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create control data DMA map, error = %d\n",
		    error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	     sc->sc_cdata16, cdatasize, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load control data DMA map, error = %d\n", error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < SONIC_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		     SONIC_NTXFRAGS, MCLBYTES, 0, BUS_DMA_NOWAIT,
		     &sc->sc_txsoft[i].ds_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create tx DMA map %d, error = %d\n",
			    i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < SONIC_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		     MCLBYTES, 0, BUS_DMA_NOWAIT,
		     &sc->sc_rxsoft[i].ds_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create rx DMA map %d, error = %d\n",
			    i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].ds_mbuf = NULL;
	}

	/*
	 * create and map the pad buffer
	 */
	if ((error = bus_dmamap_create(sc->sc_dmat, ETHER_PAD_LEN, 1,
	    ETHER_PAD_LEN, 0, BUS_DMA_NOWAIT, &sc->sc_nulldmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create pad buffer DMA map, error = %d\n", error);
		goto fail_5;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_nulldmamap,
	    nullbuf, ETHER_PAD_LEN, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load pad buffer DMA map, error = %d\n", error);
		goto fail_6;
	}
	bus_dmamap_sync(sc->sc_dmat, sc->sc_nulldmamap, 0, ETHER_PAD_LEN,
	    BUS_DMASYNC_PREWRITE);

	/*
	 * Reset the chip to a known state.
	 */
	sonic_reset(sc);

	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(enaddr));

	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sonic_ioctl;
	ifp->if_start = sonic_start;
	ifp->if_watchdog = sonic_watchdog;
	ifp->if_init = sonic_init;
	ifp->if_stop = sonic_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	if (pmf_device_register1(sc->sc_dev, NULL, NULL, sonic_shutdown))
		pmf_class_network_register(sc->sc_dev, ifp);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_6:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_nulldmamap);
 fail_5:
	for (i = 0; i < SONIC_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].ds_dmamap);
	}
 fail_4:
	for (i = 0; i < SONIC_NTXDESC; i++) {
		if (sc->sc_txsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].ds_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_cdata16, cdatasize);
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * sonic_shutdown:
 *
 *	Make sure the interface is stopped at reboot.
 */
bool
sonic_shutdown(device_t self, int howto)
{
	struct sonic_softc *sc = device_private(self);

	sonic_stop(&sc->sc_ethercom.ec_if, 1);

	return true;
}

/*
 * sonic_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
void
sonic_start(struct ifnet *ifp)
{
	struct sonic_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct sonic_tda16 *tda16;
	struct sonic_tda32 *tda32;
	struct sonic_descsoft *ds;
	bus_dmamap_t dmamap;
	int error, olasttx, nexttx, opending, totlen, olseg;
	int seg = 0;	/* XXX: gcc */

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous txpending and the current "last txdesc
	 * used" index.
	 */
	opending = sc->sc_txpending;
	olasttx = sc->sc_txlast;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.  Leave one at the end for sanity's sake.
	 */
	while (sc->sc_txpending < (SONIC_NTXDESC - 1)) {
		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		/*
		 * Get the next available transmit descriptor.
		 */
		nexttx = SONIC_NEXTTX(sc->sc_txlast);
		ds = &sc->sc_txsoft[nexttx];
		dmamap = ds->ds_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the allotted number of frags, or we were
		 * short on resources.  In this case, we'll copy and try
		 * again.
		 */
		if ((error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT)) != 0 ||
		    (m0->m_pkthdr.len < ETHER_PAD_LEN &&
		    dmamap->dm_nsegs == SONIC_NTXFRAGS)) {
			if (error == 0)
				bus_dmamap_unload(sc->sc_dmat, dmamap);
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    device_xname(sc->sc_dev));
				break;
			}
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n",
					    device_xname(sc->sc_dev));
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap,
			    m, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			if (error) {
				printf("%s: unable to load Tx buffer, "
				    "error = %d\n", device_xname(sc->sc_dev),
				    error);
				m_freem(m);
				break;
			}
		}
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m != NULL) {
			m_freem(m0);
			m0 = m;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Store a pointer to the packet so we can free it later.
		 */
		ds->ds_mbuf = m0;

		/*
		 * Initialize the transmit descriptor.
		 */
		totlen = 0;
		if (sc->sc_32bit) {
			tda32 = &sc->sc_tda32[nexttx];
			for (seg = 0; seg < dmamap->dm_nsegs; seg++) {
				tda32->tda_frags[seg].frag_ptr1 =
				    htosonic32(sc,
				    (dmamap->dm_segs[seg].ds_addr >> 16) &
				    0xffff);
				tda32->tda_frags[seg].frag_ptr0 =
				    htosonic32(sc,
				    dmamap->dm_segs[seg].ds_addr & 0xffff);
				tda32->tda_frags[seg].frag_size =
				    htosonic32(sc, dmamap->dm_segs[seg].ds_len);
				totlen += dmamap->dm_segs[seg].ds_len;
			}
			if (totlen < ETHER_PAD_LEN) {
				tda32->tda_frags[seg].frag_ptr1 =
				    htosonic32(sc,
				    (sc->sc_nulldma >> 16) & 0xffff);
				tda32->tda_frags[seg].frag_ptr0 =
				    htosonic32(sc, sc->sc_nulldma & 0xffff);
				tda32->tda_frags[seg].frag_size =
				    htosonic32(sc, ETHER_PAD_LEN - totlen);
				totlen = ETHER_PAD_LEN;
				seg++;
			}

			tda32->tda_status = 0;
			tda32->tda_pktconfig = 0;
			tda32->tda_pktsize = htosonic32(sc, totlen);
			tda32->tda_fragcnt = htosonic32(sc, seg);

			/* Link it up. */
			tda32->tda_frags[seg].frag_ptr0 =
			    htosonic32(sc, SONIC_CDTXADDR32(sc,
			    SONIC_NEXTTX(nexttx)) & 0xffff);

			/* Sync the Tx descriptor. */
			SONIC_CDTXSYNC32(sc, nexttx,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		} else {
			tda16 = &sc->sc_tda16[nexttx];
			for (seg = 0; seg < dmamap->dm_nsegs; seg++) {
				tda16->tda_frags[seg].frag_ptr1 =
				    htosonic16(sc,
				    (dmamap->dm_segs[seg].ds_addr >> 16) &
				    0xffff);
				tda16->tda_frags[seg].frag_ptr0 =
				    htosonic16(sc,
				    dmamap->dm_segs[seg].ds_addr & 0xffff);
				tda16->tda_frags[seg].frag_size =
				    htosonic16(sc, dmamap->dm_segs[seg].ds_len);
				totlen += dmamap->dm_segs[seg].ds_len;
			}
			if (totlen < ETHER_PAD_LEN) {
				tda16->tda_frags[seg].frag_ptr1 =
				    htosonic16(sc,
				    (sc->sc_nulldma >> 16) & 0xffff);
				tda16->tda_frags[seg].frag_ptr0 =
				    htosonic16(sc, sc->sc_nulldma & 0xffff);
				tda16->tda_frags[seg].frag_size =
				    htosonic16(sc, ETHER_PAD_LEN - totlen);
				totlen = ETHER_PAD_LEN;
				seg++;
			}

			tda16->tda_status = 0;
			tda16->tda_pktconfig = 0;
			tda16->tda_pktsize = htosonic16(sc, totlen);
			tda16->tda_fragcnt = htosonic16(sc, seg);

			/* Link it up. */
			tda16->tda_frags[seg].frag_ptr0 =
			    htosonic16(sc, SONIC_CDTXADDR16(sc,
			    SONIC_NEXTTX(nexttx)) & 0xffff);

			/* Sync the Tx descriptor. */
			SONIC_CDTXSYNC16(sc, nexttx,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}

		/* Advance the Tx pointer. */
		sc->sc_txpending++;
		sc->sc_txlast = nexttx;

		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap(ifp, m0);
	}

	if (sc->sc_txpending == (SONIC_NTXDESC - 1)) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txpending != opending) {
		/*
		 * We enqueued packets.  If the transmitter was idle,
		 * reset the txdirty pointer.
		 */
		if (opending == 0)
			sc->sc_txdirty = SONIC_NEXTTX(olasttx);

		/*
		 * Stop the SONIC on the last packet we've set up,
		 * and clear end-of-list on the descriptor previous
		 * to our new chain.
		 *
		 * NOTE: our `seg' variable should still be valid!
		 */
		if (sc->sc_32bit) {
			olseg =
			    sonic32toh(sc, sc->sc_tda32[olasttx].tda_fragcnt);
			sc->sc_tda32[sc->sc_txlast].tda_frags[seg].frag_ptr0 |=
			    htosonic32(sc, TDA_LINK_EOL);
			SONIC_CDTXSYNC32(sc, sc->sc_txlast,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			sc->sc_tda32[olasttx].tda_frags[olseg].frag_ptr0 &=
			    htosonic32(sc, ~TDA_LINK_EOL);
			SONIC_CDTXSYNC32(sc, olasttx,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		} else {
			olseg =
			    sonic16toh(sc, sc->sc_tda16[olasttx].tda_fragcnt);
			sc->sc_tda16[sc->sc_txlast].tda_frags[seg].frag_ptr0 |=
			    htosonic16(sc, TDA_LINK_EOL);
			SONIC_CDTXSYNC16(sc, sc->sc_txlast,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			sc->sc_tda16[olasttx].tda_frags[olseg].frag_ptr0 &=
			    htosonic16(sc, ~TDA_LINK_EOL);
			SONIC_CDTXSYNC16(sc, olasttx,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}

		/* Start the transmitter. */
		CSR_WRITE(sc, SONIC_CR, CR_TXP);

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * sonic_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
void
sonic_watchdog(struct ifnet *ifp)
{
	struct sonic_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", device_xname(sc->sc_dev));
	ifp->if_oerrors++;

	(void)sonic_init(ifp);
}

/*
 * sonic_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
int
sonic_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int s, error;

	s = splnet();

	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		/*
		 * Multicast list has changed; set the hardware
		 * filter accordingly.
		 */
		if (ifp->if_flags & IFF_RUNNING)
			(void)sonic_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

/*
 * sonic_intr:
 *
 *	Interrupt service routine.
 */
int
sonic_intr(void *arg)
{
	struct sonic_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint16_t isr;
	int handled = 0, wantinit;

	for (wantinit = 0; wantinit == 0;) {
		isr = CSR_READ(sc, SONIC_ISR) & sc->sc_imr;
		if (isr == 0)
			break;
		CSR_WRITE(sc, SONIC_ISR, isr);	/* ACK */

		handled = 1;

		if (isr & IMR_PRX)
			sonic_rxintr(sc);

		if (isr & (IMR_PTX|IMR_TXER)) {
			if (sonic_txintr(sc) & TCR_FU) {
				printf("%s: transmit FIFO underrun\n",
				    device_xname(sc->sc_dev));
				wantinit = 1;
			}
		}

		if (isr & (IMR_RFO|IMR_RBA|IMR_RBE|IMR_RDE)) {
#define	PRINTERR(bit, str)						\
			if (isr & (bit))				\
				printf("%s: %s\n",device_xname(sc->sc_dev), str)
			PRINTERR(IMR_RFO, "receive FIFO overrun");
			PRINTERR(IMR_RBA, "receive buffer exceeded");
			PRINTERR(IMR_RBE, "receive buffers exhausted");
			PRINTERR(IMR_RDE, "receive descriptors exhausted");
			wantinit = 1;
		}
	}

	if (handled) {
		if (wantinit)
			(void)sonic_init(ifp);
		sonic_start(ifp);
	}

	return handled;
}

/*
 * sonic_txintr:
 *
 *	Helper; handle transmit complete interrupts.
 */
uint16_t
sonic_txintr(struct sonic_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct sonic_descsoft *ds;
	struct sonic_tda32 *tda32;
	struct sonic_tda16 *tda16;
	uint16_t status, totstat = 0;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	for (i = sc->sc_txdirty; sc->sc_txpending != 0;
	     i = SONIC_NEXTTX(i), sc->sc_txpending--) {
		ds = &sc->sc_txsoft[i];

		if (sc->sc_32bit) {
			SONIC_CDTXSYNC32(sc, i,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			tda32 = &sc->sc_tda32[i];
			status = sonic32toh(sc, tda32->tda_status);
			SONIC_CDTXSYNC32(sc, i, BUS_DMASYNC_PREREAD);
		} else {
			SONIC_CDTXSYNC16(sc, i,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			tda16 = &sc->sc_tda16[i];
			status = sonic16toh(sc, tda16->tda_status);
			SONIC_CDTXSYNC16(sc, i, BUS_DMASYNC_PREREAD);
		}

		if ((status & ~(TCR_EXDIS|TCR_CRCI|TCR_POWC|TCR_PINT)) == 0)
			break;

		totstat |= status;

		bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
		m_freem(ds->ds_mbuf);
		ds->ds_mbuf = NULL;

		/*
		 * Check for errors and collisions.
		 */
		if (status & TCR_PTX)
			ifp->if_opackets++;
		else
			ifp->if_oerrors++;
		ifp->if_collisions += TDA_STATUS_NCOL(status);
	}

	/* Update the dirty transmit buffer pointer. */
	sc->sc_txdirty = i;

	/*
	 * Cancel the watchdog timer if there are no pending
	 * transmissions.
	 */
	if (sc->sc_txpending == 0)
		ifp->if_timer = 0;

	return totstat;
}

/*
 * sonic_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
void
sonic_rxintr(struct sonic_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct sonic_descsoft *ds;
	struct sonic_rda32 *rda32;
	struct sonic_rda16 *rda16;
	struct mbuf *m;
	int i, len;
	uint16_t status, bytecount /*, ptr0, ptr1, seqno */;

	for (i = sc->sc_rxptr;; i = SONIC_NEXTRX(i)) {
		ds = &sc->sc_rxsoft[i];

		if (sc->sc_32bit) {
			SONIC_CDRXSYNC32(sc, i,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			rda32 = &sc->sc_rda32[i];
			SONIC_CDRXSYNC32(sc, i, BUS_DMASYNC_PREREAD);
			if (rda32->rda_inuse != 0)
				break;
			status = sonic32toh(sc, rda32->rda_status);
			bytecount = sonic32toh(sc, rda32->rda_bytecount);
			/* ptr0 = sonic32toh(sc, rda32->rda_pkt_ptr0); */
			/* ptr1 = sonic32toh(sc, rda32->rda_pkt_ptr1); */
			/* seqno = sonic32toh(sc, rda32->rda_seqno); */
		} else {
			SONIC_CDRXSYNC16(sc, i,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			rda16 = &sc->sc_rda16[i];
			SONIC_CDRXSYNC16(sc, i, BUS_DMASYNC_PREREAD);
			if (rda16->rda_inuse != 0)
				break;
			status = sonic16toh(sc, rda16->rda_status);
			bytecount = sonic16toh(sc, rda16->rda_bytecount);
			/* ptr0 = sonic16toh(sc, rda16->rda_pkt_ptr0); */
			/* ptr1 = sonic16toh(sc, rda16->rda_pkt_ptr1); */
			/* seqno = sonic16toh(sc, rda16->rda_seqno); */
		}

		/*
		 * Make absolutely sure this is the only packet
		 * in this receive buffer.  Our entire Rx buffer
		 * management scheme depends on this, and if the
		 * SONIC didn't follow our rule, it means we've
		 * misconfigured it.
		 */
		KASSERT(status & RCR_LPKT);

		/*
		 * Make sure the packet arrived OK.  If an error occurred,
		 * update stats and reset the descriptor.  The buffer will
		 * be reused the next time the descriptor comes up in the
		 * ring.
		 */
		if ((status & RCR_PRX) == 0) {
			if (status & RCR_FAER)
				printf("%s: Rx frame alignment error\n",
				    device_xname(sc->sc_dev));
			else if (status & RCR_CRCR)
				printf("%s: Rx CRC error\n",
				    device_xname(sc->sc_dev));
			ifp->if_ierrors++;
			SONIC_INIT_RXDESC(sc, i);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/*
		 * The SONIC includes the CRC with every packet.
		 */
		len = bytecount - ETHER_CRC_LEN;

		/*
		 * Ok, if the chip is in 32-bit mode, then receive
		 * buffers must be aligned to 32-bit boundaries,
		 * which means the payload is misaligned.  In this
		 * case, we must allocate a new mbuf, and copy the
		 * packet into it, scooted forward 2 bytes to ensure
		 * proper alignment.
		 *
		 * Note, in 16-bit mode, we can configure the SONIC
		 * to do what we want, and we have.
		 */
#ifndef __NO_STRICT_ALIGNMENT
		if (sc->sc_32bit) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			if (len > (MHLEN - 2)) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0)
					goto dropit;
			}
			m->m_data += 2;
			/*
			 * Note that we use a cluster for incoming frames,
			 * so the buffer is virtually contiguous.
			 */
			memcpy(mtod(m, void *), mtod(ds->ds_mbuf, void *),
			    len);
			SONIC_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
			    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
		} else
#endif /* ! __NO_STRICT_ALIGNMENT */
		/*
		 * If the packet is small enough to fit in a single
		 * header mbuf, allocate one and copy the data into
		 * it.  This greatly reduces memory consumption when
		 * we receive lots of small packets.
		 */
		if (sonic_copy_small != 0 && len <= (MHLEN - 2)) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			m->m_data += 2;
			/*
			 * Note that we use a cluster for incoming frames,
			 * so the buffer is virtually contiguous.
			 */
			memcpy(mtod(m, void *), mtod(ds->ds_mbuf, void *),
			    len);
			SONIC_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
			    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
		} else {
			m = ds->ds_mbuf;
			if (sonic_add_rxbuf(sc, i) != 0) {
 dropit:
				ifp->if_ierrors++;
				SONIC_INIT_RXDESC(sc, i);
				bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
				    ds->ds_dmamap->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/*
		 * Pass this up to any BPF listeners.
		 */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
	CSR_WRITE(sc, SONIC_RWR, SONIC_CDRRADDR(sc, SONIC_PREVRX(i)));
}

/*
 * sonic_reset:
 *
 *	Perform a soft reset on the SONIC.
 */
void
sonic_reset(struct sonic_softc *sc)
{

	/* stop TX, RX and timer, and ensure RST is clear */
	CSR_WRITE(sc, SONIC_CR, CR_STP | CR_RXDIS | CR_HTX);
	delay(1000);

	CSR_WRITE(sc, SONIC_CR, CR_RST);
	delay(1000);

	/* clear all interrupts */
	CSR_WRITE(sc, SONIC_IMR, 0);
	CSR_WRITE(sc, SONIC_ISR, IMR_ALL);

	CSR_WRITE(sc, SONIC_CR, 0);
	delay(1000);
}

/*
 * sonic_init:		[ifnet interface function]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
int
sonic_init(struct ifnet *ifp)
{
	struct sonic_softc *sc = ifp->if_softc;
	struct sonic_descsoft *ds;
	int i, error = 0;
	uint16_t reg;

	/*
	 * Cancel any pending I/O.
	 */
	sonic_stop(ifp, 0);

	/*
	 * Reset the SONIC to a known state.
	 */
	sonic_reset(sc);

	/*
	 * Bring the SONIC into reset state, and program the DCR.
	 *
	 * Note: We don't bother optimizing the transmit and receive
	 * thresholds, here. TFT/RFT values should be set in MD attachments.
	 */
	reg = sc->sc_dcr;
	if (sc->sc_32bit)
		reg |= DCR_DW;
	CSR_WRITE(sc, SONIC_CR, CR_RST);
	CSR_WRITE(sc, SONIC_DCR, reg);
	CSR_WRITE(sc, SONIC_DCR2, sc->sc_dcr2);
	CSR_WRITE(sc, SONIC_CR, 0);

	/*
	 * Initialize the transmit descriptors.
	 */
	if (sc->sc_32bit) {
		for (i = 0; i < SONIC_NTXDESC; i++) {
			memset(&sc->sc_tda32[i], 0, sizeof(struct sonic_tda32));
			SONIC_CDTXSYNC32(sc, i,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}
	} else {
		for (i = 0; i < SONIC_NTXDESC; i++) {
			memset(&sc->sc_tda16[i], 0, sizeof(struct sonic_tda16));
			SONIC_CDTXSYNC16(sc, i,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		}
	}
	sc->sc_txpending = 0;
	sc->sc_txdirty = 0;
	sc->sc_txlast = SONIC_NTXDESC - 1;

	/*
	 * Initialize the receive descriptor ring.
	 */
	for (i = 0; i < SONIC_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf == NULL) {
			if ((error = sonic_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map Rx "
				    "buffer %d, error = %d\n",
				    device_xname(sc->sc_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				sonic_rxdrain(sc);
				goto out;
			}
		} else
			SONIC_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;

	/* Give the transmit ring to the SONIC. */
	CSR_WRITE(sc, SONIC_UTDAR, (SONIC_CDTXADDR(sc, 0) >> 16) & 0xffff);
	CSR_WRITE(sc, SONIC_CTDAR, SONIC_CDTXADDR(sc, 0) & 0xffff);

	/* Give the receive descriptor ring to the SONIC. */
	CSR_WRITE(sc, SONIC_URDAR, (SONIC_CDRXADDR(sc, 0) >> 16) & 0xffff);
	CSR_WRITE(sc, SONIC_CRDAR, SONIC_CDRXADDR(sc, 0) & 0xffff);

	/* Give the receive buffer ring to the SONIC. */
	CSR_WRITE(sc, SONIC_URRAR, (SONIC_CDRRADDR(sc, 0) >> 16) & 0xffff);
	CSR_WRITE(sc, SONIC_RSAR, SONIC_CDRRADDR(sc, 0) & 0xffff);
	if (sc->sc_32bit)
		CSR_WRITE(sc, SONIC_REAR,
		    (SONIC_CDRRADDR(sc, SONIC_NRXDESC - 1) +
		    sizeof(struct sonic_rra32)) & 0xffff);
	else
		CSR_WRITE(sc, SONIC_REAR,
		    (SONIC_CDRRADDR(sc, SONIC_NRXDESC - 1) +
		    sizeof(struct sonic_rra16)) & 0xffff);
	CSR_WRITE(sc, SONIC_RRR, SONIC_CDRRADDR(sc, 0) & 0xffff);
	CSR_WRITE(sc, SONIC_RWR, SONIC_CDRRADDR(sc, SONIC_NRXDESC - 1));

	/*
	 * Set the End-Of-Buffer counter such that only one packet
	 * will be placed into each buffer we provide.  Note we are
	 * following the recommendation of section 3.4.4 of the manual
	 * here, and have "lengthened" the receive buffers accordingly.
	 */
	if (sc->sc_32bit)
		CSR_WRITE(sc, SONIC_EOBC, (ETHER_MAX_LEN + 2) / 2);
	else
		CSR_WRITE(sc, SONIC_EOBC, (ETHER_MAX_LEN / 2));

	/* Reset the receive sequence counter. */
	CSR_WRITE(sc, SONIC_RSC, 0);

	/* Clear the tally registers. */
	CSR_WRITE(sc, SONIC_CRCETC, 0xffff);
	CSR_WRITE(sc, SONIC_FAET, 0xffff);
	CSR_WRITE(sc, SONIC_MPT, 0xffff);

	/* Set the receive filter. */
	sonic_set_filter(sc);

	/*
	 * Set the interrupt mask register.
	 */
	sc->sc_imr = IMR_RFO | IMR_RBA | IMR_RBE | IMR_RDE |
	    IMR_TXER | IMR_PTX | IMR_PRX;
	CSR_WRITE(sc, SONIC_IMR, sc->sc_imr);

	/*
	 * Start the receive process in motion.  Note, we don't
	 * start the transmit process until we actually try to
	 * transmit packets.
	 */
	CSR_WRITE(sc, SONIC_CR, CR_RXEN | CR_RRRA);

	/*
	 * ...all done!
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

 out:
	if (error)
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	return error;
}

/*
 * sonic_rxdrain:
 *
 *	Drain the receive queue.
 */
void
sonic_rxdrain(struct sonic_softc *sc)
{
	struct sonic_descsoft *ds;
	int i;

	for (i = 0; i < SONIC_NRXDESC; i++) {
		ds = &sc->sc_rxsoft[i];
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}
}

/*
 * sonic_stop:		[ifnet interface function]
 *
 *	Stop transmission on the interface.
 */
void
sonic_stop(struct ifnet *ifp, int disable)
{
	struct sonic_softc *sc = ifp->if_softc;
	struct sonic_descsoft *ds;
	int i;

	/*
	 * Disable interrupts.
	 */
	CSR_WRITE(sc, SONIC_IMR, 0);

	/*
	 * Stop the transmitter, receiver, and timer.
	 */
	CSR_WRITE(sc, SONIC_CR, CR_HTX|CR_RXDIS|CR_STP);
	for (i = 0; i < 1000; i++) {
		if ((CSR_READ(sc, SONIC_CR) & (CR_TXP|CR_RXEN|CR_ST)) == 0)
			break;
		delay(2);
	}
	if ((CSR_READ(sc, SONIC_CR) & (CR_TXP|CR_RXEN|CR_ST)) != 0)
		printf("%s: SONIC failed to stop\n", device_xname(sc->sc_dev));

	/*
	 * Release any queued transmit buffers.
	 */
	for (i = 0; i < SONIC_NTXDESC; i++) {
		ds = &sc->sc_txsoft[i];
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		sonic_rxdrain(sc);
}

/*
 * sonic_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
int
sonic_add_rxbuf(struct sonic_softc *sc, int idx)
{
	struct sonic_descsoft *ds = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return ENOBUFS;
	}

	if (ds->ds_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, ds->ds_dmamap);

	ds->ds_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, ds->ds_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    device_xname(sc->sc_dev), idx, error);
		panic("sonic_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, ds->ds_dmamap, 0,
	    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	SONIC_INIT_RXDESC(sc, idx);

	return 0;
}

static void
sonic_set_camentry(struct sonic_softc *sc, int entry, const uint8_t *enaddr)
{

	if (sc->sc_32bit) {
		struct sonic_cda32 *cda = &sc->sc_cda32[entry];

		cda->cda_entry = htosonic32(sc, entry);
		cda->cda_addr0 = htosonic32(sc, enaddr[0] | (enaddr[1] << 8));
		cda->cda_addr1 = htosonic32(sc, enaddr[2] | (enaddr[3] << 8));
		cda->cda_addr2 = htosonic32(sc, enaddr[4] | (enaddr[5] << 8));
	} else {
		struct sonic_cda16 *cda = &sc->sc_cda16[entry];

		cda->cda_entry = htosonic16(sc, entry);
		cda->cda_addr0 = htosonic16(sc, enaddr[0] | (enaddr[1] << 8));
		cda->cda_addr1 = htosonic16(sc, enaddr[2] | (enaddr[3] << 8));
		cda->cda_addr2 = htosonic16(sc, enaddr[4] | (enaddr[5] << 8));
	}
}

/*
 * sonic_set_filter:
 *
 *	Set the SONIC receive filter.
 */
void
sonic_set_filter(struct sonic_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	int i, entry = 0;
	uint16_t camvalid = 0;
	uint16_t rcr = 0;

	if (ifp->if_flags & IFF_BROADCAST)
		rcr |= RCR_BRD;

	if (ifp->if_flags & IFF_PROMISC) {
		rcr |= RCR_PRO;
		goto allmulti;
	}

	/* Put our station address in the first CAM slot. */
	sonic_set_camentry(sc, entry, CLLADDR(ifp->if_sadl));
	camvalid |= (1U << entry);
	entry++;

	/* Add the multicast addresses to the CAM. */
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * The only way to do this on the SONIC is to enable
			 * reception of all multicast packets.
			 */
			goto allmulti;
		}

		if (entry == SONIC_NCAMENT) {
			/*
			 * Out of CAM slots.  Have to enable reception
			 * of all multicast addresses.
			 */
			goto allmulti;
		}

		sonic_set_camentry(sc, entry, enm->enm_addrlo);
		camvalid |= (1U << entry);
		entry++;

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	/* Use only the first CAM slot (station address). */
	camvalid = 0x0001;
	entry = 1;
	rcr |= RCR_AMC;

 setit:
	/* set mask for the CAM Enable register */
	if (sc->sc_32bit) {
		if (entry == SONIC_NCAMENT)
			sc->sc_cdaenable32 = htosonic32(sc, camvalid);
		else
			sc->sc_cda32[entry].cda_entry =
			    htosonic32(sc, camvalid);
	} else {
		if (entry == SONIC_NCAMENT)
			sc->sc_cdaenable16 = htosonic16(sc, camvalid);
		else
			sc->sc_cda16[entry].cda_entry =
			    htosonic16(sc, camvalid);
	}

	/* Load the CAM. */
	SONIC_CDCAMSYNC(sc, BUS_DMASYNC_PREWRITE);
	CSR_WRITE(sc, SONIC_CDP, SONIC_CDCAMADDR(sc) & 0xffff);
	CSR_WRITE(sc, SONIC_CDC, entry);
	CSR_WRITE(sc, SONIC_CR, CR_LCAM);
	for (i = 0; i < 10000; i++) {
		if ((CSR_READ(sc, SONIC_CR) & CR_LCAM) == 0)
			break;
		delay(2);
	}
	if (CSR_READ(sc, SONIC_CR) & CR_LCAM)
		printf("%s: CAM load failed\n", device_xname(sc->sc_dev));
	SONIC_CDCAMSYNC(sc, BUS_DMASYNC_POSTWRITE);

	/* Set the receive control register. */
	CSR_WRITE(sc, SONIC_RCR, rcr);
}
