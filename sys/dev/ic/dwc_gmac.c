/* $NetBSD: dwc_gmac.c,v 1.34 2015/08/21 20:12:29 jmcneill Exp $ */

/*-
 * Copyright (c) 2013, 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry and Martin Husemann.
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
 * This driver supports the Synopsis Designware GMAC core, as found
 * on Allwinner A20 cores and others.
 *
 * Real documentation seems to not be available, the marketing product
 * documents could be found here:
 *
 *  http://www.synopsys.com/dw/ipdir.php?ds=dwc_ether_mac10_100_1000_unive
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(1, "$NetBSD: dwc_gmac.c,v 1.34 2015/08/21 20:12:29 jmcneill Exp $");

/* #define	DWC_GMAC_DEBUG	1 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/bpf.h>
#ifdef INET
#include <netinet/if_inarp.h>
#endif

#include <dev/mii/miivar.h>

#include <dev/ic/dwc_gmac_reg.h>
#include <dev/ic/dwc_gmac_var.h>

static int dwc_gmac_miibus_read_reg(device_t, int, int);
static void dwc_gmac_miibus_write_reg(device_t, int, int, int);
static void dwc_gmac_miibus_statchg(struct ifnet *);

static int dwc_gmac_reset(struct dwc_gmac_softc *sc);
static void dwc_gmac_write_hwaddr(struct dwc_gmac_softc *sc,
			 uint8_t enaddr[ETHER_ADDR_LEN]);
static int dwc_gmac_alloc_dma_rings(struct dwc_gmac_softc *sc);
static void dwc_gmac_free_dma_rings(struct dwc_gmac_softc *sc);
static int dwc_gmac_alloc_rx_ring(struct dwc_gmac_softc *sc, struct dwc_gmac_rx_ring *);
static void dwc_gmac_reset_rx_ring(struct dwc_gmac_softc *sc, struct dwc_gmac_rx_ring *);
static void dwc_gmac_free_rx_ring(struct dwc_gmac_softc *sc, struct dwc_gmac_rx_ring *);
static int dwc_gmac_alloc_tx_ring(struct dwc_gmac_softc *sc, struct dwc_gmac_tx_ring *);
static void dwc_gmac_reset_tx_ring(struct dwc_gmac_softc *sc, struct dwc_gmac_tx_ring *);
static void dwc_gmac_free_tx_ring(struct dwc_gmac_softc *sc, struct dwc_gmac_tx_ring *);
static void dwc_gmac_txdesc_sync(struct dwc_gmac_softc *sc, int start, int end, int ops);
static int dwc_gmac_init(struct ifnet *ifp);
static void dwc_gmac_stop(struct ifnet *ifp, int disable);
static void dwc_gmac_start(struct ifnet *ifp);
static int dwc_gmac_queue(struct dwc_gmac_softc *sc, struct mbuf *m0);
static int dwc_gmac_ioctl(struct ifnet *, u_long, void *);
static void dwc_gmac_tx_intr(struct dwc_gmac_softc *sc);
static void dwc_gmac_rx_intr(struct dwc_gmac_softc *sc);
static void dwc_gmac_setmulti(struct dwc_gmac_softc *sc);
static int dwc_gmac_ifflags_cb(struct ethercom *);
static uint32_t	bitrev32(uint32_t x);

#define	TX_DESC_OFFSET(N)	((AWGE_RX_RING_COUNT+(N)) \
				    *sizeof(struct dwc_gmac_dev_dmadesc))
#define	TX_NEXT(N)		(((N)+1) & (AWGE_TX_RING_COUNT-1))

#define RX_DESC_OFFSET(N)	((N)*sizeof(struct dwc_gmac_dev_dmadesc))
#define	RX_NEXT(N)		(((N)+1) & (AWGE_RX_RING_COUNT-1))



#define	GMAC_DEF_DMA_INT_MASK	(GMAC_DMA_INT_TIE|GMAC_DMA_INT_RIE| \
				GMAC_DMA_INT_NIE|GMAC_DMA_INT_AIE| \
				GMAC_DMA_INT_FBE|GMAC_DMA_INT_UNE)

#define	GMAC_DMA_INT_ERRORS	(GMAC_DMA_INT_AIE|GMAC_DMA_INT_ERE| \
				GMAC_DMA_INT_FBE|	\
				GMAC_DMA_INT_RWE|GMAC_DMA_INT_RUE| \
				GMAC_DMA_INT_UNE|GMAC_DMA_INT_OVE| \
				GMAC_DMA_INT_TJE)

#define	AWIN_DEF_MAC_INTRMASK	\
	(AWIN_GMAC_MAC_INT_TSI | AWIN_GMAC_MAC_INT_ANEG |	\
	AWIN_GMAC_MAC_INT_LINKCHG | AWIN_GMAC_MAC_INT_RGSMII)


#ifdef DWC_GMAC_DEBUG
static void dwc_gmac_dump_dma(struct dwc_gmac_softc *sc);
static void dwc_gmac_dump_tx_desc(struct dwc_gmac_softc *sc);
static void dwc_gmac_dump_rx_desc(struct dwc_gmac_softc *sc);
static void dwc_dump_and_abort(struct dwc_gmac_softc *sc, const char *msg);
static void dwc_dump_status(struct dwc_gmac_softc *sc);
static void dwc_gmac_dump_ffilt(struct dwc_gmac_softc *sc, uint32_t ffilt);
#endif

void
dwc_gmac_attach(struct dwc_gmac_softc *sc, uint32_t mii_clk)
{
	uint8_t enaddr[ETHER_ADDR_LEN];
	uint32_t maclo, machi;
	struct mii_data * const mii = &sc->sc_mii;
	struct ifnet * const ifp = &sc->sc_ec.ec_if;
	prop_dictionary_t dict;
	int s;

	mutex_init(&sc->sc_mdio_lock, MUTEX_DEFAULT, IPL_NET);
	sc->sc_mii_clk = mii_clk & 7;

	dict = device_properties(sc->sc_dev);
	prop_data_t ea = dict ? prop_dictionary_get(dict, "mac-address") : NULL;
	if (ea != NULL) {
		/*
		 * If the MAC address is overriden by a device property,
		 * use that.
		 */
		KASSERT(prop_object_type(ea) == PROP_TYPE_DATA);
		KASSERT(prop_data_size(ea) == ETHER_ADDR_LEN);
		memcpy(enaddr, prop_data_data_nocopy(ea), ETHER_ADDR_LEN);
	} else {
		/*
		 * If we did not get an externaly configure address,
		 * try to read one from the current filter setup,
		 * before resetting the chip.
		 */
		maclo = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    AWIN_GMAC_MAC_ADDR0LO);
		machi = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    AWIN_GMAC_MAC_ADDR0HI);

		if (maclo == 0xffffffff && (machi & 0xffff) == 0xffff) {
			/* fake MAC address */
			maclo = 0x00f2 | (cprng_strong32() << 16);
			machi = cprng_strong32();
		}

		enaddr[0] = maclo & 0x0ff;
		enaddr[1] = (maclo >> 8) & 0x0ff;
		enaddr[2] = (maclo >> 16) & 0x0ff;
		enaddr[3] = (maclo >> 24) & 0x0ff;
		enaddr[4] = machi & 0x0ff;
		enaddr[5] = (machi >> 8) & 0x0ff;
	}

	/*
	 * Init chip and do initial setup
	 */
	if (dwc_gmac_reset(sc) != 0)
		return;	/* not much to cleanup, haven't attached yet */
	dwc_gmac_write_hwaddr(sc, enaddr);
	aprint_normal_dev(sc->sc_dev, "Ethernet address: %s\n",
	    ether_sprintf(enaddr));

	/*
	 * Allocate Tx and Rx rings
	 */
	if (dwc_gmac_alloc_dma_rings(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate DMA rings\n");
		goto fail;
	}
		
	if (dwc_gmac_alloc_tx_ring(sc, &sc->sc_txq) != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Tx ring\n");
		goto fail;
	}

	mutex_init(&sc->sc_rxq.r_mtx, MUTEX_DEFAULT, IPL_NET);
	if (dwc_gmac_alloc_rx_ring(sc, &sc->sc_rxq) != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Rx ring\n");
		goto fail;
	}

	/*
	 * Prepare interface data
	 */
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = dwc_gmac_ioctl;
	ifp->if_start = dwc_gmac_start;
	ifp->if_init = dwc_gmac_init;
	ifp->if_stop = dwc_gmac_stop;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Attach MII subdevices
	 */
	sc->sc_ec.ec_mii = &sc->sc_mii;
	ifmedia_init(&mii->mii_media, 0, ether_mediachange, ether_mediastatus);
        mii->mii_ifp = ifp;
        mii->mii_readreg = dwc_gmac_miibus_read_reg;
        mii->mii_writereg = dwc_gmac_miibus_write_reg;
        mii->mii_statchg = dwc_gmac_miibus_statchg;
        mii_attach(sc->sc_dev, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY,
	    MIIF_DOPAUSE);

        if (LIST_EMPTY(&mii->mii_phys)) { 
                aprint_error_dev(sc->sc_dev, "no PHY found!\n");
                ifmedia_add(&mii->mii_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
                ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_MANUAL);
        } else {
                ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_AUTO);
        }

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ec.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/*
	 * Ready, attach interface
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
	ether_set_ifflags_cb(&sc->sc_ec, dwc_gmac_ifflags_cb);

	/*
	 * Enable interrupts
	 */
	s = splnet();
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_INTMASK,
	    AWIN_DEF_MAC_INTRMASK);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_INTENABLE,
	    GMAC_DEF_DMA_INT_MASK);
	splx(s);

	return;

fail:
	dwc_gmac_free_rx_ring(sc, &sc->sc_rxq);
	dwc_gmac_free_tx_ring(sc, &sc->sc_txq);
}



static int
dwc_gmac_reset(struct dwc_gmac_softc *sc)
{
	size_t cnt;
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_BUSMODE,
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_BUSMODE) | GMAC_BUSMODE_RESET);
	for (cnt = 0; cnt < 3000; cnt++) {
		if ((bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_BUSMODE)
		    & GMAC_BUSMODE_RESET) == 0)
			return 0;
		delay(10);
	}

	aprint_error_dev(sc->sc_dev, "reset timed out\n");
	return EIO;
}

static void
dwc_gmac_write_hwaddr(struct dwc_gmac_softc *sc,
    uint8_t enaddr[ETHER_ADDR_LEN])
{
	uint32_t lo, hi;

	lo = enaddr[0] | (enaddr[1] << 8) | (enaddr[2] << 16)
	    | (enaddr[3] << 24);
	hi = enaddr[4] | (enaddr[5] << 8);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_ADDR0LO, lo);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_ADDR0HI, hi);
}

static int
dwc_gmac_miibus_read_reg(device_t self, int phy, int reg)
{
	struct dwc_gmac_softc * const sc = device_private(self);
	uint16_t mii;
	size_t cnt;
	int rv = 0;

	mii = __SHIFTIN(phy,GMAC_MII_PHY_MASK)
	    | __SHIFTIN(reg,GMAC_MII_REG_MASK)
	    | __SHIFTIN(sc->sc_mii_clk,GMAC_MII_CLKMASK)
	    | GMAC_MII_BUSY;

	mutex_enter(&sc->sc_mdio_lock);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_MIIADDR, mii);

	for (cnt = 0; cnt < 1000; cnt++) {
		if (!(bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    AWIN_GMAC_MAC_MIIADDR) & GMAC_MII_BUSY)) {
			rv = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
			    AWIN_GMAC_MAC_MIIDATA);
			break;
		}
		delay(10);
	}

	mutex_exit(&sc->sc_mdio_lock);

	return rv;
}

static void
dwc_gmac_miibus_write_reg(device_t self, int phy, int reg, int val)
{
	struct dwc_gmac_softc * const sc = device_private(self);
	uint16_t mii;
	size_t cnt;

	mii = __SHIFTIN(phy,GMAC_MII_PHY_MASK)
	    | __SHIFTIN(reg,GMAC_MII_REG_MASK)
	    | __SHIFTIN(sc->sc_mii_clk,GMAC_MII_CLKMASK)
	    | GMAC_MII_BUSY | GMAC_MII_WRITE;

	mutex_enter(&sc->sc_mdio_lock);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_MIIDATA, val);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_MIIADDR, mii);

	for (cnt = 0; cnt < 1000; cnt++) {
		if (!(bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    AWIN_GMAC_MAC_MIIADDR) & GMAC_MII_BUSY))
			break;
		delay(10);
	}
	
	mutex_exit(&sc->sc_mdio_lock);
}

static int
dwc_gmac_alloc_rx_ring(struct dwc_gmac_softc *sc,
	struct dwc_gmac_rx_ring *ring)
{
	struct dwc_gmac_rx_data *data;
	bus_addr_t physaddr;
	const size_t descsize = AWGE_RX_RING_COUNT * sizeof(*ring->r_desc);
	int error, i, next;

	ring->r_cur = ring->r_next = 0;
	memset(ring->r_desc, 0, descsize);

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	for (i = 0; i < AWGE_RX_RING_COUNT; i++) {
		struct dwc_gmac_dev_dmadesc *desc;

		data = &sc->sc_rxq.r_data[i];

		MGETHDR(data->rd_m, M_DONTWAIT, MT_DATA);
		if (data->rd_m == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate rx mbuf #%d\n", i);
			error = ENOMEM;
			goto fail;
		}
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &data->rd_map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create DMA map\n");
			data->rd_map = NULL;
			goto fail;
		}
		MCLGET(data->rd_m, M_DONTWAIT);
		if (!(data->rd_m->m_flags & M_EXT)) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate mbuf cluster #%d\n", i);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->rd_map,
		    mtod(data->rd_m, void *), MCLBYTES, NULL,
		    BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not load rx buf DMA map #%d", i);
			goto fail;
		}
		physaddr = data->rd_map->dm_segs[0].ds_addr;

		desc = &sc->sc_rxq.r_desc[i];
		desc->ddesc_data = htole32(physaddr);
		next = RX_NEXT(i);
		desc->ddesc_next = htole32(ring->r_physaddr 
		    + next * sizeof(*desc));
		desc->ddesc_cntl = htole32(
		    __SHIFTIN(AWGE_MAX_PACKET,DDESC_CNTL_SIZE1MASK) |
		    DDESC_CNTL_RXCHAIN);
		desc->ddesc_status = htole32(DDESC_STATUS_OWNEDBYDEV);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map, 0,
	    AWGE_RX_RING_COUNT*sizeof(struct dwc_gmac_dev_dmadesc),
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_RX_ADDR,
	    ring->r_physaddr);

	return 0;

fail:
	dwc_gmac_free_rx_ring(sc, ring);
	return error;
}

static void
dwc_gmac_reset_rx_ring(struct dwc_gmac_softc *sc,
	struct dwc_gmac_rx_ring *ring)
{
	struct dwc_gmac_dev_dmadesc *desc;
	int i;

	for (i = 0; i < AWGE_RX_RING_COUNT; i++) {
		desc = &sc->sc_rxq.r_desc[i];
		desc->ddesc_cntl = htole32(
		    __SHIFTIN(AWGE_MAX_PACKET,DDESC_CNTL_SIZE1MASK) |
		    DDESC_CNTL_RXCHAIN);
		desc->ddesc_status = htole32(DDESC_STATUS_OWNEDBYDEV);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map, 0,
	    AWGE_RX_RING_COUNT*sizeof(struct dwc_gmac_dev_dmadesc),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	ring->r_cur = ring->r_next = 0;
	/* reset DMA address to start of ring */
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_RX_ADDR,
	    sc->sc_rxq.r_physaddr);
}

static int
dwc_gmac_alloc_dma_rings(struct dwc_gmac_softc *sc)
{
	const size_t descsize = AWGE_TOTAL_RING_COUNT *
		sizeof(struct dwc_gmac_dev_dmadesc);
	int error, nsegs;
	void *rings;

	error = bus_dmamap_create(sc->sc_dmat, descsize, 1, descsize, 0,
	    BUS_DMA_NOWAIT, &sc->sc_dma_ring_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create desc DMA map\n");
		sc->sc_dma_ring_map = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, descsize, PAGE_SIZE, 0,
	    &sc->sc_dma_ring_seg, 1, &nsegs, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not map DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->sc_dma_ring_seg, nsegs,
	    descsize, &rings, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_dma_ring_map, rings,
	    descsize, NULL, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load desc DMA map\n");
		goto fail;
	}

	/* give first AWGE_RX_RING_COUNT to the RX side */
	sc->sc_rxq.r_desc = rings;
	sc->sc_rxq.r_physaddr = sc->sc_dma_ring_map->dm_segs[0].ds_addr;

	/* and next rings to the TX side */
	sc->sc_txq.t_desc = sc->sc_rxq.r_desc + AWGE_RX_RING_COUNT;
	sc->sc_txq.t_physaddr = sc->sc_rxq.r_physaddr + 
	    AWGE_RX_RING_COUNT*sizeof(struct dwc_gmac_dev_dmadesc);

	return 0;

fail:
	dwc_gmac_free_dma_rings(sc);
	return error;
}

static void
dwc_gmac_free_dma_rings(struct dwc_gmac_softc *sc)
{
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map, 0,
	    sc->sc_dma_ring_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dma_ring_map);
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_rxq.r_desc,
	    AWGE_TOTAL_RING_COUNT * sizeof(struct dwc_gmac_dev_dmadesc));
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dma_ring_seg, 1);
}

static void
dwc_gmac_free_rx_ring(struct dwc_gmac_softc *sc, struct dwc_gmac_rx_ring *ring)
{
	struct dwc_gmac_rx_data *data;
	int i;

	if (ring->r_desc == NULL)
		return;


	for (i = 0; i < AWGE_RX_RING_COUNT; i++) {
		data = &ring->r_data[i];

		if (data->rd_map != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->rd_map, 0,
			    AWGE_RX_RING_COUNT
				*sizeof(struct dwc_gmac_dev_dmadesc),
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, data->rd_map);
			bus_dmamap_destroy(sc->sc_dmat, data->rd_map);
		}
		if (data->rd_m != NULL)
			m_freem(data->rd_m);
	}
}

static int
dwc_gmac_alloc_tx_ring(struct dwc_gmac_softc *sc,
	struct dwc_gmac_tx_ring *ring)
{
	int i, error = 0;

	ring->t_queued = 0;
	ring->t_cur = ring->t_next = 0;

	memset(ring->t_desc, 0, AWGE_TX_RING_COUNT*sizeof(*ring->t_desc));
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map,
	    TX_DESC_OFFSET(0),
	    AWGE_TX_RING_COUNT*sizeof(struct dwc_gmac_dev_dmadesc),
	    BUS_DMASYNC_POSTWRITE);

	for (i = 0; i < AWGE_TX_RING_COUNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    AWGE_TX_RING_COUNT, MCLBYTES, 0,
		    BUS_DMA_NOWAIT|BUS_DMA_COHERENT,
		    &ring->t_data[i].td_map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create TX DMA map #%d\n", i);
			ring->t_data[i].td_map = NULL;
			goto fail;
		}
		ring->t_desc[i].ddesc_next = htole32(
		    ring->t_physaddr + sizeof(struct dwc_gmac_dev_dmadesc)
		    *TX_NEXT(i));
	}

	return 0;

fail:
	dwc_gmac_free_tx_ring(sc, ring);
	return error;
}

static void
dwc_gmac_txdesc_sync(struct dwc_gmac_softc *sc, int start, int end, int ops)
{
	/* 'end' is pointing one descriptor beyound the last we want to sync */
	if (end > start) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map,
		    TX_DESC_OFFSET(start),
		    TX_DESC_OFFSET(end)-TX_DESC_OFFSET(start),
		    ops);
		return;
	}
	/* sync from 'start' to end of ring */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map,
	    TX_DESC_OFFSET(start),
	    TX_DESC_OFFSET(AWGE_TX_RING_COUNT)-TX_DESC_OFFSET(start),
	    ops);
	/* sync from start of ring to 'end' */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map,
	    TX_DESC_OFFSET(0),
	    TX_DESC_OFFSET(end)-TX_DESC_OFFSET(0),
	    ops);
}

static void
dwc_gmac_reset_tx_ring(struct dwc_gmac_softc *sc,
	struct dwc_gmac_tx_ring *ring)
{
	int i;

	for (i = 0; i < AWGE_TX_RING_COUNT; i++) {
		struct dwc_gmac_tx_data *data = &ring->t_data[i];

		if (data->td_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->td_active,
			    0, data->td_active->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->td_active);
			m_freem(data->td_m);
			data->td_m = NULL;
		}
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map,
	    TX_DESC_OFFSET(0),
	    AWGE_TX_RING_COUNT*sizeof(struct dwc_gmac_dev_dmadesc),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_TX_ADDR,
	    sc->sc_txq.t_physaddr);

	ring->t_queued = 0;
	ring->t_cur = ring->t_next = 0;
}

static void
dwc_gmac_free_tx_ring(struct dwc_gmac_softc *sc,
	struct dwc_gmac_tx_ring *ring)
{
	int i;

	/* unload the maps */
	for (i = 0; i < AWGE_TX_RING_COUNT; i++) {
		struct dwc_gmac_tx_data *data = &ring->t_data[i];

		if (data->td_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->td_active,
			    0, data->td_map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->td_active);
			m_freem(data->td_m);
			data->td_m = NULL;
		}
	}

	/* and actually free them */
	for (i = 0; i < AWGE_TX_RING_COUNT; i++) {
		struct dwc_gmac_tx_data *data = &ring->t_data[i];

		bus_dmamap_destroy(sc->sc_dmat, data->td_map);
	}
}

static void
dwc_gmac_miibus_statchg(struct ifnet *ifp)
{
	struct dwc_gmac_softc * const sc = ifp->if_softc;
	struct mii_data * const mii = &sc->sc_mii;
	uint32_t conf, flow;

	/*
	 * Set MII or GMII interface based on the speed
	 * negotiated by the PHY.                                           
	 */
	conf = bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_CONF);
	conf &= ~(AWIN_GMAC_MAC_CONF_FES100|AWIN_GMAC_MAC_CONF_MIISEL
	    |AWIN_GMAC_MAC_CONF_FULLDPLX);
	conf |= AWIN_GMAC_MAC_CONF_FRAMEBURST
	    | AWIN_GMAC_MAC_CONF_DISABLERXOWN
	    | AWIN_GMAC_MAC_CONF_DISABLEJABBER
	    | AWIN_GMAC_MAC_CONF_ACS
	    | AWIN_GMAC_MAC_CONF_RXENABLE
	    | AWIN_GMAC_MAC_CONF_TXENABLE;
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
		conf |= AWIN_GMAC_MAC_CONF_MIISEL;
		break;
	case IFM_100_TX:
		conf |= AWIN_GMAC_MAC_CONF_FES100 |
			AWIN_GMAC_MAC_CONF_MIISEL;
		break;
	case IFM_1000_T:
		break;
	}

	flow = 0;
	if (IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) {
		conf |= AWIN_GMAC_MAC_CONF_FULLDPLX;
		flow |= __SHIFTIN(0x200, AWIN_GMAC_MAC_FLOWCTRL_PAUSE);
	}
	if (mii->mii_media_active & IFM_ETH_TXPAUSE) {
		flow |= AWIN_GMAC_MAC_FLOWCTRL_TFE;
	}
	if (mii->mii_media_active & IFM_ETH_RXPAUSE) {
		flow |= AWIN_GMAC_MAC_FLOWCTRL_RFE;
	}
	bus_space_write_4(sc->sc_bst, sc->sc_bsh,
	    AWIN_GMAC_MAC_FLOWCTRL, flow);

#ifdef DWC_GMAC_DEBUG
	aprint_normal_dev(sc->sc_dev,
	    "setting MAC conf register: %08x\n", conf);
#endif

	bus_space_write_4(sc->sc_bst, sc->sc_bsh,
	    AWIN_GMAC_MAC_CONF, conf);
}

static int
dwc_gmac_init(struct ifnet *ifp)
{
	struct dwc_gmac_softc *sc = ifp->if_softc;
	uint32_t ffilt;

	if (ifp->if_flags & IFF_RUNNING)
		return 0;

	dwc_gmac_stop(ifp, 0);

	/*
	 * Configure DMA burst/transfer mode and RX/TX priorities.
	 * XXX - the GMAC_BUSMODE_PRIORXTX bits are undocumented.
	 */
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_BUSMODE,
	    GMAC_BUSMODE_FIXEDBURST | GMAC_BUSMODE_4PBL |
	    __SHIFTIN(2, GMAC_BUSMODE_RPBL) |
	    __SHIFTIN(2, GMAC_BUSMODE_PBL));

	/*
	 * Set up address filter
	 */
	ffilt = bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_FFILT);
	if (ifp->if_flags & IFF_PROMISC) {
		ffilt |= AWIN_GMAC_MAC_FFILT_PR;
	} else {
		ffilt &= ~AWIN_GMAC_MAC_FFILT_PR;
	}
	if (ifp->if_flags & IFF_BROADCAST) {
		ffilt &= ~AWIN_GMAC_MAC_FFILT_DBF;
	} else {
		ffilt |= AWIN_GMAC_MAC_FFILT_DBF;
	}
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_FFILT, ffilt);

	/*
	 * Set up multicast filter
	 */
	dwc_gmac_setmulti(sc);

	/*
	 * Set up dma pointer for RX and TX ring
	 */
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_RX_ADDR,
	    sc->sc_rxq.r_physaddr);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_TX_ADDR,
	    sc->sc_txq.t_physaddr);

	/*
	 * Start RX/TX part
	 */
	bus_space_write_4(sc->sc_bst, sc->sc_bsh,
	    AWIN_GMAC_DMA_OPMODE, GMAC_DMA_OP_RXSTART | GMAC_DMA_OP_TXSTART |
	    GMAC_DMA_OP_RXSTOREFORWARD | GMAC_DMA_OP_TXSTOREFORWARD);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return 0;
}

static void
dwc_gmac_start(struct ifnet *ifp)
{
	struct dwc_gmac_softc *sc = ifp->if_softc;
	int old = sc->sc_txq.t_queued;
	int start = sc->sc_txq.t_cur;
	struct mbuf *m0;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		if (dwc_gmac_queue(sc, m0) != 0) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		bpf_mtap(ifp, m0);
		if (sc->sc_txq.t_queued == AWGE_TX_RING_COUNT) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	if (sc->sc_txq.t_queued != old) {
		/* packets have been queued, kick it off */
		dwc_gmac_txdesc_sync(sc, start, sc->sc_txq.t_cur,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		bus_space_write_4(sc->sc_bst, sc->sc_bsh,
		    AWIN_GMAC_DMA_TXPOLL, ~0U);
#ifdef DWC_GMAC_DEBUG
		dwc_dump_status(sc);
#endif
	}
}

static void
dwc_gmac_stop(struct ifnet *ifp, int disable)
{
	struct dwc_gmac_softc *sc = ifp->if_softc;

	bus_space_write_4(sc->sc_bst, sc->sc_bsh,
	    AWIN_GMAC_DMA_OPMODE,
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh,
	        AWIN_GMAC_DMA_OPMODE)
		& ~(GMAC_DMA_OP_TXSTART|GMAC_DMA_OP_RXSTART));
	bus_space_write_4(sc->sc_bst, sc->sc_bsh,
	    AWIN_GMAC_DMA_OPMODE,
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh,
	        AWIN_GMAC_DMA_OPMODE) | GMAC_DMA_OP_FLUSHTX);

	mii_down(&sc->sc_mii);
	dwc_gmac_reset_tx_ring(sc, &sc->sc_txq);
	dwc_gmac_reset_rx_ring(sc, &sc->sc_rxq);
}

/*
 * Add m0 to the TX ring
 */
static int
dwc_gmac_queue(struct dwc_gmac_softc *sc, struct mbuf *m0)
{
	struct dwc_gmac_dev_dmadesc *desc = NULL;
	struct dwc_gmac_tx_data *data = NULL;
	bus_dmamap_t map;
	uint32_t flags, len, status;
	int error, i, first;

#ifdef DWC_GMAC_DEBUG
	aprint_normal_dev(sc->sc_dev,
	    "dwc_gmac_queue: adding mbuf chain %p\n", m0);
#endif

	first = sc->sc_txq.t_cur;
	map = sc->sc_txq.t_data[first].td_map;

	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m0,
	    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map mbuf "
		    "(len: %d, error %d)\n", m0->m_pkthdr.len, error);
		return error;
	}

	if (sc->sc_txq.t_queued + map->dm_nsegs > AWGE_TX_RING_COUNT) {
		bus_dmamap_unload(sc->sc_dmat, map);
		return ENOBUFS;
	}

	flags = DDESC_CNTL_TXFIRST|DDESC_CNTL_TXCHAIN;
	status = 0;
	for (i = 0; i < map->dm_nsegs; i++) {
		data = &sc->sc_txq.t_data[sc->sc_txq.t_cur];
		desc = &sc->sc_txq.t_desc[sc->sc_txq.t_cur];

		desc->ddesc_data = htole32(map->dm_segs[i].ds_addr);
		len = __SHIFTIN(map->dm_segs[i].ds_len, DDESC_CNTL_SIZE1MASK);

#ifdef DWC_GMAC_DEBUG
		aprint_normal_dev(sc->sc_dev, "enqueing desc #%d data %08lx "
		    "len %lu (flags: %08x, len: %08x)\n", sc->sc_txq.t_cur,
		    (unsigned long)map->dm_segs[i].ds_addr,
		    (unsigned long)map->dm_segs[i].ds_len,
		    flags, len);
#endif

		desc->ddesc_cntl = htole32(len|flags);
		flags &= ~DDESC_CNTL_TXFIRST;

		/*
		 * Defer passing ownership of the first descriptor
		 * until we are done.
		 */
		desc->ddesc_status = htole32(status);
		status |= DDESC_STATUS_OWNEDBYDEV;

		sc->sc_txq.t_queued++;
		sc->sc_txq.t_cur = TX_NEXT(sc->sc_txq.t_cur);
	}

	desc->ddesc_cntl |= htole32(DDESC_CNTL_TXLAST|DDESC_CNTL_TXINT);

	data->td_m = m0;
	data->td_active = map;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* Pass first to device */
	sc->sc_txq.t_desc[first].ddesc_status =
	    htole32(DDESC_STATUS_OWNEDBYDEV);

	return 0;
}

/*
 * If the interface is up and running, only modify the receive
 * filter when setting promiscuous or debug mode.  Otherwise fall
 * through to ether_ioctl, which will reset the chip.
 */
static int
dwc_gmac_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct dwc_gmac_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		return ENETRESET;
	if ((change & IFF_PROMISC) != 0)
		dwc_gmac_setmulti(sc);
	return 0;
}

static int
dwc_gmac_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct dwc_gmac_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
		error = 0;
		if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			dwc_gmac_setmulti(sc);
		}
	}

	/* Try to get things going again */
	if (ifp->if_flags & IFF_UP)
		dwc_gmac_start(ifp);
	sc->sc_if_flags = sc->sc_ec.ec_if.if_flags;
	splx(s);
	return error;
}

static void
dwc_gmac_tx_intr(struct dwc_gmac_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct dwc_gmac_tx_data *data;
	struct dwc_gmac_dev_dmadesc *desc;
	uint32_t status;
	int i, nsegs;

	for (i = sc->sc_txq.t_next; sc->sc_txq.t_queued > 0; i = TX_NEXT(i)) {
#ifdef DWC_GMAC_DEBUG
		aprint_normal_dev(sc->sc_dev,
		    "dwc_gmac_tx_intr: checking desc #%d (t_queued: %d)\n",
		    i, sc->sc_txq.t_queued);
#endif

		/*
		 * i+1 does not need to be a valid descriptor,
		 * this is just a special notion to just sync
		 * a single tx descriptor (i)
		 */
		dwc_gmac_txdesc_sync(sc, i, i+1,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		desc = &sc->sc_txq.t_desc[i];
		status = le32toh(desc->ddesc_status);
		if (status & DDESC_STATUS_OWNEDBYDEV)
			break;

		data = &sc->sc_txq.t_data[i];
		if (data->td_m == NULL)
			continue;

		ifp->if_opackets++;
		nsegs = data->td_active->dm_nsegs;
		bus_dmamap_sync(sc->sc_dmat, data->td_active, 0,
		    data->td_active->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->td_active);

#ifdef DWC_GMAC_DEBUG
		aprint_normal_dev(sc->sc_dev,
		    "dwc_gmac_tx_intr: done with packet at desc #%d, "
		    "freeing mbuf %p\n", i, data->td_m);
#endif

		m_freem(data->td_m);
		data->td_m = NULL;

		sc->sc_txq.t_queued -= nsegs;
	}

	sc->sc_txq.t_next = i;

	if (sc->sc_txq.t_queued < AWGE_TX_RING_COUNT) {
		ifp->if_flags &= ~IFF_OACTIVE;
	}
}

static void
dwc_gmac_rx_intr(struct dwc_gmac_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct dwc_gmac_dev_dmadesc *desc;
	struct dwc_gmac_rx_data *data;
	bus_addr_t physaddr;
	uint32_t status;
	struct mbuf *m, *mnew;
	int i, len, error;

	for (i = sc->sc_rxq.r_cur; ; i = RX_NEXT(i)) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map,
		    RX_DESC_OFFSET(i), sizeof(*desc),
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		desc = &sc->sc_rxq.r_desc[i];
		data = &sc->sc_rxq.r_data[i];

		status = le32toh(desc->ddesc_status);
		if (status & DDESC_STATUS_OWNEDBYDEV)
			break;

		if (status & (DDESC_STATUS_RXERROR|DDESC_STATUS_RXTRUNCATED)) {
#ifdef DWC_GMAC_DEBUG
			aprint_normal_dev(sc->sc_dev,
			    "RX error: descriptor status %08x, skipping\n",
			    status);
#endif
			ifp->if_ierrors++;
			goto skip;
		}

		len = __SHIFTOUT(status, DDESC_STATUS_FRMLENMSK);

#ifdef DWC_GMAC_DEBUG
		aprint_normal_dev(sc->sc_dev,
		    "rx int: device is done with descriptor #%d, len: %d\n",
		    i, len);
#endif

		/*
		 * Try to get a new mbuf before passing this one
		 * up, if that fails, drop the packet and reuse
		 * the existing one.
		 */
		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			ifp->if_ierrors++;
			goto skip;
		}
		MCLGET(mnew, M_DONTWAIT);
		if ((mnew->m_flags & M_EXT) == 0) {
			m_freem(mnew);
			ifp->if_ierrors++;
			goto skip;
		}

		/* unload old DMA map */
		bus_dmamap_sync(sc->sc_dmat, data->rd_map, 0,
		    data->rd_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, data->rd_map);

		/* and reload with new mbuf */
		error = bus_dmamap_load(sc->sc_dmat, data->rd_map,
		    mtod(mnew, void*), MCLBYTES, NULL,
		    BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (error != 0) {
			m_freem(mnew);
			/* try to reload old mbuf */
			error = bus_dmamap_load(sc->sc_dmat, data->rd_map,
			    mtod(data->rd_m, void*), MCLBYTES, NULL,
			    BUS_DMA_READ | BUS_DMA_NOWAIT);
			if (error != 0) {
				panic("%s: could not load old rx mbuf",
				    device_xname(sc->sc_dev));
			}
			ifp->if_ierrors++;
			goto skip;
		}
		physaddr = data->rd_map->dm_segs[0].ds_addr;

		/*
		 * New mbuf loaded, update RX ring and continue
		 */
		m = data->rd_m;
		data->rd_m = mnew;
		desc->ddesc_data = htole32(physaddr);

		/* finalize mbuf */
		m->m_pkthdr.len = m->m_len = len;
		m->m_pkthdr.rcvif = ifp;
		m->m_flags |= M_HASFCS;

		bpf_mtap(ifp, m);
		ifp->if_ipackets++;
		(*ifp->if_input)(ifp, m);

skip:
		bus_dmamap_sync(sc->sc_dmat, data->rd_map, 0,
		    data->rd_map->dm_mapsize, BUS_DMASYNC_PREREAD);
		desc->ddesc_cntl = htole32(
		    __SHIFTIN(AWGE_MAX_PACKET,DDESC_CNTL_SIZE1MASK) |
		    DDESC_CNTL_RXCHAIN);
		desc->ddesc_status = htole32(DDESC_STATUS_OWNEDBYDEV);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_ring_map,
		    RX_DESC_OFFSET(i), sizeof(*desc),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}

	/* update RX pointer */
	sc->sc_rxq.r_cur = i;

}

/*
 * Reverse order of bits - http://aggregate.org/MAGIC/#Bit%20Reversal
 */
static uint32_t
bitrev32(uint32_t x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

	return (x >> 16) | (x << 16);
}

static void
dwc_gmac_setmulti(struct dwc_gmac_softc *sc)
{
	struct ifnet * const ifp = &sc->sc_ec.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t hashes[2] = { 0, 0 };
	uint32_t ffilt, h;
	int mcnt, s;

	s = splnet();

	ffilt = bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_FFILT);
	
	if (ifp->if_flags & IFF_PROMISC) {
		ffilt |= AWIN_GMAC_MAC_FFILT_PR;
		goto special_filter;
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	ffilt &= ~(AWIN_GMAC_MAC_FFILT_PM|AWIN_GMAC_MAC_FFILT_PR);

	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_HTLOW, 0);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_HTHIGH, 0);

	ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
	mcnt = 0;
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0) {
			ffilt |= AWIN_GMAC_MAC_FFILT_PM;
			ifp->if_flags |= IFF_ALLMULTI;
			goto special_filter;
		}

		h = bitrev32(
			~ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN)
		    ) >> 26;
		hashes[h >> 5] |= (1 << (h & 0x1f));

		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	if (mcnt)
		ffilt |= AWIN_GMAC_MAC_FFILT_HMC;
	else
		ffilt &= ~AWIN_GMAC_MAC_FFILT_HMC;

	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_FFILT, ffilt);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_HTLOW,
	    hashes[0]);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_HTHIGH,
	    hashes[1]);
	sc->sc_if_flags = sc->sc_ec.ec_if.if_flags;

	splx(s);

#ifdef DWC_GMAC_DEBUG
	dwc_gmac_dump_ffilt(sc, ffilt);
#endif
	return;

special_filter:
#ifdef DWC_GMAC_DEBUG
	dwc_gmac_dump_ffilt(sc, ffilt);
#endif
	/* no MAC hashes, ALLMULTI or PROMISC */
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_FFILT,
	    ffilt);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_HTLOW,
	    0xffffffff);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_HTHIGH,
	    0xffffffff);
	sc->sc_if_flags = sc->sc_ec.ec_if.if_flags;
	splx(s);
}

int
dwc_gmac_intr(struct dwc_gmac_softc *sc)
{
	uint32_t status, dma_status;
	int rv = 0;

	status = bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_INTR);
	if (status & AWIN_GMAC_MII_IRQ) {
		(void)bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    AWIN_GMAC_MII_STATUS);
		rv = 1;
		mii_pollstat(&sc->sc_mii);
	}

	dma_status = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
	    AWIN_GMAC_DMA_STATUS);

	if (dma_status & (GMAC_DMA_INT_NIE|GMAC_DMA_INT_AIE))
		rv = 1;

	if (dma_status & GMAC_DMA_INT_TIE)
		dwc_gmac_tx_intr(sc);

	if (dma_status & GMAC_DMA_INT_RIE)
		dwc_gmac_rx_intr(sc);

	/*
	 * Check error conditions
	 */
	if (dma_status & GMAC_DMA_INT_ERRORS) {
		sc->sc_ec.ec_if.if_oerrors++;
#ifdef DWC_GMAC_DEBUG
		dwc_dump_and_abort(sc, "interrupt error condition");
#endif
	}

	/* ack interrupt */
	if (dma_status)
		bus_space_write_4(sc->sc_bst, sc->sc_bsh,
		    AWIN_GMAC_DMA_STATUS, dma_status & GMAC_DMA_INT_MASK);

	/*
	 * Get more packets
	 */
	if (rv)
		sc->sc_ec.ec_if.if_start(&sc->sc_ec.ec_if);

	return rv;
}

#ifdef DWC_GMAC_DEBUG
static void
dwc_gmac_dump_dma(struct dwc_gmac_softc *sc)
{
	aprint_normal_dev(sc->sc_dev, "busmode: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_BUSMODE));
	aprint_normal_dev(sc->sc_dev, "tx poll: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_TXPOLL));
	aprint_normal_dev(sc->sc_dev, "rx poll: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_RXPOLL));
	aprint_normal_dev(sc->sc_dev, "rx descriptors: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_RX_ADDR));
	aprint_normal_dev(sc->sc_dev, "tx descriptors: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_TX_ADDR));
	aprint_normal_dev(sc->sc_dev, "status: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_STATUS));
	aprint_normal_dev(sc->sc_dev, "op mode: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_OPMODE));
	aprint_normal_dev(sc->sc_dev, "int enable: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_INTENABLE));
	aprint_normal_dev(sc->sc_dev, "cur tx: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_CUR_TX_DESC));
	aprint_normal_dev(sc->sc_dev, "cur rx: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_CUR_RX_DESC));
	aprint_normal_dev(sc->sc_dev, "cur tx buffer: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_CUR_TX_BUFADDR));
	aprint_normal_dev(sc->sc_dev, "cur rx buffer: %08x\n",
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_DMA_CUR_RX_BUFADDR));
}

static void
dwc_gmac_dump_tx_desc(struct dwc_gmac_softc *sc)
{
	int i;

	aprint_normal_dev(sc->sc_dev, "TX queue: cur=%d, next=%d, queued=%d\n",
	    sc->sc_txq.t_cur, sc->sc_txq.t_next, sc->sc_txq.t_queued);
	aprint_normal_dev(sc->sc_dev, "TX DMA descriptors:\n");
	for (i = 0; i < AWGE_TX_RING_COUNT; i++) {
		struct dwc_gmac_dev_dmadesc *desc = &sc->sc_txq.t_desc[i];
		aprint_normal("#%d (%08lx): status: %08x cntl: %08x "
		    "data: %08x next: %08x\n",
		    i, sc->sc_txq.t_physaddr +
			i*sizeof(struct dwc_gmac_dev_dmadesc),
		    le32toh(desc->ddesc_status), le32toh(desc->ddesc_cntl),
		    le32toh(desc->ddesc_data), le32toh(desc->ddesc_next));
	}
}

static void
dwc_gmac_dump_rx_desc(struct dwc_gmac_softc *sc)
{
	int i;

	aprint_normal_dev(sc->sc_dev, "RX queue: cur=%d, next=%d\n",
	    sc->sc_rxq.r_cur, sc->sc_rxq.r_next);
	aprint_normal_dev(sc->sc_dev, "RX DMA descriptors:\n");
	for (i = 0; i < AWGE_RX_RING_COUNT; i++) {
		struct dwc_gmac_dev_dmadesc *desc = &sc->sc_rxq.r_desc[i];
		aprint_normal("#%d (%08lx): status: %08x cntl: %08x "
		    "data: %08x next: %08x\n",
		    i, sc->sc_rxq.r_physaddr +
			i*sizeof(struct dwc_gmac_dev_dmadesc),
		    le32toh(desc->ddesc_status), le32toh(desc->ddesc_cntl),
		    le32toh(desc->ddesc_data), le32toh(desc->ddesc_next));
	}
}

static void
dwc_dump_status(struct dwc_gmac_softc *sc)
{
	uint32_t status = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
	     AWIN_GMAC_MAC_INTR);
	uint32_t dma_status = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
	     AWIN_GMAC_DMA_STATUS);
	char buf[200];

	/* print interrupt state */
	snprintb(buf, sizeof(buf), "\177\20"
	    "b\x10""NI\0"
	    "b\x0f""AI\0"
	    "b\x0e""ER\0"
	    "b\x0d""FB\0"
	    "b\x0a""ET\0"
	    "b\x09""RW\0"
	    "b\x08""RS\0"
	    "b\x07""RU\0"
	    "b\x06""RI\0"
	    "b\x05""UN\0"
	    "b\x04""OV\0"
	    "b\x03""TJ\0"
	    "b\x02""TU\0"
	    "b\x01""TS\0"
	    "b\x00""TI\0"
	    "\0", dma_status);
	aprint_normal_dev(sc->sc_dev, "INTR status: %08x, DMA status: %s\n",
	    status, buf);
}

static void
dwc_dump_and_abort(struct dwc_gmac_softc *sc, const char *msg)
{
	dwc_dump_status(sc);
	dwc_gmac_dump_ffilt(sc,
	    bus_space_read_4(sc->sc_bst, sc->sc_bsh, AWIN_GMAC_MAC_FFILT));
	dwc_gmac_dump_dma(sc);
	dwc_gmac_dump_tx_desc(sc);
	dwc_gmac_dump_rx_desc(sc);

	panic("%s", msg);
}

static void dwc_gmac_dump_ffilt(struct dwc_gmac_softc *sc, uint32_t ffilt)
{
	char buf[200];

	/* print filter setup */
	snprintb(buf, sizeof(buf), "\177\20"
	    "b\x1f""RA\0"
	    "b\x0a""HPF\0"
	    "b\x09""SAF\0"
	    "b\x08""SAIF\0"
	    "b\x05""DBF\0"
	    "b\x04""PM\0"
	    "b\x03""DAIF\0"
	    "b\x02""HMC\0"
	    "b\x01""HUC\0"
	    "b\x00""PR\0"
	    "\0", ffilt);
	aprint_normal_dev(sc->sc_dev, "FFILT: %s\n", buf);
}
#endif
