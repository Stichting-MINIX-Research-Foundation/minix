/*	$NetBSD: if_iwn.c,v 1.76 2015/09/22 23:23:06 nonaka Exp $	*/
/*	$OpenBSD: if_iwn.c,v 1.135 2014/09/10 07:22:09 dcoppa Exp $	*/

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
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
 * Driver for Intel WiFi Link 4965 and 1000/5000/6000 Series 802.11 network
 * adapters.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_iwn.c,v 1.76 2015/09/22 23:23:06 nonaka Exp $");

#define IWN_USE_RBUF	/* Use local storage for RX */
#undef IWN_HWCRYPTO	/* XXX does not even compile yet */

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#ifdef notyetMODULE
#include <sys/module.h>
#endif
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/callout.h>

#include <dev/sysmon/sysmonvar.h>

#include <sys/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <net/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/pci/if_iwnreg.h>
#include <dev/pci/if_iwnvar.h>

static const pci_product_id_t iwn_devices[] = {
	PCI_PRODUCT_INTEL_WIFI_LINK_1030_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_1030_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_4965_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_4965_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_4965_3,
	PCI_PRODUCT_INTEL_WIFI_LINK_4965_4,
	PCI_PRODUCT_INTEL_WIFI_LINK_5100_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_5100_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_5150_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_5150_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_5300_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_5300_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_5350_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_5350_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_1000_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_1000_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_6000_3X3_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_6000_3X3_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_6050_2X2_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_6050_2X2_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_6005_2X2_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_6005_2X2_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_6230_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_6230_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_6235,
	PCI_PRODUCT_INTEL_WIFI_LINK_6235_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_100_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_100_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_130_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_130_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_2230_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_2230_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_2200_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_2200_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_135_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_135_2,
	PCI_PRODUCT_INTEL_WIFI_LINK_105_1,
	PCI_PRODUCT_INTEL_WIFI_LINK_105_2,
};

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset iwn_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset iwn_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset iwn_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

static int	iwn_match(device_t , struct cfdata *, void *);
static void	iwn_attach(device_t , device_t , void *);
static int	iwn4965_attach(struct iwn_softc *, pci_product_id_t);
static int	iwn5000_attach(struct iwn_softc *, pci_product_id_t);
static void	iwn_radiotap_attach(struct iwn_softc *);
static int	iwn_detach(device_t , int);
#if 0
static void	iwn_power(int, void *);
#endif
static bool	iwn_resume(device_t, const pmf_qual_t *);
static int	iwn_nic_lock(struct iwn_softc *);
static int	iwn_eeprom_lock(struct iwn_softc *);
static int	iwn_init_otprom(struct iwn_softc *);
static int	iwn_read_prom_data(struct iwn_softc *, uint32_t, void *, int);
static int	iwn_dma_contig_alloc(bus_dma_tag_t, struct iwn_dma_info *,
		    void **, bus_size_t, bus_size_t);
static void	iwn_dma_contig_free(struct iwn_dma_info *);
static int	iwn_alloc_sched(struct iwn_softc *);
static void	iwn_free_sched(struct iwn_softc *);
static int	iwn_alloc_kw(struct iwn_softc *);
static void	iwn_free_kw(struct iwn_softc *);
static int	iwn_alloc_ict(struct iwn_softc *);
static void	iwn_free_ict(struct iwn_softc *);
static int	iwn_alloc_fwmem(struct iwn_softc *);
static void	iwn_free_fwmem(struct iwn_softc *);
static int	iwn_alloc_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
static void	iwn_reset_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
static void	iwn_free_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
static int	iwn_alloc_tx_ring(struct iwn_softc *, struct iwn_tx_ring *,
		    int);
static void	iwn_reset_tx_ring(struct iwn_softc *, struct iwn_tx_ring *);
static void	iwn_free_tx_ring(struct iwn_softc *, struct iwn_tx_ring *);
static void	iwn5000_ict_reset(struct iwn_softc *);
static int	iwn_read_eeprom(struct iwn_softc *);
static void	iwn4965_read_eeprom(struct iwn_softc *);

#ifdef IWN_DEBUG
static void	iwn4965_print_power_group(struct iwn_softc *, int);
#endif
static void	iwn5000_read_eeprom(struct iwn_softc *);
static void	iwn_read_eeprom_channels(struct iwn_softc *, int, uint32_t);
static void	iwn_read_eeprom_enhinfo(struct iwn_softc *);
static struct	ieee80211_node *iwn_node_alloc(struct ieee80211_node_table *);
static void	iwn_newassoc(struct ieee80211_node *, int);
static int	iwn_media_change(struct ifnet *);
static int	iwn_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void	iwn_iter_func(void *, struct ieee80211_node *);
static void	iwn_calib_timeout(void *);
static void	iwn_rx_phy(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
static void	iwn_rx_done(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
#ifndef IEEE80211_NO_HT
static void	iwn_rx_compressed_ba(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
#endif
static void	iwn5000_rx_calib_results(struct iwn_softc *,
		    struct iwn_rx_desc *, struct iwn_rx_data *);
static void	iwn_rx_statistics(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
static void	iwn4965_tx_done(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
static void	iwn5000_tx_done(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
static void	iwn_tx_done(struct iwn_softc *, struct iwn_rx_desc *, int,
		    uint8_t);
static void	iwn_cmd_done(struct iwn_softc *, struct iwn_rx_desc *);
static void	iwn_notif_intr(struct iwn_softc *);
static void	iwn_wakeup_intr(struct iwn_softc *);
static void	iwn_fatal_intr(struct iwn_softc *);
static int	iwn_intr(void *);
static void	iwn4965_update_sched(struct iwn_softc *, int, int, uint8_t,
		    uint16_t);
static void	iwn5000_update_sched(struct iwn_softc *, int, int, uint8_t,
		    uint16_t);
#ifdef notyet
static void	iwn5000_reset_sched(struct iwn_softc *, int, int);
#endif
static int	iwn_tx(struct iwn_softc *, struct mbuf *,
		    struct ieee80211_node *, int);
static void	iwn_start(struct ifnet *);
static void	iwn_watchdog(struct ifnet *);
static int	iwn_ioctl(struct ifnet *, u_long, void *);
static int	iwn_cmd(struct iwn_softc *, int, const void *, int, int);
static int	iwn4965_add_node(struct iwn_softc *, struct iwn_node_info *,
		    int);
static int	iwn5000_add_node(struct iwn_softc *, struct iwn_node_info *,
		    int);
static int	iwn_set_link_quality(struct iwn_softc *,
		    struct ieee80211_node *);
static int	iwn_add_broadcast_node(struct iwn_softc *, int);
static void	iwn_set_led(struct iwn_softc *, uint8_t, uint8_t, uint8_t);
static int	iwn_set_critical_temp(struct iwn_softc *);
static int	iwn_set_timing(struct iwn_softc *, struct ieee80211_node *);
static void	iwn4965_power_calibration(struct iwn_softc *, int);
static int	iwn4965_set_txpower(struct iwn_softc *, int);
static int	iwn5000_set_txpower(struct iwn_softc *, int);
static int	iwn4965_get_rssi(const struct iwn_rx_stat *);
static int	iwn5000_get_rssi(const struct iwn_rx_stat *);
static int	iwn_get_noise(const struct iwn_rx_general_stats *);
static int	iwn4965_get_temperature(struct iwn_softc *);
static int	iwn5000_get_temperature(struct iwn_softc *);
static int	iwn_init_sensitivity(struct iwn_softc *);
static void	iwn_collect_noise(struct iwn_softc *,
		    const struct iwn_rx_general_stats *);
static int	iwn4965_init_gains(struct iwn_softc *);
static int	iwn5000_init_gains(struct iwn_softc *);
static int	iwn4965_set_gains(struct iwn_softc *);
static int	iwn5000_set_gains(struct iwn_softc *);
static void	iwn_tune_sensitivity(struct iwn_softc *,
		    const struct iwn_rx_stats *);
static int	iwn_send_sensitivity(struct iwn_softc *);
static int	iwn_set_pslevel(struct iwn_softc *, int, int, int);
static int	iwn5000_runtime_calib(struct iwn_softc *);

static int	iwn_config_bt_coex_bluetooth(struct iwn_softc *);
static int	iwn_config_bt_coex_prio_table(struct iwn_softc *);
static int	iwn_config_bt_coex_adv1(struct iwn_softc *);
static int	iwn_config_bt_coex_adv2(struct iwn_softc *);

static int	iwn_config(struct iwn_softc *);
static uint16_t	iwn_get_active_dwell_time(struct iwn_softc *, uint16_t,
		    uint8_t);
static uint16_t	iwn_limit_dwell(struct iwn_softc *, uint16_t);
static uint16_t	iwn_get_passive_dwell_time(struct iwn_softc *, uint16_t);
static int	iwn_scan(struct iwn_softc *, uint16_t);
static int	iwn_auth(struct iwn_softc *);
static int	iwn_run(struct iwn_softc *);
#ifdef IWN_HWCRYPTO
static int	iwn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
static void	iwn_delete_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
#endif
static int	iwn_wme_update(struct ieee80211com *);
#ifndef IEEE80211_NO_HT
static int	iwn_ampdu_rx_start(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
static void	iwn_ampdu_rx_stop(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
static int	iwn_ampdu_tx_start(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
static void	iwn_ampdu_tx_stop(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
static void	iwn4965_ampdu_tx_start(struct iwn_softc *,
		    struct ieee80211_node *, uint8_t, uint16_t);
static void	iwn4965_ampdu_tx_stop(struct iwn_softc *,
		    uint8_t, uint16_t);
static void	iwn5000_ampdu_tx_start(struct iwn_softc *,
		    struct ieee80211_node *, uint8_t, uint16_t);
static void	iwn5000_ampdu_tx_stop(struct iwn_softc *,
		    uint8_t, uint16_t);
#endif
static int	iwn5000_query_calibration(struct iwn_softc *);
static int	iwn5000_send_calibration(struct iwn_softc *);
static int	iwn5000_send_wimax_coex(struct iwn_softc *);
static int	iwn6000_temp_offset_calib(struct iwn_softc *);
static int	iwn2000_temp_offset_calib(struct iwn_softc *);
static int	iwn4965_post_alive(struct iwn_softc *);
static int	iwn5000_post_alive(struct iwn_softc *);
static int	iwn4965_load_bootcode(struct iwn_softc *, const uint8_t *,
		    int);
static int	iwn4965_load_firmware(struct iwn_softc *);
static int	iwn5000_load_firmware_section(struct iwn_softc *, uint32_t,
		    const uint8_t *, int);
static int	iwn5000_load_firmware(struct iwn_softc *);
static int	iwn_read_firmware_leg(struct iwn_softc *,
		    struct iwn_fw_info *);
static int	iwn_read_firmware_tlv(struct iwn_softc *,
		    struct iwn_fw_info *, uint16_t);
static int	iwn_read_firmware(struct iwn_softc *);
static int	iwn_clock_wait(struct iwn_softc *);
static int	iwn_apm_init(struct iwn_softc *);
static void	iwn_apm_stop_master(struct iwn_softc *);
static void	iwn_apm_stop(struct iwn_softc *);
static int	iwn4965_nic_config(struct iwn_softc *);
static int	iwn5000_nic_config(struct iwn_softc *);
static int	iwn_hw_prepare(struct iwn_softc *);
static int	iwn_hw_init(struct iwn_softc *);
static void	iwn_hw_stop(struct iwn_softc *);
static int	iwn_init(struct ifnet *);
static void	iwn_stop(struct ifnet *, int);

/* XXX MCLGETI alternative */
static struct	mbuf *MCLGETIalt(struct iwn_softc *, int,
		    struct ifnet *, u_int);
#ifdef IWN_USE_RBUF
static struct	iwn_rbuf *iwn_alloc_rbuf(struct iwn_softc *);
static void	iwn_free_rbuf(struct mbuf *, void *, size_t, void *);
static int	iwn_alloc_rpool(struct iwn_softc *);
static void	iwn_free_rpool(struct iwn_softc *);
#endif

/* XXX needed by iwn_scan */
static u_int8_t	*ieee80211_add_ssid(u_int8_t *, const u_int8_t *, u_int);
static u_int8_t	*ieee80211_add_rates(u_int8_t *,
    const struct ieee80211_rateset *);
static u_int8_t	*ieee80211_add_xrates(u_int8_t *,
    const struct ieee80211_rateset *);

static void	iwn_fix_channel(struct ieee80211com *, struct mbuf *,
		    struct iwn_rx_stat *);

#ifdef IWN_DEBUG
#define DPRINTF(x)	do { if (iwn_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (iwn_debug >= (n)) printf x; } while (0)
int iwn_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

CFATTACH_DECL_NEW(iwn, sizeof(struct iwn_softc), iwn_match, iwn_attach,
	iwn_detach, NULL);

static int
iwn_match(device_t parent, cfdata_t match __unused, void *aux)
{
	struct pci_attach_args *pa = aux;
	size_t i;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return 0;

	for (i = 0; i < __arraycount(iwn_devices); i++)
		if (PCI_PRODUCT(pa->pa_id) == iwn_devices[i])
			return 1;

	return 0;
}

static void
iwn_attach(device_t parent __unused, device_t self, void *aux)
{
	struct iwn_softc *sc = device_private(self);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	pci_intr_handle_t ih;
	pcireg_t memtype, reg;
	int i, error;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);

	callout_init(&sc->calib_to, 0);
	callout_setfunc(&sc->calib_to, iwn_calib_timeout, sc);

	pci_aprint_devinfo(pa, NULL);

	/*
	 * Get the offset of the PCI Express Capability Structure in PCI
	 * Configuration Space.
	 */
	error = pci_get_capability(sc->sc_pct, sc->sc_pcitag,
	    PCI_CAP_PCIEXPRESS, &sc->sc_cap_off, NULL);
	if (error == 0) {
		aprint_error_dev(self,
		    "PCIe capability structure not found!\n");
		return;
	}

	/* Clear device-specific "PCI retry timeout" register (41h). */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	if (reg & 0xff00)
		pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, reg & ~0xff00);

	/* Enable bus-mastering and hardware bug workaround. */
	/* XXX verify the bus-mastering is really needed (not in OpenBSD) */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	if (reg & PCI_COMMAND_INTERRUPT_DISABLE) {
		DPRINTF(("PCIe INTx Disable set\n"));
		reg &= ~PCI_COMMAND_INTERRUPT_DISABLE;
	}
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG, reg);

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, IWN_PCI_BAR0);
	error = pci_mapreg_map(pa, IWN_PCI_BAR0, memtype, 0, &sc->sc_st,
	    &sc->sc_sh, NULL, &sc->sc_sz);
	if (error != 0) {
		aprint_error_dev(self, "can't map mem space\n");
		return;
	}

	/* Install interrupt handler. */
	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(sc->sc_pct, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, iwn_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* Read hardware revision and attach. */
	sc->hw_type =
	    (IWN_READ(sc, IWN_HW_REV) & IWN_HW_REV_TYPE_MASK)
	      >> IWN_HW_REV_TYPE_SHIFT;
	if (sc->hw_type == IWN_HW_REV_TYPE_4965)
		error = iwn4965_attach(sc, PCI_PRODUCT(pa->pa_id));
	else
		error = iwn5000_attach(sc, PCI_PRODUCT(pa->pa_id));
	if (error != 0) {
		aprint_error_dev(self, "could not attach device\n");
		return;
	}	

	if ((error = iwn_hw_prepare(sc)) != 0) {
		aprint_error_dev(self, "hardware not ready\n");
		return;
	}

	/* Read MAC address, channels, etc from EEPROM. */
	if ((error = iwn_read_eeprom(sc)) != 0) {
		aprint_error_dev(self, "could not read EEPROM\n");
		return;
	}

	/* Allocate DMA memory for firmware transfers. */
	if ((error = iwn_alloc_fwmem(sc)) != 0) {
		aprint_error_dev(self,
		    "could not allocate memory for firmware\n");
		return;
	}

	/* Allocate "Keep Warm" page. */
	if ((error = iwn_alloc_kw(sc)) != 0) {
		aprint_error_dev(self, "could not allocate keep warm page\n");
		goto fail1;
	}

	/* Allocate ICT table for 5000 Series. */
	if (sc->hw_type != IWN_HW_REV_TYPE_4965 &&
	    (error = iwn_alloc_ict(sc)) != 0) {
		aprint_error_dev(self, "could not allocate ICT table\n");
		goto fail2;
	}

	/* Allocate TX scheduler "rings". */
	if ((error = iwn_alloc_sched(sc)) != 0) {
		aprint_error_dev(self,
		    "could not allocate TX scheduler rings\n");
		goto fail3;
	}

#ifdef IWN_USE_RBUF
	/* Allocate RX buffers. */
	if ((error = iwn_alloc_rpool(sc)) != 0) {
		aprint_error_dev(self, "could not allocate RX buffers\n");
		goto fail3;
	}
#endif

	/* Allocate TX rings (16 on 4965AGN, 20 on >=5000). */
	for (i = 0; i < sc->ntxqs; i++) {
		if ((error = iwn_alloc_tx_ring(sc, &sc->txq[i], i)) != 0) {
			aprint_error_dev(self,
			    "could not allocate TX ring %d\n", i);
			goto fail4;
		}
	}

	/* Allocate RX ring. */
	if ((error = iwn_alloc_rx_ring(sc, &sc->rxq)) != 0) {
		aprint_error_dev(self, "could not allocate RX ring\n");
		goto fail4;
	}

	/* Clear pending interrupts. */
	IWN_WRITE(sc, IWN_INT, 0xffffffff);

	/* Count the number of available chains. */
	sc->ntxchains =
	    ((sc->txchainmask >> 2) & 1) +
	    ((sc->txchainmask >> 1) & 1) +
	    ((sc->txchainmask >> 0) & 1);
	sc->nrxchains =
	    ((sc->rxchainmask >> 2) & 1) +
	    ((sc->rxchainmask >> 1) & 1) +
	    ((sc->rxchainmask >> 0) & 1);
	aprint_normal_dev(self, "MIMO %dT%dR, %.4s, address %s\n",
	    sc->ntxchains, sc->nrxchains, sc->eeprom_domain,
	    ether_sprintf(ic->ic_myaddr));

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	/* XXX OpenBSD has IEEE80211_C_WEP, IEEE80211_C_RSN,
	 * and IEEE80211_C_PMGT too. */
	ic->ic_caps =
	    IEEE80211_C_IBSS |		/* IBSS mode support */
	    IEEE80211_C_WPA |		/* 802.11i */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_WME;		/* 802.11e */

#ifndef IEEE80211_NO_HT
	if (sc->sc_flags & IWN_FLAG_HAS_11N) {
		/* Set HT capabilities. */
		ic->ic_htcaps =
#if IWN_RBUF_SIZE == 8192
		    IEEE80211_HTCAP_AMSDU7935 |
#endif
		    IEEE80211_HTCAP_CBW20_40 |
		    IEEE80211_HTCAP_SGI20 |
		    IEEE80211_HTCAP_SGI40;
		if (sc->hw_type != IWN_HW_REV_TYPE_4965)
			ic->ic_htcaps |= IEEE80211_HTCAP_GF;
		if (sc->hw_type == IWN_HW_REV_TYPE_6050)
			ic->ic_htcaps |= IEEE80211_HTCAP_SMPS_DYN;
		else
			ic->ic_htcaps |= IEEE80211_HTCAP_SMPS_DIS;
	}
#endif	/* !IEEE80211_NO_HT */

	/* Set supported legacy rates. */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = iwn_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = iwn_rateset_11g;
	if (sc->sc_flags & IWN_FLAG_HAS_5GHZ) {
		ic->ic_sup_rates[IEEE80211_MODE_11A] = iwn_rateset_11a;
	}
#ifndef IEEE80211_NO_HT
	if (sc->sc_flags & IWN_FLAG_HAS_11N) {
		/* Set supported HT rates. */
		ic->ic_sup_mcs[0] = 0xff;		/* MCS 0-7 */
		if (sc->nrxchains > 1)
			ic->ic_sup_mcs[1] = 0xff;	/* MCS 7-15 */
		if (sc->nrxchains > 2)
			ic->ic_sup_mcs[2] = 0xff;	/* MCS 16-23 */
	}
#endif

	/* IBSS channel undefined for now. */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = iwn_init;
	ifp->if_ioctl = iwn_ioctl;
	ifp->if_start = iwn_start;
	ifp->if_stop = iwn_stop;
	ifp->if_watchdog = iwn_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ic);
	ic->ic_node_alloc = iwn_node_alloc;
	ic->ic_newassoc = iwn_newassoc;
#ifdef IWN_HWCRYPTO
	ic->ic_crypto.cs_key_set = iwn_set_key;
	ic->ic_crypto.cs_key_delete = iwn_delete_key;
#endif
	ic->ic_wme.wme_update = iwn_wme_update;
#ifndef IEEE80211_NO_HT
	ic->ic_ampdu_rx_start = iwn_ampdu_rx_start;
	ic->ic_ampdu_rx_stop = iwn_ampdu_rx_stop;
	ic->ic_ampdu_tx_start = iwn_ampdu_tx_start;
	ic->ic_ampdu_tx_stop = iwn_ampdu_tx_stop;
#endif

	/* Override 802.11 state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwn_newstate;
	ieee80211_media_init(ic, iwn_media_change, ieee80211_media_status);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 15;

	iwn_radiotap_attach(sc);

	/*
	 * XXX for NetBSD, OpenBSD timeout_set replaced by
	 * callout_init and callout_setfunc, above.
	*/

	if (pmf_device_register(self, NULL, iwn_resume))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* XXX NetBSD add call to ieee80211_announce for dmesg. */
	ieee80211_announce(ic);

	return;

	/* Free allocated memory if something failed during attachment. */
fail4:	while (--i >= 0)
		iwn_free_tx_ring(sc, &sc->txq[i]);
#ifdef IWN_USE_RBUF
	iwn_free_rpool(sc);
#endif
	iwn_free_sched(sc);
fail3:	if (sc->ict != NULL)
		iwn_free_ict(sc);
fail2:	iwn_free_kw(sc);
fail1:	iwn_free_fwmem(sc);
}

int
iwn4965_attach(struct iwn_softc *sc, pci_product_id_t pid)
{
	struct iwn_ops *ops = &sc->ops;

	ops->load_firmware = iwn4965_load_firmware;
	ops->read_eeprom = iwn4965_read_eeprom;
	ops->post_alive = iwn4965_post_alive;
	ops->nic_config = iwn4965_nic_config;
	ops->config_bt_coex = iwn_config_bt_coex_bluetooth;
	ops->update_sched = iwn4965_update_sched;
	ops->get_temperature = iwn4965_get_temperature;
	ops->get_rssi = iwn4965_get_rssi;
	ops->set_txpower = iwn4965_set_txpower;
	ops->init_gains = iwn4965_init_gains;
	ops->set_gains = iwn4965_set_gains;
	ops->add_node = iwn4965_add_node;
	ops->tx_done = iwn4965_tx_done;
#ifndef IEEE80211_NO_HT
	ops->ampdu_tx_start = iwn4965_ampdu_tx_start;
	ops->ampdu_tx_stop = iwn4965_ampdu_tx_stop;
#endif
	sc->ntxqs = IWN4965_NTXQUEUES;
	sc->ndmachnls = IWN4965_NDMACHNLS;
	sc->broadcast_id = IWN4965_ID_BROADCAST;
	sc->rxonsz = IWN4965_RXONSZ;
	sc->schedsz = IWN4965_SCHEDSZ;
	sc->fw_text_maxsz = IWN4965_FW_TEXT_MAXSZ;
	sc->fw_data_maxsz = IWN4965_FW_DATA_MAXSZ;
	sc->fwsz = IWN4965_FWSZ;
	sc->sched_txfact_addr = IWN4965_SCHED_TXFACT;
	sc->limits = &iwn4965_sensitivity_limits;
	sc->fwname = "iwlwifi-4965-2.ucode";
	/* Override chains masks, ROM is known to be broken. */
	sc->txchainmask = IWN_ANT_AB;
	sc->rxchainmask = IWN_ANT_ABC;

	return 0;
}

int
iwn5000_attach(struct iwn_softc *sc, pci_product_id_t pid)
{
	struct iwn_ops *ops = &sc->ops;

	ops->load_firmware = iwn5000_load_firmware;
	ops->read_eeprom = iwn5000_read_eeprom;
	ops->post_alive = iwn5000_post_alive;
	ops->nic_config = iwn5000_nic_config;
	ops->config_bt_coex = iwn_config_bt_coex_bluetooth;
	ops->update_sched = iwn5000_update_sched;
	ops->get_temperature = iwn5000_get_temperature;
	ops->get_rssi = iwn5000_get_rssi;
	ops->set_txpower = iwn5000_set_txpower;
	ops->init_gains = iwn5000_init_gains;
	ops->set_gains = iwn5000_set_gains;
	ops->add_node = iwn5000_add_node;
	ops->tx_done = iwn5000_tx_done;
#ifndef IEEE80211_NO_HT
	ops->ampdu_tx_start = iwn5000_ampdu_tx_start;
	ops->ampdu_tx_stop = iwn5000_ampdu_tx_stop;
#endif
	sc->ntxqs = IWN5000_NTXQUEUES;
	sc->ndmachnls = IWN5000_NDMACHNLS;
	sc->broadcast_id = IWN5000_ID_BROADCAST;
	sc->rxonsz = IWN5000_RXONSZ;
	sc->schedsz = IWN5000_SCHEDSZ;
	sc->fw_text_maxsz = IWN5000_FW_TEXT_MAXSZ;
	sc->fw_data_maxsz = IWN5000_FW_DATA_MAXSZ;
	sc->fwsz = IWN5000_FWSZ;
	sc->sched_txfact_addr = IWN5000_SCHED_TXFACT;

	switch (sc->hw_type) {
	case IWN_HW_REV_TYPE_5100:
		sc->limits = &iwn5000_sensitivity_limits;
		sc->fwname = "iwlwifi-5000-2.ucode";
		/* Override chains masks, ROM is known to be broken. */
		sc->txchainmask = IWN_ANT_B;
		sc->rxchainmask = IWN_ANT_AB;
		break;
	case IWN_HW_REV_TYPE_5150:
		sc->limits = &iwn5150_sensitivity_limits;
		sc->fwname = "iwlwifi-5150-2.ucode";
		break;
	case IWN_HW_REV_TYPE_5300:
	case IWN_HW_REV_TYPE_5350:
		sc->limits = &iwn5000_sensitivity_limits;
		sc->fwname = "iwlwifi-5000-2.ucode";
		break;
	case IWN_HW_REV_TYPE_1000:
		sc->limits = &iwn1000_sensitivity_limits;
		if (pid == PCI_PRODUCT_INTEL_WIFI_LINK_100_1 ||
		    pid == PCI_PRODUCT_INTEL_WIFI_LINK_100_2)
			sc->fwname = "iwlwifi-100-5.ucode";
		else
			sc->fwname = "iwlwifi-1000-3.ucode";
		break;
	case IWN_HW_REV_TYPE_6000:
		sc->limits = &iwn6000_sensitivity_limits;
		sc->fwname = "iwlwifi-6000-4.ucode";
		if (pid == PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_1 ||
		    pid == PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_2) {
			sc->sc_flags |= IWN_FLAG_INTERNAL_PA;
			/* Override chains masks, ROM is known to be broken. */
			sc->txchainmask = IWN_ANT_BC;
			sc->rxchainmask = IWN_ANT_BC;
		}
		break;
	case IWN_HW_REV_TYPE_6050:
		sc->limits = &iwn6000_sensitivity_limits;
		sc->fwname = "iwlwifi-6050-5.ucode";
		break;
	case IWN_HW_REV_TYPE_6005:
		sc->limits = &iwn6000_sensitivity_limits;
		/* Type 6030 cards return IWN_HW_REV_TYPE_6005 */
		if (pid == PCI_PRODUCT_INTEL_WIFI_LINK_1030_1 ||
		    pid == PCI_PRODUCT_INTEL_WIFI_LINK_1030_2 ||
		    pid == PCI_PRODUCT_INTEL_WIFI_LINK_6230_1 ||
		    pid == PCI_PRODUCT_INTEL_WIFI_LINK_6230_2 ||
		    pid == PCI_PRODUCT_INTEL_WIFI_LINK_6235   ||
		    pid == PCI_PRODUCT_INTEL_WIFI_LINK_6235_2) {
			sc->fwname = "iwlwifi-6000g2b-6.ucode";
			ops->config_bt_coex = iwn_config_bt_coex_adv1;
		}
		else
			sc->fwname = "iwlwifi-6000g2a-5.ucode";
		break;
	case IWN_HW_REV_TYPE_2030:
		sc->limits = &iwn2000_sensitivity_limits;
		sc->fwname = "iwlwifi-2030-6.ucode";
		ops->config_bt_coex = iwn_config_bt_coex_adv2;
		break;
	case IWN_HW_REV_TYPE_2000:
		sc->limits = &iwn2000_sensitivity_limits;
		sc->fwname = "iwlwifi-2000-6.ucode";
		break;
	case IWN_HW_REV_TYPE_135:
		sc->limits = &iwn2000_sensitivity_limits;
		sc->fwname = "iwlwifi-135-6.ucode";
		ops->config_bt_coex = iwn_config_bt_coex_adv2;
		break;
	case IWN_HW_REV_TYPE_105:
		sc->limits = &iwn2000_sensitivity_limits;
		sc->fwname = "iwlwifi-105-6.ucode";
		break;
	default:
		aprint_normal(": adapter type %d not supported\n", sc->hw_type);
		return ENOTSUP;
	}
	return 0;
}

/*
 * Attach the interface to 802.11 radiotap.
 */
static void
iwn_radiotap_attach(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ic.ic_ifp;

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN,
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWN_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWN_TX_RADIOTAP_PRESENT);
}

static int
iwn_detach(device_t self, int flags __unused)
{
	struct iwn_softc *sc = device_private(self);
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	int qid;

	callout_stop(&sc->calib_to);

	/* Uninstall interrupt handler. */
	if (sc->sc_ih != NULL)
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);

	/* Free DMA resources. */
	iwn_free_rx_ring(sc, &sc->rxq);
	for (qid = 0; qid < sc->ntxqs; qid++)
		iwn_free_tx_ring(sc, &sc->txq[qid]);
#ifdef IWN_USE_RBUF
	iwn_free_rpool(sc);
#endif
	iwn_free_sched(sc);
	iwn_free_kw(sc);
	if (sc->ict != NULL)
		iwn_free_ict(sc);
	iwn_free_fwmem(sc);

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);

	ieee80211_ifdetach(&sc->sc_ic);
	if_detach(ifp);

	return 0;
}

#if 0
/*
 * XXX Investigate if clearing the PCI retry timeout could eliminate
 * the repeated scan calls.  Also the calls to if_init and if_start
 * are similar to the effect of adding the call to ifioctl_common .
 */
static void
iwn_power(int why, void *arg)
{
	struct iwn_softc *sc = arg;
	struct ifnet *ifp;
	pcireg_t reg;
	int s;

	if (why != PWR_RESUME)
		return;

	/* Clear device-specific "PCI retry timeout" register (41h). */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	if (reg & 0xff00)
		pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, reg & ~0xff00);

	s = splnet();
	ifp = &sc->sc_ic.ic_if;
	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp);
		if (ifp->if_flags & IFF_RUNNING)
			ifp->if_start(ifp);
	}
	splx(s);
}
#endif

static bool
iwn_resume(device_t dv, const pmf_qual_t *qual)
{
	return true;
}

static int
iwn_nic_lock(struct iwn_softc *sc)
{
	int ntries;

	/* Request exclusive access to NIC. */
	IWN_SETBITS(sc, IWN_GP_CNTRL, IWN_GP_CNTRL_MAC_ACCESS_REQ);

	/* Spin until we actually get the lock. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((IWN_READ(sc, IWN_GP_CNTRL) &
		     (IWN_GP_CNTRL_MAC_ACCESS_ENA | IWN_GP_CNTRL_SLEEP)) ==
		    IWN_GP_CNTRL_MAC_ACCESS_ENA)
			return 0;
		DELAY(10);
	}
	return ETIMEDOUT;
}

static __inline void
iwn_nic_unlock(struct iwn_softc *sc)
{
	IWN_CLRBITS(sc, IWN_GP_CNTRL, IWN_GP_CNTRL_MAC_ACCESS_REQ);
}

static __inline uint32_t
iwn_prph_read(struct iwn_softc *sc, uint32_t addr)
{
	IWN_WRITE(sc, IWN_PRPH_RADDR, IWN_PRPH_DWORD | addr);
	IWN_BARRIER_READ_WRITE(sc);
	return IWN_READ(sc, IWN_PRPH_RDATA);
}

static __inline void
iwn_prph_write(struct iwn_softc *sc, uint32_t addr, uint32_t data)
{
	IWN_WRITE(sc, IWN_PRPH_WADDR, IWN_PRPH_DWORD | addr);
	IWN_BARRIER_WRITE(sc);
	IWN_WRITE(sc, IWN_PRPH_WDATA, data);
}

static __inline void
iwn_prph_setbits(struct iwn_softc *sc, uint32_t addr, uint32_t mask)
{
	iwn_prph_write(sc, addr, iwn_prph_read(sc, addr) | mask);
}

static __inline void
iwn_prph_clrbits(struct iwn_softc *sc, uint32_t addr, uint32_t mask)
{
	iwn_prph_write(sc, addr, iwn_prph_read(sc, addr) & ~mask);
}

static __inline void
iwn_prph_write_region_4(struct iwn_softc *sc, uint32_t addr,
    const uint32_t *data, int count)
{
	for (; count > 0; count--, data++, addr += 4)
		iwn_prph_write(sc, addr, *data);
}

static __inline uint32_t
iwn_mem_read(struct iwn_softc *sc, uint32_t addr)
{
	IWN_WRITE(sc, IWN_MEM_RADDR, addr);
	IWN_BARRIER_READ_WRITE(sc);
	return IWN_READ(sc, IWN_MEM_RDATA);
}

static __inline void
iwn_mem_write(struct iwn_softc *sc, uint32_t addr, uint32_t data)
{
	IWN_WRITE(sc, IWN_MEM_WADDR, addr);
	IWN_BARRIER_WRITE(sc);
	IWN_WRITE(sc, IWN_MEM_WDATA, data);
}

#ifndef IEEE80211_NO_HT
static __inline void
iwn_mem_write_2(struct iwn_softc *sc, uint32_t addr, uint16_t data)
{
	uint32_t tmp;

	tmp = iwn_mem_read(sc, addr & ~3);
	if (addr & 3)
		tmp = (tmp & 0x0000ffff) | data << 16;
	else
		tmp = (tmp & 0xffff0000) | data;
	iwn_mem_write(sc, addr & ~3, tmp);
}
#endif

static __inline void
iwn_mem_read_region_4(struct iwn_softc *sc, uint32_t addr, uint32_t *data,
    int count)
{
	for (; count > 0; count--, addr += 4)
		*data++ = iwn_mem_read(sc, addr);
}

static __inline void
iwn_mem_set_region_4(struct iwn_softc *sc, uint32_t addr, uint32_t val,
    int count)
{
	for (; count > 0; count--, addr += 4)
		iwn_mem_write(sc, addr, val);
}

static int
iwn_eeprom_lock(struct iwn_softc *sc)
{
	int i, ntries;

	for (i = 0; i < 100; i++) {
		/* Request exclusive access to EEPROM. */
		IWN_SETBITS(sc, IWN_HW_IF_CONFIG,
		    IWN_HW_IF_CONFIG_EEPROM_LOCKED);

		/* Spin until we actually get the lock. */
		for (ntries = 0; ntries < 100; ntries++) {
			if (IWN_READ(sc, IWN_HW_IF_CONFIG) &
			    IWN_HW_IF_CONFIG_EEPROM_LOCKED)
				return 0;
			DELAY(10);
		}
	}
	return ETIMEDOUT;
}

static __inline void
iwn_eeprom_unlock(struct iwn_softc *sc)
{
	IWN_CLRBITS(sc, IWN_HW_IF_CONFIG, IWN_HW_IF_CONFIG_EEPROM_LOCKED);
}

/*
 * Initialize access by host to One Time Programmable ROM.
 * NB: This kind of ROM can be found on 1000 or 6000 Series only.
 */
static int
iwn_init_otprom(struct iwn_softc *sc)
{
	uint16_t prev = 0, base, next;
	int count, error;

	/* Wait for clock stabilization before accessing prph. */
	if ((error = iwn_clock_wait(sc)) != 0)
		return error;

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	iwn_prph_setbits(sc, IWN_APMG_PS, IWN_APMG_PS_RESET_REQ);
	DELAY(5);
	iwn_prph_clrbits(sc, IWN_APMG_PS, IWN_APMG_PS_RESET_REQ);
	iwn_nic_unlock(sc);

	/* Set auto clock gate disable bit for HW with OTP shadow RAM. */
	if (sc->hw_type != IWN_HW_REV_TYPE_1000) {
		IWN_SETBITS(sc, IWN_DBG_LINK_PWR_MGMT,
		    IWN_RESET_LINK_PWR_MGMT_DIS);
	}
	IWN_CLRBITS(sc, IWN_EEPROM_GP, IWN_EEPROM_GP_IF_OWNER);
	/* Clear ECC status. */
	IWN_SETBITS(sc, IWN_OTP_GP,
	    IWN_OTP_GP_ECC_CORR_STTS | IWN_OTP_GP_ECC_UNCORR_STTS);

	/*
	 * Find the block before last block (contains the EEPROM image)
	 * for HW without OTP shadow RAM.
	 */
	if (sc->hw_type == IWN_HW_REV_TYPE_1000) {
		/* Switch to absolute addressing mode. */
		IWN_CLRBITS(sc, IWN_OTP_GP, IWN_OTP_GP_RELATIVE_ACCESS);
		base = 0;
		for (count = 0; count < IWN1000_OTP_NBLOCKS; count++) {
			error = iwn_read_prom_data(sc, base, &next, 2);
			if (error != 0)
				return error;
			if (next == 0)	/* End of linked-list. */
				break;
			prev = base;
			base = le16toh(next);
		}
		if (count == 0 || count == IWN1000_OTP_NBLOCKS)
			return EIO;
		/* Skip "next" word. */
		sc->prom_base = prev + 1;
	}
	return 0;
}

static int
iwn_read_prom_data(struct iwn_softc *sc, uint32_t addr, void *data, int count)
{
	uint8_t *out = data;
	uint32_t val, tmp;
	int ntries;

	addr += sc->prom_base;
	for (; count > 0; count -= 2, addr++) {
		IWN_WRITE(sc, IWN_EEPROM, addr << 2);
		for (ntries = 0; ntries < 10; ntries++) {
			val = IWN_READ(sc, IWN_EEPROM);
			if (val & IWN_EEPROM_READ_VALID)
				break;
			DELAY(5);
		}
		if (ntries == 10) {
			aprint_error_dev(sc->sc_dev,
			    "timeout reading ROM at 0x%x\n", addr);
			return ETIMEDOUT;
		}
		if (sc->sc_flags & IWN_FLAG_HAS_OTPROM) {
			/* OTPROM, check for ECC errors. */
			tmp = IWN_READ(sc, IWN_OTP_GP);
			if (tmp & IWN_OTP_GP_ECC_UNCORR_STTS) {
				aprint_error_dev(sc->sc_dev,
				    "OTPROM ECC error at 0x%x\n", addr);
				return EIO;
			}
			if (tmp & IWN_OTP_GP_ECC_CORR_STTS) {
				/* Correctable ECC error, clear bit. */
				IWN_SETBITS(sc, IWN_OTP_GP,
				    IWN_OTP_GP_ECC_CORR_STTS);
			}
		}
		*out++ = val >> 16;
		if (count > 1)
			*out++ = val >> 24;
	}
	return 0;
}

static int
iwn_dma_contig_alloc(bus_dma_tag_t tag, struct iwn_dma_info *dma, void **kvap,
    bus_size_t size, bus_size_t alignment)
{
	int nsegs, error;

	dma->tag = tag;
	dma->size = size;

	error = bus_dmamap_create(tag, size, 1, size, 0, BUS_DMA_NOWAIT,
	    &dma->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(tag, size, alignment, 0, &dma->seg, 1, &nsegs,
	    BUS_DMA_NOWAIT); /* XXX OpenBSD adds BUS_DMA_ZERO */
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(tag, &dma->seg, 1, size, &dma->vaddr,
	    BUS_DMA_NOWAIT); /* XXX OpenBSD adds BUS_DMA_COHERENT */
	if (error != 0)
		goto fail;

	error = bus_dmamap_load(tag, dma->map, dma->vaddr, size, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;

	/* XXX Presumably needed because of missing BUS_DMA_ZERO, above. */
	memset(dma->vaddr, 0, size);
	bus_dmamap_sync(tag, dma->map, 0, size, BUS_DMASYNC_PREWRITE);

	dma->paddr = dma->map->dm_segs[0].ds_addr;
	if (kvap != NULL)
		*kvap = dma->vaddr;

	return 0;

fail:	iwn_dma_contig_free(dma);
	return error;
}

static void
iwn_dma_contig_free(struct iwn_dma_info *dma)
{
	if (dma->map != NULL) {
		if (dma->vaddr != NULL) {
			bus_dmamap_sync(dma->tag, dma->map, 0, dma->size,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(dma->tag, dma->map);
			bus_dmamem_unmap(dma->tag, dma->vaddr, dma->size);
			bus_dmamem_free(dma->tag, &dma->seg, 1);
			dma->vaddr = NULL;
		}
		bus_dmamap_destroy(dma->tag, dma->map);
		dma->map = NULL;
	}
}

static int
iwn_alloc_sched(struct iwn_softc *sc)
{
	/* TX scheduler rings must be aligned on a 1KB boundary. */
	return iwn_dma_contig_alloc(sc->sc_dmat, &sc->sched_dma,
	    (void **)&sc->sched, sc->schedsz, 1024);
}

static void
iwn_free_sched(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->sched_dma);
}

static int
iwn_alloc_kw(struct iwn_softc *sc)
{
	/* "Keep Warm" page must be aligned on a 4KB boundary. */
	return iwn_dma_contig_alloc(sc->sc_dmat, &sc->kw_dma, NULL, 4096,
	    4096);
}

static void
iwn_free_kw(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->kw_dma);
}

static int
iwn_alloc_ict(struct iwn_softc *sc)
{
	/* ICT table must be aligned on a 4KB boundary. */
	return iwn_dma_contig_alloc(sc->sc_dmat, &sc->ict_dma,
	    (void **)&sc->ict, IWN_ICT_SIZE, 4096);
}

static void
iwn_free_ict(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->ict_dma);
}

static int
iwn_alloc_fwmem(struct iwn_softc *sc)
{
	/* Must be aligned on a 16-byte boundary. */
	return iwn_dma_contig_alloc(sc->sc_dmat, &sc->fw_dma, NULL,
	    sc->fwsz, 16);
}

static void
iwn_free_fwmem(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->fw_dma);
}

static int
iwn_alloc_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	bus_size_t size;
	int i, error;

	ring->cur = 0;

	/* Allocate RX descriptors (256-byte aligned). */
	size = IWN_RX_RING_COUNT * sizeof (uint32_t);
	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, size, 256);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate RX ring DMA memory\n");
		goto fail;
	}

	/* Allocate RX status area (16-byte aligned). */
	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->stat_dma,
	    (void **)&ring->stat, sizeof (struct iwn_rx_status), 16);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate RX status DMA memory\n");
		goto fail;
	}

	/*
	 * Allocate and map RX buffers.
	 */
	for (i = 0; i < IWN_RX_RING_COUNT; i++) {
		struct iwn_rx_data *data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, IWN_RBUF_SIZE, 1,
		    IWN_RBUF_SIZE, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &data->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create RX buf DMA map\n");
			goto fail;
		}

		data->m = MCLGETIalt(sc, M_DONTWAIT, NULL, IWN_RBUF_SIZE);
		if (data->m == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate RX mbuf\n");
			error = ENOBUFS;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), IWN_RBUF_SIZE, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "can't not map mbuf (error %d)\n", error);
			goto fail;
		}

		/* Set physical address of RX buffer (256-byte aligned). */
		ring->desc[i] = htole32(data->map->dm_segs[0].ds_addr >> 8);
	}

	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0, size,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	iwn_free_rx_ring(sc, ring);
	return error;
}

static void
iwn_reset_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int ntries;

	if (iwn_nic_lock(sc) == 0) {
		IWN_WRITE(sc, IWN_FH_RX_CONFIG, 0);
		for (ntries = 0; ntries < 1000; ntries++) {
			if (IWN_READ(sc, IWN_FH_RX_STATUS) &
			    IWN_FH_RX_STATUS_IDLE)
				break;
			DELAY(10);
		}
		iwn_nic_unlock(sc);
	}
	ring->cur = 0;
	sc->last_rx_valid = 0;
}

static void
iwn_free_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int i;

	iwn_dma_contig_free(&ring->desc_dma);
	iwn_dma_contig_free(&ring->stat_dma);

	for (i = 0; i < IWN_RX_RING_COUNT; i++) {
		struct iwn_rx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
		}
		if (data->map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

static int
iwn_alloc_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring, int qid)
{
	bus_addr_t paddr;
	bus_size_t size;
	int i, error;

	ring->qid = qid;
	ring->queued = 0;
	ring->cur = 0;

	/* Allocate TX descriptors (256-byte aligned). */
	size = IWN_TX_RING_COUNT * sizeof (struct iwn_tx_desc);
	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma,
	    (void **)&ring->desc, size, 256);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate TX ring DMA memory\n");
		goto fail;
	}
	/*
	 * We only use rings 0 through 4 (4 EDCA + cmd) so there is no need
	 * to allocate commands space for other rings.
	 * XXX Do we really need to allocate descriptors for other rings?
	 */
	if (qid > 4)
		return 0;

	size = IWN_TX_RING_COUNT * sizeof (struct iwn_tx_cmd);
	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->cmd_dma,
	    (void **)&ring->cmd, size, 4);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate TX cmd DMA memory\n");
		goto fail;
	}

	paddr = ring->cmd_dma.paddr;
	for (i = 0; i < IWN_TX_RING_COUNT; i++) {
		struct iwn_tx_data *data = &ring->data[i];

		data->cmd_paddr = paddr;
		data->scratch_paddr = paddr + 12;
		paddr += sizeof (struct iwn_tx_cmd);

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    IWN_MAX_SCATTER - 1, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &data->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not create TX buf DMA map\n");
			goto fail;
		}
	}
	return 0;

fail:	iwn_free_tx_ring(sc, ring);
	return error;
}

static void
iwn_reset_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring)
{
	int i;

	for (i = 0; i < IWN_TX_RING_COUNT; i++) {
		struct iwn_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}
	/* Clear TX descriptors. */
	memset(ring->desc, 0, ring->desc_dma.size);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map, 0,
	    ring->desc_dma.size, BUS_DMASYNC_PREWRITE);
	sc->qfullmsk &= ~(1 << ring->qid);
	ring->queued = 0;
	ring->cur = 0;
}

static void
iwn_free_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring)
{
	int i;

	iwn_dma_contig_free(&ring->desc_dma);
	iwn_dma_contig_free(&ring->cmd_dma);

	for (i = 0; i < IWN_TX_RING_COUNT; i++) {
		struct iwn_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
		}
		if (data->map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, data->map);
	}
}

static void
iwn5000_ict_reset(struct iwn_softc *sc)
{
	/* Disable interrupts. */
	IWN_WRITE(sc, IWN_INT_MASK, 0);

	/* Reset ICT table. */
	memset(sc->ict, 0, IWN_ICT_SIZE);
	sc->ict_cur = 0;

	/* Set physical address of ICT table (4KB aligned). */
	DPRINTF(("enabling ICT\n"));
	IWN_WRITE(sc, IWN_DRAM_INT_TBL, IWN_DRAM_INT_TBL_ENABLE |
	    IWN_DRAM_INT_TBL_WRAP_CHECK | sc->ict_dma.paddr >> 12);

	/* Enable periodic RX interrupt. */
	sc->int_mask |= IWN_INT_RX_PERIODIC;
	/* Switch to ICT interrupt mode in driver. */
	sc->sc_flags |= IWN_FLAG_USE_ICT;

	/* Re-enable interrupts. */
	IWN_WRITE(sc, IWN_INT, 0xffffffff);
	IWN_WRITE(sc, IWN_INT_MASK, sc->int_mask);
}

static int
iwn_read_eeprom(struct iwn_softc *sc)
{
	struct iwn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;
	int error;

	/* Check whether adapter has an EEPROM or an OTPROM. */
	if (sc->hw_type >= IWN_HW_REV_TYPE_1000 &&
	    (IWN_READ(sc, IWN_OTP_GP) & IWN_OTP_GP_DEV_SEL_OTP))
		sc->sc_flags |= IWN_FLAG_HAS_OTPROM;
	DPRINTF(("%s found\n", (sc->sc_flags & IWN_FLAG_HAS_OTPROM) ?
	    "OTPROM" : "EEPROM"));

	/* Adapter has to be powered on for EEPROM access to work. */
	if ((error = iwn_apm_init(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not power ON adapter\n");
		return error;
	}

	if ((IWN_READ(sc, IWN_EEPROM_GP) & 0x7) == 0) {
		aprint_error_dev(sc->sc_dev,
		    "bad ROM signature\n");
		return EIO;
	}
	if ((error = iwn_eeprom_lock(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not lock ROM (error=%d)\n", error);
		return error;
	}
	if (sc->sc_flags & IWN_FLAG_HAS_OTPROM) {
		if ((error = iwn_init_otprom(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not initialize OTPROM\n");
			return error;
		}
	}

	iwn_read_prom_data(sc, IWN_EEPROM_SKU_CAP, &val, 2);
	DPRINTF(("SKU capabilities=0x%04x\n", le16toh(val)));
	/* Check if HT support is bonded out. */
	if (val & htole16(IWN_EEPROM_SKU_CAP_11N))
		sc->sc_flags |= IWN_FLAG_HAS_11N;

	iwn_read_prom_data(sc, IWN_EEPROM_RFCFG, &val, 2);
	sc->rfcfg = le16toh(val);
	DPRINTF(("radio config=0x%04x\n", sc->rfcfg));
	/* Read Tx/Rx chains from ROM unless it's known to be broken. */
	if (sc->txchainmask == 0)
		sc->txchainmask = IWN_RFCFG_TXANTMSK(sc->rfcfg);
	if (sc->rxchainmask == 0)
		sc->rxchainmask = IWN_RFCFG_RXANTMSK(sc->rfcfg);

	/* Read MAC address. */
	iwn_read_prom_data(sc, IWN_EEPROM_MAC, ic->ic_myaddr, 6);

	/* Read adapter-specific information from EEPROM. */
	ops->read_eeprom(sc);

	iwn_apm_stop(sc);	/* Power OFF adapter. */

	iwn_eeprom_unlock(sc);
	return 0;
}

static void
iwn4965_read_eeprom(struct iwn_softc *sc)
{
	uint32_t addr;
	uint16_t val;
	int i;

	/* Read regulatory domain (4 ASCII characters). */
	iwn_read_prom_data(sc, IWN4965_EEPROM_DOMAIN, sc->eeprom_domain, 4);

	/* Read the list of authorized channels (20MHz ones only). */
	for (i = 0; i < 5; i++) {
		addr = iwn4965_regulatory_bands[i];
		iwn_read_eeprom_channels(sc, i, addr);
	}

	/* Read maximum allowed TX power for 2GHz and 5GHz bands. */
	iwn_read_prom_data(sc, IWN4965_EEPROM_MAXPOW, &val, 2);
	sc->maxpwr2GHz = val & 0xff;
	sc->maxpwr5GHz = val >> 8;
	/* Check that EEPROM values are within valid range. */
	if (sc->maxpwr5GHz < 20 || sc->maxpwr5GHz > 50)
		sc->maxpwr5GHz = 38;
	if (sc->maxpwr2GHz < 20 || sc->maxpwr2GHz > 50)
		sc->maxpwr2GHz = 38;
	DPRINTF(("maxpwr 2GHz=%d 5GHz=%d\n", sc->maxpwr2GHz, sc->maxpwr5GHz));

	/* Read samples for each TX power group. */
	iwn_read_prom_data(sc, IWN4965_EEPROM_BANDS, sc->bands,
	    sizeof sc->bands);

	/* Read voltage at which samples were taken. */
	iwn_read_prom_data(sc, IWN4965_EEPROM_VOLTAGE, &val, 2);
	sc->eeprom_voltage = (int16_t)le16toh(val);
	DPRINTF(("voltage=%d (in 0.3V)\n", sc->eeprom_voltage));

#ifdef IWN_DEBUG
	/* Print samples. */
	if (iwn_debug > 0) {
		for (i = 0; i < IWN_NBANDS; i++)
			iwn4965_print_power_group(sc, i);
	}
#endif
}

#ifdef IWN_DEBUG
static void
iwn4965_print_power_group(struct iwn_softc *sc, int i)
{
	struct iwn4965_eeprom_band *band = &sc->bands[i];
	struct iwn4965_eeprom_chan_samples *chans = band->chans;
	int j, c;

	aprint_normal("===band %d===\n", i);
	aprint_normal("chan lo=%d, chan hi=%d\n", band->lo, band->hi);
	aprint_normal("chan1 num=%d\n", chans[0].num);
	for (c = 0; c < 2; c++) {
		for (j = 0; j < IWN_NSAMPLES; j++) {
			aprint_normal("chain %d, sample %d: temp=%d gain=%d "
			    "power=%d pa_det=%d\n", c, j,
			    chans[0].samples[c][j].temp,
			    chans[0].samples[c][j].gain,
			    chans[0].samples[c][j].power,
			    chans[0].samples[c][j].pa_det);
		}
	}
	aprint_normal("chan2 num=%d\n", chans[1].num);
	for (c = 0; c < 2; c++) {
		for (j = 0; j < IWN_NSAMPLES; j++) {
			aprint_normal("chain %d, sample %d: temp=%d gain=%d "
			    "power=%d pa_det=%d\n", c, j,
			    chans[1].samples[c][j].temp,
			    chans[1].samples[c][j].gain,
			    chans[1].samples[c][j].power,
			    chans[1].samples[c][j].pa_det);
		}
	}
}
#endif

static void
iwn5000_read_eeprom(struct iwn_softc *sc)
{
	struct iwn5000_eeprom_calib_hdr hdr;
	int32_t volt;
	uint32_t base, addr;
	uint16_t val;
	int i;

	/* Read regulatory domain (4 ASCII characters). */
	iwn_read_prom_data(sc, IWN5000_EEPROM_REG, &val, 2);
	base = le16toh(val);
	iwn_read_prom_data(sc, base + IWN5000_EEPROM_DOMAIN,
	    sc->eeprom_domain, 4);

	/* Read the list of authorized channels (20MHz ones only). */
	for (i = 0; i < 5; i++) {
		addr = base + iwn5000_regulatory_bands[i];
		iwn_read_eeprom_channels(sc, i, addr);
	}

	/* Read enhanced TX power information for 6000 Series. */
	if (sc->hw_type >= IWN_HW_REV_TYPE_6000)
		iwn_read_eeprom_enhinfo(sc);

	iwn_read_prom_data(sc, IWN5000_EEPROM_CAL, &val, 2);
	base = le16toh(val);
	iwn_read_prom_data(sc, base, &hdr, sizeof hdr);
	DPRINTF(("calib version=%u pa type=%u voltage=%u\n",
	    hdr.version, hdr.pa_type, le16toh(hdr.volt)));
	sc->calib_ver = hdr.version;

	if (sc->hw_type == IWN_HW_REV_TYPE_2030 ||
	    sc->hw_type == IWN_HW_REV_TYPE_2000 ||
	    sc->hw_type == IWN_HW_REV_TYPE_135  ||
	    sc->hw_type == IWN_HW_REV_TYPE_105) {
		sc->eeprom_voltage = le16toh(hdr.volt);
		iwn_read_prom_data(sc, base + IWN5000_EEPROM_TEMP, &val, 2);
		sc->eeprom_temp = le16toh(val);
		iwn_read_prom_data(sc, base + IWN2000_EEPROM_RAWTEMP, &val, 2);
		sc->eeprom_rawtemp = le16toh(val);
	}

	if (sc->hw_type == IWN_HW_REV_TYPE_5150) {
		/* Compute temperature offset. */
		iwn_read_prom_data(sc, base + IWN5000_EEPROM_TEMP, &val, 2);
		sc->eeprom_temp = le16toh(val);
		iwn_read_prom_data(sc, base + IWN5000_EEPROM_VOLT, &val, 2);
		volt = le16toh(val);
		sc->temp_off = sc->eeprom_temp - (volt / -5);
		DPRINTF(("temp=%d volt=%d offset=%dK\n",
		    sc->eeprom_temp, volt, sc->temp_off));
	} else {
		/* Read crystal calibration. */
		iwn_read_prom_data(sc, base + IWN5000_EEPROM_CRYSTAL,
		    &sc->eeprom_crystal, sizeof (uint32_t));
		DPRINTF(("crystal calibration 0x%08x\n",
		    le32toh(sc->eeprom_crystal)));
	}
}

static void
iwn_read_eeprom_channels(struct iwn_softc *sc, int n, uint32_t addr)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct iwn_chan_band *band = &iwn_bands[n];
	struct iwn_eeprom_chan channels[IWN_MAX_CHAN_PER_BAND];
	uint8_t chan;
	int i;

	iwn_read_prom_data(sc, addr, channels,
	    band->nchan * sizeof (struct iwn_eeprom_chan));

	for (i = 0; i < band->nchan; i++) {
		if (!(channels[i].flags & IWN_EEPROM_CHAN_VALID))
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
			 * Some adapters support channels 7, 8, 11 and 12
			 * both in the 2GHz and 4.9GHz bands.
			 * Because of limitations in our net80211 layer,
			 * we don't support them in the 4.9GHz band.
			 */
			if (chan <= 14)
				continue;

			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A;
			/* We have at least one valid 5GHz channel. */
			sc->sc_flags |= IWN_FLAG_HAS_5GHZ;
		}

		/* Is active scan allowed on this channel? */
		if (!(channels[i].flags & IWN_EEPROM_CHAN_ACTIVE)) {
			ic->ic_channels[chan].ic_flags |=
			    IEEE80211_CHAN_PASSIVE;
		}

		/* Save maximum allowed TX power for this channel. */
		sc->maxpwr[chan] = channels[i].maxpwr;

		DPRINTF(("adding chan %d flags=0x%x maxpwr=%d\n",
		    chan, channels[i].flags, sc->maxpwr[chan]));
	}
}

static void
iwn_read_eeprom_enhinfo(struct iwn_softc *sc)
{
	struct iwn_eeprom_enhinfo enhinfo[35];
	uint16_t val, base;
	int8_t maxpwr;
	int i;

	iwn_read_prom_data(sc, IWN5000_EEPROM_REG, &val, 2);
	base = le16toh(val);
	iwn_read_prom_data(sc, base + IWN6000_EEPROM_ENHINFO,
	    enhinfo, sizeof enhinfo);

	memset(sc->enh_maxpwr, 0, sizeof sc->enh_maxpwr);
	for (i = 0; i < __arraycount(enhinfo); i++) {
		if (enhinfo[i].chan == 0 || enhinfo[i].reserved != 0)
			continue;	/* Skip invalid entries. */

		maxpwr = 0;
		if (sc->txchainmask & IWN_ANT_A)
			maxpwr = MAX(maxpwr, enhinfo[i].chain[0]);
		if (sc->txchainmask & IWN_ANT_B)
			maxpwr = MAX(maxpwr, enhinfo[i].chain[1]);
		if (sc->txchainmask & IWN_ANT_C)
			maxpwr = MAX(maxpwr, enhinfo[i].chain[2]);
		if (sc->ntxchains == 2)
			maxpwr = MAX(maxpwr, enhinfo[i].mimo2);
		else if (sc->ntxchains == 3)
			maxpwr = MAX(maxpwr, enhinfo[i].mimo3);
		maxpwr /= 2;	/* Convert half-dBm to dBm. */

		DPRINTF(("enhinfo %d, maxpwr=%d\n", i, maxpwr));
		sc->enh_maxpwr[i] = maxpwr;
	}
}

static struct ieee80211_node *
iwn_node_alloc(struct ieee80211_node_table *ic __unused)
{
	return malloc(sizeof (struct iwn_node), M_80211_NODE, M_NOWAIT | M_ZERO);
}

static void
iwn_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct iwn_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	struct iwn_node *wn = (void *)ni;
	uint8_t rate;
	int ridx, i;

	ieee80211_amrr_node_init(&sc->amrr, &wn->amn);
	/* Start at lowest available bit-rate, AMRR will raise. */
	ni->ni_txrate = 0;

	for (i = 0; i < ni->ni_rates.rs_nrates; i++) {
		rate = ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL;
		/* Map 802.11 rate to HW rate index. */
		for (ridx = 0; ridx <= IWN_RIDX_MAX; ridx++)
			if (iwn_rates[ridx].rate == rate)
				break;
		wn->ridx[i] = ridx;
	}
}

static int
iwn_media_change(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t rate, ridx;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if (ic->ic_fixed_rate != -1) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		/* Map 802.11 rate to HW rate index. */
		for (ridx = 0; ridx <= IWN_RIDX_MAX; ridx++)
			if (iwn_rates[ridx].rate == rate)
				break;
		sc->fixed_ridx = ridx;
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		iwn_stop(ifp, 0);
		error = iwn_init(ifp);
	}
	return error;
}

static int
iwn_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwn_softc *sc = ifp->if_softc;
	int error;

	callout_stop(&sc->calib_to);

	switch (nstate) {
	case IEEE80211_S_SCAN:
		/* XXX Do not abort a running scan. */
		if (sc->sc_flags & IWN_FLAG_SCANNING) {
			if (ic->ic_state != nstate)
				aprint_error_dev(sc->sc_dev, "scan request(%d) "
				    "while scanning(%d) ignored\n", nstate,
				    ic->ic_state);
			break;
		}

		/* XXX Not sure if call and flags are needed. */
		ieee80211_node_table_reset(&ic->ic_scan);
		ic->ic_flags |= IEEE80211_F_SCAN | IEEE80211_F_ASCAN;
		sc->sc_flags |= IWN_FLAG_SCANNING_2GHZ;

		/* Make the link LED blink while we're scanning. */
		iwn_set_led(sc, IWN_LED_LINK, 10, 10);

		if ((error = iwn_scan(sc, IEEE80211_CHAN_2GHZ)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not initiate scan\n");
			return error;
		}
		ic->ic_state = nstate;
		return 0;

	case IEEE80211_S_ASSOC:
		if (ic->ic_state != IEEE80211_S_RUN)
			break;
		/* FALLTHROUGH */
	case IEEE80211_S_AUTH:
		/* Reset state to handle reassociations correctly. */
		sc->rxon.associd = 0;
		sc->rxon.filter &= ~htole32(IWN_FILTER_BSS);
		sc->calib.state = IWN_CALIB_STATE_INIT;

		if ((error = iwn_auth(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not move to auth state\n");
			return error;
		}
		break;

	case IEEE80211_S_RUN:
		if ((error = iwn_run(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not move to run state\n");
			return error;
		}
		break;

	case IEEE80211_S_INIT:
		sc->sc_flags &= ~IWN_FLAG_SCANNING;
		sc->calib.state = IWN_CALIB_STATE_INIT;
		break;
	}

	return sc->sc_newstate(ic, nstate, arg);
}

static void
iwn_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct iwn_softc *sc = arg;
	struct iwn_node *wn = (struct iwn_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &wn->amn);
}

static void
iwn_calib_timeout(void *arg)
{
	struct iwn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	if (ic->ic_fixed_rate == -1) {
		if (ic->ic_opmode == IEEE80211_M_STA)
			iwn_iter_func(sc, ic->ic_bss);
		else
			ieee80211_iterate_nodes(&ic->ic_sta, iwn_iter_func, sc);
	}
	/* Force automatic TX power calibration every 60 secs. */
	if (++sc->calib_cnt >= 120) {
		uint32_t flags = 0;

		DPRINTF(("sending request for statistics\n"));
		(void)iwn_cmd(sc, IWN_CMD_GET_STATISTICS, &flags,
		    sizeof flags, 1);
		sc->calib_cnt = 0;
	}
	splx(s);

	/* Automatic rate control triggered every 500ms. */
	callout_schedule(&sc->calib_to, hz/2);
}

/*
 * Process an RX_PHY firmware notification.  This is usually immediately
 * followed by an MPDU_RX_DONE notification.
 */
static void
iwn_rx_phy(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct iwn_rx_stat *stat = (struct iwn_rx_stat *)(desc + 1);

	DPRINTFN(2, ("received PHY stats\n"));
	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
	    sizeof (*stat), BUS_DMASYNC_POSTREAD);

	/* Save RX statistics, they will be used on MPDU_RX_DONE. */
	memcpy(&sc->last_rx_stat, stat, sizeof (*stat));
	sc->last_rx_valid = 1;
}

/*
 * Process an RX_DONE (4965AGN only) or MPDU_RX_DONE firmware notification.
 * Each MPDU_RX_DONE notification must be preceded by an RX_PHY one.
 */
static void
iwn_rx_done(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct iwn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwn_rx_ring *ring = &sc->rxq;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m, *m1;
	struct iwn_rx_stat *stat;
	char	*head;
	uint32_t flags;
	int error, len, rssi;

	if (desc->type == IWN_MPDU_RX_DONE) {
		/* Check for prior RX_PHY notification. */
		if (!sc->last_rx_valid) {
			DPRINTF(("missing RX_PHY\n"));
			return;
		}
		sc->last_rx_valid = 0;
		stat = &sc->last_rx_stat;
	} else
		stat = (struct iwn_rx_stat *)(desc + 1);

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, IWN_RBUF_SIZE,
	    BUS_DMASYNC_POSTREAD);

	if (stat->cfg_phy_len > IWN_STAT_MAXLEN) {
		aprint_error_dev(sc->sc_dev,
		    "invalid RX statistic header\n");
		return;
	}
	if (desc->type == IWN_MPDU_RX_DONE) {
		struct iwn_rx_mpdu *mpdu = (struct iwn_rx_mpdu *)(desc + 1);
		head = (char *)(mpdu + 1);
		len = le16toh(mpdu->len);
	} else {
		head = (char *)(stat + 1) + stat->cfg_phy_len;
		len = le16toh(stat->len);
	}

	flags = le32toh(*(uint32_t *)(head + len));

	/* Discard frames with a bad FCS early. */
	if ((flags & IWN_RX_NOERROR) != IWN_RX_NOERROR) {
		DPRINTFN(2, ("RX flags error %x\n", flags));
		ifp->if_ierrors++;
		return;
	}
	/* Discard frames that are too short. */
	if (len < sizeof (*wh)) {
		DPRINTF(("frame too short: %d\n", len));
		ic->ic_stats.is_rx_tooshort++;
		ifp->if_ierrors++;
		return;
	}

	m1 = MCLGETIalt(sc, M_DONTWAIT, NULL, IWN_RBUF_SIZE);
	if (m1 == NULL) {
		ic->ic_stats.is_rx_nobuf++;
		ifp->if_ierrors++;
		return;
	}
	bus_dmamap_unload(sc->sc_dmat, data->map);

	error = bus_dmamap_load(sc->sc_dmat, data->map, mtod(m1, void *),
	    IWN_RBUF_SIZE, NULL, BUS_DMA_NOWAIT | BUS_DMA_READ);
	if (error != 0) {
		m_freem(m1);

		/* Try to reload the old mbuf. */
		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), IWN_RBUF_SIZE, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error != 0) {
			panic("%s: could not load old RX mbuf",
			    device_xname(sc->sc_dev));
		}
		/* Physical address may have changed. */
		ring->desc[ring->cur] =
		    htole32(data->map->dm_segs[0].ds_addr >> 8);
		bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map,
		    ring->cur * sizeof (uint32_t), sizeof (uint32_t),
		    BUS_DMASYNC_PREWRITE);
		ifp->if_ierrors++;
		return;
	}

	m = data->m;
	data->m = m1;
	/* Update RX descriptor. */
	ring->desc[ring->cur] = htole32(data->map->dm_segs[0].ds_addr >> 8);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map,
	    ring->cur * sizeof (uint32_t), sizeof (uint32_t),
	    BUS_DMASYNC_PREWRITE);

	/* Finalize mbuf. */
	m->m_pkthdr.rcvif = ifp;
	m->m_data = head;
	m->m_pkthdr.len = m->m_len = len;

	/* Grab a reference to the source node. */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* XXX OpenBSD adds decryption here (see also comments in iwn_tx). */
	/* NetBSD does decryption in ieee80211_input. */

	rssi = ops->get_rssi(stat);

	/* XXX Added for NetBSD: scans never stop without it */
	if (ic->ic_state == IEEE80211_S_SCAN)
		iwn_fix_channel(ic, m, stat);

	if (sc->sc_drvbpf != NULL) {
		struct iwn_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		if (stat->flags & htole16(IWN_STAT_FLAG_SHPREAMBLE))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[stat->chan].ic_freq);
		tap->wr_chan_flags =
		    htole16(ic->ic_channels[stat->chan].ic_flags);
		tap->wr_dbm_antsignal = (int8_t)rssi;
		tap->wr_dbm_antnoise = (int8_t)sc->noise;
		tap->wr_tsft = stat->tstamp;
		switch (stat->rate) {
		/* CCK rates. */
		case  10: tap->wr_rate =   2; break;
		case  20: tap->wr_rate =   4; break;
		case  55: tap->wr_rate =  11; break;
		case 110: tap->wr_rate =  22; break;
		/* OFDM rates. */
		case 0xd: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0x5: tap->wr_rate =  24; break;
		case 0x7: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xb: tap->wr_rate =  72; break;
		case 0x1: tap->wr_rate =  96; break;
		case 0x3: tap->wr_rate = 108; break;
		/* Unknown rate: should not happen. */
		default:  tap->wr_rate =   0;
		}

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	/* Send the frame to the 802.11 layer. */
	ieee80211_input(ic, m, ni, rssi, 0);

	/* Node is no longer needed. */
	ieee80211_free_node(ni);
}

#ifndef IEEE80211_NO_HT
/* Process an incoming Compressed BlockAck. */
static void
iwn_rx_compressed_ba(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct iwn_compressed_ba *ba = (struct iwn_compressed_ba *)(desc + 1);
	struct iwn_tx_ring *txq;

	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc), sizeof (*ba),
	    BUS_DMASYNC_POSTREAD);

	txq = &sc->txq[le16toh(ba->qid)];
	/* XXX TBD */
}
#endif

/*
 * Process a CALIBRATION_RESULT notification sent by the initialization
 * firmware on response to a CMD_CALIB_CONFIG command (5000 only).
 */
static void
iwn5000_rx_calib_results(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct iwn_phy_calib *calib = (struct iwn_phy_calib *)(desc + 1);
	int len, idx = -1;

	/* Runtime firmware should not send such a notification. */
	if (sc->sc_flags & IWN_FLAG_CALIB_DONE)
		return;

	len = (le32toh(desc->len) & 0x3fff) - 4;
	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc), len,
	    BUS_DMASYNC_POSTREAD);

	switch (calib->code) {
	case IWN5000_PHY_CALIB_DC:
		if (sc->hw_type == IWN_HW_REV_TYPE_5150 ||
		    sc->hw_type == IWN_HW_REV_TYPE_2030 ||
		    sc->hw_type == IWN_HW_REV_TYPE_2000 ||
		    sc->hw_type == IWN_HW_REV_TYPE_135  ||
		    sc->hw_type == IWN_HW_REV_TYPE_105)
			idx = 0;
		break;
	case IWN5000_PHY_CALIB_LO:
		idx = 1;
		break;
	case IWN5000_PHY_CALIB_TX_IQ:
		idx = 2;
		break;
	case IWN5000_PHY_CALIB_TX_IQ_PERIODIC:
		if (sc->hw_type < IWN_HW_REV_TYPE_6000 &&
		    sc->hw_type != IWN_HW_REV_TYPE_5150)
			idx = 3;
		break;
	case IWN5000_PHY_CALIB_BASE_BAND:
		idx = 4;
		break;
	}
	if (idx == -1)	/* Ignore other results. */
		return;

	/* Save calibration result. */
	if (sc->calibcmd[idx].buf != NULL)
		free(sc->calibcmd[idx].buf, M_DEVBUF);
	sc->calibcmd[idx].buf = malloc(len, M_DEVBUF, M_NOWAIT);
	if (sc->calibcmd[idx].buf == NULL) {
		DPRINTF(("not enough memory for calibration result %d\n",
		    calib->code));
		return;
	}
	DPRINTF(("saving calibration result code=%d len=%d\n",
	    calib->code, len));
	sc->calibcmd[idx].len = len;
	memcpy(sc->calibcmd[idx].buf, calib, len);
}

/*
 * Process an RX_STATISTICS or BEACON_STATISTICS firmware notification.
 * The latter is sent by the firmware after each received beacon.
 */
static void
iwn_rx_statistics(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct iwn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_stats *stats = (struct iwn_stats *)(desc + 1);
	int temp;

	/* Ignore statistics received during a scan. */
	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
	    sizeof (*stats), BUS_DMASYNC_POSTREAD);

	DPRINTFN(3, ("received statistics (cmd=%d)\n", desc->type));
	sc->calib_cnt = 0;	/* Reset TX power calibration timeout. */

	/* Test if temperature has changed. */
	if (stats->general.temp != sc->rawtemp) {
		/* Convert "raw" temperature to degC. */
		sc->rawtemp = stats->general.temp;
		temp = ops->get_temperature(sc);
		DPRINTFN(2, ("temperature=%dC\n", temp));

		/* Update TX power if need be (4965AGN only). */
		if (sc->hw_type == IWN_HW_REV_TYPE_4965)
			iwn4965_power_calibration(sc, temp);
	}

	if (desc->type != IWN_BEACON_STATISTICS)
		return;	/* Reply to a statistics request. */

	sc->noise = iwn_get_noise(&stats->rx.general);

	/* Test that RSSI and noise are present in stats report. */
	if (le32toh(stats->rx.general.flags) != 1) {
		DPRINTF(("received statistics without RSSI\n"));
		return;
	}

	/*
	 * XXX Differential gain calibration makes the 6005 firmware
	 * crap out, so skip it for now.  This effectively disables
	 * sensitivity tuning as well.
	 */
	if (sc->hw_type == IWN_HW_REV_TYPE_6005)
		return;

	if (calib->state == IWN_CALIB_STATE_ASSOC)
		iwn_collect_noise(sc, &stats->rx.general);
	else if (calib->state == IWN_CALIB_STATE_RUN)
		iwn_tune_sensitivity(sc, &stats->rx);
}

/*
 * Process a TX_DONE firmware notification.  Unfortunately, the 4965AGN
 * and 5000 adapters have different incompatible TX status formats.
 */
static void
iwn4965_tx_done(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct iwn4965_tx_stat *stat = (struct iwn4965_tx_stat *)(desc + 1);

	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
	    sizeof (*stat), BUS_DMASYNC_POSTREAD);
	iwn_tx_done(sc, desc, stat->ackfailcnt, le32toh(stat->status) & 0xff);
}

static void
iwn5000_tx_done(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct iwn5000_tx_stat *stat = (struct iwn5000_tx_stat *)(desc + 1);

#ifdef notyet
	/* Reset TX scheduler slot. */
	iwn5000_reset_sched(sc, desc->qid & 0xf, desc->idx);
#endif

	bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
	    sizeof (*stat), BUS_DMASYNC_POSTREAD);
	iwn_tx_done(sc, desc, stat->ackfailcnt, le16toh(stat->status) & 0xff);
}

/*
 * Adapter-independent backend for TX_DONE firmware notifications.
 */
static void
iwn_tx_done(struct iwn_softc *sc, struct iwn_rx_desc *desc, int ackfailcnt,
    uint8_t status)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwn_tx_ring *ring = &sc->txq[desc->qid & 0xf];
	struct iwn_tx_data *data = &ring->data[desc->idx];
	struct iwn_node *wn = (struct iwn_node *)data->ni;

	/* Update rate control statistics. */
	wn->amn.amn_txcnt++;
	if (ackfailcnt > 0)
		wn->amn.amn_retrycnt++;

	if (status != 1 && status != 2)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	/* Unmap and free mbuf. */
	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, data->map);
	m_freem(data->m);
	data->m = NULL;
	ieee80211_free_node(data->ni);
	data->ni = NULL;

	sc->sc_tx_timer = 0;
	if (--ring->queued < IWN_TX_RING_LOMARK) {
		sc->qfullmsk &= ~(1 << ring->qid);
		if (sc->qfullmsk == 0 && (ifp->if_flags & IFF_OACTIVE)) {
			ifp->if_flags &= ~IFF_OACTIVE;
			(*ifp->if_start)(ifp);
		}
	}
}

/*
 * Process a "command done" firmware notification.  This is where we wakeup
 * processes waiting for a synchronous command completion.
 */
static void
iwn_cmd_done(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_data *data;

	if ((desc->qid & 0xf) != 4)
		return;	/* Not a command ack. */

	data = &ring->data[desc->idx];

	/* If the command was mapped in an mbuf, free it. */
	if (data->m != NULL) {
		bus_dmamap_sync(sc->sc_dmat, data->map, 0,
		    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}
	wakeup(&ring->desc[desc->idx]);
}

/*
 * Process an INT_FH_RX or INT_SW_RX interrupt.
 */
static void
iwn_notif_intr(struct iwn_softc *sc)
{
	struct iwn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	uint16_t hw;

	bus_dmamap_sync(sc->sc_dmat, sc->rxq.stat_dma.map,
	    0, sc->rxq.stat_dma.size, BUS_DMASYNC_POSTREAD);

	hw = le16toh(sc->rxq.stat->closed_count) & 0xfff;
	while (sc->rxq.cur != hw) {
		struct iwn_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		struct iwn_rx_desc *desc;

		bus_dmamap_sync(sc->sc_dmat, data->map, 0, sizeof (*desc),
		    BUS_DMASYNC_POSTREAD);
		desc = mtod(data->m, struct iwn_rx_desc *);

		DPRINTFN(4, ("notification qid=%d idx=%d flags=%x type=%d\n",
		    desc->qid & 0xf, desc->idx, desc->flags, desc->type));

		if (!(desc->qid & 0x80))	/* Reply to a command. */
			iwn_cmd_done(sc, desc);

		switch (desc->type) {
		case IWN_RX_PHY:
			iwn_rx_phy(sc, desc, data);
			break;

		case IWN_RX_DONE:		/* 4965AGN only. */
		case IWN_MPDU_RX_DONE:
			/* An 802.11 frame has been received. */
			iwn_rx_done(sc, desc, data);
			break;
#ifndef IEEE80211_NO_HT
		case IWN_RX_COMPRESSED_BA:
			/* A Compressed BlockAck has been received. */
			iwn_rx_compressed_ba(sc, desc, data);
			break;
#endif
		case IWN_TX_DONE:
			/* An 802.11 frame has been transmitted. */
			ops->tx_done(sc, desc, data);
			break;

		case IWN_RX_STATISTICS:
		case IWN_BEACON_STATISTICS:
			iwn_rx_statistics(sc, desc, data);
			break;

		case IWN_BEACON_MISSED:
		{
			struct iwn_beacon_missed *miss =
			    (struct iwn_beacon_missed *)(desc + 1);

			bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
			    sizeof (*miss), BUS_DMASYNC_POSTREAD);
			/*
			 * If more than 5 consecutive beacons are missed,
			 * reinitialize the sensitivity state machine.
			 */
			DPRINTF(("beacons missed %d/%d\n",
			    le32toh(miss->consecutive), le32toh(miss->total)));
			if (ic->ic_state == IEEE80211_S_RUN &&
			    le32toh(miss->consecutive) > 5)
				(void)iwn_init_sensitivity(sc);
			break;
		}
		case IWN_UC_READY:
		{
			struct iwn_ucode_info *uc =
			    (struct iwn_ucode_info *)(desc + 1);

			/* The microcontroller is ready. */
			bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
			    sizeof (*uc), BUS_DMASYNC_POSTREAD);
			DPRINTF(("microcode alive notification version=%d.%d "
			    "subtype=%x alive=%x\n", uc->major, uc->minor,
			    uc->subtype, le32toh(uc->valid)));

			if (le32toh(uc->valid) != 1) {
				aprint_error_dev(sc->sc_dev,
				    "microcontroller initialization "
				    "failed\n");
				break;
			}
			if (uc->subtype == IWN_UCODE_INIT) {
				/* Save microcontroller report. */
				memcpy(&sc->ucode_info, uc, sizeof (*uc));
			}
			/* Save the address of the error log in SRAM. */
			sc->errptr = le32toh(uc->errptr);
			break;
		}
		case IWN_STATE_CHANGED:
		{
			uint32_t *status = (uint32_t *)(desc + 1);

			/* Enabled/disabled notification. */
			bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
			    sizeof (*status), BUS_DMASYNC_POSTREAD);
			DPRINTF(("state changed to %x\n", le32toh(*status)));

			if (le32toh(*status) & 1) {
				/* The radio button has to be pushed. */
				aprint_error_dev(sc->sc_dev,
				    "Radio transmitter is off\n");
				/* Turn the interface down. */
				ifp->if_flags &= ~IFF_UP;
				iwn_stop(ifp, 1);
				return;	/* No further processing. */
			}
			break;
		}
		case IWN_START_SCAN:
		{
			struct iwn_start_scan *scan =
			    (struct iwn_start_scan *)(desc + 1);

			bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
			    sizeof (*scan), BUS_DMASYNC_POSTREAD);
			DPRINTFN(2, ("scanning channel %d status %x\n",
			    scan->chan, le32toh(scan->status)));

			/* Fix current channel. */
			ic->ic_bss->ni_chan = &ic->ic_channels[scan->chan];
			break;
		}
		case IWN_STOP_SCAN:
		{
			struct iwn_stop_scan *scan =
			    (struct iwn_stop_scan *)(desc + 1);

			bus_dmamap_sync(sc->sc_dmat, data->map, sizeof (*desc),
			    sizeof (*scan), BUS_DMASYNC_POSTREAD);
			DPRINTF(("scan finished nchan=%d status=%d chan=%d\n",
			    scan->nchan, scan->status, scan->chan));

			if (scan->status == 1 && scan->chan <= 14 &&
			    (sc->sc_flags & IWN_FLAG_HAS_5GHZ)) {
				/*
				 * We just finished scanning 2GHz channels,
				 * start scanning 5GHz ones.
				 */
				sc->sc_flags &= ~IWN_FLAG_SCANNING_2GHZ;
				sc->sc_flags |= IWN_FLAG_SCANNING_5GHZ;
				if (iwn_scan(sc, IEEE80211_CHAN_5GHZ) == 0)
					break;
			}
			sc->sc_flags &= ~IWN_FLAG_SCANNING;
			ieee80211_end_scan(ic);
			break;
		}
		case IWN5000_CALIBRATION_RESULT:
			iwn5000_rx_calib_results(sc, desc, data);
			break;

		case IWN5000_CALIBRATION_DONE:
			sc->sc_flags |= IWN_FLAG_CALIB_DONE;
			wakeup(sc);
			break;
		}

		sc->rxq.cur = (sc->rxq.cur + 1) % IWN_RX_RING_COUNT;
	}

	/* Tell the firmware what we have processed. */
	hw = (hw == 0) ? IWN_RX_RING_COUNT - 1 : hw - 1;
	IWN_WRITE(sc, IWN_FH_RX_WPTR, hw & ~7);
}

/*
 * Process an INT_WAKEUP interrupt raised when the microcontroller wakes up
 * from power-down sleep mode.
 */
static void
iwn_wakeup_intr(struct iwn_softc *sc)
{
	int qid;

	DPRINTF(("ucode wakeup from power-down sleep\n"));

	/* Wakeup RX and TX rings. */
	IWN_WRITE(sc, IWN_FH_RX_WPTR, sc->rxq.cur & ~7);
	for (qid = 0; qid < sc->ntxqs; qid++) {
		struct iwn_tx_ring *ring = &sc->txq[qid];
		IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, qid << 8 | ring->cur);
	}
}

/*
 * Dump the error log of the firmware when a firmware panic occurs.  Although
 * we can't debug the firmware because it is neither open source nor free, it
 * can help us to identify certain classes of problems.
 */
static void
iwn_fatal_intr(struct iwn_softc *sc)
{
	struct iwn_fw_dump dump;
	int i;

	/* Force a complete recalibration on next init. */
	sc->sc_flags &= ~IWN_FLAG_CALIB_DONE;

	/* Check that the error log address is valid. */
	if (sc->errptr < IWN_FW_DATA_BASE ||
	    sc->errptr + sizeof (dump) >
	    IWN_FW_DATA_BASE + sc->fw_data_maxsz) {
		aprint_error_dev(sc->sc_dev,
		    "bad firmware error log address 0x%08x\n", sc->errptr);
		return;
	}
	if (iwn_nic_lock(sc) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not read firmware error log\n");
		return;
	}
	/* Read firmware error log from SRAM. */
	iwn_mem_read_region_4(sc, sc->errptr, (uint32_t *)&dump,
	    sizeof (dump) / sizeof (uint32_t));
	iwn_nic_unlock(sc);

	if (dump.valid == 0) {
		aprint_error_dev(sc->sc_dev,
		    "firmware error log is empty\n");
		return;
	}
	aprint_error("firmware error log:\n");
	aprint_error("  error type      = \"%s\" (0x%08X)\n",
	    (dump.id < __arraycount(iwn_fw_errmsg)) ?
		iwn_fw_errmsg[dump.id] : "UNKNOWN",
	    dump.id);
	aprint_error("  program counter = 0x%08X\n", dump.pc);
	aprint_error("  source line     = 0x%08X\n", dump.src_line);
	aprint_error("  error data      = 0x%08X%08X\n",
	    dump.error_data[0], dump.error_data[1]);
	aprint_error("  branch link     = 0x%08X%08X\n",
	    dump.branch_link[0], dump.branch_link[1]);
	aprint_error("  interrupt link  = 0x%08X%08X\n",
	    dump.interrupt_link[0], dump.interrupt_link[1]);
	aprint_error("  time            = %u\n", dump.time[0]);

	/* Dump driver status (TX and RX rings) while we're here. */
	aprint_error("driver status:\n");
	for (i = 0; i < sc->ntxqs; i++) {
		struct iwn_tx_ring *ring = &sc->txq[i];
		aprint_error("  tx ring %2d: qid=%-2d cur=%-3d queued=%-3d\n",
		    i, ring->qid, ring->cur, ring->queued);
	}
	aprint_error("  rx ring: cur=%d\n", sc->rxq.cur);
	aprint_error("  802.11 state %d\n", sc->sc_ic.ic_state);
}

static int
iwn_intr(void *arg)
{
	struct iwn_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	uint32_t r1, r2, tmp;

	/* Disable interrupts. */
	IWN_WRITE(sc, IWN_INT_MASK, 0);

	/* Read interrupts from ICT (fast) or from registers (slow). */
	if (sc->sc_flags & IWN_FLAG_USE_ICT) {
		tmp = 0;
		while (sc->ict[sc->ict_cur] != 0) {
			tmp |= sc->ict[sc->ict_cur];
			sc->ict[sc->ict_cur] = 0;	/* Acknowledge. */
			sc->ict_cur = (sc->ict_cur + 1) % IWN_ICT_COUNT;
		}
		tmp = le32toh(tmp);
		if (tmp == 0xffffffff)	/* Shouldn't happen. */
			tmp = 0;
		else if (tmp & 0xc0000)	/* Workaround a HW bug. */
			tmp |= 0x8000;
		r1 = (tmp & 0xff00) << 16 | (tmp & 0xff);
		r2 = 0;	/* Unused. */
	} else {
		r1 = IWN_READ(sc, IWN_INT);
		if (r1 == 0xffffffff || (r1 & 0xfffffff0) == 0xa5a5a5a0)
			return 0;	/* Hardware gone! */
		r2 = IWN_READ(sc, IWN_FH_INT);
	}
	if (r1 == 0 && r2 == 0) {
		if (ifp->if_flags & IFF_UP)
			IWN_WRITE(sc, IWN_INT_MASK, sc->int_mask);
		return 0;	/* Interrupt not for us. */
	}

	/* Acknowledge interrupts. */
	IWN_WRITE(sc, IWN_INT, r1);
	if (!(sc->sc_flags & IWN_FLAG_USE_ICT))
		IWN_WRITE(sc, IWN_FH_INT, r2);

	if (r1 & IWN_INT_RF_TOGGLED) {
		tmp = IWN_READ(sc, IWN_GP_CNTRL);
		aprint_error_dev(sc->sc_dev,
		    "RF switch: radio %s\n",
		    (tmp & IWN_GP_CNTRL_RFKILL) ? "enabled" : "disabled");
	}
	if (r1 & IWN_INT_CT_REACHED) {
		aprint_error_dev(sc->sc_dev,
		    "critical temperature reached!\n");
	}
	if (r1 & (IWN_INT_SW_ERR | IWN_INT_HW_ERR)) {
		aprint_error_dev(sc->sc_dev,
		    "fatal firmware error\n");
		/* Dump firmware error log and stop. */
		iwn_fatal_intr(sc);
		ifp->if_flags &= ~IFF_UP;
		iwn_stop(ifp, 1);
		return 1;
	}
	if ((r1 & (IWN_INT_FH_RX | IWN_INT_SW_RX | IWN_INT_RX_PERIODIC)) ||
	    (r2 & IWN_FH_INT_RX)) {
		if (sc->sc_flags & IWN_FLAG_USE_ICT) {
			if (r1 & (IWN_INT_FH_RX | IWN_INT_SW_RX))
				IWN_WRITE(sc, IWN_FH_INT, IWN_FH_INT_RX);
			IWN_WRITE_1(sc, IWN_INT_PERIODIC,
			    IWN_INT_PERIODIC_DIS);
			iwn_notif_intr(sc);
			if (r1 & (IWN_INT_FH_RX | IWN_INT_SW_RX)) {
				IWN_WRITE_1(sc, IWN_INT_PERIODIC,
				    IWN_INT_PERIODIC_ENA);
			}
		} else
			iwn_notif_intr(sc);
	}

	if ((r1 & IWN_INT_FH_TX) || (r2 & IWN_FH_INT_TX)) {
		if (sc->sc_flags & IWN_FLAG_USE_ICT)
			IWN_WRITE(sc, IWN_FH_INT, IWN_FH_INT_TX);
		wakeup(sc);	/* FH DMA transfer completed. */
	}

	if (r1 & IWN_INT_ALIVE)
		wakeup(sc);	/* Firmware is alive. */

	if (r1 & IWN_INT_WAKEUP)
		iwn_wakeup_intr(sc);

	/* Re-enable interrupts. */
	if (ifp->if_flags & IFF_UP)
		IWN_WRITE(sc, IWN_INT_MASK, sc->int_mask);

	return 1;
}

/*
 * Update TX scheduler ring when transmitting an 802.11 frame (4965AGN and
 * 5000 adapters use a slightly different format).
 */
static void
iwn4965_update_sched(struct iwn_softc *sc, int qid, int idx, uint8_t id,
    uint16_t len)
{
	uint16_t *w = &sc->sched[qid * IWN4965_SCHED_COUNT + idx];

	*w = htole16(len + 8);
	bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
	    (char *)(void *)w - (char *)(void *)sc->sched_dma.vaddr,
	    sizeof (uint16_t),
	    BUS_DMASYNC_PREWRITE);
	if (idx < IWN_SCHED_WINSZ) {
		*(w + IWN_TX_RING_COUNT) = *w;
		bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
		    (char *)(void *)(w + IWN_TX_RING_COUNT) -
		    (char *)(void *)sc->sched_dma.vaddr,
		    sizeof (uint16_t), BUS_DMASYNC_PREWRITE);
	}
}

static void
iwn5000_update_sched(struct iwn_softc *sc, int qid, int idx, uint8_t id,
    uint16_t len)
{
	uint16_t *w = &sc->sched[qid * IWN5000_SCHED_COUNT + idx];

	*w = htole16(id << 12 | (len + 8));
	bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
	    (char *)(void *)w - (char *)(void *)sc->sched_dma.vaddr,
	    sizeof (uint16_t), BUS_DMASYNC_PREWRITE);
	if (idx < IWN_SCHED_WINSZ) {
		*(w + IWN_TX_RING_COUNT) = *w;
		bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
		    (char *)(void *)(w + IWN_TX_RING_COUNT) -
		    (char *)(void *)sc->sched_dma.vaddr,
		    sizeof (uint16_t), BUS_DMASYNC_PREWRITE);
	}
}

#ifdef notyet
static void
iwn5000_reset_sched(struct iwn_softc *sc, int qid, int idx)
{
	uint16_t *w = &sc->sched[qid * IWN5000_SCHED_COUNT + idx];

	*w = (*w & htole16(0xf000)) | htole16(1);
	bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
	    (char *)(void *)w - (char *)(void *)sc->sched_dma.vaddr,
	    sizeof (uint16_t), BUS_DMASYNC_PREWRITE);
	if (idx < IWN_SCHED_WINSZ) {
		*(w + IWN_TX_RING_COUNT) = *w;
		bus_dmamap_sync(sc->sc_dmat, sc->sched_dma.map,
		    (char *)(void *)(w + IWN_TX_RING_COUNT) -
		    (char *)(void *)sc->sched_dma.vaddr,
		    sizeof (uint16_t), BUS_DMASYNC_PREWRITE);
	}
}
#endif

static int
iwn_tx(struct iwn_softc *sc, struct mbuf *m, struct ieee80211_node *ni, int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_node *wn = (void *)ni;
	struct iwn_tx_ring *ring;
	struct iwn_tx_desc *desc;
	struct iwn_tx_data *data;
	struct iwn_tx_cmd *cmd;
	struct iwn_cmd_data *tx;
	const struct iwn_rate *rinfo;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct mbuf *m1;
	uint32_t flags;
	u_int hdrlen;
	bus_dma_segment_t *seg;
	uint8_t tid, ridx, txant, type;
	int i, totlen, error, pad;

	const struct chanAccParams *cap;
	int noack;
	int hdrlen2;

	wh = mtod(m, struct ieee80211_frame *);
	hdrlen = ieee80211_anyhdrsize(wh);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	hdrlen2 = (ieee80211_has_qos(wh)) ?
	    sizeof (struct ieee80211_qosframe) :
	    sizeof (struct ieee80211_frame);

	if (hdrlen != hdrlen2)
	    aprint_error_dev(sc->sc_dev, "hdrlen error (%d != %d)\n",
		hdrlen, hdrlen2);

	/* XXX OpenBSD sets a different tid when using QOS */
	tid = 0;
	if (ieee80211_has_qos(wh)) {
		cap = &ic->ic_wme.wme_chanParams;
		noack = cap->cap_wmeParams[ac].wmep_noackPolicy;
	}
	else
		noack = 0;

	ring = &sc->txq[ac];
	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	/* Choose a TX rate index. */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA) {
		ridx = (ic->ic_curmode == IEEE80211_MODE_11A) ?
		    IWN_RIDX_OFDM6 : IWN_RIDX_CCK1;
	} else if (ic->ic_fixed_rate != -1) {
		ridx = sc->fixed_ridx;
	} else
		ridx = wn->ridx[ni->ni_txrate];
	rinfo = &iwn_rates[ridx];

	/* Encrypt the frame if need be. */
	/*
	 * XXX For now, NetBSD swaps the encryption and bpf sections
	 * in order to match old code and other drivers. Tests with
	 * tcpdump indicates that the order is irrelevant, however,
	 * as bpf produces unencrypted data for both ordering choices.
	 */
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m);
		if (k == NULL) {
			m_freem(m);
			return ENOBUFS;
		}
		/* Packet header may have moved, reset our local pointer. */
		wh = mtod(m, struct ieee80211_frame *);
	}
	totlen = m->m_pkthdr.len;

	if (sc->sc_drvbpf != NULL) {
		struct iwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ni->ni_chan->ic_flags);
		tap->wt_rate = rinfo->rate;
		tap->wt_hwqueue = ac;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m);
	}

	/* Prepare TX firmware command. */
	cmd = &ring->cmd[ring->cur];
	cmd->code = IWN_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct iwn_cmd_data *)cmd->data;
	/* NB: No need to clear tx, all fields are reinitialized here. */
	tx->scratch = 0;	/* clear "scratch" area */

	flags = 0;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* Unicast frame, check if an ACK is expected. */
		if (!noack)
			flags |= IWN_TX_NEED_ACK;
	}

#ifdef notyet
	/* XXX NetBSD does not define IEEE80211_FC0_SUBTYPE_BAR */
	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_BAR))
		flags |= IWN_TX_IMM_BA;		/* Cannot happen yet. */
#endif         

	if (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG)
		flags |= IWN_TX_MORE_FRAG;	/* Cannot happen yet. */

	/* Check if frame must be protected using RTS/CTS or CTS-to-self. */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* NB: Group frames are sent using CCK in 802.11b/g. */
		if (totlen + IEEE80211_CRC_LEN > ic->ic_rtsthreshold) {
			flags |= IWN_TX_NEED_RTS;
		} else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    ridx >= IWN_RIDX_OFDM6) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				flags |= IWN_TX_NEED_CTS;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				flags |= IWN_TX_NEED_RTS;
		}
		if (flags & (IWN_TX_NEED_RTS | IWN_TX_NEED_CTS)) {
			if (sc->hw_type != IWN_HW_REV_TYPE_4965) {
				/* 5000 autoselects RTS/CTS or CTS-to-self. */
				flags &= ~(IWN_TX_NEED_RTS | IWN_TX_NEED_CTS);
				flags |= IWN_TX_NEED_PROTECTION;
			} else
				flags |= IWN_TX_FULL_TXOP;
		}
	}

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA)
		tx->id = sc->broadcast_id;
	else
		tx->id = wn->id;

	if (type == IEEE80211_FC0_TYPE_MGT) {
		uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

#ifndef IEEE80211_STA_ONLY
		/* Tell HW to set timestamp in probe responses. */
		/* XXX NetBSD rev 1.11 added probe requests here but */
		/* probe requests do not take timestamps (from Bergamini). */
		if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			flags |= IWN_TX_INSERT_TSTAMP;
#endif
		/* XXX NetBSD rev 1.11 and 1.20 added AUTH/DAUTH and RTS/CTS */
		/* changes here. These are not needed (from Bergamini). */
		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			tx->timeout = htole16(3);
		else
			tx->timeout = htole16(2);
	} else
		tx->timeout = htole16(0);

	if (hdrlen & 3) {
		/* First segment length must be a multiple of 4. */
		flags |= IWN_TX_NEED_PADDING;
		pad = 4 - (hdrlen & 3);
	} else
		pad = 0;

	tx->len = htole16(totlen);
	tx->tid = tid;
	tx->rts_ntries = 60;
	tx->data_ntries = 15;
	tx->lifetime = htole32(IWN_LIFETIME_INFINITE);
	tx->plcp = rinfo->plcp;
	tx->rflags = rinfo->flags;
	if (tx->id == sc->broadcast_id) {
		/* Group or management frame. */
		tx->linkq = 0;
		/* XXX Alternate between antenna A and B? */
		txant = IWN_LSB(sc->txchainmask);
		tx->rflags |= IWN_RFLAG_ANT(txant);
	} else {
		tx->linkq = ni->ni_rates.rs_nrates - ni->ni_txrate - 1;
		flags |= IWN_TX_LINKQ;	/* enable MRR */
	}
	/* Set physical address of "scratch area". */
	tx->loaddr = htole32(IWN_LOADDR(data->scratch_paddr));
	tx->hiaddr = IWN_HIADDR(data->scratch_paddr);

	/* Copy 802.11 header in TX command. */
	/* XXX NetBSD changed this in rev 1.20 */
	memcpy(((uint8_t *)tx) + sizeof(*tx), wh, hdrlen);

	/* Trim 802.11 header. */
	m_adj(m, hdrlen);
	tx->security = 0;
	tx->flags = htole32(flags);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (error != 0) {
		if (error != EFBIG) {
			aprint_error_dev(sc->sc_dev,
			    "can't map mbuf (error %d)\n", error);
			m_freem(m);
			return error;
		}
		/* Too many DMA segments, linearize mbuf. */
		MGETHDR(m1, M_DONTWAIT, MT_DATA);
		if (m1 == NULL) {
			m_freem(m);
			return ENOBUFS;
		}
		if (m->m_pkthdr.len > MHLEN) {
			MCLGET(m1, M_DONTWAIT);
			if (!(m1->m_flags & M_EXT)) {
				m_freem(m);
				m_freem(m1);
				return ENOBUFS;
			}
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m1, void *));
		m1->m_pkthdr.len = m1->m_len = m->m_pkthdr.len;
		m_freem(m);
		m = m1;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, data->map, m,
		    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "can't map mbuf (error %d)\n", error);
			m_freem(m);
			return error;
		}
	}

	data->m = m;
	data->ni = ni;

	DPRINTFN(4, ("sending data: qid=%d idx=%d len=%d nsegs=%d\n",
	    ring->qid, ring->cur, m->m_pkthdr.len, data->map->dm_nsegs));

	/* Fill TX descriptor. */
	desc->nsegs = 1 + data->map->dm_nsegs;
	/* First DMA segment is used by the TX command. */
	desc->segs[0].addr = htole32(IWN_LOADDR(data->cmd_paddr));
	desc->segs[0].len  = htole16(IWN_HIADDR(data->cmd_paddr) |
	    (4 + sizeof (*tx) + hdrlen + pad) << 4);
	/* Other DMA segments are for data payload. */
	seg = data->map->dm_segs;
	for (i = 1; i <= data->map->dm_nsegs; i++) {
		desc->segs[i].addr = htole32(IWN_LOADDR(seg->ds_addr));
		desc->segs[i].len  = htole16(IWN_HIADDR(seg->ds_addr) |
		    seg->ds_len << 4);
		seg++;
	}

	bus_dmamap_sync(sc->sc_dmat, data->map, 0, data->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, ring->cmd_dma.map,
	    (char *)(void *)cmd - (char *)(void *)ring->cmd_dma.vaddr,
	    sizeof (*cmd), BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map,
	    (char *)(void *)desc - (char *)(void *)ring->desc_dma.vaddr,
	    sizeof (*desc), BUS_DMASYNC_PREWRITE);

#ifdef notyet
	/* Update TX scheduler. */
	ops->update_sched(sc, ring->qid, ring->cur, tx->id, totlen);
#endif

	/* Kick TX ring. */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, ring->qid << 8 | ring->cur);

	/* Mark TX ring as full if we reach a certain threshold. */
	if (++ring->queued > IWN_TX_RING_HIMARK)
		sc->qfullmsk |= 1 << ring->qid;

	return 0;
}

static void
iwn_start(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ether_header *eh;
	struct mbuf *m;
	int ac;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		if (sc->qfullmsk != 0) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/* Send pending management frames first. */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m != NULL) {
			ni = (void *)m->m_pkthdr.rcvif;
			ac = 0;
			goto sendit;
		}
		if (ic->ic_state != IEEE80211_S_RUN)
			break;

		/* Encapsulate and send data frames. */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		if (m->m_len < sizeof (*eh) &&
		    (m = m_pullup(m, sizeof (*eh))) == NULL) {
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
		/* classify mbuf so we can find which tx ring to use */
		if (ieee80211_classify(ic, m, ni) != 0) {
			m_freem(m);
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		/* No QoS encapsulation for EAPOL frames. */
		ac = (eh->ether_type != htons(ETHERTYPE_PAE)) ?
		    M_WME_GETAC(m) : WME_AC_BE;

		bpf_mtap(ifp, m);

		if ((m = ieee80211_encap(ic, m, ni)) == NULL) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}
sendit:
		bpf_mtap3(ic->ic_rawbpf, m);

		if (iwn_tx(sc, m, ni, ac) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

static void
iwn_watchdog(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev,
			    "device timeout\n");
			ifp->if_flags &= ~IFF_UP;
			iwn_stop(ifp, 1);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(&sc->sc_ic);
}

static int
iwn_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	const struct sockaddr *sa;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		/* XXX Added as it is in every NetBSD driver */
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				error = iwn_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				iwn_stop(ifp, 1);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sa = ifreq_getaddr(SIOCADDMULTI, (struct ifreq *)data);
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(sa, &sc->sc_ec) :
		    ether_delmulti(sa, &sc->sc_ec);

		if (error == ENETRESET)
			error = 0;
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		error = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			iwn_stop(ifp, 0);
			error = iwn_init(ifp);
		}
	}

	splx(s);
	return error;
}

/*
 * Send a command to the firmware.
 */
static int
iwn_cmd(struct iwn_softc *sc, int code, const void *buf, int size, int async)
{
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_desc *desc;
	struct iwn_tx_data *data;
	struct iwn_tx_cmd *cmd;
	struct mbuf *m;
	bus_addr_t paddr;
	int totlen, error;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];
	totlen = 4 + size;

	if (size > sizeof cmd->data) {
		/* Command is too large to fit in a descriptor. */
		if (totlen > MCLBYTES)
			return EINVAL;
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return ENOMEM;
		if (totlen > MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if (!(m->m_flags & M_EXT)) {
				m_freem(m);
				return ENOMEM;
			}
		}
		cmd = mtod(m, struct iwn_tx_cmd *);
		error = bus_dmamap_load(sc->sc_dmat, data->map, cmd, totlen,
		    NULL, BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error != 0) {
			m_freem(m);
			return error;
		}
		data->m = m;
		paddr = data->map->dm_segs[0].ds_addr;
	} else {
		cmd = &ring->cmd[ring->cur];
		paddr = data->cmd_paddr;
	}

	cmd->code = code;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;
	memcpy(cmd->data, buf, size);

	desc->nsegs = 1;
	desc->segs[0].addr = htole32(IWN_LOADDR(paddr));
	desc->segs[0].len  = htole16(IWN_HIADDR(paddr) | totlen << 4);

	if (size > sizeof cmd->data) {
		bus_dmamap_sync(sc->sc_dmat, data->map, 0, totlen,
		    BUS_DMASYNC_PREWRITE);
	} else {
		bus_dmamap_sync(sc->sc_dmat, ring->cmd_dma.map,
		    (char *)(void *)cmd - (char *)(void *)ring->cmd_dma.vaddr,
		    totlen, BUS_DMASYNC_PREWRITE);
	}
	bus_dmamap_sync(sc->sc_dmat, ring->desc_dma.map,
	    (char *)(void *)desc - (char *)(void *)ring->desc_dma.vaddr,
	    sizeof (*desc), BUS_DMASYNC_PREWRITE);

#ifdef notyet
	/* Update TX scheduler. */
	ops->update_sched(sc, ring->qid, ring->cur, 0, 0);
#endif
	DPRINTFN(4, ("iwn_cmd %d size=%d %s\n", code, size, async ? " (async)" : ""));

	/* Kick command ring. */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, ring->qid << 8 | ring->cur);

	return async ? 0 : tsleep(desc, PCATCH, "iwncmd", hz);
}

static int
iwn4965_add_node(struct iwn_softc *sc, struct iwn_node_info *node, int async)
{
	struct iwn4965_node_info hnode;
	char *src, *dst;

	/*
	 * We use the node structure for 5000 Series internally (it is
	 * a superset of the one for 4965AGN). We thus copy the common
	 * fields before sending the command.
	 */
	src = (char *)node;
	dst = (char *)&hnode;
	memcpy(dst, src, 48);
	/* Skip TSC, RX MIC and TX MIC fields from ``src''. */
	memcpy(dst + 48, src + 72, 20);
	return iwn_cmd(sc, IWN_CMD_ADD_NODE, &hnode, sizeof hnode, async);
}

static int
iwn5000_add_node(struct iwn_softc *sc, struct iwn_node_info *node, int async)
{
	/* Direct mapping. */
	return iwn_cmd(sc, IWN_CMD_ADD_NODE, node, sizeof (*node), async);
}

static int
iwn_set_link_quality(struct iwn_softc *sc, struct ieee80211_node *ni)
{
	struct iwn_node *wn = (void *)ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	struct iwn_cmd_link_quality linkq;
	const struct iwn_rate *rinfo;
	uint8_t txant;
	int i, txrate;

	/* Use the first valid TX antenna. */
	txant = IWN_LSB(sc->txchainmask);

	memset(&linkq, 0, sizeof linkq);
	linkq.id = wn->id;
	linkq.antmsk_1stream = txant;
	linkq.antmsk_2stream = IWN_ANT_AB;
	linkq.ampdu_max = 31;
	linkq.ampdu_threshold = 3;
	linkq.ampdu_limit = htole16(4000);	/* 4ms */

	/* Start at highest available bit-rate. */
	txrate = rs->rs_nrates - 1;
	for (i = 0; i < IWN_MAX_TX_RETRIES; i++) {
		rinfo = &iwn_rates[wn->ridx[txrate]];
		linkq.retry[i].plcp = rinfo->plcp;
		linkq.retry[i].rflags = rinfo->flags;
		linkq.retry[i].rflags |= IWN_RFLAG_ANT(txant);
		/* Next retry at immediate lower bit-rate. */
		if (txrate > 0)
			txrate--;
	}
	return iwn_cmd(sc, IWN_CMD_LINK_QUALITY, &linkq, sizeof linkq, 1);
}

/*
 * Broadcast node is used to send group-addressed and management frames.
 */
static int
iwn_add_broadcast_node(struct iwn_softc *sc, int async)
{
	struct iwn_ops *ops = &sc->ops;
	struct iwn_node_info node;
	struct iwn_cmd_link_quality linkq;
	const struct iwn_rate *rinfo;
	uint8_t txant;
	int i, error;

	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, etherbroadcastaddr);
	node.id = sc->broadcast_id;
	DPRINTF(("adding broadcast node\n"));
	if ((error = ops->add_node(sc, &node, async)) != 0)
		return error;

	/* Use the first valid TX antenna. */
	txant = IWN_LSB(sc->txchainmask);

	memset(&linkq, 0, sizeof linkq);
	linkq.id = sc->broadcast_id;
	linkq.antmsk_1stream = txant;
	linkq.antmsk_2stream = IWN_ANT_AB;
	linkq.ampdu_max = 64;
	linkq.ampdu_threshold = 3;
	linkq.ampdu_limit = htole16(4000);	/* 4ms */

	/* Use lowest mandatory bit-rate. */
	rinfo = (sc->sc_ic.ic_curmode != IEEE80211_MODE_11A) ?
	    &iwn_rates[IWN_RIDX_CCK1] : &iwn_rates[IWN_RIDX_OFDM6];
	linkq.retry[0].plcp = rinfo->plcp;
	linkq.retry[0].rflags = rinfo->flags;
	linkq.retry[0].rflags |= IWN_RFLAG_ANT(txant);
	/* Use same bit-rate for all TX retries. */
	for (i = 1; i < IWN_MAX_TX_RETRIES; i++) {
		linkq.retry[i].plcp = linkq.retry[0].plcp;
		linkq.retry[i].rflags = linkq.retry[0].rflags;
	}
	return iwn_cmd(sc, IWN_CMD_LINK_QUALITY, &linkq, sizeof linkq, async);
}

static void
iwn_set_led(struct iwn_softc *sc, uint8_t which, uint8_t off, uint8_t on)
{
	struct iwn_cmd_led led;

	/* Clear microcode LED ownership. */
	IWN_CLRBITS(sc, IWN_LED, IWN_LED_BSM_CTRL);

	led.which = which;
	led.unit = htole32(10000);	/* on/off in unit of 100ms */
	led.off = off;
	led.on = on;
	(void)iwn_cmd(sc, IWN_CMD_SET_LED, &led, sizeof led, 1);
}

/*
 * Set the critical temperature at which the firmware will stop the radio
 * and notify us.
 */
static int
iwn_set_critical_temp(struct iwn_softc *sc)
{
	struct iwn_critical_temp crit;
	int32_t temp;

	IWN_WRITE(sc, IWN_UCODE_GP1_CLR, IWN_UCODE_GP1_CTEMP_STOP_RF);

	if (sc->hw_type == IWN_HW_REV_TYPE_5150)
		temp = (IWN_CTOK(110) - sc->temp_off) * -5;
	else if (sc->hw_type == IWN_HW_REV_TYPE_4965)
		temp = IWN_CTOK(110);
	else
		temp = 110;
	memset(&crit, 0, sizeof crit);
	crit.tempR = htole32(temp);
	DPRINTF(("setting critical temperature to %d\n", temp));
	return iwn_cmd(sc, IWN_CMD_SET_CRITICAL_TEMP, &crit, sizeof crit, 0);
}

static int
iwn_set_timing(struct iwn_softc *sc, struct ieee80211_node *ni)
{
	struct iwn_cmd_timing cmd;
	uint64_t val, mod;

	memset(&cmd, 0, sizeof cmd);
	memcpy(&cmd.tstamp, ni->ni_tstamp.data, sizeof (uint64_t));
	cmd.bintval = htole16(ni->ni_intval);
	cmd.lintval = htole16(10);

	/* Compute remaining time until next beacon. */
	val = (uint64_t)ni->ni_intval * 1024;	/* msecs -> usecs */
	mod = le64toh(cmd.tstamp) % val;
	cmd.binitval = htole32((uint32_t)(val - mod));

	DPRINTF(("timing bintval=%u, tstamp=%" PRIu64 ", init=%" PRIu32 "\n",
	    ni->ni_intval, le64toh(cmd.tstamp), (uint32_t)(val - mod)));

	return iwn_cmd(sc, IWN_CMD_TIMING, &cmd, sizeof cmd, 1);
}

static void
iwn4965_power_calibration(struct iwn_softc *sc, int temp)
{
	/* Adjust TX power if need be (delta >= 3 degC). */
	DPRINTF(("temperature %d->%d\n", sc->temp, temp));
	if (abs(temp - sc->temp) >= 3) {
		/* Record temperature of last calibration. */
		sc->temp = temp;
		(void)iwn4965_set_txpower(sc, 1);
	}
}

/*
 * Set TX power for current channel (each rate has its own power settings).
 * This function takes into account the regulatory information from EEPROM,
 * the current temperature and the current voltage.
 */
static int
iwn4965_set_txpower(struct iwn_softc *sc, int async)
{
/* Fixed-point arithmetic division using a n-bit fractional part. */
#define fdivround(a, b, n)	\
	((((1 << n) * (a)) / (b) + (1 << n) / 2) / (1 << n))
/* Linear interpolation. */
#define interpolate(x, x1, y1, x2, y2, n)	\
	((y1) + fdivround(((int)(x) - (x1)) * ((y2) - (y1)), (x2) - (x1), n))

	static const int tdiv[IWN_NATTEN_GROUPS] = { 9, 8, 8, 8, 6 };
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_ucode_info *uc = &sc->ucode_info;
	struct ieee80211_channel *ch;
	struct iwn4965_cmd_txpower cmd;
	struct iwn4965_eeprom_chan_samples *chans;
	const uint8_t *rf_gain, *dsp_gain;
	int32_t vdiff, tdiff;
	int i, c, grp, maxpwr;
	uint8_t chan;

	/* Retrieve current channel from last RXON. */
	chan = sc->rxon.chan;
	DPRINTF(("setting TX power for channel %d\n", chan));
	ch = &ic->ic_channels[chan];

	memset(&cmd, 0, sizeof cmd);
	cmd.band = IEEE80211_IS_CHAN_5GHZ(ch) ? 0 : 1;
	cmd.chan = chan;

	if (IEEE80211_IS_CHAN_5GHZ(ch)) {
		maxpwr   = sc->maxpwr5GHz;
		rf_gain  = iwn4965_rf_gain_5ghz;
		dsp_gain = iwn4965_dsp_gain_5ghz;
	} else {
		maxpwr   = sc->maxpwr2GHz;
		rf_gain  = iwn4965_rf_gain_2ghz;
		dsp_gain = iwn4965_dsp_gain_2ghz;
	}

	/* Compute voltage compensation. */
	vdiff = ((int32_t)le32toh(uc->volt) - sc->eeprom_voltage) / 7;
	if (vdiff > 0)
		vdiff *= 2;
	if (abs(vdiff) > 2)
		vdiff = 0;
	DPRINTF(("voltage compensation=%d (UCODE=%d, EEPROM=%d)\n",
	    vdiff, le32toh(uc->volt), sc->eeprom_voltage));

	/* Get channel attenuation group. */
	if (chan <= 20)		/* 1-20 */
		grp = 4;
	else if (chan <= 43)	/* 34-43 */
		grp = 0;
	else if (chan <= 70)	/* 44-70 */
		grp = 1;
	else if (chan <= 124)	/* 71-124 */
		grp = 2;
	else			/* 125-200 */
		grp = 3;
	DPRINTF(("chan %d, attenuation group=%d\n", chan, grp));

	/* Get channel sub-band. */
	for (i = 0; i < IWN_NBANDS; i++)
		if (sc->bands[i].lo != 0 &&
		    sc->bands[i].lo <= chan && chan <= sc->bands[i].hi)
			break;
	if (i == IWN_NBANDS)	/* Can't happen in real-life. */
		return EINVAL;
	chans = sc->bands[i].chans;
	DPRINTF(("chan %d sub-band=%d\n", chan, i));

	for (c = 0; c < 2; c++) {
		uint8_t power, gain, temp;
		int maxchpwr, pwr, ridx, idx;

		power = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].power,
		    chans[1].num, chans[1].samples[c][1].power, 1);
		gain  = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].gain,
		    chans[1].num, chans[1].samples[c][1].gain, 1);
		temp  = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].temp,
		    chans[1].num, chans[1].samples[c][1].temp, 1);
		DPRINTF(("TX chain %d: power=%d gain=%d temp=%d\n",
		    c, power, gain, temp));

		/* Compute temperature compensation. */
		tdiff = ((sc->temp - temp) * 2) / tdiv[grp];
		DPRINTF(("temperature compensation=%d (current=%d, "
		    "EEPROM=%d)\n", tdiff, sc->temp, temp));

		for (ridx = 0; ridx <= IWN_RIDX_MAX; ridx++) {
			/* Convert dBm to half-dBm. */
			maxchpwr = sc->maxpwr[chan] * 2;
			if ((ridx / 8) & 1)
				maxchpwr -= 6;	/* MIMO 2T: -3dB */

			pwr = maxpwr;

			/* Adjust TX power based on rate. */
			if ((ridx % 8) == 5)
				pwr -= 15;	/* OFDM48: -7.5dB */
			else if ((ridx % 8) == 6)
				pwr -= 17;	/* OFDM54: -8.5dB */
			else if ((ridx % 8) == 7)
				pwr -= 20;	/* OFDM60: -10dB */
			else
				pwr -= 10;	/* Others: -5dB */

			/* Do not exceed channel max TX power. */
			if (pwr > maxchpwr)
				pwr = maxchpwr;

			idx = gain - (pwr - power) - tdiff - vdiff;
			if ((ridx / 8) & 1)	/* MIMO */
				idx += (int32_t)le32toh(uc->atten[grp][c]);

			if (cmd.band == 0)
				idx += 9;	/* 5GHz */
			if (ridx == IWN_RIDX_MAX)
				idx += 5;	/* CCK */

			/* Make sure idx stays in a valid range. */
			if (idx < 0)
				idx = 0;
			else if (idx > IWN4965_MAX_PWR_INDEX)
				idx = IWN4965_MAX_PWR_INDEX;

			DPRINTF(("TX chain %d, rate idx %d: power=%d\n",
			    c, ridx, idx));
			cmd.power[ridx].rf_gain[c] = rf_gain[idx];
			cmd.power[ridx].dsp_gain[c] = dsp_gain[idx];
		}
	}

	DPRINTF(("setting TX power for chan %d\n", chan));
	return iwn_cmd(sc, IWN_CMD_TXPOWER, &cmd, sizeof cmd, async);

#undef interpolate
#undef fdivround
}

static int
iwn5000_set_txpower(struct iwn_softc *sc, int async)
{
	struct iwn5000_cmd_txpower cmd;

	/*
	 * TX power calibration is handled automatically by the firmware
	 * for 5000 Series.
	 */
	memset(&cmd, 0, sizeof cmd);
	cmd.global_limit = 2 * IWN5000_TXPOWER_MAX_DBM;	/* 16 dBm */
	cmd.flags = IWN5000_TXPOWER_NO_CLOSED;
	cmd.srv_limit = IWN5000_TXPOWER_AUTO;
	DPRINTF(("setting TX power\n"));
	return iwn_cmd(sc, IWN_CMD_TXPOWER_DBM, &cmd, sizeof cmd, async);
}

/*
 * Retrieve the maximum RSSI (in dBm) among receivers.
 */
static int
iwn4965_get_rssi(const struct iwn_rx_stat *stat)
{
	const struct iwn4965_rx_phystat *phy = (const void *)stat->phybuf;
	uint8_t mask, agc;
	int rssi;

	mask = (le16toh(phy->antenna) >> 4) & IWN_ANT_ABC;
	agc  = (le16toh(phy->agc) >> 7) & 0x7f;

	rssi = 0;
	if (mask & IWN_ANT_A)
		rssi = MAX(rssi, phy->rssi[0]);
	if (mask & IWN_ANT_B)
		rssi = MAX(rssi, phy->rssi[2]);
	if (mask & IWN_ANT_C)
		rssi = MAX(rssi, phy->rssi[4]);

	return rssi - agc - IWN_RSSI_TO_DBM;
}

static int
iwn5000_get_rssi(const struct iwn_rx_stat *stat)
{
	const struct iwn5000_rx_phystat *phy = (const void *)stat->phybuf;
	uint8_t agc;
	int rssi;

	agc = (le32toh(phy->agc) >> 9) & 0x7f;

	rssi = MAX(le16toh(phy->rssi[0]) & 0xff,
		   le16toh(phy->rssi[1]) & 0xff);
	rssi = MAX(le16toh(phy->rssi[2]) & 0xff, rssi);

	return rssi - agc - IWN_RSSI_TO_DBM;
}

/*
 * Retrieve the average noise (in dBm) among receivers.
 */
static int
iwn_get_noise(const struct iwn_rx_general_stats *stats)
{
	int i, total, nbant, noise;

	total = nbant = 0;
	for (i = 0; i < 3; i++) {
		if ((noise = le32toh(stats->noise[i]) & 0xff) == 0)
			continue;
		total += noise;
		nbant++;
	}
	/* There should be at least one antenna but check anyway. */
	return (nbant == 0) ? -127 : (total / nbant) - 107;
}

/*
 * Compute temperature (in degC) from last received statistics.
 */
static int
iwn4965_get_temperature(struct iwn_softc *sc)
{
	struct iwn_ucode_info *uc = &sc->ucode_info;
	int32_t r1, r2, r3, r4, temp;

	r1 = le32toh(uc->temp[0].chan20MHz);
	r2 = le32toh(uc->temp[1].chan20MHz);
	r3 = le32toh(uc->temp[2].chan20MHz);
	r4 = le32toh(sc->rawtemp);

	if (r1 == r3)	/* Prevents division by 0 (should not happen). */
		return 0;

	/* Sign-extend 23-bit R4 value to 32-bit. */
	r4 = ((r4 & 0xffffff) ^ 0x800000) - 0x800000;
	/* Compute temperature in Kelvin. */
	temp = (259 * (r4 - r2)) / (r3 - r1);
	temp = (temp * 97) / 100 + 8;

	DPRINTF(("temperature %dK/%dC\n", temp, IWN_KTOC(temp)));
	return IWN_KTOC(temp);
}

static int
iwn5000_get_temperature(struct iwn_softc *sc)
{
	int32_t temp;

	/*
	 * Temperature is not used by the driver for 5000 Series because
	 * TX power calibration is handled by firmware.  We export it to
	 * users through the sensor framework though.
	 */
	temp = le32toh(sc->rawtemp);
	if (sc->hw_type == IWN_HW_REV_TYPE_5150) {
		temp = (temp / -5) + sc->temp_off;
		temp = IWN_KTOC(temp);
	}
	return temp;
}

/*
 * Initialize sensitivity calibration state machine.
 */
static int
iwn_init_sensitivity(struct iwn_softc *sc)
{
	struct iwn_ops *ops = &sc->ops;
	struct iwn_calib_state *calib = &sc->calib;
	uint32_t flags;
	int error;

	/* Reset calibration state machine. */
	memset(calib, 0, sizeof (*calib));
	calib->state = IWN_CALIB_STATE_INIT;
	calib->cck_state = IWN_CCK_STATE_HIFA;
	/* Set initial correlation values. */
	calib->ofdm_x1     = sc->limits->min_ofdm_x1;
	calib->ofdm_mrc_x1 = sc->limits->min_ofdm_mrc_x1;
	calib->ofdm_x4     = sc->limits->min_ofdm_x4;
	calib->ofdm_mrc_x4 = sc->limits->min_ofdm_mrc_x4;
	calib->cck_x4      = 125;
	calib->cck_mrc_x4  = sc->limits->min_cck_mrc_x4;
	calib->energy_cck  = sc->limits->energy_cck;

	/* Write initial sensitivity. */
	if ((error = iwn_send_sensitivity(sc)) != 0)
		return error;

	/* Write initial gains. */
	if ((error = ops->init_gains(sc)) != 0)
		return error;

	/* Request statistics at each beacon interval. */
	flags = 0;
	DPRINTF(("sending request for statistics\n"));
	return iwn_cmd(sc, IWN_CMD_GET_STATISTICS, &flags, sizeof flags, 1);
}

/*
 * Collect noise and RSSI statistics for the first 20 beacons received
 * after association and use them to determine connected antennas and
 * to set differential gains.
 */
static void
iwn_collect_noise(struct iwn_softc *sc,
    const struct iwn_rx_general_stats *stats)
{
	struct iwn_ops *ops = &sc->ops;
	struct iwn_calib_state *calib = &sc->calib;
	uint32_t val;
	int i;

	/* Accumulate RSSI and noise for all 3 antennas. */
	for (i = 0; i < 3; i++) {
		calib->rssi[i] += le32toh(stats->rssi[i]) & 0xff;
		calib->noise[i] += le32toh(stats->noise[i]) & 0xff;
	}
	/* NB: We update differential gains only once after 20 beacons. */
	if (++calib->nbeacons < 20)
		return;

	/* Determine highest average RSSI. */
	val = MAX(calib->rssi[0], calib->rssi[1]);
	val = MAX(calib->rssi[2], val);

	/* Determine which antennas are connected. */
	sc->chainmask = sc->rxchainmask;
	for (i = 0; i < 3; i++)
		if (val - calib->rssi[i] > 15 * 20)
			sc->chainmask &= ~(1 << i);
	DPRINTF(("RX chains mask: theoretical=0x%x, actual=0x%x\n",
	    sc->rxchainmask, sc->chainmask));

	/* If none of the TX antennas are connected, keep at least one. */
	if ((sc->chainmask & sc->txchainmask) == 0)
		sc->chainmask |= IWN_LSB(sc->txchainmask);

	(void)ops->set_gains(sc);
	calib->state = IWN_CALIB_STATE_RUN;

#ifdef notyet
	/* XXX Disable RX chains with no antennas connected. */
	sc->rxon.rxchain = htole16(IWN_RXCHAIN_SEL(sc->chainmask));
	(void)iwn_cmd(sc, IWN_CMD_RXON, &sc->rxon, sc->rxonsz, 1);
#endif

	/* Enable power-saving mode if requested by user. */
	if (sc->sc_ic.ic_flags & IEEE80211_F_PMGTON)
		(void)iwn_set_pslevel(sc, 0, 3, 1);
}

static int
iwn4965_init_gains(struct iwn_softc *sc)
{
	struct iwn_phy_calib_gain cmd;

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN4965_PHY_CALIB_DIFF_GAIN;
	/* Differential gains initially set to 0 for all 3 antennas. */
	DPRINTF(("setting initial differential gains\n"));
	return iwn_cmd(sc, IWN_CMD_PHY_CALIB, &cmd, sizeof cmd, 1);
}

static int
iwn5000_init_gains(struct iwn_softc *sc)
{
	struct iwn_phy_calib cmd;

	memset(&cmd, 0, sizeof cmd);
	cmd.code = sc->reset_noise_gain;
	cmd.ngroups = 1;
	cmd.isvalid = 1;
	DPRINTF(("setting initial differential gains\n"));
	return iwn_cmd(sc, IWN_CMD_PHY_CALIB, &cmd, sizeof cmd, 1);
}

static int
iwn4965_set_gains(struct iwn_softc *sc)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_phy_calib_gain cmd;
	int i, delta, noise;

	/* Get minimal noise among connected antennas. */
	noise = INT_MAX;	/* NB: There's at least one antenna. */
	for (i = 0; i < 3; i++)
		if (sc->chainmask & (1 << i))
			noise = MIN(calib->noise[i], noise);

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN4965_PHY_CALIB_DIFF_GAIN;
	/* Set differential gains for connected antennas. */
	for (i = 0; i < 3; i++) {
		if (sc->chainmask & (1 << i)) {
			/* Compute attenuation (in unit of 1.5dB). */
			delta = (noise - (int32_t)calib->noise[i]) / 30;
			/* NB: delta <= 0 */
			/* Limit to [-4.5dB,0]. */
			cmd.gain[i] = MIN(abs(delta), 3);
			if (delta < 0)
				cmd.gain[i] |= 1 << 2;	/* sign bit */
		}
	}
	DPRINTF(("setting differential gains Ant A/B/C: %x/%x/%x (%x)\n",
	    cmd.gain[0], cmd.gain[1], cmd.gain[2], sc->chainmask));
	return iwn_cmd(sc, IWN_CMD_PHY_CALIB, &cmd, sizeof cmd, 1);
}

static int
iwn5000_set_gains(struct iwn_softc *sc)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_phy_calib_gain cmd;
	int i, ant, div, delta;

	/* We collected 20 beacons and !=6050 need a 1.5 factor. */
	div = (sc->hw_type == IWN_HW_REV_TYPE_6050) ? 20 : 30;

	memset(&cmd, 0, sizeof cmd);
	cmd.code = sc->noise_gain;
	cmd.ngroups = 1;
	cmd.isvalid = 1;
	/* Get first available RX antenna as referential. */
	ant = IWN_LSB(sc->rxchainmask);
	/* Set differential gains for other antennas. */
	for (i = ant + 1; i < 3; i++) {
		if (sc->chainmask & (1 << i)) {
			/* The delta is relative to antenna "ant". */
			delta = ((int32_t)calib->noise[ant] -
			    (int32_t)calib->noise[i]) / div;
			/* Limit to [-4.5dB,+4.5dB]. */
			cmd.gain[i - 1] = MIN(abs(delta), 3);
			if (delta < 0)
				cmd.gain[i - 1] |= 1 << 2;	/* sign bit */
		}
	}
	DPRINTF(("setting differential gains: %x/%x (%x)\n",
	    cmd.gain[0], cmd.gain[1], sc->chainmask));
	return iwn_cmd(sc, IWN_CMD_PHY_CALIB, &cmd, sizeof cmd, 1);
}

/*
 * Tune RF RX sensitivity based on the number of false alarms detected
 * during the last beacon period.
 */
static void
iwn_tune_sensitivity(struct iwn_softc *sc, const struct iwn_rx_stats *stats)
{
#define inc(val, inc, max)			\
	if ((val) < (max)) {			\
		if ((val) < (max) - (inc))	\
			(val) += (inc);		\
		else				\
			(val) = (max);		\
		needs_update = 1;		\
	}
#define dec(val, dec, min)			\
	if ((val) > (min)) {			\
		if ((val) > (min) + (dec))	\
			(val) -= (dec);		\
		else				\
			(val) = (min);		\
		needs_update = 1;		\
	}

	const struct iwn_sensitivity_limits *limits = sc->limits;
	struct iwn_calib_state *calib = &sc->calib;
	uint32_t val, rxena, fa;
	uint32_t energy[3], energy_min;
	uint8_t noise[3], noise_ref;
	int i, needs_update = 0;

	/* Check that we've been enabled long enough. */
	if ((rxena = le32toh(stats->general.load)) == 0)
		return;

	/* Compute number of false alarms since last call for OFDM. */
	fa  = le32toh(stats->ofdm.bad_plcp) - calib->bad_plcp_ofdm;
	fa += le32toh(stats->ofdm.fa) - calib->fa_ofdm;
	fa *= 200 * 1024;	/* 200TU */

	/* Save counters values for next call. */
	calib->bad_plcp_ofdm = le32toh(stats->ofdm.bad_plcp);
	calib->fa_ofdm = le32toh(stats->ofdm.fa);

	if (fa > 50 * rxena) {
		/* High false alarm count, decrease sensitivity. */
		DPRINTFN(2, ("OFDM high false alarm count: %u\n", fa));
		inc(calib->ofdm_x1,     1, limits->max_ofdm_x1);
		inc(calib->ofdm_mrc_x1, 1, limits->max_ofdm_mrc_x1);
		inc(calib->ofdm_x4,     1, limits->max_ofdm_x4);
		inc(calib->ofdm_mrc_x4, 1, limits->max_ofdm_mrc_x4);

	} else if (fa < 5 * rxena) {
		/* Low false alarm count, increase sensitivity. */
		DPRINTFN(2, ("OFDM low false alarm count: %u\n", fa));
		dec(calib->ofdm_x1,     1, limits->min_ofdm_x1);
		dec(calib->ofdm_mrc_x1, 1, limits->min_ofdm_mrc_x1);
		dec(calib->ofdm_x4,     1, limits->min_ofdm_x4);
		dec(calib->ofdm_mrc_x4, 1, limits->min_ofdm_mrc_x4);
	}

	/* Compute maximum noise among 3 receivers. */
	for (i = 0; i < 3; i++)
		noise[i] = (le32toh(stats->general.noise[i]) >> 8) & 0xff;
	val = MAX(noise[0], noise[1]);
	val = MAX(noise[2], val);
	/* Insert it into our samples table. */
	calib->noise_samples[calib->cur_noise_sample] = val;
	calib->cur_noise_sample = (calib->cur_noise_sample + 1) % 20;

	/* Compute maximum noise among last 20 samples. */
	noise_ref = calib->noise_samples[0];
	for (i = 1; i < 20; i++)
		noise_ref = MAX(noise_ref, calib->noise_samples[i]);

	/* Compute maximum energy among 3 receivers. */
	for (i = 0; i < 3; i++)
		energy[i] = le32toh(stats->general.energy[i]);
	val = MIN(energy[0], energy[1]);
	val = MIN(energy[2], val);
	/* Insert it into our samples table. */
	calib->energy_samples[calib->cur_energy_sample] = val;
	calib->cur_energy_sample = (calib->cur_energy_sample + 1) % 10;

	/* Compute minimum energy among last 10 samples. */
	energy_min = calib->energy_samples[0];
	for (i = 1; i < 10; i++)
		energy_min = MAX(energy_min, calib->energy_samples[i]);
	energy_min += 6;

	/* Compute number of false alarms since last call for CCK. */
	fa  = le32toh(stats->cck.bad_plcp) - calib->bad_plcp_cck;
	fa += le32toh(stats->cck.fa) - calib->fa_cck;
	fa *= 200 * 1024;	/* 200TU */

	/* Save counters values for next call. */
	calib->bad_plcp_cck = le32toh(stats->cck.bad_plcp);
	calib->fa_cck = le32toh(stats->cck.fa);

	if (fa > 50 * rxena) {
		/* High false alarm count, decrease sensitivity. */
		DPRINTFN(2, ("CCK high false alarm count: %u\n", fa));
		calib->cck_state = IWN_CCK_STATE_HIFA;
		calib->low_fa = 0;

		if (calib->cck_x4 > 160) {
			calib->noise_ref = noise_ref;
			if (calib->energy_cck > 2)
				dec(calib->energy_cck, 2, energy_min);
		}
		if (calib->cck_x4 < 160) {
			calib->cck_x4 = 161;
			needs_update = 1;
		} else
			inc(calib->cck_x4, 3, limits->max_cck_x4);

		inc(calib->cck_mrc_x4, 3, limits->max_cck_mrc_x4);

	} else if (fa < 5 * rxena) {
		/* Low false alarm count, increase sensitivity. */
		DPRINTFN(2, ("CCK low false alarm count: %u\n", fa));
		calib->cck_state = IWN_CCK_STATE_LOFA;
		calib->low_fa++;

		if (calib->cck_state != IWN_CCK_STATE_INIT &&
		    (((int32_t)calib->noise_ref - (int32_t)noise_ref) > 2 ||
		     calib->low_fa > 100)) {
			inc(calib->energy_cck, 2, limits->min_energy_cck);
			dec(calib->cck_x4,     3, limits->min_cck_x4);
			dec(calib->cck_mrc_x4, 3, limits->min_cck_mrc_x4);
		}
	} else {
		/* Not worth to increase or decrease sensitivity. */
		DPRINTFN(2, ("CCK normal false alarm count: %u\n", fa));
		calib->low_fa = 0;
		calib->noise_ref = noise_ref;

		if (calib->cck_state == IWN_CCK_STATE_HIFA) {
			/* Previous interval had many false alarms. */
			dec(calib->energy_cck, 8, energy_min);
		}
		calib->cck_state = IWN_CCK_STATE_INIT;
	}

	if (needs_update)
		(void)iwn_send_sensitivity(sc);
#undef dec
#undef inc
}

static int
iwn_send_sensitivity(struct iwn_softc *sc)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_enhanced_sensitivity_cmd cmd;
	int len;

	memset(&cmd, 0, sizeof cmd);
	len = sizeof (struct iwn_sensitivity_cmd);
	cmd.which = IWN_SENSITIVITY_WORKTBL;
	/* OFDM modulation. */
	cmd.corr_ofdm_x1     = htole16(calib->ofdm_x1);
	cmd.corr_ofdm_mrc_x1 = htole16(calib->ofdm_mrc_x1);
	cmd.corr_ofdm_x4     = htole16(calib->ofdm_x4);
	cmd.corr_ofdm_mrc_x4 = htole16(calib->ofdm_mrc_x4);
	cmd.energy_ofdm      = htole16(sc->limits->energy_ofdm);
	cmd.energy_ofdm_th   = htole16(62);
	/* CCK modulation. */
	cmd.corr_cck_x4      = htole16(calib->cck_x4);
	cmd.corr_cck_mrc_x4  = htole16(calib->cck_mrc_x4);
	cmd.energy_cck       = htole16(calib->energy_cck);
	/* Barker modulation: use default values. */
	cmd.corr_barker      = htole16(190);
	cmd.corr_barker_mrc  = htole16(390);
	if (!(sc->sc_flags & IWN_FLAG_ENH_SENS))
		goto send;
	/* Enhanced sensitivity settings. */
	len = sizeof (struct iwn_enhanced_sensitivity_cmd);
	cmd.ofdm_det_slope_mrc = htole16(668);
	cmd.ofdm_det_icept_mrc = htole16(4);
	cmd.ofdm_det_slope     = htole16(486);
	cmd.ofdm_det_icept     = htole16(37);
	cmd.cck_det_slope_mrc  = htole16(853);
	cmd.cck_det_icept_mrc  = htole16(4);
	cmd.cck_det_slope      = htole16(476);
	cmd.cck_det_icept      = htole16(99);
send:
	DPRINTFN(2, ("setting sensitivity %d/%d/%d/%d/%d/%d/%d\n",
	    calib->ofdm_x1, calib->ofdm_mrc_x1, calib->ofdm_x4,
	    calib->ofdm_mrc_x4, calib->cck_x4, calib->cck_mrc_x4,
	    calib->energy_cck));
	return iwn_cmd(sc, IWN_CMD_SET_SENSITIVITY, &cmd, len, 1);
}

/*
 * Set STA mode power saving level (between 0 and 5).
 * Level 0 is CAM (Continuously Aware Mode), 5 is for maximum power saving.
 */
static int
iwn_set_pslevel(struct iwn_softc *sc, int dtim, int level, int async)
{
	struct iwn_pmgt_cmd cmd;
	const struct iwn_pmgt *pmgt;
	uint32_t maxp, skip_dtim;
	pcireg_t reg;
	int i;

	/* Select which PS parameters to use. */
	if (dtim <= 2)
		pmgt = &iwn_pmgt[0][level];
	else if (dtim <= 10)
		pmgt = &iwn_pmgt[1][level];
	else
		pmgt = &iwn_pmgt[2][level];

	memset(&cmd, 0, sizeof cmd);
	if (level != 0)	/* not CAM */
		cmd.flags |= htole16(IWN_PS_ALLOW_SLEEP);
	if (level == 5)
		cmd.flags |= htole16(IWN_PS_FAST_PD);
	/* Retrieve PCIe Active State Power Management (ASPM). */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
	    sc->sc_cap_off + PCIE_LCSR);
	if (!(reg & PCIE_LCSR_ASPM_L0S))	/* L0s Entry disabled. */
		cmd.flags |= htole16(IWN_PS_PCI_PMGT);
	cmd.rxtimeout = htole32(pmgt->rxtimeout * 1024);
	cmd.txtimeout = htole32(pmgt->txtimeout * 1024);

	if (dtim == 0) {
		dtim = 1;
		skip_dtim = 0;
	} else
		skip_dtim = pmgt->skip_dtim;
	if (skip_dtim != 0) {
		cmd.flags |= htole16(IWN_PS_SLEEP_OVER_DTIM);
		maxp = pmgt->intval[4];
		if (maxp == (uint32_t)-1)
			maxp = dtim * (skip_dtim + 1);
		else if (maxp > dtim)
			maxp = (maxp / dtim) * dtim;
	} else
		maxp = dtim;
	for (i = 0; i < 5; i++)
		cmd.intval[i] = htole32(MIN(maxp, pmgt->intval[i]));

	DPRINTF(("setting power saving level to %d\n", level));
	return iwn_cmd(sc, IWN_CMD_SET_POWER_MODE, &cmd, sizeof cmd, async);
}

int
iwn5000_runtime_calib(struct iwn_softc *sc)
{
	struct iwn5000_calib_config cmd;

	memset(&cmd, 0, sizeof cmd);
	cmd.ucode.once.enable = 0xffffffff;
	cmd.ucode.once.start = IWN5000_CALIB_DC;
	DPRINTF(("configuring runtime calibration\n"));
	return iwn_cmd(sc, IWN5000_CMD_CALIB_CONFIG, &cmd, sizeof(cmd), 0);
}

static int
iwn_config_bt_coex_bluetooth(struct iwn_softc *sc)
{
	struct iwn_bluetooth bluetooth;

	memset(&bluetooth, 0, sizeof bluetooth);
	bluetooth.flags = IWN_BT_COEX_ENABLE;
	bluetooth.lead_time = IWN_BT_LEAD_TIME_DEF;
	bluetooth.max_kill = IWN_BT_MAX_KILL_DEF;

	DPRINTF(("configuring bluetooth coexistence\n"));
	return iwn_cmd(sc, IWN_CMD_BT_COEX, &bluetooth, sizeof bluetooth, 0);
}

static int
iwn_config_bt_coex_prio_table(struct iwn_softc *sc)
{
	uint8_t prio_table[16];

	memset(&prio_table, 0, sizeof prio_table);
	prio_table[ 0] =  6;	/* init calibration 1		*/
	prio_table[ 1] =  7;	/* init calibration 2		*/
	prio_table[ 2] =  2;	/* periodic calib low 1		*/
	prio_table[ 3] =  3;	/* periodic calib low 2		*/
	prio_table[ 4] =  4;	/* periodic calib high 1	*/
	prio_table[ 5] =  5;	/* periodic calib high 2	*/
	prio_table[ 6] =  6;	/* dtim				*/
	prio_table[ 7] =  8;	/* scan52			*/
	prio_table[ 8] = 10;	/* scan24			*/

	DPRINTF(("sending priority lookup table\n"));
	return iwn_cmd(sc, IWN_CMD_BT_COEX_PRIO_TABLE,
	               &prio_table, sizeof prio_table, 0);
}

static int
iwn_config_bt_coex_adv_config(struct iwn_softc *sc, struct iwn_bt_basic *basic,
    size_t len)
{
	struct iwn_btcoex_prot btprot;
	int error;

	basic->bt.flags = IWN_BT_COEX_ENABLE;
	basic->bt.lead_time = IWN_BT_LEAD_TIME_DEF;
	basic->bt.max_kill = IWN_BT_MAX_KILL_DEF;
	basic->bt.bt3_timer_t7_value = IWN_BT_BT3_T7_DEF;
	basic->bt.kill_ack_mask = IWN_BT_KILL_ACK_MASK_DEF;
	basic->bt.kill_cts_mask = IWN_BT_KILL_CTS_MASK_DEF;
	basic->bt3_prio_sample_time = IWN_BT_BT3_PRIO_SAMPLE_DEF;
	basic->bt3_timer_t2_value = IWN_BT_BT3_T2_DEF;
	basic->bt3_lookup_table[ 0] = htole32(0xaaaaaaaa); /* Normal */
	basic->bt3_lookup_table[ 1] = htole32(0xaaaaaaaa);
	basic->bt3_lookup_table[ 2] = htole32(0xaeaaaaaa);
	basic->bt3_lookup_table[ 3] = htole32(0xaaaaaaaa);
	basic->bt3_lookup_table[ 4] = htole32(0xcc00ff28);
	basic->bt3_lookup_table[ 5] = htole32(0x0000aaaa);
	basic->bt3_lookup_table[ 6] = htole32(0xcc00aaaa);
	basic->bt3_lookup_table[ 7] = htole32(0x0000aaaa);
	basic->bt3_lookup_table[ 8] = htole32(0xc0004000);
	basic->bt3_lookup_table[ 9] = htole32(0x00004000);
	basic->bt3_lookup_table[10] = htole32(0xf0005000);
	basic->bt3_lookup_table[11] = htole32(0xf0005000);
	basic->reduce_txpower = 0; /* as not implemented */
	basic->valid = IWN_BT_ALL_VALID_MASK;

	DPRINTF(("configuring advanced bluetooth coexistence v1\n"));
	error = iwn_cmd(sc, IWN_CMD_BT_COEX, basic, len, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
			"could not configure advanced bluetooth coexistence\n");
		return error;
	}

	error = iwn_config_bt_coex_prio_table(sc);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
			"could not configure send BT priority table\n");
		return error;
	}

	/* Force BT state machine change */
	memset(&btprot, 0, sizeof btprot);
	btprot.open = 1;
	btprot.type = 1;
	error = iwn_cmd(sc, IWN_CMD_BT_COEX_PROT, &btprot, sizeof btprot, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not open BT protcol\n");
		return error;
	}

	btprot.open = 0;
	error = iwn_cmd(sc, IWN_CMD_BT_COEX_PROT, &btprot, sizeof btprot, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not close BT protcol\n");
		return error;
	}
	return 0;
}

static int
iwn_config_bt_coex_adv1(struct iwn_softc *sc)
{
	struct iwn_bt_adv1 d;

	memset(&d, 0, sizeof d);
	d.prio_boost = IWN_BT_PRIO_BOOST_DEF;
	d.tx_prio_boost = 0;
	d.rx_prio_boost = 0;
	return iwn_config_bt_coex_adv_config(sc, &d.basic, sizeof d);
}

static int
iwn_config_bt_coex_adv2(struct iwn_softc *sc)
{
	struct iwn_bt_adv2 d;

	memset(&d, 0, sizeof d);
	d.prio_boost = IWN_BT_PRIO_BOOST_DEF;
	d.tx_prio_boost = 0;
	d.rx_prio_boost = 0;
	return iwn_config_bt_coex_adv_config(sc, &d.basic, sizeof d);
}

static int
iwn_config(struct iwn_softc *sc)
{
	struct iwn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	uint32_t txmask;
	uint16_t rxchain;
	int error;

	error = ops->config_bt_coex(sc);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
			"could not configure bluetooth coexistence\n");
		return error;
	}

	/* Set radio temperature sensor offset. */
	if (sc->hw_type == IWN_HW_REV_TYPE_6005) {
		error = iwn6000_temp_offset_calib(sc);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not set temperature offset\n");
			return error;
		}
	}

	if (sc->hw_type == IWN_HW_REV_TYPE_2030 ||
	    sc->hw_type == IWN_HW_REV_TYPE_2000 ||
	    sc->hw_type == IWN_HW_REV_TYPE_135  ||
	    sc->hw_type == IWN_HW_REV_TYPE_105) {
		error = iwn2000_temp_offset_calib(sc);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not set temperature offset\n");
			return error;
		}
	}

	if (sc->hw_type == IWN_HW_REV_TYPE_6050 ||
	    sc->hw_type == IWN_HW_REV_TYPE_6005) {
		/* Configure runtime DC calibration. */
		error = iwn5000_runtime_calib(sc);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not configure runtime calibration\n");
			return error;
		}
	}

	/* Configure valid TX chains for 5000 Series. */
	if (sc->hw_type != IWN_HW_REV_TYPE_4965) {
		txmask = htole32(sc->txchainmask);
		DPRINTF(("configuring valid TX chains 0x%x\n", txmask));
		error = iwn_cmd(sc, IWN5000_CMD_TX_ANT_CONFIG, &txmask,
		    sizeof txmask, 0);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not configure valid TX chains\n");
			return error;
		}
	}

	/* Set mode, channel, RX filter and enable RX. */
	memset(&sc->rxon, 0, sizeof (struct iwn_rxon));
	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	IEEE80211_ADDR_COPY(sc->rxon.myaddr, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(sc->rxon.wlap, ic->ic_myaddr);
	sc->rxon.chan = ieee80211_chan2ieee(ic, ic->ic_ibss_chan);
	sc->rxon.flags = htole32(IWN_RXON_TSF | IWN_RXON_CTS_TO_SELF);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_ibss_chan))
		sc->rxon.flags |= htole32(IWN_RXON_AUTO | IWN_RXON_24GHZ);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		sc->rxon.mode = IWN_MODE_STA;
		sc->rxon.filter = htole32(IWN_FILTER_MULTICAST);
		break;
	case IEEE80211_M_MONITOR:
		sc->rxon.mode = IWN_MODE_MONITOR;
		sc->rxon.filter = htole32(IWN_FILTER_MULTICAST |
		    IWN_FILTER_CTL | IWN_FILTER_PROMISC);
		break;
	default:
		/* Should not get there. */
		break;
	}
	sc->rxon.cck_mask  = 0x0f;	/* not yet negotiated */
	sc->rxon.ofdm_mask = 0xff;	/* not yet negotiated */
	sc->rxon.ht_single_mask = 0xff;
	sc->rxon.ht_dual_mask = 0xff;
	sc->rxon.ht_triple_mask = 0xff;
	rxchain =
	    IWN_RXCHAIN_VALID(sc->rxchainmask) |
	    IWN_RXCHAIN_MIMO_COUNT(2) |
	    IWN_RXCHAIN_IDLE_COUNT(2);
	sc->rxon.rxchain = htole16(rxchain);
	DPRINTF(("setting configuration\n"));
	error = iwn_cmd(sc, IWN_CMD_RXON, &sc->rxon, sc->rxonsz, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "RXON command failed\n");
		return error;
	}

	if ((error = iwn_add_broadcast_node(sc, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not add broadcast node\n");
		return error;
	}

	/* Configuration has changed, set TX power accordingly. */
	if ((error = ops->set_txpower(sc, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set TX power\n");
		return error;
	}

	if ((error = iwn_set_critical_temp(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set critical temperature\n");
		return error;
	}

	/* Set power saving level to CAM during initialization. */
	if ((error = iwn_set_pslevel(sc, 0, 0, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set power saving level\n");
		return error;
	}
	return 0;
}

static uint16_t
iwn_get_active_dwell_time(struct iwn_softc *sc, uint16_t flags,
    uint8_t n_probes)
{
	/* No channel? Default to 2GHz settings */
	if (flags & IEEE80211_CHAN_2GHZ)
		return IWN_ACTIVE_DWELL_TIME_2GHZ +
		    IWN_ACTIVE_DWELL_FACTOR_2GHZ * (n_probes + 1);

	/* 5GHz dwell time */
	return IWN_ACTIVE_DWELL_TIME_5GHZ +
	    IWN_ACTIVE_DWELL_FACTOR_5GHZ * (n_probes + 1);
}

/*
 * Limit the total dwell time to 85% of the beacon interval.
 *
 * Returns the dwell time in milliseconds.
 */
static uint16_t
iwn_limit_dwell(struct iwn_softc *sc, uint16_t dwell_time)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	int bintval = 0;

	/* bintval is in TU (1.024mS) */
	if (ni != NULL)
		bintval = ni->ni_intval;

	/*
	 * If it's non-zero, we should calculate the minimum of
	 * it and the DWELL_BASE.
	 *
	 * XXX Yes, the math should take into account that bintval
	 * is 1.024mS, not 1mS..
	 */
	if (bintval > 0)
		return MIN(IWN_PASSIVE_DWELL_BASE, ((bintval * 85) / 100));

	/* No association context? Default */
	return IWN_PASSIVE_DWELL_BASE;
}

static uint16_t
iwn_get_passive_dwell_time(struct iwn_softc *sc, uint16_t flags)
{
	uint16_t passive;
	if (flags & IEEE80211_CHAN_2GHZ)
		passive = IWN_PASSIVE_DWELL_BASE + IWN_PASSIVE_DWELL_TIME_2GHZ;
	else
		passive = IWN_PASSIVE_DWELL_BASE + IWN_PASSIVE_DWELL_TIME_5GHZ;

	/* Clamp to the beacon interval if we're associated */
	return iwn_limit_dwell(sc, passive);
}

static int
iwn_scan(struct iwn_softc *sc, uint16_t flags)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwn_scan_hdr *hdr;
	struct iwn_cmd_data *tx;
	struct iwn_scan_essid *essid;
	struct iwn_scan_chan *chan;
	struct ieee80211_frame *wh;
	struct ieee80211_rateset *rs;
	struct ieee80211_channel *c;
	uint8_t *buf, *frm;
	uint16_t rxchain, dwell_active, dwell_passive;
	uint8_t txant;
	int buflen, error, is_active;

	buf = malloc(IWN_SCAN_MAXSZ, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (buf == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate buffer for scan command\n");
		return ENOMEM;
	}
	hdr = (struct iwn_scan_hdr *)buf;
	/*
	 * Move to the next channel if no frames are received within 10ms
	 * after sending the probe request.
	 */
	hdr->quiet_time = htole16(10);		/* timeout in milliseconds */
	hdr->quiet_threshold = htole16(1);	/* min # of packets */

	/* Select antennas for scanning. */
	rxchain =
	    IWN_RXCHAIN_VALID(sc->rxchainmask) |
	    IWN_RXCHAIN_FORCE_MIMO_SEL(sc->rxchainmask) |
	    IWN_RXCHAIN_DRIVER_FORCE;
	if ((flags & IEEE80211_CHAN_5GHZ) &&
	    sc->hw_type == IWN_HW_REV_TYPE_4965) {
		/* Ant A must be avoided in 5GHz because of an HW bug. */
		rxchain |= IWN_RXCHAIN_FORCE_SEL(IWN_ANT_BC);
	} else	/* Use all available RX antennas. */
		rxchain |= IWN_RXCHAIN_FORCE_SEL(sc->rxchainmask);
	hdr->rxchain = htole16(rxchain);
	hdr->filter = htole32(IWN_FILTER_MULTICAST | IWN_FILTER_BEACON);

	tx = (struct iwn_cmd_data *)(hdr + 1);
	tx->flags = htole32(IWN_TX_AUTO_SEQ);
	tx->id = sc->broadcast_id;
	tx->lifetime = htole32(IWN_LIFETIME_INFINITE);

	if (flags & IEEE80211_CHAN_5GHZ) {
		hdr->crc_threshold = 0xffff;
		/* Send probe requests at 6Mbps. */
		tx->plcp = iwn_rates[IWN_RIDX_OFDM6].plcp;
		rs = &ic->ic_sup_rates[IEEE80211_MODE_11A];
	} else {
		hdr->flags = htole32(IWN_RXON_24GHZ | IWN_RXON_AUTO);
		/* Send probe requests at 1Mbps. */
		tx->plcp = iwn_rates[IWN_RIDX_CCK1].plcp;
		tx->rflags = IWN_RFLAG_CCK;
		rs = &ic->ic_sup_rates[IEEE80211_MODE_11G];
	}
	/* Use the first valid TX antenna. */
	txant = IWN_LSB(sc->txchainmask);
	tx->rflags |= IWN_RFLAG_ANT(txant);

	/*
	 * Only do active scanning if we're announcing a probe request
	 * for a given SSID (or more, if we ever add it to the driver.)
	 */
	is_active = 0;

	essid = (struct iwn_scan_essid *)(tx + 1);
	if (ic->ic_des_esslen != 0) {
		essid[0].id = IEEE80211_ELEMID_SSID;
		essid[0].len = ic->ic_des_esslen;
		memcpy(essid[0].data, ic->ic_des_essid, ic->ic_des_esslen);

		is_active = 1;
	}
	/*
	 * Build a probe request frame.  Most of the following code is a
	 * copy & paste of what is done in net80211.
	 */
	wh = (struct ieee80211_frame *)(essid + 20);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, etherbroadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, etherbroadcastaddr);
	*(uint16_t *)&wh->i_dur[0] = 0;	/* filled by HW */
	*(uint16_t *)&wh->i_seq[0] = 0;	/* filled by HW */

	frm = (uint8_t *)(wh + 1);
	frm = ieee80211_add_ssid(frm, NULL, 0);
	frm = ieee80211_add_rates(frm, rs);
#ifndef IEEE80211_NO_HT
	if (ic->ic_flags & IEEE80211_F_HTON)
		frm = ieee80211_add_htcaps(frm, ic);
#endif
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);

	/* Set length of probe request. */
	tx->len = htole16(frm - (uint8_t *)wh);


	/*
	 * If active scanning is requested but a certain channel is
	 * marked passive, we can do active scanning if we detect
	 * transmissions.
	 *
	 * There is an issue with some firmware versions that triggers
	 * a sysassert on a "good CRC threshold" of zero (== disabled),
	 * on a radar channel even though this means that we should NOT
	 * send probes.
	 *
	 * The "good CRC threshold" is the number of frames that we
	 * need to receive during our dwell time on a channel before
	 * sending out probes -- setting this to a huge value will
	 * mean we never reach it, but at the same time work around
	 * the aforementioned issue. Thus use IWN_GOOD_CRC_TH_NEVER
	 * here instead of IWN_GOOD_CRC_TH_DISABLED.
	 *
	 * This was fixed in later versions along with some other
	 * scan changes, and the threshold behaves as a flag in those
	 * versions.
	 */

	/*
	 * If we're doing active scanning, set the crc_threshold
	 * to a suitable value.  This is different to active veruss
	 * passive scanning depending upon the channel flags; the
	 * firmware will obey that particular check for us.
	 */
	if (sc->tlv_feature_flags & IWN_UCODE_TLV_FLAGS_NEWSCAN)
		hdr->crc_threshold = is_active ?
		    IWN_GOOD_CRC_TH_DEFAULT : IWN_GOOD_CRC_TH_DISABLED;
	else
		hdr->crc_threshold = is_active ?
		    IWN_GOOD_CRC_TH_DEFAULT : IWN_GOOD_CRC_TH_NEVER;

	chan = (struct iwn_scan_chan *)frm;
	for (c  = &ic->ic_channels[1];
	     c <= &ic->ic_channels[IEEE80211_CHAN_MAX]; c++) {
		if ((c->ic_flags & flags) != flags)
			continue;

		chan->chan = htole16(ieee80211_chan2ieee(ic, c));
		DPRINTFN(2, ("adding channel %d\n", chan->chan));
		chan->flags = 0;
		if (!(c->ic_flags & IEEE80211_CHAN_PASSIVE))
			chan->flags |= htole32(IWN_CHAN_ACTIVE);
		if (ic->ic_des_esslen != 0)
			chan->flags |= htole32(IWN_CHAN_NPBREQS(1));

		/*
		 * Calculate the active/passive dwell times.
		 */

		dwell_active = iwn_get_active_dwell_time(sc, flags, is_active);
		dwell_passive = iwn_get_passive_dwell_time(sc, flags);

		/* Make sure they're valid */
		if (dwell_passive <= dwell_active)
			dwell_passive = dwell_active + 1;

		chan->active = htole16(dwell_active);
		chan->passive = htole16(dwell_passive);

		chan->dsp_gain = 0x6e;
		if (IEEE80211_IS_CHAN_5GHZ(c)) {
			chan->rf_gain = 0x3b;
		} else {
			chan->rf_gain = 0x28;
		}
		hdr->nchan++;
		chan++;
	}

	buflen = (uint8_t *)chan - buf;
	hdr->len = htole16(buflen);

	DPRINTF(("sending scan command nchan=%d\n", hdr->nchan));
	error = iwn_cmd(sc, IWN_CMD_SCAN, buf, buflen, 1);
	free(buf, M_DEVBUF);
	return error;
}

static int
iwn_auth(struct iwn_softc *sc)
{
	struct iwn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	int error;

	/* Update adapter configuration. */
	IEEE80211_ADDR_COPY(sc->rxon.bssid, ni->ni_bssid);
	sc->rxon.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	sc->rxon.flags = htole32(IWN_RXON_TSF | IWN_RXON_CTS_TO_SELF);
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		sc->rxon.flags |= htole32(IWN_RXON_AUTO | IWN_RXON_24GHZ);
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->rxon.flags |= htole32(IWN_RXON_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->rxon.flags |= htole32(IWN_RXON_SHPREAMBLE);
	switch (ic->ic_curmode) {
	case IEEE80211_MODE_11A:
		sc->rxon.cck_mask  = 0;
		sc->rxon.ofdm_mask = 0x15;
		break;
	case IEEE80211_MODE_11B:
		sc->rxon.cck_mask  = 0x03;
		sc->rxon.ofdm_mask = 0;
		break;
	default:	/* Assume 802.11b/g. */
		sc->rxon.cck_mask  = 0x0f;
		sc->rxon.ofdm_mask = 0x15;
	}
	DPRINTF(("rxon chan %d flags %x cck %x ofdm %x\n", sc->rxon.chan,
	    sc->rxon.flags, sc->rxon.cck_mask, sc->rxon.ofdm_mask));
	error = iwn_cmd(sc, IWN_CMD_RXON, &sc->rxon, sc->rxonsz, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "RXON command failed\n");
		return error;
	}

	/* Configuration has changed, set TX power accordingly. */
	if ((error = ops->set_txpower(sc, 1)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set TX power\n");
		return error;
	}
	/*
	 * Reconfiguring RXON clears the firmware nodes table so we must
	 * add the broadcast node again.
	 */
	if ((error = iwn_add_broadcast_node(sc, 1)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not add broadcast node\n");
		return error;
	}
	return 0;
}

static int
iwn_run(struct iwn_softc *sc)
{
	struct iwn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwn_node_info node;
	int error;

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		/* Link LED blinks while monitoring. */
		iwn_set_led(sc, IWN_LED_LINK, 5, 5);
		return 0;
	}
	if ((error = iwn_set_timing(sc, ni)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set timing\n");
		return error;
	}

	/* Update adapter configuration. */
	sc->rxon.associd = htole16(IEEE80211_AID(ni->ni_associd));
	/* Short preamble and slot time are negotiated when associating. */
	sc->rxon.flags &= ~htole32(IWN_RXON_SHPREAMBLE | IWN_RXON_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->rxon.flags |= htole32(IWN_RXON_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->rxon.flags |= htole32(IWN_RXON_SHPREAMBLE);
	sc->rxon.filter |= htole32(IWN_FILTER_BSS);
	DPRINTF(("rxon chan %d flags %x\n", sc->rxon.chan, sc->rxon.flags));
	error = iwn_cmd(sc, IWN_CMD_RXON, &sc->rxon, sc->rxonsz, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not update configuration\n");
		return error;
	}

	/* Configuration has changed, set TX power accordingly. */
	if ((error = ops->set_txpower(sc, 1)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set TX power\n");
		return error;
	}

	/* Fake a join to initialize the TX rate. */
	((struct iwn_node *)ni)->id = IWN_ID_BSS;
	iwn_newassoc(ni, 1);

	/* Add BSS node. */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, ni->ni_macaddr);
	node.id = IWN_ID_BSS;
#ifdef notyet
	node.htflags = htole32(IWN_AMDPU_SIZE_FACTOR(3) |
	    IWN_AMDPU_DENSITY(5));	/* 2us */
#endif
	DPRINTF(("adding BSS node\n"));
	error = ops->add_node(sc, &node, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not add BSS node\n");
		return error;
	}
	DPRINTF(("setting link quality for node %d\n", node.id));
	if ((error = iwn_set_link_quality(sc, ni)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not setup link quality for node %d\n", node.id);
		return error;
	}

	if ((error = iwn_init_sensitivity(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set sensitivity\n");
		return error;
	}
	/* Start periodic calibration timer. */
	sc->calib.state = IWN_CALIB_STATE_ASSOC;
	sc->calib_cnt = 0;
	callout_schedule(&sc->calib_to, hz/2);

	/* Link LED always on while associated. */
	iwn_set_led(sc, IWN_LED_LINK, 0, 1);
	return 0;
}

#ifdef IWN_HWCRYPTO
/*
 * We support CCMP hardware encryption/decryption of unicast frames only.
 * HW support for TKIP really sucks.  We should let TKIP die anyway.
 */
static int
iwn_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_ops *ops = &sc->ops;
	struct iwn_node *wn = (void *)ni;
	struct iwn_node_info node;
	uint16_t kflags;

	if ((k->k_flags & IEEE80211_KEY_GROUP) ||
	    k->k_cipher != IEEE80211_CIPHER_CCMP)
		return ieee80211_set_key(ic, ni, k);

	kflags = IWN_KFLAG_CCMP | IWN_KFLAG_MAP | IWN_KFLAG_KID(k->k_id);
	if (k->k_flags & IEEE80211_KEY_GROUP)
		kflags |= IWN_KFLAG_GROUP;

	memset(&node, 0, sizeof node);
	node.id = (k->k_flags & IEEE80211_KEY_GROUP) ?
	    sc->broadcast_id : wn->id;
	node.control = IWN_NODE_UPDATE;
	node.flags = IWN_FLAG_SET_KEY;
	node.kflags = htole16(kflags);
	node.kid = k->k_id;
	memcpy(node.key, k->k_key, k->k_len);
	DPRINTF(("set key id=%d for node %d\n", k->k_id, node.id));
	return ops->add_node(sc, &node, 1);
}

static void
iwn_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_ops *ops = &sc->ops;
	struct iwn_node *wn = (void *)ni;
	struct iwn_node_info node;

	if ((k->k_flags & IEEE80211_KEY_GROUP) ||
	    k->k_cipher != IEEE80211_CIPHER_CCMP) {
		/* See comment about other ciphers above. */
		ieee80211_delete_key(ic, ni, k);
		return;
	}
	if (ic->ic_state != IEEE80211_S_RUN)
		return;	/* Nothing to do. */
	memset(&node, 0, sizeof node);
	node.id = (k->k_flags & IEEE80211_KEY_GROUP) ?
	    sc->broadcast_id : wn->id;
	node.control = IWN_NODE_UPDATE;
	node.flags = IWN_FLAG_SET_KEY;
	node.kflags = htole16(IWN_KFLAG_INVALID);
	node.kid = 0xff;
	DPRINTF(("delete keys for node %d\n", node.id));
	(void)ops->add_node(sc, &node, 1);
}
#endif

/* XXX Added for NetBSD (copied from rev 1.39). */

static int
iwn_wme_update(struct ieee80211com *ic)
{
#define IWN_EXP2(v)    htole16((1 << (v)) - 1)
#define IWN_USEC(v)    htole16(IEEE80211_TXOP_TO_US(v))
	struct iwn_softc *sc = ic->ic_ifp->if_softc;
	const struct wmeParams *wmep;
	struct iwn_edca_params cmd;
	int ac;

	/* don't override default WME values if WME is not actually enabled */
	if (!(ic->ic_flags & IEEE80211_F_WME))
		return 0;
	cmd.flags = 0;
	for (ac = 0; ac < WME_NUM_AC; ac++) {
		wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
		cmd.ac[ac].aifsn = wmep->wmep_aifsn;
		cmd.ac[ac].cwmin = IWN_EXP2(wmep->wmep_logcwmin);
		cmd.ac[ac].cwmax = IWN_EXP2(wmep->wmep_logcwmax);
		cmd.ac[ac].txoplimit  = IWN_USEC(wmep->wmep_txopLimit);

		DPRINTF(("setting WME for queue %d aifsn=%d cwmin=%d cwmax=%d "
					"txop=%d\n", ac, cmd.ac[ac].aifsn,
					cmd.ac[ac].cwmin,
					cmd.ac[ac].cwmax, cmd.ac[ac].txoplimit));
	}
	return iwn_cmd(sc, IWN_CMD_EDCA_PARAMS, &cmd, sizeof cmd, 1);
#undef IWN_USEC
#undef IWN_EXP2
}

#ifndef IEEE80211_NO_HT
/*
 * This function is called by upper layer when an ADDBA request is received
 * from another STA and before the ADDBA response is sent.
 */
static int
iwn_ampdu_rx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_ops *ops = &sc->ops;
	struct iwn_node *wn = (void *)ni;
	struct iwn_node_info node;

	memset(&node, 0, sizeof node);
	node.id = wn->id;
	node.control = IWN_NODE_UPDATE;
	node.flags = IWN_FLAG_SET_ADDBA;
	node.addba_tid = tid;
	node.addba_ssn = htole16(ba->ba_winstart);
	DPRINTFN(2, ("ADDBA RA=%d TID=%d SSN=%d\n", wn->id, tid,
	    ba->ba_winstart));
	return ops->add_node(sc, &node, 1);
}

/*
 * This function is called by upper layer on teardown of an HT-immediate
 * Block Ack agreement (eg. uppon receipt of a DELBA frame).
 */
static void
iwn_ampdu_rx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_ops *ops = &sc->ops;
	struct iwn_node *wn = (void *)ni;
	struct iwn_node_info node;

	memset(&node, 0, sizeof node);
	node.id = wn->id;
	node.control = IWN_NODE_UPDATE;
	node.flags = IWN_FLAG_SET_DELBA;
	node.delba_tid = tid;
	DPRINTFN(2, ("DELBA RA=%d TID=%d\n", wn->id, tid));
	(void)ops->add_node(sc, &node, 1);
}

/*
 * This function is called by upper layer when an ADDBA response is received
 * from another STA.
 */
static int
iwn_ampdu_tx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[tid];
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_ops *ops = &sc->ops;
	struct iwn_node *wn = (void *)ni;
	struct iwn_node_info node;
	int error;

	/* Enable TX for the specified RA/TID. */
	wn->disable_tid &= ~(1 << tid);
	memset(&node, 0, sizeof node);
	node.id = wn->id;
	node.control = IWN_NODE_UPDATE;
	node.flags = IWN_FLAG_SET_DISABLE_TID;
	node.disable_tid = htole16(wn->disable_tid);
	error = ops->add_node(sc, &node, 1);
	if (error != 0)
		return error;

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	ops->ampdu_tx_start(sc, ni, tid, ba->ba_winstart);
	iwn_nic_unlock(sc);
	return 0;
}

static void
iwn_ampdu_tx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[tid];
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_ops *ops = &sc->ops;

	if (iwn_nic_lock(sc) != 0)
		return;
	ops->ampdu_tx_stop(sc, tid, ba->ba_winstart);
	iwn_nic_unlock(sc);
}

static void
iwn4965_ampdu_tx_start(struct iwn_softc *sc, struct ieee80211_node *ni,
    uint8_t tid, uint16_t ssn)
{
	struct iwn_node *wn = (void *)ni;
	int qid = 7 + tid;

	/* Stop TX scheduler while we're changing its configuration. */
	iwn_prph_write(sc, IWN4965_SCHED_QUEUE_STATUS(qid),
	    IWN4965_TXQ_STATUS_CHGACT);

	/* Assign RA/TID translation to the queue. */
	iwn_mem_write_2(sc, sc->sched_base + IWN4965_SCHED_TRANS_TBL(qid),
	    wn->id << 4 | tid);

	/* Enable chain-building mode for the queue. */
	iwn_prph_setbits(sc, IWN4965_SCHED_QCHAIN_SEL, 1 << qid);

	/* Set starting sequence number from the ADDBA request. */
	IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, qid << 8 | (ssn & 0xff));
	iwn_prph_write(sc, IWN4965_SCHED_QUEUE_RDPTR(qid), ssn);

	/* Set scheduler window size. */
	iwn_mem_write(sc, sc->sched_base + IWN4965_SCHED_QUEUE_OFFSET(qid),
	    IWN_SCHED_WINSZ);
	/* Set scheduler frame limit. */
	iwn_mem_write(sc, sc->sched_base + IWN4965_SCHED_QUEUE_OFFSET(qid) + 4,
	    IWN_SCHED_LIMIT << 16);

	/* Enable interrupts for the queue. */
	iwn_prph_setbits(sc, IWN4965_SCHED_INTR_MASK, 1 << qid);

	/* Mark the queue as active. */
	iwn_prph_write(sc, IWN4965_SCHED_QUEUE_STATUS(qid),
	    IWN4965_TXQ_STATUS_ACTIVE | IWN4965_TXQ_STATUS_AGGR_ENA |
	    iwn_tid2fifo[tid] << 1);
}

static void
iwn4965_ampdu_tx_stop(struct iwn_softc *sc, uint8_t tid, uint16_t ssn)
{
	int qid = 7 + tid;

	/* Stop TX scheduler while we're changing its configuration. */
	iwn_prph_write(sc, IWN4965_SCHED_QUEUE_STATUS(qid),
	    IWN4965_TXQ_STATUS_CHGACT);

	/* Set starting sequence number from the ADDBA request. */
	IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, qid << 8 | (ssn & 0xff));
	iwn_prph_write(sc, IWN4965_SCHED_QUEUE_RDPTR(qid), ssn);

	/* Disable interrupts for the queue. */
	iwn_prph_clrbits(sc, IWN4965_SCHED_INTR_MASK, 1 << qid);

	/* Mark the queue as inactive. */
	iwn_prph_write(sc, IWN4965_SCHED_QUEUE_STATUS(qid),
	    IWN4965_TXQ_STATUS_INACTIVE | iwn_tid2fifo[tid] << 1);
}

static void
iwn5000_ampdu_tx_start(struct iwn_softc *sc, struct ieee80211_node *ni,
    uint8_t tid, uint16_t ssn)
{
	struct iwn_node *wn = (void *)ni;
	int qid = 10 + tid;

	/* Stop TX scheduler while we're changing its configuration. */
	iwn_prph_write(sc, IWN5000_SCHED_QUEUE_STATUS(qid),
	    IWN5000_TXQ_STATUS_CHGACT);

	/* Assign RA/TID translation to the queue. */
	iwn_mem_write_2(sc, sc->sched_base + IWN5000_SCHED_TRANS_TBL(qid),
	    wn->id << 4 | tid);

	/* Enable chain-building mode for the queue. */
	iwn_prph_setbits(sc, IWN5000_SCHED_QCHAIN_SEL, 1 << qid);

	/* Enable aggregation for the queue. */
	iwn_prph_setbits(sc, IWN5000_SCHED_AGGR_SEL, 1 << qid);

	/* Set starting sequence number from the ADDBA request. */
	IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, qid << 8 | (ssn & 0xff));
	iwn_prph_write(sc, IWN5000_SCHED_QUEUE_RDPTR(qid), ssn);

	/* Set scheduler window size and frame limit. */
	iwn_mem_write(sc, sc->sched_base + IWN5000_SCHED_QUEUE_OFFSET(qid) + 4,
	    IWN_SCHED_LIMIT << 16 | IWN_SCHED_WINSZ);

	/* Enable interrupts for the queue. */
	iwn_prph_setbits(sc, IWN5000_SCHED_INTR_MASK, 1 << qid);

	/* Mark the queue as active. */
	iwn_prph_write(sc, IWN5000_SCHED_QUEUE_STATUS(qid),
	    IWN5000_TXQ_STATUS_ACTIVE | iwn_tid2fifo[tid]);
}

static void
iwn5000_ampdu_tx_stop(struct iwn_softc *sc, uint8_t tid, uint16_t ssn)
{
	int qid = 10 + tid;

	/* Stop TX scheduler while we're changing its configuration. */
	iwn_prph_write(sc, IWN5000_SCHED_QUEUE_STATUS(qid),
	    IWN5000_TXQ_STATUS_CHGACT);

	/* Disable aggregation for the queue. */
	iwn_prph_clrbits(sc, IWN5000_SCHED_AGGR_SEL, 1 << qid);

	/* Set starting sequence number from the ADDBA request. */
	IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, qid << 8 | (ssn & 0xff));
	iwn_prph_write(sc, IWN5000_SCHED_QUEUE_RDPTR(qid), ssn);

	/* Disable interrupts for the queue. */
	iwn_prph_clrbits(sc, IWN5000_SCHED_INTR_MASK, 1 << qid);

	/* Mark the queue as inactive. */
	iwn_prph_write(sc, IWN5000_SCHED_QUEUE_STATUS(qid),
	    IWN5000_TXQ_STATUS_INACTIVE | iwn_tid2fifo[tid]);
}
#endif	/* !IEEE80211_NO_HT */

/*
 * Query calibration tables from the initialization firmware.  We do this
 * only once at first boot.  Called from a process context.
 */
static int
iwn5000_query_calibration(struct iwn_softc *sc)
{
	struct iwn5000_calib_config cmd;
	int error;

	memset(&cmd, 0, sizeof cmd);
	cmd.ucode.once.enable = 0xffffffff;
	cmd.ucode.once.start  = 0xffffffff;
	cmd.ucode.once.send   = 0xffffffff;
	cmd.ucode.flags       = 0xffffffff;
	DPRINTF(("sending calibration query\n"));
	error = iwn_cmd(sc, IWN5000_CMD_CALIB_CONFIG, &cmd, sizeof cmd, 0);
	if (error != 0)
		return error;

	/* Wait at most two seconds for calibration to complete. */
	if (!(sc->sc_flags & IWN_FLAG_CALIB_DONE))
		error = tsleep(sc, PCATCH, "iwncal", 2 * hz);
	return error;
}

/*
 * Send calibration results to the runtime firmware.  These results were
 * obtained on first boot from the initialization firmware.
 */
static int
iwn5000_send_calibration(struct iwn_softc *sc)
{
	int idx, error;

	for (idx = 0; idx < 5; idx++) {
		if (sc->calibcmd[idx].buf == NULL)
			continue;	/* No results available. */
		DPRINTF(("send calibration result idx=%d len=%d\n",
		    idx, sc->calibcmd[idx].len));
		error = iwn_cmd(sc, IWN_CMD_PHY_CALIB, sc->calibcmd[idx].buf,
		    sc->calibcmd[idx].len, 0);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not send calibration result\n");
			return error;
		}
	}
	return 0;
}

static int
iwn5000_send_wimax_coex(struct iwn_softc *sc)
{
	struct iwn5000_wimax_coex wimax;

#ifdef notyet
	if (sc->hw_type == IWN_HW_REV_TYPE_6050) {
		/* Enable WiMAX coexistence for combo adapters. */
		wimax.flags =
		    IWN_WIMAX_COEX_ASSOC_WA_UNMASK |
		    IWN_WIMAX_COEX_UNASSOC_WA_UNMASK |
		    IWN_WIMAX_COEX_STA_TABLE_VALID |
		    IWN_WIMAX_COEX_ENABLE;
		memcpy(wimax.events, iwn6050_wimax_events,
		    sizeof iwn6050_wimax_events);
	} else
#endif
	{
		/* Disable WiMAX coexistence. */
		wimax.flags = 0;
		memset(wimax.events, 0, sizeof wimax.events);
	}
	DPRINTF(("Configuring WiMAX coexistence\n"));
	return iwn_cmd(sc, IWN5000_CMD_WIMAX_COEX, &wimax, sizeof wimax, 0);
}

static int
iwn6000_temp_offset_calib(struct iwn_softc *sc)
{
	struct iwn6000_phy_calib_temp_offset cmd;

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN6000_PHY_CALIB_TEMP_OFFSET;
	cmd.ngroups = 1;
	cmd.isvalid = 1;
	if (sc->eeprom_temp != 0)
		cmd.offset = htole16(sc->eeprom_temp);
	else
		cmd.offset = htole16(IWN_DEFAULT_TEMP_OFFSET);
	DPRINTF(("setting radio sensor offset to %d\n", le16toh(cmd.offset)));
	return iwn_cmd(sc, IWN_CMD_PHY_CALIB, &cmd, sizeof cmd, 0);
}

static int
iwn2000_temp_offset_calib(struct iwn_softc *sc)
{
	struct iwn2000_phy_calib_temp_offset cmd;

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN2000_PHY_CALIB_TEMP_OFFSET;
	cmd.ngroups = 1;
	cmd.isvalid = 1;
	if (sc->eeprom_rawtemp != 0) {
		cmd.offset_low = htole16(sc->eeprom_rawtemp);
		cmd.offset_high = htole16(sc->eeprom_temp);
	} else {
		cmd.offset_low = htole16(IWN_DEFAULT_TEMP_OFFSET);
		cmd.offset_high = htole16(IWN_DEFAULT_TEMP_OFFSET);
	}
	cmd.burnt_voltage_ref = htole16(sc->eeprom_voltage);
	DPRINTF(("setting radio sensor offset to %d:%d, voltage to %d\n",
	    le16toh(cmd.offset_low), le16toh(cmd.offset_high),
	    le16toh(cmd.burnt_voltage_ref)));
	return iwn_cmd(sc, IWN_CMD_PHY_CALIB, &cmd, sizeof cmd, 0);
}

/*
 * This function is called after the runtime firmware notifies us of its
 * readiness (called in a process context).
 */
static int
iwn4965_post_alive(struct iwn_softc *sc)
{
	int error, qid;

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;

	/* Clear TX scheduler state in SRAM. */
	sc->sched_base = iwn_prph_read(sc, IWN_SCHED_SRAM_ADDR);
	iwn_mem_set_region_4(sc, sc->sched_base + IWN4965_SCHED_CTX_OFF, 0,
	    IWN4965_SCHED_CTX_LEN / sizeof (uint32_t));

	/* Set physical address of TX scheduler rings (1KB aligned). */
	iwn_prph_write(sc, IWN4965_SCHED_DRAM_ADDR, sc->sched_dma.paddr >> 10);

	IWN_SETBITS(sc, IWN_FH_TX_CHICKEN, IWN_FH_TX_CHICKEN_SCHED_RETRY);

	/* Disable chain mode for all our 16 queues. */
	iwn_prph_write(sc, IWN4965_SCHED_QCHAIN_SEL, 0);

	for (qid = 0; qid < IWN4965_NTXQUEUES; qid++) {
		iwn_prph_write(sc, IWN4965_SCHED_QUEUE_RDPTR(qid), 0);
		IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, qid << 8 | 0);

		/* Set scheduler window size. */
		iwn_mem_write(sc, sc->sched_base +
		    IWN4965_SCHED_QUEUE_OFFSET(qid), IWN_SCHED_WINSZ);
		/* Set scheduler frame limit. */
		iwn_mem_write(sc, sc->sched_base +
		    IWN4965_SCHED_QUEUE_OFFSET(qid) + 4,
		    IWN_SCHED_LIMIT << 16);
	}

	/* Enable interrupts for all our 16 queues. */
	iwn_prph_write(sc, IWN4965_SCHED_INTR_MASK, 0xffff);
	/* Identify TX FIFO rings (0-7). */
	iwn_prph_write(sc, IWN4965_SCHED_TXFACT, 0xff);

	/* Mark TX rings (4 EDCA + cmd + 2 HCCA) as active. */
	for (qid = 0; qid < 7; qid++) {
		static uint8_t qid2fifo[] = { 3, 2, 1, 0, 4, 5, 6 };
		iwn_prph_write(sc, IWN4965_SCHED_QUEUE_STATUS(qid),
		    IWN4965_TXQ_STATUS_ACTIVE | qid2fifo[qid] << 1);
	}
	iwn_nic_unlock(sc);
	return 0;
}

/*
 * This function is called after the initialization or runtime firmware
 * notifies us of its readiness (called in a process context).
 */
static int
iwn5000_post_alive(struct iwn_softc *sc)
{
	int error, qid;

	/* Switch to using ICT interrupt mode. */
	iwn5000_ict_reset(sc);

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;

	/* Clear TX scheduler state in SRAM. */
	sc->sched_base = iwn_prph_read(sc, IWN_SCHED_SRAM_ADDR);
	iwn_mem_set_region_4(sc, sc->sched_base + IWN5000_SCHED_CTX_OFF, 0,
	    IWN5000_SCHED_CTX_LEN / sizeof (uint32_t));

	/* Set physical address of TX scheduler rings (1KB aligned). */
	iwn_prph_write(sc, IWN5000_SCHED_DRAM_ADDR, sc->sched_dma.paddr >> 10);

	IWN_SETBITS(sc, IWN_FH_TX_CHICKEN, IWN_FH_TX_CHICKEN_SCHED_RETRY);

	/* Enable chain mode for all queues, except command queue. */
	iwn_prph_write(sc, IWN5000_SCHED_QCHAIN_SEL, 0xfffef);
	iwn_prph_write(sc, IWN5000_SCHED_AGGR_SEL, 0);

	for (qid = 0; qid < IWN5000_NTXQUEUES; qid++) {
		iwn_prph_write(sc, IWN5000_SCHED_QUEUE_RDPTR(qid), 0);
		IWN_WRITE(sc, IWN_HBUS_TARG_WRPTR, qid << 8 | 0);

		iwn_mem_write(sc, sc->sched_base +
		    IWN5000_SCHED_QUEUE_OFFSET(qid), 0);
		/* Set scheduler window size and frame limit. */
		iwn_mem_write(sc, sc->sched_base +
		    IWN5000_SCHED_QUEUE_OFFSET(qid) + 4,
		    IWN_SCHED_LIMIT << 16 | IWN_SCHED_WINSZ);
	}

	/* Enable interrupts for all our 20 queues. */
	iwn_prph_write(sc, IWN5000_SCHED_INTR_MASK, 0xfffff);
	/* Identify TX FIFO rings (0-7). */
	iwn_prph_write(sc, IWN5000_SCHED_TXFACT, 0xff);

	/* Mark TX rings (4 EDCA + cmd + 2 HCCA) as active. */
	for (qid = 0; qid < 7; qid++) {
		static uint8_t qid2fifo[] = { 3, 2, 1, 0, 7, 5, 6 };
		iwn_prph_write(sc, IWN5000_SCHED_QUEUE_STATUS(qid),
		    IWN5000_TXQ_STATUS_ACTIVE | qid2fifo[qid]);
	}
	iwn_nic_unlock(sc);

	/* Configure WiMAX coexistence for combo adapters. */
	error = iwn5000_send_wimax_coex(sc);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not configure WiMAX coexistence\n");
		return error;
	}
	if (sc->hw_type != IWN_HW_REV_TYPE_5150) {
		struct iwn5000_phy_calib_crystal cmd;

		/* Perform crystal calibration. */
		memset(&cmd, 0, sizeof cmd);
		cmd.code = IWN5000_PHY_CALIB_CRYSTAL;
		cmd.ngroups = 1;
		cmd.isvalid = 1;
		cmd.cap_pin[0] = le32toh(sc->eeprom_crystal) & 0xff;
		cmd.cap_pin[1] = (le32toh(sc->eeprom_crystal) >> 16) & 0xff;
		DPRINTF(("sending crystal calibration %d, %d\n",
		    cmd.cap_pin[0], cmd.cap_pin[1]));
		error = iwn_cmd(sc, IWN_CMD_PHY_CALIB, &cmd, sizeof cmd, 0);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "crystal calibration failed\n");
			return error;
		}
	}
	if (!(sc->sc_flags & IWN_FLAG_CALIB_DONE)) {
		/* Query calibration from the initialization firmware. */
		if ((error = iwn5000_query_calibration(sc)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not query calibration\n");
			return error;
		}
		/*
		 * We have the calibration results now, reboot with the
		 * runtime firmware (call ourselves recursively!)
		 */
		iwn_hw_stop(sc);
		error = iwn_hw_init(sc);
	} else {
		/* Send calibration results to runtime firmware. */
		error = iwn5000_send_calibration(sc);
	}
	return error;
}

/*
 * The firmware boot code is small and is intended to be copied directly into
 * the NIC internal memory (no DMA transfer).
 */
static int
iwn4965_load_bootcode(struct iwn_softc *sc, const uint8_t *ucode, int size)
{
	int error, ntries;

	size /= sizeof (uint32_t);

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;

	/* Copy microcode image into NIC memory. */
	iwn_prph_write_region_4(sc, IWN_BSM_SRAM_BASE,
	    (const uint32_t *)ucode, size);

	iwn_prph_write(sc, IWN_BSM_WR_MEM_SRC, 0);
	iwn_prph_write(sc, IWN_BSM_WR_MEM_DST, IWN_FW_TEXT_BASE);
	iwn_prph_write(sc, IWN_BSM_WR_DWCOUNT, size);

	/* Start boot load now. */
	iwn_prph_write(sc, IWN_BSM_WR_CTRL, IWN_BSM_WR_CTRL_START);

	/* Wait for transfer to complete. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(iwn_prph_read(sc, IWN_BSM_WR_CTRL) &
		    IWN_BSM_WR_CTRL_START))
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "could not load boot firmware\n");
		iwn_nic_unlock(sc);
		return ETIMEDOUT;
	}

	/* Enable boot after power up. */
	iwn_prph_write(sc, IWN_BSM_WR_CTRL, IWN_BSM_WR_CTRL_START_EN);

	iwn_nic_unlock(sc);
	return 0;
}

static int
iwn4965_load_firmware(struct iwn_softc *sc)
{
	struct iwn_fw_info *fw = &sc->fw;
	struct iwn_dma_info *dma = &sc->fw_dma;
	int error;

	/* Copy initialization sections into pre-allocated DMA-safe memory. */
	memcpy(dma->vaddr, fw->init.data, fw->init.datasz);
	bus_dmamap_sync(sc->sc_dmat, dma->map, 0, fw->init.datasz,
	    BUS_DMASYNC_PREWRITE);
	memcpy((char *)dma->vaddr + IWN4965_FW_DATA_MAXSZ,
	    fw->init.text, fw->init.textsz);
	bus_dmamap_sync(sc->sc_dmat, dma->map, IWN4965_FW_DATA_MAXSZ,
	    fw->init.textsz, BUS_DMASYNC_PREWRITE);

	/* Tell adapter where to find initialization sections. */
	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	iwn_prph_write(sc, IWN_BSM_DRAM_DATA_ADDR, dma->paddr >> 4);
	iwn_prph_write(sc, IWN_BSM_DRAM_DATA_SIZE, fw->init.datasz);
	iwn_prph_write(sc, IWN_BSM_DRAM_TEXT_ADDR,
	    (dma->paddr + IWN4965_FW_DATA_MAXSZ) >> 4);
	iwn_prph_write(sc, IWN_BSM_DRAM_TEXT_SIZE, fw->init.textsz);
	iwn_nic_unlock(sc);

	/* Load firmware boot code. */
	error = iwn4965_load_bootcode(sc, fw->boot.text, fw->boot.textsz);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load boot firmware\n");
		return error;
	}
	/* Now press "execute". */
	IWN_WRITE(sc, IWN_RESET, 0);

	/* Wait at most one second for first alive notification. */
	if ((error = tsleep(sc, PCATCH, "iwninit", hz)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for adapter to initialize\n");
		return error;
	}

	/* Retrieve current temperature for initial TX power calibration. */
	sc->rawtemp = sc->ucode_info.temp[3].chan20MHz;
	sc->temp = iwn4965_get_temperature(sc);

	/* Copy runtime sections into pre-allocated DMA-safe memory. */
	memcpy(dma->vaddr, fw->main.data, fw->main.datasz);
	bus_dmamap_sync(sc->sc_dmat, dma->map, 0, fw->main.datasz,
	    BUS_DMASYNC_PREWRITE);
	memcpy((char *)dma->vaddr + IWN4965_FW_DATA_MAXSZ,
	    fw->main.text, fw->main.textsz);
	bus_dmamap_sync(sc->sc_dmat, dma->map, IWN4965_FW_DATA_MAXSZ,
	    fw->main.textsz, BUS_DMASYNC_PREWRITE);

	/* Tell adapter where to find runtime sections. */
	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	iwn_prph_write(sc, IWN_BSM_DRAM_DATA_ADDR, dma->paddr >> 4);
	iwn_prph_write(sc, IWN_BSM_DRAM_DATA_SIZE, fw->main.datasz);
	iwn_prph_write(sc, IWN_BSM_DRAM_TEXT_ADDR,
	    (dma->paddr + IWN4965_FW_DATA_MAXSZ) >> 4);
	iwn_prph_write(sc, IWN_BSM_DRAM_TEXT_SIZE,
	    IWN_FW_UPDATED | fw->main.textsz);
	iwn_nic_unlock(sc);

	return 0;
}

static int
iwn5000_load_firmware_section(struct iwn_softc *sc, uint32_t dst,
    const uint8_t *section, int size)
{
	struct iwn_dma_info *dma = &sc->fw_dma;
	int error;

	/* Copy firmware section into pre-allocated DMA-safe memory. */
	memcpy(dma->vaddr, section, size);
	bus_dmamap_sync(sc->sc_dmat, dma->map, 0, size, BUS_DMASYNC_PREWRITE);

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;

	IWN_WRITE(sc, IWN_FH_TX_CONFIG(IWN_SRVC_DMACHNL),
	    IWN_FH_TX_CONFIG_DMA_PAUSE);

	IWN_WRITE(sc, IWN_FH_SRAM_ADDR(IWN_SRVC_DMACHNL), dst);
	IWN_WRITE(sc, IWN_FH_TFBD_CTRL0(IWN_SRVC_DMACHNL),
	    IWN_LOADDR(dma->paddr));
	IWN_WRITE(sc, IWN_FH_TFBD_CTRL1(IWN_SRVC_DMACHNL),
	    IWN_HIADDR(dma->paddr) << 28 | size);
	IWN_WRITE(sc, IWN_FH_TXBUF_STATUS(IWN_SRVC_DMACHNL),
	    IWN_FH_TXBUF_STATUS_TBNUM(1) |
	    IWN_FH_TXBUF_STATUS_TBIDX(1) |
	    IWN_FH_TXBUF_STATUS_TFBD_VALID);

	/* Kick Flow Handler to start DMA transfer. */
	IWN_WRITE(sc, IWN_FH_TX_CONFIG(IWN_SRVC_DMACHNL),
	    IWN_FH_TX_CONFIG_DMA_ENA | IWN_FH_TX_CONFIG_CIRQ_HOST_ENDTFD);

	iwn_nic_unlock(sc);

	/* Wait at most five seconds for FH DMA transfer to complete. */
	return tsleep(sc, PCATCH, "iwninit", 5 * hz);
}

static int
iwn5000_load_firmware(struct iwn_softc *sc)
{
	struct iwn_fw_part *fw;
	int error;

	/* Load the initialization firmware on first boot only. */
	fw = (sc->sc_flags & IWN_FLAG_CALIB_DONE) ?
	    &sc->fw.main : &sc->fw.init;

	error = iwn5000_load_firmware_section(sc, IWN_FW_TEXT_BASE,
	    fw->text, fw->textsz);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load firmware %s section\n", ".text");
		return error;
	}
	error = iwn5000_load_firmware_section(sc, IWN_FW_DATA_BASE,
	    fw->data, fw->datasz);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load firmware %s section\n", ".data");
		return error;
	}

	/* Now press "execute". */
	IWN_WRITE(sc, IWN_RESET, 0);
	return 0;
}

/*
 * Extract text and data sections from a legacy firmware image.
 */
static int
iwn_read_firmware_leg(struct iwn_softc *sc, struct iwn_fw_info *fw)
{
	const uint32_t *ptr;
	size_t hdrlen = 24;
	uint32_t rev;

	ptr = (const uint32_t *)fw->data;
	rev = le32toh(*ptr++);

	/* Check firmware API version. */
	if (IWN_FW_API(rev) <= 1) {
		aprint_error_dev(sc->sc_dev,
		    "bad firmware, need API version >=2\n");
		return EINVAL;
	}
	if (IWN_FW_API(rev) >= 3) {
		/* Skip build number (version 2 header). */
		hdrlen += 4;
		ptr++;
	}
	if (fw->size < hdrlen) {
		aprint_error_dev(sc->sc_dev,
		    "firmware too short: %zd bytes\n", fw->size);
		return EINVAL;
	}
	fw->main.textsz = le32toh(*ptr++);
	fw->main.datasz = le32toh(*ptr++);
	fw->init.textsz = le32toh(*ptr++);
	fw->init.datasz = le32toh(*ptr++);
	fw->boot.textsz = le32toh(*ptr++);

	/* Check that all firmware sections fit. */
	if (fw->size < hdrlen + fw->main.textsz + fw->main.datasz +
	    fw->init.textsz + fw->init.datasz + fw->boot.textsz) {
		aprint_error_dev(sc->sc_dev,
		    "firmware too short: %zd bytes\n", fw->size);
		return EINVAL;
	}

	/* Get pointers to firmware sections. */
	fw->main.text = (const uint8_t *)ptr;
	fw->main.data = fw->main.text + fw->main.textsz;
	fw->init.text = fw->main.data + fw->main.datasz;
	fw->init.data = fw->init.text + fw->init.textsz;
	fw->boot.text = fw->init.data + fw->init.datasz;
	return 0;
}

/*
 * Extract text and data sections from a TLV firmware image.
 */
static int
iwn_read_firmware_tlv(struct iwn_softc *sc, struct iwn_fw_info *fw,
    uint16_t alt)
{
	const struct iwn_fw_tlv_hdr *hdr;
	const struct iwn_fw_tlv *tlv;
	const uint8_t *ptr, *end;
	uint64_t altmask;
	uint32_t len;

	if (fw->size < sizeof (*hdr)) {
		aprint_error_dev(sc->sc_dev,
		    "firmware too short: %zd bytes\n", fw->size);
		return EINVAL;
	}
	hdr = (const struct iwn_fw_tlv_hdr *)fw->data;
	if (hdr->signature != htole32(IWN_FW_SIGNATURE)) {
		aprint_error_dev(sc->sc_dev,
		    "bad firmware signature 0x%08x\n", le32toh(hdr->signature));
		return EINVAL;
	}
	DPRINTF(("FW: \"%.64s\", build 0x%x\n", hdr->descr,
	    le32toh(hdr->build)));

	/*
	 * Select the closest supported alternative that is less than
	 * or equal to the specified one.
	 */
	altmask = le64toh(hdr->altmask);
	while (alt > 0 && !(altmask & (1ULL << alt)))
		alt--;	/* Downgrade. */
	DPRINTF(("using alternative %d\n", alt));

	ptr = (const uint8_t *)(hdr + 1);
	end = (const uint8_t *)(fw->data + fw->size);

	/* Parse type-length-value fields. */
	while (ptr + sizeof (*tlv) <= end) {
		tlv = (const struct iwn_fw_tlv *)ptr;
		len = le32toh(tlv->len);

		ptr += sizeof (*tlv);
		if (ptr + len > end) {
			aprint_error_dev(sc->sc_dev,
			    "firmware too short: %zd bytes\n", fw->size);
			return EINVAL;
		}
		/* Skip other alternatives. */
		if (tlv->alt != 0 && tlv->alt != htole16(alt))
			goto next;

		switch (le16toh(tlv->type)) {
		case IWN_FW_TLV_MAIN_TEXT:
			fw->main.text = ptr;
			fw->main.textsz = len;
			break;
		case IWN_FW_TLV_MAIN_DATA:
			fw->main.data = ptr;
			fw->main.datasz = len;
			break;
		case IWN_FW_TLV_INIT_TEXT:
			fw->init.text = ptr;
			fw->init.textsz = len;
			break;
		case IWN_FW_TLV_INIT_DATA:
			fw->init.data = ptr;
			fw->init.datasz = len;
			break;
		case IWN_FW_TLV_BOOT_TEXT:
			fw->boot.text = ptr;
			fw->boot.textsz = len;
			break;
		case IWN_FW_TLV_ENH_SENS:
			if (len != 0) {
				aprint_error_dev(sc->sc_dev,
				    "TLV type %d has invalid size %u\n",
				    le16toh(tlv->type), len);
				goto next;
			}
			sc->sc_flags |= IWN_FLAG_ENH_SENS;
			break;
		case IWN_FW_TLV_PHY_CALIB:
			if (len != sizeof(uint32_t)) {
				aprint_error_dev(sc->sc_dev,
				    "TLV type %d has invalid size %u\n",
				    le16toh(tlv->type), len);
				goto next;
			}
			if (le32toh(*ptr) <= IWN5000_PHY_CALIB_MAX) {
				sc->reset_noise_gain = le32toh(*ptr);
				sc->noise_gain = le32toh(*ptr) + 1;
			}
			break;
		case IWN_FW_TLV_FLAGS:
			if (len < sizeof(uint32_t))
				break;
			if (len % sizeof(uint32_t))
				break;
			sc->tlv_feature_flags = le32toh(*ptr);
			DPRINTF(("feature: 0x%08x\n", sc->tlv_feature_flags));
			break;
		default:
			DPRINTF(("TLV type %d not handled\n",
			    le16toh(tlv->type)));
			break;
		}
 next:		/* TLV fields are 32-bit aligned. */
		ptr += (len + 3) & ~3;
	}
	return 0;
}

static int
iwn_read_firmware(struct iwn_softc *sc)
{
	struct iwn_fw_info *fw = &sc->fw;
	firmware_handle_t fwh;
	int error;

	/*
	 * Some PHY calibration commands are firmware-dependent; these
	 * are the default values that will be overridden if
	 * necessary.
	 */
	sc->reset_noise_gain = IWN5000_PHY_CALIB_RESET_NOISE_GAIN;
	sc->noise_gain = IWN5000_PHY_CALIB_NOISE_GAIN;

	/* Initialize for error returns */
	fw->data = NULL;
	fw->size = 0;

	/* Open firmware image. */
	if ((error = firmware_open("if_iwn", sc->fwname, &fwh)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not get firmware handle %s\n", sc->fwname);
		return error;
	}
	fw->size = firmware_get_size(fwh);
	if (fw->size < sizeof (uint32_t)) {
		aprint_error_dev(sc->sc_dev,
		    "firmware too short: %zd bytes\n", fw->size);
		firmware_close(fwh);
		return EINVAL;
	}

	/* Read the firmware. */
	fw->data = firmware_malloc(fw->size);
	if (fw->data == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "not enough memory to stock firmware %s\n", sc->fwname);
		firmware_close(fwh);
		return ENOMEM;
	}
	error = firmware_read(fwh, 0, fw->data, fw->size);
	firmware_close(fwh);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not read firmware %s\n", sc->fwname);
		goto out;
	}

	/* Retrieve text and data sections. */
	if (*(const uint32_t *)fw->data != 0)	/* Legacy image. */
		error = iwn_read_firmware_leg(sc, fw);
	else
		error = iwn_read_firmware_tlv(sc, fw, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not read firmware sections\n");
		goto out;
	}

	/* Make sure text and data sections fit in hardware memory. */
	if (fw->main.textsz > sc->fw_text_maxsz ||
	    fw->main.datasz > sc->fw_data_maxsz ||
	    fw->init.textsz > sc->fw_text_maxsz ||
	    fw->init.datasz > sc->fw_data_maxsz ||
	    fw->boot.textsz > IWN_FW_BOOT_TEXT_MAXSZ ||
	    (fw->boot.textsz & 3) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "firmware sections too large\n");
		goto out;
	}

	/* We can proceed with loading the firmware. */
	return 0;
out:
	firmware_free(fw->data, fw->size);
	fw->data = NULL;
	fw->size = 0;
	return error ? error : EINVAL;
}

static int
iwn_clock_wait(struct iwn_softc *sc)
{
	int ntries;

	/* Set "initialization complete" bit. */
	IWN_SETBITS(sc, IWN_GP_CNTRL, IWN_GP_CNTRL_INIT_DONE);

	/* Wait for clock stabilization. */
	for (ntries = 0; ntries < 2500; ntries++) {
		if (IWN_READ(sc, IWN_GP_CNTRL) & IWN_GP_CNTRL_MAC_CLOCK_READY)
			return 0;
		DELAY(10);
	}
	aprint_error_dev(sc->sc_dev,
	    "timeout waiting for clock stabilization\n");
	return ETIMEDOUT;
}

static int
iwn_apm_init(struct iwn_softc *sc)
{
	pcireg_t reg;
	int error;

	/* Disable L0s exit timer (NMI bug workaround). */
	IWN_SETBITS(sc, IWN_GIO_CHICKEN, IWN_GIO_CHICKEN_DIS_L0S_TIMER);
	/* Don't wait for ICH L0s (ICH bug workaround). */
	IWN_SETBITS(sc, IWN_GIO_CHICKEN, IWN_GIO_CHICKEN_L1A_NO_L0S_RX);

	/* Set FH wait threshold to max (HW bug under stress workaround). */
	IWN_SETBITS(sc, IWN_DBG_HPET_MEM, 0xffff0000);

	/* Enable HAP INTA to move adapter from L1a to L0s. */
	IWN_SETBITS(sc, IWN_HW_IF_CONFIG, IWN_HW_IF_CONFIG_HAP_WAKE_L1A);

	/* Retrieve PCIe Active State Power Management (ASPM). */
	reg = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
	    sc->sc_cap_off + PCIE_LCSR);
	/* Workaround for HW instability in PCIe L0->L0s->L1 transition. */
	if (reg & PCIE_LCSR_ASPM_L1)	/* L1 Entry enabled. */
		IWN_SETBITS(sc, IWN_GIO, IWN_GIO_L0S_ENA);
	else
		IWN_CLRBITS(sc, IWN_GIO, IWN_GIO_L0S_ENA);

	if (sc->hw_type != IWN_HW_REV_TYPE_4965 &&
	    sc->hw_type <= IWN_HW_REV_TYPE_1000)
		IWN_SETBITS(sc, IWN_ANA_PLL, IWN_ANA_PLL_INIT);

	/* Wait for clock stabilization before accessing prph. */
	if ((error = iwn_clock_wait(sc)) != 0)
		return error;

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	if (sc->hw_type == IWN_HW_REV_TYPE_4965) {
		/* Enable DMA and BSM (Bootstrap State Machine). */
		iwn_prph_write(sc, IWN_APMG_CLK_EN,
		    IWN_APMG_CLK_CTRL_DMA_CLK_RQT |
		    IWN_APMG_CLK_CTRL_BSM_CLK_RQT);
	} else {
		/* Enable DMA. */
		iwn_prph_write(sc, IWN_APMG_CLK_EN,
		    IWN_APMG_CLK_CTRL_DMA_CLK_RQT);
	}
	DELAY(20);
	/* Disable L1-Active. */
	iwn_prph_setbits(sc, IWN_APMG_PCI_STT, IWN_APMG_PCI_STT_L1A_DIS);
	iwn_nic_unlock(sc);

	return 0;
}

static void
iwn_apm_stop_master(struct iwn_softc *sc)
{
	int ntries;

	/* Stop busmaster DMA activity. */
	IWN_SETBITS(sc, IWN_RESET, IWN_RESET_STOP_MASTER);
	for (ntries = 0; ntries < 100; ntries++) {
		if (IWN_READ(sc, IWN_RESET) & IWN_RESET_MASTER_DISABLED)
			return;
		DELAY(10);
	}
	aprint_error_dev(sc->sc_dev,
	    "timeout waiting for master\n");
}

static void
iwn_apm_stop(struct iwn_softc *sc)
{
	iwn_apm_stop_master(sc);

	/* Reset the entire device. */
	IWN_SETBITS(sc, IWN_RESET, IWN_RESET_SW);
	DELAY(10);
	/* Clear "initialization complete" bit. */
	IWN_CLRBITS(sc, IWN_GP_CNTRL, IWN_GP_CNTRL_INIT_DONE);
}

static int
iwn4965_nic_config(struct iwn_softc *sc)
{
	if (IWN_RFCFG_TYPE(sc->rfcfg) == 1) {
		/*
		 * I don't believe this to be correct but this is what the
		 * vendor driver is doing. Probably the bits should not be
		 * shifted in IWN_RFCFG_*.
		 */
		IWN_SETBITS(sc, IWN_HW_IF_CONFIG,
		    IWN_RFCFG_TYPE(sc->rfcfg) |
		    IWN_RFCFG_STEP(sc->rfcfg) |
		    IWN_RFCFG_DASH(sc->rfcfg));
	}
	IWN_SETBITS(sc, IWN_HW_IF_CONFIG,
	    IWN_HW_IF_CONFIG_RADIO_SI | IWN_HW_IF_CONFIG_MAC_SI);
	return 0;
}

static int
iwn5000_nic_config(struct iwn_softc *sc)
{
	uint32_t tmp;
	int error;

	if (IWN_RFCFG_TYPE(sc->rfcfg) < 3) {
		IWN_SETBITS(sc, IWN_HW_IF_CONFIG,
		    IWN_RFCFG_TYPE(sc->rfcfg) |
		    IWN_RFCFG_STEP(sc->rfcfg) |
		    IWN_RFCFG_DASH(sc->rfcfg));
	}
	IWN_SETBITS(sc, IWN_HW_IF_CONFIG,
	    IWN_HW_IF_CONFIG_RADIO_SI | IWN_HW_IF_CONFIG_MAC_SI);

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	iwn_prph_setbits(sc, IWN_APMG_PS, IWN_APMG_PS_EARLY_PWROFF_DIS);

	if (sc->hw_type == IWN_HW_REV_TYPE_1000) {
		/*
		 * Select first Switching Voltage Regulator (1.32V) to
		 * solve a stability issue related to noisy DC2DC line
		 * in the silicon of 1000 Series.
		 */
		tmp = iwn_prph_read(sc, IWN_APMG_DIGITAL_SVR);
		tmp &= ~IWN_APMG_DIGITAL_SVR_VOLTAGE_MASK;
		tmp |= IWN_APMG_DIGITAL_SVR_VOLTAGE_1_32;
		iwn_prph_write(sc, IWN_APMG_DIGITAL_SVR, tmp);
	}
	iwn_nic_unlock(sc);

	if (sc->sc_flags & IWN_FLAG_INTERNAL_PA) {
		/* Use internal power amplifier only. */
		IWN_WRITE(sc, IWN_GP_DRIVER, IWN_GP_DRIVER_RADIO_2X2_IPA);
	}
	if ((sc->hw_type == IWN_HW_REV_TYPE_6050 ||
		sc->hw_type == IWN_HW_REV_TYPE_6005) && sc->calib_ver >= 6) {
		/* Indicate that ROM calibration version is >=6. */
		IWN_SETBITS(sc, IWN_GP_DRIVER, IWN_GP_DRIVER_CALIB_VER6);
	}
	if (sc->hw_type == IWN_HW_REV_TYPE_6005)
		IWN_SETBITS(sc, IWN_GP_DRIVER, IWN_GP_DRIVER_6050_1X2);
	if (sc->hw_type == IWN_HW_REV_TYPE_2030 ||
	    sc->hw_type == IWN_HW_REV_TYPE_2000 ||
	    sc->hw_type == IWN_HW_REV_TYPE_135  ||
	    sc->hw_type == IWN_HW_REV_TYPE_105)
		IWN_SETBITS(sc, IWN_GP_DRIVER, IWN_GP_DRIVER_RADIO_IQ_INVERT);
	return 0;
}

/*
 * Take NIC ownership over Intel Active Management Technology (AMT).
 */
static int
iwn_hw_prepare(struct iwn_softc *sc)
{
	int ntries;

	/* Check if hardware is ready. */
	IWN_SETBITS(sc, IWN_HW_IF_CONFIG, IWN_HW_IF_CONFIG_NIC_READY);
	for (ntries = 0; ntries < 5; ntries++) {
		if (IWN_READ(sc, IWN_HW_IF_CONFIG) &
		    IWN_HW_IF_CONFIG_NIC_READY)
			return 0;
		DELAY(10);
	}

	/* Hardware not ready, force into ready state. */
	IWN_SETBITS(sc, IWN_HW_IF_CONFIG, IWN_HW_IF_CONFIG_PREPARE);
	for (ntries = 0; ntries < 15000; ntries++) {
		if (!(IWN_READ(sc, IWN_HW_IF_CONFIG) &
		    IWN_HW_IF_CONFIG_PREPARE_DONE))
			break;
		DELAY(10);
	}
	if (ntries == 15000)
		return ETIMEDOUT;

	/* Hardware should be ready now. */
	IWN_SETBITS(sc, IWN_HW_IF_CONFIG, IWN_HW_IF_CONFIG_NIC_READY);
	for (ntries = 0; ntries < 5; ntries++) {
		if (IWN_READ(sc, IWN_HW_IF_CONFIG) &
		    IWN_HW_IF_CONFIG_NIC_READY)
			return 0;
		DELAY(10);
	}
	return ETIMEDOUT;
}

static int
iwn_hw_init(struct iwn_softc *sc)
{
	struct iwn_ops *ops = &sc->ops;
	int error, chnl, qid;

	/* Clear pending interrupts. */
	IWN_WRITE(sc, IWN_INT, 0xffffffff);

	if ((error = iwn_apm_init(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not power ON adapter\n");
		return error;
	}

	/* Select VMAIN power source. */
	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	iwn_prph_clrbits(sc, IWN_APMG_PS, IWN_APMG_PS_PWR_SRC_MASK);
	iwn_nic_unlock(sc);

	/* Perform adapter-specific initialization. */
	if ((error = ops->nic_config(sc)) != 0)
		return error;

	/* Initialize RX ring. */
	if ((error = iwn_nic_lock(sc)) != 0)
		return error;
	IWN_WRITE(sc, IWN_FH_RX_CONFIG, 0);
	IWN_WRITE(sc, IWN_FH_RX_WPTR, 0);
	/* Set physical address of RX ring (256-byte aligned). */
	IWN_WRITE(sc, IWN_FH_RX_BASE, sc->rxq.desc_dma.paddr >> 8);
	/* Set physical address of RX status (16-byte aligned). */
	IWN_WRITE(sc, IWN_FH_STATUS_WPTR, sc->rxq.stat_dma.paddr >> 4);
	/* Enable RX. */
	IWN_WRITE(sc, IWN_FH_RX_CONFIG,
	    IWN_FH_RX_CONFIG_ENA           |
	    IWN_FH_RX_CONFIG_IGN_RXF_EMPTY |	/* HW bug workaround */
	    IWN_FH_RX_CONFIG_IRQ_DST_HOST  |
	    IWN_FH_RX_CONFIG_SINGLE_FRAME  |
	    IWN_FH_RX_CONFIG_RB_TIMEOUT(0) |
	    IWN_FH_RX_CONFIG_NRBD(IWN_RX_RING_COUNT_LOG));
	iwn_nic_unlock(sc);
	IWN_WRITE(sc, IWN_FH_RX_WPTR, (IWN_RX_RING_COUNT - 1) & ~7);

	if ((error = iwn_nic_lock(sc)) != 0)
		return error;

	/* Initialize TX scheduler. */
	iwn_prph_write(sc, sc->sched_txfact_addr, 0);

	/* Set physical address of "keep warm" page (16-byte aligned). */
	IWN_WRITE(sc, IWN_FH_KW_ADDR, sc->kw_dma.paddr >> 4);

	/* Initialize TX rings. */
	for (qid = 0; qid < sc->ntxqs; qid++) {
		struct iwn_tx_ring *txq = &sc->txq[qid];

		/* Set physical address of TX ring (256-byte aligned). */
		IWN_WRITE(sc, IWN_FH_CBBC_QUEUE(qid),
		    txq->desc_dma.paddr >> 8);
	}
	iwn_nic_unlock(sc);

	/* Enable DMA channels. */
	for (chnl = 0; chnl < sc->ndmachnls; chnl++) {
		IWN_WRITE(sc, IWN_FH_TX_CONFIG(chnl),
		    IWN_FH_TX_CONFIG_DMA_ENA |
		    IWN_FH_TX_CONFIG_DMA_CREDIT_ENA);
	}

	/* Clear "radio off" and "commands blocked" bits. */
	IWN_WRITE(sc, IWN_UCODE_GP1_CLR, IWN_UCODE_GP1_RFKILL);
	IWN_WRITE(sc, IWN_UCODE_GP1_CLR, IWN_UCODE_GP1_CMD_BLOCKED);

	/* Clear pending interrupts. */
	IWN_WRITE(sc, IWN_INT, 0xffffffff);
	/* Enable interrupt coalescing. */
	IWN_WRITE(sc, IWN_INT_COALESCING, 512 / 8);
	/* Enable interrupts. */
	IWN_WRITE(sc, IWN_INT_MASK, sc->int_mask);

	/* _Really_ make sure "radio off" bit is cleared! */
	IWN_WRITE(sc, IWN_UCODE_GP1_CLR, IWN_UCODE_GP1_RFKILL);
	IWN_WRITE(sc, IWN_UCODE_GP1_CLR, IWN_UCODE_GP1_RFKILL);

	/* Enable shadow registers. */
	if (sc->hw_type >= IWN_HW_REV_TYPE_6000)
		IWN_SETBITS(sc, IWN_SHADOW_REG_CTRL, 0x800fffff);

	if ((error = ops->load_firmware(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load firmware\n");
		return error;
	}
	/* Wait at most one second for firmware alive notification. */
	if ((error = tsleep(sc, PCATCH, "iwninit", hz)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for adapter to initialize\n");
		return error;
	}
	/* Do post-firmware initialization. */
	return ops->post_alive(sc);
}

static void
iwn_hw_stop(struct iwn_softc *sc)
{
	int chnl, qid, ntries;

	IWN_WRITE(sc, IWN_RESET, IWN_RESET_NEVO);

	/* Disable interrupts. */
	IWN_WRITE(sc, IWN_INT_MASK, 0);
	IWN_WRITE(sc, IWN_INT, 0xffffffff);
	IWN_WRITE(sc, IWN_FH_INT, 0xffffffff);
	sc->sc_flags &= ~IWN_FLAG_USE_ICT;

	/* Make sure we no longer hold the NIC lock. */
	iwn_nic_unlock(sc);

	/* Stop TX scheduler. */
	iwn_prph_write(sc, sc->sched_txfact_addr, 0);

	/* Stop all DMA channels. */
	if (iwn_nic_lock(sc) == 0) {
		for (chnl = 0; chnl < sc->ndmachnls; chnl++) {
			IWN_WRITE(sc, IWN_FH_TX_CONFIG(chnl), 0);
			for (ntries = 0; ntries < 200; ntries++) {
				if (IWN_READ(sc, IWN_FH_TX_STATUS) &
				    IWN_FH_TX_STATUS_IDLE(chnl))
					break;
				DELAY(10);
			}
		}
		iwn_nic_unlock(sc);
	}

	/* Stop RX ring. */
	iwn_reset_rx_ring(sc, &sc->rxq);

	/* Reset all TX rings. */
	for (qid = 0; qid < sc->ntxqs; qid++)
		iwn_reset_tx_ring(sc, &sc->txq[qid]);

	if (iwn_nic_lock(sc) == 0) {
		iwn_prph_write(sc, IWN_APMG_CLK_DIS,
		    IWN_APMG_CLK_CTRL_DMA_CLK_RQT);
		iwn_nic_unlock(sc);
	}
	DELAY(5);
	/* Power OFF adapter. */
	iwn_apm_stop(sc);
}

static int
iwn_init(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error;

	mutex_enter(&sc->sc_mtx);
	if (sc->sc_flags & IWN_FLAG_HW_INITED)
		goto out;
	if ((error = iwn_hw_prepare(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "hardware not ready\n");
		goto fail;
	}

	/* Check that the radio is not disabled by hardware switch. */
	if (!(IWN_READ(sc, IWN_GP_CNTRL) & IWN_GP_CNTRL_RFKILL)) {
		aprint_error_dev(sc->sc_dev,
		    "radio is disabled by hardware switch\n");
		error = EPERM;	/* :-) */
		goto fail;
	}

	/* Read firmware images from the filesystem. */
	if ((error = iwn_read_firmware(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not read firmware\n");
		goto fail;
	}

	/* Initialize interrupt mask to default value. */
	sc->int_mask = IWN_INT_MASK_DEF;
	sc->sc_flags &= ~IWN_FLAG_USE_ICT;

	/* Initialize hardware and upload firmware. */
	KASSERT(sc->fw.data != NULL && sc->fw.size > 0);
	error = iwn_hw_init(sc);
	firmware_free(sc->fw.data, sc->fw.size);
	sc->fw.data = NULL;
	sc->fw.size = 0;
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not initialize hardware\n");
		goto fail;
	}

	/* Configure adapter now that it is ready. */
	if ((error = iwn_config(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not configure device\n");
		goto fail;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		ieee80211_begin_scan(ic, 0);
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	sc->sc_flags |= IWN_FLAG_HW_INITED;
out:
	mutex_exit(&sc->sc_mtx);
	return 0;

fail:	mutex_exit(&sc->sc_mtx);
	iwn_stop(ifp, 1);
	return error;
}

static void
iwn_stop(struct ifnet *ifp, int disable)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (!disable)
		mutex_enter(&sc->sc_mtx);
	sc->sc_flags &= ~IWN_FLAG_HW_INITED;
	ifp->if_timer = sc->sc_tx_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* Power OFF hardware. */
	iwn_hw_stop(sc);

	if (!disable)
		mutex_exit(&sc->sc_mtx);
}

/*
 * XXX MCLGETI alternative
 *
 * With IWN_USE_RBUF defined it uses the rbuf cache for receive buffers
 * as long as there are available free buffers then it uses MEXTMALLOC.,
 * Without IWN_USE_RBUF defined it uses MEXTMALLOC exclusively.
 * The MCLGET4K code is used for testing an alternative mbuf cache.
 */

static struct mbuf *
MCLGETIalt(struct iwn_softc *sc, int how,
    struct ifnet *ifp __unused, u_int size)
{
	struct mbuf *m;
#ifdef IWN_USE_RBUF
	struct iwn_rbuf *rbuf;
#endif

	MGETHDR(m, how, MT_DATA);
	if (m == NULL)
		return NULL;

#ifdef IWN_USE_RBUF
	if (sc->rxq.nb_free_entries > 0 &&
	    (rbuf = iwn_alloc_rbuf(sc)) != NULL) {
		/* Attach buffer to mbuf header. */
		MEXTADD(m, rbuf->vaddr, size, 0, iwn_free_rbuf, rbuf);
		m->m_flags |= M_EXT_RW;
	}
	else {
		MEXTMALLOC(m, size, how);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return NULL;
		}
	}

#else
#ifdef MCLGET4K
	if (size == 4096)
		MCLGET4K(m, how);
	else
		panic("size must be 4k");
#else
	MEXTMALLOC(m, size, how);
#endif
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return NULL;
	}
#endif

	return m;
}

#ifdef IWN_USE_RBUF
static struct iwn_rbuf *
iwn_alloc_rbuf(struct iwn_softc *sc)
{
	struct iwn_rbuf *rbuf;
	mutex_enter(&sc->rxq.freelist_mtx);

	rbuf = SLIST_FIRST(&sc->rxq.freelist);
	if (rbuf != NULL) {
		SLIST_REMOVE_HEAD(&sc->rxq.freelist, next);
		sc->rxq.nb_free_entries --;
	}
	mutex_exit(&sc->rxq.freelist_mtx);
	return rbuf;
}

/*
 * This is called automatically by the network stack when the mbuf to which
 * our RX buffer is attached is freed.
 */
static void
iwn_free_rbuf(struct mbuf* m, void *buf,  size_t size, void *arg)
{
	struct iwn_rbuf *rbuf = arg;
	struct iwn_softc *sc = rbuf->sc;

	/* Put the RX buffer back in the free list. */
	mutex_enter(&sc->rxq.freelist_mtx);
	SLIST_INSERT_HEAD(&sc->rxq.freelist, rbuf, next);
	mutex_exit(&sc->rxq.freelist_mtx);

	sc->rxq.nb_free_entries ++;
	if (__predict_true(m != NULL))
		pool_cache_put(mb_cache, m);
}

static int
iwn_alloc_rpool(struct iwn_softc *sc)
{
	struct iwn_rx_ring *ring = &sc->rxq;
	struct iwn_rbuf *rbuf;
	int i, error;

	mutex_init(&ring->freelist_mtx, MUTEX_DEFAULT, IPL_NET);

	/* Allocate a big chunk of DMA'able memory... */
	error = iwn_dma_contig_alloc(sc->sc_dmat, &ring->buf_dma, NULL,
	    IWN_RBUF_COUNT * IWN_RBUF_SIZE, PAGE_SIZE);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate RX buffers DMA memory\n");
		return error;
	}
	/* ...and split it into chunks of IWN_RBUF_SIZE bytes. */
	SLIST_INIT(&ring->freelist);
	for (i = 0; i < IWN_RBUF_COUNT; i++) {
		rbuf = &ring->rbuf[i];

		rbuf->sc = sc;	/* Backpointer for callbacks. */
		rbuf->vaddr = (void *)((vaddr_t)ring->buf_dma.vaddr + i * IWN_RBUF_SIZE);
		rbuf->paddr = ring->buf_dma.paddr + i * IWN_RBUF_SIZE;

		SLIST_INSERT_HEAD(&ring->freelist, rbuf, next);
	}
	ring->nb_free_entries = IWN_RBUF_COUNT;
	return 0;
}

static void
iwn_free_rpool(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->rxq.buf_dma);
}
#endif

/*
 * XXX code from OpenBSD src/sys/net80211/ieee80211_output.c
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2007-2009 Damien Bergamini
 * All rights reserved.
 */

/*
 * Add an SSID element to a frame (see 7.3.2.1).
 */
static u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

/*
 * Add a supported rates element to a frame (see 7.3.2.2).
 */
static u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = min(rs->rs_nrates, IEEE80211_RATE_SIZE);
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add an extended supported rates element to a frame (see 7.3.2.14).
 */
static u_int8_t *
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	KASSERT(rs->rs_nrates > IEEE80211_RATE_SIZE);

	*frm++ = IEEE80211_ELEMID_XRATES;
	nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
	return frm + nrates;
}

/*
 * XXX: Hack to set the current channel to the value advertised in beacons or
 * probe responses. Only used during AP detection.
 * XXX: Duplicated from if_iwi.c
 */
static void
iwn_fix_channel(struct ieee80211com *ic, struct mbuf *m,
    struct iwn_rx_stat *stat)
{
	struct iwn_softc *sc = ic->ic_ifp->if_softc;
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

	if (sc->sc_flags & IWN_FLAG_SCANNING_5GHZ) {
		int chan = le16toh(stat->chan);
		if (chan < __arraycount(ic->ic_channels))
			ic->ic_curchan = &ic->ic_channels[chan];
		return;
	}

	frm = (uint8_t *)(wh + 1);
	efrm = mtod(m, uint8_t *) + m->m_len;

	frm += 12;      /* skip tstamp, bintval and capinfo fields */
	while (frm < efrm) {
		if (*frm == IEEE80211_ELEMID_DSPARMS)
#if IEEE80211_CHAN_MAX < 255
		if (frm[2] <= IEEE80211_CHAN_MAX)
#endif
			ic->ic_curchan = &ic->ic_channels[frm[2]];

		frm += frm[1] + 2;
	}
}

#ifdef notyetMODULE

MODULE(MODULE_CLASS_DRIVER, if_iwn, "pci");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
if_iwn_modcmd(modcmd_t cmd, void *data)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_if_iwn,
			cfattach_ioconf_if_iwn, cfdata_ioconf_if_iwn);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_if_iwn,
			cfattach_ioconf_if_iwn, cfdata_ioconf_if_iwn);
#endif
		return error;
	case MODULE_CMD_AUTOUNLOAD:
#ifdef _MODULE
		/* XXX This is not optional! */
#endif
		return error;
	default:
		return ENOTTY;
	}
}
#endif
