/*	$NetBSD: if_iwi.c,v 1.98 2015/01/07 07:05:48 ozaki-r Exp $  */
/*	$OpenBSD: if_iwi.c,v 1.111 2010/11/15 19:11:57 damien Exp $	*/

/*-
 * Copyright (c) 2004-2008
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_iwi.c,v 1.98 2015/01/07 07:05:48 ozaki-r Exp $");

/*-
 * Intel(R) PRO/Wireless 2200BG/2225BG/2915ABG driver
 * http://www.intel.com/network/connectivity/products/wireless/prowireless_mobile.htm
 */


#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/cprng.h>

#include <sys/bus.h>
#include <machine/endian.h>
#include <sys/intr.h>

#include <dev/firmload.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <dev/pci/if_iwireg.h>
#include <dev/pci/if_iwivar.h>

#ifdef IWI_DEBUG
#define DPRINTF(x)	if (iwi_debug > 0) printf x
#define DPRINTFN(n, x)	if (iwi_debug >= (n)) printf x
int iwi_debug = 4;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* Permit loading the Intel firmware */
static int iwi_accept_eula;

static int	iwi_match(device_t, cfdata_t, void *);
static void	iwi_attach(device_t, device_t, void *);
static int	iwi_detach(device_t, int);

static int	iwi_alloc_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *,
    int);
static void	iwi_reset_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
static void	iwi_free_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
static int	iwi_alloc_tx_ring(struct iwi_softc *, struct iwi_tx_ring *,
    int, bus_size_t, bus_size_t);
static void	iwi_reset_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
static void	iwi_free_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
static struct mbuf *
		iwi_alloc_rx_buf(struct iwi_softc *sc);
static int	iwi_alloc_rx_ring(struct iwi_softc *, struct iwi_rx_ring *,
    int);
static void	iwi_reset_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
static void	iwi_free_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);

static struct	ieee80211_node *iwi_node_alloc(struct ieee80211_node_table *);
static void	iwi_node_free(struct ieee80211_node *);

static int	iwi_cvtrate(int);
static int	iwi_media_change(struct ifnet *);
static void	iwi_media_status(struct ifnet *, struct ifmediareq *);
static int	iwi_wme_update(struct ieee80211com *);
static uint16_t	iwi_read_prom_word(struct iwi_softc *, uint8_t);
static int	iwi_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void	iwi_fix_channel(struct ieee80211com *, struct mbuf *);
static void	iwi_frame_intr(struct iwi_softc *, struct iwi_rx_data *, int,
    struct iwi_frame *);
static void	iwi_notification_intr(struct iwi_softc *, struct iwi_notif *);
static void	iwi_cmd_intr(struct iwi_softc *);
static void	iwi_rx_intr(struct iwi_softc *);
static void	iwi_tx_intr(struct iwi_softc *, struct iwi_tx_ring *);
static int	iwi_intr(void *);
static int	iwi_cmd(struct iwi_softc *, uint8_t, void *, uint8_t, int);
static void	iwi_write_ibssnode(struct iwi_softc *, const struct iwi_node *);
static int	iwi_tx_start(struct ifnet *, struct mbuf *, struct ieee80211_node *,
    int);
static void	iwi_start(struct ifnet *);
static void	iwi_watchdog(struct ifnet *);

static int	iwi_alloc_unr(struct iwi_softc *);
static void	iwi_free_unr(struct iwi_softc *, int);

static int	iwi_get_table0(struct iwi_softc *, uint32_t *);

static int	iwi_ioctl(struct ifnet *, u_long, void *);
static void	iwi_stop_master(struct iwi_softc *);
static int	iwi_reset(struct iwi_softc *);
static int	iwi_load_ucode(struct iwi_softc *, void *, int);
static int	iwi_load_firmware(struct iwi_softc *, void *, int);
static int	iwi_cache_firmware(struct iwi_softc *);
static void	iwi_free_firmware(struct iwi_softc *);
static int	iwi_config(struct iwi_softc *);
static int	iwi_set_chan(struct iwi_softc *, struct ieee80211_channel *);
static int	iwi_scan(struct iwi_softc *);
static int	iwi_auth_and_assoc(struct iwi_softc *);
static int	iwi_init(struct ifnet *);
static void	iwi_stop(struct ifnet *, int);
static int	iwi_getrfkill(struct iwi_softc *);
static void	iwi_led_set(struct iwi_softc *, uint32_t, int);
static void	iwi_sysctlattach(struct iwi_softc *);

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset iwi_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset iwi_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset iwi_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

static inline uint8_t
MEM_READ_1(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_1(sc, IWI_CSR_INDIRECT_DATA);
}

static inline uint32_t
MEM_READ_4(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_4(sc, IWI_CSR_INDIRECT_DATA);
}

CFATTACH_DECL_NEW(iwi, sizeof (struct iwi_softc), iwi_match, iwi_attach,
    iwi_detach, NULL);

static int
iwi_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return 0;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_2200BG ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_2225BG ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_2915ABG_2)
		return 1;

	return 0;
}

/* Base Address Register */
#define IWI_PCI_BAR0	0x10

static void
iwi_attach(device_t parent, device_t self, void *aux)
{
	struct iwi_softc *sc = device_private(self);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	pci_intr_handle_t ih;
	pcireg_t data;
	uint16_t val;
	int error, i;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	pci_aprint_devinfo(pa, NULL);

	/* clear unit numbers allocated to IBSS */
	sc->sc_unr = 0;

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    NULL)) && error != EOPNOTSUPP) {
		aprint_error_dev(self, "cannot activate %d\n", error);
		return;
	}

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);


	/* enable bus-mastering */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG, data);

	/* map the register window */
	error = pci_mapreg_map(pa, IWI_PCI_BAR0, PCI_MAPREG_TYPE_MEM |
	    PCI_MAPREG_MEM_TYPE_32BIT, 0, &memt, &memh, NULL, &sc->sc_sz);
	if (error != 0) {
		aprint_error_dev(self, "could not map memory space\n");
		return;
	}

	sc->sc_st = memt;
	sc->sc_sh = memh;
	sc->sc_dmat = pa->pa_dmat;

	/* disable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, 0);

	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "could not map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, iwi_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "could not establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	if (iwi_reset(sc) != 0) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		aprint_error_dev(self, "could not reset adapter\n");
		return;
	}

	ic->ic_ifp = ifp;
	ic->ic_wme.wme_update = iwi_wme_update;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	sc->sc_fwname = "ipw2200-bss.fw";

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_WPA |		/* 802.11i */
	    IEEE80211_C_WME;		/* 802.11e */

	/* read MAC address from EEPROM */
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 0);
	ic->ic_myaddr[0] = val & 0xff;
	ic->ic_myaddr[1] = val >> 8;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 1);
	ic->ic_myaddr[2] = val & 0xff;
	ic->ic_myaddr[3] = val >> 8;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 2);
	ic->ic_myaddr[4] = val & 0xff;
	ic->ic_myaddr[5] = val >> 8;

	aprint_verbose_dev(self, "802.11 address %s\n",
	    ether_sprintf(ic->ic_myaddr));

	/* read the NIC type from EEPROM */
	val = iwi_read_prom_word(sc, IWI_EEPROM_NIC_TYPE);
	sc->nictype = val & 0xff;

	DPRINTF(("%s: NIC type %d\n", device_xname(self), sc->nictype));

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_2915ABG_2) {
		/* set supported .11a rates (2915ABG only) */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = iwi_rateset_11a;

		/* set supported .11a channels */
		for (i = 36; i <= 64; i += 4) {
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
	ic->ic_sup_rates[IEEE80211_MODE_11B] = iwi_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = iwi_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = iwi_init;
	ifp->if_stop = iwi_stop;
	ifp->if_ioctl = iwi_ioctl;
	ifp->if_start = iwi_start;
	ifp->if_watchdog = iwi_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ic);
	/* override default methods */
	ic->ic_node_alloc = iwi_node_alloc;
	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = iwi_node_free;
	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwi_newstate;
	ieee80211_media_init(ic, iwi_media_change, iwi_media_status);

	/*
	 * Allocate rings.
	 */
	if (iwi_alloc_cmd_ring(sc, &sc->cmdq, IWI_CMD_RING_COUNT) != 0) {
		aprint_error_dev(self, "could not allocate command ring\n");
		goto fail;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[0], IWI_TX_RING_COUNT,
	    IWI_CSR_TX1_RIDX, IWI_CSR_TX1_WIDX);
	if (error != 0) {
		aprint_error_dev(self, "could not allocate Tx ring 1\n");
		goto fail;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[1], IWI_TX_RING_COUNT,
	    IWI_CSR_TX2_RIDX, IWI_CSR_TX2_WIDX);
	if (error != 0) {
		aprint_error_dev(self, "could not allocate Tx ring 2\n");
		goto fail;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[2], IWI_TX_RING_COUNT,
	    IWI_CSR_TX3_RIDX, IWI_CSR_TX3_WIDX);
	if (error != 0) {
		aprint_error_dev(self, "could not allocate Tx ring 3\n");
		goto fail;
	}

	error = iwi_alloc_tx_ring(sc, &sc->txq[3], IWI_TX_RING_COUNT,
	    IWI_CSR_TX4_RIDX, IWI_CSR_TX4_WIDX);
	if (error != 0) {
		aprint_error_dev(self, "could not allocate Tx ring 4\n");
		goto fail;
	}

	if (iwi_alloc_rx_ring(sc, &sc->rxq, IWI_RX_RING_COUNT) != 0) {
		aprint_error_dev(self, "could not allocate Rx ring\n");
		goto fail;
	}

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64, &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWI_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWI_TX_RADIOTAP_PRESENT);

	iwi_sysctlattach(sc);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	ieee80211_announce(ic);

	return;

fail:	iwi_detach(self, 0);
}

static int
iwi_detach(device_t self, int flags)
{
	struct iwi_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_if;

	pmf_device_deregister(self);

	if (ifp != NULL)
		iwi_stop(ifp, 1);

	iwi_free_firmware(sc);

	ieee80211_ifdetach(&sc->sc_ic);
	if (ifp != NULL)
		if_detach(ifp);

	iwi_free_cmd_ring(sc, &sc->cmdq);
	iwi_free_tx_ring(sc, &sc->txq[0]);
	iwi_free_tx_ring(sc, &sc->txq[1]);
	iwi_free_tx_ring(sc, &sc->txq[2]);
	iwi_free_tx_ring(sc, &sc->txq[3]);
	iwi_free_rx_ring(sc, &sc->rxq);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);

	return 0;
}

static int
iwi_alloc_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring,
    int count)
{
	int error, nsegs;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;

	/*
	 * Allocate and map command ring
	 */
	error = bus_dmamap_create(sc->sc_dmat,
	    IWI_CMD_DESC_SIZE * count, 1,
	    IWI_CMD_DESC_SIZE * count, 0,
	    BUS_DMA_NOWAIT, &ring->desc_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create command ring DMA map\n");
		ring->desc_map = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    IWI_CMD_DESC_SIZE * count, PAGE_SIZE, 0,
	    &sc->cmdq.desc_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate command ring DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->cmdq.desc_seg, nsegs,
	    IWI_CMD_DESC_SIZE * count,
	    (void **)&sc->cmdq.desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not map command ring DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->cmdq.desc_map, sc->cmdq.desc,
	    IWI_CMD_DESC_SIZE * count, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load command ring DMA map\n");
		goto fail;
	}

	memset(sc->cmdq.desc, 0,
	    IWI_CMD_DESC_SIZE * count);

	return 0;

fail:	return error;
}

static void
iwi_reset_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	int i;

	for (i = ring->next; i != ring->cur;) {
		bus_dmamap_sync(sc->sc_dmat, sc->cmdq.desc_map,
		    i * IWI_CMD_DESC_SIZE, IWI_CMD_DESC_SIZE,
		    BUS_DMASYNC_POSTWRITE);

		wakeup(&ring->desc[i]);
		i = (i + 1) % ring->count;
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;
}

static void
iwi_free_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	if (ring->desc_map != NULL) {
		if (ring->desc != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ring->desc_map);
			bus_dmamem_unmap(sc->sc_dmat, (void *)ring->desc,
			    IWI_CMD_DESC_SIZE * ring->count);
			bus_dmamem_free(sc->sc_dmat, &ring->desc_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, ring->desc_map);
	}
}

static int
iwi_alloc_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring,
    int count, bus_size_t csr_ridx, bus_size_t csr_widx)
{
	int i, error, nsegs;

	ring->count  = 0;
	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->csr_ridx = csr_ridx;
	ring->csr_widx = csr_widx;

	/*
	 * Allocate and map Tx ring
	 */
	error = bus_dmamap_create(sc->sc_dmat,
	    IWI_TX_DESC_SIZE * count, 1,
	    IWI_TX_DESC_SIZE * count, 0, BUS_DMA_NOWAIT,
	    &ring->desc_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create tx ring DMA map\n");
		ring->desc_map = NULL;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    IWI_TX_DESC_SIZE * count, PAGE_SIZE, 0,
	    &ring->desc_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate tx ring DMA memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->desc_seg, nsegs,
	    IWI_TX_DESC_SIZE * count,
	    (void **)&ring->desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not map tx ring DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->desc_map, ring->desc,
	    IWI_TX_DESC_SIZE * count, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load tx ring DMA map\n");
		goto fail;
	}

	memset(ring->desc, 0, IWI_TX_DESC_SIZE * count);

	ring->data = malloc(count * sizeof (struct iwi_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}
	ring->count = count;

	/*
	 * Allocate Tx buffers DMA maps
	 */
	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, IWI_MAX_NSEG,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &ring->data[i].map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create tx buf DMA map");
			ring->data[i].map = NULL;
			goto fail;
		}
	}
	return 0;

fail:	return error;
}

static void
iwi_reset_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	struct iwi_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			m_freem(data->m);
			data->m = NULL;
		}
	
		if (data->map != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
		}

		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;
}

static void
iwi_free_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	int i;
	struct iwi_tx_data *data;

	if (ring->desc_map != NULL) {
		if (ring->desc != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ring->desc_map);
			bus_dmamem_unmap(sc->sc_dmat, (void *)ring->desc,
			    IWI_TX_DESC_SIZE * ring->count);
			bus_dmamem_free(sc->sc_dmat, &ring->desc_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, ring->desc_map);
	}

	for (i = 0; i < ring->count; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			m_freem(data->m);
		}

		if (data->map != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
	}
}

static int
iwi_alloc_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring, int count)
{
	int i, error;

	ring->count = 0;
	ring->cur = 0;

	ring->data = malloc(count * sizeof (struct iwi_rx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	ring->count = count;

	/*
	 * Allocate and map Rx buffers
	 */
	for (i = 0; i < count; i++) {

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &ring->data[i].map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create rx buf DMA map");
			ring->data[i].map = NULL;
			goto fail;
		}

		if ((ring->data[i].m = iwi_alloc_rx_buf(sc)) == NULL) {
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load_mbuf(sc->sc_dmat, ring->data[i].map,
		    ring->data[i].m, BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not load rx buffer DMA map\n");
			goto fail;
		}

		bus_dmamap_sync(sc->sc_dmat, ring->data[i].map, 0,
		    ring->data[i].map->dm_mapsize, BUS_DMASYNC_PREREAD);
	}

	return 0;

fail:	return error;
}

static void
iwi_reset_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	ring->cur = 0;
}

static void
iwi_free_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	int i;
	struct iwi_rx_data *data;

	for (i = 0; i < ring->count; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			m_freem(data->m);
		}

		if (data->map != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			bus_dmamap_destroy(sc->sc_dmat, data->map);
		}

	}
}

static struct ieee80211_node *
iwi_node_alloc(struct ieee80211_node_table *nt)
{
	struct iwi_node *in;

	in = malloc(sizeof (struct iwi_node), M_80211_NODE, M_NOWAIT | M_ZERO);
	if (in == NULL)
		return NULL;

	in->in_station = -1;

	return &in->in_node;
}

static int
iwi_alloc_unr(struct iwi_softc *sc)
{
	int i;

	for (i = 0; i < IWI_MAX_IBSSNODE - 1; i++)
		if ((sc->sc_unr & (1 << i)) == 0) {
			sc->sc_unr |= 1 << i;
			return i;
		}

	return -1;
}

static void
iwi_free_unr(struct iwi_softc *sc, int r)
{

	sc->sc_unr &= 1 << r;
}

static void
iwi_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct iwi_softc *sc = ic->ic_ifp->if_softc;
	struct iwi_node *in = (struct iwi_node *)ni;

	if (in->in_station != -1)
		iwi_free_unr(sc, in->in_station);

	sc->sc_node_free(ni);
}

static int
iwi_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		iwi_init(ifp);

	return 0;
}

/*
 * Convert h/w rate code to IEEE rate code.
 */
static int
iwi_cvtrate(int iwirate)
{
	switch (iwirate) {
	case IWI_RATE_DS1:	return 2;
	case IWI_RATE_DS2:	return 4;
	case IWI_RATE_DS5:	return 11;
	case IWI_RATE_DS11:	return 22;
	case IWI_RATE_OFDM6:	return 12;
	case IWI_RATE_OFDM9:	return 18;
	case IWI_RATE_OFDM12:	return 24;
	case IWI_RATE_OFDM18:	return 36;
	case IWI_RATE_OFDM24:	return 48;
	case IWI_RATE_OFDM36:	return 72;
	case IWI_RATE_OFDM48:	return 96;
	case IWI_RATE_OFDM54:	return 108;
	}
	return 0;
}

/*
 * The firmware automatically adapts the transmit speed.  We report its current
 * value here.
 */
static void
iwi_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int rate;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/* read current transmission rate from adapter */
	rate = iwi_cvtrate(CSR_READ_4(sc, IWI_CSR_CURRENT_TX_RATE));
	imr->ifm_active |= ieee80211_rate2media(ic, rate, ic->ic_curmode);

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;

	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;

	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;

	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_HOSTAP:
		/* should not get there */
		break;
	}
}

static int
iwi_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct iwi_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(("%s: %s -> %s flags 0x%x\n", __func__,
	    ieee80211_state_name[ic->ic_state],
	    ieee80211_state_name[nstate], sc->flags));

	switch (nstate) {
	case IEEE80211_S_SCAN:
		if (sc->flags & IWI_FLAG_SCANNING)
			break;

		ieee80211_node_table_reset(&ic->ic_scan);
		ic->ic_flags |= IEEE80211_F_SCAN | IEEE80211_F_ASCAN;
		sc->flags |= IWI_FLAG_SCANNING;
		/* blink the led while scanning */
		iwi_led_set(sc, IWI_LED_ASSOCIATED, 1);
		iwi_scan(sc);
		break;

	case IEEE80211_S_AUTH:
		iwi_auth_and_assoc(sc);
		break;

	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    ic->ic_state == IEEE80211_S_SCAN)
			iwi_auth_and_assoc(sc);
		else if (ic->ic_opmode == IEEE80211_M_MONITOR)
			iwi_set_chan(sc, ic->ic_ibss_chan);
		break;
	case IEEE80211_S_ASSOC:
		iwi_led_set(sc, IWI_LED_ASSOCIATED, 0);
		if (ic->ic_state == IEEE80211_S_AUTH)
			break;
		iwi_auth_and_assoc(sc);
		break;

	case IEEE80211_S_INIT:
		sc->flags &= ~IWI_FLAG_SCANNING;
		break;
	}

	return sc->sc_newstate(ic, nstate, arg);
}

/*
 * WME parameters coming from IEEE 802.11e specification.  These values are
 * already declared in ieee80211_proto.c, but they are static so they can't
 * be reused here.
 */
static const struct wmeParams iwi_wme_cck_params[WME_NUM_AC] = {
	{ 0, 3, 5,  7,   0, 0, },	/* WME_AC_BE */
	{ 0, 3, 5, 10,   0, 0, },	/* WME_AC_BK */
	{ 0, 2, 4,  5, 188, 0, },	/* WME_AC_VI */
	{ 0, 2, 3,  4, 102, 0, },	/* WME_AC_VO */
};

static const struct wmeParams iwi_wme_ofdm_params[WME_NUM_AC] = {
	{ 0, 3, 4,  6,   0, 0, },	/* WME_AC_BE */
	{ 0, 3, 4, 10,   0, 0, },	/* WME_AC_BK */
	{ 0, 2, 3,  4,  94, 0, },	/* WME_AC_VI */
	{ 0, 2, 2,  3,  47, 0, },	/* WME_AC_VO */
};

static int
iwi_wme_update(struct ieee80211com *ic)
{
#define IWI_EXP2(v)	htole16((1 << (v)) - 1)
#define IWI_USEC(v)	htole16(IEEE80211_TXOP_TO_US(v))
	struct iwi_softc *sc = ic->ic_ifp->if_softc;
	struct iwi_wme_params wme[3];
	const struct wmeParams *wmep;
	int ac;

	/*
	 * We shall not override firmware default WME values if WME is not
	 * actually enabled.
	 */
	if (!(ic->ic_flags & IEEE80211_F_WME))
		return 0;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		/* set WME values for current operating mode */
		wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
		wme[0].aifsn[ac] = wmep->wmep_aifsn;
		wme[0].cwmin[ac] = IWI_EXP2(wmep->wmep_logcwmin);
		wme[0].cwmax[ac] = IWI_EXP2(wmep->wmep_logcwmax);
		wme[0].burst[ac] = IWI_USEC(wmep->wmep_txopLimit);
		wme[0].acm[ac]   = wmep->wmep_acm;

		/* set WME values for CCK modulation */
		wmep = &iwi_wme_cck_params[ac];
		wme[1].aifsn[ac] = wmep->wmep_aifsn;
		wme[1].cwmin[ac] = IWI_EXP2(wmep->wmep_logcwmin);
		wme[1].cwmax[ac] = IWI_EXP2(wmep->wmep_logcwmax);
		wme[1].burst[ac] = IWI_USEC(wmep->wmep_txopLimit);
		wme[1].acm[ac]   = wmep->wmep_acm;

		/* set WME values for OFDM modulation */
		wmep = &iwi_wme_ofdm_params[ac];
		wme[2].aifsn[ac] = wmep->wmep_aifsn;
		wme[2].cwmin[ac] = IWI_EXP2(wmep->wmep_logcwmin);
		wme[2].cwmax[ac] = IWI_EXP2(wmep->wmep_logcwmax);
		wme[2].burst[ac] = IWI_USEC(wmep->wmep_txopLimit);
		wme[2].acm[ac]   = wmep->wmep_acm;
	}

	DPRINTF(("Setting WME parameters\n"));
	return iwi_cmd(sc, IWI_CMD_SET_WME_PARAMS, wme, sizeof wme, 1);
#undef IWI_USEC
#undef IWI_EXP2
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM.
 */
static uint16_t
iwi_read_prom_word(struct iwi_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* Clock C once before the first command */
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* Write start bit (1) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);

	/* Write READ opcode (10) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);

	/* Write address A7-A0 */
	for (n = 7; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D));
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D) | IWI_EEPROM_C);
	}

	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* Read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
		tmp = MEM_READ_4(sc, IWI_MEM_EEPROM_CTL);
		val |= ((tmp & IWI_EEPROM_Q) >> IWI_EEPROM_SHIFT_Q) << n;
	}

	IWI_EEPROM_CTL(sc, 0);

	/* Clear Chip Select and clock C */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_C);

	return val;
}

/*
 * XXX: Hack to set the current channel to the value advertised in beacons or
 * probe responses. Only used during AP detection.
 */
static void
iwi_fix_channel(struct ieee80211com *ic, struct mbuf *m)
{
	struct ieee80211_frame *wh;
	uint8_t subtype;
	uint8_t *frm, *efrm;

	wh = mtod(m, struct ieee80211_frame *);

	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_MGT)
		return;

	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
	    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP)
		return;

	frm = (uint8_t *)(wh + 1);
	efrm = mtod(m, uint8_t *) + m->m_len;

	frm += 12;	/* skip tstamp, bintval and capinfo fields */
	while (frm < efrm) {
		if (*frm == IEEE80211_ELEMID_DSPARMS)
#if IEEE80211_CHAN_MAX < 255
		if (frm[2] <= IEEE80211_CHAN_MAX)
#endif
			ic->ic_curchan = &ic->ic_channels[frm[2]];

		frm += frm[1] + 2;
	}
}

static struct mbuf *
iwi_alloc_rx_buf(struct iwi_softc *sc)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate rx mbuf\n");
		return NULL;
	}

	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate rx mbuf cluster\n");
		m_freem(m);
		return NULL;
	}

	m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;
	return m;
}

static void
iwi_frame_intr(struct iwi_softc *sc, struct iwi_rx_data *data, int i,
    struct iwi_frame *frame)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct mbuf *m, *m_new;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	int error;

	DPRINTFN(5, ("received frame len=%u chan=%u rssi=%u\n",
	    le16toh(frame->len), frame->chan, frame->rssi_dbm));

	if (le16toh(frame->len) < sizeof (struct ieee80211_frame) ||
	    le16toh(frame->len) > MCLBYTES) {
		DPRINTF(("%s: bad frame length\n", device_xname(sc->sc_dev)));
		ifp->if_ierrors++;
		return;
	}

	/*
	 * Try to allocate a new mbuf for this ring element and
	 * load it before processing the current mbuf. If the ring
	 * element cannot be reloaded, drop the received packet
	 * and reuse the old mbuf. In the unlikely case that
	 * the old mbuf can't be reloaded either, explicitly panic.
	 *
	 * XXX Reorganize buffer by moving elements from the logical
	 * end of the ring to the front instead of dropping.
	 */
	if ((m_new = iwi_alloc_rx_buf(sc)) == NULL) {
		ifp->if_ierrors++;
		return;
	}

	bus_dmamap_unload(sc->sc_dmat, data->map);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m_new,
	    BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load rx buf DMA map\n");
		m_freem(m_new);
		ifp->if_ierrors++;
		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map,
		    data->m, BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (error)
			panic("%s: unable to remap rx buf",
			    device_xname(sc->sc_dev));
		return;
	}

	/*
	 * New mbuf successfully loaded, update RX ring and continue
	 * processing.
	 */
	m = data->m;
	data->m = m_new;
	CSR_WRITE_4(sc, IWI_CSR_RX_BASE + i * 4, data->map->dm_segs[0].ds_addr);

	/* Finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = sizeof (struct iwi_hdr) +
	    sizeof (struct iwi_frame) + le16toh(frame->len);

	m_adj(m, sizeof (struct iwi_hdr) + sizeof (struct iwi_frame));

	if (ic->ic_state == IEEE80211_S_SCAN)
		iwi_fix_channel(ic, m);

	if (sc->sc_drvbpf != NULL) {
		struct iwi_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_rate = iwi_cvtrate(frame->rate);
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[frame->chan].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[frame->chan].ic_flags);
		tap->wr_antsignal = frame->signal;
		tap->wr_antenna = frame->antenna;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* Send the frame to the upper layer */
	ieee80211_input(ic, m, ni, frame->rssi_dbm, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);
}

static void
iwi_notification_intr(struct iwi_softc *sc, struct iwi_notif *notif)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_notif_authentication *auth;
	struct iwi_notif_association *assoc;
	struct iwi_notif_beacon_state *beacon;

	switch (notif->type) {
	case IWI_NOTIF_TYPE_SCAN_CHANNEL:
#ifdef IWI_DEBUG
		{
			struct iwi_notif_scan_channel *chan =
			    (struct iwi_notif_scan_channel *)(notif + 1);

			DPRINTFN(2, ("Scan of channel %u complete (%u)\n",
			    ic->ic_channels[chan->nchan].ic_freq, chan->nchan));
		}
#endif
		break;

	case IWI_NOTIF_TYPE_SCAN_COMPLETE:
#ifdef IWI_DEBUG
		{
			struct iwi_notif_scan_complete *scan =
			    (struct iwi_notif_scan_complete *)(notif + 1);

			DPRINTFN(2, ("Scan completed (%u, %u)\n", scan->nchan,
			    scan->status));
		}
#endif

		/* monitor mode uses scan to set the channel ... */
		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			sc->flags &= ~IWI_FLAG_SCANNING;
			ieee80211_end_scan(ic);
		} else
			iwi_set_chan(sc, ic->ic_ibss_chan);
		break;

	case IWI_NOTIF_TYPE_AUTHENTICATION:
		auth = (struct iwi_notif_authentication *)(notif + 1);

		DPRINTFN(2, ("Authentication (%u)\n", auth->state));

		switch (auth->state) {
		case IWI_AUTH_SUCCESS:
			ieee80211_node_authorize(ic->ic_bss);
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
			break;

		case IWI_AUTH_FAIL:
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown authentication state %u\n", auth->state);
		}
		break;

	case IWI_NOTIF_TYPE_ASSOCIATION:
		assoc = (struct iwi_notif_association *)(notif + 1);

		DPRINTFN(2, ("Association (%u, %u)\n", assoc->state,
		    assoc->status));

		switch (assoc->state) {
		case IWI_AUTH_SUCCESS:
			/* re-association, do nothing */
			break;

		case IWI_ASSOC_SUCCESS:
			ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
			break;

		case IWI_ASSOC_FAIL:
			ieee80211_begin_scan(ic, 1);
			break;

		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown association state %u\n", assoc->state);
		}
		break;

	case IWI_NOTIF_TYPE_BEACON:
		beacon = (struct iwi_notif_beacon_state *)(notif + 1);

		if (beacon->state == IWI_BEACON_MISS) {
			DPRINTFN(5, ("%s: %u beacon(s) missed\n",
			    device_xname(sc->sc_dev), le32toh(beacon->number)));
		}
		break;

	case IWI_NOTIF_TYPE_FRAG_LENGTH:
	case IWI_NOTIF_TYPE_LINK_QUALITY:
	case IWI_NOTIF_TYPE_TGI_TX_KEY:
	case IWI_NOTIF_TYPE_CALIBRATION:
	case IWI_NOTIF_TYPE_NOISE:
		DPRINTFN(5, ("Notification (%u)\n", notif->type));
		break;

	default:
		DPRINTF(("%s: unknown notification type %u flags 0x%x len %d\n",
		    device_xname(sc->sc_dev), notif->type, notif->flags,
		    le16toh(notif->len)));
	}
}

static void
iwi_cmd_intr(struct iwi_softc *sc)
{

	(void)CSR_READ_4(sc, IWI_CSR_CMD_RIDX);

	bus_dmamap_sync(sc->sc_dmat, sc->cmdq.desc_map,
	    sc->cmdq.next * IWI_CMD_DESC_SIZE, IWI_CMD_DESC_SIZE,
	    BUS_DMASYNC_POSTWRITE);

	wakeup(&sc->cmdq.desc[sc->cmdq.next]);

	sc->cmdq.next = (sc->cmdq.next + 1) % sc->cmdq.count;

	if (--sc->cmdq.queued > 0) {
		CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, (sc->cmdq.next + 1) % sc->cmdq.count);
	}
}

static void
iwi_rx_intr(struct iwi_softc *sc)
{
	struct iwi_rx_data *data;
	struct iwi_hdr *hdr;
	uint32_t hw;

	hw = CSR_READ_4(sc, IWI_CSR_RX_RIDX);

	for (; sc->rxq.cur != hw;) {
		data = &sc->rxq.data[sc->rxq.cur];

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTREAD);

		hdr = mtod(data->m, struct iwi_hdr *);

		switch (hdr->type) {
		case IWI_HDR_TYPE_FRAME:
			iwi_frame_intr(sc, data, sc->rxq.cur,
			    (struct iwi_frame *)(hdr + 1));
			break;

		case IWI_HDR_TYPE_NOTIF:
			iwi_notification_intr(sc,
			    (struct iwi_notif *)(hdr + 1));
			break;

		default:
			aprint_error_dev(sc->sc_dev, "unknown hdr type %u\n",
			    hdr->type);
		}

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_PREREAD);

		DPRINTFN(15, ("rx done idx=%u\n", sc->rxq.cur));

		sc->rxq.cur = (sc->rxq.cur + 1) % sc->rxq.count;
	}

	/* Tell the firmware what we have processed */
	hw = (hw == 0) ? sc->rxq.count - 1 : hw - 1;
	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, hw);
}

static void
iwi_tx_intr(struct iwi_softc *sc, struct iwi_tx_ring *txq)
{
	struct ifnet *ifp = &sc->sc_if;
	struct iwi_tx_data *data;
	uint32_t hw;

	hw = CSR_READ_4(sc, txq->csr_ridx);

	for (; txq->next != hw;) {
		data = &txq->data[txq->next];

		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
		ieee80211_free_node(data->ni);
		data->ni = NULL;

		DPRINTFN(15, ("tx done idx=%u\n", txq->next));

		ifp->if_opackets++;

		txq->queued--;
		txq->next = (txq->next + 1) % txq->count;
	}

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Call start() since some buffer descriptors have been released */
	(*ifp->if_start)(ifp);
}

static int
iwi_intr(void *arg)
{
	struct iwi_softc *sc = arg;
	uint32_t r;

	if ((r = CSR_READ_4(sc, IWI_CSR_INTR)) == 0 || r == 0xffffffff)
		return 0;

	/* Acknowledge interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR, r);

	if (r & IWI_INTR_FATAL_ERROR) {
		aprint_error_dev(sc->sc_dev, "fatal error\n");
		sc->sc_ic.ic_ifp->if_flags &= ~IFF_UP;
		iwi_stop(&sc->sc_if, 1);
		return (1);
	}

	if (r & IWI_INTR_FW_INITED) {
		if (!(r & (IWI_INTR_FATAL_ERROR | IWI_INTR_PARITY_ERROR)))
			wakeup(sc);
	}

	if (r & IWI_INTR_RADIO_OFF) {
		DPRINTF(("radio transmitter off\n"));
		sc->sc_ic.ic_ifp->if_flags &= ~IFF_UP;
		iwi_stop(&sc->sc_if, 1);
		return (1);
	}

	if (r & IWI_INTR_CMD_DONE)
		iwi_cmd_intr(sc);

	if (r & IWI_INTR_TX1_DONE)
		iwi_tx_intr(sc, &sc->txq[0]);

	if (r & IWI_INTR_TX2_DONE)
		iwi_tx_intr(sc, &sc->txq[1]);

	if (r & IWI_INTR_TX3_DONE)
		iwi_tx_intr(sc, &sc->txq[2]);

	if (r & IWI_INTR_TX4_DONE)
		iwi_tx_intr(sc, &sc->txq[3]);

	if (r & IWI_INTR_RX_DONE)
		iwi_rx_intr(sc);

	if (r & IWI_INTR_PARITY_ERROR)
		aprint_error_dev(sc->sc_dev, "parity error\n");

	return 1;
}

static int
iwi_cmd(struct iwi_softc *sc, uint8_t type, void *data, uint8_t len,
    int async)
{
	struct iwi_cmd_desc *desc;

	desc = &sc->cmdq.desc[sc->cmdq.cur];

	desc->hdr.type = IWI_HDR_TYPE_COMMAND;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->type = type;
	desc->len = len;
	memcpy(desc->data, data, len);

	bus_dmamap_sync(sc->sc_dmat, sc->cmdq.desc_map,
	    sc->cmdq.cur * IWI_CMD_DESC_SIZE,
	    IWI_CMD_DESC_SIZE, BUS_DMASYNC_PREWRITE);

	DPRINTFN(2, ("sending command idx=%u type=%u len=%u async=%d\n",
	    sc->cmdq.cur, type, len, async));

	sc->cmdq.cur = (sc->cmdq.cur + 1) % sc->cmdq.count;

	if (++sc->cmdq.queued == 1)
		CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.cur);

	return async ? 0 : tsleep(desc, 0, "iwicmd", hz);
}

static void
iwi_write_ibssnode(struct iwi_softc *sc, const struct iwi_node *in)
{
	struct iwi_ibssnode node;

	/* write node information into NIC memory */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, in->in_node.ni_macaddr);

	CSR_WRITE_REGION_1(sc,
	    IWI_CSR_NODE_BASE + in->in_station * sizeof node,
	    (uint8_t *)&node, sizeof node);
}

static int
iwi_tx_start(struct ifnet *ifp, struct mbuf *m0, struct ieee80211_node *ni,
    int ac)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_node *in = (struct iwi_node *)ni;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	const struct chanAccParams *cap;
	struct iwi_tx_ring *txq = &sc->txq[ac];
	struct iwi_tx_data *data;
	struct iwi_tx_desc *desc;
	struct mbuf *mnew;
	int error, hdrlen, i, noack = 0;

	wh = mtod(m0, struct ieee80211_frame *);

	if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
		hdrlen = sizeof (struct ieee80211_qosframe);
		cap = &ic->ic_wme.wme_chanParams;
		noack = cap->cap_wmeParams[ac].wmep_noackPolicy;
	} else
		hdrlen = sizeof (struct ieee80211_frame);

	/*
	 * This is only used in IBSS mode where the firmware expect an index
	 * in a h/w table instead of a destination address.
	 */
	if (ic->ic_opmode == IEEE80211_M_IBSS && in->in_station == -1) {
		in->in_station = iwi_alloc_unr(sc);

		if (in->in_station == -1) {	/* h/w table is full */
			m_freem(m0);
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			return 0;
		}
		iwi_write_ibssnode(sc, in);
	}

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	if (sc->sc_drvbpf != NULL) {
		struct iwi_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data = &txq->data[txq->cur];
	desc = &txq->desc[txq->cur];

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, hdrlen, (void *)&desc->wh);
	m_adj(m0, hdrlen);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
	    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
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

		/* If the data won't fit in the header, get a cluster */
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
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not map mbuf (error %d)\n", error);
			m_freem(m0);
			return error;
		}
	}

	data->m = m0;
	data->ni = ni;

	desc->hdr.type = IWI_HDR_TYPE_DATA;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->station =
	    (ic->ic_opmode == IEEE80211_M_IBSS) ? in->in_station : 0;
	desc->cmd = IWI_DATA_CMD_TX;
	desc->len = htole16(m0->m_pkthdr.len);
	desc->flags = 0;
	desc->xflags = 0;

	if (!noack && !IEEE80211_IS_MULTICAST(desc->wh.i_addr1))
		desc->flags |= IWI_DATA_FLAG_NEED_ACK;

#if 0
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		desc->wh.i_fc[1] |= IEEE80211_FC1_WEP;
		desc->wep_txkey = ic->ic_crypto.cs_def_txkey;
	} else
#endif
		desc->flags |= IWI_DATA_FLAG_NO_WEP;

	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		desc->flags |= IWI_DATA_FLAG_SHPREAMBLE;

	if (desc->wh.i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS)
		desc->xflags |= IWI_DATA_XFLAG_QOS;

	if (ic->ic_curmode == IEEE80211_MODE_11B)
		desc->xflags |= IWI_DATA_XFLAG_CCK;

	desc->nseg = htole32(data->map->dm_nsegs);
	for (i = 0; i < data->map->dm_nsegs; i++) {
		desc->seg_addr[i] = htole32(data->map->dm_segs[i].ds_addr);
		desc->seg_len[i]  = htole16(data->map->dm_segs[i].ds_len);
	}

	bus_dmamap_sync(sc->sc_dmat, txq->desc_map,
	    txq->cur * IWI_TX_DESC_SIZE,
	    IWI_TX_DESC_SIZE, BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(5, ("sending data frame txq=%u idx=%u len=%u nseg=%u\n",
	    ac, txq->cur, le16toh(desc->len), le32toh(desc->nseg)));

	/* Inform firmware about this new packet */
	txq->queued++;
	txq->cur = (txq->cur + 1) % txq->count;
	CSR_WRITE_4(sc, txq->csr_widx, txq->cur);

	return 0;
}

static void
iwi_start(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	int ac;

	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (m0->m_len < sizeof (struct ether_header) &&
		    (m0 = m_pullup(m0, sizeof (struct ether_header))) == NULL) {
			ifp->if_oerrors++;
			continue;
		}

		eh = mtod(m0, struct ether_header *);
		ni = ieee80211_find_txnode(ic, eh->ether_dhost);
		if (ni == NULL) {
			m_freem(m0);
			ifp->if_oerrors++;
			continue;
		}

		/* classify mbuf so we can find which tx ring to use */
		if (ieee80211_classify(ic, m0, ni) != 0) {
			m_freem(m0);
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		/* no QoS encapsulation for EAPOL frames */
		ac = (eh->ether_type != htons(ETHERTYPE_PAE)) ?
		    M_WME_GETAC(m0) : WME_AC_BE;

		if (sc->txq[ac].queued > sc->txq[ac].count - 8) {
			/* there is no place left in this ring */
			IF_PREPEND(&ifp->if_snd, m0);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		bpf_mtap(ifp, m0);

		m0 = ieee80211_encap(ic, m0, ni);
		if (m0 == NULL) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		bpf_mtap3(ic->ic_rawbpf, m0);

		if (iwi_tx_start(ifp, m0, ni, ac) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}

		/* start watchdog timer */
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

static void
iwi_watchdog(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			ifp->if_oerrors++;
			ifp->if_flags &= ~IFF_UP;
			iwi_stop(ifp, 1);
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(&sc->sc_ic);
}

static int
iwi_get_table0(struct iwi_softc *sc, uint32_t *tbl)
{
	uint32_t size, buf[128];

	if (!(sc->flags & IWI_FLAG_FW_INITED)) {
		memset(buf, 0, sizeof buf);
		return copyout(buf, tbl, sizeof buf);
	}

	size = min(CSR_READ_4(sc, IWI_CSR_TABLE0_SIZE), 128 - 1);
	CSR_READ_REGION_4(sc, IWI_CSR_TABLE0_BASE, &buf[1], size);

	return copyout(buf, tbl, sizeof buf);
}

static int
iwi_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
#define	IS_RUNNING(ifp) \
	((ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING))

	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;
	int val;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				iwi_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iwi_stop(ifp, 1);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* XXX no h/w multicast filter? --dyoung */
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/* setup multicast filter, etc */
			error = 0;
		}
		break;

	case SIOCGTABLE0:
		error = iwi_get_table0(sc, (uint32_t *)ifr->ifr_data);
		break;

	case SIOCGRADIO:
		val = !iwi_getrfkill(sc);
		error = copyout(&val, (int *)ifr->ifr_data, sizeof val);
		break;

	case SIOCSIFMEDIA:
		if (ifr->ifr_media & IFM_IEEE80211_ADHOC) {
			sc->sc_fwname = "ipw2200-ibss.fw";
		} else if (ifr->ifr_media & IFM_IEEE80211_MONITOR) {
			sc->sc_fwname = "ipw2200-sniffer.fw";
		} else {
			sc->sc_fwname = "ipw2200-bss.fw";
		}
		error = iwi_cache_firmware(sc);
		if (error)
 			break;
 		/* FALLTRHOUGH */

	default:
		error = ieee80211_ioctl(&sc->sc_ic, cmd, data);

		if (error == ENETRESET) {
			if (IS_RUNNING(ifp) &&
			    (ic->ic_roaming != IEEE80211_ROAMING_MANUAL))
				iwi_init(ifp);
			error = 0;
		}
	}

	splx(s);
	return error;
#undef IS_RUNNING
}

static void
iwi_stop_master(struct iwi_softc *sc)
{
	int ntries;

	/* Disable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, 0);

	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5)
		aprint_error_dev(sc->sc_dev, "timeout waiting for master\n");

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_PRINCETON_RESET);

	sc->flags &= ~IWI_FLAG_FW_INITED;
}

static int
iwi_reset(struct iwi_softc *sc)
{
	int i, ntries;

	iwi_stop_master(sc);

	/* Move adapter to D0 state */
	CSR_WRITE_4(sc, IWI_CSR_CTL, CSR_READ_4(sc, IWI_CSR_CTL) |
	    IWI_CTL_INIT);

	/* Initialize Phase-Locked Level  (PLL) */
	CSR_WRITE_4(sc, IWI_CSR_READ_INT, IWI_READ_INT_INIT_HOST);

	/* Wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_CTL) & IWI_CTL_CLOCK_READY)
			break;
		DELAY(200);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for clock stabilization\n");
		return ETIMEDOUT;
	}

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_SW_RESET);

	DELAY(10);

	CSR_WRITE_4(sc, IWI_CSR_CTL, CSR_READ_4(sc, IWI_CSR_CTL) |
	    IWI_CTL_INIT);

	/* Clear NIC memory */
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0);
	for (i = 0; i < 0xc000; i++)
		CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	return 0;
}

static int
iwi_load_ucode(struct iwi_softc *sc, void *uc, int size)
{
	uint16_t *w;
	int ntries, i;

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5) {
		aprint_error_dev(sc->sc_dev, "timeout waiting for master\n");
		return ETIMEDOUT;
	}

	MEM_WRITE_4(sc, 0x3000e0, 0x80000000);
	DELAY(5000);
	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) &
	    ~IWI_RST_PRINCETON_RESET);
	DELAY(5000);
	MEM_WRITE_4(sc, 0x3000e0, 0);
	DELAY(1000);
	MEM_WRITE_4(sc, 0x300004, 1);
	DELAY(1000);
	MEM_WRITE_4(sc, 0x300004, 0);
	DELAY(1000);
	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x40);
	DELAY(1000);

	/* Adapter is buggy, we must set the address for each word */
	for (w = uc; size > 0; w++, size -= 2)
		MEM_WRITE_2(sc, 0x200010, htole16(*w));

	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x80);

	/* Wait until we get a response in the uc queue */
	for (ntries = 0; ntries < 100; ntries++) {
		if (MEM_READ_1(sc, 0x200000) & 1)
			break;
		DELAY(100);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for ucode to initialize\n");
		return ETIMEDOUT;
	}

	/* Empty the uc queue or the firmware will not initialize properly */
	for (i = 0; i < 7; i++)
		MEM_READ_4(sc, 0x200004);

	MEM_WRITE_1(sc, 0x200000, 0x00);

	return 0;
}

/* macro to handle unaligned little endian data in firmware image */
#define GETLE32(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)
static int
iwi_load_firmware(struct iwi_softc *sc, void *fw, int size)
{
	bus_dmamap_t map;
	u_char *p, *end;
	uint32_t sentinel, ctl, sum;
	uint32_t cs, sl, cd, cl;
	int ntries, nsegs, error;
	int sn;

	nsegs = atop((vaddr_t)fw+size-1) - atop((vaddr_t)fw) + 1;

	/* Create a DMA map for the firmware image */
	error = bus_dmamap_create(sc->sc_dmat, size, nsegs, size, 0,
	    BUS_DMA_NOWAIT, &map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not create firmware DMA map\n");
		map = NULL;
		goto fail1;
	}

	error = bus_dmamap_load(sc->sc_dmat, map, fw, size, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load fw dma map(%d)\n",
		    error);
		goto fail2;
	}

	/* Make sure the adapter will get up-to-date values */
	bus_dmamap_sync(sc->sc_dmat, map, 0, size, BUS_DMASYNC_PREWRITE);

	/* Tell the adapter where the command blocks are stored */
	MEM_WRITE_4(sc, 0x3000a0, 0x27000);

	/*
	 * Store command blocks into adapter's internal memory using register
	 * indirections. The adapter will read the firmware image through DMA
	 * using information stored in command blocks.
	 */
	p = fw;
	end = p + size;
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0x27000);

	sn = 0;
	sl = cl = 0;
	cs = cd = 0;
	while (p < end) {
		if (sl == 0) {
			cs = map->dm_segs[sn].ds_addr;
			sl = map->dm_segs[sn].ds_len;
			sn++;
		}
		if (cl == 0) {
			cd = GETLE32(p); p += 4; cs += 4; sl -= 4;
			cl = GETLE32(p); p += 4; cs += 4; sl -= 4;
		}
		while (sl > 0 && cl > 0) {
			int len = min(cl, sl);

			sl -= len;
			cl -= len;
			p += len;

			while (len > 0) {
				int mlen = min(len, IWI_CB_MAXDATALEN);

				ctl = IWI_CB_DEFAULT_CTL | mlen;
				sum = ctl ^ cs ^ cd;

				/* Write a command block */
				CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, ctl);
				CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, cs);
				CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, cd);
				CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, sum);

				cs += mlen;
				cd += mlen;
				len -= mlen;
			}
		}
	}

	/* Write a fictive final command block (sentinel) */
	sentinel = CSR_READ_4(sc, IWI_CSR_AUTOINC_ADDR);
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) &
	    ~(IWI_RST_MASTER_DISABLED | IWI_RST_STOP_MASTER));

	/* Tell the adapter to start processing command blocks */
	MEM_WRITE_4(sc, 0x3000a4, 0x540100);

	/* Wait until the adapter has processed all command blocks */
	for (ntries = 0; ntries < 400; ntries++) {
		if (MEM_READ_4(sc, 0x3000d0) >= sentinel)
			break;
		DELAY(100);
	}
	if (ntries == 400) {
		aprint_error_dev(sc->sc_dev, "timeout processing cb\n");
		error = ETIMEDOUT;
		goto fail3;
	}

	/* We're done with command blocks processing */
	MEM_WRITE_4(sc, 0x3000a4, 0x540c00);

	/* Allow interrupts so we know when the firmware is inited */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, IWI_INTR_MASK);

	/* Tell the adapter to initialize the firmware */
	CSR_WRITE_4(sc, IWI_CSR_RST, 0);
	CSR_WRITE_4(sc, IWI_CSR_CTL, CSR_READ_4(sc, IWI_CSR_CTL) |
	    IWI_CTL_ALLOW_STANDBY);

	/* Wait at most one second for firmware initialization to complete */
	if ((error = tsleep(sc, 0, "iwiinit", hz)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for firmware initialization to complete\n");
		goto fail3;
	}

fail3:
	bus_dmamap_sync(sc->sc_dmat, map, 0, size, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, map);
fail2:
	if (map != NULL)
		bus_dmamap_destroy(sc->sc_dmat, map);

fail1:
	return error;
}

/*
 * Store firmware into kernel memory so we can download it when we need to,
 * e.g when the adapter wakes up from suspend mode.
 */
static int
iwi_cache_firmware(struct iwi_softc *sc)
{
	struct iwi_firmware *kfw = &sc->fw;
	firmware_handle_t fwh;
	struct iwi_firmware_hdr *hdr;
	off_t size;
	char *fw;
	int error;

	if (iwi_accept_eula == 0) {
		aprint_error_dev(sc->sc_dev,
		    "EULA not accepted; please see the iwi(4) man page.\n");
		return EPERM;
	}

	iwi_free_firmware(sc);
	error = firmware_open("if_iwi", sc->sc_fwname, &fwh);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "firmware_open failed\n");
		goto fail1;
	}

	size = firmware_get_size(fwh);
	if (size < sizeof(struct iwi_firmware_hdr)) {
		aprint_error_dev(sc->sc_dev, "image '%s' has no header\n",
		    sc->sc_fwname);
		error = EIO;
		goto fail1;
	}
	sc->sc_blobsize = size;

	sc->sc_blob = firmware_malloc(size);
	if (sc->sc_blob == NULL) {
		error = ENOMEM;
		firmware_close(fwh);
		goto fail1;
	}

	error = firmware_read(fwh, 0, sc->sc_blob, size);
	firmware_close(fwh);
	if (error != 0)
		goto fail2;

	hdr = (struct iwi_firmware_hdr *)sc->sc_blob;
	hdr->version = le32toh(hdr->version);
	hdr->bsize = le32toh(hdr->bsize);
	hdr->usize = le32toh(hdr->usize);
	hdr->fsize = le32toh(hdr->fsize);

	if (size < sizeof(struct iwi_firmware_hdr) + hdr->bsize + hdr->usize + hdr->fsize) {
		aprint_error_dev(sc->sc_dev, "image '%s' too small\n",
		    sc->sc_fwname);
		error = EIO;
		goto fail2;
	}

	DPRINTF(("firmware version = %d\n", hdr->version));
	if ((IWI_FW_GET_MAJOR(hdr->version) != IWI_FW_REQ_MAJOR) ||
	    (IWI_FW_GET_MINOR(hdr->version) != IWI_FW_REQ_MINOR)) {
		aprint_error_dev(sc->sc_dev,
		    "version for '%s' %d.%d != %d.%d\n", sc->sc_fwname,
		    IWI_FW_GET_MAJOR(hdr->version),
		    IWI_FW_GET_MINOR(hdr->version),
		    IWI_FW_REQ_MAJOR, IWI_FW_REQ_MINOR);
		error = EIO;
		goto fail2;
	}

	kfw->boot_size = hdr->bsize;
	kfw->ucode_size = hdr->usize;
	kfw->main_size = hdr->fsize;

	fw = sc->sc_blob + sizeof(struct iwi_firmware_hdr);
	kfw->boot = fw;
	fw += kfw->boot_size;
	kfw->ucode = fw;
	fw += kfw->ucode_size;
	kfw->main = fw;

	DPRINTF(("Firmware cached: boot %p, ucode %p, main %p\n",
	    kfw->boot, kfw->ucode, kfw->main));
	DPRINTF(("Firmware cached: boot %u, ucode %u, main %u\n",
	    kfw->boot_size, kfw->ucode_size, kfw->main_size));

	sc->flags |= IWI_FLAG_FW_CACHED;

	return 0;


fail2:	firmware_free(sc->sc_blob, sc->sc_blobsize);
fail1:
	return error;
}

static void
iwi_free_firmware(struct iwi_softc *sc)
{

	if (!(sc->flags & IWI_FLAG_FW_CACHED))
		return;

	firmware_free(sc->sc_blob, sc->sc_blobsize);

	sc->flags &= ~IWI_FLAG_FW_CACHED;
}

static int
iwi_config(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct iwi_configuration config;
	struct iwi_rateset rs;
	struct iwi_txpower power;
	struct ieee80211_key *wk;
	struct iwi_wep_key wepkey;
	uint32_t data;
	int error, nchan, i;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	DPRINTF(("Setting MAC address to %s\n", ether_sprintf(ic->ic_myaddr)));
	error = iwi_cmd(sc, IWI_CMD_SET_MAC_ADDRESS, ic->ic_myaddr,
	    IEEE80211_ADDR_LEN, 0);
	if (error != 0)
		return error;

	memset(&config, 0, sizeof config);
	config.bluetooth_coexistence = sc->bluetooth;
	config.antenna = sc->antenna;
	config.silence_threshold = 0x1e;
	config.multicast_enabled = 1;
	config.answer_pbreq = (ic->ic_opmode == IEEE80211_M_IBSS) ? 1 : 0;
	config.disable_unicast_decryption = 1;
	config.disable_multicast_decryption = 1;
	DPRINTF(("Configuring adapter\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_CONFIGURATION, &config, sizeof config,
	    0);
	if (error != 0)
		return error;

	data = htole32(IWI_POWER_MODE_CAM);
	DPRINTF(("Setting power mode to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_POWER_MODE, &data, sizeof data, 0);
	if (error != 0)
		return error;

	data = htole32(ic->ic_rtsthreshold);
	DPRINTF(("Setting RTS threshold to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_RTS_THRESHOLD, &data, sizeof data, 0);
	if (error != 0)
		return error;

	data = htole32(ic->ic_fragthreshold);
	DPRINTF(("Setting fragmentation threshold to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_FRAG_THRESHOLD, &data, sizeof data, 0);
	if (error != 0)
		return error;

	/*
	 * Set default Tx power for 802.11b/g and 802.11a channels.
	 */
	nchan = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (!IEEE80211_IS_CHAN_2GHZ(&ic->ic_channels[i]))
			continue;
		power.chan[nchan].chan = i;
		power.chan[nchan].power = IWI_TXPOWER_MAX;
		nchan++;
	}
	power.nchan = nchan;

	power.mode = IWI_MODE_11G;
	DPRINTF(("Setting .11g channels tx power\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power, 0);
	if (error != 0)
		return error;

	power.mode = IWI_MODE_11B;
	DPRINTF(("Setting .11b channels tx power\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power, 0);
	if (error != 0)
		return error;

	nchan = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (!IEEE80211_IS_CHAN_5GHZ(&ic->ic_channels[i]))
			continue;
		power.chan[nchan].chan = i;
		power.chan[nchan].power = IWI_TXPOWER_MAX;
		nchan++;
	}
	power.nchan = nchan;

	if (nchan > 0) {	/* 2915ABG only */
		power.mode = IWI_MODE_11A;
		DPRINTF(("Setting .11a channels tx power\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power,
		    0);
		if (error != 0)
			return error;
	}

	rs.mode = IWI_MODE_11G;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates;
	memcpy(rs.rates, ic->ic_sup_rates[IEEE80211_MODE_11G].rs_rates,
	    rs.nrates);
	DPRINTF(("Setting .11bg supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 0);
	if (error != 0)
		return error;

	rs.mode = IWI_MODE_11A;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11A].rs_nrates;
	memcpy(rs.rates, ic->ic_sup_rates[IEEE80211_MODE_11A].rs_rates,
	    rs.nrates);
	DPRINTF(("Setting .11a supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 0);
	if (error != 0)
		return error;

	/* if we have a desired ESSID, set it now */
	if (ic->ic_des_esslen != 0) {
#ifdef IWI_DEBUG
		if (iwi_debug > 0) {
			printf("Setting desired ESSID to ");
			ieee80211_print_essid(ic->ic_des_essid,
			    ic->ic_des_esslen);
			printf("\n");
		}
#endif
		error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ic->ic_des_essid,
		    ic->ic_des_esslen, 0);
		if (error != 0)
			return error;
	}

	cprng_fast(&data, sizeof(data));
	data = htole32(data);
	DPRINTF(("Setting initialization vector to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_IV, &data, sizeof data, 0);
	if (error != 0)
		return error;

	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		/* XXX iwi_setwepkeys? */
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			wk = &ic->ic_crypto.cs_nw_keys[i];

			wepkey.cmd = IWI_WEP_KEY_CMD_SETKEY;
			wepkey.idx = i;
			wepkey.len = wk->wk_keylen;
			memset(wepkey.key, 0, sizeof wepkey.key);
			memcpy(wepkey.key, wk->wk_key, wk->wk_keylen);
			DPRINTF(("Setting wep key index %u len %u\n",
			    wepkey.idx, wepkey.len));
			error = iwi_cmd(sc, IWI_CMD_SET_WEP_KEY, &wepkey,
			    sizeof wepkey, 0);
			if (error != 0)
				return error;
		}
	}

	/* Enable adapter */
	DPRINTF(("Enabling adapter\n"));
	return iwi_cmd(sc, IWI_CMD_ENABLE, NULL, 0, 0);
}

static int
iwi_set_chan(struct iwi_softc *sc, struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_scan_v2 scan;

	(void)memset(&scan, 0, sizeof scan);

	scan.dwelltime[IWI_SCAN_TYPE_PASSIVE] = htole16(2000);
	scan.channels[0] = 1 |
	    (IEEE80211_IS_CHAN_5GHZ(chan) ? IWI_CHAN_5GHZ : IWI_CHAN_2GHZ);
	scan.channels[1] = ieee80211_chan2ieee(ic, chan);
	iwi_scan_type_set(scan, 1, IWI_SCAN_TYPE_PASSIVE);

	DPRINTF(("Setting channel to %u\n", ieee80211_chan2ieee(ic, chan)));
	return iwi_cmd(sc, IWI_CMD_SCAN_V2, &scan, sizeof scan, 1);
}

static int
iwi_scan(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_scan_v2 scan;
	uint32_t type;
	uint8_t *p;
	int i, count, idx;

	(void)memset(&scan, 0, sizeof scan);
	scan.dwelltime[IWI_SCAN_TYPE_ACTIVE_BROADCAST] =
	    htole16(sc->dwelltime);
	scan.dwelltime[IWI_SCAN_TYPE_ACTIVE_BDIRECT] =
	    htole16(sc->dwelltime);

	/* tell the firmware about the desired essid */
	if (ic->ic_des_esslen) {
		int error;

		DPRINTF(("%s: Setting adapter desired ESSID to %s\n",
		    __func__, ic->ic_des_essid));

		error = iwi_cmd(sc, IWI_CMD_SET_ESSID,
		    ic->ic_des_essid, ic->ic_des_esslen, 1);
		if (error)
			return error;

		type = IWI_SCAN_TYPE_ACTIVE_BDIRECT;
	} else {
		type = IWI_SCAN_TYPE_ACTIVE_BROADCAST;
	}

	p = &scan.channels[0];
	count = idx = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (IEEE80211_IS_CHAN_5GHZ(&ic->ic_channels[i]) &&
		    isset(ic->ic_chan_active, i)) {
			*++p = i;
			count++;
			idx++;
 			iwi_scan_type_set(scan, idx, type);
		}
	}
	if (count) {
		*(p - count) = IWI_CHAN_5GHZ | count;
		p++;
	}

	count = 0;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		if (IEEE80211_IS_CHAN_2GHZ(&ic->ic_channels[i]) &&
		    isset(ic->ic_chan_active, i)) {
			*++p = i;
			count++;
			idx++;
			iwi_scan_type_set(scan, idx, type);
		}
	}
	*(p - count) = IWI_CHAN_2GHZ | count;

	DPRINTF(("Start scanning\n"));
	return iwi_cmd(sc, IWI_CMD_SCAN_V2, &scan, sizeof scan, 1);
}

static int
iwi_auth_and_assoc(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211_wme_info wme;
	struct iwi_configuration config;
	struct iwi_associate assoc;
	struct iwi_rateset rs;
	uint16_t capinfo;
	uint32_t data;
	int error;

	memset(&config, 0, sizeof config);
	config.bluetooth_coexistence = sc->bluetooth;
	config.antenna = sc->antenna;
	config.multicast_enabled = 1;
	config.silence_threshold = 0x1e;
	if (ic->ic_curmode == IEEE80211_MODE_11G)
		config.use_protection = 1;
	config.answer_pbreq = (ic->ic_opmode == IEEE80211_M_IBSS) ? 1 : 0;
	config.disable_unicast_decryption = 1;
	config.disable_multicast_decryption = 1;

	DPRINTF(("Configuring adapter\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_CONFIGURATION, &config,
	    sizeof config, 1);
	if (error != 0)
		return error;

#ifdef IWI_DEBUG
	if (iwi_debug > 0) {
		aprint_debug_dev(sc->sc_dev, "Setting ESSID to ");
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		aprint_debug("\n");
	}
#endif
	error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ni->ni_essid, ni->ni_esslen, 1);
	if (error != 0)
		return error;

	/* the rate set has already been "negotiated" */
	rs.mode = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? IWI_MODE_11A :
	    IWI_MODE_11G;
	rs.type = IWI_RATESET_TYPE_NEGOTIATED;
	rs.nrates = ni->ni_rates.rs_nrates;

	if (rs.nrates > IWI_RATESET_SIZE) {
		DPRINTF(("Truncating negotiated rate set from %u\n",
		    rs.nrates));
		rs.nrates = IWI_RATESET_SIZE;
	}
	memcpy(rs.rates, ni->ni_rates.rs_rates, rs.nrates);
	DPRINTF(("Setting negotiated rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs, 1);
	if (error != 0)
		return error;

	if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL) {
		wme.wme_id = IEEE80211_ELEMID_VENDOR;
		wme.wme_len = sizeof (struct ieee80211_wme_info) - 2;
		wme.wme_oui[0] = 0x00;
		wme.wme_oui[1] = 0x50;
		wme.wme_oui[2] = 0xf2;
		wme.wme_type = WME_OUI_TYPE;
		wme.wme_subtype = WME_INFO_OUI_SUBTYPE;
		wme.wme_version = WME_VERSION;
		wme.wme_info = 0;

		DPRINTF(("Setting WME IE (len=%u)\n", wme.wme_len));
		error = iwi_cmd(sc, IWI_CMD_SET_WMEIE, &wme, sizeof wme, 1);
		if (error != 0)
			return error;
	}

	if (ic->ic_opt_ie != NULL) {
		DPRINTF(("Setting optional IE (len=%u)\n", ic->ic_opt_ie_len));
		error = iwi_cmd(sc, IWI_CMD_SET_OPTIE, ic->ic_opt_ie,
		    ic->ic_opt_ie_len, 1);
		if (error != 0)
			return error;
	}
	data = htole32(ni->ni_rssi);
	DPRINTF(("Setting sensitivity to %d\n", (int8_t)ni->ni_rssi));
	error = iwi_cmd(sc, IWI_CMD_SET_SENSITIVITY, &data, sizeof data, 1);
	if (error != 0)
		return error;

	memset(&assoc, 0, sizeof assoc);
	if (IEEE80211_IS_CHAN_A(ni->ni_chan))
		assoc.mode = IWI_MODE_11A;
	else if (IEEE80211_IS_CHAN_G(ni->ni_chan))
		assoc.mode = IWI_MODE_11G;
	else if (IEEE80211_IS_CHAN_B(ni->ni_chan))
		assoc.mode = IWI_MODE_11B;

	assoc.chan = ieee80211_chan2ieee(ic, ni->ni_chan);

	if (ni->ni_authmode == IEEE80211_AUTH_SHARED)
		assoc.auth = (ic->ic_crypto.cs_def_txkey << 4) | IWI_AUTH_SHARED;

	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		assoc.plen = IWI_ASSOC_SHPREAMBLE;

	if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL)
		assoc.policy |= htole16(IWI_POLICY_WME);
	if (ic->ic_flags & IEEE80211_F_WPA)
		assoc.policy |= htole16(IWI_POLICY_WPA);
	if (ic->ic_opmode == IEEE80211_M_IBSS && ni->ni_tstamp.tsf == 0)
		assoc.type = IWI_HC_IBSS_START;
	else
		assoc.type = IWI_HC_ASSOC;
	memcpy(assoc.tstamp, ni->ni_tstamp.data, 8);

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	assoc.capinfo = htole16(capinfo);

	assoc.lintval = htole16(ic->ic_lintval);
	assoc.intval = htole16(ni->ni_intval);
	IEEE80211_ADDR_COPY(assoc.bssid, ni->ni_bssid);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		IEEE80211_ADDR_COPY(assoc.dst, ifp->if_broadcastaddr);
	else
		IEEE80211_ADDR_COPY(assoc.dst, ni->ni_bssid);

	DPRINTF(("%s bssid %s dst %s channel %u policy 0x%x "
	    "auth %u capinfo 0x%x lintval %u bintval %u\n",
	    assoc.type == IWI_HC_IBSS_START ? "Start" : "Join",
	    ether_sprintf(assoc.bssid), ether_sprintf(assoc.dst),
	    assoc.chan, le16toh(assoc.policy), assoc.auth,
	    le16toh(assoc.capinfo), le16toh(assoc.lintval),
	    le16toh(assoc.intval)));

	return iwi_cmd(sc, IWI_CMD_ASSOCIATE, &assoc, sizeof assoc, 1);
}

static int
iwi_init(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_firmware *fw = &sc->fw;
	int i, error;

	/* exit immediately if firmware has not been ioctl'd */
	if (!(sc->flags & IWI_FLAG_FW_CACHED)) {
		if ((error = iwi_cache_firmware(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not cache the firmware\n");
			goto fail;
		}
	}

	iwi_stop(ifp, 0);

	if ((error = iwi_reset(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not reset adapter\n");
		goto fail;
	}

	if ((error = iwi_load_firmware(sc, fw->boot, fw->boot_size)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not load boot firmware\n");
		goto fail;
	}

	if ((error = iwi_load_ucode(sc, fw->ucode, fw->ucode_size)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not load microcode\n");
		goto fail;
	}

	iwi_stop_master(sc);

	CSR_WRITE_4(sc, IWI_CSR_CMD_BASE, sc->cmdq.desc_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_CMD_SIZE, sc->cmdq.count);
	CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.cur);

	CSR_WRITE_4(sc, IWI_CSR_TX1_BASE, sc->txq[0].desc_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX1_SIZE, sc->txq[0].count);
	CSR_WRITE_4(sc, IWI_CSR_TX1_WIDX, sc->txq[0].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX2_BASE, sc->txq[1].desc_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX2_SIZE, sc->txq[1].count);
	CSR_WRITE_4(sc, IWI_CSR_TX2_WIDX, sc->txq[1].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX3_BASE, sc->txq[2].desc_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX3_SIZE, sc->txq[2].count);
	CSR_WRITE_4(sc, IWI_CSR_TX3_WIDX, sc->txq[2].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX4_BASE, sc->txq[3].desc_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IWI_CSR_TX4_SIZE, sc->txq[3].count);
	CSR_WRITE_4(sc, IWI_CSR_TX4_WIDX, sc->txq[3].cur);

	for (i = 0; i < sc->rxq.count; i++)
		CSR_WRITE_4(sc, IWI_CSR_RX_BASE + i * 4,
		    sc->rxq.data[i].map->dm_segs[0].ds_addr);

	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, sc->rxq.count -1);

	if ((error = iwi_load_firmware(sc, fw->main, fw->main_size)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not load main firmware\n");
		goto fail;
	}

	sc->flags |= IWI_FLAG_FW_INITED;

	if ((error = iwi_config(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "device configuration failed\n");
		goto fail;
	}

	ic->ic_state = IEEE80211_S_INIT;

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	return 0;

fail:	ifp->if_flags &= ~IFF_UP;
	iwi_stop(ifp, 0);

	return error;
}


/*
 * Return whether or not the radio is enabled in hardware
 * (i.e. the rfkill switch is "off").
 */
static int
iwi_getrfkill(struct iwi_softc *sc)
{
	return (CSR_READ_4(sc, IWI_CSR_IO) & IWI_IO_RADIO_ENABLED) == 0;
}

static int
iwi_sysctl_radio(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct iwi_softc *sc;
	int val, error;

	node = *rnode;
	sc = (struct iwi_softc *)node.sysctl_data;

	val = !iwi_getrfkill(sc);

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	return 0;
}

#ifdef IWI_DEBUG
SYSCTL_SETUP(sysctl_iwi, "sysctl iwi(4) subtree setup")
{
	int rc;
	const struct sysctlnode *rnode;
	const struct sysctlnode *cnode;

	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "iwi",
	    SYSCTL_DESCR("iwi global controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* control debugging printfs */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "debug", SYSCTL_DESCR("Enable debugging output"),
	    NULL, 0, &iwi_debug, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return;
err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

#endif /* IWI_DEBUG */

/*
 * Add sysctl knobs.
 */
static void
iwi_sysctlattach(struct iwi_softc *sc)
{
	int rc;
	const struct sysctlnode *rnode;
	const struct sysctlnode *cnode;

	struct sysctllog **clog = &sc->sc_sysctllog;

	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, device_xname(sc->sc_dev),
	    SYSCTL_DESCR("iwi controls and statistics"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT, CTLTYPE_INT, "radio",
	    SYSCTL_DESCR("radio transmitter switch state (0=off, 1=on)"),
	    iwi_sysctl_radio, 0, (void *)sc, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	sc->dwelltime = 100;
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "dwell", SYSCTL_DESCR("channel dwell time (ms) for AP/station scanning"),
	    NULL, 0, &sc->dwelltime, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	sc->bluetooth = 0;
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "bluetooth", SYSCTL_DESCR("bluetooth coexistence"),
	    NULL, 0, &sc->bluetooth, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	sc->antenna = IWI_ANTENNA_AUTO;
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "antenna", SYSCTL_DESCR("antenna (0=auto)"),
	    NULL, 0, &sc->antenna, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return;
err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static void
iwi_stop(struct ifnet *ifp, int disable)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	IWI_LED_OFF(sc);

	iwi_stop_master(sc);
	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_SW_RESET);

	/* reset rings */
	iwi_reset_cmd_ring(sc, &sc->cmdq);
	iwi_reset_tx_ring(sc, &sc->txq[0]);
	iwi_reset_tx_ring(sc, &sc->txq[1]);
	iwi_reset_tx_ring(sc, &sc->txq[2]);
	iwi_reset_tx_ring(sc, &sc->txq[3]);
	iwi_reset_rx_ring(sc, &sc->rxq);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
}

static void
iwi_led_set(struct iwi_softc *sc, uint32_t state, int toggle)
{
	uint32_t val;

	val = MEM_READ_4(sc, IWI_MEM_EVENT_CTL);

	switch (sc->nictype) {
	case 1:
		/* special NIC type: reversed leds */
		if (state == IWI_LED_ACTIVITY) {
			state &= ~IWI_LED_ACTIVITY;
			state |= IWI_LED_ASSOCIATED;
		} else if (state == IWI_LED_ASSOCIATED) {
			state &= ~IWI_LED_ASSOCIATED;
			state |= IWI_LED_ACTIVITY;
		}
		/* and ignore toggle effect */
		val |= state;
		break;
	case 0:
	case 2:
	case 3:
	case 4:
		val = (toggle && (val & state)) ? val & ~state : val | state;
		break;
	default:
		aprint_normal_dev(sc->sc_dev, "unknown NIC type %d\n",
		    sc->nictype);
		return;
		break;
	}

	MEM_WRITE_4(sc, IWI_MEM_EVENT_CTL, val);

	return;
}

SYSCTL_SETUP(sysctl_hw_iwi_accept_eula_setup, "sysctl hw.iwi.accept_eula")
{
	const struct sysctlnode *rnode;
	const struct sysctlnode *cnode;

	sysctl_createv(NULL, 0, NULL, &rnode,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "iwi",
		NULL,
		NULL, 0,
		NULL, 0,
		CTL_HW, CTL_CREATE, CTL_EOL);

	sysctl_createv(NULL, 0, &rnode, &cnode,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "accept_eula",
		SYSCTL_DESCR("Accept Intel EULA and permit use of iwi(4) firmware"),
		NULL, 0,
		&iwi_accept_eula, sizeof(iwi_accept_eula),
		CTL_CREATE, CTL_EOL);
}
