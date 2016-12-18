/*	$NetBSD: rt2661.c,v 1.30 2015/01/07 07:05:48 ozaki-r Exp $	*/
/*	$OpenBSD: rt2661.c,v 1.17 2006/05/01 08:41:11 damien Exp $	*/
/*	$FreeBSD: rt2560.c,v 1.5 2006/06/02 19:59:31 csjp Exp $	*/

/*-
 * Copyright (c) 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Ralink Technology RT2561, RT2561S and RT2661 chipset driver
 * http://www.ralinktech.com/
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rt2661.c,v 1.30 2015/01/07 07:05:48 ozaki-r Exp $");


#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/endian.h>
#include <sys/intr.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/rt2661reg.h>
#include <dev/ic/rt2661var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/firmload.h>

#ifdef RAL_DEBUG
#define DPRINTF(x)	do { if (rt2661_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (rt2661_debug >= (n)) printf x; } while (0)
int rt2661_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

static int	rt2661_alloc_tx_ring(struct rt2661_softc *,
		    struct rt2661_tx_ring *, int);
static void	rt2661_reset_tx_ring(struct rt2661_softc *,
		    struct rt2661_tx_ring *);
static void	rt2661_free_tx_ring(struct rt2661_softc *,
		    struct rt2661_tx_ring *);
static int	rt2661_alloc_rx_ring(struct rt2661_softc *,
		    struct rt2661_rx_ring *, int);
static void	rt2661_reset_rx_ring(struct rt2661_softc *,
		    struct rt2661_rx_ring *);
static void	rt2661_free_rx_ring(struct rt2661_softc *,
		    struct rt2661_rx_ring *);
static struct ieee80211_node *
		rt2661_node_alloc(struct ieee80211_node_table *);
static int	rt2661_media_change(struct ifnet *);
static void	rt2661_next_scan(void *);
static void	rt2661_iter_func(void *, struct ieee80211_node *);
static void	rt2661_updatestats(void *);
static void	rt2661_newassoc(struct ieee80211_node *, int);
static int	rt2661_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
static uint16_t	rt2661_eeprom_read(struct rt2661_softc *, uint8_t);
static void	rt2661_tx_intr(struct rt2661_softc *);
static void	rt2661_tx_dma_intr(struct rt2661_softc *,
		    struct rt2661_tx_ring *);
static void	rt2661_rx_intr(struct rt2661_softc *);
static void	rt2661_mcu_beacon_expire(struct rt2661_softc *);
static void	rt2661_mcu_wakeup(struct rt2661_softc *);
static void	rt2661_mcu_cmd_intr(struct rt2661_softc *);
int		rt2661_intr(void *);
static uint8_t	rt2661_rxrate(struct rt2661_rx_desc *);
static int	rt2661_ack_rate(struct ieee80211com *, int);
static uint16_t	rt2661_txtime(int, int, uint32_t);
static uint8_t	rt2661_plcp_signal(int);
static void	rt2661_setup_tx_desc(struct rt2661_softc *,
		    struct rt2661_tx_desc *, uint32_t, uint16_t, int, int,
		    const bus_dma_segment_t *, int, int);
static int	rt2661_tx_mgt(struct rt2661_softc *, struct mbuf *,
		    struct ieee80211_node *);
static struct mbuf *
		rt2661_get_rts(struct rt2661_softc *,
		    struct ieee80211_frame *, uint16_t);
static int	rt2661_tx_data(struct rt2661_softc *, struct mbuf *,
		    struct ieee80211_node *, int);
static void	rt2661_start(struct ifnet *);
static void	rt2661_watchdog(struct ifnet *);
static int	rt2661_reset(struct ifnet *);
static int	rt2661_ioctl(struct ifnet *, u_long, void *);
static void	rt2661_bbp_write(struct rt2661_softc *, uint8_t, uint8_t);
static uint8_t	rt2661_bbp_read(struct rt2661_softc *, uint8_t);
static void	rt2661_rf_write(struct rt2661_softc *, uint8_t, uint32_t);
static int	rt2661_tx_cmd(struct rt2661_softc *, uint8_t, uint16_t);
static void	rt2661_select_antenna(struct rt2661_softc *);
static void	rt2661_enable_mrr(struct rt2661_softc *);
static void	rt2661_set_txpreamble(struct rt2661_softc *);
static void	rt2661_set_basicrates(struct rt2661_softc *,
			const struct ieee80211_rateset *);
static void	rt2661_select_band(struct rt2661_softc *,
		    struct ieee80211_channel *);
static void	rt2661_set_chan(struct rt2661_softc *,
		    struct ieee80211_channel *);
static void	rt2661_set_bssid(struct rt2661_softc *, const uint8_t *);
static void	rt2661_set_macaddr(struct rt2661_softc *, const uint8_t *);
static void	rt2661_update_promisc(struct rt2661_softc *);
#if 0
static int	rt2661_wme_update(struct ieee80211com *);
#endif

static void	rt2661_updateslot(struct ifnet *);
static void	rt2661_set_slottime(struct rt2661_softc *);
static const char *
		rt2661_get_rf(int);
static void	rt2661_read_eeprom(struct rt2661_softc *);
static int	rt2661_bbp_init(struct rt2661_softc *);
static int     	rt2661_init(struct ifnet *);
static void	rt2661_stop(struct ifnet *, int);
static int	rt2661_load_microcode(struct rt2661_softc *, const uint8_t *,
		    int);
static void	rt2661_rx_tune(struct rt2661_softc *);
#ifdef notyet
static void	rt2661_radar_start(struct rt2661_softc *);
static int	rt2661_radar_stop(struct rt2661_softc *);
#endif
static int	rt2661_prepare_beacon(struct rt2661_softc *);
static void	rt2661_enable_tsf_sync(struct rt2661_softc *);
static int	rt2661_get_rssi(struct rt2661_softc *, uint8_t);

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset rt2661_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset rt2661_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset rt2661_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct {
	uint32_t	reg;
	uint32_t	val;
} rt2661_def_mac[] = {
	RT2661_DEF_MAC
};

static const struct {
	uint8_t	reg;
	uint8_t	val;
} rt2661_def_bbp[] = {
	RT2661_DEF_BBP
};

static const struct rfprog {
	uint8_t		chan;
	uint32_t	r1, r2, r3, r4;
} rt2661_rf5225_1[] = {
	RT2661_RF5225_1
}, rt2661_rf5225_2[] = {
	RT2661_RF5225_2
};

int
rt2661_attach(void *xsc, int id)
{
	struct rt2661_softc *sc = xsc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	uint32_t val;
	int error, i, ntries;

	sc->sc_id = id;

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 15;
	callout_init(&sc->scan_ch, 0);
	callout_init(&sc->amrr_ch, 0);

	/* wait for NIC to initialize */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((val = RAL_READ(sc, RT2661_MAC_CSR0)) != 0)
			break;
		DELAY(1000);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev, "timeout waiting for NIC to initialize\n");
		return EIO;
	}

	/* retrieve RF rev. no and various other things from EEPROM */
	rt2661_read_eeprom(sc);
	aprint_normal_dev(sc->sc_dev, "802.11 address %s\n",
	    ether_sprintf(ic->ic_myaddr));

	aprint_normal_dev(sc->sc_dev, "MAC/BBP RT%X, RF %s\n", val,
	    rt2661_get_rf(sc->rf_rev));

	/*
	 * Allocate Tx and Rx rings.
	 */
	error = rt2661_alloc_tx_ring(sc, &sc->txq[0], RT2661_TX_RING_COUNT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Tx ring 0\n");
		goto fail1;
	}

	error = rt2661_alloc_tx_ring(sc, &sc->txq[1], RT2661_TX_RING_COUNT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Tx ring 1\n");
		goto fail2;
	}

	error = rt2661_alloc_tx_ring(sc, &sc->txq[2], RT2661_TX_RING_COUNT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Tx ring 2\n");
		goto fail3;
	}

	error = rt2661_alloc_tx_ring(sc, &sc->txq[3], RT2661_TX_RING_COUNT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Tx ring 3\n");
		goto fail4;
	}

	error = rt2661_alloc_tx_ring(sc, &sc->mgtq, RT2661_MGT_RING_COUNT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Mgt ring\n");
		goto fail5;
	}

	error = rt2661_alloc_rx_ring(sc, &sc->rxq, RT2661_RX_RING_COUNT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Rx ring\n");
		goto fail6;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = rt2661_init;
	ifp->if_stop = rt2661_stop;
	ifp->if_ioctl = rt2661_ioctl;
	ifp->if_start = rt2661_start;
	ifp->if_watchdog = rt2661_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_HOSTAP |	/* HostAP mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_WPA;		/* 802.11i */

	if (sc->rf_rev == RT2661_RF_5225 || sc->rf_rev == RT2661_RF_5325) {
		/* set supported .11a rates */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = rt2661_rateset_11a;

		/* set supported .11a channels */
		for (i = 36; i <= 64; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 100; i <= 140; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 149; i <= 165; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
	}

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = rt2661_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = rt2661_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	if_attach(ifp);
	ieee80211_ifattach(ic);
	ic->ic_node_alloc = rt2661_node_alloc;
	ic->ic_newassoc = rt2661_newassoc;
	ic->ic_updateslot = rt2661_updateslot;
	ic->ic_reset = rt2661_reset;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = rt2661_newstate;
	ieee80211_media_init(ic, rt2661_media_change, ieee80211_media_status);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + sizeof(sc->sc_txtap),
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = roundup(sizeof(sc->sc_rxtap), sizeof(u_int32_t));
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RT2661_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = roundup(sizeof(sc->sc_txtap), sizeof(u_int32_t));
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RT2661_TX_RADIOTAP_PRESENT);

	ieee80211_announce(ic);

	if (pmf_device_register(sc->sc_dev, NULL, NULL))
		pmf_class_network_register(sc->sc_dev, ifp);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	return 0;

fail6:	rt2661_free_tx_ring(sc, &sc->mgtq);
fail5:	rt2661_free_tx_ring(sc, &sc->txq[3]);
fail4:	rt2661_free_tx_ring(sc, &sc->txq[2]);
fail3:	rt2661_free_tx_ring(sc, &sc->txq[1]);
fail2:	rt2661_free_tx_ring(sc, &sc->txq[0]);
fail1:	return ENXIO;
}

int
rt2661_detach(void *xsc)
{
	struct rt2661_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_if;

	callout_stop(&sc->scan_ch);
	callout_stop(&sc->amrr_ch);

	pmf_device_deregister(sc->sc_dev);

	ieee80211_ifdetach(&sc->sc_ic);
	if_detach(ifp);

	rt2661_free_tx_ring(sc, &sc->txq[0]);
	rt2661_free_tx_ring(sc, &sc->txq[1]);
	rt2661_free_tx_ring(sc, &sc->txq[2]);
	rt2661_free_tx_ring(sc, &sc->txq[3]);
	rt2661_free_tx_ring(sc, &sc->mgtq);
	rt2661_free_rx_ring(sc, &sc->rxq);

	return 0;
}

static int
rt2661_alloc_tx_ring(struct rt2661_softc *sc, struct rt2661_tx_ring *ring,
    int count)
{
	int i, nsegs, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = ring->stat = 0;

	error = bus_dmamap_create(sc->sc_dmat, count * RT2661_TX_DESC_SIZE, 1,
	    count * RT2661_TX_DESC_SIZE, 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not create desc DMA map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, count * RT2661_TX_DESC_SIZE,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * RT2661_TX_DESC_SIZE, (void **)&ring->desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map desc DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * RT2661_TX_DESC_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	memset(ring->desc, 0, count * RT2661_TX_DESC_SIZE);
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct rt2661_tx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	memset(ring->data, 0, count * sizeof (struct rt2661_tx_data));
	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    RT2661_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &ring->data[i].map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not create DMA map\n");
			goto fail;
		}
	}

	return 0;

fail:	rt2661_free_tx_ring(sc, ring);
	return error;
}

static void
rt2661_reset_tx_ring(struct rt2661_softc *sc, struct rt2661_tx_ring *ring)
{
	struct rt2661_tx_desc *desc;
	struct rt2661_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}

		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}

		desc->flags = 0;
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = ring->stat = 0;
}


static void
rt2661_free_tx_ring(struct rt2661_softc *sc, struct rt2661_tx_ring *ring)
{
	struct rt2661_tx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (void *)ring->desc,
		    ring->count * RT2661_TX_DESC_SIZE);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->ni != NULL)
				ieee80211_free_node(data->ni);

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

static int
rt2661_alloc_rx_ring(struct rt2661_softc *sc, struct rt2661_rx_ring *ring,
    int count)
{
	struct rt2661_rx_desc *desc;
	struct rt2661_rx_data *data;
	int i, nsegs, error;

	ring->count = count;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat, count * RT2661_RX_DESC_SIZE, 1,
	    count * RT2661_RX_DESC_SIZE, 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not create desc DMA map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, count * RT2661_RX_DESC_SIZE,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * RT2661_RX_DESC_SIZE, (void **)&ring->desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map desc DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * RT2661_RX_DESC_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	memset(ring->desc, 0, count * RT2661_RX_DESC_SIZE);
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct rt2661_rx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	memset(ring->data, 0, count * sizeof (struct rt2661_rx_data));
	for (i = 0; i < count; i++) {
		desc = &sc->rxq.desc[i];
		data = &sc->rxq.data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not create DMA map\n");
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			aprint_error_dev(sc->sc_dev, "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			aprint_error_dev(sc->sc_dev, "could not allocate rx mbuf cluster\n");
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not load rx buf DMA map");
			goto fail;
		}

		desc->physaddr = htole32(data->map->dm_segs->ds_addr);
		desc->flags = htole32(RT2661_RX_BUSY);
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	rt2661_free_rx_ring(sc, ring);
	return error;
}

static void
rt2661_reset_rx_ring(struct rt2661_softc *sc, struct rt2661_rx_ring *ring)
{
	int i;

	for (i = 0; i < ring->count; i++)
		ring->desc[i].flags = htole32(RT2661_RX_BUSY);

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
}

static void
rt2661_free_rx_ring(struct rt2661_softc *sc, struct rt2661_rx_ring *ring)
{
	struct rt2661_rx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (void *)ring->desc,
		    ring->count * RT2661_RX_DESC_SIZE);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

static struct ieee80211_node *
rt2661_node_alloc(struct ieee80211_node_table *nt)
{
	struct rt2661_node *rn;

	rn = malloc(sizeof (struct rt2661_node), M_80211_NODE,
	    M_NOWAIT | M_ZERO);

	return (rn != NULL) ? &rn->ni : NULL;
}

static int
rt2661_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		rt2661_init(ifp);

	return 0;
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
static void
rt2661_next_scan(void *arg)
{
	struct rt2661_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
	splx(s);
}

/*
 * This function is called for each neighbor node.
 */
static void
rt2661_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct rt2661_softc *sc = arg;
	struct rt2661_node *rn = (struct rt2661_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &rn->amn);
}

/*
 * This function is called periodically (every 500ms) in RUN state to update
 * various settings like rate control statistics or Rx sensitivity.
 */
static void
rt2661_updatestats(void *arg)
{
	struct rt2661_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	if (ic->ic_opmode == IEEE80211_M_STA)
		rt2661_iter_func(sc, ic->ic_bss);
	else
		ieee80211_iterate_nodes(&ic->ic_sta, rt2661_iter_func, arg);

	/* update rx sensitivity every 1 sec */
	if (++sc->ncalls & 1)
		rt2661_rx_tune(sc);
	splx(s);

	callout_reset(&sc->amrr_ch, hz / 2, rt2661_updatestats, sc);
}

static void
rt2661_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct rt2661_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int i;

	ieee80211_amrr_node_init(&sc->amrr, &((struct rt2661_node *)ni)->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;
}

static int
rt2661_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct rt2661_softc *sc = ic->ic_ifp->if_softc;
	enum ieee80211_state ostate;
	struct ieee80211_node *ni;
	uint32_t tmp;

	ostate = ic->ic_state;
	callout_stop(&sc->scan_ch);

	switch (nstate) {
	case IEEE80211_S_INIT:
		callout_stop(&sc->amrr_ch);

		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			tmp = RAL_READ(sc, RT2661_TXRX_CSR9);
			RAL_WRITE(sc, RT2661_TXRX_CSR9, tmp & ~0x00ffffff);
		}
		break;

	case IEEE80211_S_SCAN:
		rt2661_set_chan(sc, ic->ic_curchan);
		callout_reset(&sc->scan_ch, hz / 5, rt2661_next_scan, sc);
		break;

	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		rt2661_set_chan(sc, ic->ic_curchan);
		break;

	case IEEE80211_S_RUN:
		rt2661_set_chan(sc, ic->ic_curchan);

		ni = ic->ic_bss;

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			rt2661_set_slottime(sc);
			rt2661_enable_mrr(sc);
			rt2661_set_txpreamble(sc);
			rt2661_set_basicrates(sc, &ni->ni_rates);
			rt2661_set_bssid(sc, ni->ni_bssid);
		}

		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS)
			rt2661_prepare_beacon(sc);

		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* fake a join to init the tx rate */
			rt2661_newassoc(ni, 1);
		}

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			sc->ncalls = 0;
			sc->avg_rssi = -95;	/* reset EMA */
			callout_reset(&sc->amrr_ch, hz / 2,
			    rt2661_updatestats, sc);
			rt2661_enable_tsf_sync(sc);
		}
		break;
	}

	return sc->sc_newstate(ic, nstate, arg);
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM (either 93C46 or
 * 93C66).
 */
static uint16_t
rt2661_eeprom_read(struct rt2661_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* clock C once before the first command */
	RT2661_EEPROM_CTL(sc, 0);

	RT2661_EEPROM_CTL(sc, RT2661_S);
	RT2661_EEPROM_CTL(sc, RT2661_S | RT2661_C);
	RT2661_EEPROM_CTL(sc, RT2661_S);

	/* write start bit (1) */
	RT2661_EEPROM_CTL(sc, RT2661_S | RT2661_D);
	RT2661_EEPROM_CTL(sc, RT2661_S | RT2661_D | RT2661_C);

	/* write READ opcode (10) */
	RT2661_EEPROM_CTL(sc, RT2661_S | RT2661_D);
	RT2661_EEPROM_CTL(sc, RT2661_S | RT2661_D | RT2661_C);
	RT2661_EEPROM_CTL(sc, RT2661_S);
	RT2661_EEPROM_CTL(sc, RT2661_S | RT2661_C);

	/* write address (A5-A0 or A7-A0) */
	n = (RAL_READ(sc, RT2661_E2PROM_CSR) & RT2661_93C46) ? 5 : 7;
	for (; n >= 0; n--) {
		RT2661_EEPROM_CTL(sc, RT2661_S |
		    (((addr >> n) & 1) << RT2661_SHIFT_D));
		RT2661_EEPROM_CTL(sc, RT2661_S |
		    (((addr >> n) & 1) << RT2661_SHIFT_D) | RT2661_C);
	}

	RT2661_EEPROM_CTL(sc, RT2661_S);

	/* read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		RT2661_EEPROM_CTL(sc, RT2661_S | RT2661_C);
		tmp = RAL_READ(sc, RT2661_E2PROM_CSR);
		val |= ((tmp & RT2661_Q) >> RT2661_SHIFT_Q) << n;
		RT2661_EEPROM_CTL(sc, RT2661_S);
	}

	RT2661_EEPROM_CTL(sc, 0);

	/* clear Chip Select and clock C */
	RT2661_EEPROM_CTL(sc, RT2661_S);
	RT2661_EEPROM_CTL(sc, 0);
	RT2661_EEPROM_CTL(sc, RT2661_C);

	return val;
}

static void
rt2661_tx_intr(struct rt2661_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct rt2661_tx_ring *txq;
	struct rt2661_tx_data *data;
	struct rt2661_node *rn;
	uint32_t val;
	int qid, retrycnt;

	for (;;) {
		val = RAL_READ(sc, RT2661_STA_CSR4);
		if (!(val & RT2661_TX_STAT_VALID))
			break;

		/* retrieve the queue in which this frame was sent */
		qid = RT2661_TX_QID(val);
		txq = (qid <= 3) ? &sc->txq[qid] : &sc->mgtq;

		/* retrieve rate control algorithm context */
		data = &txq->data[txq->stat];
		rn = (struct rt2661_node *)data->ni;

		/* if no frame has been sent, ignore */
		if (rn == NULL)
			continue;

		switch (RT2661_TX_RESULT(val)) {
		case RT2661_TX_SUCCESS:
			retrycnt = RT2661_TX_RETRYCNT(val);

			DPRINTFN(10, ("data frame sent successfully after "
			    "%d retries\n", retrycnt));
			rn->amn.amn_txcnt++;
			if (retrycnt > 0)
				rn->amn.amn_retrycnt++;
			ifp->if_opackets++;
			break;

		case RT2661_TX_RETRY_FAIL:
			DPRINTFN(9, ("sending data frame failed (too much "
			    "retries)\n"));
			rn->amn.amn_txcnt++;
			rn->amn.amn_retrycnt++;
			ifp->if_oerrors++;
			break;

		default:
			/* other failure */
			aprint_error_dev(sc->sc_dev, "sending data frame failed 0x%08x\n", val);
			ifp->if_oerrors++;
		}

		ieee80211_free_node(data->ni);
		data->ni = NULL;

		DPRINTFN(15, ("tx done q=%d idx=%u\n", qid, txq->stat));

		txq->queued--;
		if (++txq->stat >= txq->count)	/* faster than % count */
			txq->stat = 0;
	}

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	rt2661_start(ifp);
}

static void
rt2661_tx_dma_intr(struct rt2661_softc *sc, struct rt2661_tx_ring *txq)
{
	struct rt2661_tx_desc *desc;
	struct rt2661_tx_data *data;

	for (;;) {
		desc = &txq->desc[txq->next];
		data = &txq->data[txq->next];

		bus_dmamap_sync(sc->sc_dmat, txq->map,
		    txq->next * RT2661_TX_DESC_SIZE, RT2661_TX_DESC_SIZE,
		    BUS_DMASYNC_POSTREAD);

		if ((le32toh(desc->flags) & RT2661_TX_BUSY) ||
		    !(le32toh(desc->flags) & RT2661_TX_VALID))
			break;

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		/* node reference is released in rt2661_tx_intr() */

		/* descriptor is no longer valid */
		desc->flags &= ~htole32(RT2661_TX_VALID);

		bus_dmamap_sync(sc->sc_dmat, txq->map,
		    txq->next * RT2661_TX_DESC_SIZE, RT2661_TX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		DPRINTFN(15, ("tx dma done q=%p idx=%u\n", txq, txq->next));

		if (++txq->next >= txq->count)	/* faster than % count */
			txq->next = 0;
	}
}

static void
rt2661_rx_intr(struct rt2661_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct rt2661_rx_desc *desc;
	struct rt2661_rx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *mnew, *m;
	int error, rssi;

	for (;;) {
		desc = &sc->rxq.desc[sc->rxq.cur];
		data = &sc->rxq.data[sc->rxq.cur];

		bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
		    sc->rxq.cur * RT2661_RX_DESC_SIZE, RT2661_RX_DESC_SIZE,
		    BUS_DMASYNC_POSTREAD);

		if (le32toh(desc->flags) & RT2661_RX_BUSY)
			break;

		if ((le32toh(desc->flags) & RT2661_RX_PHY_ERROR) ||
		    (le32toh(desc->flags) & RT2661_RX_CRC_ERROR)) {
			/*
			 * This should not happen since we did not request
			 * to receive those frames when we filled TXRX_CSR0.
			 */
			DPRINTFN(5, ("PHY or CRC error flags 0x%08x\n",
			    le32toh(desc->flags)));
			ifp->if_ierrors++;
			goto skip;
		}

		if ((le32toh(desc->flags) & RT2661_RX_CIPHER_MASK) != 0) {
			ifp->if_ierrors++;
			goto skip;
		}

		/*
		 * Try to allocate a new mbuf for this ring element and load it
		 * before processing the current mbuf. If the ring element
		 * cannot be loaded, drop the received packet and reuse the old
		 * mbuf. In the unlikely case that the old mbuf can't be
		 * reloaded either, explicitly panic.
		 */
		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			ifp->if_ierrors++;
			goto skip;
		}

		MCLGET(mnew, M_DONTWAIT);
		if (!(mnew->m_flags & M_EXT)) {
			m_freem(mnew);
			ifp->if_ierrors++;
			goto skip;
		}

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, data->map);

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(mnew, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			m_freem(mnew);

			/* try to reload the old mbuf */
			error = bus_dmamap_load(sc->sc_dmat, data->map,
			    mtod(data->m, void *), MCLBYTES, NULL,
			    BUS_DMA_NOWAIT);
			if (error != 0) {
				/* very unlikely that it will fail... */
				panic("%s: could not load old rx mbuf",
				    device_xname(sc->sc_dev));
			}
			/* physical address may have changed */
			desc->physaddr = htole32(data->map->dm_segs->ds_addr);
			ifp->if_ierrors++;
			goto skip;
		}

		/*
	 	 * New mbuf successfully loaded, update Rx ring and continue
		 * processing.
		 */
		m = data->m;
		data->m = mnew;
		desc->physaddr = htole32(data->map->dm_segs->ds_addr);

		/* finalize mbuf */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len =
		    (le32toh(desc->flags) >> 16) & 0xfff;

		if (sc->sc_drvbpf != NULL) {
			struct rt2661_rx_radiotap_header *tap = &sc->sc_rxtap;
			uint32_t tsf_lo, tsf_hi;

			/* get timestamp (low and high 32 bits) */
			tsf_hi = RAL_READ(sc, RT2661_TXRX_CSR13);
			tsf_lo = RAL_READ(sc, RT2661_TXRX_CSR12);

			tap->wr_tsf =
			    htole64(((uint64_t)tsf_hi << 32) | tsf_lo);
			tap->wr_flags = 0;
			tap->wr_rate = rt2661_rxrate(desc);
			tap->wr_chan_freq = htole16(sc->sc_curchan->ic_freq);
			tap->wr_chan_flags = htole16(sc->sc_curchan->ic_flags);
			tap->wr_antsignal = desc->rssi;

			bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
		}

		wh = mtod(m, struct ieee80211_frame *);
		ni = ieee80211_find_rxnode(ic, 
		    (struct ieee80211_frame_min *)wh);

		/* send the frame to the 802.11 layer */
		ieee80211_input(ic, m, ni, desc->rssi, 0);

		/*-
		 * Keep track of the average RSSI using an Exponential Moving
		 * Average (EMA) of 8 Wilder's days:
		 *     avg = (1 / N) x rssi + ((N - 1) / N) x avg
		 */
		rssi = rt2661_get_rssi(sc, desc->rssi);
		sc->avg_rssi = (rssi + 7 * sc->avg_rssi) / 8;

		/* node is no longer needed */
		ieee80211_free_node(ni);

skip:		desc->flags |= htole32(RT2661_RX_BUSY);

		bus_dmamap_sync(sc->sc_dmat, sc->rxq.map,
		    sc->rxq.cur * RT2661_RX_DESC_SIZE, RT2661_RX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		DPRINTFN(16, ("rx intr idx=%u\n", sc->rxq.cur));

		sc->rxq.cur = (sc->rxq.cur + 1) % RT2661_RX_RING_COUNT;
	}

	/*
	 * In HostAP mode, ieee80211_input() will enqueue packets in if_snd
	 * without calling if_start().
	 */
	if (!IFQ_IS_EMPTY(&ifp->if_snd) && !(ifp->if_flags & IFF_OACTIVE))
		rt2661_start(ifp);
}

/*
 * This function is called in HostAP or IBSS modes when it's time to send a
 * new beacon (every ni_intval milliseconds).
 */
static void
rt2661_mcu_beacon_expire(struct rt2661_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if (sc->sc_flags & RT2661_UPDATE_SLOT) {
		sc->sc_flags &= ~RT2661_UPDATE_SLOT;
		sc->sc_flags |= RT2661_SET_SLOTTIME;
	} else if (sc->sc_flags & RT2661_SET_SLOTTIME) {
		sc->sc_flags &= ~RT2661_SET_SLOTTIME;
		rt2661_set_slottime(sc);
	}

	if (ic->ic_curmode == IEEE80211_MODE_11G) {
		/* update ERP Information Element */
		RAL_WRITE_1(sc, sc->erp_csr, ic->ic_bss->ni_erp);
		RAL_RW_BARRIER_1(sc, sc->erp_csr);
	}

	DPRINTFN(15, ("beacon expired\n"));
}

static void
rt2661_mcu_wakeup(struct rt2661_softc *sc)
{
	RAL_WRITE(sc, RT2661_MAC_CSR11, 5 << 16);

	RAL_WRITE(sc, RT2661_SOFT_RESET_CSR, 0x7);
	RAL_WRITE(sc, RT2661_IO_CNTL_CSR, 0x18);
	RAL_WRITE(sc, RT2661_PCI_USEC_CSR, 0x20);

	/* send wakeup command to MCU */
	rt2661_tx_cmd(sc, RT2661_MCU_CMD_WAKEUP, 0);
}

static void
rt2661_mcu_cmd_intr(struct rt2661_softc *sc)
{
	RAL_READ(sc, RT2661_M2H_CMD_DONE_CSR);
	RAL_WRITE(sc, RT2661_M2H_CMD_DONE_CSR, 0xffffffff);
}

int
rt2661_intr(void *arg)
{
	struct rt2661_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	uint32_t r1, r2;
	int rv = 0;

	/* don't re-enable interrupts if we're shutting down */
	if (!(ifp->if_flags & IFF_RUNNING)) {
		/* disable MAC and MCU interrupts */
		RAL_WRITE(sc, RT2661_INT_MASK_CSR, 0xffffff7f);
		RAL_WRITE(sc, RT2661_MCU_INT_MASK_CSR, 0xffffffff);
		return 0;
	}

	for (;;) {
		r1 = RAL_READ(sc, RT2661_INT_SOURCE_CSR);
		r2 = RAL_READ(sc, RT2661_MCU_INT_SOURCE_CSR);

		if ((r1 & RT2661_INT_CSR_ALL) == 0 &&
		    (r2 & RT2661_MCU_INT_ALL) == 0)
			break;

		RAL_WRITE(sc, RT2661_INT_SOURCE_CSR, r1);
		RAL_WRITE(sc, RT2661_MCU_INT_SOURCE_CSR, r2);

		rv = 1;

		if (r1 & RT2661_MGT_DONE)
			rt2661_tx_dma_intr(sc, &sc->mgtq);

		if (r1 & RT2661_RX_DONE)
			rt2661_rx_intr(sc);

		if (r1 & RT2661_TX0_DMA_DONE)
			rt2661_tx_dma_intr(sc, &sc->txq[0]);

		if (r1 & RT2661_TX1_DMA_DONE)
			rt2661_tx_dma_intr(sc, &sc->txq[1]);

		if (r1 & RT2661_TX2_DMA_DONE)
			rt2661_tx_dma_intr(sc, &sc->txq[2]);

		if (r1 & RT2661_TX3_DMA_DONE)
			rt2661_tx_dma_intr(sc, &sc->txq[3]);

		if (r1 & RT2661_TX_DONE)
			rt2661_tx_intr(sc);

		if (r2 & RT2661_MCU_CMD_DONE)
			rt2661_mcu_cmd_intr(sc);

		if (r2 & RT2661_MCU_BEACON_EXPIRE)
			rt2661_mcu_beacon_expire(sc);

		if (r2 & RT2661_MCU_WAKEUP)
			rt2661_mcu_wakeup(sc);
	}

	return rv;
}

/* quickly determine if a given rate is CCK or OFDM */
#define RAL_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

#define RAL_ACK_SIZE	14	/* 10 + 4(FCS) */
#define RAL_CTS_SIZE	14	/* 10 + 4(FCS) */

/*
 * This function is only used by the Rx radiotap code. It returns the rate at
 * which a given frame was received.
 */
static uint8_t
rt2661_rxrate(struct rt2661_rx_desc *desc)
{
	if (le32toh(desc->flags) & RT2661_RX_OFDM) {
		/* reverse function of rt2661_plcp_signal */
		switch (desc->rate & 0xf) {
		case 0xb:	return 12;
		case 0xf:	return 18;
		case 0xa:	return 24;
		case 0xe:	return 36;
		case 0x9:	return 48;
		case 0xd:	return 72;
		case 0x8:	return 96;
		case 0xc:	return 108;
		}
	} else {
		if (desc->rate == 10)
			return 2;
		if (desc->rate == 20)
			return 4;
		if (desc->rate == 55)
			return 11;
		if (desc->rate == 110)
			return 22;
	}
	return 2;	/* should not get there */
}

/*
 * Return the expected ack rate for a frame transmitted at rate `rate'.
 * XXX: this should depend on the destination node basic rate set.
 */
static int
rt2661_ack_rate(struct ieee80211com *ic, int rate)
{
	switch (rate) {
	/* CCK rates */
	case 2:
		return 2;
	case 4:
	case 11:
	case 22:
		return (ic->ic_curmode == IEEE80211_MODE_11B) ? 4 : rate;

	/* OFDM rates */
	case 12:
	case 18:
		return 12;
	case 24:
	case 36:
		return 24;
	case 48:
	case 72:
	case 96:
	case 108:
		return 48;
	}

	/* default to 1Mbps */
	return 2;
}

/*
 * Compute the duration (in us) needed to transmit `len' bytes at rate `rate'.
 * The function automatically determines the operating mode depending on the
 * given rate. `flags' indicates whether short preamble is in use or not.
 */
static uint16_t
rt2661_txtime(int len, int rate, uint32_t flags)
{
	uint16_t txtime;

	if (RAL_RATE_IS_OFDM(rate)) {
		/* IEEE Std 802.11g-2003, pp. 44 */
		txtime = (8 + 4 * len + 3 + rate - 1) / rate;
		txtime = 16 + 4 + 4 * txtime + 6;
	} else {
		/* IEEE Std 802.11b-1999, pp. 28 */
		txtime = (16 * len + rate - 1) / rate;
		if (rate != 2 && (flags & IEEE80211_F_SHPREAMBLE))
			txtime +=  72 + 24;
		else
			txtime += 144 + 48;
	}
	return txtime;
}

static uint8_t
rt2661_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 0x0;
	case 4:		return 0x1;
	case 11:	return 0x2;
	case 22:	return 0x3;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 12:	return 0xb;
	case 18:	return 0xf;
	case 24:	return 0xa;
	case 36:	return 0xe;
	case 48:	return 0x9;
	case 72:	return 0xd;
	case 96:	return 0x8;
	case 108:	return 0xc;

	/* unsupported rates (should not get there) */
	default:	return 0xff;
	}
}

static void
rt2661_setup_tx_desc(struct rt2661_softc *sc, struct rt2661_tx_desc *desc,
    uint32_t flags, uint16_t xflags, int len, int rate,
    const bus_dma_segment_t *segs, int nsegs, int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t plcp_length;
	int i, remainder;

	desc->flags = htole32(flags);
	desc->flags |= htole32(len << 16);

	desc->xflags = htole16(xflags);
	desc->xflags |= htole16(nsegs << 13);

	desc->wme = htole16(
	    RT2661_QID(ac) |
	    RT2661_AIFSN(2) |
	    RT2661_LOGCWMIN(4) |
	    RT2661_LOGCWMAX(10));

	/*
	 * Remember in which queue this frame was sent. This field is driver
	 * private data only. It will be made available by the NIC in STA_CSR4
	 * on Tx interrupts.
	 */
	desc->qid = ac;

	/* setup PLCP fields */
	desc->plcp_signal  = rt2661_plcp_signal(rate);
	desc->plcp_service = 4;

	len += IEEE80211_CRC_LEN;
	if (RAL_RATE_IS_OFDM(rate)) {
		desc->flags |= htole32(RT2661_TX_OFDM);

		plcp_length = len & 0xfff;
		desc->plcp_length_hi = plcp_length >> 6;
		desc->plcp_length_lo = plcp_length & 0x3f;
	} else {
		plcp_length = (16 * len + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if (remainder != 0 && remainder < 7)
				desc->plcp_service |= RT2661_PLCP_LENGEXT;
		}
		desc->plcp_length_hi = plcp_length >> 8;
		desc->plcp_length_lo = plcp_length & 0xff;

		if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			desc->plcp_signal |= 0x08;
	}

	/* RT2x61 supports scatter with up to 5 segments */
	for (i = 0; i < nsegs; i++) {
		desc->addr[i] = htole32(segs[i].ds_addr);
		desc->len [i] = htole16(segs[i].ds_len);
	}

	desc->flags |= htole32(RT2661_TX_BUSY | RT2661_TX_VALID);
}

static int
rt2661_tx_mgt(struct rt2661_softc *sc, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2661_tx_desc *desc;
	struct rt2661_tx_data *data;
	struct ieee80211_frame *wh;
	uint16_t dur;
	uint32_t flags = 0;
	int rate, error;

	desc = &sc->mgtq.desc[sc->mgtq.cur];
	data = &sc->mgtq.data[sc->mgtq.cur];

	/* send mgt frames at the lowest available rate */
	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;

	wh = mtod(m0, struct ieee80211_frame *);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		if (ieee80211_crypto_encap(ic, ni, m0) == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}

	if (sc->sc_drvbpf != NULL) {
		struct rt2661_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(sc->sc_curchan->ic_freq);
		tap->wt_chan_flags = htole16(sc->sc_curchan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2661_TX_NEED_ACK;

		dur = rt2661_txtime(RAL_ACK_SIZE, rate, ic->ic_flags) +
		    sc->sifs;
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to set timestamp in probe responses */
		if ((wh->i_fc[0] &
		    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
		    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			flags |= RT2661_TX_TIMESTAMP;
	}

	rt2661_setup_tx_desc(sc, desc, flags, 0 /* XXX HWSEQ */,
	    m0->m_pkthdr.len, rate, data->map->dm_segs, data->map->dm_nsegs,
	    RT2661_QID_MGT);

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->mgtq.map,
	    sc->mgtq.cur * RT2661_TX_DESC_SIZE, RT2661_TX_DESC_SIZE,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending mgt frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, sc->mgtq.cur, rate));

	/* kick mgt */
	sc->mgtq.queued++;
	sc->mgtq.cur = (sc->mgtq.cur + 1) % RT2661_MGT_RING_COUNT;
	RAL_WRITE(sc, RT2661_TX_CNTL_CSR, RT2661_KICK_MGT);

	return 0;
}

/*
 * Build a RTS control frame.
 */
static struct mbuf *
rt2661_get_rts(struct rt2661_softc *sc, struct ieee80211_frame *wh,
    uint16_t dur)
{
	struct ieee80211_frame_rts *rts;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		sc->sc_ic.ic_stats.is_tx_nobuf++;
		aprint_error_dev(sc->sc_dev, "could not allocate RTS frame\n");
		return NULL;
	}

	rts = mtod(m, struct ieee80211_frame_rts *);

	rts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	    IEEE80211_FC0_SUBTYPE_RTS;
	rts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(uint16_t *)rts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(rts->i_ra, wh->i_addr1);
	IEEE80211_ADDR_COPY(rts->i_ta, wh->i_addr2);

	m->m_pkthdr.len = m->m_len = sizeof (struct ieee80211_frame_rts);

	return m;
}

static int
rt2661_tx_data(struct rt2661_softc *sc, struct mbuf *m0,
    struct ieee80211_node *ni, int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2661_tx_ring *txq = &sc->txq[ac];
	struct rt2661_tx_desc *desc;
	struct rt2661_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct mbuf *mnew;
	uint16_t dur;
	uint32_t flags = 0;
	int rate, useprot, error, tid;

	wh = mtod(m0, struct ieee80211_frame *);

	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate];
	} else
		rate = ni->ni_rates.rs_rates[ni->ni_txrate];
	rate &= IEEE80211_RATE_VAL;
	if (rate == 0)
		rate = 2;	/* XXX should not happen */

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	/*
	 * Packet Bursting: backoff after ppb=8 frames to give other STAs a
	 * chance to contend for the wireless medium.
	 */
	tid = WME_AC_TO_TID(M_WME_GETAC(m0));
	if (ic->ic_opmode == IEEE80211_M_STA && (ni->ni_txseqs[tid] & 7))
		flags |= RT2661_TX_IFS_SIFS;

	/*
	 * IEEE Std 802.11-1999, pp 82: "A STA shall use an RTS/CTS exchange
	 * for directed frames only when the length of the MPDU is greater
	 * than the length threshold indicated by" ic_rtsthreshold.
	 *
	 * IEEE Std 802.11-2003g, pp 13: "ERP STAs shall use protection
	 * mechanism (such as RTS/CTS or CTS-to-self) for ERP-OFDM MPDUs of
	 * type Data or an MMPDU".
	 */
	useprot = !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (m0->m_pkthdr.len + IEEE80211_CRC_LEN > ic->ic_rtsthreshold ||
	    ((ic->ic_flags & IEEE80211_F_USEPROT) && RAL_RATE_IS_OFDM(rate)));
	if (useprot) {
		struct mbuf *m;
		int rtsrate, ackrate;

		rtsrate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;
		ackrate = rt2661_ack_rate(ic, rate);

		dur = rt2661_txtime(m0->m_pkthdr.len + 4, rate, ic->ic_flags) +
		      rt2661_txtime(RAL_CTS_SIZE, rtsrate, ic->ic_flags) +
		      rt2661_txtime(RAL_ACK_SIZE, ackrate, ic->ic_flags) +
		      3 * sc->sifs;

		m = rt2661_get_rts(sc, wh, dur);
		if (m == NULL) {
			aprint_error_dev(sc->sc_dev, "could not allocate RTS "
			    "frame\n");
			m_freem(m0);
			return ENOBUFS;
		}

		desc = &txq->desc[txq->cur];
		data = &txq->data[txq->cur];

		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
		    BUS_DMA_NOWAIT);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not map mbuf (error %d)\n", error);
			m_freem(m);
			m_freem(m0);
			return error;
		}

		/* avoid multiple free() of the same node for each fragment */
		ieee80211_ref_node(ni);

		data->m = m;
		data->ni = ni;

		rt2661_setup_tx_desc(sc, desc, RT2661_TX_NEED_ACK |
		    RT2661_TX_MORE_FRAG, 0, m->m_pkthdr.len, rtsrate,
		    data->map->dm_segs, data->map->dm_nsegs, ac);

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, txq->map,
		    txq->cur * RT2661_TX_DESC_SIZE, RT2661_TX_DESC_SIZE,
		    BUS_DMASYNC_PREWRITE);

		txq->queued++;
		txq->cur = (txq->cur + 1) % RT2661_TX_RING_COUNT;

		flags |= RT2661_TX_LONG_RETRY | RT2661_TX_IFS_SIFS;
	}

	data = &txq->data[txq->cur];
	desc = &txq->desc[txq->cur];

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
	    BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		aprint_error_dev(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}
	if (error != 0) {
		/* too many fragments, linearize */

		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			m_freem(m0);
			return ENOMEM;
		}

		M_COPY_PKTHDR(mnew, m0);
		if (m0->m_pkthdr.len > MHLEN) {
			MCLGET(mnew, M_DONTWAIT);
			if (!(mnew->m_flags & M_EXT)) {
				m_freem(m0);
				m_freem(mnew);
				return ENOMEM;
			}
		}

		m_copydata(m0, 0, m0->m_pkthdr.len, mtod(mnew, void *));
		m_freem(m0);
		mnew->m_len = mnew->m_pkthdr.len;
		m0 = mnew;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
		    BUS_DMA_NOWAIT);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not map mbuf (error %d)\n", error);
			m_freem(m0);
			return error;
		}

		/* packet header have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	if (sc->sc_drvbpf != NULL) {
		struct rt2661_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(sc->sc_curchan->ic_freq);
		tap->wt_chan_flags = htole16(sc->sc_curchan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data->m = m0;
	data->ni = ni;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2661_TX_NEED_ACK;

		dur = rt2661_txtime(RAL_ACK_SIZE, rt2661_ack_rate(ic, rate),
		    ic->ic_flags) + sc->sifs;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

	rt2661_setup_tx_desc(sc, desc, flags, 0, m0->m_pkthdr.len, rate,
	    data->map->dm_segs, data->map->dm_nsegs, ac);

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, txq->map, txq->cur * RT2661_TX_DESC_SIZE,
	    RT2661_TX_DESC_SIZE, BUS_DMASYNC_PREWRITE);

	DPRINTFN(10, ("sending data frame len=%u idx=%u rate=%u\n",
	    m0->m_pkthdr.len, txq->cur, rate));

	/* kick Tx */
	txq->queued++;
	txq->cur = (txq->cur + 1) % RT2661_TX_RING_COUNT;
	RAL_WRITE(sc, RT2661_TX_CNTL_CSR, 1);

	return 0;
}

static void
rt2661_start(struct ifnet *ifp)
{
	struct rt2661_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ether_header *eh;
	struct ieee80211_node *ni = NULL;

	/*
	 * net80211 may still try to send management frames even if the
	 * IFF_RUNNING flag is not set...
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->mgtq.queued >= RT2661_MGT_RING_COUNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);
			if (m0 == NULL)
				break;

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
			bpf_mtap3(ic->ic_rawbpf, m0);
			if (rt2661_tx_mgt(sc, m0, ni) != 0)
				break;

		} else {
			IF_POLL(&ifp->if_snd, m0);
			if (m0 == NULL || ic->ic_state != IEEE80211_S_RUN)
				break;

			if (sc->txq[0].queued >= RT2661_TX_RING_COUNT - 1) {
				/* there is no place left in this ring */
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}

			IFQ_DEQUEUE(&ifp->if_snd, m0);

			if (m0->m_len < sizeof (struct ether_header) &&
			    !(m0 = m_pullup(m0, sizeof (struct ether_header))))
				continue;

			eh = mtod(m0, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				m_freem(m0);
				ifp->if_oerrors++;
				continue;
			}

			bpf_mtap3(ifp->if_bpf, m0);
			m0 = ieee80211_encap(ic, m0, ni);
			if (m0 == NULL) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				continue;
			}
			bpf_mtap3(ic->ic_rawbpf, m0);
			if (rt2661_tx_data(sc, m0, ni, 0) != 0) {
				if (ni != NULL)
					ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

static void
rt2661_watchdog(struct ifnet *ifp)
{
	struct rt2661_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			rt2661_init(ifp);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(&sc->sc_ic);
}

/*
 * This function allows for fast channel switching in monitor mode (used by
 * kismet). In IBSS mode, we must explicitly reset the interface to
 * generate a new beacon frame.
 */
static int
rt2661_reset(struct ifnet *ifp)
{
	struct rt2661_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		return ENETRESET;

	rt2661_set_chan(sc, ic->ic_curchan);

	return 0;
}

static int
rt2661_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct rt2661_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				rt2661_update_promisc(sc);
			else
				rt2661_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				rt2661_stop(ifp, 1);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* XXX no h/w multicast filter? --dyoung */
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;

	case SIOCS80211CHANNEL:
		/*
		 * This allows for fast channel switching in monitor mode
		 * (used by kismet). In IBSS mode, we must explicitly reset
		 * the interface to generate a new beacon frame.
		 */
		error = ieee80211_ioctl(ic, cmd, data);
		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
			     (IFF_UP | IFF_RUNNING))
				rt2661_set_chan(sc, ic->ic_ibss_chan);
			error = 0;
		}
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);

	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			rt2661_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

static void
rt2661_bbp_write(struct rt2661_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RT2661_PHY_CSR3) & RT2661_BBP_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev, "could not write to BBP\n");
		return;
	}

	tmp = RT2661_BBP_BUSY | (reg & 0x7f) << 8 | val;
	RAL_WRITE(sc, RT2661_PHY_CSR3, tmp);

	DPRINTFN(15, ("BBP R%u <- 0x%02x\n", reg, val));
}

static uint8_t
rt2661_bbp_read(struct rt2661_softc *sc, uint8_t reg)
{
	uint32_t val;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RT2661_PHY_CSR3) & RT2661_BBP_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev, "could not read from BBP\n");
		return 0;
	}

	val = RT2661_BBP_BUSY | RT2661_BBP_READ | reg << 8;
	RAL_WRITE(sc, RT2661_PHY_CSR3, val);

	for (ntries = 0; ntries < 100; ntries++) {
		val = RAL_READ(sc, RT2661_PHY_CSR3);
		if (!(val & RT2661_BBP_BUSY))
			return val & 0xff;
		DELAY(1);
	}

	aprint_error_dev(sc->sc_dev, "could not read from BBP\n");
	return 0;
}

static void
rt2661_rf_write(struct rt2661_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if (!(RAL_READ(sc, RT2661_PHY_CSR4) & RT2661_RF_BUSY))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev, "could not write to RF\n");
		return;
	}
	tmp = RT2661_RF_BUSY | RT2661_RF_21BIT | (val & 0x1fffff) << 2 |
	    (reg & 3);
	RAL_WRITE(sc, RT2661_PHY_CSR4, tmp);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 3, val & 0x1fffff));
}

static int
rt2661_tx_cmd(struct rt2661_softc *sc, uint8_t cmd, uint16_t arg)
{
	if (RAL_READ(sc, RT2661_H2M_MAILBOX_CSR) & RT2661_H2M_BUSY)
		return EIO;	/* there is already a command pending */

	RAL_WRITE(sc, RT2661_H2M_MAILBOX_CSR,
	    RT2661_H2M_BUSY | RT2661_TOKEN_NO_INTR << 16 | arg);

	RAL_WRITE(sc, RT2661_HOST_CMD_CSR, RT2661_KICK_CMD | cmd);

	return 0;
}

static void
rt2661_select_antenna(struct rt2661_softc *sc)
{
	uint8_t bbp4, bbp77;
	uint32_t tmp;

	bbp4  = rt2661_bbp_read(sc,  4);
	bbp77 = rt2661_bbp_read(sc, 77);

	/* TBD */

	/* make sure Rx is disabled before switching antenna */
	tmp = RAL_READ(sc, RT2661_TXRX_CSR0);
	RAL_WRITE(sc, RT2661_TXRX_CSR0, tmp | RT2661_DISABLE_RX);

	rt2661_bbp_write(sc,  4, bbp4);
	rt2661_bbp_write(sc, 77, bbp77);

	/* restore Rx filter */
	RAL_WRITE(sc, RT2661_TXRX_CSR0, tmp);
}

/*
 * Enable multi-rate retries for frames sent at OFDM rates.
 * In 802.11b/g mode, allow fallback to CCK rates.
 */
static void
rt2661_enable_mrr(struct rt2661_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	tmp = RAL_READ(sc, RT2661_TXRX_CSR4);

	tmp &= ~RT2661_MRR_CCK_FALLBACK;
	if (!IEEE80211_IS_CHAN_5GHZ(ic->ic_bss->ni_chan))
		tmp |= RT2661_MRR_CCK_FALLBACK;
	tmp |= RT2661_MRR_ENABLED;

	RAL_WRITE(sc, RT2661_TXRX_CSR4, tmp);
}

static void
rt2661_set_txpreamble(struct rt2661_softc *sc)
{
	uint32_t tmp;

	tmp = RAL_READ(sc, RT2661_TXRX_CSR4);

	tmp &= ~RT2661_SHORT_PREAMBLE;
	if (sc->sc_ic.ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= RT2661_SHORT_PREAMBLE;

	RAL_WRITE(sc, RT2661_TXRX_CSR4, tmp);
}

static void
rt2661_set_basicrates(struct rt2661_softc *sc,
    const struct ieee80211_rateset *rs)
{
#define RV(r)	((r) & IEEE80211_RATE_VAL)
	uint32_t mask = 0;
	uint8_t rate;
	int i, j;

	for (i = 0; i < rs->rs_nrates; i++) {
		rate = rs->rs_rates[i];

		if (!(rate & IEEE80211_RATE_BASIC))
			continue;

		/*
		 * Find h/w rate index.  We know it exists because the rate
		 * set has already been negotiated.
		 */
		for (j = 0; rt2661_rateset_11g.rs_rates[j] != RV(rate); j++);

		mask |= 1 << j;
	}

	RAL_WRITE(sc, RT2661_TXRX_CSR5, mask);

	DPRINTF(("Setting basic rate mask to 0x%x\n", mask));
#undef RV
}

/*
 * Reprogram MAC/BBP to switch to a new band.  Values taken from the reference
 * driver.
 */
static void
rt2661_select_band(struct rt2661_softc *sc, struct ieee80211_channel *c)
{
	uint8_t bbp17, bbp35, bbp96, bbp97, bbp98, bbp104;
	uint32_t tmp;

	/* update all BBP registers that depend on the band */
	bbp17 = 0x20; bbp96 = 0x48; bbp104 = 0x2c;
	bbp35 = 0x50; bbp97 = 0x48; bbp98  = 0x48;
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		bbp17 += 0x08; bbp96 += 0x10; bbp104 += 0x0c;
		bbp35 += 0x10; bbp97 += 0x10; bbp98  += 0x10;
	}
	if ((IEEE80211_IS_CHAN_2GHZ(c) && sc->ext_2ghz_lna) ||
	    (IEEE80211_IS_CHAN_5GHZ(c) && sc->ext_5ghz_lna)) {
		bbp17 += 0x10; bbp96 += 0x10; bbp104 += 0x10;
	}

	sc->bbp17 = bbp17;
	rt2661_bbp_write(sc,  17, bbp17);
	rt2661_bbp_write(sc,  96, bbp96);
	rt2661_bbp_write(sc, 104, bbp104);

	if ((IEEE80211_IS_CHAN_2GHZ(c) && sc->ext_2ghz_lna) ||
	    (IEEE80211_IS_CHAN_5GHZ(c) && sc->ext_5ghz_lna)) {
		rt2661_bbp_write(sc, 75, 0x80);
		rt2661_bbp_write(sc, 86, 0x80);
		rt2661_bbp_write(sc, 88, 0x80);
	}

	rt2661_bbp_write(sc, 35, bbp35);
	rt2661_bbp_write(sc, 97, bbp97);
	rt2661_bbp_write(sc, 98, bbp98);

	tmp = RAL_READ(sc, RT2661_PHY_CSR0);
	tmp &= ~(RT2661_PA_PE_2GHZ | RT2661_PA_PE_5GHZ);
	if (IEEE80211_IS_CHAN_2GHZ(c))
		tmp |= RT2661_PA_PE_2GHZ;
	else
		tmp |= RT2661_PA_PE_5GHZ;
	RAL_WRITE(sc, RT2661_PHY_CSR0, tmp);

	/* 802.11a uses a 16 microseconds short interframe space */
	sc->sifs = IEEE80211_IS_CHAN_5GHZ(c) ? 16 : 10;
}

static void
rt2661_set_chan(struct rt2661_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct rfprog *rfprog;
	uint8_t bbp3, bbp94 = RT2661_BBPR94_DEFAULT;
	int8_t power;
	u_int i, chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	/* select the appropriate RF settings based on what EEPROM says */
	rfprog = (sc->rfprog == 0) ? rt2661_rf5225_1 : rt2661_rf5225_2;

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rfprog[i].chan != chan; i++);

	power = sc->txpow[i];
	if (power < 0) {
		bbp94 += power;
		power = 0;
	} else if (power > 31) {
		bbp94 += power - 31;
		power = 31;
	}

	/*
	 * If we've yet to select a channel, or we are switching from the
	 * 2GHz band to the 5GHz band or vice-versa, BBP registers need to
	 * be reprogrammed.
	 */
	if (sc->sc_curchan == NULL || c->ic_flags != sc->sc_curchan->ic_flags) {
		rt2661_select_band(sc, c);
		rt2661_select_antenna(sc);
	}
	sc->sc_curchan = c;

	rt2661_rf_write(sc, RAL_RF1, rfprog[i].r1);
	rt2661_rf_write(sc, RAL_RF2, rfprog[i].r2);
	rt2661_rf_write(sc, RAL_RF3, rfprog[i].r3 | power << 7);
	rt2661_rf_write(sc, RAL_RF4, rfprog[i].r4 | sc->rffreq << 10);

	DELAY(200);

	rt2661_rf_write(sc, RAL_RF1, rfprog[i].r1);
	rt2661_rf_write(sc, RAL_RF2, rfprog[i].r2);
	rt2661_rf_write(sc, RAL_RF3, rfprog[i].r3 | power << 7 | 1);
	rt2661_rf_write(sc, RAL_RF4, rfprog[i].r4 | sc->rffreq << 10);

	DELAY(200);

	rt2661_rf_write(sc, RAL_RF1, rfprog[i].r1);
	rt2661_rf_write(sc, RAL_RF2, rfprog[i].r2);
	rt2661_rf_write(sc, RAL_RF3, rfprog[i].r3 | power << 7);
	rt2661_rf_write(sc, RAL_RF4, rfprog[i].r4 | sc->rffreq << 10);

	/* enable smart mode for MIMO-capable RFs */
	bbp3 = rt2661_bbp_read(sc, 3);

	bbp3 &= ~RT2661_SMART_MODE;
	if (sc->rf_rev == RT2661_RF_5325 || sc->rf_rev == RT2661_RF_2529)
		bbp3 |= RT2661_SMART_MODE;

	rt2661_bbp_write(sc, 3, bbp3);

	if (bbp94 != RT2661_BBPR94_DEFAULT)
		rt2661_bbp_write(sc, 94, bbp94);

	/* 5GHz radio needs a 1ms delay here */
	if (IEEE80211_IS_CHAN_5GHZ(c))
		DELAY(1000);
}

static void
rt2661_set_bssid(struct rt2661_softc *sc, const uint8_t *bssid)
{
	uint32_t tmp;

	tmp = bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24;
	RAL_WRITE(sc, RT2661_MAC_CSR4, tmp);

	tmp = bssid[4] | bssid[5] << 8 | RT2661_ONE_BSSID << 16;
	RAL_WRITE(sc, RT2661_MAC_CSR5, tmp);
}

static void
rt2661_set_macaddr(struct rt2661_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24;
	RAL_WRITE(sc, RT2661_MAC_CSR2, tmp);

	tmp = addr[4] | addr[5] << 8 | 0xff << 16;
	RAL_WRITE(sc, RT2661_MAC_CSR3, tmp);
}

static void
rt2661_update_promisc(struct rt2661_softc *sc)
{
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	uint32_t tmp;

	tmp = RAL_READ(sc, RT2661_TXRX_CSR0);

	tmp &= ~RT2661_DROP_NOT_TO_ME;
	if (!(ifp->if_flags & IFF_PROMISC))
		tmp |= RT2661_DROP_NOT_TO_ME;

	RAL_WRITE(sc, RT2661_TXRX_CSR0, tmp);

	DPRINTF(("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving"));
}

#if 0
/*
 * Update QoS (802.11e) settings for each h/w Tx ring.
 */
static int
rt2661_wme_update(struct ieee80211com *ic)
{
	struct rt2661_softc *sc = ic->ic_ifp->if_softc;
	const struct wmeParams *wmep;

	wmep = ic->ic_wme.wme_chanParams.cap_wmeParams;

	/* XXX: not sure about shifts. */
	/* XXX: the reference driver plays with AC_VI settings too. */

	/* update TxOp */
	RAL_WRITE(sc, RT2661_AC_TXOP_CSR0,
	    wmep[WME_AC_BE].wmep_txopLimit << 16 |
	    wmep[WME_AC_BK].wmep_txopLimit);
	RAL_WRITE(sc, RT2661_AC_TXOP_CSR1,
	    wmep[WME_AC_VI].wmep_txopLimit << 16 |
	    wmep[WME_AC_VO].wmep_txopLimit);

	/* update CWmin */
	RAL_WRITE(sc, RT2661_CWMIN_CSR,
	    wmep[WME_AC_BE].wmep_logcwmin << 12 |
	    wmep[WME_AC_BK].wmep_logcwmin <<  8 |
	    wmep[WME_AC_VI].wmep_logcwmin <<  4 |
	    wmep[WME_AC_VO].wmep_logcwmin);

	/* update CWmax */
	RAL_WRITE(sc, RT2661_CWMAX_CSR,
	    wmep[WME_AC_BE].wmep_logcwmax << 12 |
	    wmep[WME_AC_BK].wmep_logcwmax <<  8 |
	    wmep[WME_AC_VI].wmep_logcwmax <<  4 |
	    wmep[WME_AC_VO].wmep_logcwmax);

	/* update Aifsn */
	RAL_WRITE(sc, RT2661_AIFSN_CSR,
	    wmep[WME_AC_BE].wmep_aifsn << 12 |
	    wmep[WME_AC_BK].wmep_aifsn <<  8 |
	    wmep[WME_AC_VI].wmep_aifsn <<  4 |
	    wmep[WME_AC_VO].wmep_aifsn);

	return 0;
}
#endif

static void
rt2661_updateslot(struct ifnet *ifp)
{
	struct rt2661_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		/*
		 * In HostAP mode, we defer setting of new slot time until
		 * updated ERP Information Element has propagated to all
		 * associated STAs.
		 */
		sc->sc_flags |= RT2661_UPDATE_SLOT;
	} else
		rt2661_set_slottime(sc);
}

static void
rt2661_set_slottime(struct rt2661_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t slottime;
	uint32_t tmp;

	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	tmp = RAL_READ(sc, RT2661_MAC_CSR9);
	tmp = (tmp & ~0xff) | slottime;
	RAL_WRITE(sc, RT2661_MAC_CSR9, tmp);

	DPRINTF(("setting slot time to %uus\n", slottime));
}

static const char *
rt2661_get_rf(int rev)
{
	switch (rev) {
	case RT2661_RF_5225:	return "RT5225";
	case RT2661_RF_5325:	return "RT5325 (MIMO XR)";
	case RT2661_RF_2527:	return "RT2527";
	case RT2661_RF_2529:	return "RT2529 (MIMO XR)";
	default:		return "unknown";
	}
}

static void
rt2661_read_eeprom(struct rt2661_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;
	int i;

	/* read MAC address */
	val = rt2661_eeprom_read(sc, RT2661_EEPROM_MAC01);
	ic->ic_myaddr[0] = val & 0xff;
	ic->ic_myaddr[1] = val >> 8;

	val = rt2661_eeprom_read(sc, RT2661_EEPROM_MAC23);
	ic->ic_myaddr[2] = val & 0xff;
	ic->ic_myaddr[3] = val >> 8;

	val = rt2661_eeprom_read(sc, RT2661_EEPROM_MAC45);
	ic->ic_myaddr[4] = val & 0xff;
	ic->ic_myaddr[5] = val >> 8;

	val = rt2661_eeprom_read(sc, RT2661_EEPROM_ANTENNA);
	/* XXX: test if different from 0xffff? */
	sc->rf_rev   = (val >> 11) & 0x1f;
	sc->hw_radio = (val >> 10) & 0x1;
	sc->rx_ant   = (val >> 4)  & 0x3;
	sc->tx_ant   = (val >> 2)  & 0x3;
	sc->nb_ant   = val & 0x3;

	DPRINTF(("RF revision=%d\n", sc->rf_rev));

	val = rt2661_eeprom_read(sc, RT2661_EEPROM_CONFIG2);
	sc->ext_5ghz_lna = (val >> 6) & 0x1;
	sc->ext_2ghz_lna = (val >> 4) & 0x1;

	DPRINTF(("External 2GHz LNA=%d\nExternal 5GHz LNA=%d\n",
	    sc->ext_2ghz_lna, sc->ext_5ghz_lna));

	val = rt2661_eeprom_read(sc, RT2661_EEPROM_RSSI_2GHZ_OFFSET);
	if ((val & 0xff) != 0xff)
		sc->rssi_2ghz_corr = (int8_t)(val & 0xff);	/* signed */

	val = rt2661_eeprom_read(sc, RT2661_EEPROM_RSSI_5GHZ_OFFSET);
	if ((val & 0xff) != 0xff)
		sc->rssi_5ghz_corr = (int8_t)(val & 0xff);	/* signed */

	/* adjust RSSI correction for external low-noise amplifier */
	if (sc->ext_2ghz_lna)
		sc->rssi_2ghz_corr -= 14;
	if (sc->ext_5ghz_lna)
		sc->rssi_5ghz_corr -= 14;

	DPRINTF(("RSSI 2GHz corr=%d\nRSSI 5GHz corr=%d\n",
	    sc->rssi_2ghz_corr, sc->rssi_5ghz_corr));

	val = rt2661_eeprom_read(sc, RT2661_EEPROM_FREQ_OFFSET);
	if ((val >> 8) != 0xff)
		sc->rfprog = (val >> 8) & 0x3;
	if ((val & 0xff) != 0xff)
		sc->rffreq = val & 0xff;

	DPRINTF(("RF prog=%d\nRF freq=%d\n", sc->rfprog, sc->rffreq));

	/* read Tx power for all a/b/g channels */
	for (i = 0; i < 19; i++) {
		val = rt2661_eeprom_read(sc, RT2661_EEPROM_TXPOWER + i);
		sc->txpow[i * 2] = (int8_t)(val >> 8);		/* signed */
		DPRINTF(("Channel=%d Tx power=%d\n",
		    rt2661_rf5225_1[i * 2].chan, sc->txpow[i * 2]));
		sc->txpow[i * 2 + 1] = (int8_t)(val & 0xff);	/* signed */
		DPRINTF(("Channel=%d Tx power=%d\n",
		    rt2661_rf5225_1[i * 2 + 1].chan, sc->txpow[i * 2 + 1]));
	}

	/* read vendor-specific BBP values */
	for (i = 0; i < 16; i++) {
		val = rt2661_eeprom_read(sc, RT2661_EEPROM_BBP_BASE + i);
		if (val == 0 || val == 0xffff)
			continue;	/* skip invalid entries */
		sc->bbp_prom[i].reg = val >> 8;
		sc->bbp_prom[i].val = val & 0xff;
		DPRINTF(("BBP R%d=%02x\n", sc->bbp_prom[i].reg,
		    sc->bbp_prom[i].val));
	}
}

static int
rt2661_bbp_init(struct rt2661_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;
	uint8_t val;

	/* wait for BBP to be ready */
	for (ntries = 0; ntries < 100; ntries++) {
		val = rt2661_bbp_read(sc, 0);
		if (val != 0 && val != 0xff)
			break;
		DELAY(100);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev, "timeout waiting for BBP\n");
		return EIO;
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N(rt2661_def_bbp); i++) {
		rt2661_bbp_write(sc, rt2661_def_bbp[i].reg,
		    rt2661_def_bbp[i].val);
	}

	/* write vendor-specific BBP values (from EEPROM) */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0)
			continue;
		rt2661_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}

	return 0;
#undef N
}

static int
rt2661_init(struct ifnet *ifp)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct rt2661_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	const char *name = NULL;	/* make lint happy */
	uint8_t *ucode;
	size_t size;
	uint32_t tmp, star[3];
	int i, ntries;
	firmware_handle_t fh;

	/* for CardBus, power on the socket */
	if (!(sc->sc_flags & RT2661_ENABLED)) {
		if (sc->sc_enable != NULL && (*sc->sc_enable)(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "could not enable device\n");
			return EIO;
		}
		sc->sc_flags |= RT2661_ENABLED;
	}

	rt2661_stop(ifp, 0);

	if (!(sc->sc_flags & RT2661_FWLOADED)) {
		switch (sc->sc_id) {
		case PCI_PRODUCT_RALINK_RT2561:
			name = "ral-rt2561";
			break;
		case PCI_PRODUCT_RALINK_RT2561S:
			name = "ral-rt2561s";
			break;
		case PCI_PRODUCT_RALINK_RT2661:
			name = "ral-rt2661";
			break;
		}

		if (firmware_open("ral", name, &fh) != 0) {
			aprint_error_dev(sc->sc_dev, "could not open microcode %s\n", name);
			rt2661_stop(ifp, 1);
			return EIO;
		}

		size = firmware_get_size(fh);
		if (!(ucode = firmware_malloc(size))) {
			aprint_error_dev(sc->sc_dev, "could not alloc microcode memory\n");
			firmware_close(fh);
			rt2661_stop(ifp, 1);
			return ENOMEM;
		}

		if (firmware_read(fh, 0, ucode, size) != 0) {
			aprint_error_dev(sc->sc_dev, "could not read microcode %s\n", name);
			firmware_free(ucode, size);
			firmware_close(fh);
			rt2661_stop(ifp, 1);
			return EIO;
		}

		if (rt2661_load_microcode(sc, ucode, size) != 0) {
			aprint_error_dev(sc->sc_dev, "could not load 8051 microcode\n");
			firmware_free(ucode, size);
			firmware_close(fh);
			rt2661_stop(ifp, 1);
			return EIO;
		}

		firmware_free(ucode, size);
		firmware_close(fh);
		sc->sc_flags |= RT2661_FWLOADED;
	}

	/* initialize Tx rings */
	RAL_WRITE(sc, RT2661_AC1_BASE_CSR, sc->txq[1].physaddr);
	RAL_WRITE(sc, RT2661_AC0_BASE_CSR, sc->txq[0].physaddr);
	RAL_WRITE(sc, RT2661_AC2_BASE_CSR, sc->txq[2].physaddr);
	RAL_WRITE(sc, RT2661_AC3_BASE_CSR, sc->txq[3].physaddr);

	/* initialize Mgt ring */
	RAL_WRITE(sc, RT2661_MGT_BASE_CSR, sc->mgtq.physaddr);

	/* initialize Rx ring */
	RAL_WRITE(sc, RT2661_RX_BASE_CSR, sc->rxq.physaddr);

	/* initialize Tx rings sizes */
	RAL_WRITE(sc, RT2661_TX_RING_CSR0,
	    RT2661_TX_RING_COUNT << 24 |
	    RT2661_TX_RING_COUNT << 16 |
	    RT2661_TX_RING_COUNT <<  8 |
	    RT2661_TX_RING_COUNT);

	RAL_WRITE(sc, RT2661_TX_RING_CSR1,
	    RT2661_TX_DESC_WSIZE << 16 |
	    RT2661_TX_RING_COUNT <<  8 |	/* XXX: HCCA ring unused */
	    RT2661_MGT_RING_COUNT);

	/* initialize Rx rings */
	RAL_WRITE(sc, RT2661_RX_RING_CSR,
	    RT2661_RX_DESC_BACK  << 16 |
	    RT2661_RX_DESC_WSIZE <<  8 |
	    RT2661_RX_RING_COUNT);

	/* XXX: some magic here */
	RAL_WRITE(sc, RT2661_TX_DMA_DST_CSR, 0xaa);

	/* load base addresses of all 5 Tx rings (4 data + 1 mgt) */
	RAL_WRITE(sc, RT2661_LOAD_TX_RING_CSR, 0x1f);

	/* load base address of Rx ring */
	RAL_WRITE(sc, RT2661_RX_CNTL_CSR, 2);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(rt2661_def_mac); i++)
		RAL_WRITE(sc, rt2661_def_mac[i].reg, rt2661_def_mac[i].val);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	rt2661_set_macaddr(sc, ic->ic_myaddr);

	/* set host ready */
	RAL_WRITE(sc, RT2661_MAC_CSR1, 3);
	RAL_WRITE(sc, RT2661_MAC_CSR1, 0);

	/* wait for BBP/RF to wakeup */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (RAL_READ(sc, RT2661_MAC_CSR12) & 8)
			break;
		DELAY(1000);
	}
	if (ntries == 1000) {
		printf("timeout waiting for BBP/RF to wakeup\n");
		rt2661_stop(ifp, 1);
		return EIO;
	}

	if (rt2661_bbp_init(sc) != 0) {
		rt2661_stop(ifp, 1);
		return EIO;
	}

	/* select default channel */
	sc->sc_curchan = ic->ic_curchan;
	rt2661_select_band(sc, sc->sc_curchan);
	rt2661_select_antenna(sc);
	rt2661_set_chan(sc, sc->sc_curchan);

	/* update Rx filter */
	tmp = RAL_READ(sc, RT2661_TXRX_CSR0) & 0xffff;

	tmp |= RT2661_DROP_PHY_ERROR | RT2661_DROP_CRC_ERROR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RT2661_DROP_CTL | RT2661_DROP_VER_ERROR |
		       RT2661_DROP_ACKCTS;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			tmp |= RT2661_DROP_TODS;
		if (!(ifp->if_flags & IFF_PROMISC))
			tmp |= RT2661_DROP_NOT_TO_ME;
	}

	RAL_WRITE(sc, RT2661_TXRX_CSR0, tmp);

	/* clear STA registers */
	RAL_READ_REGION_4(sc, RT2661_STA_CSR0, star, N(star));

	/* initialize ASIC */
	RAL_WRITE(sc, RT2661_MAC_CSR1, 4);

	/* clear any pending interrupt */
	RAL_WRITE(sc, RT2661_INT_SOURCE_CSR, 0xffffffff);

	/* enable interrupts */
	RAL_WRITE(sc, RT2661_INT_MASK_CSR, 0x0000ff10);
	RAL_WRITE(sc, RT2661_MCU_INT_MASK_CSR, 0);

	/* kick Rx */
	RAL_WRITE(sc, RT2661_RX_CNTL_CSR, 1);

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	return 0;
#undef N
}

static void
rt2661_stop(struct ifnet *ifp, int disable)
{
	struct rt2661_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);	/* free all nodes */

	/* abort Tx (for all 5 Tx rings) */
	RAL_WRITE(sc, RT2661_TX_CNTL_CSR, 0x1f << 16);

	/* disable Rx (value remains after reset!) */
	tmp = RAL_READ(sc, RT2661_TXRX_CSR0);
	RAL_WRITE(sc, RT2661_TXRX_CSR0, tmp | RT2661_DISABLE_RX);

	/* reset ASIC */
	RAL_WRITE(sc, RT2661_MAC_CSR1, 3);
	RAL_WRITE(sc, RT2661_MAC_CSR1, 0);

	/* disable interrupts */
	RAL_WRITE(sc, RT2661_INT_MASK_CSR, 0xffffff7f);
	RAL_WRITE(sc, RT2661_MCU_INT_MASK_CSR, 0xffffffff);

	/* clear any pending interrupt */
	RAL_WRITE(sc, RT2661_INT_SOURCE_CSR, 0xffffffff);
	RAL_WRITE(sc, RT2661_MCU_INT_SOURCE_CSR, 0xffffffff);

	/* reset Tx and Rx rings */
	rt2661_reset_tx_ring(sc, &sc->txq[0]);
	rt2661_reset_tx_ring(sc, &sc->txq[1]);
	rt2661_reset_tx_ring(sc, &sc->txq[2]);
	rt2661_reset_tx_ring(sc, &sc->txq[3]);
	rt2661_reset_tx_ring(sc, &sc->mgtq);
	rt2661_reset_rx_ring(sc, &sc->rxq);

	/* for CardBus, power down the socket */
	if (disable && sc->sc_disable != NULL) {
		if (sc->sc_flags & RT2661_ENABLED) {
			(*sc->sc_disable)(sc);
			sc->sc_flags &= ~(RT2661_ENABLED | RT2661_FWLOADED);
		}
	}
}

static int
rt2661_load_microcode(struct rt2661_softc *sc, const uint8_t *ucode, int size)
{
	int ntries;

	/* reset 8051 */
	RAL_WRITE(sc, RT2661_MCU_CNTL_CSR, RT2661_MCU_RESET);

	/* cancel any pending Host to MCU command */
	RAL_WRITE(sc, RT2661_H2M_MAILBOX_CSR, 0);
	RAL_WRITE(sc, RT2661_M2H_CMD_DONE_CSR, 0xffffffff);
	RAL_WRITE(sc, RT2661_HOST_CMD_CSR, 0);

	/* write 8051's microcode */
	RAL_WRITE(sc, RT2661_MCU_CNTL_CSR, RT2661_MCU_RESET | RT2661_MCU_SEL);
	RAL_WRITE_REGION_1(sc, RT2661_MCU_CODE_BASE, ucode, size);
	RAL_WRITE(sc, RT2661_MCU_CNTL_CSR, RT2661_MCU_RESET);

	/* kick 8051's ass */
	RAL_WRITE(sc, RT2661_MCU_CNTL_CSR, 0);

	/* wait for 8051 to initialize */
	for (ntries = 0; ntries < 500; ntries++) {
		if (RAL_READ(sc, RT2661_MCU_CNTL_CSR) & RT2661_MCU_READY)
			break;
		DELAY(100);
	}
	if (ntries == 500) {
		printf("timeout waiting for MCU to initialize\n");
		return EIO;
	}
	return 0;
}

/*
 * Dynamically tune Rx sensitivity (BBP register 17) based on average RSSI and
 * false CCA count.  This function is called periodically (every seconds) when
 * in the RUN state.  Values taken from the reference driver.
 */
static void
rt2661_rx_tune(struct rt2661_softc *sc)
{
	uint8_t bbp17;
	uint16_t cca;
	int lo, hi, dbm;

	/*
	 * Tuning range depends on operating band and on the presence of an
	 * external low-noise amplifier.
	 */
	lo = 0x20;
	if (IEEE80211_IS_CHAN_5GHZ(sc->sc_curchan))
		lo += 0x08;
	if ((IEEE80211_IS_CHAN_2GHZ(sc->sc_curchan) && sc->ext_2ghz_lna) ||
	    (IEEE80211_IS_CHAN_5GHZ(sc->sc_curchan) && sc->ext_5ghz_lna))
		lo += 0x10;
	hi = lo + 0x20;

	dbm = sc->avg_rssi;
	/* retrieve false CCA count since last call (clear on read) */
	cca = RAL_READ(sc, RT2661_STA_CSR1) & 0xffff;

	DPRINTFN(2, ("RSSI=%ddBm false CCA=%d\n", dbm, cca));

	if (dbm < -74) {
		/* very bad RSSI, tune using false CCA count */
		bbp17 = sc->bbp17; /* current value */

		hi -= 2 * (-74 - dbm);
		if (hi < lo)
			hi = lo;

		if (bbp17 > hi)
			bbp17 = hi;
		else if (cca > 512)
			bbp17 = min(bbp17 + 1, hi);
		else if (cca < 100)
			bbp17 = max(bbp17 - 1, lo);

	} else if (dbm < -66) {
		bbp17 = lo + 0x08;
	} else if (dbm < -58) {
		bbp17 = lo + 0x10;
	} else if (dbm < -35) {
		bbp17 = hi;
	} else {	/* very good RSSI >= -35dBm */
		bbp17 = 0x60;	/* very low sensitivity */
	}

	if (bbp17 != sc->bbp17) {
		DPRINTF(("BBP17 %x->%x\n", sc->bbp17, bbp17));
		rt2661_bbp_write(sc, 17, bbp17);
		sc->bbp17 = bbp17;
	}
}

#ifdef notyet
/*
 * Enter/Leave radar detection mode.
 * This is for 802.11h additional regulatory domains.
 */
static void
rt2661_radar_start(struct rt2661_softc *sc)
{
	uint32_t tmp;

	/* disable Rx */
	tmp = RAL_READ(sc, RT2661_TXRX_CSR0);
	RAL_WRITE(sc, RT2661_TXRX_CSR0, tmp | RT2661_DISABLE_RX);

	rt2661_bbp_write(sc, 82, 0x20);
	rt2661_bbp_write(sc, 83, 0x00);
	rt2661_bbp_write(sc, 84, 0x40);

	/* save current BBP registers values */
	sc->bbp18 = rt2661_bbp_read(sc, 18);
	sc->bbp21 = rt2661_bbp_read(sc, 21);
	sc->bbp22 = rt2661_bbp_read(sc, 22);
	sc->bbp16 = rt2661_bbp_read(sc, 16);
	sc->bbp17 = rt2661_bbp_read(sc, 17);
	sc->bbp64 = rt2661_bbp_read(sc, 64);

	rt2661_bbp_write(sc, 18, 0xff);
	rt2661_bbp_write(sc, 21, 0x3f);
	rt2661_bbp_write(sc, 22, 0x3f);
	rt2661_bbp_write(sc, 16, 0xbd);
	rt2661_bbp_write(sc, 17, sc->ext_5ghz_lna ? 0x44 : 0x34);
	rt2661_bbp_write(sc, 64, 0x21);

	/* restore Rx filter */
	RAL_WRITE(sc, RT2661_TXRX_CSR0, tmp);
}

static int
rt2661_radar_stop(struct rt2661_softc *sc)
{
	uint8_t bbp66;

	/* read radar detection result */
	bbp66 = rt2661_bbp_read(sc, 66);

	/* restore BBP registers values */
	rt2661_bbp_write(sc, 16, sc->bbp16);
	rt2661_bbp_write(sc, 17, sc->bbp17);
	rt2661_bbp_write(sc, 18, sc->bbp18);
	rt2661_bbp_write(sc, 21, sc->bbp21);
	rt2661_bbp_write(sc, 22, sc->bbp22);
	rt2661_bbp_write(sc, 64, sc->bbp64);

	return bbp66 == 1;
}
#endif

static int
rt2661_prepare_beacon(struct rt2661_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct rt2661_tx_desc desc;
	struct mbuf *m0;
	struct ieee80211_beacon_offsets bo;
	int rate;

	m0 = ieee80211_beacon_alloc(ic, ni, &bo);
	if (m0 == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate beacon frame\n");
		return ENOBUFS;
	}

	/* send beacons at the lowest available rate */
	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 2;

	rt2661_setup_tx_desc(sc, &desc, RT2661_TX_TIMESTAMP, RT2661_TX_HWSEQ,
	    m0->m_pkthdr.len, rate, NULL, 0, RT2661_QID_MGT);

	/* copy the first 24 bytes of Tx descriptor into NIC memory */
	RAL_WRITE_REGION_1(sc, RT2661_HW_BEACON_BASE0, (uint8_t *)&desc, 24);

	/* copy beacon header and payload into NIC memory */
	RAL_WRITE_REGION_1(sc, RT2661_HW_BEACON_BASE0 + 24,
	    mtod(m0, uint8_t *), m0->m_pkthdr.len);

	m_freem(m0);

	/*
	 * Store offset of ERP Information Element so that we can update it
	 * dynamically when the slot time changes.
	 * XXX: this is ugly since it depends on how net80211 builds beacon
	 * frames but ieee80211_beacon_alloc() doesn't store offsets for us.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11G) {
		sc->erp_csr =
		    RT2661_HW_BEACON_BASE0 + 24 +
		    sizeof (struct ieee80211_frame) +
		    8 + 2 + 2 + 2 + ni->ni_esslen +
		    2 + min(ni->ni_rates.rs_nrates, IEEE80211_RATE_SIZE) +
		    2 + 1 +
		    ((ic->ic_opmode == IEEE80211_M_IBSS) ? 4 : 6) +
		    2;
	}

	return 0;
}

/*
 * Enable TSF synchronization and tell h/w to start sending beacons for IBSS
 * and HostAP operating modes.
 */
static void
rt2661_enable_tsf_sync(struct rt2661_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	if (ic->ic_opmode != IEEE80211_M_STA) {
		/*
		 * Change default 16ms TBTT adjustment to 8ms.
		 * Must be done before enabling beacon generation.
		 */
		RAL_WRITE(sc, RT2661_TXRX_CSR10, 1 << 12 | 8);
	}

	tmp = RAL_READ(sc, RT2661_TXRX_CSR9) & 0xff000000;

	/* set beacon interval (in 1/16ms unit) */
	tmp |= ic->ic_bss->ni_intval * 16;

	tmp |= RT2661_TSF_TICKING | RT2661_ENABLE_TBTT;
	if (ic->ic_opmode == IEEE80211_M_STA)
		tmp |= RT2661_TSF_MODE(1);
	else
		tmp |= RT2661_TSF_MODE(2) | RT2661_GENERATE_BEACON;

	RAL_WRITE(sc, RT2661_TXRX_CSR9, tmp);
}

/*
 * Retrieve the "Received Signal Strength Indicator" from the raw values
 * contained in Rx descriptors.  The computation depends on which band the
 * frame was received.  Correction values taken from the reference driver.
 */
static int
rt2661_get_rssi(struct rt2661_softc *sc, uint8_t raw)
{
	int lna, agc, rssi;

	lna = (raw >> 5) & 0x3;
	agc = raw & 0x1f;

	rssi = 2 * agc;

	if (IEEE80211_IS_CHAN_2GHZ(sc->sc_curchan)) {
		rssi += sc->rssi_2ghz_corr;

		if (lna == 1)
			rssi -= 64;
		else if (lna == 2)
			rssi -= 74;
		else if (lna == 3)
			rssi -= 90;
	} else {
		rssi += sc->rssi_5ghz_corr;

		if (lna == 1)
			rssi -= 64;
		else if (lna == 2)
			rssi -= 86;
		else if (lna == 3)
			rssi -= 100;
	}
	return rssi;
}
