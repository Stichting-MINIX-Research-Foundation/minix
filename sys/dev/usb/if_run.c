/*	$NetBSD: if_run.c,v 1.11 2015/03/13 15:33:04 nonaka Exp $	*/
/*	$OpenBSD: if_run.c,v 1.90 2012/03/24 15:11:04 jsg Exp $	*/

/*-
 * Copyright (c) 2008-2010 Damien Bergamini <damien.bergamini@free.fr>
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
 * Ralink Technology RT2700U/RT2800U/RT3000U chipset driver.
 * http://www.ralinktech.com/
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_run.c,v 1.11 2015/03/13 15:33:04 nonaka Exp $");

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/callout.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/endian.h>
#include <sys/intr.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/ic/rt2860reg.h>		/* shared with ral(4) */
#include <dev/usb/if_runvar.h>

#ifdef RUN_DEBUG
#define DPRINTF(x)	do { if (run_debug) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (run_debug >= (n)) printf x; } while (0)
int run_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

#define USB_ID(v, p)	{ USB_VENDOR_##v, USB_PRODUCT_##v##_##p }
static const struct usb_devno run_devs[] = {
	USB_ID(ABOCOM,		RT2770),
	USB_ID(ABOCOM,		RT2870),
	USB_ID(ABOCOM,		RT3070),
	USB_ID(ABOCOM,		RT3071),
	USB_ID(ABOCOM,		RT3072),
	USB_ID(ABOCOM2,		RT2870_1),
	USB_ID(ACCTON,		RT2770),
	USB_ID(ACCTON,		RT2870_1),
	USB_ID(ACCTON,		RT2870_2),
	USB_ID(ACCTON,		RT2870_3),
	USB_ID(ACCTON,		RT2870_4),
	USB_ID(ACCTON,		RT2870_5),
	USB_ID(ACCTON,		RT3070),
	USB_ID(ACCTON,		RT3070_1),
	USB_ID(ACCTON,		RT3070_2),
	USB_ID(ACCTON,		RT3070_3),
	USB_ID(ACCTON,		RT3070_4),
	USB_ID(ACCTON,		RT3070_5),
	USB_ID(ACCTON,		RT3070_6),
	USB_ID(AIRTIES,		RT3070),
	USB_ID(AIRTIES,		RT3070_2),
	USB_ID(ALLWIN,		RT2070),
	USB_ID(ALLWIN,		RT2770),
	USB_ID(ALLWIN,		RT2870),
	USB_ID(ALLWIN,		RT3070),
	USB_ID(ALLWIN,		RT3071),
	USB_ID(ALLWIN,		RT3072),
	USB_ID(ALLWIN,		RT3572),
	USB_ID(AMIGO,		RT2870_1),
	USB_ID(AMIGO,		RT2870_2),
	USB_ID(AMIT,		CGWLUSB2GNR),
	USB_ID(AMIT,		RT2870_1),
	USB_ID(AMIT2,		RT2870),
	USB_ID(ASUSTEK,		RT2870_1),
	USB_ID(ASUSTEK,		RT2870_2),
	USB_ID(ASUSTEK,		RT2870_3),
	USB_ID(ASUSTEK,		RT2870_4),
	USB_ID(ASUSTEK,		RT2870_5),
	USB_ID(ASUSTEK,		RT3070),
	USB_ID(ASUSTEK,		RT3070_1),
	USB_ID(ASUSTEK2,	USBN11),
	USB_ID(AZUREWAVE,	RT2870_1),
	USB_ID(AZUREWAVE,	RT2870_2),
	USB_ID(AZUREWAVE,	RT3070),
	USB_ID(AZUREWAVE,	RT3070_2),
	USB_ID(AZUREWAVE,	RT3070_3),
	USB_ID(AZUREWAVE,	RT3070_4),
	USB_ID(AZUREWAVE,	RT3070_5),
	USB_ID(BELKIN,		F5D8053V3),
	USB_ID(BELKIN,		F5D8055),
	USB_ID(BELKIN,		F5D8055V2),
	USB_ID(BELKIN,		F6D4050V1),
	USB_ID(BELKIN,		F6D4050V2),
	USB_ID(BELKIN,		F7D1101V2),
	USB_ID(BELKIN,		RT2870_1),
	USB_ID(BELKIN,		RT2870_2),
	USB_ID(BEWAN,		RT3070),
	USB_ID(CISCOLINKSYS,	AE1000),
	USB_ID(CISCOLINKSYS,	AM10),
	USB_ID(CISCOLINKSYS2,	RT3070),
	USB_ID(CISCOLINKSYS3,	RT3070),
	USB_ID(CONCEPTRONIC,	RT2870_1),
	USB_ID(CONCEPTRONIC,	RT2870_2),
	USB_ID(CONCEPTRONIC,	RT2870_3),
	USB_ID(CONCEPTRONIC,	RT2870_4),
	USB_ID(CONCEPTRONIC,	RT2870_5),
	USB_ID(CONCEPTRONIC,	RT2870_6),
	USB_ID(CONCEPTRONIC,	RT2870_7),
	USB_ID(CONCEPTRONIC,	RT2870_8),
	USB_ID(CONCEPTRONIC,	RT3070_1),
	USB_ID(CONCEPTRONIC,	RT3070_2),
	USB_ID(CONCEPTRONIC,	RT3070_3),
	USB_ID(COREGA,		CGWLUSB300GNM),
	USB_ID(COREGA,		RT2870_1),
	USB_ID(COREGA,		RT2870_2),
	USB_ID(COREGA,		RT2870_3),
	USB_ID(COREGA,		RT3070),
	USB_ID(CYBERTAN,	RT2870),
	USB_ID(DLINK,		RT2870),
	USB_ID(DLINK,		RT3072),
	USB_ID(DLINK2,		DWA130),
	USB_ID(DLINK2,		RT2870_1),
	USB_ID(DLINK2,		RT2870_2),
	USB_ID(DLINK2,		RT3070_1),
	USB_ID(DLINK2,		RT3070_2),
	USB_ID(DLINK2,		RT3070_3),
	USB_ID(DLINK2,		RT3070_4),
	USB_ID(DLINK2,		RT3070_5),
	USB_ID(DLINK2,		RT3072),
	USB_ID(DLINK2,		RT3072_1),
	USB_ID(DVICO,		RT3070),
	USB_ID(EDIMAX,		EW7717),
	USB_ID(EDIMAX,		EW7718),
	USB_ID(EDIMAX,		EW7722UTN),
	USB_ID(EDIMAX,		RT2870_1),
	USB_ID(ENCORE,		RT3070),
	USB_ID(ENCORE,		RT3070_2),
	USB_ID(ENCORE,		RT3070_3),
	USB_ID(GIGABYTE,	GNWB31N),
	USB_ID(GIGABYTE,	GNWB32L),
	USB_ID(GIGABYTE,	RT2870_1),
	USB_ID(GIGASET,		RT3070_1),
	USB_ID(GIGASET,		RT3070_2),
	USB_ID(GUILLEMOT,	HWNU300),
	USB_ID(HAWKING,		HWUN2),
	USB_ID(HAWKING,		RT2870_1),
	USB_ID(HAWKING,		RT2870_2),
	USB_ID(HAWKING,		RT2870_3),
	USB_ID(HAWKING,		RT2870_4),
	USB_ID(HAWKING,		RT2870_5),
	USB_ID(HAWKING,		RT3070),
	USB_ID(IODATA,		RT3072_1),
	USB_ID(IODATA,		RT3072_2),
	USB_ID(IODATA,		RT3072_3),
	USB_ID(IODATA,		RT3072_4),
	USB_ID(LINKSYS4,	RT3070),
	USB_ID(LINKSYS4,	WUSB100),
	USB_ID(LINKSYS4,	WUSB54GC_3),
	USB_ID(LINKSYS4,	WUSB600N),
	USB_ID(LINKSYS4,	WUSB600NV2),
	USB_ID(LOGITEC,		LANW300NU2),
	USB_ID(LOGITEC,		RT2870_1),
	USB_ID(LOGITEC,		RT2870_2),
	USB_ID(LOGITEC,		RT2870_3),
	USB_ID(LOGITEC,		RT3020),
	USB_ID(MELCO,		RT2870_1),
	USB_ID(MELCO,		RT2870_2),
	USB_ID(MELCO,		WLIUCAG300N),
	USB_ID(MELCO,		WLIUCG300N),
	USB_ID(MELCO,		WLIUCG301N),
	USB_ID(MELCO,		WLIUCGN),
	USB_ID(MELCO,		WLIUCGNHP),
	USB_ID(MELCO,		WLIUCGNM),
	USB_ID(MELCO,		WLIUCGNM2T),
	USB_ID(MOTOROLA4,	RT2770),
	USB_ID(MOTOROLA4,	RT3070),
	USB_ID(MSI,		RT3070),
	USB_ID(MSI,		RT3070_2),
	USB_ID(MSI,		RT3070_3),
	USB_ID(MSI,		RT3070_4),
	USB_ID(MSI,		RT3070_5),
	USB_ID(MSI,		RT3070_6),
	USB_ID(MSI,		RT3070_7),
	USB_ID(MSI,		RT3070_8),
	USB_ID(MSI,		RT3070_9),
	USB_ID(MSI,		RT3070_10),
	USB_ID(MSI,		RT3070_11),
	USB_ID(MSI,		RT3070_12),
	USB_ID(MSI,		RT3070_13),
	USB_ID(MSI,		RT3070_14),
	USB_ID(MSI,		RT3070_15),
	USB_ID(OVISLINK,	RT3071),
	USB_ID(OVISLINK,	RT3072),
	USB_ID(PARA,		RT3070),
	USB_ID(PEGATRON,	RT2870),
	USB_ID(PEGATRON,	RT3070),
	USB_ID(PEGATRON,	RT3070_2),
	USB_ID(PEGATRON,	RT3070_3),
	USB_ID(PEGATRON,	RT3072),
	USB_ID(PHILIPS,		RT2870),
	USB_ID(PLANEX2,		GWUS300MINIS),
	USB_ID(PLANEX2,		GWUSMICRO300),
	USB_ID(PLANEX2,		GWUSMICRON),
	USB_ID(PLANEX2,		GWUS300MINIX),
	USB_ID(PLANEX2,		RT3070),
	USB_ID(QCOM,		RT2870),
	USB_ID(QUANTA,		RT3070),
	USB_ID(RALINK,		RT2070),
	USB_ID(RALINK,		RT2770),
	USB_ID(RALINK,		RT2870),
	USB_ID(RALINK,		RT3070),
	USB_ID(RALINK,		RT3071),
	USB_ID(RALINK,		RT3072),
	USB_ID(RALINK,		RT3370),
	USB_ID(RALINK,		RT3572),
	USB_ID(RALINK,		RT8070),
	USB_ID(SAMSUNG,		RT2870_1),
	USB_ID(SENAO,		RT2870_1),
	USB_ID(SENAO,		RT2870_2),
	USB_ID(SENAO,		RT2870_3),
	USB_ID(SENAO,		RT2870_4),
	USB_ID(SENAO,		RT3070),
	USB_ID(SENAO,		RT3071),
	USB_ID(SENAO,		RT3072),
	USB_ID(SENAO,		RT3072_2),
	USB_ID(SENAO,		RT3072_3),
	USB_ID(SENAO,		RT3072_4),
	USB_ID(SENAO,		RT3072_5),
	USB_ID(SITECOMEU,	RT2870_1),
	USB_ID(SITECOMEU,	RT2870_2),
	USB_ID(SITECOMEU,	RT2870_3),
	USB_ID(SITECOMEU,	RT3070_1),
	USB_ID(SITECOMEU,	RT3072_3),
	USB_ID(SITECOMEU,	RT3072_4),
	USB_ID(SITECOMEU,	RT3072_5),
	USB_ID(SITECOMEU,	WL302),
	USB_ID(SITECOMEU,	WL315),
	USB_ID(SITECOMEU,	WL321),
	USB_ID(SITECOMEU,	WL324),
	USB_ID(SITECOMEU,	WL329),
	USB_ID(SITECOMEU,	WL343),
	USB_ID(SITECOMEU,	WL344),
	USB_ID(SITECOMEU,	WL345),
	USB_ID(SITECOMEU,	WL349V4),
	USB_ID(SITECOMEU,	WL608),
	USB_ID(SITECOMEU,	WLA4000),
	USB_ID(SITECOMEU,	WLA5000),
	USB_ID(SPARKLAN,	RT2870_1),
	USB_ID(SPARKLAN,	RT2870_2),
	USB_ID(SPARKLAN,	RT3070),
	USB_ID(SWEEX2,		LW153),
	USB_ID(SWEEX2,		LW303),
	USB_ID(SWEEX2,		LW313),
	USB_ID(TOSHIBA,		RT3070),
	USB_ID(UMEDIA,		RT2870_1),
	USB_ID(UMEDIA,		TEW645UB),
	USB_ID(ZCOM,		RT2870_1),
	USB_ID(ZCOM,		RT2870_2),
	USB_ID(ZINWELL,		RT2870_1),
	USB_ID(ZINWELL,		RT2870_2),
	USB_ID(ZINWELL,		RT3070),
	USB_ID(ZINWELL,		RT3072),
	USB_ID(ZINWELL,		RT3072_2),
	USB_ID(ZYXEL,		NWD2105),
	USB_ID(ZYXEL,		NWD211AN),
	USB_ID(ZYXEL,		RT2870_1),
	USB_ID(ZYXEL,		RT2870_2),
	USB_ID(ZYXEL,		RT3070),
};

static int		run_match(device_t, cfdata_t, void *);
static void		run_attach(device_t, device_t, void *);
static int		run_detach(device_t, int);
static int		run_activate(device_t, enum devact);

CFATTACH_DECL_NEW(run, sizeof(struct run_softc),
	run_match, run_attach, run_detach, run_activate);

static int		run_alloc_rx_ring(struct run_softc *);
static void		run_free_rx_ring(struct run_softc *);
static int		run_alloc_tx_ring(struct run_softc *, int);
static void		run_free_tx_ring(struct run_softc *, int);
static int		run_load_microcode(struct run_softc *);
static int		run_reset(struct run_softc *);
static int		run_read(struct run_softc *, uint16_t, uint32_t *);
static int		run_read_region_1(struct run_softc *, uint16_t,
			    uint8_t *, int);
static int		run_write_2(struct run_softc *, uint16_t, uint16_t);
static int		run_write(struct run_softc *, uint16_t, uint32_t);
static int		run_write_region_1(struct run_softc *, uint16_t,
			    const uint8_t *, int);
static int		run_set_region_4(struct run_softc *, uint16_t,
			    uint32_t, int);
static int		run_efuse_read_2(struct run_softc *, uint16_t,
			    uint16_t *);
static int		run_eeprom_read_2(struct run_softc *, uint16_t,
			    uint16_t *);
static int		run_rt2870_rf_write(struct run_softc *, uint8_t,
			    uint32_t);
static int		run_rt3070_rf_read(struct run_softc *, uint8_t,
			    uint8_t *);
static int		run_rt3070_rf_write(struct run_softc *, uint8_t,
			    uint8_t);
static int		run_bbp_read(struct run_softc *, uint8_t, uint8_t *);
static int		run_bbp_write(struct run_softc *, uint8_t, uint8_t);
static int		run_mcu_cmd(struct run_softc *, uint8_t, uint16_t);
static const char *	run_get_rf(int);
static int		run_read_eeprom(struct run_softc *);
static struct ieee80211_node *
			run_node_alloc(struct ieee80211_node_table *);
static int		run_media_change(struct ifnet *);
static void		run_next_scan(void *);
static void		run_task(void *);
static void		run_do_async(struct run_softc *,
			    void (*)(struct run_softc *, void *), void *, int);
static int		run_newstate(struct ieee80211com *,
			    enum ieee80211_state, int);
static void		run_newstate_cb(struct run_softc *, void *);
static int		run_updateedca(struct ieee80211com *);
static void		run_updateedca_cb(struct run_softc *, void *);
#ifdef RUN_HWCRYPTO
static int		run_set_key(struct ieee80211com *,
			    const struct ieee80211_key *, const uint8_t *);
static void		run_set_key_cb(struct run_softc *, void *);
static int		run_delete_key(struct ieee80211com *,
			    const struct ieee80211_key *);
static void		run_delete_key_cb(struct run_softc *, void *);
#endif
static void		run_calibrate_to(void *);
static void		run_calibrate_cb(struct run_softc *, void *);
static void		run_newassoc(struct ieee80211_node *, int);
static void		run_rx_frame(struct run_softc *, uint8_t *, int);
static void		run_rxeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static void		run_txeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
static int		run_tx(struct run_softc *, struct mbuf *,
			    struct ieee80211_node *);
static void		run_start(struct ifnet *);
static void		run_watchdog(struct ifnet *);
static int		run_ioctl(struct ifnet *, u_long, void *);
static void		run_select_chan_group(struct run_softc *, int);
static void		run_set_agc(struct run_softc *, uint8_t);
static void		run_set_rx_antenna(struct run_softc *, int);
static void		run_rt2870_set_chan(struct run_softc *, u_int);
static void		run_rt3070_set_chan(struct run_softc *, u_int);
static void		run_rt3572_set_chan(struct run_softc *, u_int);
static int		run_set_chan(struct run_softc *,
			    struct ieee80211_channel *);
static void		run_enable_tsf_sync(struct run_softc *);
static void		run_enable_mrr(struct run_softc *);
static void		run_set_txpreamble(struct run_softc *);
static void		run_set_basicrates(struct run_softc *);
static void		run_set_leds(struct run_softc *, uint16_t);
static void		run_set_bssid(struct run_softc *, const uint8_t *);
static void		run_set_macaddr(struct run_softc *, const uint8_t *);
static void		run_updateslot(struct ifnet *);
static void		run_updateslot_cb(struct run_softc *, void *);
static int8_t		run_rssi2dbm(struct run_softc *, uint8_t, uint8_t);
static int		run_bbp_init(struct run_softc *);
static int		run_rt3070_rf_init(struct run_softc *);
static int		run_rt3070_filter_calib(struct run_softc *, uint8_t,
			    uint8_t, uint8_t *);
static void		run_rt3070_rf_setup(struct run_softc *);
static int		run_txrx_enable(struct run_softc *);
static int		run_init(struct ifnet *);
static void		run_stop(struct ifnet *, int);
#ifndef IEEE80211_STA_ONLY
static int		run_setup_beacon(struct run_softc *);
#endif

static const struct {
	uint32_t reg;
	uint32_t val;
} rt2870_def_mac[] = {
	RT2870_DEF_MAC
};

static const struct {
	uint8_t reg;
	uint8_t val;
} rt2860_def_bbp[] = {
	RT2860_DEF_BBP
};

static const struct rfprog {
	uint8_t chan;
	uint32_t r1, r2, r3, r4;
} rt2860_rf2850[] = {
	RT2860_RF2850
};

static const struct {
	uint8_t n, r, k;
} rt3070_freqs[] = {
	RT3070_RF3052
};

static const struct {
	uint8_t reg;
	uint8_t val;
} rt3070_def_rf[] = {
	RT3070_DEF_RF
}, rt3572_def_rf[] = {
	RT3572_DEF_RF
};

static int
firmware_load(const char *dname, const char *iname, uint8_t **ucodep,
    size_t *sizep)
{
	firmware_handle_t fh;
	int error;

	if ((error = firmware_open(dname, iname, &fh)) != 0)
		return (error);
	*sizep = firmware_get_size(fh);
	if ((*ucodep = firmware_malloc(*sizep)) == NULL) {
		firmware_close(fh);
		return (ENOMEM);
	}
	if ((error = firmware_read(fh, 0, *ucodep, *sizep)) != 0)
		firmware_free(*ucodep, *sizep);
	firmware_close(fh);

	return (error);
}

static int
run_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (usb_lookup(run_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

static void
run_attach(device_t parent, device_t self, void *aux)
{
	struct run_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	int i, nrx, ntx, ntries, error;
	uint32_t ver;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(sc->sc_dev, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	error = usbd_set_config_no(sc->sc_udev, 1, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(error));
		return;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not get interface handle\n");
		return;
	}

	/*
	 * Find all bulk endpoints.  There are 7 bulk endpoints: 1 for RX
	 * and 6 for TX (4 EDCAs + HCCA + Prio).
	 * Update 03-14-2009:  some devices like the Planex GW-US300MiniS
	 * seem to have only 4 TX bulk endpoints (Fukaumi Naoki).
	 */
	nrx = ntx = 0;
	id = usbd_get_interface_descriptor(sc->sc_iface);
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL || UE_GET_XFERTYPE(ed->bmAttributes) != UE_BULK)
			continue;

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN) {
			sc->rxq.pipe_no = ed->bEndpointAddress;
			nrx++;
		} else if (ntx < 4) {
			sc->txq[ntx].pipe_no = ed->bEndpointAddress;
			ntx++;
		}
	}
	/* make sure we've got them all */
	if (nrx < 1 || ntx < 4) {
		aprint_error_dev(sc->sc_dev, "missing endpoint\n");
		return;
	}

	usb_init_task(&sc->sc_task, run_task, sc, 0);
	callout_init(&sc->scan_to, 0);
	callout_setfunc(&sc->scan_to, run_next_scan, sc);
	callout_init(&sc->calib_to, 0);
	callout_setfunc(&sc->calib_to, run_calibrate_to, sc);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 10;

	/* wait for the chip to settle */
	for (ntries = 0; ntries < 100; ntries++) {
		if (run_read(sc, RT2860_ASIC_VER_ID, &ver) != 0)
			return;
		if (ver != 0 && ver != 0xffffffff)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for NIC to initialize\n");
		return;
	}
	sc->mac_ver = ver >> 16;
	sc->mac_rev = ver & 0xffff;

	/* retrieve RF rev. no and various other things from EEPROM */
	run_read_eeprom(sc);

	aprint_error_dev(sc->sc_dev,
	    "MAC/BBP RT%04X (rev 0x%04X), RF %s (MIMO %dT%dR), address %s\n",
	    sc->mac_ver, sc->mac_rev, run_get_rf(sc->rf_rev), sc->ntxchains,
	    sc->nrxchains, ether_sprintf(ic->ic_myaddr));

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
#ifndef IEEE80211_STA_ONLY
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_HOSTAP |	/* HostAP mode supported */
#endif
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
#ifdef RUN_HWCRYPTO
	    IEEE80211_C_WEP |		/* WEP */
	    IEEE80211_C_TKIP |		/* TKIP */
	    IEEE80211_C_AES_CCM |	/* AES CCMP */
	    IEEE80211_C_TKIPMIC |	/* TKIPMIC */
#endif
	    IEEE80211_C_WME |		/* WME */
	    IEEE80211_C_WPA;		/* WPA/RSN */

	if (sc->rf_rev == RT2860_RF_2750 ||
	    sc->rf_rev == RT2860_RF_2850 ||
	    sc->rf_rev == RT3070_RF_3052) {
		/* set supported .11a rates */
		ic->ic_sup_rates[IEEE80211_MODE_11A] =
		    ieee80211_std_rateset_11a;

		/* set supported .11a channels */
		for (i = 14; i < (int)__arraycount(rt2860_rf2850); i++) {
			uint8_t chan = rt2860_rf2850[i].chan;
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A;
		}
	}

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

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
	ifp->if_init = run_init;
	ifp->if_ioctl = run_ioctl;
	ifp->if_start = run_start;
	ifp->if_watchdog = run_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ic);
	ic->ic_node_alloc = run_node_alloc;
	ic->ic_newassoc = run_newassoc;
	ic->ic_updateslot = run_updateslot;
	ic->ic_wme.wme_update = run_updateedca;
#ifdef RUN_HWCRYPTO
	ic->ic_crypto.cs_key_set = run_set_key;
	ic->ic_crypto.cs_key_delete = run_delete_key;
#endif
	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = run_newstate;
	ieee80211_media_init(ic, run_media_change, ieee80211_media_status);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN,
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RUN_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RUN_TX_RADIOTAP_PRESENT);

	ieee80211_announce(ic);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
run_detach(device_t self, int flags)
{
	struct run_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	if (ifp->if_softc == NULL)
		return (0);

	pmf_device_deregister(self);

	s = splnet();

	sc->sc_flags |= RUN_DETACHING;

	if (ifp->if_flags & IFF_RUNNING) {
		usb_rem_task(sc->sc_udev, &sc->sc_task);
		run_stop(ifp, 0);
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	bpf_detach(ifp);
	ieee80211_ifdetach(ic);
	if_detach(ifp);

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	callout_destroy(&sc->scan_to);
	callout_destroy(&sc->calib_to);

	return (0);
}

static int
run_activate(device_t self, enum devact act)
{
	struct run_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(sc->sc_ic.ic_ifp);
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static int
run_alloc_rx_ring(struct run_softc *sc)
{
	struct run_rx_ring *rxq = &sc->rxq;
	int i, error;

	error = usbd_open_pipe(sc->sc_iface, rxq->pipe_no, 0, &rxq->pipeh);
	if (error != 0)
		goto fail;

	for (i = 0; i < RUN_RX_RING_COUNT; i++) {
		struct run_rx_data *data = &rxq->data[i];

		data->sc = sc;	/* backpointer for callbacks */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, RUN_MAX_RXSZ);
		if (data->buf == NULL) {
			error = ENOMEM;
			goto fail;
		}
	}
	if (error != 0)
fail:		run_free_rx_ring(sc);
	return (error);
}

static void
run_free_rx_ring(struct run_softc *sc)
{
	struct run_rx_ring *rxq = &sc->rxq;
	int i;

	if (rxq->pipeh != NULL) {
		usbd_abort_pipe(rxq->pipeh);
		usbd_close_pipe(rxq->pipeh);
		rxq->pipeh = NULL;
	}
	for (i = 0; i < RUN_RX_RING_COUNT; i++) {
		if (rxq->data[i].xfer != NULL)
			usbd_free_xfer(rxq->data[i].xfer);
		rxq->data[i].xfer = NULL;
	}
}

static int
run_alloc_tx_ring(struct run_softc *sc, int qid)
{
	struct run_tx_ring *txq = &sc->txq[qid];
	int i, error;

	txq->cur = txq->queued = 0;

	error = usbd_open_pipe(sc->sc_iface, txq->pipe_no, 0, &txq->pipeh);
	if (error != 0)
		goto fail;

	for (i = 0; i < RUN_TX_RING_COUNT; i++) {
		struct run_tx_data *data = &txq->data[i];

		data->sc = sc;	/* backpointer for callbacks */
		data->qid = qid;

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, RUN_MAX_TXSZ);
		if (data->buf == NULL) {
			error = ENOMEM;
			goto fail;
		}
		/* zeroize the TXD + TXWI part */
		memset(data->buf, 0, sizeof (struct rt2870_txd) +
		    sizeof (struct rt2860_txwi));
	}
	if (error != 0)
fail:		run_free_tx_ring(sc, qid);
	return (error);
}

static void
run_free_tx_ring(struct run_softc *sc, int qid)
{
	struct run_tx_ring *txq = &sc->txq[qid];
	int i;

	if (txq->pipeh != NULL) {
		usbd_abort_pipe(txq->pipeh);
		usbd_close_pipe(txq->pipeh);
		txq->pipeh = NULL;
	}
	for (i = 0; i < RUN_TX_RING_COUNT; i++) {
		if (txq->data[i].xfer != NULL)
			usbd_free_xfer(txq->data[i].xfer);
		txq->data[i].xfer = NULL;
	}
}

static int
run_load_microcode(struct run_softc *sc)
{
	usb_device_request_t req;
	const char *fwname;
	u_char *ucode = NULL;	/* XXX gcc 4.8.3: maybe-uninitialized */
	size_t size = 0;	/* XXX gcc 4.8.3: maybe-uninitialized */
	uint32_t tmp;
	int ntries, error;

	/* RT3071/RT3072 use a different firmware */
	if (sc->mac_ver != 0x2860 &&
	    sc->mac_ver != 0x2872 &&
	    sc->mac_ver != 0x3070)
		fwname = "run-rt3071";
	else
		fwname = "run-rt2870";

	if ((error = firmware_load("run", fwname, &ucode, &size)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "error %d, could not read firmware %s\n", error, fwname);
		return (error);
	}
	if (size != 4096) {
		aprint_error_dev(sc->sc_dev,
		    "invalid firmware size (should be 4KB)\n");
		firmware_free(ucode, size);
		return (EINVAL);
	}

	run_read(sc, RT2860_ASIC_VER_ID, &tmp);
	/* write microcode image */
	run_write_region_1(sc, RT2870_FW_BASE, ucode, size);
	firmware_free(ucode, size);
	run_write(sc, RT2860_H2M_MAILBOX_CID, 0xffffffff);
	run_write(sc, RT2860_H2M_MAILBOX_STATUS, 0xffffffff);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_RESET;
	USETW(req.wValue, 8);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	if ((error = usbd_do_request(sc->sc_udev, &req, NULL)) != 0)
		return (error);

	usbd_delay_ms(sc->sc_udev, 10);
	run_write(sc, RT2860_H2M_MAILBOX, 0);
	if ((error = run_mcu_cmd(sc, RT2860_MCU_CMD_RFRESET, 0)) != 0)
		return (error);

	/* wait until microcontroller is ready */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((error = run_read(sc, RT2860_SYS_CTRL, &tmp)) != 0)
			return (error);
		if (tmp & RT2860_MCU_READY)
			break;
		DELAY(1000);
	}
	if (ntries == 1000) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for MCU to initialize\n");
		return (ETIMEDOUT);
	}

	sc->sc_flags |= RUN_FWLOADED;

	DPRINTF(("microcode successfully loaded after %d tries\n", ntries));
	return (0);
}

static int
run_reset(struct run_softc *sc)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_RESET;
	USETW(req.wValue, 1);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return usbd_do_request(sc->sc_udev, &req, NULL);
}

static int
run_read(struct run_softc *sc, uint16_t reg, uint32_t *val)
{
	uint32_t tmp;
	int error;

	error = run_read_region_1(sc, reg, (uint8_t *)&tmp, sizeof tmp);
	if (error == 0)
		*val = le32toh(tmp);
	else
		*val = 0xffffffff;
	return (error);
}

static int
run_read_region_1(struct run_softc *sc, uint16_t reg, uint8_t *buf, int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2870_READ_REGION_1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);
	return usbd_do_request(sc->sc_udev, &req, buf);
}

static int
run_write_2(struct run_softc *sc, uint16_t reg, uint16_t val)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_WRITE_2;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);
	return usbd_do_request(sc->sc_udev, &req, NULL);
}

static int
run_write(struct run_softc *sc, uint16_t reg, uint32_t val)
{
	int error;

	if ((error = run_write_2(sc, reg, val & 0xffff)) == 0)
		error = run_write_2(sc, reg + 2, val >> 16);
	return (error);
}

static int
run_write_region_1(struct run_softc *sc, uint16_t reg, const uint8_t *buf,
    int len)
{
#if 1
	int i, error = 0;
	/*
	 * NB: the WRITE_REGION_1 command is not stable on RT2860.
	 * We thus issue multiple WRITE_2 commands instead.
	 */
	KASSERT((len & 1) == 0);
	for (i = 0; i < len && error == 0; i += 2)
		error = run_write_2(sc, reg + i, buf[i] | buf[i + 1] << 8);
	return (error);
#else
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2870_WRITE_REGION_1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);
	return usbd_do_request(sc->sc_udev, &req, buf);
#endif
}

static int
run_set_region_4(struct run_softc *sc, uint16_t reg, uint32_t val, int count)
{
	int error = 0;

	for (; count > 0 && error == 0; count--, reg += 4)
		error = run_write(sc, reg, val);
	return (error);
}

/* Read 16-bit from eFUSE ROM (RT3070 only.) */
static int
run_efuse_read_2(struct run_softc *sc, uint16_t addr, uint16_t *val)
{
	uint32_t tmp;
	uint16_t reg;
	int error, ntries;

	if ((error = run_read(sc, RT3070_EFUSE_CTRL, &tmp)) != 0)
		return (error);

	addr *= 2;
	/*-
	 * Read one 16-byte block into registers EFUSE_DATA[0-3]:
	 * DATA0: F E D C
	 * DATA1: B A 9 8
	 * DATA2: 7 6 5 4
	 * DATA3: 3 2 1 0
	 */
	tmp &= ~(RT3070_EFSROM_MODE_MASK | RT3070_EFSROM_AIN_MASK);
	tmp |= (addr & ~0xf) << RT3070_EFSROM_AIN_SHIFT | RT3070_EFSROM_KICK;
	run_write(sc, RT3070_EFUSE_CTRL, tmp);
	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT3070_EFUSE_CTRL, &tmp)) != 0)
			return (error);
		if (!(tmp & RT3070_EFSROM_KICK))
			break;
		DELAY(2);
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	if ((tmp & RT3070_EFUSE_AOUT_MASK) == RT3070_EFUSE_AOUT_MASK) {
		*val = 0xffff;	/* address not found */
		return (0);
	}
	/* determine to which 32-bit register our 16-bit word belongs */
	reg = RT3070_EFUSE_DATA3 - (addr & 0xc);
	if ((error = run_read(sc, reg, &tmp)) != 0)
		return (error);

	*val = (addr & 2) ? tmp >> 16 : tmp & 0xffff;
	return (0);
}

static int
run_eeprom_read_2(struct run_softc *sc, uint16_t addr, uint16_t *val)
{
	usb_device_request_t req;
	uint16_t tmp;
	int error;

	addr *= 2;
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2870_EEPROM_READ;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, sizeof tmp);
	error = usbd_do_request(sc->sc_udev, &req, &tmp);
	if (error == 0)
		*val = le16toh(tmp);
	else
		*val = 0xffff;
	return (error);
}

static __inline int
run_srom_read(struct run_softc *sc, uint16_t addr, uint16_t *val)
{

	/* either eFUSE ROM or EEPROM */
	return sc->sc_srom_read(sc, addr, val);
}

static int
run_rt2870_rf_write(struct run_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_RF_CSR_CFG0, &tmp)) != 0)
			return (error);
		if (!(tmp & RT2860_RF_REG_CTRL))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	/* RF registers are 24-bit on the RT2860 */
	tmp = RT2860_RF_REG_CTRL | 24 << RT2860_RF_REG_WIDTH_SHIFT |
	    (val & 0x3fffff) << 2 | (reg & 3);
	return run_write(sc, RT2860_RF_CSR_CFG0, tmp);
}

static int
run_rt3070_rf_read(struct run_softc *sc, uint8_t reg, uint8_t *val)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT3070_RF_CSR_CFG, &tmp)) != 0)
			return (error);
		if (!(tmp & RT3070_RF_KICK))
			break;
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	tmp = RT3070_RF_KICK | reg << 8;
	if ((error = run_write(sc, RT3070_RF_CSR_CFG, tmp)) != 0)
		return (error);

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT3070_RF_CSR_CFG, &tmp)) != 0)
			return (error);
		if (!(tmp & RT3070_RF_KICK))
			break;
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	*val = tmp & 0xff;
	return (0);
}

static int
run_rt3070_rf_write(struct run_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT3070_RF_CSR_CFG, &tmp)) != 0)
			return (error);
		if (!(tmp & RT3070_RF_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	tmp = RT3070_RF_WRITE | RT3070_RF_KICK | reg << 8 | val;
	return run_write(sc, RT3070_RF_CSR_CFG, tmp);
}

static int
run_bbp_read(struct run_softc *sc, uint8_t reg, uint8_t *val)
{
	uint32_t tmp;
	int ntries, error;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_BBP_CSR_CFG, &tmp)) != 0)
			return (error);
		if (!(tmp & RT2860_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	tmp = RT2860_BBP_CSR_READ | RT2860_BBP_CSR_KICK | reg << 8;
	if ((error = run_write(sc, RT2860_BBP_CSR_CFG, tmp)) != 0)
		return (error);

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_BBP_CSR_CFG, &tmp)) != 0)
			return (error);
		if (!(tmp & RT2860_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	*val = tmp & 0xff;
	return (0);
}

static int
run_bbp_write(struct run_softc *sc, uint8_t reg, uint8_t val)
{
	uint32_t tmp;
	int ntries, error;

	for (ntries = 0; ntries < 10; ntries++) {
		if ((error = run_read(sc, RT2860_BBP_CSR_CFG, &tmp)) != 0)
			return (error);
		if (!(tmp & RT2860_BBP_CSR_KICK))
			break;
	}
	if (ntries == 10)
		return (ETIMEDOUT);

	tmp = RT2860_BBP_CSR_KICK | reg << 8 | val;
	return run_write(sc, RT2860_BBP_CSR_CFG, tmp);
}

/*
 * Send a command to the 8051 microcontroller unit.
 */
static int
run_mcu_cmd(struct run_softc *sc, uint8_t cmd, uint16_t arg)
{
	uint32_t tmp;
	int error, ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT2860_H2M_MAILBOX, &tmp)) != 0)
			return (error);
		if (!(tmp & RT2860_H2M_BUSY))
			break;
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	tmp = RT2860_H2M_BUSY | RT2860_TOKEN_NO_INTR << 16 | arg;
	if ((error = run_write(sc, RT2860_H2M_MAILBOX, tmp)) == 0)
		error = run_write(sc, RT2860_HOST_CMD, cmd);
	return (error);
}

/*
 * Add `delta' (signed) to each 4-bit sub-word of a 32-bit word.
 * Used to adjust per-rate Tx power registers.
 */
static __inline uint32_t
b4inc(uint32_t b32, int8_t delta)
{
	int8_t i, b4;

	for (i = 0; i < 8; i++) {
		b4 = b32 & 0xf;
		b4 += delta;
		if (b4 < 0)
			b4 = 0;
		else if (b4 > 0xf)
			b4 = 0xf;
		b32 = b32 >> 4 | b4 << 28;
	}
	return (b32);
}

static const char *
run_get_rf(int rev)
{
	switch (rev) {
	case RT2860_RF_2820:	return "RT2820";
	case RT2860_RF_2850:	return "RT2850";
	case RT2860_RF_2720:	return "RT2720";
	case RT2860_RF_2750:	return "RT2750";
	case RT3070_RF_3020:	return "RT3020";
	case RT3070_RF_2020:	return "RT2020";
	case RT3070_RF_3021:	return "RT3021";
	case RT3070_RF_3022:	return "RT3022";
	case RT3070_RF_3052:	return "RT3052";
	}
	return "unknown";
}

static int
run_read_eeprom(struct run_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int8_t delta_2ghz, delta_5ghz;
	uint32_t tmp;
	uint16_t val;
	int ridx, ant, i;

	/* check whether the ROM is eFUSE ROM or EEPROM */
	sc->sc_srom_read = run_eeprom_read_2;
	if (sc->mac_ver >= 0x3070) {
		run_read(sc, RT3070_EFUSE_CTRL, &tmp);
		DPRINTF(("EFUSE_CTRL=0x%08x\n", tmp));
		if (tmp & RT3070_SEL_EFUSE)
			sc->sc_srom_read = run_efuse_read_2;
	}

	/* read ROM version */
	run_srom_read(sc, RT2860_EEPROM_VERSION, &val);
	DPRINTF(("EEPROM rev=%d, FAE=%d\n", val & 0xff, val >> 8));

	/* read MAC address */
	run_srom_read(sc, RT2860_EEPROM_MAC01, &val);
	ic->ic_myaddr[0] = val & 0xff;
	ic->ic_myaddr[1] = val >> 8;
	run_srom_read(sc, RT2860_EEPROM_MAC23, &val);
	ic->ic_myaddr[2] = val & 0xff;
	ic->ic_myaddr[3] = val >> 8;
	run_srom_read(sc, RT2860_EEPROM_MAC45, &val);
	ic->ic_myaddr[4] = val & 0xff;
	ic->ic_myaddr[5] = val >> 8;

	/* read vendor BBP settings */
	for (i = 0; i < 10; i++) {
		run_srom_read(sc, RT2860_EEPROM_BBP_BASE + i, &val);
		sc->bbp[i].val = val & 0xff;
		sc->bbp[i].reg = val >> 8;
		DPRINTF(("BBP%d=0x%02x\n", sc->bbp[i].reg, sc->bbp[i].val));
	}
	if (sc->mac_ver >= 0x3071) {
		/* read vendor RF settings */
		for (i = 0; i < 10; i++) {
			run_srom_read(sc, RT3071_EEPROM_RF_BASE + i, &val);
			sc->rf[i].val = val & 0xff;
			sc->rf[i].reg = val >> 8;
			DPRINTF(("RF%d=0x%02x\n", sc->rf[i].reg,
			    sc->rf[i].val));
		}
	}

	/* read RF frequency offset from EEPROM */
	run_srom_read(sc, RT2860_EEPROM_FREQ_LEDS, &val);
	sc->freq = ((val & 0xff) != 0xff) ? val & 0xff : 0;
	DPRINTF(("EEPROM freq offset %d\n", sc->freq & 0xff));

	if ((val >> 8) != 0xff) {
		/* read LEDs operating mode */
		sc->leds = val >> 8;
		run_srom_read(sc, RT2860_EEPROM_LED1, &sc->led[0]);
		run_srom_read(sc, RT2860_EEPROM_LED2, &sc->led[1]);
		run_srom_read(sc, RT2860_EEPROM_LED3, &sc->led[2]);
	} else {
		/* broken EEPROM, use default settings */
		sc->leds = 0x01;
		sc->led[0] = 0x5555;
		sc->led[1] = 0x2221;
		sc->led[2] = 0x5627;	/* differs from RT2860 */
	}
	DPRINTF(("EEPROM LED mode=0x%02x, LEDs=0x%04x/0x%04x/0x%04x\n",
	    sc->leds, sc->led[0], sc->led[1], sc->led[2]));

	/* read RF information */
	run_srom_read(sc, RT2860_EEPROM_ANTENNA, &val);
	if (val == 0xffff) {
		DPRINTF(("invalid EEPROM antenna info, using default\n"));
		if (sc->mac_ver == 0x3572) {
			/* default to RF3052 2T2R */
			sc->rf_rev = RT3070_RF_3052;
			sc->ntxchains = 2;
			sc->nrxchains = 2;
		} else if (sc->mac_ver >= 0x3070) {
			/* default to RF3020 1T1R */
			sc->rf_rev = RT3070_RF_3020;
			sc->ntxchains = 1;
			sc->nrxchains = 1;
		} else {
			/* default to RF2820 1T2R */
			sc->rf_rev = RT2860_RF_2820;
			sc->ntxchains = 1;
			sc->nrxchains = 2;
		}
	} else {
		sc->rf_rev = (val >> 8) & 0xf;
		sc->ntxchains = (val >> 4) & 0xf;
		sc->nrxchains = val & 0xf;
	}
	DPRINTF(("EEPROM RF rev=0x%02x chains=%dT%dR\n",
	    sc->rf_rev, sc->ntxchains, sc->nrxchains));

	run_srom_read(sc, RT2860_EEPROM_CONFIG, &val);
	DPRINTF(("EEPROM CFG 0x%04x\n", val));
	/* check if driver should patch the DAC issue */
	if ((val >> 8) != 0xff)
		sc->patch_dac = (val >> 15) & 1;
	if ((val & 0xff) != 0xff) {
		sc->ext_5ghz_lna = (val >> 3) & 1;
		sc->ext_2ghz_lna = (val >> 2) & 1;
		/* check if RF supports automatic Tx access gain control */
		sc->calib_2ghz = sc->calib_5ghz = (val >> 1) & 1;
		/* check if we have a hardware radio switch */
		sc->rfswitch = val & 1;
	}

	/* read power settings for 2GHz channels */
	for (i = 0; i < 14; i += 2) {
		run_srom_read(sc, RT2860_EEPROM_PWR2GHZ_BASE1 + i / 2, &val);
		sc->txpow1[i + 0] = (int8_t)(val & 0xff);
		sc->txpow1[i + 1] = (int8_t)(val >> 8);

		run_srom_read(sc, RT2860_EEPROM_PWR2GHZ_BASE2 + i / 2, &val);
		sc->txpow2[i + 0] = (int8_t)(val & 0xff);
		sc->txpow2[i + 1] = (int8_t)(val >> 8);
	}
	/* fix broken Tx power entries */
	for (i = 0; i < 14; i++) {
		if (sc->txpow1[i] < 0 || sc->txpow1[i] > 31)
			sc->txpow1[i] = 5;
		if (sc->txpow2[i] < 0 || sc->txpow2[i] > 31)
			sc->txpow2[i] = 5;
		DPRINTF(("chan %d: power1=%d, power2=%d\n",
		    rt2860_rf2850[i].chan, sc->txpow1[i], sc->txpow2[i]));
	}
	/* read power settings for 5GHz channels */
	for (i = 0; i < 40; i += 2) {
		run_srom_read(sc, RT2860_EEPROM_PWR5GHZ_BASE1 + i / 2, &val);
		sc->txpow1[i + 14] = (int8_t)(val & 0xff);
		sc->txpow1[i + 15] = (int8_t)(val >> 8);

		run_srom_read(sc, RT2860_EEPROM_PWR5GHZ_BASE2 + i / 2, &val);
		sc->txpow2[i + 14] = (int8_t)(val & 0xff);
		sc->txpow2[i + 15] = (int8_t)(val >> 8);
	}
	/* fix broken Tx power entries */
	for (i = 0; i < 40; i++) {
		if (sc->txpow1[14 + i] < -7 || sc->txpow1[14 + i] > 15)
			sc->txpow1[14 + i] = 5;
		if (sc->txpow2[14 + i] < -7 || sc->txpow2[14 + i] > 15)
			sc->txpow2[14 + i] = 5;
		DPRINTF(("chan %d: power1=%d, power2=%d\n",
		    rt2860_rf2850[14 + i].chan, sc->txpow1[14 + i],
		    sc->txpow2[14 + i]));
	}

	/* read Tx power compensation for each Tx rate */
	run_srom_read(sc, RT2860_EEPROM_DELTAPWR, &val);
	delta_2ghz = delta_5ghz = 0;
	if ((val & 0xff) != 0xff && (val & 0x80)) {
		delta_2ghz = val & 0xf;
		if (!(val & 0x40))	/* negative number */
			delta_2ghz = -delta_2ghz;
	}
	val >>= 8;
	if ((val & 0xff) != 0xff && (val & 0x80)) {
		delta_5ghz = val & 0xf;
		if (!(val & 0x40))	/* negative number */
			delta_5ghz = -delta_5ghz;
	}
	DPRINTF(("power compensation=%d (2GHz), %d (5GHz)\n",
	    delta_2ghz, delta_5ghz));

	for (ridx = 0; ridx < 5; ridx++) {
		uint32_t reg;

		run_srom_read(sc, RT2860_EEPROM_RPWR + ridx * 2, &val);
		reg = val;
		run_srom_read(sc, RT2860_EEPROM_RPWR + ridx * 2 + 1, &val);
		reg |= (uint32_t)val << 16;

		sc->txpow20mhz[ridx] = reg;
		sc->txpow40mhz_2ghz[ridx] = b4inc(reg, delta_2ghz);
		sc->txpow40mhz_5ghz[ridx] = b4inc(reg, delta_5ghz);

		DPRINTF(("ridx %d: power 20MHz=0x%08x, 40MHz/2GHz=0x%08x, "
		    "40MHz/5GHz=0x%08x\n", ridx, sc->txpow20mhz[ridx],
		    sc->txpow40mhz_2ghz[ridx], sc->txpow40mhz_5ghz[ridx]));
	}

	/* read RSSI offsets and LNA gains from EEPROM */
	run_srom_read(sc, RT2860_EEPROM_RSSI1_2GHZ, &val);
	sc->rssi_2ghz[0] = val & 0xff;	/* Ant A */
	sc->rssi_2ghz[1] = val >> 8;	/* Ant B */
	run_srom_read(sc, RT2860_EEPROM_RSSI2_2GHZ, &val);
	if (sc->mac_ver >= 0x3070) {
		/*
		 * On RT3070 chips (limited to 2 Rx chains), this ROM
		 * field contains the Tx mixer gain for the 2GHz band.
		 */
		if ((val & 0xff) != 0xff)
			sc->txmixgain_2ghz = val & 0x7;
		DPRINTF(("tx mixer gain=%u (2GHz)\n", sc->txmixgain_2ghz));
	} else {
		sc->rssi_2ghz[2] = val & 0xff;	/* Ant C */
	}
	sc->lna[2] = val >> 8;		/* channel group 2 */

	run_srom_read(sc, RT2860_EEPROM_RSSI1_5GHZ, &val);
	sc->rssi_5ghz[0] = val & 0xff;	/* Ant A */
	sc->rssi_5ghz[1] = val >> 8;	/* Ant B */
	run_srom_read(sc, RT2860_EEPROM_RSSI2_5GHZ, &val);
	if (sc->mac_ver == 0x3572) {
		/*
		 * On RT3572 chips (limited to 2 Rx chains), this ROM
		 * field contains the Tx mixer gain for the 5GHz band.
		 */
		if ((val & 0xff) != 0xff)
			sc->txmixgain_5ghz = val & 0x7;
		DPRINTF(("tx mixer gain=%u (5GHz)\n", sc->txmixgain_5ghz));
	} else {
		sc->rssi_5ghz[2] = val & 0xff;	/* Ant C */
	}
	sc->lna[3] = val >> 8;		/* channel group 3 */

	run_srom_read(sc, RT2860_EEPROM_LNA, &val);
	sc->lna[0] = val & 0xff;	/* channel group 0 */
	sc->lna[1] = val >> 8;		/* channel group 1 */

	/* fix broken 5GHz LNA entries */
	if (sc->lna[2] == 0 || sc->lna[2] == 0xff) {
		DPRINTF(("invalid LNA for channel group %d\n", 2));
		sc->lna[2] = sc->lna[1];
	}
	if (sc->lna[3] == 0 || sc->lna[3] == 0xff) {
		DPRINTF(("invalid LNA for channel group %d\n", 3));
		sc->lna[3] = sc->lna[1];
	}

	/* fix broken RSSI offset entries */
	for (ant = 0; ant < 3; ant++) {
		if (sc->rssi_2ghz[ant] < -10 || sc->rssi_2ghz[ant] > 10) {
			DPRINTF(("invalid RSSI%d offset: %d (2GHz)\n",
			    ant + 1, sc->rssi_2ghz[ant]));
			sc->rssi_2ghz[ant] = 0;
		}
		if (sc->rssi_5ghz[ant] < -10 || sc->rssi_5ghz[ant] > 10) {
			DPRINTF(("invalid RSSI%d offset: %d (5GHz)\n",
			    ant + 1, sc->rssi_5ghz[ant]));
			sc->rssi_5ghz[ant] = 0;
		}
	}
	return (0);
}

static struct ieee80211_node *
run_node_alloc(struct ieee80211_node_table *nt)
{
	struct run_node *rn =
	    malloc(sizeof (struct run_node), M_DEVBUF, M_NOWAIT | M_ZERO);
	return (rn) ? &rn->ni : NULL;
}

static int
run_media_change(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t rate, ridx;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return (error);

	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		for (ridx = 0; ridx <= RT2860_RIDX_MAX; ridx++)
			if (rt2860_rates[ridx].rate == rate)
				break;
		sc->fixed_ridx = ridx;
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		run_init(ifp);

	return (0);
}

static void
run_next_scan(void *arg)
{
	struct run_softc *sc = arg;

	if (sc->sc_ic.ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&sc->sc_ic);
}

static void
run_task(void *arg)
{
	struct run_softc *sc = arg;
	struct run_host_cmd_ring *ring = &sc->cmdq;
	struct run_host_cmd *cmd;
	int s;

	/* process host commands */
	s = splusb();
	while (ring->next != ring->cur) {
		cmd = &ring->cmd[ring->next];
		splx(s);
		/* callback */
		cmd->cb(sc, cmd->data);
		s = splusb();
		ring->queued--;
		ring->next = (ring->next + 1) % RUN_HOST_CMD_RING_COUNT;
	}
	wakeup(ring);
	splx(s);
}

static void
run_do_async(struct run_softc *sc, void (*cb)(struct run_softc *, void *),
    void *arg, int len)
{
	struct run_host_cmd_ring *ring = &sc->cmdq;
	struct run_host_cmd *cmd;
	int s;

	if (sc->sc_flags & RUN_DETACHING)
		return;

	s = splusb();
	cmd = &ring->cmd[ring->cur];
	cmd->cb = cb;
	KASSERT(len <= sizeof (cmd->data));
	memcpy(cmd->data, arg, len);
	ring->cur = (ring->cur + 1) % RUN_HOST_CMD_RING_COUNT;

	/* if there is no pending command already, schedule a task */
	if (++ring->queued == 1)
		usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);
	splx(s);
}

static int
run_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct run_cmd_newstate cmd;

	callout_stop(&sc->scan_to);
	callout_stop(&sc->calib_to);

	/* do it in a process context */
	cmd.state = nstate;
	cmd.arg = arg;
	run_do_async(sc, run_newstate_cb, &cmd, sizeof cmd);
	return (0);
}

static void
run_newstate_cb(struct run_softc *sc, void *arg)
{
	struct run_cmd_newstate *cmd = arg;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;
	struct ieee80211_node *ni;
	uint32_t tmp, sta[3];
	uint8_t wcid;
	int s;

	s = splnet();
	ostate = ic->ic_state;

	if (ostate == IEEE80211_S_RUN) {
		/* turn link LED off */
		run_set_leds(sc, RT2860_LED_RADIO);
	}

	switch (cmd->state) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			run_read(sc, RT2860_BCN_TIME_CFG, &tmp);
			run_write(sc, RT2860_BCN_TIME_CFG,
			    tmp & ~(RT2860_BCN_TX_EN | RT2860_TSF_TIMER_EN |
			    RT2860_TBTT_TIMER_EN));
		}
		break;

	case IEEE80211_S_SCAN:
		run_set_chan(sc, ic->ic_curchan);
		callout_schedule(&sc->scan_to, hz / 5);
		break;

	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		run_set_chan(sc, ic->ic_curchan);
		break;

	case IEEE80211_S_RUN:
		run_set_chan(sc, ic->ic_curchan);

		ni = ic->ic_bss;

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			run_updateslot(ifp);
			run_enable_mrr(sc);
			run_set_txpreamble(sc);
			run_set_basicrates(sc);
			run_set_bssid(sc, ni->ni_bssid);
		}
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS)
			(void)run_setup_beacon(sc);
#endif
		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* add BSS entry to the WCID table */
			wcid = RUN_AID2WCID(ni->ni_associd);
			run_write_region_1(sc, RT2860_WCID_ENTRY(wcid),
			    ni->ni_macaddr, IEEE80211_ADDR_LEN);

			/* fake a join to init the tx rate */
			run_newassoc(ni, 1);
		}
		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			run_enable_tsf_sync(sc);

			/* clear statistic registers used by AMRR */
			run_read_region_1(sc, RT2860_TX_STA_CNT0,
			    (uint8_t *)sta, sizeof sta);
			/* start calibration timer */
			callout_schedule(&sc->calib_to, hz);
		}

		/* turn link LED on */
		run_set_leds(sc, RT2860_LED_RADIO |
		    (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan) ?
		     RT2860_LED_LINK_2GHZ : RT2860_LED_LINK_5GHZ));
		break;
	}
	(void)sc->sc_newstate(ic, cmd->state, cmd->arg);
	splx(s);
}

static int
run_updateedca(struct ieee80211com *ic)
{

	/* do it in a process context */
	run_do_async(ic->ic_ifp->if_softc, run_updateedca_cb, NULL, 0);
	return (0);
}

/* ARGSUSED */
static void
run_updateedca_cb(struct run_softc *sc, void *arg)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int s, aci;

	s = splnet();
	/* update MAC TX configuration registers */
	for (aci = 0; aci < WME_NUM_AC; aci++) {
		run_write(sc, RT2860_EDCA_AC_CFG(aci),
		    ic->ic_wme.wme_params[aci].wmep_logcwmax << 16 |
		    ic->ic_wme.wme_params[aci].wmep_logcwmin << 12 |
		    ic->ic_wme.wme_params[aci].wmep_aifsn  <<  8 |
		    ic->ic_wme.wme_params[aci].wmep_txopLimit);
	}

	/* update SCH/DMA registers too */
	run_write(sc, RT2860_WMM_AIFSN_CFG,
	    ic->ic_wme.wme_params[WME_AC_VO].wmep_aifsn  << 12 |
	    ic->ic_wme.wme_params[WME_AC_VI].wmep_aifsn  <<  8 |
	    ic->ic_wme.wme_params[WME_AC_BK].wmep_aifsn  <<  4 |
	    ic->ic_wme.wme_params[WME_AC_BE].wmep_aifsn);
	run_write(sc, RT2860_WMM_CWMIN_CFG,
	    ic->ic_wme.wme_params[WME_AC_VO].wmep_logcwmin << 12 |
	    ic->ic_wme.wme_params[WME_AC_VI].wmep_logcwmin <<  8 |
	    ic->ic_wme.wme_params[WME_AC_BK].wmep_logcwmin <<  4 |
	    ic->ic_wme.wme_params[WME_AC_BE].wmep_logcwmin);
	run_write(sc, RT2860_WMM_CWMAX_CFG,
	    ic->ic_wme.wme_params[WME_AC_VO].wmep_logcwmax << 12 |
	    ic->ic_wme.wme_params[WME_AC_VI].wmep_logcwmax <<  8 |
	    ic->ic_wme.wme_params[WME_AC_BK].wmep_logcwmax <<  4 |
	    ic->ic_wme.wme_params[WME_AC_BE].wmep_logcwmax);
	run_write(sc, RT2860_WMM_TXOP0_CFG,
	    ic->ic_wme.wme_params[WME_AC_BK].wmep_txopLimit << 16 |
	    ic->ic_wme.wme_params[WME_AC_BE].wmep_txopLimit);
	run_write(sc, RT2860_WMM_TXOP1_CFG,
	    ic->ic_wme.wme_params[WME_AC_VO].wmep_txopLimit << 16 |
	    ic->ic_wme.wme_params[WME_AC_VI].wmep_txopLimit);
	splx(s);
}

#ifdef RUN_HWCRYPTO
static int
run_set_key(struct ieee80211com *ic, const struct ieee80211_key *k,
    const uint8_t *mac)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct ieee80211_node *ni = ic->ic_bss;
	struct run_cmd_key cmd;

	/* do it in a process context */
	cmd.key = *k;
	cmd.associd = (ni != NULL) ? ni->ni_associd : 0;
	run_do_async(sc, run_set_key_cb, &cmd, sizeof cmd);
	return 1;
}

static void
run_set_key_cb(struct run_softc *sc, void *arg)
{
#ifndef IEEE80211_STA_ONLY
	struct ieee80211com *ic = &sc->sc_ic;
#endif
	struct run_cmd_key *cmd = arg;
	struct ieee80211_key *k = &cmd->key;
	uint32_t attr;
	uint16_t base;
	uint8_t mode, wcid, iv[8];

	/* map net80211 cipher to RT2860 security mode */
	switch (k->wk_cipher->ic_cipher) {
	case IEEE80211_CIPHER_WEP:
		k->wk_flags |= IEEE80211_KEY_GROUP; /* XXX */
		if (k->wk_keylen == 5)
			mode = RT2860_MODE_WEP40;
		else
			mode = RT2860_MODE_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		mode = RT2860_MODE_TKIP;
		break;
	case IEEE80211_CIPHER_AES_CCM:
		mode = RT2860_MODE_AES_CCMP;
		break;
	default:
		return;
	}

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		wcid = 0;	/* NB: update WCID0 for group keys */
		base = RT2860_SKEY(0, k->wk_keyix);
	} else {
		wcid = RUN_AID2WCID(cmd->associd);
		base = RT2860_PKEY(wcid);
	}

	if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) {
		run_write_region_1(sc, base, k->wk_key, 16);
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			run_write_region_1(sc, base + 16, &k->wk_key[16], 8);
			run_write_region_1(sc, base + 24, &k->wk_key[24], 8);
		} else
#endif
		{
			run_write_region_1(sc, base + 16, &k->wk_key[24], 8);
			run_write_region_1(sc, base + 24, &k->wk_key[16], 8);
		}
	} else {
		/* roundup len to 16-bit: XXX fix write_region_1() instead */
		run_write_region_1(sc, base, k->wk_key,
		    (k->wk_keylen + 1) & ~1);
	}

	if (!(k->wk_flags & IEEE80211_KEY_GROUP) ||
	    (k->wk_flags & IEEE80211_KEY_XMIT)) {
		/* set initial packet number in IV+EIV */
		if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP) {
			memset(iv, 0, sizeof iv);
			iv[3] = sc->sc_ic.ic_crypto.cs_def_txkey << 6;
		} else {
			if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) {
				iv[0] = k->wk_keytsc >> 8;
				iv[1] = (iv[0] | 0x20) & 0x7f;
				iv[2] = k->wk_keytsc;
			} else /* CCMP */ {
				iv[0] = k->wk_keytsc;
				iv[1] = k->wk_keytsc >> 8;
				iv[2] = 0;
			}
			iv[3] = k->wk_keyix << 6 | IEEE80211_WEP_EXTIV;
			iv[4] = k->wk_keytsc >> 16;
			iv[5] = k->wk_keytsc >> 24;
			iv[6] = k->wk_keytsc >> 32;
			iv[7] = k->wk_keytsc >> 40;
		}
		run_write_region_1(sc, RT2860_IVEIV(wcid), iv, 8);
	}

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		/* install group key */
		run_read(sc, RT2860_SKEY_MODE_0_7, &attr);
		attr &= ~(0xf << (k->wk_keyix * 4));
		attr |= mode << (k->wk_keyix * 4);
		run_write(sc, RT2860_SKEY_MODE_0_7, attr);
	} else {
		/* install pairwise key */
		run_read(sc, RT2860_WCID_ATTR(wcid), &attr);
		attr = (attr & ~0xf) | (mode << 1) | RT2860_RX_PKEY_EN;
		run_write(sc, RT2860_WCID_ATTR(wcid), attr);
	}
}

static int
run_delete_key(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	struct run_softc *sc = ic->ic_ifp->if_softc;
	struct ieee80211_node *ni = ic->ic_bss;
	struct run_cmd_key cmd;

	/* do it in a process context */
	cmd.key = *k;
	cmd.associd = (ni != NULL) ? ni->ni_associd : 0;
	run_do_async(sc, run_delete_key_cb, &cmd, sizeof cmd);
	return 1;
}

static void
run_delete_key_cb(struct run_softc *sc, void *arg)
{
	struct run_cmd_key *cmd = arg;
	struct ieee80211_key *k = &cmd->key;
	uint32_t attr;
	uint8_t wcid;

	if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP)
		k->wk_flags |= IEEE80211_KEY_GROUP; /* XXX */

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		/* remove group key */
		run_read(sc, RT2860_SKEY_MODE_0_7, &attr);
		attr &= ~(0xf << (k->wk_keyix * 4));
		run_write(sc, RT2860_SKEY_MODE_0_7, attr);

	} else {
		/* remove pairwise key */
		wcid = RUN_AID2WCID(cmd->associd);
		run_read(sc, RT2860_WCID_ATTR(wcid), &attr);
		attr &= ~0xf;
		run_write(sc, RT2860_WCID_ATTR(wcid), attr);
	}
}
#endif

static void
run_calibrate_to(void *arg)
{

	/* do it in a process context */
	run_do_async(arg, run_calibrate_cb, NULL, 0);
	/* next timeout will be rescheduled in the calibration task */
}

/* ARGSUSED */
static void
run_calibrate_cb(struct run_softc *sc, void *arg)
{
	struct ifnet *ifp = &sc->sc_if;
	uint32_t sta[3];
	int s, error;

	/* read statistic counters (clear on read) and update AMRR state */
	error = run_read_region_1(sc, RT2860_TX_STA_CNT0, (uint8_t *)sta,
	    sizeof sta);
	if (error != 0)
		goto skip;

	DPRINTF(("retrycnt=%d txcnt=%d failcnt=%d\n",
	    le32toh(sta[1]) >> 16, le32toh(sta[1]) & 0xffff,
	    le32toh(sta[0]) & 0xffff));

	s = splnet();
	/* count failed TX as errors */
	ifp->if_oerrors += le32toh(sta[0]) & 0xffff;

	sc->amn.amn_retrycnt =
	    (le32toh(sta[0]) & 0xffff) +	/* failed TX count */
	    (le32toh(sta[1]) >> 16);		/* TX retransmission count */

	sc->amn.amn_txcnt =
	    sc->amn.amn_retrycnt +
	    (le32toh(sta[1]) & 0xffff);		/* successful TX count */

	ieee80211_amrr_choose(&sc->amrr, sc->sc_ic.ic_bss, &sc->amn);
	splx(s);

skip:	callout_schedule(&sc->calib_to, hz);
}

static void
run_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct run_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	struct run_node *rn = (void *)ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	uint8_t rate;
	int ridx, i, j;

	DPRINTF(("new assoc isnew=%d addr=%s\n",
	    isnew, ether_sprintf(ni->ni_macaddr)));

	ieee80211_amrr_node_init(&sc->amrr, &sc->amn);
	/* start at lowest available bit-rate, AMRR will raise */
	ni->ni_txrate = 0;

	for (i = 0; i < rs->rs_nrates; i++) {
		rate = rs->rs_rates[i] & IEEE80211_RATE_VAL;
		/* convert 802.11 rate to hardware rate index */
		for (ridx = 0; ridx < RT2860_RIDX_MAX; ridx++)
			if (rt2860_rates[ridx].rate == rate)
				break;
		rn->ridx[i] = ridx;
		/* determine rate of control response frames */
		for (j = i; j >= 0; j--) {
			if ((rs->rs_rates[j] & IEEE80211_RATE_BASIC) &&
			    rt2860_rates[rn->ridx[i]].phy ==
			    rt2860_rates[rn->ridx[j]].phy)
				break;
		}
		if (j >= 0) {
			rn->ctl_ridx[i] = rn->ridx[j];
		} else {
			/* no basic rate found, use mandatory one */
			rn->ctl_ridx[i] = rt2860_rates[ridx].ctl_ridx;
		}
		DPRINTF(("rate=0x%02x ridx=%d ctl_ridx=%d\n",
		    rs->rs_rates[i], rn->ridx[i], rn->ctl_ridx[i]));
	}
}

/*
 * Return the Rx chain with the highest RSSI for a given frame.
 */
static __inline uint8_t
run_maxrssi_chain(struct run_softc *sc, const struct rt2860_rxwi *rxwi)
{
	uint8_t rxchain = 0;

	if (sc->nrxchains > 1) {
		if (rxwi->rssi[1] > rxwi->rssi[rxchain])
			rxchain = 1;
		if (sc->nrxchains > 2)
			if (rxwi->rssi[2] > rxwi->rssi[rxchain])
				rxchain = 2;
	}
	return (rxchain);
}

static void
run_rx_frame(struct run_softc *sc, uint8_t *buf, int dmalen)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct rt2870_rxd *rxd;
	struct rt2860_rxwi *rxwi;
	struct mbuf *m;
	uint32_t flags;
	uint16_t len, phy;
	uint8_t ant, rssi;
	int s;
#ifdef RUN_HWCRYPTO
	int decrypted = 0;
#endif

	rxwi = (struct rt2860_rxwi *)buf;
	len = le16toh(rxwi->len) & 0xfff;
	if (__predict_false(len > dmalen)) {
		DPRINTF(("bad RXWI length %u > %u\n", len, dmalen));
		return;
	}
	/* Rx descriptor is located at the end */
	rxd = (struct rt2870_rxd *)(buf + dmalen);
	flags = le32toh(rxd->flags);

	if (__predict_false(flags & (RT2860_RX_CRCERR | RT2860_RX_ICVERR))) {
		ifp->if_ierrors++;
		return;
	}

	wh = (struct ieee80211_frame *)(rxwi + 1);
#ifdef RUN_HWCRYPTO
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		decrypted = 1;
	}
#endif

	if (__predict_false((flags & RT2860_RX_MICERR))) {
		/* report MIC failures to net80211 for TKIP */
		ieee80211_notify_michael_failure(ic, wh, 0/* XXX */);
		ifp->if_ierrors++;
		return;
	}

	if (flags & RT2860_RX_L2PAD) {
		u_int hdrlen = ieee80211_hdrspace(ic, wh);
		ovbcopy(wh, (uint8_t *)wh + 2, hdrlen);
		wh = (struct ieee80211_frame *)((uint8_t *)wh + 2);
	}

	/* could use m_devget but net80211 wants contig mgmt frames */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (__predict_false(m == NULL)) {
		ifp->if_ierrors++;
		return;
	}
	if (len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (__predict_false(!(m->m_flags & M_EXT))) {
			ifp->if_ierrors++;
			m_freem(m);
			return;
		}
	}
	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	memcpy(mtod(m, void *), wh, len);
	m->m_pkthdr.len = m->m_len = len;

	ant = run_maxrssi_chain(sc, rxwi);
	rssi = rxwi->rssi[ant];

	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct run_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_antsignal = rssi;
		tap->wr_antenna = ant;
		tap->wr_dbm_antsignal = run_rssi2dbm(sc, rssi, ant);
		tap->wr_rate = 2;	/* in case it can't be found below */
		phy = le16toh(rxwi->phy);
		switch (phy & RT2860_PHY_MODE) {
		case RT2860_PHY_CCK:
			switch ((phy & RT2860_PHY_MCS) & ~RT2860_PHY_SHPRE) {
			case 0:	tap->wr_rate =   2; break;
			case 1:	tap->wr_rate =   4; break;
			case 2:	tap->wr_rate =  11; break;
			case 3:	tap->wr_rate =  22; break;
			}
			if (phy & RT2860_PHY_SHPRE)
				tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
			break;
		case RT2860_PHY_OFDM:
			switch (phy & RT2860_PHY_MCS) {
			case 0:	tap->wr_rate =  12; break;
			case 1:	tap->wr_rate =  18; break;
			case 2:	tap->wr_rate =  24; break;
			case 3:	tap->wr_rate =  36; break;
			case 4:	tap->wr_rate =  48; break;
			case 5:	tap->wr_rate =  72; break;
			case 6:	tap->wr_rate =  96; break;
			case 7:	tap->wr_rate = 108; break;
			}
			break;
		}
		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	s = splnet();
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);
#ifdef RUN_HWCRYPTO
	if (decrypted) {
		uint32_t icflags = ic->ic_flags;

		ic->ic_flags &= ~IEEE80211_F_DROPUNENC; /* XXX */
		ieee80211_input(ic, m, ni, rssi, 0);
		ic->ic_flags = icflags;
	} else
#endif
	ieee80211_input(ic, m, ni, rssi, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);

	/*
	 * In HostAP mode, ieee80211_input() will enqueue packets in if_snd
	 * without calling if_start().
	 */
	if (!IFQ_IS_EMPTY(&ifp->if_snd) && !(ifp->if_flags & IFF_OACTIVE))
		run_start(ifp);

	splx(s);
}

static void
run_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct run_rx_data *data = priv;
	struct run_softc *sc = data->sc;
	uint8_t *buf;
	uint32_t dmalen;
	int xferlen;

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTF(("RX status=%d\n", status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->rxq.pipeh);
		if (status != USBD_CANCELLED)
			goto skip;
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &xferlen, NULL);

	if (__predict_false(xferlen < (int)(sizeof(uint32_t) +
	    sizeof(struct rt2860_rxwi) + sizeof(struct rt2870_rxd)))) {
		DPRINTF(("xfer too short %d\n", xferlen));
		goto skip;
	}

	/* HW can aggregate multiple 802.11 frames in a single USB xfer */
	buf = data->buf;
	while (xferlen > 8) {
		dmalen = le32toh(*(uint32_t *)buf) & 0xffff;

		if (__predict_false(dmalen == 0 || (dmalen & 3) != 0)) {
			DPRINTF(("bad DMA length %u (%x)\n", dmalen, dmalen));
			break;
		}
		if (__predict_false(dmalen + 8 > (uint32_t)xferlen)) {
			DPRINTF(("bad DMA length %u > %d\n",
			    dmalen + 8, xferlen));
			break;
		}
		run_rx_frame(sc, buf + sizeof (uint32_t), dmalen);
		buf += dmalen + 8;
		xferlen -= dmalen + 8;
	}

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->rxq.pipeh, data, data->buf, RUN_MAX_RXSZ,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT, run_rxeof);
	(void)usbd_transfer(data->xfer);
}

static void
run_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct run_tx_data *data = priv;
	struct run_softc *sc = data->sc;
	struct run_tx_ring *txq = &sc->txq[data->qid];
	struct ifnet *ifp = &sc->sc_if;
	int s;

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTF(("TX status=%d\n", status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(txq->pipeh);
		ifp->if_oerrors++;
		return;
	}

	s = splnet();
	sc->sc_tx_timer = 0;
	ifp->if_opackets++;
	if (--txq->queued < RUN_TX_RING_COUNT) {
		sc->qfullmsk &= ~(1 << data->qid);
		ifp->if_flags &= ~IFF_OACTIVE;
		run_start(ifp);
	}
	splx(s);
}

static int
run_tx(struct run_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct run_node *rn = (void *)ni;
	struct ieee80211_frame *wh;
#ifndef RUN_HWCRYPTO
	struct ieee80211_key *k;
#endif
	struct run_tx_ring *ring;
	struct run_tx_data *data;
	struct rt2870_txd *txd;
	struct rt2860_txwi *txwi;
	uint16_t dur;
	uint8_t type, mcs, tid, qid, qos = 0;
	int error, hasqos, ridx, ctl_ridx, xferlen;

	wh = mtod(m, struct ieee80211_frame *);

#ifndef RUN_HWCRYPTO
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m);
		if (k == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m, struct ieee80211_frame *);
	}
#endif
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ((struct ieee80211_qosframe *)wh)->i_qos[0];
		tid = qos & IEEE80211_QOS_TID;
		qid = TID_TO_WME_AC(tid);
	} else {
		tid = 0;
		qid = WME_AC_BE;
	}
	ring = &sc->txq[qid];
	data = &ring->data[ring->cur];

	/* pickup a rate index */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA) {
		ridx = (ic->ic_curmode == IEEE80211_MODE_11A) ?
		    RT2860_RIDX_OFDM6 : RT2860_RIDX_CCK1;
		ctl_ridx = rt2860_rates[ridx].ctl_ridx;
	} else if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
		ridx = sc->fixed_ridx;
		ctl_ridx = rt2860_rates[ridx].ctl_ridx;
	} else {
		ridx = rn->ridx[ni->ni_txrate];
		ctl_ridx = rn->ctl_ridx[ni->ni_txrate];
	}

	/* get MCS code from rate index */
	mcs = rt2860_rates[ridx].mcs;

	xferlen = sizeof (*txwi) + m->m_pkthdr.len;
	/* roundup to 32-bit alignment */
	xferlen = (xferlen + 3) & ~3;

	txd = (struct rt2870_txd *)data->buf;
	txd->flags = RT2860_TX_QSEL_EDCA;
	txd->len = htole16(xferlen);

	/* setup TX Wireless Information */
	txwi = (struct rt2860_txwi *)(txd + 1);
	txwi->flags = 0;
	txwi->xflags = hasqos ? 0 : RT2860_TX_NSEQ;
	txwi->wcid = (type == IEEE80211_FC0_TYPE_DATA) ?
	    RUN_AID2WCID(ni->ni_associd) : 0xff;
	txwi->len = htole16(m->m_pkthdr.len);
	if (rt2860_rates[ridx].phy == IEEE80211_T_DS) {
		txwi->phy = htole16(RT2860_PHY_CCK);
		if (ridx != RT2860_RIDX_CCK1 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			mcs |= RT2860_PHY_SHPRE;
	} else
		txwi->phy = htole16(RT2860_PHY_OFDM);
	txwi->phy |= htole16(mcs);

	txwi->txop = RT2860_TX_TXOP_BACKOFF;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (!hasqos || (qos & IEEE80211_QOS_ACKPOLICY) !=
	    IEEE80211_QOS_ACKPOLICY_NOACK)) {
		txwi->xflags |= RT2860_TX_ACK;
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			dur = rt2860_rates[ctl_ridx].sp_ack_dur;
		else
			dur = rt2860_rates[ctl_ridx].lp_ack_dur;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

#ifndef IEEE80211_STA_ONLY
	/* ask MAC to insert timestamp into probe responses */
	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
	    /* NOTE: beacons do not pass through tx_data() */
		txwi->flags |= RT2860_TX_TS;
#endif

	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct run_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rt2860_rates[ridx].rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wt_hwqueue = qid;
		if (mcs & RT2860_PHY_SHPRE)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m);
	}

	m_copydata(m, 0, m->m_pkthdr.len, (void *)(txwi + 1));
	m_freem(m);

	xferlen += sizeof (*txd) + 4;

	usbd_setup_xfer(data->xfer, ring->pipeh, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RUN_TX_TIMEOUT, run_txeof);
	error = usbd_transfer(data->xfer);
	if (__predict_false(error != USBD_IN_PROGRESS &&
	    error != USBD_NORMAL_COMPLETION))
		return (error);

	ieee80211_free_node(ni);

	ring->cur = (ring->cur + 1) % RUN_TX_RING_COUNT;
	if (++ring->queued >= RUN_TX_RING_COUNT)
		sc->qfullmsk |= 1 << qid;

	return (0);
}

static void
run_start(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		if (sc->qfullmsk != 0) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/* send pending management frames first */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m != NULL) {
			ni = (void *)m->m_pkthdr.rcvif;
			m->m_pkthdr.rcvif = NULL;
			goto sendit;
		}
		if (ic->ic_state != IEEE80211_S_RUN)
			break;

		/* encapsulate and send data frames */
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

		if (run_tx(sc, m, ni) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

static void
run_watchdog(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			/* run_init(ifp); XXX needs a process context! */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ic);
}

static int
run_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_UP|IFF_RUNNING:
			break;
		case IFF_UP:
			run_init(ifp);
			break;
		case IFF_RUNNING:
			run_stop(ifp, 1);
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

	default:
		error = ieee80211_ioctl(ic, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			run_init(ifp);
		}
		error = 0;
	}

	splx(s);

	return (error);
}

static void
run_select_chan_group(struct run_softc *sc, int group)
{
	uint32_t tmp;
	uint8_t agc;

	run_bbp_write(sc, 62, 0x37 - sc->lna[group]);
	run_bbp_write(sc, 63, 0x37 - sc->lna[group]);
	run_bbp_write(sc, 64, 0x37 - sc->lna[group]);
	run_bbp_write(sc, 86, 0x00);

	if (group == 0) {
		if (sc->ext_2ghz_lna) {
			run_bbp_write(sc, 82, 0x62);
			run_bbp_write(sc, 75, 0x46);
		} else {
			run_bbp_write(sc, 82, 0x84);
			run_bbp_write(sc, 75, 0x50);
		}
	} else {
		if (sc->mac_ver == 0x3572)
			run_bbp_write(sc, 82, 0x94);
		else
			run_bbp_write(sc, 82, 0xf2);
		if (sc->ext_5ghz_lna)
			run_bbp_write(sc, 75, 0x46);
		else
			run_bbp_write(sc, 75, 0x50);
	}

	run_read(sc, RT2860_TX_BAND_CFG, &tmp);
	tmp &= ~(RT2860_5G_BAND_SEL_N | RT2860_5G_BAND_SEL_P);
	tmp |= (group == 0) ? RT2860_5G_BAND_SEL_N : RT2860_5G_BAND_SEL_P;
	run_write(sc, RT2860_TX_BAND_CFG, tmp);

	/* enable appropriate Power Amplifiers and Low Noise Amplifiers */
	tmp = RT2860_RFTR_EN | RT2860_TRSW_EN | RT2860_LNA_PE0_EN;
	if (sc->nrxchains > 1)
		tmp |= RT2860_LNA_PE1_EN;
	if (group == 0) {	/* 2GHz */
		tmp |= RT2860_PA_PE_G0_EN;
		if (sc->ntxchains > 1)
			tmp |= RT2860_PA_PE_G1_EN;
	} else {		/* 5GHz */
		tmp |= RT2860_PA_PE_A0_EN;
		if (sc->ntxchains > 1)
			tmp |= RT2860_PA_PE_A1_EN;
	}
	if (sc->mac_ver == 0x3572) {
		run_rt3070_rf_write(sc, 8, 0x00);
		run_write(sc, RT2860_TX_PIN_CFG, tmp);
		run_rt3070_rf_write(sc, 8, 0x80);
	} else
		run_write(sc, RT2860_TX_PIN_CFG, tmp);

	/* set initial AGC value */
	if (group == 0) {       /* 2GHz band */
		if (sc->mac_ver >= 0x3070)
			agc = 0x1c + sc->lna[0] * 2;
		else
			agc = 0x2e + sc->lna[0];
	} else {		/* 5GHz band */
		if (sc->mac_ver == 0x3572)
			agc = 0x22 + (sc->lna[group] * 5) / 3;
		else
			agc = 0x32 + (sc->lna[group] * 5) / 3;
	}
	run_set_agc(sc, agc);
}

static void
run_rt2870_set_chan(struct run_softc *sc, u_int chan)
{
	const struct rfprog *rfprog = rt2860_rf2850;
	uint32_t r2, r3, r4;
	int8_t txpow1, txpow2;
	int i;

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rfprog[i].chan != chan; i++);

	r2 = rfprog[i].r2;
	if (sc->ntxchains == 1)
		r2 |= 1 << 12;		/* 1T: disable Tx chain 2 */
	if (sc->nrxchains == 1)
		r2 |= 1 << 15 | 1 << 4;	/* 1R: disable Rx chains 2 & 3 */
	else if (sc->nrxchains == 2)
		r2 |= 1 << 4;		/* 2R: disable Rx chain 3 */

	/* use Tx power values from EEPROM */
	txpow1 = sc->txpow1[i];
	txpow2 = sc->txpow2[i];
	if (chan > 14) {
		if (txpow1 >= 0)
			txpow1 = txpow1 << 1 | 1;
		else
			txpow1 = (7 + txpow1) << 1;
		if (txpow2 >= 0)
			txpow2 = txpow2 << 1 | 1;
		else
			txpow2 = (7 + txpow2) << 1;
	}
	r3 = rfprog[i].r3 | txpow1 << 7;
	r4 = rfprog[i].r4 | sc->freq << 13 | txpow2 << 4;

	run_rt2870_rf_write(sc, RT2860_RF1, rfprog[i].r1);
	run_rt2870_rf_write(sc, RT2860_RF2, r2);
	run_rt2870_rf_write(sc, RT2860_RF3, r3);
	run_rt2870_rf_write(sc, RT2860_RF4, r4);

	DELAY(200);

	run_rt2870_rf_write(sc, RT2860_RF1, rfprog[i].r1);
	run_rt2870_rf_write(sc, RT2860_RF2, r2);
	run_rt2870_rf_write(sc, RT2860_RF3, r3 | 1);
	run_rt2870_rf_write(sc, RT2860_RF4, r4);

	DELAY(200);

	run_rt2870_rf_write(sc, RT2860_RF1, rfprog[i].r1);
	run_rt2870_rf_write(sc, RT2860_RF2, r2);
	run_rt2870_rf_write(sc, RT2860_RF3, r3);
	run_rt2870_rf_write(sc, RT2860_RF4, r4);
}

static void
run_rt3070_set_chan(struct run_softc *sc, u_int chan)
{
	int8_t txpow1, txpow2;
	uint8_t rf;
	int i;

	KASSERT(chan >= 1 && chan <= 14);	/* RT3070 is 2GHz only */

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rt2860_rf2850[i].chan != chan; i++)
		continue;

	/* use Tx power values from EEPROM */
	txpow1 = sc->txpow1[i];
	txpow2 = sc->txpow2[i];

	run_rt3070_rf_write(sc, 2, rt3070_freqs[i].n);
	run_rt3070_rf_write(sc, 3, rt3070_freqs[i].k);
	run_rt3070_rf_read(sc, 6, &rf);
	rf = (rf & ~0x03) | rt3070_freqs[i].r;
	run_rt3070_rf_write(sc, 6, rf);

	/* set Tx0 power */
	run_rt3070_rf_read(sc, 12, &rf);
	rf = (rf & ~0x1f) | txpow1;
	run_rt3070_rf_write(sc, 12, rf);

	/* set Tx1 power */
	run_rt3070_rf_read(sc, 13, &rf);
	rf = (rf & ~0x1f) | txpow2;
	run_rt3070_rf_write(sc, 13, rf);

	run_rt3070_rf_read(sc, 1, &rf);
	rf &= ~0xfc;
	if (sc->ntxchains == 1)
		rf |= 1 << 7 | 1 << 5;	/* 1T: disable Tx chains 2 & 3 */
	else if (sc->ntxchains == 2)
		rf |= 1 << 7;		/* 2T: disable Tx chain 3 */
	if (sc->nrxchains == 1)
		rf |= 1 << 6 | 1 << 4;	/* 1R: disable Rx chains 2 & 3 */
	else if (sc->nrxchains == 2)
		rf |= 1 << 6;		/* 2R: disable Rx chain 3 */
	run_rt3070_rf_write(sc, 1, rf);

	/* set RF offset */
	run_rt3070_rf_read(sc, 23, &rf);
	rf = (rf & ~0x7f) | sc->freq;
	run_rt3070_rf_write(sc, 23, rf);

	/* program RF filter */
	run_rt3070_rf_read(sc, 24, &rf);        /* Tx */
	rf = (rf & ~0x3f) | sc->rf24_20mhz;
	run_rt3070_rf_write(sc, 24, rf);
	run_rt3070_rf_read(sc, 31, &rf);        /* Rx */
	rf = (rf & ~0x3f) | sc->rf24_20mhz;
	run_rt3070_rf_write(sc, 31, rf);

	/* enable RF tuning */
	run_rt3070_rf_read(sc, 7, &rf);
	run_rt3070_rf_write(sc, 7, rf | 0x01);
}

static void
run_rt3572_set_chan(struct run_softc *sc, u_int chan)
{
	int8_t txpow1, txpow2;
	uint32_t tmp;
	uint8_t rf;
	int i;

	/* find the settings for this channel (we know it exists) */
	for (i = 0; rt2860_rf2850[i].chan != chan; i++);

	/* use Tx power values from EEPROM */
	txpow1 = sc->txpow1[i];
	txpow2 = sc->txpow2[i];

	if (chan <= 14) {
		run_bbp_write(sc, 25, sc->bbp25);
		run_bbp_write(sc, 26, sc->bbp26);
	} else {
		/* enable IQ phase correction */
		run_bbp_write(sc, 25, 0x09);
		run_bbp_write(sc, 26, 0xff);
	}

	run_rt3070_rf_write(sc, 2, rt3070_freqs[i].n);
	run_rt3070_rf_write(sc, 3, rt3070_freqs[i].k);
	run_rt3070_rf_read(sc, 6, &rf);
	rf  = (rf & ~0x0f) | rt3070_freqs[i].r;
	rf |= (chan <= 14) ? 0x08 : 0x04;
	run_rt3070_rf_write(sc, 6, rf);

	/* set PLL mode */
	run_rt3070_rf_read(sc, 5, &rf);
	rf &= ~(0x08 | 0x04);
	rf |= (chan <= 14) ? 0x04 : 0x08;
	run_rt3070_rf_write(sc, 5, rf);

	/* set Tx power for chain 0 */
	if (chan <= 14)
		rf = 0x60 | txpow1;
	else
		rf = 0xe0 | (txpow1 & 0xc) << 1 | (txpow1 & 0x3);
	run_rt3070_rf_write(sc, 12, rf);

	/* set Tx power for chain 1 */
	if (chan <= 14)
		rf = 0x60 | txpow2;
	else
		rf = 0xe0 | (txpow2 & 0xc) << 1 | (txpow2 & 0x3);
	run_rt3070_rf_write(sc, 13, rf);

	/* set Tx/Rx streams */
	run_rt3070_rf_read(sc, 1, &rf);
	rf &= ~0xfc;
	if (sc->ntxchains == 1)
		rf |= 1 << 7 | 1 << 5;	/* 1T: disable Tx chains 2 & 3 */
	else if (sc->ntxchains == 2)
		rf |= 1 << 7;		/* 2T: disable Tx chain 3 */
	if (sc->nrxchains == 1)
		rf |= 1 << 6 | 1 << 4;	/* 1R: disable Rx chains 2 & 3 */
	else if (sc->nrxchains == 2)
		rf |= 1 << 6;		/* 2R: disable Rx chain 3 */
	run_rt3070_rf_write(sc, 1, rf);

	/* set RF offset */
	run_rt3070_rf_read(sc, 23, &rf);
	rf = (rf & ~0x7f) | sc->freq;
	run_rt3070_rf_write(sc, 23, rf);

	/* program RF filter */
	rf = sc->rf24_20mhz;
	run_rt3070_rf_write(sc, 24, rf);	/* Tx */
	run_rt3070_rf_write(sc, 31, rf);	/* Rx */

	/* enable RF tuning */
	run_rt3070_rf_read(sc, 7, &rf);
	rf = (chan <= 14) ? 0xd8 : ((rf & ~0xc8) | 0x14);
	run_rt3070_rf_write(sc, 7, rf);

	/* TSSI */
	rf = (chan <= 14) ? 0xc3 : 0xc0;
	run_rt3070_rf_write(sc, 9, rf);

	/* set loop filter 1 */
	run_rt3070_rf_write(sc, 10, 0xf1);
	/* set loop filter 2 */
	run_rt3070_rf_write(sc, 11, (chan <= 14) ? 0xb9 : 0x00);

	/* set tx_mx2_ic */
	run_rt3070_rf_write(sc, 15, (chan <= 14) ? 0x53 : 0x43);
	/* set tx_mx1_ic */
	if (chan <= 14)
		rf = 0x48 | sc->txmixgain_2ghz;
	else
		rf = 0x78 | sc->txmixgain_5ghz;
	run_rt3070_rf_write(sc, 16, rf);

	/* set tx_lo1 */
	run_rt3070_rf_write(sc, 17, 0x23);
	/* set tx_lo2 */
	if (chan <= 14)
		rf = 0x93;
	else if (chan <= 64)
		rf = 0xb7;
	else if (chan <= 128)
		rf = 0x74;
	else
		rf = 0x72;
	run_rt3070_rf_write(sc, 19, rf);

	/* set rx_lo1 */
	if (chan <= 14)
		rf = 0xb3;
	else if (chan <= 64)
		rf = 0xf6;
	else if (chan <= 128)
		rf = 0xf4;
	else
		rf = 0xf3;
	run_rt3070_rf_write(sc, 20, rf);

	/* set pfd_delay */
	if (chan <= 14)
		rf = 0x15;
	else if (chan <= 64)
		rf = 0x3d;
	else
		rf = 0x01;
	run_rt3070_rf_write(sc, 25, rf);

	/* set rx_lo2 */
	run_rt3070_rf_write(sc, 26, (chan <= 14) ? 0x85 : 0x87);
	/* set ldo_rf_vc */
	run_rt3070_rf_write(sc, 27, (chan <= 14) ? 0x00 : 0x01);
	/* set drv_cc */
	run_rt3070_rf_write(sc, 29, (chan <= 14) ? 0x9b : 0x9f);

	run_read(sc, RT2860_GPIO_CTRL, &tmp);
	tmp &= ~0x8080;
	if (chan <= 14)
		tmp |= 0x80;
	run_write(sc, RT2860_GPIO_CTRL, tmp);

	/* enable RF tuning */
	run_rt3070_rf_read(sc, 7, &rf);
	run_rt3070_rf_write(sc, 7, rf | 0x01);

	DELAY(2000);
}

static void
run_set_agc(struct run_softc *sc, uint8_t agc)
{
	uint8_t bbp;

	if (sc->mac_ver == 0x3572) {
		run_bbp_read(sc, 27, &bbp);
		bbp &= ~(0x3 << 5);
		run_bbp_write(sc, 27, bbp | 0 << 5);	/* select Rx0 */
		run_bbp_write(sc, 66, agc);
		run_bbp_write(sc, 27, bbp | 1 << 5);	/* select Rx1 */
		run_bbp_write(sc, 66, agc);
	} else
		run_bbp_write(sc, 66, agc);
}

static void
run_set_rx_antenna(struct run_softc *sc, int aux)
{
	uint32_t tmp;

	run_mcu_cmd(sc, RT2860_MCU_CMD_ANTSEL, !aux);
	run_read(sc, RT2860_GPIO_CTRL, &tmp);
	tmp &= ~0x0808;
	if (aux)
		tmp |= 0x08;
	run_write(sc, RT2860_GPIO_CTRL, tmp);
}

static int
run_set_chan(struct run_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int chan, group;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return (EINVAL);

	if (sc->mac_ver == 0x3572)
		run_rt3572_set_chan(sc, chan);
	else if (sc->mac_ver >= 0x3070)
		run_rt3070_set_chan(sc, chan);
	else
		run_rt2870_set_chan(sc, chan);

	/* determine channel group */
	if (chan <= 14)
		group = 0;
	else if (chan <= 64)
		group = 1;
	else if (chan <= 128)
		group = 2;
	else
		group = 3;

	/* XXX necessary only when group has changed! */
	run_select_chan_group(sc, group);

	DELAY(1000);
	return (0);
}

static void
run_enable_tsf_sync(struct run_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	run_read(sc, RT2860_BCN_TIME_CFG, &tmp);
	tmp &= ~0x1fffff;
	tmp |= ic->ic_bss->ni_intval * 16;
	tmp |= RT2860_TSF_TIMER_EN | RT2860_TBTT_TIMER_EN;
	if (ic->ic_opmode == IEEE80211_M_STA) {
		/*
		 * Local TSF is always updated with remote TSF on beacon
		 * reception.
		 */
		tmp |= 1 << RT2860_TSF_SYNC_MODE_SHIFT;
	}
#ifndef IEEE80211_STA_ONLY
	else if (ic->ic_opmode == IEEE80211_M_IBSS) {
		tmp |= RT2860_BCN_TX_EN;
		/*
		 * Local TSF is updated with remote TSF on beacon reception
		 * only if the remote TSF is greater than local TSF.
		 */
		tmp |= 2 << RT2860_TSF_SYNC_MODE_SHIFT;
	} else if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		tmp |= RT2860_BCN_TX_EN;
		/* SYNC with nobody */
		tmp |= 3 << RT2860_TSF_SYNC_MODE_SHIFT;
	}
#endif
	run_write(sc, RT2860_BCN_TIME_CFG, tmp);
}

static void
run_enable_mrr(struct run_softc *sc)
{
#define CCK(mcs)	(mcs)
#define OFDM(mcs)	(1 << 3 | (mcs))
	run_write(sc, RT2860_LG_FBK_CFG0,
	    OFDM(6) << 28 |	/* 54->48 */
	    OFDM(5) << 24 |	/* 48->36 */
	    OFDM(4) << 20 |	/* 36->24 */
	    OFDM(3) << 16 |	/* 24->18 */
	    OFDM(2) << 12 |	/* 18->12 */
	    OFDM(1) <<  8 |	/* 12-> 9 */
	    OFDM(0) <<  4 |	/*  9-> 6 */
	    OFDM(0));		/*  6-> 6 */

	run_write(sc, RT2860_LG_FBK_CFG1,
	    CCK(2) << 12 |	/* 11->5.5 */
	    CCK(1) <<  8 |	/* 5.5-> 2 */
	    CCK(0) <<  4 |	/*   2-> 1 */
	    CCK(0));		/*   1-> 1 */
#undef OFDM
#undef CCK
}

static void
run_set_txpreamble(struct run_softc *sc)
{
	uint32_t tmp;

	run_read(sc, RT2860_AUTO_RSP_CFG, &tmp);
	if (sc->sc_ic.ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= RT2860_CCK_SHORT_EN;
	else
		tmp &= ~RT2860_CCK_SHORT_EN;
	run_write(sc, RT2860_AUTO_RSP_CFG, tmp);
}

static void
run_set_basicrates(struct run_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* set basic rates mask */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		run_write(sc, RT2860_LEGACY_BASIC_RATE, 0x003);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		run_write(sc, RT2860_LEGACY_BASIC_RATE, 0x150);
	else	/* 11g */
		run_write(sc, RT2860_LEGACY_BASIC_RATE, 0x15f);
}

static void
run_set_leds(struct run_softc *sc, uint16_t which)
{

	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LEDS,
	    which | (sc->leds & 0x7f));
}

static void
run_set_bssid(struct run_softc *sc, const uint8_t *bssid)
{

	run_write(sc, RT2860_MAC_BSSID_DW0,
	    bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24);
	run_write(sc, RT2860_MAC_BSSID_DW1,
	    bssid[4] | bssid[5] << 8);
}

static void
run_set_macaddr(struct run_softc *sc, const uint8_t *addr)
{

	run_write(sc, RT2860_MAC_ADDR_DW0,
	    addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24);
	run_write(sc, RT2860_MAC_ADDR_DW1,
	    addr[4] | addr[5] << 8 | 0xff << 16);
}

static void
run_updateslot(struct ifnet *ifp)
{

	/* do it in a process context */
	run_do_async(ifp->if_softc, run_updateslot_cb, NULL, 0);
}

/* ARGSUSED */
static void
run_updateslot_cb(struct run_softc *sc, void *arg)
{
	uint32_t tmp;

	run_read(sc, RT2860_BKOFF_SLOT_CFG, &tmp);
	tmp &= ~0xff;
	tmp |= (sc->sc_ic.ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	run_write(sc, RT2860_BKOFF_SLOT_CFG, tmp);
}

static int8_t
run_rssi2dbm(struct run_softc *sc, uint8_t rssi, uint8_t rxchain)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c = ic->ic_curchan;
	int delta;

	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		u_int chan = ieee80211_chan2ieee(ic, c);
		delta = sc->rssi_5ghz[rxchain];

		/* determine channel group */
		if (chan <= 64)
			delta -= sc->lna[1];
		else if (chan <= 128)
			delta -= sc->lna[2];
		else
			delta -= sc->lna[3];
	} else
		delta = sc->rssi_2ghz[rxchain] - sc->lna[0];

	return (-12 - delta - rssi);
}

static int
run_bbp_init(struct run_softc *sc)
{
	int i, error, ntries;
	uint8_t bbp0;

	/* wait for BBP to wake up */
	for (ntries = 0; ntries < 20; ntries++) {
		if ((error = run_bbp_read(sc, 0, &bbp0)) != 0)
			return (error);
		if (bbp0 != 0 && bbp0 != 0xff)
			break;
	}
	if (ntries == 20)
		return (ETIMEDOUT);

	/* initialize BBP registers to default values */
	for (i = 0; i < (int)__arraycount(rt2860_def_bbp); i++) {
		run_bbp_write(sc, rt2860_def_bbp[i].reg,
		    rt2860_def_bbp[i].val);
	}

	/* fix BBP84 for RT2860E */
	if (sc->mac_ver == 0x2860 && sc->mac_rev != 0x0101)
		run_bbp_write(sc, 84, 0x19);

	if (sc->mac_ver >= 0x3070) {
		run_bbp_write(sc, 79, 0x13);
		run_bbp_write(sc, 80, 0x05);
		run_bbp_write(sc, 81, 0x33);
	} else if (sc->mac_ver == 0x2860 && sc->mac_rev == 0x0100) {
		run_bbp_write(sc, 69, 0x16);
		run_bbp_write(sc, 73, 0x12);
	}
	return (0);
}

static int
run_rt3070_rf_init(struct run_softc *sc)
{
	uint32_t tmp;
	uint8_t rf, target, bbp4;
	int i;

	run_rt3070_rf_read(sc, 30, &rf);
	/* toggle RF R30 bit 7 */
	run_rt3070_rf_write(sc, 30, rf | 0x80);
	DELAY(1000);
	run_rt3070_rf_write(sc, 30, rf & ~0x80);

	/* initialize RF registers to default value */
	if (sc->mac_ver == 0x3572) {
		for (i = 0; i < (int)__arraycount(rt3572_def_rf); i++) {
			run_rt3070_rf_write(sc, rt3572_def_rf[i].reg,
			    rt3572_def_rf[i].val);
		}
	} else {
		for (i = 0; i < (int)__arraycount(rt3070_def_rf); i++) {
			run_rt3070_rf_write(sc, rt3070_def_rf[i].reg,
			    rt3070_def_rf[i].val);
		}
	}
	if (sc->mac_ver == 0x3572) {
		run_rt3070_rf_read(sc, 6, &rf);
		run_rt3070_rf_write(sc, 6, rf | 0x40);

		/* increase voltage from 1.2V to 1.35V */
		run_read(sc, RT3070_LDO_CFG0, &tmp);
		tmp = (tmp & ~0x1f000000) | 0x0d000000;
		run_write(sc, RT3070_LDO_CFG0, tmp);
		if (sc->mac_rev >= 0x0211 || !sc->patch_dac) {
			/* decrease voltage back to 1.2V */
			DELAY(1000);
			tmp = (tmp & ~0x1f000000) | 0x01000000;
			run_write(sc, RT3070_LDO_CFG0, tmp);
		}
	} else if (sc->mac_ver == 0x3071) {
		run_rt3070_rf_read(sc, 6, &rf);
		run_rt3070_rf_write(sc, 6, rf | 0x40);
		run_rt3070_rf_write(sc, 31, 0x14);

		run_read(sc, RT3070_LDO_CFG0, &tmp);
		tmp &= ~0x1f000000;
		if (sc->mac_rev < 0x0211)
			tmp |= 0x0d000000;	/* 1.35V */
		else
			tmp |= 0x01000000;	/* 1.2V */
		run_write(sc, RT3070_LDO_CFG0, tmp);

		/* patch LNA_PE_G1 */
		run_read(sc, RT3070_GPIO_SWITCH, &tmp);
		run_write(sc, RT3070_GPIO_SWITCH, tmp & ~0x20);
	} else if (sc->mac_ver == 0x3070) {
		/* increase voltage from 1.2V to 1.35V */
		run_read(sc, RT3070_LDO_CFG0, &tmp);
		tmp = (tmp & ~0x0f000000) | 0x0d000000;
		run_write(sc, RT3070_LDO_CFG0, tmp);
	}

	/* select 20MHz bandwidth */
	run_rt3070_rf_read(sc, 31, &rf);
	run_rt3070_rf_write(sc, 31, rf & ~0x20);

	/* calibrate filter for 20MHz bandwidth */
	sc->rf24_20mhz = 0x1f;	/* default value */
	target = (sc->mac_ver < 0x3071) ? 0x16 : 0x13;
	run_rt3070_filter_calib(sc, 0x07, target, &sc->rf24_20mhz);

	/* select 40MHz bandwidth */
	run_bbp_read(sc, 4, &bbp4);
	run_bbp_write(sc, 4, (bbp4 & ~0x08) | 0x10);
	run_rt3070_rf_read(sc, 31, &rf);
	run_rt3070_rf_write(sc, 31, rf | 0x20);

	/* calibrate filter for 40MHz bandwidth */
	sc->rf24_40mhz = 0x2f;	/* default value */
	target = (sc->mac_ver < 0x3071) ? 0x19 : 0x15;
	run_rt3070_filter_calib(sc, 0x27, target, &sc->rf24_40mhz);

	/* go back to 20MHz bandwidth */
	run_bbp_read(sc, 4, &bbp4);
	run_bbp_write(sc, 4, bbp4 & ~0x18);

	if (sc->mac_ver == 0x3572) {
		/* save default BBP registers 25 and 26 values */
		run_bbp_read(sc, 25, &sc->bbp25);
		run_bbp_read(sc, 26, &sc->bbp26);
	} else if (sc->mac_rev < 0x0211)
		run_rt3070_rf_write(sc, 27, 0x03);

	run_read(sc, RT3070_OPT_14, &tmp);
	run_write(sc, RT3070_OPT_14, tmp | 1);

	if (sc->mac_ver == 0x3070 || sc->mac_ver == 0x3071) {
		run_rt3070_rf_read(sc, 17, &rf);
		rf &= ~RT3070_TX_LO1;
		if ((sc->mac_ver == 0x3070 ||
		     (sc->mac_ver == 0x3071 && sc->mac_rev >= 0x0211)) &&
		    !sc->ext_2ghz_lna)
			rf |= 0x20;	/* fix for long range Rx issue */
		if (sc->txmixgain_2ghz >= 1)
			rf = (rf & ~0x7) | sc->txmixgain_2ghz;
		run_rt3070_rf_write(sc, 17, rf);
	}
	if (sc->mac_ver == 0x3071) {
		run_rt3070_rf_read(sc, 1, &rf);
		rf &= ~(RT3070_RX0_PD | RT3070_TX0_PD);
		rf |= RT3070_RF_BLOCK | RT3070_RX1_PD | RT3070_TX1_PD;
		run_rt3070_rf_write(sc, 1, rf);

		run_rt3070_rf_read(sc, 15, &rf);
		run_rt3070_rf_write(sc, 15, rf & ~RT3070_TX_LO2);

		run_rt3070_rf_read(sc, 20, &rf);
		run_rt3070_rf_write(sc, 20, rf & ~RT3070_RX_LO1);

		run_rt3070_rf_read(sc, 21, &rf);
		run_rt3070_rf_write(sc, 21, rf & ~RT3070_RX_LO2);
	}
	if (sc->mac_ver == 0x3070 || sc->mac_ver == 0x3071) {
		/* fix Tx to Rx IQ glitch by raising RF voltage */
		run_rt3070_rf_read(sc, 27, &rf);
		rf &= ~0x77;
		if (sc->mac_rev < 0x0211)
			rf |= 0x03;
		run_rt3070_rf_write(sc, 27, rf);
	}
	return (0);
}

static int
run_rt3070_filter_calib(struct run_softc *sc, uint8_t init, uint8_t target,
    uint8_t *val)
{
	uint8_t rf22, rf24;
	uint8_t bbp55_pb, bbp55_sb, delta;
	int ntries;

	/* program filter */
	run_rt3070_rf_read(sc, 24, &rf24);
	rf24 = (rf24 & 0xc0) | init;    /* initial filter value */
	run_rt3070_rf_write(sc, 24, rf24);

	/* enable baseband loopback mode */
	run_rt3070_rf_read(sc, 22, &rf22);
	run_rt3070_rf_write(sc, 22, rf22 | 0x01);

	/* set power and frequency of passband test tone */
	run_bbp_write(sc, 24, 0x00);
	for (ntries = 0; ntries < 100; ntries++) {
		/* transmit test tone */
		run_bbp_write(sc, 25, 0x90);
		DELAY(1000);
		/* read received power */
		run_bbp_read(sc, 55, &bbp55_pb);
		if (bbp55_pb != 0)
			break;
	}
	if (ntries == 100)
		return (ETIMEDOUT);

	/* set power and frequency of stopband test tone */
	run_bbp_write(sc, 24, 0x06);
	for (ntries = 0; ntries < 100; ntries++) {
		/* transmit test tone */
		run_bbp_write(sc, 25, 0x90);
		DELAY(1000);
		/* read received power */
		run_bbp_read(sc, 55, &bbp55_sb);

		delta = bbp55_pb - bbp55_sb;
		if (delta > target)
			break;

		/* reprogram filter */
		rf24++;
		run_rt3070_rf_write(sc, 24, rf24);
	}
	if (ntries < 100) {
		if (rf24 != init)
			rf24--;	/* backtrack */
		*val = rf24;
		run_rt3070_rf_write(sc, 24, rf24);
	}

	/* restore initial state */
	run_bbp_write(sc, 24, 0x00);

	/* disable baseband loopback mode */
	run_rt3070_rf_read(sc, 22, &rf22);
	run_rt3070_rf_write(sc, 22, rf22 & ~0x01);

	return (0);
}

static void
run_rt3070_rf_setup(struct run_softc *sc)
{
	uint8_t bbp, rf;
	int i;

	if (sc->mac_ver == 0x3572) {
		/* enable DC filter */
		if (sc->mac_rev >= 0x0201)
			run_bbp_write(sc, 103, 0xc0);

		run_bbp_read(sc, 138, &bbp);
		if (sc->ntxchains == 1)
			bbp |= 0x20;	/* turn off DAC1 */
		if (sc->nrxchains == 1)
			bbp &= ~0x02;	/* turn off ADC1 */
		run_bbp_write(sc, 138, bbp);

		if (sc->mac_rev >= 0x0211) {
			/* improve power consumption */
			run_bbp_read(sc, 31, &bbp);
			run_bbp_write(sc, 31, bbp & ~0x03);
		}

		run_rt3070_rf_read(sc, 16, &rf);
		rf = (rf & ~0x07) | sc->txmixgain_2ghz;
		run_rt3070_rf_write(sc, 16, rf);
	} else if (sc->mac_ver == 0x3071) {
		/* enable DC filter */
		if (sc->mac_rev >= 0x0201)
			run_bbp_write(sc, 103, 0xc0);

		run_bbp_read(sc, 138, &bbp);
		if (sc->ntxchains == 1)
			bbp |= 0x20;	/* turn off DAC1 */
		if (sc->nrxchains == 1)
			bbp &= ~0x02;	/* turn off ADC1 */
		run_bbp_write(sc, 138, bbp);

		if (sc->mac_rev >= 0x0211) {
			/* improve power consumption */
			run_bbp_read(sc, 31, &bbp);
			run_bbp_write(sc, 31, bbp & ~0x03);
		}

		run_write(sc, RT2860_TX_SW_CFG1, 0);
		if (sc->mac_rev < 0x0211) {
			run_write(sc, RT2860_TX_SW_CFG2,
			    sc->patch_dac ? 0x2c : 0x0f);
		} else
			run_write(sc, RT2860_TX_SW_CFG2, 0);
	} else if (sc->mac_ver == 0x3070) {
		if (sc->mac_rev >= 0x0201) {
			/* enable DC filter */
			run_bbp_write(sc, 103, 0xc0);

			/* improve power consumption */
			run_bbp_read(sc, 31, &bbp);
			run_bbp_write(sc, 31, bbp & ~0x03);
		}

		if (sc->mac_rev < 0x0211) {
			run_write(sc, RT2860_TX_SW_CFG1, 0);
			run_write(sc, RT2860_TX_SW_CFG2, 0x2c);
		} else
			run_write(sc, RT2860_TX_SW_CFG2, 0);
	}

	/* initialize RF registers from ROM for >=RT3071*/
	if (sc->mac_ver >= 0x3071) {
		for (i = 0; i < 10; i++) {
			if (sc->rf[i].reg == 0 || sc->rf[i].reg == 0xff)
				continue;
			run_rt3070_rf_write(sc, sc->rf[i].reg, sc->rf[i].val);
		}
	}
}

static int
run_txrx_enable(struct run_softc *sc)
{
	uint32_t tmp;
	int error, ntries;

	run_write(sc, RT2860_MAC_SYS_CTRL, RT2860_MAC_TX_EN);
	for (ntries = 0; ntries < 200; ntries++) {
		if ((error = run_read(sc, RT2860_WPDMA_GLO_CFG, &tmp)) != 0)
			return (error);
		if ((tmp & (RT2860_TX_DMA_BUSY | RT2860_RX_DMA_BUSY)) == 0)
			break;
		DELAY(1000);
	}
	if (ntries == 200)
		return (ETIMEDOUT);

	DELAY(50);

	tmp |= RT2860_RX_DMA_EN | RT2860_TX_DMA_EN | RT2860_TX_WB_DDONE;
	run_write(sc, RT2860_WPDMA_GLO_CFG, tmp);

	/* enable Rx bulk aggregation (set timeout and limit) */
	tmp = RT2860_USB_TX_EN | RT2860_USB_RX_EN | RT2860_USB_RX_AGG_EN |
	    RT2860_USB_RX_AGG_TO(128) | RT2860_USB_RX_AGG_LMT(2);
	run_write(sc, RT2860_USB_DMA_CFG, tmp);

	/* set Rx filter */
	tmp = RT2860_DROP_CRC_ERR | RT2860_DROP_PHY_ERR;
	if (sc->sc_ic.ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RT2860_DROP_UC_NOME | RT2860_DROP_DUPL |
		    RT2860_DROP_CTS | RT2860_DROP_BA | RT2860_DROP_ACK |
		    RT2860_DROP_VER_ERR | RT2860_DROP_CTRL_RSV |
		    RT2860_DROP_CFACK | RT2860_DROP_CFEND;
		if (sc->sc_ic.ic_opmode == IEEE80211_M_STA)
			tmp |= RT2860_DROP_RTS | RT2860_DROP_PSPOLL;
	}
	run_write(sc, RT2860_RX_FILTR_CFG, tmp);

	run_write(sc, RT2860_MAC_SYS_CTRL,
	    RT2860_MAC_RX_EN | RT2860_MAC_TX_EN);

	return (0);
}

static int
run_init(struct ifnet *ifp)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	uint8_t bbp1, bbp3;
	int i, error, qid, ridx, ntries;

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT2860_ASIC_VER_ID, &tmp)) != 0)
			goto fail;
		if (tmp != 0 && tmp != 0xffffffff)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		error = ETIMEDOUT;
		goto fail;
	}

	if ((sc->sc_flags & RUN_FWLOADED) == 0 &&
	    (error = run_load_microcode(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load 8051 microcode\n");
		goto fail;
	}

	if (ifp->if_flags & IFF_RUNNING)
		run_stop(ifp, 0);

	/* init host command ring */
	sc->cmdq.cur = sc->cmdq.next = sc->cmdq.queued = 0;

	/* init Tx rings (4 EDCAs) */
	for (qid = 0; qid < 4; qid++) {
		if ((error = run_alloc_tx_ring(sc, qid)) != 0)
			goto fail;
	}
	/* init Rx ring */
	if ((error = run_alloc_rx_ring(sc)) != 0)
		goto fail;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	run_set_macaddr(sc, ic->ic_myaddr);

	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT2860_WPDMA_GLO_CFG, &tmp)) != 0)
			goto fail;
		if ((tmp & (RT2860_TX_DMA_BUSY | RT2860_RX_DMA_BUSY)) == 0)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for DMA engine\n");
		error = ETIMEDOUT;
		goto fail;
	}
	tmp &= 0xff0;
	tmp |= RT2860_TX_WB_DDONE;
	run_write(sc, RT2860_WPDMA_GLO_CFG, tmp);

	/* turn off PME_OEN to solve high-current issue */
	run_read(sc, RT2860_SYS_CTRL, &tmp);
	run_write(sc, RT2860_SYS_CTRL, tmp & ~RT2860_PME_OEN);

	run_write(sc, RT2860_MAC_SYS_CTRL,
	    RT2860_BBP_HRST | RT2860_MAC_SRST);
	run_write(sc, RT2860_USB_DMA_CFG, 0);

	if ((error = run_reset(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not reset chipset\n");
		goto fail;
	}

	run_write(sc, RT2860_MAC_SYS_CTRL, 0);

	/* init Tx power for all Tx rates (from EEPROM) */
	for (ridx = 0; ridx < 5; ridx++) {
		if (sc->txpow20mhz[ridx] == 0xffffffff)
			continue;
		run_write(sc, RT2860_TX_PWR_CFG(ridx), sc->txpow20mhz[ridx]);
	}

	for (i = 0; i < (int)__arraycount(rt2870_def_mac); i++)
		run_write(sc, rt2870_def_mac[i].reg, rt2870_def_mac[i].val);
	run_write(sc, RT2860_WMM_AIFSN_CFG, 0x00002273);
	run_write(sc, RT2860_WMM_CWMIN_CFG, 0x00002344);
	run_write(sc, RT2860_WMM_CWMAX_CFG, 0x000034aa);

	if (sc->mac_ver >= 0x3070) {
		/* set delay of PA_PE assertion to 1us (unit of 0.25us) */
		run_write(sc, RT2860_TX_SW_CFG0,
		    4 << RT2860_DLY_PAPE_EN_SHIFT);
	}

	/* wait while MAC is busy */
	for (ntries = 0; ntries < 100; ntries++) {
		if ((error = run_read(sc, RT2860_MAC_STATUS_REG, &tmp)) != 0)
			goto fail;
		if (!(tmp & (RT2860_RX_STATUS_BUSY | RT2860_TX_STATUS_BUSY)))
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		error = ETIMEDOUT;
		goto fail;
	}

	/* clear Host to MCU mailbox */
	run_write(sc, RT2860_H2M_BBPAGENT, 0);
	run_write(sc, RT2860_H2M_MAILBOX, 0);
	DELAY(1000);

	if ((error = run_bbp_init(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not initialize BBP\n");
		goto fail;
	}

	run_read(sc, RT2860_BCN_TIME_CFG, &tmp);
	tmp &= ~(RT2860_BCN_TX_EN | RT2860_TSF_TIMER_EN |
	    RT2860_TBTT_TIMER_EN);
	run_write(sc, RT2860_BCN_TIME_CFG, tmp);

	/* clear RX WCID search table */
	run_set_region_4(sc, RT2860_WCID_ENTRY(0), 0, 512);
	/* clear Pair-wise key table */
	run_set_region_4(sc, RT2860_PKEY(0), 0, 2048);
	/* clear IV/EIV table */
	run_set_region_4(sc, RT2860_IVEIV(0), 0, 512);
	/* clear WCID attribute table */
	run_set_region_4(sc, RT2860_WCID_ATTR(0), 0, 8 * 32);
	/* clear shared key table */
	run_set_region_4(sc, RT2860_SKEY(0, 0), 0, 8 * 32);
	/* clear shared key mode */
	run_set_region_4(sc, RT2860_SKEY_MODE_0_7, 0, 4);

	run_read(sc, RT2860_US_CYC_CNT, &tmp);
	tmp = (tmp & ~0xff) | 0x1e;
	run_write(sc, RT2860_US_CYC_CNT, tmp);

	if (sc->mac_rev != 0x0101)
		run_write(sc, RT2860_TXOP_CTRL_CFG, 0x0000583f);

	run_write(sc, RT2860_WMM_TXOP0_CFG, 0);
	run_write(sc, RT2860_WMM_TXOP1_CFG, 48 << 16 | 96);

	/* write vendor-specific BBP values (from EEPROM) */
	for (i = 0; i < 8; i++) {
		if (sc->bbp[i].reg == 0 || sc->bbp[i].reg == 0xff)
			continue;
		run_bbp_write(sc, sc->bbp[i].reg, sc->bbp[i].val);
	}

	/* select Main antenna for 1T1R devices */
	if (sc->rf_rev == RT3070_RF_3020)
		run_set_rx_antenna(sc, 0);

	/* send LEDs operating mode to microcontroller */
	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LED1, sc->led[0]);
	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LED2, sc->led[1]);
	(void)run_mcu_cmd(sc, RT2860_MCU_CMD_LED3, sc->led[2]);

	if (sc->mac_ver >= 0x3070)
		run_rt3070_rf_init(sc);

	/* disable non-existing Rx chains */
	run_bbp_read(sc, 3, &bbp3);
	bbp3 &= ~(1 << 3 | 1 << 4);
	if (sc->nrxchains == 2)
		bbp3 |= 1 << 3;
	else if (sc->nrxchains == 3)
		bbp3 |= 1 << 4;
	run_bbp_write(sc, 3, bbp3);

	/* disable non-existing Tx chains */
	run_bbp_read(sc, 1, &bbp1);
	if (sc->ntxchains == 1)
		bbp1 &= ~(1 << 3 | 1 << 4);
	run_bbp_write(sc, 1, bbp1);

	if (sc->mac_ver >= 0x3070)
		run_rt3070_rf_setup(sc);

	/* select default channel */
	run_set_chan(sc, ic->ic_curchan);

	/* turn radio LED on */
	run_set_leds(sc, RT2860_LED_RADIO);

#ifdef RUN_HWCRYPTO
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		/* install WEP keys */
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			(void)run_set_key(ic, &ic->ic_crypto.cs_nw_keys[i],
			    NULL);
	}
#endif

	for (i = 0; i < RUN_RX_RING_COUNT; i++) {
		struct run_rx_data *data = &sc->rxq.data[i];

		usbd_setup_xfer(data->xfer, sc->rxq.pipeh, data, data->buf,
		    RUN_MAX_RXSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, run_rxeof);
		error = usbd_transfer(data->xfer);
		if (error != USBD_NORMAL_COMPLETION &&
		    error != USBD_IN_PROGRESS)
			goto fail;
	}

	if ((error = run_txrx_enable(sc)) != 0)
		goto fail;

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	if (error != 0)
fail:		run_stop(ifp, 1);
	return (error);
}

static void
run_stop(struct ifnet *ifp, int disable)
{
	struct run_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int ntries, qid;

	if (ifp->if_flags & IFF_RUNNING)
		run_set_leds(sc, 0);	/* turn all LEDs off */

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	callout_stop(&sc->scan_to);
	callout_stop(&sc->calib_to);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	/* wait for all queued asynchronous commands to complete */
	while (sc->cmdq.queued > 0)
		tsleep(&sc->cmdq, 0, "cmdq", 0);

	/* disable Tx/Rx */
	run_read(sc, RT2860_MAC_SYS_CTRL, &tmp);
	tmp &= ~(RT2860_MAC_RX_EN | RT2860_MAC_TX_EN);
	run_write(sc, RT2860_MAC_SYS_CTRL, tmp);

	/* wait for pending Tx to complete */
	for (ntries = 0; ntries < 100; ntries++) {
		if (run_read(sc, RT2860_TXRXQ_PCNT, &tmp) != 0)
			break;
		if ((tmp & RT2860_TX2Q_PCNT_MASK) == 0)
			break;
	}
	DELAY(1000);
	run_write(sc, RT2860_USB_DMA_CFG, 0);

	/* reset adapter */
	run_write(sc, RT2860_MAC_SYS_CTRL, RT2860_BBP_HRST | RT2860_MAC_SRST);
	run_write(sc, RT2860_MAC_SYS_CTRL, 0);

	/* reset Tx and Rx rings */
	sc->qfullmsk = 0;
	for (qid = 0; qid < 4; qid++)
		run_free_tx_ring(sc, qid);
	run_free_rx_ring(sc);
}

#ifndef IEEE80211_STA_ONLY
static int
run_setup_beacon(struct run_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rt2860_txwi txwi;
	struct mbuf *m;
	int ridx;

	if ((m = ieee80211_beacon_alloc(ic, ic->ic_bss, &sc->sc_bo)) == NULL)
		return (ENOBUFS);

	memset(&txwi, 0, sizeof txwi);
	txwi.wcid = 0xff;
	txwi.len = htole16(m->m_pkthdr.len);
	/* send beacons at the lowest available rate */
	ridx = (ic->ic_curmode == IEEE80211_MODE_11A) ?
	    RT2860_RIDX_OFDM6 : RT2860_RIDX_CCK1;
	txwi.phy = htole16(rt2860_rates[ridx].mcs);
	if (rt2860_rates[ridx].phy == IEEE80211_T_OFDM)
		txwi.phy |= htole16(RT2860_PHY_OFDM);
	txwi.txop = RT2860_TX_TXOP_HT;
	txwi.flags = RT2860_TX_TS;

	run_write_region_1(sc, RT2860_BCN_BASE(0),
	    (uint8_t *)&txwi, sizeof txwi);
	run_write_region_1(sc, RT2860_BCN_BASE(0) + sizeof txwi,
	    mtod(m, uint8_t *), m->m_pkthdr.len);

	m_freem(m);

	return (0);
}
#endif

MODULE(MODULE_CLASS_DRIVER, if_run, "bpf");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
if_run_modcmd(modcmd_t cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_run,
		    cfattach_ioconf_run, cfdata_ioconf_run);
#endif
		return (error);
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_run,
		    cfattach_ioconf_run, cfdata_ioconf_run);
#endif
		return (error);
	default:
		return (ENOTTY);
	}
}
