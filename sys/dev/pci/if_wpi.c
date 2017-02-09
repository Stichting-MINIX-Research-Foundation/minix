/*	$NetBSD: if_wpi.c,v 1.71 2015/01/09 15:25:23 bouyer Exp $	*/

/*-
 * Copyright (c) 2006, 2007
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_wpi.c,v 1.71 2015/01/09 15:25:23 bouyer Exp $");

/*
 * Driver for Intel PRO/Wireless 3945ABG 802.11 network adapters.
 */


#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/once.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <sys/bus.h>
#include <machine/endian.h>
#include <sys/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/sysmon/sysmonvar.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/pci/if_wpireg.h>
#include <dev/pci/if_wpivar.h>

static const char wpi_firmware_name[] = "iwlwifi-3945.ucode";
static once_t wpi_firmware_init;
static kmutex_t wpi_firmware_mutex;
static size_t wpi_firmware_users;
static uint8_t *wpi_firmware_image;
static size_t wpi_firmware_size;

static int	wpi_match(device_t, cfdata_t, void *);
static void	wpi_attach(device_t, device_t, void *);
static int	wpi_detach(device_t , int);
static int	wpi_dma_contig_alloc(bus_dma_tag_t, struct wpi_dma_info *,
		    void **, bus_size_t, bus_size_t, int);
static void	wpi_dma_contig_free(struct wpi_dma_info *);
static int	wpi_alloc_shared(struct wpi_softc *);
static void	wpi_free_shared(struct wpi_softc *);
static int	wpi_alloc_fwmem(struct wpi_softc *);
static void	wpi_free_fwmem(struct wpi_softc *);
static struct	wpi_rbuf *wpi_alloc_rbuf(struct wpi_softc *);
static void	wpi_free_rbuf(struct mbuf *, void *, size_t, void *);
static int	wpi_alloc_rpool(struct wpi_softc *);
static void	wpi_free_rpool(struct wpi_softc *);
static int	wpi_alloc_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
static void	wpi_reset_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
static void	wpi_free_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
static int	wpi_alloc_tx_ring(struct wpi_softc *, struct wpi_tx_ring *,
		    int, int);
static void	wpi_reset_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
static void	wpi_free_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
static struct	ieee80211_node * wpi_node_alloc(struct ieee80211_node_table *);
static void	wpi_newassoc(struct ieee80211_node *, int);
static int	wpi_media_change(struct ifnet *);
static int	wpi_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void	wpi_mem_lock(struct wpi_softc *);
static void	wpi_mem_unlock(struct wpi_softc *);
static uint32_t	wpi_mem_read(struct wpi_softc *, uint16_t);
static void	wpi_mem_write(struct wpi_softc *, uint16_t, uint32_t);
static void	wpi_mem_write_region_4(struct wpi_softc *, uint16_t,
		    const uint32_t *, int);
static int	wpi_read_prom_data(struct wpi_softc *, uint32_t, void *, int);
static int	wpi_load_microcode(struct wpi_softc *,  const uint8_t *, int);
static int	wpi_cache_firmware(struct wpi_softc *);
static void	wpi_release_firmware(void);
static int	wpi_load_firmware(struct wpi_softc *);
static void	wpi_calib_timeout(void *);
static void	wpi_iter_func(void *, struct ieee80211_node *);
static void	wpi_power_calibration(struct wpi_softc *, int);
static void	wpi_rx_intr(struct wpi_softc *, struct wpi_rx_desc *,
		    struct wpi_rx_data *);
static void	wpi_tx_intr(struct wpi_softc *, struct wpi_rx_desc *);
static void	wpi_cmd_intr(struct wpi_softc *, struct wpi_rx_desc *);
static void	wpi_notif_intr(struct wpi_softc *);
static int	wpi_intr(void *);
static void	wpi_read_eeprom(struct wpi_softc *);
static void	wpi_read_eeprom_channels(struct wpi_softc *, int);
static void	wpi_read_eeprom_group(struct wpi_softc *, int);
static uint8_t	wpi_plcp_signal(int);
static int	wpi_tx_data(struct wpi_softc *, struct mbuf *,
		    struct ieee80211_node *, int);
static void	wpi_start(struct ifnet *);
static void	wpi_watchdog(struct ifnet *);
static int	wpi_ioctl(struct ifnet *, u_long, void *);
static int	wpi_cmd(struct wpi_softc *, int, const void *, int, int);
static int	wpi_wme_update(struct ieee80211com *);
static int	wpi_mrr_setup(struct wpi_softc *);
static void	wpi_set_led(struct wpi_softc *, uint8_t, uint8_t, uint8_t);
static void	wpi_enable_tsf(struct wpi_softc *, struct ieee80211_node *);
static int	wpi_set_txpower(struct wpi_softc *,
		    struct ieee80211_channel *, int);
static int	wpi_get_power_index(struct wpi_softc *,
		    struct wpi_power_group *, struct ieee80211_channel *, int);
static int	wpi_setup_beacon(struct wpi_softc *, struct ieee80211_node *);
static int	wpi_auth(struct wpi_softc *);
static int	wpi_scan(struct wpi_softc *);
static int	wpi_config(struct wpi_softc *);
static void	wpi_stop_master(struct wpi_softc *);
static int	wpi_power_up(struct wpi_softc *);
static int	wpi_reset(struct wpi_softc *);
static void	wpi_hw_config(struct wpi_softc *);
static int	wpi_init(struct ifnet *);
static void	wpi_stop(struct ifnet *, int);
static bool	wpi_resume(device_t, const pmf_qual_t *);
static int	wpi_getrfkill(struct wpi_softc *);
static void	wpi_sysctlattach(struct wpi_softc *);
static void	wpi_rsw_thread(void *);

#ifdef WPI_DEBUG
#define DPRINTF(x)	do { if (wpi_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (wpi_debug >= (n)) printf x; } while (0)
int wpi_debug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

CFATTACH_DECL_NEW(wpi, sizeof (struct wpi_softc), wpi_match, wpi_attach,
	wpi_detach, NULL);

static int
wpi_match(device_t parent, cfdata_t match __unused, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return 0;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_3945ABG_1 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_3945ABG_2)
		return 1;

	return 0;
}

/* Base Address Register */
#define WPI_PCI_BAR0	0x10

static int
wpi_attach_once(void)
{

	mutex_init(&wpi_firmware_mutex, MUTEX_DEFAULT, IPL_NONE);
	return 0;
}

static void
wpi_attach(device_t parent __unused, device_t self, void *aux)
{
	struct wpi_softc *sc = device_private(self);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	pci_intr_handle_t ih;
	pcireg_t data;
	int ac, error;
	char intrbuf[PCI_INTRSTR_LEN];

	RUN_ONCE(&wpi_firmware_init, wpi_attach_once);
	sc->fw_used = false;

	sc->sc_dev = self;
	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	sc->sc_rsw_status = WPI_RSW_UNKNOWN;
	sc->sc_rsw.smpsw_name = device_xname(self);
	sc->sc_rsw.smpsw_type = PSWITCH_TYPE_RADIO;
	error = sysmon_pswitch_register(&sc->sc_rsw);
	if (error) {
		aprint_error_dev(self,
		    "unable to register radio switch with sysmon\n");
		return;
	}
	mutex_init(&sc->sc_rsw_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_rsw_cv, "wpirsw");
	if (kthread_create(PRI_NONE, 0, NULL,
	    wpi_rsw_thread, sc, &sc->sc_rsw_lwp, "%s", device_xname(self))) {
		aprint_error_dev(self, "couldn't create switch thread\n");
	}

	callout_init(&sc->calib_to, 0);
	callout_setfunc(&sc->calib_to, wpi_calib_timeout, sc);

	pci_aprint_devinfo(pa, NULL);

	/* enable bus-mastering */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG, data);

	/* map the register window */
	error = pci_mapreg_map(pa, WPI_PCI_BAR0, PCI_MAPREG_TYPE_MEM |
	    PCI_MAPREG_MEM_TYPE_32BIT, 0, &memt, &memh, NULL, &sc->sc_sz);
	if (error != 0) {
		aprint_error_dev(self, "could not map memory space\n");
		return;
	}

	sc->sc_st = memt;
	sc->sc_sh = memh;
	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "could not map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, wpi_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "could not establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/*
	 * Put adapter into a known state.
	 */
	if ((error = wpi_reset(sc)) != 0) {
		aprint_error_dev(self, "could not reset adapter\n");
		return;
	}

	/*
	 * Allocate DMA memory for firmware transfers.
	 */
	if ((error = wpi_alloc_fwmem(sc)) != 0) {
		aprint_error_dev(self, "could not allocate firmware memory\n");
		return;
	}

	/*
	 * Allocate shared page and Tx/Rx rings.
	 */
	if ((error = wpi_alloc_shared(sc)) != 0) {
		aprint_error_dev(self, "could not allocate shared area\n");
		goto fail1;
	}

	if ((error = wpi_alloc_rpool(sc)) != 0) {
		aprint_error_dev(self, "could not allocate Rx buffers\n");
		goto fail2;
	}

	for (ac = 0; ac < 4; ac++) {
		error = wpi_alloc_tx_ring(sc, &sc->txq[ac], WPI_TX_RING_COUNT,
		    ac);
		if (error != 0) {
			aprint_error_dev(self,
			    "could not allocate Tx ring %d\n", ac);
			goto fail3;
		}
	}

	error = wpi_alloc_tx_ring(sc, &sc->cmdq, WPI_CMD_RING_COUNT, 4);
	if (error != 0) {
		aprint_error_dev(self, "could not allocate command ring\n");
		goto fail3;
	}

	error = wpi_alloc_rx_ring(sc, &sc->rxq);
	if (error != 0) {
		aprint_error_dev(self, "could not allocate Rx ring\n");
		goto fail4;
	}

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_WPA |		/* 802.11i */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_WME;		/* 802.11e */

	/* read supported channels and MAC address from EEPROM */
	wpi_read_eeprom(sc);

	/* set supported .11a, .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11A] = ieee80211_std_rateset_11a;
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* IBSS channel undefined for now */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = wpi_init;
	ifp->if_stop = wpi_stop;
	ifp->if_ioctl = wpi_ioctl;
	ifp->if_start = wpi_start;
	ifp->if_watchdog = wpi_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ic);
	/* override default methods */
	ic->ic_node_alloc = wpi_node_alloc;
	ic->ic_newassoc = wpi_newassoc;
	ic->ic_wme.wme_update = wpi_wme_update;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = wpi_newstate;
	ieee80211_media_init(ic, wpi_media_change, ieee80211_media_status);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 15;

	wpi_sysctlattach(sc);

	if (pmf_device_register(self, NULL, wpi_resume))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN,
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(WPI_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(WPI_TX_RADIOTAP_PRESENT);

	ieee80211_announce(ic);

	return;

	/* free allocated memory if something failed during attachment */
fail4:	wpi_free_tx_ring(sc, &sc->cmdq);
fail3:	while (--ac >= 0)
		wpi_free_tx_ring(sc, &sc->txq[ac]);
	wpi_free_rpool(sc);
fail2:	wpi_free_shared(sc);
fail1:	wpi_free_fwmem(sc);
}

static int
wpi_detach(device_t self, int flags __unused)
{
	struct wpi_softc *sc = device_private(self);
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	int ac;

	wpi_stop(ifp, 1);

	if (ifp != NULL)
		bpf_detach(ifp);
	ieee80211_ifdetach(&sc->sc_ic);
	if (ifp != NULL)
		if_detach(ifp);

	for (ac = 0; ac < 4; ac++)
		wpi_free_tx_ring(sc, &sc->txq[ac]);
	wpi_free_tx_ring(sc, &sc->cmdq);
	wpi_free_rx_ring(sc, &sc->rxq);
	wpi_free_rpool(sc);
	wpi_free_shared(sc);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	mutex_enter(&sc->sc_rsw_mtx);
	sc->sc_dying = 1;
	cv_signal(&sc->sc_rsw_cv);
	while (sc->sc_rsw_lwp != NULL)
		cv_wait(&sc->sc_rsw_cv, &sc->sc_rsw_mtx);
	mutex_exit(&sc->sc_rsw_mtx);
	sysmon_pswitch_unregister(&sc->sc_rsw);

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);

	if (sc->fw_used) {
		sc->fw_used = false;
		wpi_release_firmware();
	}
	cv_destroy(&sc->sc_rsw_cv);
	mutex_destroy(&sc->sc_rsw_mtx);
	return 0;
}

static int
wpi_dma_contig_alloc(bus_dma_tag_t tag, struct wpi_dma_info *dma, void **kvap,
    bus_size_t size, bus_size_t alignment, int flags)
{
	int nsegs, error;

	dma->tag = tag;
	dma->size = size;

	error = bus_dmamap_create(tag, size, 1, size, 0, flags, &dma->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(tag, size, alignment, 0, &dma->seg, 1, &nsegs,
	    flags);
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(tag, &dma->seg, 1, size, &dma->vaddr, flags);
	if (error != 0)
		goto fail;

	error = bus_dmamap_load(tag, dma->map, dma->vaddr, size, NULL, flags);
	if (error != 0)
		goto fail;

	memset(dma->vaddr, 0, size);
	bus_dmamap_sync(dma->tag, dma->map, 0, size, BUS_DMASYNC_PREWRITE);

	dma->paddr = dma->map->dm_segs[0].ds_addr;
	if (kvap != NULL)
		*kvap = dma->vaddr;

	return 0;

fail:	wpi_dma_contig_free(dma);
	return error;
}

static void
wpi_dma_contig_free(struct wpi_dma_info *dma)
{
	if (dma->map != NULL) {
		if (dma->vaddr != NULL) {
			bus_dmamap_unload(dma->tag, dma->map);
			bus_dmamem_unmap(dma->tag, dma->vaddr, dma->size);
			bus_dmamem_free(dma->tag, &dma->seg, 1);
			dma->vaddr = NULL;
		}
		bus_dmamap_destroy(dma->tag, dma->map);
		dma->map = NULL;
	}
}

/*
 * Allocate a shared page between host and NIC.
 */
static int
wpi_alloc_shared(struct wpi_softc *sc)
{
	int error;

	/* must be aligned on a 4K-page boundary */
	error = wpi_dma_contig_alloc(sc->sc_dmat, &sc->shared_dma,
	    (void **)&sc->shared, sizeof (struct wpi_shared), WPI_BUF_ALIGN,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		aprint_error_dev(sc->sc_dev,
		    "could not allocate shared area DMA memory\n");

	return error;
}

static void
wpi_free_shared(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->shared_dma);
}

/*
 * Allocate DMA-safe memory for firmware transfer.
 */
static int
wpi_alloc_fwmem(struct wpi_softc *sc)
{
	int error;

	/* allocate enough contiguous space to store text and data */
	error = wpi_dma_contig_alloc(sc->sc_dmat, &sc->fw_dma, NULL,
	    WPI_FW_MAIN_TEXT_MAXSZ + WPI_FW_MAIN_DATA_MAXSZ, 0,
	    BUS_DMA_NOWAIT);

	if (error != 0)
		aprint_error_dev(sc->sc_dev,
		    "could not allocate firmware transfer area DMA memory\n");
	return error;
}

static void
wpi_free_fwmem(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->fw_dma);
}

static struct wpi_rbuf *
wpi_alloc_rbuf(struct wpi_softc *sc)
{
	struct wpi_rbuf *rbuf;

	mutex_enter(&sc->rxq.freelist_mtx);
	rbuf = SLIST_FIRST(&sc->rxq.freelist);
	if (rbuf != NULL) {
		SLIST_REMOVE_HEAD(&sc->rxq.freelist, next);
	}
	mutex_exit(&sc->rxq.freelist_mtx);

	return rbuf;
}

/*
 * This is called automatically by the network stack when the mbuf to which our
 * Rx buffer is attached is freed.
 */
static void
wpi_free_rbuf(struct mbuf* m, void *buf, size_t size, void *arg)
{
	struct wpi_rbuf *rbuf = arg;
	struct wpi_softc *sc = rbuf->sc;

	/* put the buffer back in the free list */

	mutex_enter(&sc->rxq.freelist_mtx);
	SLIST_INSERT_HEAD(&sc->rxq.freelist, rbuf, next);
	mutex_exit(&sc->rxq.freelist_mtx);

	if (__predict_true(m != NULL))
		pool_cache_put(mb_cache, m);
}

static int
wpi_alloc_rpool(struct wpi_softc *sc)
{
	struct wpi_rx_ring *ring = &sc->rxq;
	int i, error;

	/* allocate a big chunk of DMA'able memory.. */
	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->buf_dma, NULL,
	    WPI_RBUF_COUNT * WPI_RBUF_SIZE, WPI_BUF_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_normal_dev(sc->sc_dev,
		    "could not allocate Rx buffers DMA memory\n");
		return error;
	}

	/* ..and split it into 3KB chunks */
	mutex_init(&ring->freelist_mtx, MUTEX_DEFAULT, IPL_NET);
	SLIST_INIT(&ring->freelist);
	for (i = 0; i < WPI_RBUF_COUNT; i++) {
		struct wpi_rbuf *rbuf = &ring->rbuf[i];

		rbuf->sc = sc;	/* backpointer for callbacks */
		rbuf->vaddr = (char *)ring->buf_dma.vaddr + i * WPI_RBUF_SIZE;
		rbuf->paddr = ring->buf_dma.paddr + i * WPI_RBUF_SIZE;

		SLIST_INSERT_HEAD(&ring->freelist, rbuf, next);
	}

	return 0;
}

static void
wpi_free_rpool(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->rxq.buf_dma);
}

static int
wpi_alloc_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	bus_size_t size;
	int i, error;

	ring->cur = 0;

	size = WPI_RX_RING_COUNT * sizeof (uint32_t);
	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, size,
	    WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate rx ring DMA memory\n");
		goto fail;
	}

	/*
	 * Setup Rx buffers.
	 */
	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		struct wpi_rx_data *data = &ring->data[i];
		struct wpi_rbuf *rbuf;

		error = bus_dmamap_create(sc->sc_dmat, WPI_RBUF_SIZE, 1,
		    WPI_RBUF_SIZE, 0, BUS_DMA_NOWAIT, &data->map);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate rx dma map\n");
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}
		if ((rbuf = wpi_alloc_rbuf(sc)) == NULL) {
			m_freem(data->m);
			data->m = NULL;
			aprint_error_dev(sc->sc_dev,
			    "could not allocate rx cluster\n");
			error = ENOMEM;
			goto fail;
		}
		/* attach Rx buffer to mbuf */
		MEXTADD(data->m, rbuf->vaddr, WPI_RBUF_SIZE, 0, wpi_free_rbuf,
		    rbuf);
		data->m->m_flags |= M_EXT_RW;

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), WPI_RBUF_SIZE, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "could not load mbuf: %d\n", error);
			goto fail;
		}

		ring->desc[i] = htole32(rbuf->paddr);
	}

	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0, size,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	wpi_free_rx_ring(sc, ring);
	return error;
}

static void
wpi_reset_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	int ntries;

	wpi_mem_lock(sc);

	WPI_WRITE(sc, WPI_RX_CONFIG, 0);
	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_RX_STATUS) & WPI_RX_IDLE)
			break;
		DELAY(10);
	}
#ifdef WPI_DEBUG
	if (ntries == 100 && wpi_debug > 0)
		aprint_error_dev(sc->sc_dev, "timeout resetting Rx ring\n");
#endif
	wpi_mem_unlock(sc);

	ring->cur = 0;
}

static void
wpi_free_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	int i;

	wpi_dma_contig_free(&ring->desc_dma);

	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		if (ring->data[i].m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, ring->data[i].map);
			m_freem(ring->data[i].m);
		}
		if (ring->data[i].map != NULL) {
			bus_dmamap_destroy(sc->sc_dmat, ring->data[i].map);
		}
	}
}

static int
wpi_alloc_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring, int count,
    int qid)
{
	int i, error;

	ring->qid = qid;
	ring->count = count;
	ring->queued = 0;
	ring->cur = 0;

	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, count * sizeof (struct wpi_tx_desc),
	    WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate tx ring DMA memory\n");
		goto fail;
	}

	/* update shared page with ring's base address */
	sc->shared->txbase[qid] = htole32(ring->desc_dma.paddr);
	bus_dmamap_sync(sc->sc_dmat, sc->shared_dma.map, 0,
	    sizeof(struct wpi_shared), BUS_DMASYNC_PREWRITE);

	error = wpi_dma_contig_alloc(sc->sc_dmat, &ring->cmd_dma,
	    (void **)&ring->cmd, count * sizeof (struct wpi_tx_cmd), 4,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate tx cmd DMA memory\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct wpi_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate tx data slots\n");
		goto fail;
	}

	for (i = 0; i < count; i++) {
		struct wpi_tx_data *data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    WPI_MAX_SCATTER - 1, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &data->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create tx buf DMA map\n");
			goto fail;
		}
	}

	return 0;

fail:	wpi_free_tx_ring(sc, ring);
	return error;
}

static void
wpi_reset_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring)
{
	int i, ntries;

	wpi_mem_lock(sc);

	WPI_WRITE(sc, WPI_TX_CONFIG(ring->qid), 0);
	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_TX_STATUS) & WPI_TX_IDLE(ring->qid))
			break;
		DELAY(10);
	}
#ifdef WPI_DEBUG
	if (ntries == 100 && wpi_debug > 0) {
		aprint_error_dev(sc->sc_dev, "timeout resetting Tx ring %d\n",
		    ring->qid);
	}
#endif
	wpi_mem_unlock(sc);

	for (i = 0; i < ring->count; i++) {
		struct wpi_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = 0;
}

static void
wpi_free_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring)
{
	int i;

	wpi_dma_contig_free(&ring->desc_dma);
	wpi_dma_contig_free(&ring->cmd_dma);

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			struct wpi_tx_data *data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}
		}
		free(ring->data, M_DEVBUF);
	}
}

/*ARGUSED*/
static struct ieee80211_node *
wpi_node_alloc(struct ieee80211_node_table *nt __unused)
{
	struct wpi_node *wn;

	wn = malloc(sizeof (struct wpi_node), M_80211_NODE, M_NOWAIT | M_ZERO);

	return (struct ieee80211_node *)wn;
}

static void
wpi_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct wpi_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int i;

	ieee80211_amrr_node_init(&sc->amrr, &((struct wpi_node *)ni)->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;
}

static int
wpi_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		wpi_init(ifp);

	return 0;
}

static int
wpi_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate = ic->ic_state;
	int error;

	callout_stop(&sc->calib_to);

	switch (nstate) {
	case IEEE80211_S_SCAN:
	
		if (sc->is_scanning)
			break;

		sc->is_scanning = true;

		if (ostate != IEEE80211_S_SCAN) {
			/* make the link LED blink while we're scanning */
			wpi_set_led(sc, WPI_LED_LINK, 20, 2);
		}

		if ((error = wpi_scan(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not initiate scan\n");
			return error;
		}
		break;

	case IEEE80211_S_ASSOC:
		if (ic->ic_state != IEEE80211_S_RUN)
			break;
		/* FALLTHROUGH */
	case IEEE80211_S_AUTH:
		/* reset state to handle reassociations correctly */
		sc->config.associd = 0;
		sc->config.filter &= ~htole32(WPI_FILTER_BSS);

		if ((error = wpi_auth(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not send authentication request\n");
			return error;
		}
		break;

	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_MONITOR) {
			/* link LED blinks while monitoring */
			wpi_set_led(sc, WPI_LED_LINK, 5, 5);
			break;
		}
		ni = ic->ic_bss;

		if (ic->ic_opmode != IEEE80211_M_STA) {
			(void) wpi_auth(sc);    /* XXX */
			wpi_setup_beacon(sc, ni);
		}

		wpi_enable_tsf(sc, ni);

		/* update adapter's configuration */
		sc->config.associd = htole16(ni->ni_associd & ~0xc000);
		/* short preamble/slot time are negotiated when associating */
		sc->config.flags &= ~htole32(WPI_CONFIG_SHPREAMBLE |
		    WPI_CONFIG_SHSLOT);
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			sc->config.flags |= htole32(WPI_CONFIG_SHSLOT);
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			sc->config.flags |= htole32(WPI_CONFIG_SHPREAMBLE);
		sc->config.filter |= htole32(WPI_FILTER_BSS);
		if (ic->ic_opmode != IEEE80211_M_STA)
			sc->config.filter |= htole32(WPI_FILTER_BEACON);

/* XXX put somewhere HC_QOS_SUPPORT_ASSOC + HC_IBSS_START */

		DPRINTF(("config chan %d flags %x\n", sc->config.chan,
		    sc->config.flags));
		error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
		    sizeof (struct wpi_config), 1);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not update configuration\n");
			return error;
		}

		/* configuration has changed, set Tx power accordingly */
		if ((error = wpi_set_txpower(sc, ic->ic_curchan, 1)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not set Tx power\n");
			return error;
		}

		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* fake a join to init the tx rate */
			wpi_newassoc(ni, 1);
		}

		/* start periodic calibration timer */
		sc->calib_cnt = 0;
		callout_schedule(&sc->calib_to, hz/2);

		/* link LED always on while associated */
		wpi_set_led(sc, WPI_LED_LINK, 0, 1);
		break;

	case IEEE80211_S_INIT:
		sc->is_scanning = false;
		break;
	}

	return sc->sc_newstate(ic, nstate, arg);
}

/*
 * Grab exclusive access to NIC memory.
 */
static void
wpi_mem_lock(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp | WPI_GPIO_MAC);

	/* spin until we actually get the lock */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((WPI_READ(sc, WPI_GPIO_CTL) &
		    (WPI_GPIO_CLOCK | WPI_GPIO_SLEEP)) == WPI_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 1000)
		aprint_error_dev(sc->sc_dev, "could not lock memory\n");
}

/*
 * Release lock on NIC memory.
 */
static void
wpi_mem_unlock(struct wpi_softc *sc)
{
	uint32_t tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp & ~WPI_GPIO_MAC);
}

static uint32_t
wpi_mem_read(struct wpi_softc *sc, uint16_t addr)
{
	WPI_WRITE(sc, WPI_READ_MEM_ADDR, WPI_MEM_4 | addr);
	return WPI_READ(sc, WPI_READ_MEM_DATA);
}

static void
wpi_mem_write(struct wpi_softc *sc, uint16_t addr, uint32_t data)
{
	WPI_WRITE(sc, WPI_WRITE_MEM_ADDR, WPI_MEM_4 | addr);
	WPI_WRITE(sc, WPI_WRITE_MEM_DATA, data);
}

static void
wpi_mem_write_region_4(struct wpi_softc *sc, uint16_t addr,
    const uint32_t *data, int wlen)
{
	for (; wlen > 0; wlen--, data++, addr += 4)
		wpi_mem_write(sc, addr, *data);
}

/*
 * Read `len' bytes from the EEPROM.  We access the EEPROM through the MAC
 * instead of using the traditional bit-bang method.
 */
static int
wpi_read_prom_data(struct wpi_softc *sc, uint32_t addr, void *data, int len)
{
	uint8_t *out = data;
	uint32_t val;
	int ntries;

	wpi_mem_lock(sc);
	for (; len > 0; len -= 2, addr++) {
		WPI_WRITE(sc, WPI_EEPROM_CTL, addr << 2);

		for (ntries = 0; ntries < 10; ntries++) {
			if ((val = WPI_READ(sc, WPI_EEPROM_CTL)) &
			    WPI_EEPROM_READY)
				break;
			DELAY(5);
		}
		if (ntries == 10) {
			aprint_error_dev(sc->sc_dev, "could not read EEPROM\n");
			return ETIMEDOUT;
		}
		*out++ = val >> 16;
		if (len > 1)
			*out++ = val >> 24;
	}
	wpi_mem_unlock(sc);

	return 0;
}

/*
 * The firmware boot code is small and is intended to be copied directly into
 * the NIC internal memory.
 */
int
wpi_load_microcode(struct wpi_softc *sc, const uint8_t *ucode, int size)
{
	int ntries;

	size /= sizeof (uint32_t);

	wpi_mem_lock(sc);

	/* copy microcode image into NIC memory */
	wpi_mem_write_region_4(sc, WPI_MEM_UCODE_BASE,
	    (const uint32_t *)ucode, size);

	wpi_mem_write(sc, WPI_MEM_UCODE_SRC, 0);
	wpi_mem_write(sc, WPI_MEM_UCODE_DST, WPI_FW_TEXT);
	wpi_mem_write(sc, WPI_MEM_UCODE_SIZE, size);

	/* run microcode */
	wpi_mem_write(sc, WPI_MEM_UCODE_CTL, WPI_UC_RUN);

	/* wait for transfer to complete */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(wpi_mem_read(sc, WPI_MEM_UCODE_CTL) & WPI_UC_RUN))
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		wpi_mem_unlock(sc);
		aprint_error_dev(sc->sc_dev, "could not load boot firmware\n");
		return ETIMEDOUT;
	}
	wpi_mem_write(sc, WPI_MEM_UCODE_CTL, WPI_UC_ENABLE);

	wpi_mem_unlock(sc);

	return 0;
}

static int
wpi_cache_firmware(struct wpi_softc *sc)
{
	const char *const fwname = wpi_firmware_name;
	firmware_handle_t fw;
	int error;

	/* sc is used here only to report error messages.  */

	mutex_enter(&wpi_firmware_mutex);

	if (wpi_firmware_users == SIZE_MAX) {
		mutex_exit(&wpi_firmware_mutex);
		return ENFILE;	/* Too many of something in the system...  */
	}
	if (wpi_firmware_users++) {
		KASSERT(wpi_firmware_image != NULL);
		KASSERT(wpi_firmware_size > 0);
		mutex_exit(&wpi_firmware_mutex);
		return 0;	/* Already good to go.  */
	}

	KASSERT(wpi_firmware_image == NULL);
	KASSERT(wpi_firmware_size == 0);

	/* load firmware image from disk */
	if ((error = firmware_open("if_wpi", fwname, &fw)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not open firmware file %s: %d\n", fwname, error);
		goto fail0;
	}

	wpi_firmware_size = firmware_get_size(fw);

	if (wpi_firmware_size > sizeof (struct wpi_firmware_hdr) +
	    WPI_FW_MAIN_TEXT_MAXSZ + WPI_FW_MAIN_DATA_MAXSZ +
	    WPI_FW_INIT_TEXT_MAXSZ + WPI_FW_INIT_DATA_MAXSZ +
	    WPI_FW_BOOT_TEXT_MAXSZ) {
		aprint_error_dev(sc->sc_dev,
		    "firmware file %s too large: %zu bytes\n",
		    fwname, wpi_firmware_size);
		error = EFBIG;
		goto fail1;
	}

	if (wpi_firmware_size < sizeof (struct wpi_firmware_hdr)) {
		aprint_error_dev(sc->sc_dev,
		    "firmware file %s too small: %zu bytes\n",
		    fwname, wpi_firmware_size);
		error = EINVAL;
		goto fail1;
	}

	wpi_firmware_image = firmware_malloc(wpi_firmware_size);
	if (wpi_firmware_image == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "not enough memory for firmware file %s\n", fwname);
		error = ENOMEM;
		goto fail1;
	}

	error = firmware_read(fw, 0, wpi_firmware_image, wpi_firmware_size);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "error reading firmware file %s: %d\n", fwname, error);
		goto fail2;
	}

	/* Success!  */
	firmware_close(fw);
	mutex_exit(&wpi_firmware_mutex);
	return 0;

fail2:
	firmware_free(wpi_firmware_image, wpi_firmware_size);
	wpi_firmware_image = NULL;
fail1:
	wpi_firmware_size = 0;
	firmware_close(fw);
fail0:
	KASSERT(wpi_firmware_users == 1);
	wpi_firmware_users = 0;
	KASSERT(wpi_firmware_image == NULL);
	KASSERT(wpi_firmware_size == 0);

	mutex_exit(&wpi_firmware_mutex);
	return error;
}

static void
wpi_release_firmware(void)
{

	mutex_enter(&wpi_firmware_mutex);

	KASSERT(wpi_firmware_users > 0);
	KASSERT(wpi_firmware_image != NULL);
	KASSERT(wpi_firmware_size != 0);

	if (--wpi_firmware_users == 0) {
		firmware_free(wpi_firmware_image, wpi_firmware_size);
		wpi_firmware_image = NULL;
		wpi_firmware_size = 0;
	}

	mutex_exit(&wpi_firmware_mutex);
}

static int
wpi_load_firmware(struct wpi_softc *sc)
{
	struct wpi_dma_info *dma = &sc->fw_dma;
	struct wpi_firmware_hdr hdr;
	const uint8_t *init_text, *init_data, *main_text, *main_data;
	const uint8_t *boot_text;
	uint32_t init_textsz, init_datasz, main_textsz, main_datasz;
	uint32_t boot_textsz;
	size_t size;
	int error;

	if (!sc->fw_used) {
		if ((error = wpi_cache_firmware(sc)) != 0)
			return error;
		sc->fw_used = true;
	}

	KASSERT(sc->fw_used);
	KASSERT(wpi_firmware_image != NULL);
	KASSERT(wpi_firmware_size > sizeof(hdr));

	memcpy(&hdr, wpi_firmware_image, sizeof(hdr));

	main_textsz = le32toh(hdr.main_textsz);
	main_datasz = le32toh(hdr.main_datasz);
	init_textsz = le32toh(hdr.init_textsz);
	init_datasz = le32toh(hdr.init_datasz);
	boot_textsz = le32toh(hdr.boot_textsz);

	/* sanity-check firmware segments sizes */
	if (main_textsz > WPI_FW_MAIN_TEXT_MAXSZ ||
	    main_datasz > WPI_FW_MAIN_DATA_MAXSZ ||
	    init_textsz > WPI_FW_INIT_TEXT_MAXSZ ||
	    init_datasz > WPI_FW_INIT_DATA_MAXSZ ||
	    boot_textsz > WPI_FW_BOOT_TEXT_MAXSZ ||
	    (boot_textsz & 3) != 0) {
		aprint_error_dev(sc->sc_dev, "invalid firmware header\n");
		error = EINVAL;
		goto free_firmware;
	}

	/* check that all firmware segments are present */
	size = sizeof (struct wpi_firmware_hdr) + main_textsz +
	    main_datasz + init_textsz + init_datasz + boot_textsz;
	if (wpi_firmware_size < size) {
		aprint_error_dev(sc->sc_dev,
		    "firmware file truncated: %zu bytes, expected %zu bytes\n",
		    wpi_firmware_size, size);
		error = EINVAL;
		goto free_firmware;
	}

	/* get pointers to firmware segments */
	main_text = wpi_firmware_image + sizeof (struct wpi_firmware_hdr);
	main_data = main_text + main_textsz;
	init_text = main_data + main_datasz;
	init_data = init_text + init_textsz;
	boot_text = init_data + init_datasz;

	/* copy initialization images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, init_data, init_datasz);
	memcpy((char *)dma->vaddr + WPI_FW_INIT_DATA_MAXSZ, init_text,
	    init_textsz);

	bus_dmamap_sync(dma->tag, dma->map, 0, dma->size, BUS_DMASYNC_PREWRITE);

	/* tell adapter where to find initialization images */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_DATA_BASE, dma->paddr);
	wpi_mem_write(sc, WPI_MEM_DATA_SIZE, init_datasz);
	wpi_mem_write(sc, WPI_MEM_TEXT_BASE,
	    dma->paddr + WPI_FW_INIT_DATA_MAXSZ);
	wpi_mem_write(sc, WPI_MEM_TEXT_SIZE, init_textsz);
	wpi_mem_unlock(sc);

	/* load firmware boot code */
	if ((error = wpi_load_microcode(sc, boot_text, boot_textsz)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not load boot firmware\n");
		return error;
	}

	/* now press "execute" ;-) */
	WPI_WRITE(sc, WPI_RESET, 0);

	/* wait at most one second for first alive notification */
	if ((error = tsleep(sc, PCATCH, "wpiinit", hz)) != 0) {
		/* this isn't what was supposed to happen.. */
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for adapter to initialize\n");
	}

	/* copy runtime images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, main_data, main_datasz);
	memcpy((char *)dma->vaddr + WPI_FW_MAIN_DATA_MAXSZ, main_text,
	    main_textsz);

	bus_dmamap_sync(dma->tag, dma->map, 0, dma->size, BUS_DMASYNC_PREWRITE);

	/* tell adapter where to find runtime images */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_DATA_BASE, dma->paddr);
	wpi_mem_write(sc, WPI_MEM_DATA_SIZE, main_datasz);
	wpi_mem_write(sc, WPI_MEM_TEXT_BASE,
	    dma->paddr + WPI_FW_MAIN_DATA_MAXSZ);
	wpi_mem_write(sc, WPI_MEM_TEXT_SIZE, WPI_FW_UPDATED | main_textsz);
	wpi_mem_unlock(sc);

	/* wait at most one second for second alive notification */
	if ((error = tsleep(sc, PCATCH, "wpiinit", hz)) != 0) {
		/* this isn't what was supposed to happen.. */
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for adapter to initialize\n");
	}

	return error;

free_firmware:
	sc->fw_used = false;
	wpi_release_firmware();
	return error;
}

static void
wpi_calib_timeout(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int temp, s;

	/* automatic rate control triggered every 500ms */
	if (ic->ic_fixed_rate == -1) {
		s = splnet();
		if (ic->ic_opmode == IEEE80211_M_STA)
			wpi_iter_func(sc, ic->ic_bss);
		else
			ieee80211_iterate_nodes(&ic->ic_sta, wpi_iter_func, sc);
		splx(s);
	}

	/* update sensor data */
	temp = (int)WPI_READ(sc, WPI_TEMPERATURE);

	/* automatic power calibration every 60s */
	if (++sc->calib_cnt >= 120) {
		wpi_power_calibration(sc, temp);
		sc->calib_cnt = 0;
	}

	callout_schedule(&sc->calib_to, hz/2);
}

static void
wpi_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct wpi_softc *sc = arg;
	struct wpi_node *wn = (struct wpi_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &wn->amn);
}

/*
 * This function is called periodically (every 60 seconds) to adjust output
 * power to temperature changes.
 */
void
wpi_power_calibration(struct wpi_softc *sc, int temp)
{
	/* sanity-check read value */
	if (temp < -260 || temp > 25) {
		/* this can't be correct, ignore */
		DPRINTF(("out-of-range temperature reported: %d\n", temp));
		return;
	}

	DPRINTF(("temperature %d->%d\n", sc->temp, temp));

	/* adjust Tx power if need be */
	if (abs(temp - sc->temp) <= 6)
		return;

	sc->temp = temp;

	if (wpi_set_txpower(sc, sc->sc_ic.ic_curchan, 1) != 0) {
		/* just warn, too bad for the automatic calibration... */
		aprint_error_dev(sc->sc_dev, "could not adjust Tx power\n");
	}
}

static void
wpi_rx_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc,
    struct wpi_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct wpi_rx_ring *ring = &sc->rxq;
	struct wpi_rx_stat *stat;
	struct wpi_rx_head *head;
	struct wpi_rx_tail *tail;
	struct wpi_rbuf *rbuf;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m, *mnew;
	int data_off, error;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	stat = (struct wpi_rx_stat *)(desc + 1);

	if (stat->len > WPI_STAT_MAXLEN) {
		aprint_error_dev(sc->sc_dev, "invalid rx statistic header\n");
		ifp->if_ierrors++;
		return;
	}

	head = (struct wpi_rx_head *)((char *)(stat + 1) + stat->len);
	tail = (struct wpi_rx_tail *)((char *)(head + 1) + le16toh(head->len));

	DPRINTFN(4, ("rx intr: idx=%d len=%d stat len=%d rssi=%d rate=%x "
	    "chan=%d tstamp=%" PRIu64 "\n", ring->cur, le32toh(desc->len),
	    le16toh(head->len), (int8_t)stat->rssi, head->rate, head->chan,
	    le64toh(tail->tstamp)));

	/*
	 * Discard Rx frames with bad CRC early (XXX we may want to pass them
	 * to radiotap in monitor mode).
	 */
	if ((le32toh(tail->flags) & WPI_RX_NOERROR) != WPI_RX_NOERROR) {
		DPRINTF(("rx tail flags error %x\n",
		    le32toh(tail->flags)));
		ifp->if_ierrors++;
		return;
	}

	/* Compute where are the useful datas */
	data_off = (char*)(head + 1) - mtod(data->m, char*);
			
	MGETHDR(mnew, M_DONTWAIT, MT_DATA);
	if (mnew == NULL) {
		ifp->if_ierrors++;
		return;
	}

	rbuf = wpi_alloc_rbuf(sc);
	if (rbuf == NULL) {
		m_freem(mnew);
		ifp->if_ierrors++;
		return;
	}

	/* attach Rx buffer to mbuf */
	MEXTADD(mnew, rbuf->vaddr, WPI_RBUF_SIZE, 0, wpi_free_rbuf,
		rbuf);
	mnew->m_flags |= M_EXT_RW;

	bus_dmamap_unload(sc->sc_dmat, data->map);

	error = bus_dmamap_load(sc->sc_dmat, data->map,
	    mtod(mnew, void *), WPI_RBUF_SIZE, NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_READ);
	if (error) {
		device_printf(sc->sc_dev,
		    "couldn't load rx mbuf: %d\n", error);
		m_freem(mnew);
		ifp->if_ierrors++;

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), WPI_RBUF_SIZE, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error)
			panic("%s: bus_dmamap_load failed: %d\n",
			    device_xname(sc->sc_dev), error);
		return;
	}

	/* new mbuf loaded successfully */
	m = data->m;
	data->m = mnew;

	/* update Rx descriptor */
	ring->desc[ring->cur] = htole32(rbuf->paddr);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0,
	    ring->desc_dma.size,
	    BUS_DMASYNC_PREWRITE);

	m->m_data = (char*)m->m_data + data_off;
	m->m_pkthdr.len = m->m_len = le16toh(head->len);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;

	if (sc->sc_drvbpf != NULL) {
		struct wpi_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[head->chan].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[head->chan].ic_flags);
		tap->wr_dbm_antsignal = (int8_t)(stat->rssi - WPI_RSSI_OFFSET);
		tap->wr_dbm_antnoise = (int8_t)le16toh(stat->noise);
		tap->wr_tsft = tail->tstamp;
		tap->wr_antenna = (le16toh(head->flags) >> 4) & 0xf;
		switch (head->rate) {
		/* CCK rates */
		case  10: tap->wr_rate =   2; break;
		case  20: tap->wr_rate =   4; break;
		case  55: tap->wr_rate =  11; break;
		case 110: tap->wr_rate =  22; break;
		/* OFDM rates */
		case 0xd: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0x5: tap->wr_rate =  24; break;
		case 0x7: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xb: tap->wr_rate =  72; break;
		case 0x1: tap->wr_rate =  96; break;
		case 0x3: tap->wr_rate = 108; break;
		/* unknown rate: should not happen */
		default:  tap->wr_rate =   0;
		}
		if (le16toh(head->flags) & 0x4)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	/* grab a reference to the source node */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* send the frame to the 802.11 layer */
	ieee80211_input(ic, m, ni, stat->rssi, 0);

	/* release node reference */
	ieee80211_free_node(ni);
}

static void
wpi_tx_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc)
{
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	struct wpi_tx_ring *ring = &sc->txq[desc->qid & 0x3];
	struct wpi_tx_data *data = &ring->data[desc->idx];
	struct wpi_tx_stat *stat = (struct wpi_tx_stat *)(desc + 1);
	struct wpi_node *wn = (struct wpi_node *)data->ni;

	DPRINTFN(4, ("tx done: qid=%d idx=%d retries=%d nkill=%d rate=%x "
	    "duration=%d status=%x\n", desc->qid, desc->idx, stat->ntries,
	    stat->nkill, stat->rate, le32toh(stat->duration),
	    le32toh(stat->status)));

	/*
	 * Update rate control statistics for the node.
	 * XXX we should not count mgmt frames since they're always sent at
	 * the lowest available bit-rate.
	 */
	wn->amn.amn_txcnt++;
	if (stat->ntries > 0) {
		DPRINTFN(3, ("tx intr ntries %d\n", stat->ntries));
		wn->amn.amn_retrycnt++;
	}

	if ((le32toh(stat->status) & 0xff) != 1)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	bus_dmamap_unload(sc->sc_dmat, data->map);
	m_freem(data->m);
	data->m = NULL;
	ieee80211_free_node(data->ni);
	data->ni = NULL;

	ring->queued--;

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	wpi_start(ifp);
}

static void
wpi_cmd_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc)
{
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_data *data;

	if ((desc->qid & 7) != 4)
		return;	/* not a command ack */

	data = &ring->data[desc->idx];

	/* if the command was mapped in a mbuf, free it */
	if (data->m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}

	wakeup(&ring->cmd[desc->idx]);
}

static void
wpi_notif_intr(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp =  ic->ic_ifp;
	uint32_t hw;

	bus_dmamap_sync(sc->sc_dmat, sc->shared_dma.map, 0,
	    sizeof(struct wpi_shared), BUS_DMASYNC_POSTREAD);

	hw = le32toh(sc->shared->next);
	while (sc->rxq.cur != hw) {
		struct wpi_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		struct wpi_rx_desc *desc;

		bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		desc = mtod(data->m, struct wpi_rx_desc *);

		DPRINTFN(4, ("rx notification qid=%x idx=%d flags=%x type=%d "
		    "len=%d\n", desc->qid, desc->idx, desc->flags,
		    desc->type, le32toh(desc->len)));

		if (!(desc->qid & 0x80))	/* reply to a command */
			wpi_cmd_intr(sc, desc);

		switch (desc->type) {
		case WPI_RX_DONE:
			/* a 802.11 frame was received */
			wpi_rx_intr(sc, desc, data);
			break;

		case WPI_TX_DONE:
			/* a 802.11 frame has been transmitted */
			wpi_tx_intr(sc, desc);
			break;

		case WPI_UC_READY:
		{
			struct wpi_ucode_info *uc =
			    (struct wpi_ucode_info *)(desc + 1);

			/* the microcontroller is ready */
			DPRINTF(("microcode alive notification version %x "
			    "alive %x\n", le32toh(uc->version),
			    le32toh(uc->valid)));

			if (le32toh(uc->valid) != 1) {
				aprint_error_dev(sc->sc_dev,
				    "microcontroller initialization failed\n");
			}
			break;
		}
		case WPI_STATE_CHANGED:
		{
			uint32_t *status = (uint32_t *)(desc + 1);

			/* enabled/disabled notification */
			DPRINTF(("state changed to %x\n", le32toh(*status)));

			if (le32toh(*status) & 1) {
				/* the radio button has to be pushed */
				/* wake up thread to signal powerd */
				cv_signal(&sc->sc_rsw_cv);
				aprint_error_dev(sc->sc_dev,
				    "Radio transmitter is off\n");
				/* turn the interface down */
				ifp->if_flags &= ~IFF_UP;
				wpi_stop(ifp, 1);
				return;	/* no further processing */
			}
			break;
		}
		case WPI_START_SCAN:
		{
#if 0
			struct wpi_start_scan *scan =
			    (struct wpi_start_scan *)(desc + 1);

			DPRINTFN(2, ("scanning channel %d status %x\n",
			    scan->chan, le32toh(scan->status)));

			/* fix current channel */
			ic->ic_curchan = &ic->ic_channels[scan->chan];
#endif
			break;
		}
		case WPI_STOP_SCAN:
		{
#ifdef WPI_DEBUG
			struct wpi_stop_scan *scan =
			    (struct wpi_stop_scan *)(desc + 1);
#endif

			DPRINTF(("scan finished nchan=%d status=%d chan=%d\n",
			    scan->nchan, scan->status, scan->chan));

			sc->is_scanning = false;
			if (ic->ic_state == IEEE80211_S_SCAN)
				ieee80211_next_scan(ic);

			break;
		}
		}

		sc->rxq.cur = (sc->rxq.cur + 1) % WPI_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? WPI_RX_RING_COUNT - 1 : hw - 1;
	WPI_WRITE(sc, WPI_RX_WIDX, hw & ~7);
}

static int
wpi_intr(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	uint32_t r;

	r = WPI_READ(sc, WPI_INTR);
	if (r == 0 || r == 0xffffffff)
		return 0;	/* not for us */

	DPRINTFN(6, ("interrupt reg %x\n", r));

	/* disable interrupts */
	WPI_WRITE(sc, WPI_MASK, 0);
	/* ack interrupts */
	WPI_WRITE(sc, WPI_INTR, r);

	if (r & (WPI_SW_ERROR | WPI_HW_ERROR)) {
		/* SYSTEM FAILURE, SYSTEM FAILURE */
		aprint_error_dev(sc->sc_dev, "fatal firmware error\n");
		ifp->if_flags &= ~IFF_UP;
		wpi_stop(ifp, 1);
		return 1;
	}

	if (r & WPI_RX_INTR)
		wpi_notif_intr(sc);

	if (r & WPI_ALIVE_INTR)	/* firmware initialized */
		wakeup(sc);

	/* re-enable interrupts */
	if (ifp->if_flags & IFF_UP)
		WPI_WRITE(sc, WPI_MASK, WPI_INTR_MASK);

	return 1;
}

static uint8_t
wpi_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 10;
	case 4:		return 20;
	case 11:	return 55;
	case 22:	return 110;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	/* R1-R4, (u)ral is R4-R1 */
	case 12:	return 0xd;
	case 18:	return 0xf;
	case 24:	return 0x5;
	case 36:	return 0x7;
	case 48:	return 0x9;
	case 72:	return 0xb;
	case 96:	return 0x1;
	case 108:	return 0x3;

	/* unsupported rates (should not get there) */
	default:	return 0;
	}
}

/* quickly determine if a given rate is CCK or OFDM */
#define WPI_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

static int
wpi_tx_data(struct wpi_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
    int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_tx_ring *ring = &sc->txq[ac];
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_cmd_data *tx;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	const struct chanAccParams *cap;
	struct mbuf *mnew;
	int i, rate, error, hdrlen, noack = 0;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	wh = mtod(m0, struct ieee80211_frame *);

	if (ieee80211_has_qos(wh)) {
		cap = &ic->ic_wme.wme_chanParams;
		noack = cap->cap_wmeParams[ac].wmep_noackPolicy;
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

	hdrlen = ieee80211_anyhdrsize(wh);

	/* pickup a rate */
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT) {
		/* mgmt frames are sent at the lowest available bit-rate */
		rate = ni->ni_rates.rs_rates[0];
	} else {
		if (ic->ic_fixed_rate != -1) {
			rate = ic->ic_sup_rates[ic->ic_curmode].
			    rs_rates[ic->ic_fixed_rate];
		} else
			rate = ni->ni_rates.rs_rates[ni->ni_txrate];
	}
	rate &= IEEE80211_RATE_VAL;

	if (sc->sc_drvbpf != NULL) {
		struct wpi_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ni->ni_chan->ic_flags);
		tap->wt_rate = rate;
		tap->wt_hwqueue = ac;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	cmd = &ring->cmd[ring->cur];
	cmd->code = WPI_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct wpi_cmd_data *)cmd->data;
	/* no need to zero tx, all fields are reinitialized here */
	tx->flags = 0;

	if (!noack && !IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		tx->flags |= htole32(WPI_TX_NEED_ACK);
	} else if (m0->m_pkthdr.len + IEEE80211_CRC_LEN > ic->ic_rtsthreshold)
		tx->flags |= htole32(WPI_TX_NEED_RTS | WPI_TX_FULL_TXOP);

	tx->flags |= htole32(WPI_TX_AUTO_SEQ);

	/* retrieve destination node's id */
	tx->id = IEEE80211_IS_MULTICAST(wh->i_addr1) ? WPI_ID_BROADCAST :
		WPI_ID_BSS;

	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT) {
		/* tell h/w to set timestamp in probe responses */
		if ((wh->i_fc[0] &
		    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
		    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			tx->flags |= htole32(WPI_TX_INSERT_TSTAMP);

		if (((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
			 IEEE80211_FC0_SUBTYPE_ASSOC_REQ) ||
			((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
			 IEEE80211_FC0_SUBTYPE_REASSOC_REQ))
			tx->timeout = htole16(3);
		else
			tx->timeout = htole16(2);
	} else
		tx->timeout = htole16(0);

	tx->rate = wpi_plcp_signal(rate);

	/* be very persistant at sending frames out */
	tx->rts_ntries = 7;
	tx->data_ntries = 15;

	tx->ofdm_mask = 0xff;
	tx->cck_mask = 0x0f;
	tx->lifetime = htole32(WPI_LIFETIME_INFINITE);

	tx->len = htole16(m0->m_pkthdr.len);

	/* save and trim IEEE802.11 header */
	memcpy((uint8_t *)(tx + 1), wh, hdrlen);
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

	DPRINTFN(4, ("sending data: qid=%d idx=%d len=%d nsegs=%d\n",
	    ring->qid, ring->cur, m0->m_pkthdr.len, data->map->dm_nsegs));

	/* first scatter/gather segment is used by the tx data command */
	desc->flags = htole32(WPI_PAD32(m0->m_pkthdr.len) << 28 |
	    (1 + data->map->dm_nsegs) << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + sizeof (struct wpi_cmd_data) +
	    ((hdrlen + 3) & ~3));
	for (i = 1; i <= data->map->dm_nsegs; i++) {
		desc->segs[i].addr =
		    htole32(data->map->dm_segs[i - 1].ds_addr);
		desc->segs[i].len  =
		    htole32(data->map->dm_segs[i - 1].ds_len);
	}

	ring->queued++;

	bus_dmamap_sync(sc->sc_dmat, data->map, 0,
	    data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, ring->cmd_dma.map, 0,
	    ring->cmd_dma.size,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0,
	    ring->desc_dma.size,
	    BUS_DMASYNC_PREWRITE);

	/* kick ring */
	ring->cur = (ring->cur + 1) % WPI_TX_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;
}

static void
wpi_start(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ether_header *eh;
	struct mbuf *m0;
	int ac;

	/*
	 * net80211 may still try to send management frames even if the
	 * IFF_RUNNING flag is not set...
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IF_DEQUEUE(&ic->ic_mgtq, m0);
		if (m0 != NULL) {

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;

			/* management frames go into ring 0 */
			if (sc->txq[0].queued > sc->txq[0].count - 8) {
				ifp->if_oerrors++;
				continue;
			}
			bpf_mtap3(ic->ic_rawbpf, m0);
			if (wpi_tx_data(sc, m0, ni, 0) != 0) {
				ifp->if_oerrors++;
				break;
			}
		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_POLL(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;

			if (m0->m_len < sizeof (*eh) &&
			    (m0 = m_pullup(m0, sizeof (*eh))) == NULL) {
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
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			bpf_mtap(ifp, m0);
			m0 = ieee80211_encap(ic, m0, ni);
			if (m0 == NULL) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				continue;
			}
			bpf_mtap3(ic->ic_rawbpf, m0);
			if (wpi_tx_data(sc, m0, ni, ac) != 0) {
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
wpi_watchdog(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			ifp->if_flags &= ~IFF_UP;
			wpi_stop(ifp, 1);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(&sc->sc_ic);
}

static int
wpi_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
#define IS_RUNNING(ifp) \
	((ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING))

	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				wpi_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				wpi_stop(ifp, 1);
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

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if (IS_RUNNING(ifp) &&
			(ic->ic_roaming != IEEE80211_ROAMING_MANUAL))
			wpi_init(ifp);
		error = 0;
	}

	splx(s);
	return error;

#undef IS_RUNNING
}

/*
 * Extract various information from EEPROM.
 */
static void
wpi_read_eeprom(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	char domain[4];
	int i;

	wpi_read_prom_data(sc, WPI_EEPROM_CAPABILITIES, &sc->cap, 1);
	wpi_read_prom_data(sc, WPI_EEPROM_REVISION, &sc->rev, 2);
	wpi_read_prom_data(sc, WPI_EEPROM_TYPE, &sc->type, 1);

	DPRINTF(("cap=%x rev=%x type=%x\n", sc->cap, le16toh(sc->rev),
	    sc->type));

	/* read and print regulatory domain */
	wpi_read_prom_data(sc, WPI_EEPROM_DOMAIN, domain, 4);
	aprint_normal_dev(sc->sc_dev, "%.4s", domain);

	/* read and print MAC address */
	wpi_read_prom_data(sc, WPI_EEPROM_MAC, ic->ic_myaddr, 6);
	aprint_normal(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* read the list of authorized channels */
	for (i = 0; i < WPI_CHAN_BANDS_COUNT; i++)
		wpi_read_eeprom_channels(sc, i);

	/* read the list of power groups */
	for (i = 0; i < WPI_POWER_GROUPS_COUNT; i++)
		wpi_read_eeprom_group(sc, i);
}

static void
wpi_read_eeprom_channels(struct wpi_softc *sc, int n)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct wpi_chan_band *band = &wpi_bands[n];
	struct wpi_eeprom_chan channels[WPI_MAX_CHAN_PER_BAND];
	int chan, i;

	wpi_read_prom_data(sc, band->addr, channels,
	    band->nchan * sizeof (struct wpi_eeprom_chan));

	for (i = 0; i < band->nchan; i++) {
		if (!(channels[i].flags & WPI_EEPROM_CHAN_VALID))
			continue;

		chan = band->chan[i];

		if (n == 0) {	/* 2GHz band */
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
			ic->ic_channels[chan].ic_flags =
			    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
			    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;

		} else {	/* 5GHz band */
			/*
			 * Some 3945ABG adapters support channels 7, 8, 11
			 * and 12 in the 2GHz *and* 5GHz bands.
			 * Because of limitations in our net80211(9) stack,
			 * we can't support these channels in 5GHz band.
			 */
			if (chan <= 14)
				continue;

			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A;
		}

		/* is active scan allowed on this channel? */
		if (!(channels[i].flags & WPI_EEPROM_CHAN_ACTIVE)) {
			ic->ic_channels[chan].ic_flags |=
			    IEEE80211_CHAN_PASSIVE;
		}

		/* save maximum allowed power for this channel */
		sc->maxpwr[chan] = channels[i].maxpwr;

		DPRINTF(("adding chan %d flags=0x%x maxpwr=%d\n",
		    chan, channels[i].flags, sc->maxpwr[chan]));
	}
}

static void
wpi_read_eeprom_group(struct wpi_softc *sc, int n)
{
	struct wpi_power_group *group = &sc->groups[n];
	struct wpi_eeprom_group rgroup;
	int i;

	wpi_read_prom_data(sc, WPI_EEPROM_POWER_GRP + n * 32, &rgroup,
	    sizeof rgroup);

	/* save power group information */
	group->chan   = rgroup.chan;
	group->maxpwr = rgroup.maxpwr;
	/* temperature at which the samples were taken */
	group->temp   = (int16_t)le16toh(rgroup.temp);

	DPRINTF(("power group %d: chan=%d maxpwr=%d temp=%d\n", n,
	    group->chan, group->maxpwr, group->temp));

	for (i = 0; i < WPI_SAMPLES_COUNT; i++) {
		group->samples[i].index = rgroup.samples[i].index;
		group->samples[i].power = rgroup.samples[i].power;

		DPRINTF(("\tsample %d: index=%d power=%d\n", i,
		    group->samples[i].index, group->samples[i].power));
	}
}

/*
 * Send a command to the firmware.
 */
static int
wpi_cmd(struct wpi_softc *sc, int code, const void *buf, int size, int async)
{
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_cmd *cmd;
	struct wpi_dma_info *dma;

	KASSERT(size <= sizeof cmd->data);

	desc = &ring->desc[ring->cur];
	cmd = &ring->cmd[ring->cur];

	cmd->code = code;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;
	memcpy(cmd->data, buf, size);

	dma = &ring->cmd_dma;
	bus_dmamap_sync(dma->tag, dma->map, 0, dma->size, BUS_DMASYNC_PREWRITE);

	desc->flags = htole32(WPI_PAD32(size) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + size);

	dma = &ring->desc_dma;
	bus_dmamap_sync(dma->tag, dma->map, 0, dma->size, BUS_DMASYNC_PREWRITE);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return async ? 0 : tsleep(cmd, PCATCH, "wpicmd", hz);
}

static int
wpi_wme_update(struct ieee80211com *ic)
{
#define WPI_EXP2(v)	htole16((1 << (v)) - 1)
#define WPI_USEC(v)	htole16(IEEE80211_TXOP_TO_US(v))
	struct wpi_softc *sc = ic->ic_ifp->if_softc;
	const struct wmeParams *wmep;
	struct wpi_wme_setup wme;
	int ac;

	/* don't override default WME values if WME is not actually enabled */
	if (!(ic->ic_flags & IEEE80211_F_WME))
		return 0;

	wme.flags = 0;
	for (ac = 0; ac < WME_NUM_AC; ac++) {
		wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
		wme.ac[ac].aifsn = wmep->wmep_aifsn;
		wme.ac[ac].cwmin = WPI_EXP2(wmep->wmep_logcwmin);
		wme.ac[ac].cwmax = WPI_EXP2(wmep->wmep_logcwmax);
		wme.ac[ac].txop  = WPI_USEC(wmep->wmep_txopLimit);

		DPRINTF(("setting WME for queue %d aifsn=%d cwmin=%d cwmax=%d "
		    "txop=%d\n", ac, wme.ac[ac].aifsn, wme.ac[ac].cwmin,
		    wme.ac[ac].cwmax, wme.ac[ac].txop));
	}

	return wpi_cmd(sc, WPI_CMD_SET_WME, &wme, sizeof wme, 1);
#undef WPI_USEC
#undef WPI_EXP2
}

/*
 * Configure h/w multi-rate retries.
 */
static int
wpi_mrr_setup(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_mrr_setup mrr;
	int i, error;

	/* CCK rates (not used with 802.11a) */
	for (i = WPI_CCK1; i <= WPI_CCK11; i++) {
		mrr.rates[i].flags = 0;
		mrr.rates[i].plcp = wpi_ridx_to_plcp[i];
		/* fallback to the immediate lower CCK rate (if any) */
		mrr.rates[i].next = (i == WPI_CCK1) ? WPI_CCK1 : i - 1;
		/* try one time at this rate before falling back to "next" */
		mrr.rates[i].ntries = 1;
	}

	/* OFDM rates (not used with 802.11b) */
	for (i = WPI_OFDM6; i <= WPI_OFDM54; i++) {
		mrr.rates[i].flags = 0;
		mrr.rates[i].plcp = wpi_ridx_to_plcp[i];
		/* fallback to the immediate lower rate (if any) */
		/* we allow fallback from OFDM/6 to CCK/2 in 11b/g mode */
		mrr.rates[i].next = (i == WPI_OFDM6) ?
		    ((ic->ic_curmode == IEEE80211_MODE_11A) ?
			WPI_OFDM6 : WPI_CCK2) :
		    i - 1;
		/* try one time at this rate before falling back to "next" */
		mrr.rates[i].ntries = 1;
	}

	/* setup MRR for control frames */
	mrr.which = htole32(WPI_MRR_CTL);
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not setup MRR for control frames\n");
		return error;
	}

	/* setup MRR for data frames */
	mrr.which = htole32(WPI_MRR_DATA);
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not setup MRR for data frames\n");
		return error;
	}

	return 0;
}

static void
wpi_set_led(struct wpi_softc *sc, uint8_t which, uint8_t off, uint8_t on)
{
	struct wpi_cmd_led led;

	led.which = which;
	led.unit = htole32(100000);	/* on/off in unit of 100ms */
	led.off = off;
	led.on = on;

	(void)wpi_cmd(sc, WPI_CMD_SET_LED, &led, sizeof led, 1);
}

static void
wpi_enable_tsf(struct wpi_softc *sc, struct ieee80211_node *ni)
{
	struct wpi_cmd_tsf tsf;
	uint64_t val, mod;

	memset(&tsf, 0, sizeof tsf);
	memcpy(&tsf.tstamp, ni->ni_tstamp.data, sizeof (uint64_t));
	tsf.bintval = htole16(ni->ni_intval);
	tsf.lintval = htole16(10);

	/* compute remaining time until next beacon */
	val = (uint64_t)ni->ni_intval * 1024;	/* msecs -> usecs */
	mod = le64toh(tsf.tstamp) % val;
	tsf.binitval = htole32((uint32_t)(val - mod));

	DPRINTF(("TSF bintval=%u tstamp=%" PRIu64 ", init=%u\n",
	    ni->ni_intval, le64toh(tsf.tstamp), (uint32_t)(val - mod)));

	if (wpi_cmd(sc, WPI_CMD_TSF, &tsf, sizeof tsf, 1) != 0)
		aprint_error_dev(sc->sc_dev, "could not enable TSF\n");
}

/*
 * Update Tx power to match what is defined for channel `c'.
 */
static int
wpi_set_txpower(struct wpi_softc *sc, struct ieee80211_channel *c, int async)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_power_group *group;
	struct wpi_cmd_txpower txpower;
	u_int chan;
	int i;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, c);

	/* find the power group to which this channel belongs */
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		for (group = &sc->groups[1]; group < &sc->groups[4]; group++)
			if (chan <= group->chan)
				break;
	} else
		group = &sc->groups[0];

	memset(&txpower, 0, sizeof txpower);
	txpower.band = IEEE80211_IS_CHAN_5GHZ(c) ? 0 : 1;
	txpower.chan = htole16(chan);

	/* set Tx power for all OFDM and CCK rates */
	for (i = 0; i <= 11 ; i++) {
		/* retrieve Tx power for this channel/rate combination */
		int idx = wpi_get_power_index(sc, group, c,
		    wpi_ridx_to_rate[i]);

		txpower.rates[i].plcp = wpi_ridx_to_plcp[i];

		if (IEEE80211_IS_CHAN_5GHZ(c)) {
			txpower.rates[i].rf_gain = wpi_rf_gain_5ghz[idx];
			txpower.rates[i].dsp_gain = wpi_dsp_gain_5ghz[idx];
		} else {
			txpower.rates[i].rf_gain = wpi_rf_gain_2ghz[idx];
			txpower.rates[i].dsp_gain = wpi_dsp_gain_2ghz[idx];
		}
		DPRINTF(("chan %d/rate %d: power index %d\n", chan,
		    wpi_ridx_to_rate[i], idx));
	}

	return wpi_cmd(sc, WPI_CMD_TXPOWER, &txpower, sizeof txpower, async);
}

/*
 * Determine Tx power index for a given channel/rate combination.
 * This takes into account the regulatory information from EEPROM and the
 * current temperature.
 */
static int
wpi_get_power_index(struct wpi_softc *sc, struct wpi_power_group *group,
    struct ieee80211_channel *c, int rate)
{
/* fixed-point arithmetic division using a n-bit fractional part */
#define fdivround(a, b, n)	\
	((((1 << n) * (a)) / (b) + (1 << n) / 2) / (1 << n))

/* linear interpolation */
#define interpolate(x, x1, y1, x2, y2, n)	\
	((y1) + fdivround(((x) - (x1)) * ((y2) - (y1)), (x2) - (x1), n))

	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_power_sample *sample;
	int pwr, idx;
	u_int chan;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, c);

	/* default power is group's maximum power - 3dB */
	pwr = group->maxpwr / 2;

	/* decrease power for highest OFDM rates to reduce distortion */
	switch (rate) {
	case 72:	/* 36Mb/s */
		pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 0 :  5;
		break;
	case 96:	/* 48Mb/s */
		pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 7 : 10;
		break;
	case 108:	/* 54Mb/s */
		pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 9 : 12;
		break;
	}

	/* never exceed channel's maximum allowed Tx power */
	pwr = min(pwr, sc->maxpwr[chan]);

	/* retrieve power index into gain tables from samples */
	for (sample = group->samples; sample < &group->samples[3]; sample++)
		if (pwr > sample[1].power)
			break;
	/* fixed-point linear interpolation using a 19-bit fractional part */
	idx = interpolate(pwr, sample[0].power, sample[0].index,
	    sample[1].power, sample[1].index, 19);

	/*-
	 * Adjust power index based on current temperature:
	 * - if cooler than factory-calibrated: decrease output power
	 * - if warmer than factory-calibrated: increase output power
	 */
	idx -= (sc->temp - group->temp) * 11 / 100;

	/* decrease power for CCK rates (-5dB) */
	if (!WPI_RATE_IS_OFDM(rate))
		idx += 10;

	/* keep power index in a valid range */
	if (idx < 0)
		return 0;
	if (idx > WPI_MAX_PWR_INDEX)
		return WPI_MAX_PWR_INDEX;
	return idx;

#undef interpolate
#undef fdivround
}

/*
 * Build a beacon frame that the firmware will broadcast periodically in
 * IBSS or HostAP modes.
 */
static int
wpi_setup_beacon(struct wpi_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_cmd_beacon *bcn;
	struct ieee80211_beacon_offsets bo;
	struct mbuf *m0;
	int error;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	m0 = ieee80211_beacon_alloc(ic, ni, &bo);
	if (m0 == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate beacon frame\n");
		return ENOMEM;
	}

	cmd = &ring->cmd[ring->cur];
	cmd->code = WPI_CMD_SET_BEACON;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	bcn = (struct wpi_cmd_beacon *)cmd->data;
	memset(bcn, 0, sizeof (struct wpi_cmd_beacon));
	bcn->id = WPI_ID_BROADCAST;
	bcn->ofdm_mask = 0xff;
	bcn->cck_mask = 0x0f;
	bcn->lifetime = htole32(WPI_LIFETIME_INFINITE);
	bcn->len = htole16(m0->m_pkthdr.len);
	bcn->rate = (ic->ic_curmode == IEEE80211_MODE_11A) ?
	    wpi_plcp_signal(12) : wpi_plcp_signal(2);
	bcn->flags = htole32(WPI_TX_AUTO_SEQ | WPI_TX_INSERT_TSTAMP);

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, sizeof (struct ieee80211_frame), (void *)&bcn->wh);
	m_adj(m0, sizeof (struct ieee80211_frame));

	/* assume beacon frame is contiguous */
	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m0,
	    BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map beacon\n");
		m_freem(m0);
		return error;
	}

	data->m = m0;

	/* first scatter/gather segment is used by the beacon command */
	desc->flags = htole32(WPI_PAD32(m0->m_pkthdr.len) << 28 | 2 << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + sizeof (struct wpi_cmd_beacon));
	desc->segs[1].addr = htole32(data->map->dm_segs[0].ds_addr);
	desc->segs[1].len  = htole32(data->map->dm_segs[0].ds_len);

	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0,
	    ring->desc_dma.map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;
}

static int
wpi_auth(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct wpi_node_info node;
	int error;

	/* update adapter's configuration */
	IEEE80211_ADDR_COPY(sc->config.bssid, ni->ni_bssid);
	sc->config.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	sc->config.flags = htole32(WPI_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		sc->config.flags |= htole32(WPI_CONFIG_AUTO |
		    WPI_CONFIG_24GHZ);
	}
	switch (ic->ic_curmode) {
	case IEEE80211_MODE_11A:
		sc->config.cck_mask  = 0;
		sc->config.ofdm_mask = 0x15;
		break;
	case IEEE80211_MODE_11B:
		sc->config.cck_mask  = 0x03;
		sc->config.ofdm_mask = 0;
		break;
	default:	/* assume 802.11b/g */
		sc->config.cck_mask  = 0x0f;
		sc->config.ofdm_mask = 0x15;
	}
	DPRINTF(("config chan %d flags %x cck %x ofdm %x\n", sc->config.chan,
	    sc->config.flags, sc->config.cck_mask, sc->config.ofdm_mask));
	error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
	    sizeof (struct wpi_config), 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not configure\n");
		return error;
	}

	/* configuration has changed, set Tx power accordingly */
	if ((error = wpi_set_txpower(sc, ni->ni_chan, 1)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not set Tx power\n");
		return error;
	}

	/* add default node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, ni->ni_bssid);
	node.id = WPI_ID_BSS;
	node.rate = (ic->ic_curmode == IEEE80211_MODE_11A) ?
	    wpi_plcp_signal(12) : wpi_plcp_signal(2);
	node.action = htole32(WPI_ACTION_SET_RATE);
	node.antenna = WPI_ANTENNA_BOTH;
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not add BSS node\n");
		return error;
	}

	return 0;
}

/*
 * Send a scan request to the firmware.  Since this command is huge, we map it
 * into a mbuf instead of using the pre-allocated set of commands.
 */
static int
wpi_scan(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_scan_hdr *hdr;
	struct wpi_scan_chan *chan;
	struct ieee80211_frame *wh;
	struct ieee80211_rateset *rs;
	struct ieee80211_channel *c;
	uint8_t *frm;
	int pktlen, error, nrates;

	if (ic->ic_curchan == NULL)
		return EIO;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	MGETHDR(data->m, M_DONTWAIT, MT_DATA);
	if (data->m == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate mbuf for scan command\n");
		return ENOMEM;
	}
	MCLGET(data->m, M_DONTWAIT);
	if (!(data->m->m_flags & M_EXT)) {
		m_freem(data->m);
		data->m = NULL;
		aprint_error_dev(sc->sc_dev,
		    "could not allocate mbuf for scan command\n");
		return ENOMEM;
	}

	cmd = mtod(data->m, struct wpi_tx_cmd *);
	cmd->code = WPI_CMD_SCAN;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	hdr = (struct wpi_scan_hdr *)cmd->data;
	memset(hdr, 0, sizeof (struct wpi_scan_hdr));
	hdr->cmd.flags = htole32(WPI_TX_AUTO_SEQ);
	hdr->cmd.id = WPI_ID_BROADCAST;
	hdr->cmd.lifetime = htole32(WPI_LIFETIME_INFINITE);
	/*
	 * Move to the next channel if no packets are received within 5 msecs
	 * after sending the probe request (this helps to reduce the duration
	 * of active scans).
	 */
	hdr->quiet = htole16(5);	/* timeout in milliseconds */
	hdr->plcp_threshold = htole16(1);	/* min # of packets */

	if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_5GHZ) {
		hdr->crc_threshold = htole16(1);
		/* send probe requests at 6Mbps */
		hdr->cmd.rate = wpi_plcp_signal(12);
		rs = &ic->ic_sup_rates[IEEE80211_MODE_11A];
	} else {
		hdr->flags = htole32(WPI_CONFIG_24GHZ | WPI_CONFIG_AUTO);
		/* send probe requests at 1Mbps */
		hdr->cmd.rate = wpi_plcp_signal(2);
		rs = &ic->ic_sup_rates[IEEE80211_MODE_11G];
	}

	/* for directed scans, firmware inserts the essid IE itself */
	if (ic->ic_des_esslen != 0) {
		hdr->essid[0].id  = IEEE80211_ELEMID_SSID;
		hdr->essid[0].len = ic->ic_des_esslen;
		memcpy(hdr->essid[0].data, ic->ic_des_essid, ic->ic_des_esslen);
	}

	/*
	 * Build a probe request frame.  Most of the following code is a
	 * copy & paste of what is done in net80211.
	 */
	wh = (struct ieee80211_frame *)(hdr + 1);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, etherbroadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, etherbroadcastaddr);
	*(u_int16_t *)&wh->i_dur[0] = 0;	/* filled by h/w */
	*(u_int16_t *)&wh->i_seq[0] = 0;	/* filled by h/w */

	frm = (uint8_t *)(wh + 1);

	/* add empty essid IE (firmware generates it for directed scans) */
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = 0;

	/* add supported rates IE */
	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	frm += nrates;

	/* add supported xrates IE */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
		*frm++ = IEEE80211_ELEMID_XRATES;
		*frm++ = nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}

	/* setup length of probe request */
	hdr->cmd.len = htole16(frm - (uint8_t *)wh);

	chan = (struct wpi_scan_chan *)frm;
	c = ic->ic_curchan;

	chan->chan = ieee80211_chan2ieee(ic, c);
	chan->flags = 0;
	if (!(c->ic_flags & IEEE80211_CHAN_PASSIVE)) {
		chan->flags |= WPI_CHAN_ACTIVE;
		if (ic->ic_des_esslen != 0)
			chan->flags |= WPI_CHAN_DIRECT;
	}
	chan->dsp_gain = 0x6e;
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		chan->rf_gain = 0x3b;
		chan->active  = htole16(10);
		chan->passive = htole16(110);
	} else {
		chan->rf_gain = 0x28;
		chan->active  = htole16(20);
		chan->passive = htole16(120);
	}
	hdr->nchan++;
	chan++;

	frm += sizeof (struct wpi_scan_chan);

	hdr->len = htole16(frm - (uint8_t *)hdr);
	pktlen = frm - (uint8_t *)cmd;

	error = bus_dmamap_load(sc->sc_dmat, data->map, cmd, pktlen, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map scan command\n");
		m_freem(data->m);
		data->m = NULL;
		return error;
	}

	desc->flags = htole32(WPI_PAD32(pktlen) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(data->map->dm_segs[0].ds_addr);
	desc->segs[0].len  = htole32(data->map->dm_segs[0].ds_len);

	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0,
	    ring->desc_dma.map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;	/* will be notified async. of failure/success */
}

static int
wpi_config(struct wpi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct wpi_power power;
	struct wpi_bluetooth bluetooth;
	struct wpi_node_info node;
	int error;

	memset(&power, 0, sizeof power);
	power.flags = htole32(WPI_POWER_CAM | 0x8);
	error = wpi_cmd(sc, WPI_CMD_SET_POWER_MODE, &power, sizeof power, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not set power mode\n");
		return error;
	}

	/* configure bluetooth coexistence */
	memset(&bluetooth, 0, sizeof bluetooth);
	bluetooth.flags = 3;
	bluetooth.lead = 0xaa;
	bluetooth.kill = 1;
	error = wpi_cmd(sc, WPI_CMD_BLUETOOTH, &bluetooth, sizeof bluetooth,
	    0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
			"could not configure bluetooth coexistence\n");
		return error;
	}

	/* configure adapter */
	memset(&sc->config, 0, sizeof (struct wpi_config));
	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	IEEE80211_ADDR_COPY(sc->config.myaddr, ic->ic_myaddr);
	/* set default channel */
	sc->config.chan = ieee80211_chan2ieee(ic, ic->ic_curchan);
	sc->config.flags = htole32(WPI_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		sc->config.flags |= htole32(WPI_CONFIG_AUTO |
		    WPI_CONFIG_24GHZ);
	}
	sc->config.filter = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		sc->config.mode = WPI_MODE_STA;
		sc->config.filter |= htole32(WPI_FILTER_MULTICAST);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		sc->config.mode = WPI_MODE_IBSS;
		break;
	case IEEE80211_M_HOSTAP:
		sc->config.mode = WPI_MODE_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		sc->config.mode = WPI_MODE_MONITOR;
		sc->config.filter |= htole32(WPI_FILTER_MULTICAST |
		    WPI_FILTER_CTL | WPI_FILTER_PROMISC);
		break;
	}
	sc->config.cck_mask  = 0x0f;	/* not yet negotiated */
	sc->config.ofdm_mask = 0xff;	/* not yet negotiated */
	error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
	    sizeof (struct wpi_config), 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "configure command failed\n");
		return error;
	}

	/* configuration has changed, set Tx power accordingly */
	if ((error = wpi_set_txpower(sc, ic->ic_curchan, 0)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not set Tx power\n");
		return error;
	}

	/* add broadcast node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, etherbroadcastaddr);
	node.id = WPI_ID_BROADCAST;
	node.rate = wpi_plcp_signal(2);
	node.action = htole32(WPI_ACTION_SET_RATE);
	node.antenna = WPI_ANTENNA_BOTH;
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not add broadcast node\n");
		return error;
	}

	if ((error = wpi_mrr_setup(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not setup MRR\n");
		return error;
	}

	return 0;
}

static void
wpi_stop_master(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = WPI_READ(sc, WPI_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp | WPI_STOP_MASTER);

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	if ((tmp & WPI_GPIO_PWR_STATUS) == WPI_GPIO_PWR_SLEEP)
		return;	/* already asleep */

	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_RESET) & WPI_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev, "timeout waiting for master\n");
	}
}

static int
wpi_power_up(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	wpi_mem_lock(sc);
	tmp = wpi_mem_read(sc, WPI_MEM_POWER);
	wpi_mem_write(sc, WPI_MEM_POWER, tmp & ~0x03000000);
	wpi_mem_unlock(sc);

	for (ntries = 0; ntries < 5000; ntries++) {
		if (WPI_READ(sc, WPI_GPIO_STATUS) & WPI_POWERED)
			break;
		DELAY(10);
	}
	if (ntries == 5000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for NIC to power up\n");
		return ETIMEDOUT;
	}
	return 0;
}

static int
wpi_reset(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	/* clear any pending interrupts */
	WPI_WRITE(sc, WPI_INTR, 0xffffffff);

	tmp = WPI_READ(sc, WPI_PLL_CTL);
	WPI_WRITE(sc, WPI_PLL_CTL, tmp | WPI_PLL_INIT);

	tmp = WPI_READ(sc, WPI_CHICKEN);
	WPI_WRITE(sc, WPI_CHICKEN, tmp | WPI_CHICKEN_RXNOLOS);

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp | WPI_GPIO_INIT);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (WPI_READ(sc, WPI_GPIO_CTL) & WPI_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for clock stabilization\n");
		return ETIMEDOUT;
	}

	/* initialize EEPROM */
	tmp = WPI_READ(sc, WPI_EEPROM_STATUS);
	if ((tmp & WPI_EEPROM_VERSION) == 0) {
		aprint_error_dev(sc->sc_dev, "EEPROM not found\n");
		return EIO;
	}
	WPI_WRITE(sc, WPI_EEPROM_STATUS, tmp & ~WPI_EEPROM_LOCKED);

	return 0;
}

static void
wpi_hw_config(struct wpi_softc *sc)
{
	uint32_t rev, hw;

	/* voodoo from the reference driver */
	hw = WPI_READ(sc, WPI_HWCONFIG);

	rev = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_CLASS_REG);
	rev = PCI_REVISION(rev);
	if ((rev & 0xc0) == 0x40)
		hw |= WPI_HW_ALM_MB;
	else if (!(rev & 0x80))
		hw |= WPI_HW_ALM_MM;

	if (sc->cap == 0x80)
		hw |= WPI_HW_SKU_MRC;

	hw &= ~WPI_HW_REV_D;
	if ((le16toh(sc->rev) & 0xf0) == 0xd0)
		hw |= WPI_HW_REV_D;

	if (sc->type > 1)
		hw |= WPI_HW_TYPE_B;

	DPRINTF(("setting h/w config %x\n", hw));
	WPI_WRITE(sc, WPI_HWCONFIG, hw);
}

static int
wpi_init(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int qid, ntries, error;

	wpi_stop(ifp,1);
	(void)wpi_reset(sc);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_CLOCK1, 0xa00);
	DELAY(20);
	tmp = wpi_mem_read(sc, WPI_MEM_PCIDEV);
	wpi_mem_write(sc, WPI_MEM_PCIDEV, tmp | 0x800);
	wpi_mem_unlock(sc);

	(void)wpi_power_up(sc);
	wpi_hw_config(sc);

	/* init Rx ring */
	wpi_mem_lock(sc);
	WPI_WRITE(sc, WPI_RX_BASE, sc->rxq.desc_dma.paddr);
	WPI_WRITE(sc, WPI_RX_RIDX_PTR, sc->shared_dma.paddr +
	    offsetof(struct wpi_shared, next));
	WPI_WRITE(sc, WPI_RX_WIDX, (WPI_RX_RING_COUNT - 1) & ~7);
	WPI_WRITE(sc, WPI_RX_CONFIG, 0xa9601010);
	wpi_mem_unlock(sc);

	/* init Tx rings */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_MODE, 2);	/* bypass mode */
	wpi_mem_write(sc, WPI_MEM_RA, 1);	/* enable RA0 */
	wpi_mem_write(sc, WPI_MEM_TXCFG, 0x3f);	/* enable all 6 Tx rings */
	wpi_mem_write(sc, WPI_MEM_BYPASS1, 0x10000);
	wpi_mem_write(sc, WPI_MEM_BYPASS2, 0x30002);
	wpi_mem_write(sc, WPI_MEM_MAGIC4, 4);
	wpi_mem_write(sc, WPI_MEM_MAGIC5, 5);

	WPI_WRITE(sc, WPI_TX_BASE_PTR, sc->shared_dma.paddr);
	WPI_WRITE(sc, WPI_MSG_CONFIG, 0xffff05a5);

	for (qid = 0; qid < 6; qid++) {
		WPI_WRITE(sc, WPI_TX_CTL(qid), 0);
		WPI_WRITE(sc, WPI_TX_BASE(qid), 0);
		WPI_WRITE(sc, WPI_TX_CONFIG(qid), 0x80200008);
	}
	wpi_mem_unlock(sc);

	/* clear "radio off" and "disable command" bits (reversed logic) */
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_DISABLE_CMD);

	/* clear any pending interrupts */
	WPI_WRITE(sc, WPI_INTR, 0xffffffff);
	/* enable interrupts */
	WPI_WRITE(sc, WPI_MASK, WPI_INTR_MASK);

	/* not sure why/if this is necessary... */
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);

	if ((error = wpi_load_firmware(sc)) != 0)
		/* wpi_load_firmware prints error messages for us.  */
		goto fail1;

	/* Check the status of the radio switch */
	mutex_enter(&sc->sc_rsw_mtx);
	if (wpi_getrfkill(sc)) {
		mutex_exit(&sc->sc_rsw_mtx);
		aprint_error_dev(sc->sc_dev,
		    "radio is disabled by hardware switch\n");
		ifp->if_flags &= ~IFF_UP;
		error = EBUSY;
		goto fail1;
	}
	mutex_exit(&sc->sc_rsw_mtx);

	/* wait for thermal sensors to calibrate */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((sc->temp = (int)WPI_READ(sc, WPI_TEMPERATURE)) != 0)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for thermal sensors calibration\n");
		error = ETIMEDOUT;
		goto fail1;
	}
	DPRINTF(("temperature %d\n", sc->temp));

	if ((error = wpi_config(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not configure device\n");
		goto fail1;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	}
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	return 0;

fail1:	wpi_stop(ifp, 1);
	return error;
}

static void
wpi_stop(struct ifnet *ifp, int disable)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int ac;

	ifp->if_timer = sc->sc_tx_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* disable interrupts */
	WPI_WRITE(sc, WPI_MASK, 0);
	WPI_WRITE(sc, WPI_INTR, WPI_INTR_MASK);
	WPI_WRITE(sc, WPI_INTR_STATUS, 0xff);
	WPI_WRITE(sc, WPI_INTR_STATUS, 0x00070000);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_MODE, 0);
	wpi_mem_unlock(sc);

	/* reset all Tx rings */
	for (ac = 0; ac < 4; ac++)
		wpi_reset_tx_ring(sc, &sc->txq[ac]);
	wpi_reset_tx_ring(sc, &sc->cmdq);

	/* reset Rx ring */
	wpi_reset_rx_ring(sc, &sc->rxq);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_CLOCK2, 0x200);
	wpi_mem_unlock(sc);

	DELAY(5);

	wpi_stop_master(sc);

	tmp = WPI_READ(sc, WPI_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp | WPI_SW_RESET);
}

static bool
wpi_resume(device_t dv, const pmf_qual_t *qual)
{
	struct wpi_softc *sc = device_private(dv);

	(void)wpi_reset(sc);

	return true;
}

/*
 * Return whether or not the radio is enabled in hardware
 * (i.e. the rfkill switch is "off").
 */
static int
wpi_getrfkill(struct wpi_softc *sc)
{
	uint32_t tmp;

	wpi_mem_lock(sc);
	tmp = wpi_mem_read(sc, WPI_MEM_RFKILL);
	wpi_mem_unlock(sc);

	KASSERT(mutex_owned(&sc->sc_rsw_mtx));
	if (tmp & 0x01) {
		/* switch is on */
		if (sc->sc_rsw_status != WPI_RSW_ON) {
			sc->sc_rsw_status = WPI_RSW_ON;
			sysmon_pswitch_event(&sc->sc_rsw,
			    PSWITCH_EVENT_PRESSED);
		}
	} else {
		/* switch is off */
		if (sc->sc_rsw_status != WPI_RSW_OFF) {
			sc->sc_rsw_status = WPI_RSW_OFF;
			sysmon_pswitch_event(&sc->sc_rsw,
			    PSWITCH_EVENT_RELEASED);
		}
	}

	return !(tmp & 0x01);
}

static int
wpi_sysctl_radio(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct wpi_softc *sc;
	int val, error;

	node = *rnode;
	sc = (struct wpi_softc *)node.sysctl_data;

	mutex_enter(&sc->sc_rsw_mtx);
	val = !wpi_getrfkill(sc);
	mutex_exit(&sc->sc_rsw_mtx);

	node.sysctl_data = &val;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	return 0;
}

static void
wpi_sysctlattach(struct wpi_softc *sc)
{
	int rc;
	const struct sysctlnode *rnode;
	const struct sysctlnode *cnode;

	struct sysctllog **clog = &sc->sc_sysctllog;

	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, device_xname(sc->sc_dev),
	    SYSCTL_DESCR("wpi controls and statistics"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT, CTLTYPE_INT, "radio",
	    SYSCTL_DESCR("radio transmitter switch state (0=off, 1=on)"),
	    wpi_sysctl_radio, 0, (void *)sc, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

#ifdef WPI_DEBUG
	/* control debugging printfs */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "debug", SYSCTL_DESCR("Enable debugging output"),
	    NULL, 0, &wpi_debug, 0, CTL_CREATE, CTL_EOL)) != 0)
		goto err;
#endif

	return;
err:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static void
wpi_rsw_thread(void *arg)
{
	struct wpi_softc *sc = (struct wpi_softc *)arg;

	mutex_enter(&sc->sc_rsw_mtx);
	for (;;) {
		cv_timedwait(&sc->sc_rsw_cv, &sc->sc_rsw_mtx, hz);
		if (sc->sc_dying) {
			sc->sc_rsw_lwp = NULL;
			cv_broadcast(&sc->sc_rsw_cv);
			mutex_exit(&sc->sc_rsw_mtx);
			kthread_exit(0);
		}
		wpi_getrfkill(sc);
	}
}

