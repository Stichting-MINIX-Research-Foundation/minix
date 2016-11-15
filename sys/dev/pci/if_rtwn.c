/*	$NetBSD: if_rtwn.c,v 1.1 2015/08/27 14:04:08 nonaka Exp $	*/
/*	$OpenBSD: if_rtwn.c,v 1.5 2015/06/14 08:02:47 stsp Exp $	*/
#define	IEEE80211_NO_HT
/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2015 Stefan Sperling <stsp@openbsd.org>
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

/*
 * Driver for Realtek RTL8188CE
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_rtwn.c,v 1.1 2015/08/27 14:04:08 nonaka Exp $");

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/mutex.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_rtwnreg.h>

#ifdef RTWN_DEBUG
#define DPRINTF(x)	do { if (rtwn_debug) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (rtwn_debug >= (n)) printf x; } while (0)
int rtwn_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/*
 * PCI configuration space registers.
 */
#define	RTWN_PCI_IOBA		0x10	/* i/o mapped base */
#define	RTWN_PCI_MMBA		0x18	/* memory mapped base */

#define RTWN_INT_ENABLE_TX						\
			(R92C_IMR_VODOK | R92C_IMR_VIDOK | R92C_IMR_BEDOK | \
			 R92C_IMR_BKDOK | R92C_IMR_MGNTDOK | \
			 R92C_IMR_HIGHDOK | R92C_IMR_BDOK)
#define RTWN_INT_ENABLE_RX						\
			(R92C_IMR_ROK | R92C_IMR_RDU | R92C_IMR_RXFOVW)
#define RTWN_INT_ENABLE	(RTWN_INT_ENABLE_TX | RTWN_INT_ENABLE_RX)

static const struct rtwn_device {
	pci_vendor_id_t		rd_vendor;
	pci_product_id_t	rd_product;
} rtwn_devices[] = {
	{ PCI_VENDOR_REALTEK,	PCI_PRODUCT_REALTEK_RTL8188CE },
	{ PCI_VENDOR_REALTEK,	PCI_PRODUCT_REALTEK_RTL8192CE }
};

static int	rtwn_match(device_t, cfdata_t, void *);
static void	rtwn_attach(device_t, device_t, void *);
static int	rtwn_detach(device_t, int);
static int	rtwn_activate(device_t, enum devact);

CFATTACH_DECL_NEW(rtwn, sizeof(struct rtwn_softc), rtwn_match,
    rtwn_attach, rtwn_detach, rtwn_activate);

static int	rtwn_alloc_rx_list(struct rtwn_softc *);
static void	rtwn_reset_rx_list(struct rtwn_softc *);
static void	rtwn_free_rx_list(struct rtwn_softc *);
static void	rtwn_setup_rx_desc(struct rtwn_softc *, struct r92c_rx_desc *,
		    bus_addr_t, size_t, int);
static int	rtwn_alloc_tx_list(struct rtwn_softc *, int);
static void	rtwn_reset_tx_list(struct rtwn_softc *, int);
static void	rtwn_free_tx_list(struct rtwn_softc *, int);
static void	rtwn_write_1(struct rtwn_softc *, uint16_t, uint8_t);
static void	rtwn_write_2(struct rtwn_softc *, uint16_t, uint16_t);
static void	rtwn_write_4(struct rtwn_softc *, uint16_t, uint32_t);
static uint8_t	rtwn_read_1(struct rtwn_softc *, uint16_t);
static uint16_t	rtwn_read_2(struct rtwn_softc *, uint16_t);
static uint32_t	rtwn_read_4(struct rtwn_softc *, uint16_t);
static int	rtwn_fw_cmd(struct rtwn_softc *, uint8_t, const void *, int);
static void	rtwn_rf_write(struct rtwn_softc *, int, uint8_t, uint32_t);
static uint32_t	rtwn_rf_read(struct rtwn_softc *, int, uint8_t);
static int	rtwn_llt_write(struct rtwn_softc *, uint32_t, uint32_t);
static uint8_t	rtwn_efuse_read_1(struct rtwn_softc *, uint16_t);
static void	rtwn_efuse_read(struct rtwn_softc *);
static int	rtwn_read_chipid(struct rtwn_softc *);
static void	rtwn_efuse_switch_power(struct rtwn_softc *);
static void	rtwn_read_rom(struct rtwn_softc *);
static int	rtwn_media_change(struct ifnet *);
static int	rtwn_ra_init(struct rtwn_softc *);
static int	rtwn_get_nettype(struct rtwn_softc *);
static void	rtwn_set_nettype0_msr(struct rtwn_softc *, uint8_t);
static void	rtwn_tsf_sync_enable(struct rtwn_softc *);
static void	rtwn_set_led(struct rtwn_softc *, int, int);
static void	rtwn_calib_to(void *);
static void	rtwn_next_scan(void *);
static void	rtwn_newassoc(struct ieee80211_node *, int);
static int	rtwn_reset(struct ifnet *);
static int	rtwn_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
static int	rtwn_wme_update(struct ieee80211com *);
static void	rtwn_update_avgrssi(struct rtwn_softc *, int, int8_t);
static int8_t	rtwn_get_rssi(struct rtwn_softc *, int, void *);
static void	rtwn_rx_frame(struct rtwn_softc *, struct r92c_rx_desc *,
		    struct rtwn_rx_data *, int);
static int	rtwn_tx(struct rtwn_softc *, struct mbuf *,
		    struct ieee80211_node *);
static void	rtwn_tx_done(struct rtwn_softc *, int);
static void	rtwn_start(struct ifnet *);
static void	rtwn_watchdog(struct ifnet *);
static int	rtwn_ioctl(struct ifnet *, u_long, void *);
static int	rtwn_power_on(struct rtwn_softc *);
static int	rtwn_llt_init(struct rtwn_softc *);
static void	rtwn_fw_reset(struct rtwn_softc *);
static int	rtwn_fw_loadpage(struct rtwn_softc *, int, uint8_t *, int);
static int	rtwn_load_firmware(struct rtwn_softc *);
static int	rtwn_dma_init(struct rtwn_softc *);
static void	rtwn_mac_init(struct rtwn_softc *);
static void	rtwn_bb_init(struct rtwn_softc *);
static void	rtwn_rf_init(struct rtwn_softc *);
static void	rtwn_cam_init(struct rtwn_softc *);
static void	rtwn_pa_bias_init(struct rtwn_softc *);
static void	rtwn_rxfilter_init(struct rtwn_softc *);
static void	rtwn_edca_init(struct rtwn_softc *);
static void	rtwn_write_txpower(struct rtwn_softc *, int, uint16_t[]);
static void	rtwn_get_txpower(struct rtwn_softc *, int,
		    struct ieee80211_channel *, struct ieee80211_channel *,
		    uint16_t[]);
static void	rtwn_set_txpower(struct rtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
static void	rtwn_set_chan(struct rtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
static void	rtwn_iq_calib(struct rtwn_softc *);
static void	rtwn_lc_calib(struct rtwn_softc *);
static void	rtwn_temp_calib(struct rtwn_softc *);
static int	rtwn_init(struct ifnet *);
static void	rtwn_init_task(void *);
static void	rtwn_stop(struct ifnet *, int);
static int	rtwn_intr(void *);

/* Aliases. */
#define	rtwn_bb_write	rtwn_write_4
#define rtwn_bb_read	rtwn_read_4

static const struct rtwn_device *
rtwn_lookup(const struct pci_attach_args *pa)
{
	const struct rtwn_device *rd;
	int i;

	for (i = 0; i < __arraycount(rtwn_devices); i++) {
		rd = &rtwn_devices[i];
		if (PCI_VENDOR(pa->pa_id) == rd->rd_vendor &&
		    PCI_PRODUCT(pa->pa_id) == rd->rd_product)
			return rd;
	}
	return NULL;
}

static int
rtwn_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (rtwn_lookup(pa) != NULL)
		return 1;
	return 0;
}

static void
rtwn_attach(device_t parent, device_t self, void *aux)
{
	struct rtwn_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = GET_IFP(sc);
	int i, error;
	pcireg_t memtype;
#ifndef __HAVE_PCI_MSI_MSIX
	pci_intr_handle_t ih;
#endif
	const char *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;

	pci_aprint_devinfo(pa, NULL);

	callout_init(&sc->scan_to, 0);
	callout_setfunc(&sc->scan_to, rtwn_next_scan, sc);
	callout_init(&sc->calib_to, 0);
	callout_setfunc(&sc->calib_to, rtwn_calib_to, sc);

	sc->init_task = softint_establish(SOFTINT_NET, rtwn_init_task, sc);

	/* Power up the device */
	pci_set_powerstate(pa->pa_pc, pa->pa_tag, PCI_PMCSR_STATE_D0);

	/* Map control/status registers. */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, RTWN_PCI_MMBA);
	error = pci_mapreg_map(pa, RTWN_PCI_MMBA, memtype, 0, &sc->sc_st,
	    &sc->sc_sh, NULL, &sc->sc_mapsize);
	if (error != 0) {
		aprint_error_dev(self, "can't map mem space\n");
		return;
	}

	/* Install interrupt handler. */
#ifdef __HAVE_PCI_MSI_MSIX
	if (pci_intr_alloc(pa, &sc->sc_pihp, NULL, 0)) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(sc->sc_pc, sc->sc_pihp[0], intrbuf,
	    sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pc, sc->sc_pihp[0], IPL_NET,
	    rtwn_intr, sc);
#else	/* !__HAVE_PCI_MSI_MSIX */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(sc->sc_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_NET, rtwn_intr, sc);
#endif	/* __HAVE_PCI_MSI_MSIX */
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	error = rtwn_read_chipid(sc);
	if (error != 0) {
		aprint_error_dev(self, "unsupported test or unknown chip\n");
		return;
	}

	/* Disable PCIe Active State Power Management (ASPM). */
	if (pci_get_capability(sc->sc_pc, sc->sc_tag, PCI_CAP_PCIEXPRESS,
	    &sc->sc_cap_off, NULL)) {
		uint32_t lcsr = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    sc->sc_cap_off + PCIE_LCSR);
		lcsr &= ~(PCIE_LCSR_ASPM_L0S | PCIE_LCSR_ASPM_L1);
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    sc->sc_cap_off + PCIE_LCSR, lcsr);
	}

	/* Allocate Tx/Rx buffers. */
	error = rtwn_alloc_rx_list(sc);
	if (error != 0) {
		aprint_error_dev(self, "could not allocate Rx buffers\n");
		return;
	}
	for (i = 0; i < RTWN_NTXQUEUES; i++) {
		error = rtwn_alloc_tx_list(sc, i);
		if (error != 0) {
			aprint_error_dev(self,
			    "could not allocate Tx buffers\n");
			return;
		}
	}

	/* Determine number of Tx/Rx chains. */
	if (sc->chip & RTWN_CHIP_92C) {
		sc->ntxchains = (sc->chip & RTWN_CHIP_92C_1T2R) ? 1 : 2;
		sc->nrxchains = 2;
	} else {
		sc->ntxchains = 1;
		sc->nrxchains = 1;
	}
	rtwn_read_rom(sc);

	aprint_normal_dev(self, "MAC/BB RTL%s, RF 6052 %dT%dR, address %s\n",
	    (sc->chip & RTWN_CHIP_92C) ? "8192CE" : "8188CE",
	    sc->ntxchains, sc->nrxchains, ether_sprintf(ic->ic_myaddr));

	/*
	 * Setup the 802.11 device.
	 */
	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* Not only, but not used. */
	ic->ic_opmode = IEEE80211_M_STA;	/* Default to BSS mode. */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* Monitor mode supported. */
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_HOSTAP |	/* HostAp mode supported */
	    IEEE80211_C_SHPREAMBLE |	/* Short preamble supported. */
	    IEEE80211_C_SHSLOT |	/* Short slot time supported. */
	    IEEE80211_C_WME |		/* 802.11e */
	    IEEE80211_C_WPA;		/* WPA/RSN. */

#ifndef IEEE80211_NO_HT
	/* Set HT capabilities. */
	ic->ic_htcaps =
	    IEEE80211_HTCAP_CBW20_40 |
	    IEEE80211_HTCAP_DSSSCCK40;
	/* Set supported HT rates. */
	for (i = 0; i < sc->nrxchains; i++)
		ic->ic_sup_mcs[i] = 0xff;
#endif

	/* Set supported .11b and .11g rates. */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* Set supported .11b and .11g channels (1 through 14). */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = rtwn_init;
	ifp->if_ioctl = rtwn_ioctl;
	ifp->if_start = rtwn_start;
	ifp->if_watchdog = rtwn_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	if_initialize(ifp);
	ieee80211_ifattach(ic);
	if_register(ifp);

	/* override default methods */
	ic->ic_newassoc = rtwn_newassoc;
	ic->ic_reset = rtwn_reset;
	ic->ic_wme.wme_update = rtwn_wme_update;

	/* Override state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = rtwn_newstate;
	ieee80211_media_init(ic, rtwn_media_change, ieee80211_media_status);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN,
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RTWN_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RTWN_TX_RADIOTAP_PRESENT);

	ieee80211_announce(ic);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
rtwn_detach(device_t self, int flags)
{
	struct rtwn_softc *sc = device_private(self);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = GET_IFP(sc);
	int s, i;

	callout_stop(&sc->scan_to);
	callout_stop(&sc->calib_to);

	s = splnet();

	if (ifp->if_softc != NULL) {
		rtwn_stop(ifp, 0);

		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		bpf_detach(ifp);
		ieee80211_ifdetach(ic);
		if_detach(ifp);
	}

	/* Free Tx/Rx buffers. */
	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_free_tx_list(sc, i);
	rtwn_free_rx_list(sc);

	splx(s);

	callout_destroy(&sc->scan_to);
	callout_destroy(&sc->calib_to);

	if (sc->init_task != NULL)
		softint_disestablish(sc->init_task);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
#ifdef __HAVE_PCI_MSI_MSIX
		pci_intr_release(sc->sc_pc, sc->sc_pihp, 1);
#endif
	}

	pmf_device_deregister(self);

	return 0;
}

static int
rtwn_activate(device_t self, enum devact act)
{
	struct rtwn_softc *sc = device_private(self);
	struct ifnet *ifp = GET_IFP(sc);

	switch (act) {
	case DVACT_DEACTIVATE:
		if (ifp->if_flags & IFF_RUNNING)
			rtwn_stop(ifp, 0);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static void
rtwn_setup_rx_desc(struct rtwn_softc *sc, struct r92c_rx_desc *desc,
    bus_addr_t addr, size_t len, int idx)
{

	memset(desc, 0, sizeof(*desc));
	desc->rxdw0 = htole32(SM(R92C_RXDW0_PKTLEN, len) |
		((idx == RTWN_RX_LIST_COUNT - 1) ? R92C_RXDW0_EOR : 0));
	desc->rxbufaddr = htole32(addr);
	bus_space_barrier(sc->sc_st, sc->sc_sh, 0, sc->sc_mapsize,
	    BUS_SPACE_BARRIER_WRITE);
	desc->rxdw0 |= htole32(R92C_RXDW0_OWN);
}

static int
rtwn_alloc_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	const size_t size = sizeof(struct r92c_rx_desc) * RTWN_RX_LIST_COUNT;
	int i, error = 0;

	/* Allocate Rx descriptors. */
	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0, BUS_DMA_NOWAIT,
		&rx_ring->map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create rx desc DMA map\n");
		rx_ring->map = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, 0, 0, &rx_ring->seg, 1,
	    &rx_ring->nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate rx desc\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &rx_ring->seg, rx_ring->nsegs,
	    size, (void **)&rx_ring->desc, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0) {
		bus_dmamem_free(sc->sc_dmat, &rx_ring->seg, rx_ring->nsegs);
		rx_ring->desc = NULL;
		aprint_error_dev(sc->sc_dev, "could not map rx desc\n");
		goto fail;
	}
	memset(rx_ring->desc, 0, size);

	error = bus_dmamap_load_raw(sc->sc_dmat, rx_ring->map, &rx_ring->seg,
	    1, size, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load rx desc\n");
		goto fail;
	}

	/* Allocate Rx buffers. */
	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &rx_data->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create rx buf DMA map\n");
			goto fail;
		}

		MGETHDR(rx_data->m, M_DONTWAIT, MT_DATA);
		if (__predict_false(rx_data->m == NULL)) {
			aprint_error_dev(sc->sc_dev,
			    "couldn't allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}
		MCLGET(rx_data->m, M_DONTWAIT);
		if (__predict_false(!(rx_data->m->m_flags & M_EXT))) {
			aprint_error_dev(sc->sc_dev,
			    "couldn't allocate rx mbuf cluster\n");
			m_free(rx_data->m);
			rx_data->m = NULL;
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, rx_data->map,
		    mtod(rx_data->m, void *), MCLBYTES, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not load rx buf DMA map\n");
			goto fail;
		}

		bus_dmamap_sync(sc->sc_dmat, rx_data->map, 0, MCLBYTES,
		    BUS_DMASYNC_PREREAD);

		rtwn_setup_rx_desc(sc, &rx_ring->desc[i],
		    rx_data->map->dm_segs[0].ds_addr, MCLBYTES, i);
	}
fail:	if (error != 0)
		rtwn_free_rx_list(sc);
	return error;
}

static void
rtwn_reset_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	int i;

	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];
		rtwn_setup_rx_desc(sc, &rx_ring->desc[i],
		    rx_data->map->dm_segs[0].ds_addr, MCLBYTES, i);
	}
}

static void
rtwn_free_rx_list(struct rtwn_softc *sc)
{
	struct rtwn_rx_ring *rx_ring = &sc->rx_ring;
	struct rtwn_rx_data *rx_data;
	int i, s;

	s = splnet();

	if (rx_ring->map) {
		if (rx_ring->desc) {
			bus_dmamap_unload(sc->sc_dmat, rx_ring->map);
			bus_dmamem_unmap(sc->sc_dmat, rx_ring->desc,
			    sizeof (struct r92c_rx_desc) * RTWN_RX_LIST_COUNT);
			bus_dmamem_free(sc->sc_dmat, &rx_ring->seg,
			    rx_ring->nsegs);
			rx_ring->desc = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, rx_ring->map);
		rx_ring->map = NULL;
	}

	for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
		rx_data = &rx_ring->rx_data[i];

		if (rx_data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rx_data->map);
			m_freem(rx_data->m);
			rx_data->m = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, rx_data->map);
		rx_data->map = NULL;
	}

	splx(s);
}

static int
rtwn_alloc_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	const size_t size = sizeof(struct r92c_tx_desc) * RTWN_TX_LIST_COUNT;
	int i = 0, error = 0;

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0, BUS_DMA_NOWAIT,
	    &tx_ring->map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create tx ring DMA map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0,
	    &tx_ring->seg, 1, &tx_ring->nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate tx ring DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &tx_ring->seg, tx_ring->nsegs,
	    size, (void **)&tx_ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		bus_dmamem_free(sc->sc_dmat, &tx_ring->seg, tx_ring->nsegs);
		aprint_error_dev(sc->sc_dev, "can't map tx ring DMA memory\n");
		goto fail;
	}
	memset(tx_ring->desc, 0, size);

	error = bus_dmamap_load(sc->sc_dmat, tx_ring->map, tx_ring->desc,
	    size, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load tx ring DMA map\n");
		goto fail;
	}

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc *desc = &tx_ring->desc[i];

		/* setup tx desc */
		desc->nextdescaddr = htole32(tx_ring->map->dm_segs[0].ds_addr
		  + sizeof(*desc) * ((i + 1) % RTWN_TX_LIST_COUNT));

		tx_data = &tx_ring->tx_data[i];
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &tx_data->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create tx buf DMA map\n");
			goto fail;
		}
		tx_data->m = NULL;
		tx_data->ni = NULL;
	}

fail:
	if (error != 0)
		rtwn_free_tx_list(sc, qid);
	return error;
}

static void
rtwn_reset_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	int i;

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		struct r92c_tx_desc *desc = &tx_ring->desc[i];
		struct rtwn_tx_data *tx_data = &tx_ring->tx_data[i];

		memset(desc, 0, sizeof(*desc) -
		    (sizeof(desc->reserved) + sizeof(desc->nextdescaddr64) +
		    sizeof(desc->nextdescaddr)));

		if (tx_data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, tx_data->map);
			m_freem(tx_data->m);
			tx_data->m = NULL;
			ieee80211_free_node(tx_data->ni);
			tx_data->ni = NULL;
		}
	}

	sc->qfullmsk &= ~(1 << qid);
	tx_ring->queued = 0;
	tx_ring->cur = 0;
}

static void
rtwn_free_tx_list(struct rtwn_softc *sc, int qid)
{
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	int i;

	if (tx_ring->map != NULL) {
		if (tx_ring->desc != NULL) {
			bus_dmamap_unload(sc->sc_dmat, tx_ring->map);
			bus_dmamem_unmap(sc->sc_dmat, tx_ring->desc,
			    sizeof (struct r92c_tx_desc) * RTWN_TX_LIST_COUNT);
			bus_dmamem_free(sc->sc_dmat, &tx_ring->seg,
			    tx_ring->nsegs);
		}
		bus_dmamap_destroy(sc->sc_dmat, tx_ring->map);
	}

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		tx_data = &tx_ring->tx_data[i];

		if (tx_data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, tx_data->map);
			m_freem(tx_data->m);
			tx_data->m = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, tx_data->map);
	}

	sc->qfullmsk &= ~(1 << qid);
	tx_ring->queued = 0;
	tx_ring->cur = 0;
}

static void
rtwn_write_1(struct rtwn_softc *sc, uint16_t addr, uint8_t val)
{
	bus_space_write_1(sc->sc_st, sc->sc_sh, addr, val);
}

static void
rtwn_write_2(struct rtwn_softc *sc, uint16_t addr, uint16_t val)
{
	bus_space_write_2(sc->sc_st, sc->sc_sh, addr, htole16(val));
}

static void
rtwn_write_4(struct rtwn_softc *sc, uint16_t addr, uint32_t val)
{
	bus_space_write_4(sc->sc_st, sc->sc_sh, addr, htole32(val));
}

static uint8_t
rtwn_read_1(struct rtwn_softc *sc, uint16_t addr)
{
	return bus_space_read_1(sc->sc_st, sc->sc_sh, addr);
}

static uint16_t
rtwn_read_2(struct rtwn_softc *sc, uint16_t addr)
{
	return le16toh(bus_space_read_2(sc->sc_st, sc->sc_sh, addr));
}

static uint32_t
rtwn_read_4(struct rtwn_softc *sc, uint16_t addr)
{
	return le32toh(bus_space_read_4(sc->sc_st, sc->sc_sh, addr));
}

static int
rtwn_fw_cmd(struct rtwn_softc *sc, uint8_t id, const void *buf, int len)
{
	struct r92c_fw_cmd cmd;
	uint8_t *cp;
	int fwcur;
	int ntries;

	DPRINTFN(3, ("%s: %s: id=0x%02x, buf=%p, len=%d\n",
	    device_xname(sc->sc_dev), __func__, id, buf, len));

	fwcur = sc->fwcur;
	sc->fwcur = (sc->fwcur + 1) % R92C_H2C_NBOX;

	/* Wait for current FW box to be empty. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (!(rtwn_read_1(sc, R92C_HMETFR) & (1 << sc->fwcur)))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev,
		    "could not send firmware command %d\n", id);
		return ETIMEDOUT;
	}

	memset(&cmd, 0, sizeof(cmd));
	KASSERT(len <= sizeof(cmd.msg));
	memcpy(cmd.msg, buf, len);

	/* Write the first word last since that will trigger the FW. */
	cp = (uint8_t *)&cmd;
	if (len >= 4) {
		cmd.id = id | R92C_CMD_FLAG_EXT;
		rtwn_write_2(sc, R92C_HMEBOX_EXT(fwcur), cp[1] + (cp[2] << 8));
		rtwn_write_4(sc, R92C_HMEBOX(fwcur),
		    cp[0] + (cp[3] << 8) + (cp[4] << 16) + (cp[5] << 24));
	} else {
		cmd.id = id;
		rtwn_write_4(sc, R92C_HMEBOX(fwcur),
		    cp[0] + (cp[1] << 8) + (cp[2] << 16) + (cp[3] << 24));
	}

	/* Give firmware some time for processing. */
	DELAY(2000);

	return 0;
}

static void
rtwn_rf_write(struct rtwn_softc *sc, int chain, uint8_t addr, uint32_t val)
{

	rtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R92C_LSSI_PARAM_ADDR, addr) | SM(R92C_LSSI_PARAM_DATA, val));
}

static uint32_t
rtwn_rf_read(struct rtwn_softc *sc, int chain, uint8_t addr)
{
	uint32_t reg[R92C_MAX_CHAINS], val;

	reg[0] = rtwn_bb_read(sc, R92C_HSSI_PARAM2(0));
	if (chain != 0)
		reg[chain] = rtwn_bb_read(sc, R92C_HSSI_PARAM2(chain));

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] & ~R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(chain),
	    RW(reg[chain], R92C_HSSI_PARAM2_READ_ADDR, addr) |
	    R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	rtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] | R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	if (rtwn_bb_read(sc, R92C_HSSI_PARAM1(chain)) & R92C_HSSI_PARAM1_PI)
		val = rtwn_bb_read(sc, R92C_HSPI_READBACK(chain));
	else
		val = rtwn_bb_read(sc, R92C_LSSI_READBACK(chain));
	return MS(val, R92C_LSSI_READBACK_DATA);
}

static int
rtwn_llt_write(struct rtwn_softc *sc, uint32_t addr, uint32_t data)
{
	int ntries;

	rtwn_write_4(sc, R92C_LLT_INIT,
	    SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) |
	    SM(R92C_LLT_INIT_ADDR, addr) |
	    SM(R92C_LLT_INIT_DATA, data));
	/* Wait for write operation to complete. */
	for (ntries = 0; ntries < 20; ntries++) {
		if (MS(rtwn_read_4(sc, R92C_LLT_INIT), R92C_LLT_INIT_OP) ==
		    R92C_LLT_INIT_OP_NO_ACTIVE)
			return 0;
		DELAY(5);
	}
	return ETIMEDOUT;
}

static uint8_t
rtwn_efuse_read_1(struct rtwn_softc *sc, uint16_t addr)
{
	uint32_t reg;
	int ntries;

	reg = rtwn_read_4(sc, R92C_EFUSE_CTRL);
	reg = RW(reg, R92C_EFUSE_CTRL_ADDR, addr);
	reg &= ~R92C_EFUSE_CTRL_VALID;
	rtwn_write_4(sc, R92C_EFUSE_CTRL, reg);
	/* Wait for read operation to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = rtwn_read_4(sc, R92C_EFUSE_CTRL);
		if (reg & R92C_EFUSE_CTRL_VALID)
			return MS(reg, R92C_EFUSE_CTRL_DATA);
		DELAY(5);
	}
	aprint_error_dev(sc->sc_dev,
	    "could not read efuse byte at address 0x%x\n", addr);
	return 0xff;
}

static void
rtwn_efuse_read(struct rtwn_softc *sc)
{
	uint8_t *rom = (uint8_t *)&sc->rom;
	uint32_t reg;
	uint16_t addr = 0;
	uint8_t off, msk;
	int i;

	rtwn_efuse_switch_power(sc);

	memset(&sc->rom, 0xff, sizeof(sc->rom));
	while (addr < 512) {
		reg = rtwn_efuse_read_1(sc, addr);
		if (reg == 0xff)
			break;
		addr++;
		off = reg >> 4;
		msk = reg & 0xf;
		for (i = 0; i < 4; i++) {
			if (msk & (1 << i))
				continue;
			rom[off * 8 + i * 2 + 0] = rtwn_efuse_read_1(sc, addr);
			addr++;
			rom[off * 8 + i * 2 + 1] = rtwn_efuse_read_1(sc, addr);
			addr++;
		}
	}
#ifdef RTWN_DEBUG
	if (rtwn_debug >= 2) {
		/* Dump ROM content. */
		printf("\n");
		for (i = 0; i < sizeof(sc->rom); i++)
			printf("%02x:", rom[i]);
		printf("\n");
	}
#endif
}

static void
rtwn_efuse_switch_power(struct rtwn_softc *sc)
{
	uint32_t reg;

	reg = rtwn_read_2(sc, R92C_SYS_ISO_CTRL);
	if (!(reg & R92C_SYS_ISO_CTRL_PWC_EV12V)) {
		rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
		    reg | R92C_SYS_ISO_CTRL_PWC_EV12V);
	}
	reg = rtwn_read_2(sc, R92C_SYS_FUNC_EN);
	if (!(reg & R92C_SYS_FUNC_EN_ELDR)) {
		rtwn_write_2(sc, R92C_SYS_FUNC_EN,
		    reg | R92C_SYS_FUNC_EN_ELDR);
	}
	reg = rtwn_read_2(sc, R92C_SYS_CLKR);
	if ((reg & (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) !=
	    (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) {
		rtwn_write_2(sc, R92C_SYS_CLKR,
		    reg | R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M);
	}
}

/* rtwn_read_chipid: reg=0x40073b chipid=0x0 */
static int
rtwn_read_chipid(struct rtwn_softc *sc)
{
	uint32_t reg;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	reg = rtwn_read_4(sc, R92C_SYS_CFG);
	DPRINTF(("%s: version=0x%08x\n", device_xname(sc->sc_dev), reg));
	if (reg & R92C_SYS_CFG_TRP_VAUX_EN)
		/* Unsupported test chip. */
		return EIO;

	if (reg & R92C_SYS_CFG_TYPE_92C) {
		sc->chip |= RTWN_CHIP_92C;
		/* Check if it is a castrated 8192C. */
		if (MS(rtwn_read_4(sc, R92C_HPON_FSM),
		    R92C_HPON_FSM_CHIP_BONDING_ID) ==
		    R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R)
			sc->chip |= RTWN_CHIP_92C_1T2R;
	}
	if (reg & R92C_SYS_CFG_VENDOR_UMC) {
		sc->chip |= RTWN_CHIP_UMC;
		if (MS(reg, R92C_SYS_CFG_CHIP_VER_RTL) == 0)
			sc->chip |= RTWN_CHIP_UMC_A_CUT;
	} else if (MS(reg, R92C_SYS_CFG_CHIP_VER_RTL) != 0) {
		if (MS(reg, R92C_SYS_CFG_CHIP_VER_RTL) == 1)
			sc->chip |= RTWN_CHIP_UMC | RTWN_CHIP_UMC_B_CUT;
		else
			/* Unsupported unknown chip. */
			return EIO;
	}
	return 0;
}

static void
rtwn_read_rom(struct rtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Read full ROM image. */
	rtwn_efuse_read(sc);

	if (rom->id != 0x8129) {
		aprint_error_dev(sc->sc_dev, "invalid EEPROM ID 0x%x\n",
		    rom->id);
	}

	/* XXX Weird but this is what the vendor driver does. */
	sc->pa_setting = rtwn_efuse_read_1(sc, 0x1fa);
	sc->board_type = MS(rom->rf_opt1, R92C_ROM_RF1_BOARD_TYPE);
	sc->regulatory = MS(rom->rf_opt1, R92C_ROM_RF1_REGULATORY);

	DPRINTF(("PA setting=0x%x, board=0x%x, regulatory=%d\n",
	    sc->pa_setting, sc->board_type, sc->regulatory));

	IEEE80211_ADDR_COPY(ic->ic_myaddr, rom->macaddr);
}

static int
rtwn_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		rtwn_stop(ifp, 0);
		error = rtwn_init(ifp);
	}
	return error;
}

/*
 * Initialize rate adaptation in firmware.
 */
static int
rtwn_ra_init(struct rtwn_softc *sc)
{
	static const uint8_t map[] = {
		2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108
	};
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	struct r92c_fw_cmd_macid_cfg cmd;
	uint32_t rates, basicrates;
	uint8_t mode;
	int maxrate, maxbasicrate, error, i, j;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Get normal and basic rates mask. */
	rates = basicrates = 0;
	maxrate = maxbasicrate = 0;
	for (i = 0; i < rs->rs_nrates; i++) {
		/* Convert 802.11 rate to HW rate index. */
		for (j = 0; j < __arraycount(map); j++)
			if ((rs->rs_rates[i] & IEEE80211_RATE_VAL) == map[j])
				break;
		if (j == __arraycount(map))	/* Unknown rate, skip. */
			continue;
		rates |= 1 << j;
		if (j > maxrate)
			maxrate = j;
		if (rs->rs_rates[i] & IEEE80211_RATE_BASIC) {
			basicrates |= 1 << j;
			if (j > maxbasicrate)
				maxbasicrate = j;
		}
	}
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mode = R92C_RAID_11B;
	else
		mode = R92C_RAID_11BG;
	DPRINTF(("%s: mode=0x%x rates=0x%08x, basicrates=0x%08x\n",
	    device_xname(sc->sc_dev), mode, rates, basicrates));
	if (basicrates == 0)
		basicrates |= 1;	/* add 1Mbps */

	/* Set rates mask for group addressed frames. */
	cmd.macid = RTWN_MACID_BC | RTWN_MACID_VALID;
	cmd.mask = htole32((mode << 28) | basicrates);
	error = rtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not add broadcast station\n");
		return error;
	}
	/* Set initial MRR rate. */
	DPRINTF(("%s: maxbasicrate=%d\n", device_xname(sc->sc_dev),
	    maxbasicrate));
	rtwn_write_1(sc, R92C_INIDATA_RATE_SEL(RTWN_MACID_BC), maxbasicrate);

	/* Set rates mask for unicast frames. */
	cmd.macid = RTWN_MACID_BSS | RTWN_MACID_VALID;
	cmd.mask = htole32((mode << 28) | rates);
	error = rtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not add BSS station\n");
		return error;
	}
	/* Set initial MRR rate. */
	DPRINTF(("%s: maxrate=%d\n", device_xname(sc->sc_dev), maxrate));
	rtwn_write_1(sc, R92C_INIDATA_RATE_SEL(RTWN_MACID_BSS), maxrate);

	/* Configure Automatic Rate Fallback Register. */
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		if (rates & 0x0c)
			rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0d));
		else
			rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0f));
	} else
		rtwn_write_4(sc, R92C_ARFR(0), htole32(rates & 0x0ff5));

	/* Indicate highest supported rate. */
	ni->ni_txrate = rs->rs_nrates - 1;
	return 0;
}

static int
rtwn_get_nettype(struct rtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int type;

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		type = R92C_CR_NETTYPE_INFRA;
		break;

	case IEEE80211_M_HOSTAP:
		type = R92C_CR_NETTYPE_AP;
		break;

	case IEEE80211_M_IBSS:
		type = R92C_CR_NETTYPE_ADHOC;
		break;

	default:
		type = R92C_CR_NETTYPE_NOLINK;
		break;
	}

	return type;
}

static void
rtwn_set_nettype0_msr(struct rtwn_softc *sc, uint8_t type)
{
	uint32_t reg;

	reg = rtwn_read_4(sc, R92C_CR);
	reg = RW(reg, R92C_CR_NETTYPE, type);
	rtwn_write_4(sc, R92C_CR, reg);
}

static void
rtwn_tsf_sync_enable(struct rtwn_softc *sc)
{
	struct ieee80211_node *ni = sc->sc_ic.ic_bss;
	uint64_t tsf;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Enable TSF synchronization. */
	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_DIS_TSF_UDT0);

	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_EN_BCN);

	/* Set initial TSF. */
	tsf = ni->ni_tstamp.tsf;
	tsf = le64toh(tsf);
	tsf = tsf - (tsf % (ni->ni_intval * IEEE80211_DUR_TU));
	tsf -= IEEE80211_DUR_TU;
	rtwn_write_4(sc, R92C_TSFTR + 0, (uint32_t)tsf);
	rtwn_write_4(sc, R92C_TSFTR + 4, (uint32_t)(tsf >> 32));

	rtwn_write_1(sc, R92C_BCN_CTRL,
	    rtwn_read_1(sc, R92C_BCN_CTRL) | R92C_BCN_CTRL_EN_BCN);
}

static void
rtwn_set_led(struct rtwn_softc *sc, int led, int on)
{
	uint8_t reg;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	if (led == RTWN_LED_LINK) {
		reg = rtwn_read_1(sc, R92C_LEDCFG2) & 0xf0;
		if (!on)
			reg |= R92C_LEDCFG2_DIS;
		else
			reg |= R92C_LEDCFG2_EN;
		rtwn_write_1(sc, R92C_LEDCFG2, reg);
		sc->ledlink = on;	/* Save LED state. */
	}
}

static void
rtwn_calib_to(void *arg)
{
	struct rtwn_softc *sc = arg;
	struct r92c_fw_cmd_rssi cmd;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	if (sc->sc_ic.ic_state != IEEE80211_S_RUN)
		goto restart_timer;

	if (sc->avg_pwdb != -1) {
		/* Indicate Rx signal strength to FW for rate adaptation. */
		memset(&cmd, 0, sizeof(cmd));
		cmd.macid = 0;	/* BSS. */
		cmd.pwdb = sc->avg_pwdb;
		DPRINTFN(3, ("sending RSSI command avg=%d\n", sc->avg_pwdb));
		rtwn_fw_cmd(sc, R92C_CMD_RSSI_SETTING, &cmd, sizeof(cmd));
	}

	/* Do temperature compensation. */
	rtwn_temp_calib(sc);

 restart_timer:
	callout_schedule(&sc->calib_to, mstohz(2000));
}

static void
rtwn_next_scan(void *arg)
{
	struct rtwn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
	splx(s);
}

static void
rtwn_newassoc(struct ieee80211_node *ni, int isnew)
{

	DPRINTF(("%s: new node %s\n", __func__, ether_sprintf(ni->ni_macaddr)));

	/* start with lowest Tx rate */
	ni->ni_txrate = 0;
}

static int
rtwn_reset(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		return ENETRESET;

	rtwn_set_chan(sc, ic->ic_curchan, NULL);

	return 0;
}

static int
rtwn_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct rtwn_softc *sc = IC2IFP(ic)->if_softc;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate = ic->ic_state;
	uint32_t reg;
	int s;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	s = splnet();

	callout_stop(&sc->scan_to);
	callout_stop(&sc->calib_to);

	if (ostate != nstate) {
		DPRINTF(("%s: %s -> %s\n", __func__,
		    ieee80211_state_name[ostate],
		    ieee80211_state_name[nstate]));
	}

	switch (ostate) {
	case IEEE80211_S_INIT:
		break;

	case IEEE80211_S_SCAN:
		if (nstate != IEEE80211_S_SCAN) {
			/*
			 * End of scanning
			 */
			/* flush 4-AC Queue after site_survey */
			rtwn_write_1(sc, R92C_TXPAUSE, 0x0);

			/* Allow Rx from our BSSID only. */
			rtwn_write_4(sc, R92C_RCR,
			    rtwn_read_4(sc, R92C_RCR) |
			      R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);
		}
		break;

	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		break;

	case IEEE80211_S_RUN:
		/* Turn link LED off. */
		rtwn_set_led(sc, RTWN_LED_LINK, 0);

		/* Set media status to 'No Link'. */
		rtwn_set_nettype0_msr(sc, R92C_CR_NETTYPE_NOLINK);

		/* Stop Rx of data frames. */
		rtwn_write_2(sc, R92C_RXFLTMAP2, 0);

		/* Rest TSF. */
		rtwn_write_1(sc, R92C_DUAL_TSF_RST, 0x03);

		/* Disable TSF synchronization. */
		rtwn_write_1(sc, R92C_BCN_CTRL,
		    rtwn_read_1(sc, R92C_BCN_CTRL) |
		    R92C_BCN_CTRL_DIS_TSF_UDT0);

		/* Back to 20MHz mode */
		rtwn_set_chan(sc, ic->ic_curchan, NULL);

		/* Reset EDCA parameters. */
		rtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3217);
		rtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4317);
		rtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x00105320);
		rtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a444);

		/* flush all cam entries */
		rtwn_cam_init(sc);
		break;
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		/* Turn link LED off. */
		rtwn_set_led(sc, RTWN_LED_LINK, 0);
		break;

	case IEEE80211_S_SCAN:
		if (ostate != IEEE80211_S_SCAN) {
			/*
			 * Begin of scanning
			 */

			/* Set gain for scanning. */
			reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

			reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);

			/* Allow Rx from any BSSID. */
			rtwn_write_4(sc, R92C_RCR,
			    rtwn_read_4(sc, R92C_RCR) &
			    ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));

			/* Stop Rx of data frames. */
			rtwn_write_2(sc, R92C_RXFLTMAP2, 0);

			/* Disable update TSF */
			rtwn_write_1(sc, R92C_BCN_CTRL,
			    rtwn_read_1(sc, R92C_BCN_CTRL) |
			      R92C_BCN_CTRL_DIS_TSF_UDT0);
		}

		/* Make link LED blink during scan. */
		rtwn_set_led(sc, RTWN_LED_LINK, !sc->ledlink);

		/* Pause AC Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE,
		    rtwn_read_1(sc, R92C_TXPAUSE) | 0x0f);

		rtwn_set_chan(sc, ic->ic_curchan, NULL);

		/* Start periodic scan. */
		callout_schedule(&sc->scan_to, mstohz(200));
		break;

	case IEEE80211_S_AUTH:
		/* Set initial gain under link. */
		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
#ifdef doaslinux
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
#else
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
#endif
		rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
#ifdef doaslinux
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
#else
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
#endif
		rtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);

		/* Set media status to 'No Link'. */
		rtwn_set_nettype0_msr(sc, R92C_CR_NETTYPE_NOLINK);

		/* Allow Rx from any BSSID. */
		rtwn_write_4(sc, R92C_RCR,
		    rtwn_read_4(sc, R92C_RCR) &
		      ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));

		rtwn_set_chan(sc, ic->ic_curchan, NULL);
		break;

	case IEEE80211_S_ASSOC:
		break;

	case IEEE80211_S_RUN:
		ni = ic->ic_bss;

		rtwn_set_chan(sc, ic->ic_curchan, NULL);

		if (ic->ic_opmode == IEEE80211_M_MONITOR) {
			/* Back to 20Mhz mode */
			rtwn_set_chan(sc, ic->ic_curchan, NULL);

			/* Set media status to 'No Link'. */
			rtwn_set_nettype0_msr(sc, R92C_CR_NETTYPE_NOLINK);

			/* Enable Rx of data frames. */
			rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

			/* Allow Rx from any BSSID. */
			rtwn_write_4(sc, R92C_RCR,
			    rtwn_read_4(sc, R92C_RCR) &
			    ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));

			/* Accept Rx data/control/management frames */
			rtwn_write_4(sc, R92C_RCR,
			    rtwn_read_4(sc, R92C_RCR) |
			    R92C_RCR_ADF | R92C_RCR_ACF | R92C_RCR_AMF);

			/* Turn link LED on. */
			rtwn_set_led(sc, RTWN_LED_LINK, 1);
			break;
		}

		/* Set media status to 'Associated'. */
		rtwn_set_nettype0_msr(sc, rtwn_get_nettype(sc));

		/* Set BSSID. */
		rtwn_write_4(sc, R92C_BSSID + 0, LE_READ_4(&ni->ni_bssid[0]));
		rtwn_write_4(sc, R92C_BSSID + 4, LE_READ_2(&ni->ni_bssid[4]));

		if (ic->ic_curmode == IEEE80211_MODE_11B)
			rtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 0);
		else	/* 802.11b/g */
			rtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 3);

		/* Enable Rx of data frames. */
		rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

		/* Flush all AC queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0);

		/* Set beacon interval. */
		rtwn_write_2(sc, R92C_BCN_INTERVAL, ni->ni_intval);

		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			/* Allow Rx from our BSSID only. */
			rtwn_write_4(sc, R92C_RCR,
			    rtwn_read_4(sc, R92C_RCR) |
			      R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);

			/* Enable TSF synchronization. */
			rtwn_tsf_sync_enable(sc);
			break;

		case IEEE80211_M_HOSTAP:
			rtwn_write_2(sc, R92C_BCNTCFG, 0x000f);

			/* Allow Rx from any BSSID. */
			rtwn_write_4(sc, R92C_RCR,
			    rtwn_read_4(sc, R92C_RCR) &
			    ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));

			/* Reset TSF timer to zero. */
			reg = rtwn_read_4(sc, R92C_TCR);
			reg &= ~0x01;
			rtwn_write_4(sc, R92C_TCR, reg);
			reg |= 0x01;
			rtwn_write_4(sc, R92C_TCR, reg);
			break;

		case IEEE80211_M_MONITOR:
		default:
			break;
		}

		rtwn_write_1(sc, R92C_SIFS_CCK + 1, 10);
		rtwn_write_1(sc, R92C_SIFS_OFDM + 1, 10);
		rtwn_write_1(sc, R92C_SPEC_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_MAC_SPEC_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_R2T_SIFS + 1, 10);
		rtwn_write_1(sc, R92C_T2T_SIFS + 1, 10);

		/* Intialize rate adaptation. */
		rtwn_ra_init(sc);

		/* Turn link LED on. */
		rtwn_set_led(sc, RTWN_LED_LINK, 1);

		/* Reset average RSSI. */
		sc->avg_pwdb = -1;

		/* Reset temperature calibration state machine. */
		sc->thcal_state = 0;
		sc->thcal_lctemp = 0;

		/* Start periodic calibration. */
		callout_schedule(&sc->calib_to, mstohz(2000));
		break;
	}

	(void)sc->sc_newstate(ic, nstate, arg);

	splx(s);

	return 0;
}

static int
rtwn_wme_update(struct ieee80211com *ic)
{
	static const uint16_t aci2reg[WME_NUM_AC] = {
		R92C_EDCA_BE_PARAM,
		R92C_EDCA_BK_PARAM,
		R92C_EDCA_VI_PARAM,
		R92C_EDCA_VO_PARAM
	};
	struct rtwn_softc *sc = IC2IFP(ic)->if_softc;
	const struct wmeParams *wmep;
	int s, aci, aifs, slottime;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	s = splnet();
	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	for (aci = 0; aci < WME_NUM_AC; aci++) {
		wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[aci];
		/* AIFS[AC] = AIFSN[AC] * aSlotTime + aSIFSTime. */
		aifs = wmep->wmep_aifsn * slottime + 10;
		rtwn_write_4(sc, aci2reg[aci],
		    SM(R92C_EDCA_PARAM_TXOP, wmep->wmep_txopLimit) |
		    SM(R92C_EDCA_PARAM_ECWMIN, wmep->wmep_logcwmin) |
		    SM(R92C_EDCA_PARAM_ECWMAX, wmep->wmep_logcwmax) |
		    SM(R92C_EDCA_PARAM_AIFS, aifs));
	}
	splx(s);

	return 0;
}

static void
rtwn_update_avgrssi(struct rtwn_softc *sc, int rate, int8_t rssi)
{
	int pwdb;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Convert antenna signal to percentage. */
	if (rssi <= -100 || rssi >= 20)
		pwdb = 0;
	else if (rssi >= 0)
		pwdb = 100;
	else
		pwdb = 100 + rssi;
	if (rate <= 3) {
		/* CCK gain is smaller than OFDM/MCS gain. */
		pwdb += 6;
		if (pwdb > 100)
			pwdb = 100;
		if (pwdb <= 14)
			pwdb -= 4;
		else if (pwdb <= 26)
			pwdb -= 8;
		else if (pwdb <= 34)
			pwdb -= 6;
		else if (pwdb <= 42)
			pwdb -= 2;
	}
	if (sc->avg_pwdb == -1)	/* Init. */
		sc->avg_pwdb = pwdb;
	else if (sc->avg_pwdb < pwdb)
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20) + 1;
	else
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20);
	DPRINTFN(4, ("PWDB=%d EMA=%d\n", pwdb, sc->avg_pwdb));
}

static int8_t
rtwn_get_rssi(struct rtwn_softc *sc, int rate, void *physt)
{
	static const int8_t cckoff[] = { 16, -12, -26, -46 };
	struct r92c_rx_phystat *phy;
	struct r92c_rx_cck *cck;
	uint8_t rpt;
	int8_t rssi;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	if (rate <= 3) {
		cck = (struct r92c_rx_cck *)physt;
		if (sc->sc_flags & RTWN_FLAG_CCK_HIPWR) {
			rpt = (cck->agc_rpt >> 5) & 0x3;
			rssi = (cck->agc_rpt & 0x1f) << 1;
		} else {
			rpt = (cck->agc_rpt >> 6) & 0x3;
			rssi = cck->agc_rpt & 0x3e;
		}
		rssi = cckoff[rpt] - rssi;
	} else {	/* OFDM/HT. */
		phy = (struct r92c_rx_phystat *)physt;
		rssi = ((le32toh(phy->phydw1) >> 1) & 0x7f) - 110;
	}
	return rssi;
}

static void
rtwn_rx_frame(struct rtwn_softc *sc, struct r92c_rx_desc *rx_desc,
    struct rtwn_rx_data *rx_data, int desc_idx)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct r92c_rx_phystat *phy = NULL;
	uint32_t rxdw0, rxdw3;
	struct mbuf *m, *m1;
	uint8_t rate;
	int8_t rssi = 0;
	int infosz, pktlen, shift, totlen, error;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	rxdw0 = le32toh(rx_desc->rxdw0);
	rxdw3 = le32toh(rx_desc->rxdw3);

	if (__predict_false(rxdw0 & (R92C_RXDW0_CRCERR | R92C_RXDW0_ICVERR))) {
		/*
		 * This should not happen since we setup our Rx filter
		 * to not receive these frames.
		 */
		ifp->if_ierrors++;
		return;
	}

	pktlen = MS(rxdw0, R92C_RXDW0_PKTLEN);
        /*
	 * XXX: This will drop most control packets.  Do we really
	 * want this in IEEE80211_M_MONITOR mode?
	 */
	if (__predict_false(pktlen < (int)sizeof(struct ieee80211_frame_ack))) {
		ic->ic_stats.is_rx_tooshort++;
		ifp->if_ierrors++;
		return;
	}
	if (__predict_false(pktlen > MCLBYTES)) {
		ifp->if_ierrors++;
		return;
	}

	rate = MS(rxdw3, R92C_RXDW3_RATE);
	infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;
	if (infosz > sizeof(struct r92c_rx_phystat))
		infosz = sizeof(struct r92c_rx_phystat);
	shift = MS(rxdw0, R92C_RXDW0_SHIFT);
	totlen = pktlen + infosz + shift;

	/* Get RSSI from PHY status descriptor if present. */
	if (infosz != 0 && (rxdw0 & R92C_RXDW0_PHYST)) {
		phy = mtod(rx_data->m, struct r92c_rx_phystat *);
		rssi = rtwn_get_rssi(sc, rate, phy);
		/* Update our average RSSI. */
		rtwn_update_avgrssi(sc, rate, rssi);
	}

	DPRINTFN(5, ("Rx frame len=%d rate=%d infosz=%d shift=%d rssi=%d\n",
	    pktlen, rate, infosz, shift, rssi));

	MGETHDR(m1, M_DONTWAIT, MT_DATA);
	if (__predict_false(m1 == NULL)) {
		ic->ic_stats.is_rx_nobuf++;
		ifp->if_ierrors++;
		return;
	}
	MCLGET(m1, M_DONTWAIT);
	if (__predict_false(!(m1->m_flags & M_EXT))) {
		m_freem(m1);
		ic->ic_stats.is_rx_nobuf++;
		ifp->if_ierrors++;
		return;
	}

	bus_dmamap_sync(sc->sc_dmat, rx_data->map, 0, totlen,
	    BUS_DMASYNC_POSTREAD);

	bus_dmamap_unload(sc->sc_dmat, rx_data->map);
	error = bus_dmamap_load(sc->sc_dmat, rx_data->map, mtod(m1, void *),
	    MCLBYTES, NULL, BUS_DMA_NOWAIT | BUS_DMA_READ);
	if (error != 0) {
		m_freem(m1);

		if (bus_dmamap_load_mbuf(sc->sc_dmat, rx_data->map,
		    rx_data->m, BUS_DMA_NOWAIT))
			panic("%s: could not load old RX mbuf",
			    device_xname(sc->sc_dev));

		bus_dmamap_sync(sc->sc_dmat, rx_data->map, 0, MCLBYTES,
		    BUS_DMASYNC_PREREAD);

		/* Physical address may have changed. */
		rtwn_setup_rx_desc(sc, rx_desc,
		    rx_data->map->dm_segs[0].ds_addr, MCLBYTES, desc_idx);

		ifp->if_ierrors++;
		return;
	}

	/* Finalize mbuf. */
	m = rx_data->m;
	rx_data->m = m1;
	m->m_pkthdr.len = m->m_len = totlen;
	m->m_pkthdr.rcvif = ifp;

	bus_dmamap_sync(sc->sc_dmat, rx_data->map, 0, MCLBYTES,
	    BUS_DMASYNC_PREREAD);

	/* Update RX descriptor. */
	rtwn_setup_rx_desc(sc, rx_desc, rx_data->map->dm_segs[0].ds_addr,
	    MCLBYTES, desc_idx);

	/* Get ieee80211 frame header. */
	if (rxdw0 & R92C_RXDW0_PHYST)
		m_adj(m, infosz + shift);
	else
		m_adj(m, shift);
	wh = mtod(m, struct ieee80211_frame *);

	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct rtwn_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		/* Map HW rate index to 802.11 rate. */
		tap->wr_flags = 2;
		if (!(rxdw3 & R92C_RXDW3_HT)) {
			switch (rate) {
			/* CCK. */
			case  0: tap->wr_rate =   2; break;
			case  1: tap->wr_rate =   4; break;
			case  2: tap->wr_rate =  11; break;
			case  3: tap->wr_rate =  22; break;
			/* OFDM. */
			case  4: tap->wr_rate =  12; break;
			case  5: tap->wr_rate =  18; break;
			case  6: tap->wr_rate =  24; break;
			case  7: tap->wr_rate =  36; break;
			case  8: tap->wr_rate =  48; break;
			case  9: tap->wr_rate =  72; break;
			case 10: tap->wr_rate =  96; break;
			case 11: tap->wr_rate = 108; break;
			}
		} else if (rate >= 12) {	/* MCS0~15. */
			/* Bit 7 set means HT MCS instead of rate. */
			tap->wr_rate = 0x80 | (rate - 12);
		}
		tap->wr_dbm_antsignal = rssi;
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* push the frame up to the 802.11 stack */
	ieee80211_input(ic, m, ni, rssi, 0);

	/* Node is no longer needed. */
	ieee80211_free_node(ni);
}

static int
rtwn_tx(struct rtwn_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct rtwn_tx_ring *tx_ring;
	struct rtwn_tx_data *data;
	struct r92c_tx_desc *txd;
	uint16_t qos, seq;
	uint8_t raid, type, tid, qid;
	int hasqos, error;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m);
		if (k == NULL)
			return ENOBUFS;

		wh = mtod(m, struct ieee80211_frame *);
	}

	if ((hasqos = ieee80211_has_qos(wh))) {
		/* data frames in 11n mode */
		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
		qid = TID_TO_WME_AC(tid);
	} else if (type != IEEE80211_FC0_TYPE_DATA) {
		/* Use AC_VO for management frames. */
		tid = 0;	/* compiler happy */
		qid = RTWN_VO_QUEUE;
	} else {
		/* non-qos data frames */
		tid = R92C_TXDW1_QSEL_BE;
		qid = RTWN_BE_QUEUE;
	}

	/* Grab a Tx buffer from the ring. */
	tx_ring = &sc->tx_ring[qid];
	data = &tx_ring->tx_data[tx_ring->cur];
	if (data->m != NULL) {
		m_freem(m);
		return ENOBUFS;
	}

	/* Fill Tx descriptor. */
	txd = &tx_ring->desc[tx_ring->cur];
	if (htole32(txd->txdw0) & R92C_RXDW0_OWN) {
		m_freem(m);
		return ENOBUFS;
	}

	txd->txdw0 = htole32(
	    SM(R92C_TXDW0_PKTLEN, m->m_pkthdr.len) |
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) |
	    R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		txd->txdw0 |= htole32(R92C_TXDW0_BMCAST);

	txd->txdw1 = 0;
	txd->txdw4 = 0;
	txd->txdw5 = 0;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    type == IEEE80211_FC0_TYPE_DATA) {
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			raid = R92C_RAID_11B;
		else
			raid = R92C_RAID_11BG;

		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, RTWN_MACID_BSS) |
		    SM(R92C_TXDW1_QSEL, tid) |
		    SM(R92C_TXDW1_RAID, raid) |
		    R92C_TXDW1_AGGBK);

		if (ic->ic_flags & IEEE80211_F_USEPROT) {
			/* for 11g */
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY) {
				txd->txdw4 |= htole32(R92C_TXDW4_CTS2SELF |
				    R92C_TXDW4_HWRTSEN);
			} else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS) {
				txd->txdw4 |= htole32(R92C_TXDW4_RTSEN |
				    R92C_TXDW4_HWRTSEN);
			}
		}
		/* Send RTS at OFDM24. */
		txd->txdw4 |= htole32(SM(R92C_TXDW4_RTSRATE, 8));
		txd->txdw5 |= htole32(SM(R92C_TXDW5_RTSRATE_FBLIMIT, 0xf));
		/* Send data at OFDM54. */
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 11));
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE_FBLIMIT, 0x1f));
	} else if (type == IEEE80211_FC0_TYPE_MGT) {
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, RTWN_MACID_BSS) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_MGNT) |
		    SM(R92C_TXDW1_RAID, R92C_RAID_11B));

		/* Force CCK1. */
		txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);
		/* Use 1Mbps */
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 0));
	} else {
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, RTWN_MACID_BC) |
		    SM(R92C_TXDW1_RAID, R92C_RAID_11B));

		/* Force CCK1. */
		txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);
		/* Use 1Mbps */
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 0));
	}

	/* Set sequence number (already little endian). */
	seq = LE_READ_2(&wh->i_seq[0]) >> IEEE80211_SEQ_SEQ_SHIFT;
	txd->txdseq = htole16(seq);

	if (!hasqos) {
		/* Use HW sequence numbering for non-QoS frames. */
		txd->txdw4  |= htole32(R92C_TXDW4_HWSEQ);
		txd->txdseq |= htole16(0x8000);		/* WTF? */
	} else
		txd->txdw4 |= htole32(R92C_TXDW4_QOS);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (error && error != EFBIG) {
		aprint_error_dev(sc->sc_dev, "can't map mbuf (error %d)\n",
		    error);
		m_freem(m);
		return error;
	}
	if (error != 0) {
		/* Too many DMA segments, linearize mbuf. */
		if ((m = m_defrag(m, M_DONTWAIT)) == NULL) {
			aprint_error_dev(sc->sc_dev, "can't defrag mbuf\n");
			return ENOBUFS;
		}

		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
		    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "can't map mbuf (error %d)\n", error);
			m_freem(m);
			return error;
		}
	}

	txd->txbufaddr = htole32(data->map->dm_segs[0].ds_addr);
	txd->txbufsize = htole16(m->m_pkthdr.len);
	bus_space_barrier(sc->sc_st, sc->sc_sh, 0, sc->sc_mapsize,
	    BUS_SPACE_BARRIER_WRITE);
	txd->txdw0 |= htole32(R92C_TXDW0_OWN);

	bus_dmamap_sync(sc->sc_dmat, tx_ring->map, 0,
	    sizeof(*txd) * RTWN_TX_LIST_COUNT, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, data->map, 0, m->m_pkthdr.len,
	    BUS_DMASYNC_PREWRITE);

	data->m = m;
	data->ni = ni;

	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct rtwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m);
	}

	tx_ring->cur = (tx_ring->cur + 1) % RTWN_TX_LIST_COUNT;
	tx_ring->queued++;

	if (tx_ring->queued >= (RTWN_TX_LIST_COUNT - 1))
		sc->qfullmsk |= (1 << qid);

	/* Kick TX. */
	rtwn_write_2(sc, R92C_PCIE_CTRL_REG, (1 << qid));

	return 0;
}

static void
rtwn_tx_done(struct rtwn_softc *sc, int qid)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = IC2IFP(ic);
	struct rtwn_tx_ring *tx_ring = &sc->tx_ring[qid];
	struct rtwn_tx_data *tx_data;
	struct r92c_tx_desc *tx_desc;
	int i;

	DPRINTFN(3, ("%s: %s: qid=%d\n", device_xname(sc->sc_dev), __func__,
	    qid));

	bus_dmamap_sync(sc->sc_dmat, tx_ring->map,
	    0, sizeof(*tx_desc) * RTWN_TX_LIST_COUNT,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (i = 0; i < RTWN_TX_LIST_COUNT; i++) {
		tx_data = &tx_ring->tx_data[i];
		if (tx_data->m == NULL)
			continue;

		tx_desc = &tx_ring->desc[i];
		if (le32toh(tx_desc->txdw0) & R92C_TXDW0_OWN)
			continue;

		bus_dmamap_unload(sc->sc_dmat, tx_data->map);
		m_freem(tx_data->m);
		tx_data->m = NULL;
		ieee80211_free_node(tx_data->ni);
		tx_data->ni = NULL;

		ifp->if_opackets++;
		sc->sc_tx_timer = 0;
		tx_ring->queued--;
	}

	if (tx_ring->queued < (RTWN_TX_LIST_COUNT - 1))
		sc->qfullmsk &= ~(1 << qid);
}

static void
rtwn_start(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	for (;;) {
		if (sc->qfullmsk != 0) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/* Send pending management frames first. */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m != NULL) {
			ni = (void *)m->m_pkthdr.rcvif;
			m->m_pkthdr.rcvif = NULL;
			goto sendit;
		}
		if (ic->ic_state != IEEE80211_S_RUN)
			break;

		/* Encapsulate and send data frames. */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (m->m_len < (int)sizeof(*eh) &&
		    (m = m_pullup(m, sizeof(*eh))) == NULL) {
			ifp->if_oerrors++;
			continue;
		}
		eh = mtod(m, struct ether_header *);
		ni = ieee80211_find_txnode(ic, eh->ether_dhost);
		if (ni == NULL) {
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}

		bpf_mtap(ifp, m);

		if ((m = ieee80211_encap(ic, m, ni)) == NULL) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}
sendit:
		bpf_mtap3(ic->ic_rawbpf, m);

		if (rtwn_tx(sc, m, ni) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}

	DPRINTFN(3, ("%s: %s done\n", device_xname(sc->sc_dev), __func__));
}

static void
rtwn_watchdog(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			softint_schedule(sc->init_task);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}
	ieee80211_watchdog(ic);
}

static int
rtwn_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	DPRINTFN(3, ("%s: %s: cmd=0x%08lx, data=%p\n", device_xname(sc->sc_dev),
	    __func__, cmd, data));

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_UP | IFF_RUNNING:
			break;
		case IFF_UP:
			error = rtwn_init(ifp);
			if (error != 0)
				ifp->if_flags &= ~IFF_UP;
			break;
		case IFF_RUNNING:
			rtwn_stop(ifp, 1);
			break;
		case 0:
			break;
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/* setup multicast filter, etc */
			error = 0;
		}
		break;

	case SIOCS80211CHANNEL:
		error = ieee80211_ioctl(ic, cmd, data);
		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
			    (IFF_UP | IFF_RUNNING)) {
				rtwn_set_chan(sc, ic->ic_curchan, NULL);
			}
			error = 0;
		}
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		error = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			rtwn_stop(ifp, 0);
			error = rtwn_init(ifp);
		}
	}

	splx(s);

	DPRINTFN(3, ("%s: %s: error=%d\n", device_xname(sc->sc_dev), __func__,
	    error));

	return error;
}

static int
rtwn_power_on(struct rtwn_softc *sc)
{
	uint32_t reg;
	int ntries;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Wait for autoload done bit. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_1(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_PFM_ALDN)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for chip autoload\n");
		return ETIMEDOUT;
	}

	/* Unlock ISO/CLK/Power control register. */
	rtwn_write_1(sc, R92C_RSV_CTRL, 0);

	/* TODO: check if we need this for 8188CE */
	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_APS_FSMCO);
		reg |= (R92C_APS_FSMCO_SOP_ABG |
			R92C_APS_FSMCO_SOP_AMB |
			R92C_APS_FSMCO_XOP_BTCK);
		rtwn_write_4(sc, R92C_APS_FSMCO, reg);
	}

	/* Move SPS into PWM mode. */
	rtwn_write_1(sc, R92C_SPS0_CTRL, 0x2b);
	DELAY(100);

	/* Set low byte to 0x0f, leave others unchanged. */
	rtwn_write_4(sc, R92C_AFE_XTAL_CTRL,
	    (rtwn_read_4(sc, R92C_AFE_XTAL_CTRL) & 0xffffff00) | 0x0f);

	/* TODO: check if we need this for 8188CE */
	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_AFE_XTAL_CTRL);
		reg &= ~0x00024800; /* XXX magic from linux */
		rtwn_write_4(sc, R92C_AFE_XTAL_CTRL, reg);
	}

	rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	  (rtwn_read_2(sc, R92C_SYS_ISO_CTRL) & 0xff) |
	  R92C_SYS_ISO_CTRL_PWC_EV12V | R92C_SYS_ISO_CTRL_DIOR);
	DELAY(200);

	/* TODO: linux does additional btcoex stuff here */

	/* Auto enable WLAN. */
	rtwn_write_2(sc, R92C_APS_FSMCO,
	    rtwn_read_2(sc, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(rtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC))
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for MAC auto ON\n");
		return ETIMEDOUT;
	}

	/* Enable radio, GPIO and LED functions. */
	rtwn_write_2(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_PCIE |
	    R92C_APS_FSMCO_PDN_EN |
	    R92C_APS_FSMCO_PFM_ALDN);

	/* Release RF digital isolation. */
	rtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	    rtwn_read_2(sc, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_DIOR);

	if (sc->chip & RTWN_CHIP_92C)
		rtwn_write_1(sc, R92C_PCIE_CTRL_REG + 3, 0x77);
	else
		rtwn_write_1(sc, R92C_PCIE_CTRL_REG + 3, 0x22);

	rtwn_write_4(sc, R92C_INT_MIG, 0);

	if (sc->board_type != R92C_BOARD_TYPE_DONGLE) {
		/* bt coex */
		reg = rtwn_read_4(sc, R92C_AFE_XTAL_CTRL + 2);
		reg &= 0xfd; /* XXX magic from linux */
		rtwn_write_4(sc, R92C_AFE_XTAL_CTRL + 2, reg);
	}

	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    rtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_RFKILL);

	reg = rtwn_read_1(sc, R92C_GPIO_IO_SEL);
	if (!(reg & R92C_GPIO_IO_SEL_RFKILL)) {
		aprint_error_dev(sc->sc_dev,
		    "radio is disabled by hardware switch\n");
		return EPERM;	/* :-) */
	}

	/* Initialize MAC. */
	reg = rtwn_read_1(sc, R92C_APSD_CTRL);
	rtwn_write_1(sc, R92C_APSD_CTRL,
	    rtwn_read_1(sc, R92C_APSD_CTRL) & ~R92C_APSD_CTRL_OFF);
	for (ntries = 0; ntries < 200; ntries++) {
		if (!(rtwn_read_1(sc, R92C_APSD_CTRL) &
		    R92C_APSD_CTRL_OFF_STATUS))
			break;
		DELAY(500);
	}
	if (ntries == 200) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for MAC initialization\n");
		return ETIMEDOUT;
	}

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC blocks. */
	reg = rtwn_read_2(sc, R92C_CR);
	reg |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC;
	rtwn_write_2(sc, R92C_CR, reg);

	rtwn_write_1(sc, 0xfe10, 0x19);

	return 0;
}

static int
rtwn_llt_init(struct rtwn_softc *sc)
{
	int i, error;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Reserve pages [0; R92C_TX_PAGE_COUNT]. */
	for (i = 0; i < R92C_TX_PAGE_COUNT; i++) {
		if ((error = rtwn_llt_write(sc, i, i + 1)) != 0)
			return error;
	}
	/* NB: 0xff indicates end-of-list. */
	if ((error = rtwn_llt_write(sc, i, 0xff)) != 0)
		return error;
	/*
	 * Use pages [R92C_TX_PAGE_COUNT + 1; R92C_TXPKTBUF_COUNT - 1]
	 * as ring buffer.
	 */
	for (++i; i < R92C_TXPKTBUF_COUNT - 1; i++) {
		if ((error = rtwn_llt_write(sc, i, i + 1)) != 0)
			return error;
	}
	/* Make the last page point to the beginning of the ring buffer. */
	error = rtwn_llt_write(sc, i, R92C_TX_PAGE_COUNT + 1);
	return error;
}

static void
rtwn_fw_reset(struct rtwn_softc *sc)
{
	uint16_t reg;
	int ntries;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Tell 8051 to reset itself. */
	rtwn_write_1(sc, R92C_HMETFR + 3, 0x20);

	/* Wait until 8051 resets by itself. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = rtwn_read_2(sc, R92C_SYS_FUNC_EN);
		if (!(reg & R92C_SYS_FUNC_EN_CPUEN))
			goto sleep;
		DELAY(50);
	}
	/* Force 8051 reset. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
sleep:
	CLR(sc->sc_flags, RTWN_FLAG_FW_LOADED);
#if 0
	/*
	 * We must sleep for one second to let the firmware settle.
	 * Accessing registers too early will hang the whole system.
	 */
	tsleep(&reg, 0, "rtwnrst", hz);
#else
	DELAY(1000 * 1000);
#endif
}

static int
rtwn_fw_loadpage(struct rtwn_softc *sc, int page, uint8_t *buf, int len)
{
	uint32_t reg;
	int off, mlen, error = 0, i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	reg = rtwn_read_4(sc, R92C_MCUFWDL);
	reg = RW(reg, R92C_MCUFWDL_PAGE, page);
	rtwn_write_4(sc, R92C_MCUFWDL, reg);

	DELAY(5);

	off = R92C_FW_START_ADDR;
	while (len > 0) {
		if (len > 196)
			mlen = 196;
		else if (len > 4)
			mlen = 4;
		else
			mlen = 1;
		for (i = 0; i < mlen; i++)
			rtwn_write_1(sc, off++, buf[i]);
		buf += mlen;
		len -= mlen;
	}

	return error;
}

static int
rtwn_load_firmware(struct rtwn_softc *sc)
{
	firmware_handle_t fwh;
	const struct r92c_fw_hdr *hdr;
	const char *name;
	u_char *fw, *ptr;
	size_t len;
	uint32_t reg;
	int mlen, ntries, page, error;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Read firmware image from the filesystem. */
	if ((sc->chip & (RTWN_CHIP_UMC_A_CUT | RTWN_CHIP_92C)) ==
	    RTWN_CHIP_UMC_A_CUT)
		name = "rtl8192cfwU.bin";
	else if (sc->chip & RTWN_CHIP_UMC_B_CUT)
		name = "rtl8192cfwU_B.bin";
	else
		name = "rtl8192cfw.bin";
	DPRINTF(("%s: firmware: %s\n", device_xname(sc->sc_dev), name));
	if ((error = firmware_open("if_rtwn", name, &fwh)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not read firmware %s (error %d)\n", name, error);
		return error;
	}
	const size_t fwlen = len = firmware_get_size(fwh);
	fw = firmware_malloc(len);
	if (fw == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "failed to allocate firmware memory (size=%zu)\n", len);
		firmware_close(fwh);
		return ENOMEM;
	}
	error = firmware_read(fwh, 0, fw, len);
	firmware_close(fwh);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "failed to read firmware (error %d)\n", error);
		firmware_free(fw, fwlen);
		return error;
	}

	if (len < sizeof(*hdr)) {
		aprint_error_dev(sc->sc_dev, "firmware too short\n");
		error = EINVAL;
		goto fail;
	}
	ptr = fw;
	hdr = (const struct r92c_fw_hdr *)ptr;
	/* Check if there is a valid FW header and skip it. */
	if ((le16toh(hdr->signature) >> 4) == 0x88c ||
	    (le16toh(hdr->signature) >> 4) == 0x92c) {
		DPRINTF(("FW V%d.%d %02d-%02d %02d:%02d\n",
		    le16toh(hdr->version), le16toh(hdr->subversion),
		    hdr->month, hdr->date, hdr->hour, hdr->minute));
		ptr += sizeof(*hdr);
		len -= sizeof(*hdr);
	}

	if (rtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL)
		rtwn_fw_reset(sc);

	/* Enable FW download. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    rtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_CPUEN);
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_EN);
	rtwn_write_1(sc, R92C_MCUFWDL + 2,
	    rtwn_read_1(sc, R92C_MCUFWDL + 2) & ~0x08);

	/* Reset the FWDL checksum. */
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_CHKSUM_RPT);

	/* download firmware */
	for (page = 0; len > 0; page++) {
		mlen = MIN(len, R92C_FW_PAGE_SIZE);
		error = rtwn_fw_loadpage(sc, page, ptr, mlen);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not load firmware page %d\n", page);
			goto fail;
		}
		ptr += mlen;
		len -= mlen;
	}

	/* Disable FW download. */
	rtwn_write_1(sc, R92C_MCUFWDL,
	    rtwn_read_1(sc, R92C_MCUFWDL) & ~R92C_MCUFWDL_EN);
	rtwn_write_1(sc, R92C_MCUFWDL + 1, 0);

	/* Wait for checksum report. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_CHKSUM_RPT)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for checksum report\n");
		error = ETIMEDOUT;
		goto fail;
	}

	reg = rtwn_read_4(sc, R92C_MCUFWDL);
	reg = (reg & ~R92C_MCUFWDL_WINTINI_RDY) | R92C_MCUFWDL_RDY;
	rtwn_write_4(sc, R92C_MCUFWDL, reg);

	/* Wait for firmware readiness. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (rtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_WINTINI_RDY)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for firmware readiness\n");
		error = ETIMEDOUT;
		goto fail;
	}
	SET(sc->sc_flags, RTWN_FLAG_FW_LOADED);

 fail:
	firmware_free(fw, fwlen);
	return error;
}

static int
rtwn_dma_init(struct rtwn_softc *sc)
{
	uint32_t reg;
	int error;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Initialize LLT table. */
	error = rtwn_llt_init(sc);
	if (error != 0)
		return error;

	/* Set number of pages for normal priority queue. */
	rtwn_write_2(sc, R92C_RQPN_NPQ, 0);
	rtwn_write_4(sc, R92C_RQPN,
	    /* Set number of pages for public queue. */
	    SM(R92C_RQPN_PUBQ, R92C_PUBQ_NPAGES) |
	    /* Set number of pages for high priority queue. */
	    SM(R92C_RQPN_HPQ, R92C_HPQ_NPAGES) |
	    /* Set number of pages for low priority queue. */
	    SM(R92C_RQPN_LPQ, R92C_LPQ_NPAGES) |
	    /* Load values. */
	    R92C_RQPN_LD);

	rtwn_write_1(sc, R92C_TXPKTBUF_BCNQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TXPKTBUF_MGQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TXPKTBUF_WMAC_LBK_BF_HD, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TRXFF_BNDY, R92C_TX_PAGE_BOUNDARY);
	rtwn_write_1(sc, R92C_TDECTRL + 1, R92C_TX_PAGE_BOUNDARY);

	reg = rtwn_read_2(sc, R92C_TRXDMA_CTRL);
	reg &= ~R92C_TRXDMA_CTRL_QMAP_M;
	reg |= 0xF771;
	rtwn_write_2(sc, R92C_TRXDMA_CTRL, reg);

	rtwn_write_4(sc, R92C_TCR, R92C_TCR_CFENDFORM | (1 << 12) | (1 << 13));

	/* Configure Tx DMA. */
	rtwn_write_4(sc, R92C_BKQ_DESA,
		sc->tx_ring[RTWN_BK_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_BEQ_DESA,
		sc->tx_ring[RTWN_BE_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_VIQ_DESA,
		sc->tx_ring[RTWN_VI_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_VOQ_DESA,
		sc->tx_ring[RTWN_VO_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_BCNQ_DESA,
		sc->tx_ring[RTWN_BEACON_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_MGQ_DESA,
		sc->tx_ring[RTWN_MGNT_QUEUE].map->dm_segs[0].ds_addr);
	rtwn_write_4(sc, R92C_HQ_DESA,
		sc->tx_ring[RTWN_HIGH_QUEUE].map->dm_segs[0].ds_addr);

	/* Configure Rx DMA. */
	rtwn_write_4(sc, R92C_RX_DESA, sc->rx_ring.map->dm_segs[0].ds_addr);

	/* Set Tx/Rx transfer page boundary. */
	rtwn_write_2(sc, R92C_TRXFF_BNDY + 2, 0x27ff);

	/* Set Tx/Rx transfer page size. */
	rtwn_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_128) |
	    SM(R92C_PBP_PSTX, R92C_PBP_128));
	return 0;
}

static void
rtwn_mac_init(struct rtwn_softc *sc)
{
	int i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Write MAC initialization values. */
	for (i = 0; i < __arraycount(rtl8192ce_mac); i++)
		rtwn_write_1(sc, rtl8192ce_mac[i].reg, rtl8192ce_mac[i].val);
}

static void
rtwn_bb_init(struct rtwn_softc *sc)
{
	const struct rtwn_bb_prog *prog;
	uint32_t reg;
	int i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Enable BB and RF. */
	rtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    rtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_DIO_RF);

	rtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0xdb83);

	rtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);

	rtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_DIO_PCIE | R92C_SYS_FUNC_EN_PCIEA |
	    R92C_SYS_FUNC_EN_PPLL | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_BBRSTB);

	rtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);

	rtwn_write_4(sc, R92C_LEDCFG0,
	    rtwn_read_4(sc, R92C_LEDCFG0) | 0x00800000);

	/* Select BB programming. */
	prog = (sc->chip & RTWN_CHIP_92C) ?
	    &rtl8192ce_bb_prog_2t : &rtl8192ce_bb_prog_1t;

	/* Write BB initialization values. */
	for (i = 0; i < prog->count; i++) {
		rtwn_bb_write(sc, prog->regs[i], prog->vals[i]);
		DELAY(1);
	}

	if (sc->chip & RTWN_CHIP_92C_1T2R) {
		/* 8192C 1T only configuration. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_TXINFO);
		reg = (reg & ~0x00000003) | 0x2;
		rtwn_bb_write(sc, R92C_FPGA0_TXINFO, reg);

		reg = rtwn_bb_read(sc, R92C_FPGA1_TXINFO);
		reg = (reg & ~0x00300033) | 0x00200022;
		rtwn_bb_write(sc, R92C_FPGA1_TXINFO, reg);

		reg = rtwn_bb_read(sc, R92C_CCK0_AFESETTING);
		reg = (reg & ~0xff000000) | 0x45 << 24;
		rtwn_bb_write(sc, R92C_CCK0_AFESETTING, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_TRXPATHENA);
		reg = (reg & ~0x000000ff) | 0x23;
		rtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM0_AGCPARAM1);
		reg = (reg & ~0x00000030) | 1 << 4;
		rtwn_bb_write(sc, R92C_OFDM0_AGCPARAM1, reg);

		reg = rtwn_bb_read(sc, 0xe74);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe74, reg);
		reg = rtwn_bb_read(sc, 0xe78);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe78, reg);
		reg = rtwn_bb_read(sc, 0xe7c);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe7c, reg);
		reg = rtwn_bb_read(sc, 0xe80);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe80, reg);
		reg = rtwn_bb_read(sc, 0xe88);
		reg = (reg & ~0x0c000000) | 2 << 26;
		rtwn_bb_write(sc, 0xe88, reg);
	}

	/* Write AGC values. */
	for (i = 0; i < prog->agccount; i++) {
		rtwn_bb_write(sc, R92C_OFDM0_AGCRSSITABLE,
		    prog->agcvals[i]);
		DELAY(1);
	}

	if (rtwn_bb_read(sc, R92C_HSSI_PARAM2(0)) &
	    R92C_HSSI_PARAM2_CCK_HIPWR)
		sc->sc_flags |= RTWN_FLAG_CCK_HIPWR;
}

static void
rtwn_rf_init(struct rtwn_softc *sc)
{
	const struct rtwn_rf_prog *prog;
	uint32_t reg, type;
	int i, j, idx, off;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Select RF programming based on board type. */
	if (!(sc->chip & RTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = rtl8188ce_rf_prog;
		else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			prog = rtl8188ru_rf_prog;
		else
			prog = rtl8188cu_rf_prog;
	} else
		prog = rtl8192ce_rf_prog;

	for (i = 0; i < sc->nrxchains; i++) {
		/* Save RF_ENV control type. */
		idx = i / 2;
		off = (i % 2) * 16;
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		type = (reg >> off) & 0x10;

		/* Set RF_ENV enable. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x100000;
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set RF_ENV output high. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x10;
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set address and data lengths of RF registers. */
		reg = rtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_ADDR_LENGTH;
		rtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);
		reg = rtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_DATA_LENGTH;
		rtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);

		/* Write RF initialization values for this chain. */
		for (j = 0; j < prog[i].count; j++) {
			if (prog[i].regs[j] >= 0xf9 &&
			    prog[i].regs[j] <= 0xfe) {
				/*
				 * These are fake RF registers offsets that
				 * indicate a delay is required.
				 */
				DELAY(50);
				continue;
			}
			rtwn_rf_write(sc, i, prog[i].regs[j],
			    prog[i].vals[j]);
			DELAY(1);
		}

		/* Restore RF_ENV control type. */
		reg = rtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		reg &= ~(0x10 << off) | (type << off);
		rtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(idx), reg);

		/* Cache RF register CHNLBW. */
		sc->rf_chnlbw[i] = rtwn_rf_read(sc, i, R92C_RF_CHNLBW);
	}

	if ((sc->chip & (RTWN_CHIP_UMC_A_CUT | RTWN_CHIP_92C)) ==
	    RTWN_CHIP_UMC_A_CUT) {
		rtwn_rf_write(sc, 0, R92C_RF_RX_G1, 0x30255);
		rtwn_rf_write(sc, 0, R92C_RF_RX_G2, 0x50a00);
	}
}

static void
rtwn_cam_init(struct rtwn_softc *sc)
{

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Invalidate all CAM entries. */
	rtwn_write_4(sc, R92C_CAMCMD, R92C_CAMCMD_POLLING | R92C_CAMCMD_CLR);
}

static void
rtwn_pa_bias_init(struct rtwn_softc *sc)
{
	uint8_t reg;
	int i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	for (i = 0; i < sc->nrxchains; i++) {
		if (sc->pa_setting & (1 << i))
			continue;
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x0f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x4f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0x8f406);
		rtwn_rf_write(sc, i, R92C_RF_IPA, 0xcf406);
	}
	if (!(sc->pa_setting & 0x10)) {
		reg = rtwn_read_1(sc, 0x16);
		reg = (reg & ~0xf0) | 0x90;
		rtwn_write_1(sc, 0x16, reg);
	}
}

static void
rtwn_rxfilter_init(struct rtwn_softc *sc)
{

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Initialize Rx filter. */
	/* TODO: use better filter for monitor mode. */
	rtwn_write_4(sc, R92C_RCR,
	    R92C_RCR_AAP | R92C_RCR_APM | R92C_RCR_AM | R92C_RCR_AB |
	    R92C_RCR_APP_ICV | R92C_RCR_AMF | R92C_RCR_HTC_LOC_CTRL |
	    R92C_RCR_APP_MIC | R92C_RCR_APP_PHYSTS);
	/* Accept all multicast frames. */
	rtwn_write_4(sc, R92C_MAR + 0, 0xffffffff);
	rtwn_write_4(sc, R92C_MAR + 4, 0xffffffff);
	/* Accept all management frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP0, 0xffff);
	/* Reject all control frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP1, 0x0000);
	/* Accept all data frames. */
	rtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);
}

static void
rtwn_edca_init(struct rtwn_softc *sc)
{

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* set spec SIFS (used in NAV) */
	rtwn_write_2(sc, R92C_SPEC_SIFS, 0x1010);
	rtwn_write_2(sc, R92C_MAC_SPEC_SIFS, 0x1010);

	/* set SIFS CCK/OFDM */
	rtwn_write_2(sc, R92C_SIFS_CCK, 0x1010);
	rtwn_write_2(sc, R92C_SIFS_OFDM, 0x0e0e);

	/* TXOP */
	rtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x005ea42b);
	rtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a44f);
	rtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4322);
	rtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3222);
}

static void
rtwn_write_txpower(struct rtwn_softc *sc, int chain,
    uint16_t power[RTWN_RIDX_COUNT])
{
	uint32_t reg;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Write per-CCK rate Tx power. */
	if (chain == 0) {
		reg = rtwn_bb_read(sc, R92C_TXAGC_A_CCK1_MCS32);
		reg = RW(reg, R92C_TXAGC_A_CCK1,  power[0]);
		rtwn_bb_write(sc, R92C_TXAGC_A_CCK1_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_A_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_A_CCK55, power[2]);
		reg = RW(reg, R92C_TXAGC_A_CCK11, power[3]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	} else {
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK1_55_MCS32);
		reg = RW(reg, R92C_TXAGC_B_CCK1,  power[0]);
		reg = RW(reg, R92C_TXAGC_B_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_B_CCK55, power[2]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK1_55_MCS32, reg);
		reg = rtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_B_CCK11, power[3]);
		rtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	}
	/* Write per-OFDM rate Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_RATE18_06(chain),
	    SM(R92C_TXAGC_RATE06, power[ 4]) |
	    SM(R92C_TXAGC_RATE09, power[ 5]) |
	    SM(R92C_TXAGC_RATE12, power[ 6]) |
	    SM(R92C_TXAGC_RATE18, power[ 7]));
	rtwn_bb_write(sc, R92C_TXAGC_RATE54_24(chain),
	    SM(R92C_TXAGC_RATE24, power[ 8]) |
	    SM(R92C_TXAGC_RATE36, power[ 9]) |
	    SM(R92C_TXAGC_RATE48, power[10]) |
	    SM(R92C_TXAGC_RATE54, power[11]));
	/* Write per-MCS Tx power. */
	rtwn_bb_write(sc, R92C_TXAGC_MCS03_MCS00(chain),
	    SM(R92C_TXAGC_MCS00,  power[12]) |
	    SM(R92C_TXAGC_MCS01,  power[13]) |
	    SM(R92C_TXAGC_MCS02,  power[14]) |
	    SM(R92C_TXAGC_MCS03,  power[15]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS07_MCS04(chain),
	    SM(R92C_TXAGC_MCS04,  power[16]) |
	    SM(R92C_TXAGC_MCS05,  power[17]) |
	    SM(R92C_TXAGC_MCS06,  power[18]) |
	    SM(R92C_TXAGC_MCS07,  power[19]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS11_MCS08(chain),
	    SM(R92C_TXAGC_MCS08,  power[20]) |
	    SM(R92C_TXAGC_MCS09,  power[21]) |
	    SM(R92C_TXAGC_MCS10,  power[22]) |
	    SM(R92C_TXAGC_MCS11,  power[23]));
	rtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
	    SM(R92C_TXAGC_MCS12,  power[24]) |
	    SM(R92C_TXAGC_MCS13,  power[25]) |
	    SM(R92C_TXAGC_MCS14,  power[26]) |
	    SM(R92C_TXAGC_MCS15,  power[27]));
}

static void
rtwn_get_txpower(struct rtwn_softc *sc, int chain,
    struct ieee80211_channel *c, struct ieee80211_channel *extc,
    uint16_t power[RTWN_RIDX_COUNT])
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom;
	uint16_t cckpow, ofdmpow, htpow, diff, max;
	const struct rtwn_txpwr *base;
	int ridx, chan, group;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Determine channel group. */
	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */
	if (chan <= 3)
		group = 0;
	else if (chan <= 9)
		group = 1;
	else
		group = 2;

	/* Get original Tx power based on board type and RF chain. */
	if (!(sc->chip & RTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			base = &rtl8188ru_txagc[chain];
		else
			base = &rtl8192cu_txagc[chain];
	} else
		base = &rtl8192cu_txagc[chain];

	memset(power, 0, RTWN_RIDX_COUNT * sizeof(power[0]));
	if (sc->regulatory == 0) {
		for (ridx = 0; ridx <= 3; ridx++)
			power[ridx] = base->pwr[0][ridx];
	}
	for (ridx = 4; ridx < RTWN_RIDX_COUNT; ridx++) {
		if (sc->regulatory == 3) {
			power[ridx] = base->pwr[0][ridx];
			/* Apply vendor limits. */
			if (extc != NULL)
				max = rom->ht40_max_pwr[group];
			else
				max = rom->ht20_max_pwr[group];
			max = (max >> (chain * 4)) & 0xf;
			if (power[ridx] > max)
				power[ridx] = max;
		} else if (sc->regulatory == 1) {
			if (extc == NULL)
				power[ridx] = base->pwr[group][ridx];
		} else if (sc->regulatory != 2)
			power[ridx] = base->pwr[0][ridx];
	}

	/* Compute per-CCK rate Tx power. */
	cckpow = rom->cck_tx_pwr[chain][group];
	for (ridx = 0; ridx <= 3; ridx++) {
		power[ridx] += cckpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	htpow = rom->ht40_1s_tx_pwr[chain][group];
	if (sc->ntxchains > 1) {
		/* Apply reduction for 2 spatial streams. */
		diff = rom->ht40_2s_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow = (htpow > diff) ? htpow - diff : 0;
	}

	/* Compute per-OFDM rate Tx power. */
	diff = rom->ofdm_tx_pwr_diff[group];
	diff = (diff >> (chain * 4)) & 0xf;
	ofdmpow = htpow + diff;	/* HT->OFDM correction. */
	for (ridx = 4; ridx <= 11; ridx++) {
		power[ridx] += ofdmpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	/* Compute per-MCS Tx power. */
	if (extc == NULL) {
		diff = rom->ht20_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow += diff;	/* HT40->HT20 correction. */
	}
	for (ridx = 12; ridx <= 27; ridx++) {
		power[ridx] += htpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}
#ifdef RTWN_DEBUG
	if (rtwn_debug >= 4) {
		/* Dump per-rate Tx power values. */
		printf("Tx power for chain %d:\n", chain);
		for (ridx = 0; ridx < RTWN_RIDX_COUNT; ridx++)
			printf("Rate %d = %u\n", ridx, power[ridx]);
	}
#endif
}

static void
rtwn_set_txpower(struct rtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint16_t power[RTWN_RIDX_COUNT];
	int i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	for (i = 0; i < sc->ntxchains; i++) {
		/* Compute per-rate Tx power values. */
		rtwn_get_txpower(sc, i, c, extc, power);
		/* Write per-rate Tx power values to hardware. */
		rtwn_write_txpower(sc, i, power);
	}
}

static void
rtwn_set_chan(struct rtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int chan;
	int i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */

	/* Set Tx power for this new channel. */
	rtwn_set_txpower(sc, c, extc);

	for (i = 0; i < sc->nrxchains; i++) {
		rtwn_rf_write(sc, i, R92C_RF_CHNLBW,
		    RW(sc->rf_chnlbw[i], R92C_RF_CHNLBW_CHNL, chan));
	}
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		uint32_t reg;

		/* Is secondary channel below or above primary? */
		int prichlo = c->ic_freq < extc->ic_freq;

		rtwn_write_1(sc, R92C_BWOPMODE,
		    rtwn_read_1(sc, R92C_BWOPMODE) & ~R92C_BWOPMODE_20MHZ);

		reg = rtwn_read_1(sc, R92C_RRSR + 2);
		reg = (reg & ~0x6f) | (prichlo ? 1 : 2) << 5;
		rtwn_write_1(sc, R92C_RRSR + 2, reg);

		rtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA0_RFMOD) | R92C_RFMOD_40MHZ);
		rtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA1_RFMOD) | R92C_RFMOD_40MHZ);

		/* Set CCK side band. */
		reg = rtwn_bb_read(sc, R92C_CCK0_SYSTEM);
		reg = (reg & ~0x00000010) | (prichlo ? 0 : 1) << 4;
		rtwn_bb_write(sc, R92C_CCK0_SYSTEM, reg);

		reg = rtwn_bb_read(sc, R92C_OFDM1_LSTF);
		reg = (reg & ~0x00000c00) | (prichlo ? 1 : 2) << 10;
		rtwn_bb_write(sc, R92C_OFDM1_LSTF, reg);

		rtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    rtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) &
		    ~R92C_FPGA0_ANAPARAM2_CBW20);

		reg = rtwn_bb_read(sc, 0x818);
		reg = (reg & ~0x0c000000) | (prichlo ? 2 : 1) << 26;
		rtwn_bb_write(sc, 0x818, reg);

		/* Select 40MHz bandwidth. */
		rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | chan);
	} else
#endif
	{
		rtwn_write_1(sc, R92C_BWOPMODE,
		    rtwn_read_1(sc, R92C_BWOPMODE) | R92C_BWOPMODE_20MHZ);

		rtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA0_RFMOD) & ~R92C_RFMOD_40MHZ);
		rtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    rtwn_bb_read(sc, R92C_FPGA1_RFMOD) & ~R92C_RFMOD_40MHZ);

		rtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    rtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) |
		    R92C_FPGA0_ANAPARAM2_CBW20);

		/* Select 20MHz bandwidth. */
		rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | R92C_RF_CHNLBW_BW20 | chan);
	}
}

static void
rtwn_iq_calib(struct rtwn_softc *sc)
{

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* XXX */
}

static void
rtwn_lc_calib(struct rtwn_softc *sc)
{
	uint32_t rf_ac[2];
	uint8_t txmode;
	int i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	txmode = rtwn_read_1(sc, R92C_OFDM1_LSTF + 3);
	if ((txmode & 0x70) != 0) {
		/* Disable all continuous Tx. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode & ~0x70);

		/* Set RF mode to standby mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			rf_ac[i] = rtwn_rf_read(sc, i, R92C_RF_AC);
			rtwn_rf_write(sc, i, R92C_RF_AC,
			    RW(rf_ac[i], R92C_RF_AC_MODE,
				R92C_RF_AC_MODE_STANDBY));
		}
	} else {
		/* Block all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	}
	/* Start calibration. */
	rtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
	    rtwn_rf_read(sc, 0, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	DELAY(100);

	/* Restore configuration. */
	if ((txmode & 0x70) != 0) {
		/* Restore Tx mode. */
		rtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode);
		/* Restore RF mode. */
		for (i = 0; i < sc->nrxchains; i++)
			rtwn_rf_write(sc, i, R92C_RF_AC, rf_ac[i]);
	} else {
		/* Unblock all Tx queues. */
		rtwn_write_1(sc, R92C_TXPAUSE, 0x00);
	}
}

static void
rtwn_temp_calib(struct rtwn_softc *sc)
{
	int temp;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	if (sc->thcal_state == 0) {
		/* Start measuring temperature. */
		rtwn_rf_write(sc, 0, R92C_RF_T_METER, 0x60);
		sc->thcal_state = 1;
		return;
	}
	sc->thcal_state = 0;

	/* Read measured temperature. */
	temp = rtwn_rf_read(sc, 0, R92C_RF_T_METER) & 0x1f;
	if (temp == 0)	/* Read failed, skip. */
		return;
	DPRINTFN(2, ("temperature=%d\n", temp));

	/*
	 * Redo IQ and LC calibration if temperature changed significantly
	 * since last calibration.
	 */
	if (sc->thcal_lctemp == 0) {
		/* First calibration is performed in rtwn_init(). */
		sc->thcal_lctemp = temp;
	} else if (abs(temp - sc->thcal_lctemp) > 1) {
		DPRINTF(("IQ/LC calib triggered by temp: %d -> %d\n",
 		    sc->thcal_lctemp, temp));
		rtwn_iq_calib(sc);
		rtwn_lc_calib(sc);
		/* Record temperature of last calibration. */
		sc->thcal_lctemp = temp;
	}
}

static int
rtwn_init(struct ifnet *ifp)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t reg;
	int i, error;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	/* Init firmware commands ring. */
	sc->fwcur = 0;

	/* Power on adapter. */
	error = rtwn_power_on(sc);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not power on adapter\n");
		goto fail;
	}

	/* Initialize DMA. */
	error = rtwn_dma_init(sc);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not initialize DMA\n");
		goto fail;
	}

	/* Set info size in Rx descriptors (in 64-bit words). */
	rtwn_write_1(sc, R92C_RX_DRVINFO_SZ, 4);

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0xffffffff);
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Set MAC address. */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		rtwn_write_1(sc, R92C_MACID + i, ic->ic_myaddr[i]);

	/* Set initial network type. */
	rtwn_set_nettype0_msr(sc, rtwn_get_nettype(sc));

	rtwn_rxfilter_init(sc);

	reg = rtwn_read_4(sc, R92C_RRSR);
	reg = RW(reg, R92C_RRSR_RATE_BITMAP, R92C_RRSR_RATE_ALL);
	rtwn_write_4(sc, R92C_RRSR, reg);

	/* Set short/long retry limits. */
	rtwn_write_2(sc, R92C_RL,
	    SM(R92C_RL_SRL, 0x07) | SM(R92C_RL_LRL, 0x07));

	/* Initialize EDCA parameters. */
	rtwn_edca_init(sc);

	/* Set data and response automatic rate fallback retry counts. */
	rtwn_write_4(sc, R92C_DARFRC + 0, 0x01000000);
	rtwn_write_4(sc, R92C_DARFRC + 4, 0x07060504);
	rtwn_write_4(sc, R92C_RARFRC + 0, 0x01000000);
	rtwn_write_4(sc, R92C_RARFRC + 4, 0x07060504);

	rtwn_write_2(sc, R92C_FWHW_TXQ_CTRL, 0x1f80);

	/* Set ACK timeout. */
	rtwn_write_1(sc, R92C_ACKTO, 0x40);

	/* Initialize beacon parameters. */
	rtwn_write_2(sc, R92C_TBTT_PROHIBIT, 0x6404);
	rtwn_write_1(sc, R92C_DRVERLYINT, 0x05);
	rtwn_write_1(sc, R92C_BCNDMATIM, 0x02);
	rtwn_write_2(sc, R92C_BCNTCFG, 0x660f);

	/* Setup AMPDU aggregation. */
	rtwn_write_4(sc, R92C_AGGLEN_LMT, 0x99997631);	/* MCS7~0 */
	rtwn_write_1(sc, R92C_AGGR_BREAK_TIME, 0x16);

	rtwn_write_1(sc, R92C_BCN_MAX_ERR, 0xff);
	rtwn_write_1(sc, R92C_BCN_CTRL, R92C_BCN_CTRL_DIS_TSF_UDT0);

	rtwn_write_4(sc, R92C_PIFS, 0x1c);
	rtwn_write_4(sc, R92C_MCUTST_1, 0x0);

	/* Load 8051 microcode. */
	error = rtwn_load_firmware(sc);
	if (error != 0)
		goto fail;

	/* Initialize MAC/BB/RF blocks. */
	rtwn_mac_init(sc);
	rtwn_bb_init(sc);
	rtwn_rf_init(sc);

	/* Turn CCK and OFDM blocks on. */
	reg = rtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_CCK_EN;
	rtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);
	reg = rtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_OFDM_EN;
	rtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);

	/* Clear per-station keys table. */
	rtwn_cam_init(sc);

	/* Enable hardware sequence numbering. */
	rtwn_write_1(sc, R92C_HWSEQ_CTRL, 0xff);

	/* Perform LO and IQ calibrations. */
	rtwn_iq_calib(sc);
	/* Perform LC calibration. */
	rtwn_lc_calib(sc);

	rtwn_pa_bias_init(sc);

	/* Initialize GPIO setting. */
	rtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    rtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_ENBT);

	/* Fix for lower temperature. */
	rtwn_write_1(sc, 0x15, 0xe9);

	/* Set default channel. */
	rtwn_set_chan(sc, ic->ic_curchan, NULL);

	/* Clear pending interrupts. */
	rtwn_write_4(sc, R92C_HISR, 0xffffffff);

	/* Enable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);

	/* We're ready to go. */
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return 0;

 fail:
	rtwn_stop(ifp, 1);
	return error;
}

static void
rtwn_init_task(void *arg)
{
	struct rtwn_softc *sc = arg;
	struct ifnet *ifp = GET_IFP(sc);
	int s;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	s = splnet();

	rtwn_stop(ifp, 0);

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP)
		rtwn_init(ifp);

	splx(s);
}

static void
rtwn_stop(struct ifnet *ifp, int disable)
{
	struct rtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t reg;
	int s, i;

	DPRINTFN(3, ("%s: %s\n", device_xname(sc->sc_dev), __func__));

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	callout_stop(&sc->scan_to);
	callout_stop(&sc->calib_to);

	s = splnet();

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Pause MAC TX queue */
	rtwn_write_1(sc, R92C_TXPAUSE, 0xff);

	rtwn_write_1(sc, R92C_RF_CTRL, 0x00);

	/* Reset BB state machine */
	reg = rtwn_read_1(sc, R92C_SYS_FUNC_EN);
	reg |= R92C_SYS_FUNC_EN_BB_GLB_RST;
	rtwn_write_1(sc, R92C_SYS_FUNC_EN, reg);
	reg &= ~R92C_SYS_FUNC_EN_BB_GLB_RST;
	rtwn_write_1(sc, R92C_SYS_FUNC_EN, reg);

	reg = rtwn_read_2(sc, R92C_CR);
	reg &= ~(R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC);
	rtwn_write_2(sc, R92C_CR, reg);

	if (rtwn_read_1(sc, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL)
		rtwn_fw_reset(sc);

	/* Reset MAC and Enable 8051 */
	rtwn_write_1(sc, R92C_SYS_FUNC_EN + 1, 0x54);

	/* TODO: linux does additional btcoex stuff here */

	/* Disable AFE PLL */
	rtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0x80); /* linux magic number */
	/* Enter PFM mode */
	rtwn_write_1(sc, R92C_SPS0_CTRL, 0x23); /* ditto */
	/* Gated AFE DIG_CLOCK */
	rtwn_write_1(sc, R92C_AFE_XTAL_CTRL, 0x0e); /* different with btcoex */
	rtwn_write_1(sc, R92C_RSV_CTRL, 0x0e);
	rtwn_write_1(sc, R92C_APS_FSMCO, R92C_APS_FSMCO_PDN_EN);

	for (i = 0; i < RTWN_NTXQUEUES; i++)
		rtwn_reset_tx_list(sc, i);
	rtwn_reset_rx_list(sc);

	splx(s);
}

static int
rtwn_intr(void *xsc)
{
	struct rtwn_softc *sc = xsc;
	uint32_t status;
	int i;

	if (!ISSET(sc->sc_flags, RTWN_FLAG_FW_LOADED))
		return 0;

	status = rtwn_read_4(sc, R92C_HISR);
	if (status == 0 || status == 0xffffffff)
		return 0;

	/* Disable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, 0x00000000);

	/* Ack interrupts. */
	rtwn_write_4(sc, R92C_HISR, status);

	/* Vendor driver treats RX errors like ROK... */
	if (status & RTWN_INT_ENABLE_RX) {
		for (i = 0; i < RTWN_RX_LIST_COUNT; i++) {
			struct r92c_rx_desc *rx_desc = &sc->rx_ring.desc[i];
			struct rtwn_rx_data *rx_data = &sc->rx_ring.rx_data[i];

			if (le32toh(rx_desc->rxdw0) & R92C_RXDW0_OWN)
				continue;

			rtwn_rx_frame(sc, rx_desc, rx_data, i);
		}
	}

	if (status & R92C_IMR_BDOK)
		rtwn_tx_done(sc, RTWN_BEACON_QUEUE);
	if (status & R92C_IMR_HIGHDOK)
		rtwn_tx_done(sc, RTWN_HIGH_QUEUE);
	if (status & R92C_IMR_MGNTDOK)
		rtwn_tx_done(sc, RTWN_MGNT_QUEUE);
	if (status & R92C_IMR_BKDOK)
		rtwn_tx_done(sc, RTWN_BK_QUEUE);
	if (status & R92C_IMR_BEDOK)
		rtwn_tx_done(sc, RTWN_BE_QUEUE);
	if (status & R92C_IMR_VIDOK)
		rtwn_tx_done(sc, RTWN_VI_QUEUE);
	if (status & R92C_IMR_VODOK)
		rtwn_tx_done(sc, RTWN_VO_QUEUE);
	if ((status & RTWN_INT_ENABLE_TX) && sc->qfullmsk == 0) {
		struct ifnet *ifp = GET_IFP(sc);
		ifp->if_flags &= ~IFF_OACTIVE;
		rtwn_start(ifp);
	}

	/* Enable interrupts. */
	rtwn_write_4(sc, R92C_HIMR, RTWN_INT_ENABLE);

	return 1;
}
