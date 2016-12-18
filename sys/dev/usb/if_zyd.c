/*	$OpenBSD: if_zyd.c,v 1.52 2007/02/11 00:08:04 jsg Exp $	*/
/*	$NetBSD: if_zyd.c,v 1.37 2015/01/07 07:05:48 ozaki-r Exp $	*/

/*-
 * Copyright (c) 2006 by Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 by Florian Stoehr <ich@florian-stoehr.de>
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
 * ZyDAS ZD1211/ZD1211B USB WLAN driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_zyd.c,v 1.37 2015/01/07 07:05:48 ozaki-r Exp $");

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/endian.h>

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

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_zydreg.h>

#ifdef ZYD_DEBUG
#define DPRINTF(x)	do { if (zyddebug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (zyddebug > (n)) printf x; } while (0)
int zyddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

static const struct zyd_phy_pair zyd_def_phy[] = ZYD_DEF_PHY;
static const struct zyd_phy_pair zyd_def_phyB[] = ZYD_DEF_PHYB;

/* various supported device vendors/products */
#define ZYD_ZD1211_DEV(v, p)	\
	{ { USB_VENDOR_##v, USB_PRODUCT_##v##_##p }, ZYD_ZD1211 }
#define ZYD_ZD1211B_DEV(v, p)	\
	{ { USB_VENDOR_##v, USB_PRODUCT_##v##_##p }, ZYD_ZD1211B }
static const struct zyd_type {
	struct usb_devno	dev;
	uint8_t			rev;
#define ZYD_ZD1211	0
#define ZYD_ZD1211B	1
} zyd_devs[] = {
	ZYD_ZD1211_DEV(3COM2,		3CRUSB10075),
	ZYD_ZD1211_DEV(ABOCOM,		WL54),
	ZYD_ZD1211_DEV(ASUSTEK,		WL159G),
	ZYD_ZD1211_DEV(CYBERTAN,	TG54USB),
	ZYD_ZD1211_DEV(DRAYTEK,		VIGOR550),
	ZYD_ZD1211_DEV(PLANEX2,		GWUS54GD),
	ZYD_ZD1211_DEV(PLANEX2,		GWUS54GZL),
	ZYD_ZD1211_DEV(PLANEX3,		GWUS54GZ),
	ZYD_ZD1211_DEV(PLANEX3,		GWUS54MINI),
	ZYD_ZD1211_DEV(SAGEM,		XG760A),
	ZYD_ZD1211_DEV(SENAO,		NUB8301),
	ZYD_ZD1211_DEV(SITECOMEU,	WL113),
	ZYD_ZD1211_DEV(SWEEX,		ZD1211),
	ZYD_ZD1211_DEV(TEKRAM,		QUICKWLAN),
	ZYD_ZD1211_DEV(TEKRAM,		ZD1211_1),
	ZYD_ZD1211_DEV(TEKRAM,		ZD1211_2),
	ZYD_ZD1211_DEV(TWINMOS,		G240),
	ZYD_ZD1211_DEV(UMEDIA,		ALL0298V2),
	ZYD_ZD1211_DEV(UMEDIA,		TEW429UB_A),
	ZYD_ZD1211_DEV(UMEDIA,		TEW429UB),
	ZYD_ZD1211_DEV(WISTRONNEWEB,	UR055G),
	ZYD_ZD1211_DEV(ZCOM,		ZD1211),
	ZYD_ZD1211_DEV(ZYDAS,		ZD1211),
	ZYD_ZD1211_DEV(ZYXEL,		AG225H),
	ZYD_ZD1211_DEV(ZYXEL,		ZYAIRG220),
	ZYD_ZD1211_DEV(ZYXEL,		G200V2),

	ZYD_ZD1211B_DEV(ACCTON,		SMCWUSBG),
	ZYD_ZD1211B_DEV(ACCTON,		WN4501H_LF_IR),
	ZYD_ZD1211B_DEV(ACCTON,		WUS201),
	ZYD_ZD1211B_DEV(ACCTON,		ZD1211B),
	ZYD_ZD1211B_DEV(ASUSTEK,	A9T_WIFI),
	ZYD_ZD1211B_DEV(BELKIN,		F5D7050C),
	ZYD_ZD1211B_DEV(BELKIN,		ZD1211B),
	ZYD_ZD1211B_DEV(BEWAN,		BWIFI_USB54AR),
	ZYD_ZD1211B_DEV(CISCOLINKSYS,	WUSBF54G),
	ZYD_ZD1211B_DEV(CYBERTAN,	ZD1211B),
	ZYD_ZD1211B_DEV(FIBERLINE,	WL430U),
	ZYD_ZD1211B_DEV(MELCO,		KG54L),
	ZYD_ZD1211B_DEV(PHILIPS,	SNU5600),
	ZYD_ZD1211B_DEV(PHILIPS,	SNU5630NS05),
	ZYD_ZD1211B_DEV(PLANEX2,	GWUS54GXS),
	ZYD_ZD1211B_DEV(SAGEM,		XG76NA),
	ZYD_ZD1211B_DEV(SITECOMEU,	WL603),
	ZYD_ZD1211B_DEV(SITECOMEU,	ZD1211B),
	ZYD_ZD1211B_DEV(SONY,		IFU_WLM2),
	ZYD_ZD1211B_DEV(UMEDIA,		TEW429UBC1),
	ZYD_ZD1211B_DEV(UNKNOWN1,	ZD1211B_1),
	ZYD_ZD1211B_DEV(UNKNOWN1,	ZD1211B_2),
	ZYD_ZD1211B_DEV(UNKNOWN2,	ZD1211B),
	ZYD_ZD1211B_DEV(UNKNOWN3,	ZD1211B),
	ZYD_ZD1211B_DEV(USR,		USR5423),
	ZYD_ZD1211B_DEV(VTECH,		ZD1211B),
	ZYD_ZD1211B_DEV(ZCOM,		ZD1211B),
	ZYD_ZD1211B_DEV(ZYDAS,		ZD1211B),
	ZYD_ZD1211B_DEV(ZYDAS,		ZD1211B_2),
	ZYD_ZD1211B_DEV(ZYXEL,		M202),
	ZYD_ZD1211B_DEV(ZYXEL,		G220V2),
};
#define zyd_lookup(v, p)	\
	((const struct zyd_type *)usb_lookup(zyd_devs, v, p))

int zyd_match(device_t, cfdata_t, void *);
void zyd_attach(device_t, device_t, void *);
int zyd_detach(device_t, int);
int zyd_activate(device_t, enum devact);
extern struct cfdriver zyd_cd;

CFATTACH_DECL_NEW(zyd, sizeof(struct zyd_softc), zyd_match,
    zyd_attach, zyd_detach, zyd_activate);

Static void	zyd_attachhook(device_t);
Static int	zyd_complete_attach(struct zyd_softc *);
Static int	zyd_open_pipes(struct zyd_softc *);
Static void	zyd_close_pipes(struct zyd_softc *);
Static int	zyd_alloc_tx_list(struct zyd_softc *);
Static void	zyd_free_tx_list(struct zyd_softc *);
Static int	zyd_alloc_rx_list(struct zyd_softc *);
Static void	zyd_free_rx_list(struct zyd_softc *);
Static struct	ieee80211_node *zyd_node_alloc(struct ieee80211_node_table *);
Static int	zyd_media_change(struct ifnet *);
Static void	zyd_next_scan(void *);
Static void	zyd_task(void *);
Static int	zyd_newstate(struct ieee80211com *, enum ieee80211_state, int);
Static int	zyd_cmd(struct zyd_softc *, uint16_t, const void *, int,
		    void *, int, u_int);
Static int	zyd_read16(struct zyd_softc *, uint16_t, uint16_t *);
Static int	zyd_read32(struct zyd_softc *, uint16_t, uint32_t *);
Static int	zyd_write16(struct zyd_softc *, uint16_t, uint16_t);
Static int	zyd_write32(struct zyd_softc *, uint16_t, uint32_t);
Static int	zyd_rfwrite(struct zyd_softc *, uint32_t);
Static void	zyd_lock_phy(struct zyd_softc *);
Static void	zyd_unlock_phy(struct zyd_softc *);
Static int	zyd_rfmd_init(struct zyd_rf *);
Static int	zyd_rfmd_switch_radio(struct zyd_rf *, int);
Static int	zyd_rfmd_set_channel(struct zyd_rf *, uint8_t);
Static int	zyd_al2230_init(struct zyd_rf *);
Static int	zyd_al2230_switch_radio(struct zyd_rf *, int);
Static int	zyd_al2230_set_channel(struct zyd_rf *, uint8_t);
Static int	zyd_al2230_init_b(struct zyd_rf *);
Static int	zyd_al7230B_init(struct zyd_rf *);
Static int	zyd_al7230B_switch_radio(struct zyd_rf *, int);
Static int	zyd_al7230B_set_channel(struct zyd_rf *, uint8_t);
Static int	zyd_al2210_init(struct zyd_rf *);
Static int	zyd_al2210_switch_radio(struct zyd_rf *, int);
Static int	zyd_al2210_set_channel(struct zyd_rf *, uint8_t);
Static int	zyd_gct_init(struct zyd_rf *);
Static int	zyd_gct_switch_radio(struct zyd_rf *, int);
Static int	zyd_gct_set_channel(struct zyd_rf *, uint8_t);
Static int	zyd_maxim_init(struct zyd_rf *);
Static int	zyd_maxim_switch_radio(struct zyd_rf *, int);
Static int	zyd_maxim_set_channel(struct zyd_rf *, uint8_t);
Static int	zyd_maxim2_init(struct zyd_rf *);
Static int	zyd_maxim2_switch_radio(struct zyd_rf *, int);
Static int	zyd_maxim2_set_channel(struct zyd_rf *, uint8_t);
Static int	zyd_rf_attach(struct zyd_softc *, uint8_t);
Static const char *zyd_rf_name(uint8_t);
Static int	zyd_hw_init(struct zyd_softc *);
Static int	zyd_read_eeprom(struct zyd_softc *);
Static int	zyd_set_macaddr(struct zyd_softc *, const uint8_t *);
Static int	zyd_set_bssid(struct zyd_softc *, const uint8_t *);
Static int	zyd_switch_radio(struct zyd_softc *, int);
Static void	zyd_set_led(struct zyd_softc *, int, int);
Static int	zyd_set_rxfilter(struct zyd_softc *);
Static void	zyd_set_chan(struct zyd_softc *, struct ieee80211_channel *);
Static int	zyd_set_beacon_interval(struct zyd_softc *, int);
Static uint8_t	zyd_plcp_signal(int);
Static void	zyd_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void	zyd_rx_data(struct zyd_softc *, const uint8_t *, uint16_t);
Static void	zyd_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void	zyd_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static int	zyd_tx_mgt(struct zyd_softc *, struct mbuf *,
		    struct ieee80211_node *);
Static int	zyd_tx_data(struct zyd_softc *, struct mbuf *,
		    struct ieee80211_node *);
Static void	zyd_start(struct ifnet *);
Static void	zyd_watchdog(struct ifnet *);
Static int	zyd_ioctl(struct ifnet *, u_long, void *);
Static int	zyd_init(struct ifnet *);
Static void	zyd_stop(struct ifnet *, int);
Static int	zyd_loadfirmware(struct zyd_softc *, u_char *, size_t);
Static void	zyd_iter_func(void *, struct ieee80211_node *);
Static void	zyd_amrr_timeout(void *);
Static void	zyd_newassoc(struct ieee80211_node *, int);

static const struct ieee80211_rateset zyd_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset zyd_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

int
zyd_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (zyd_lookup(uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

Static void
zyd_attachhook(device_t self)
{
	struct zyd_softc *sc = device_private(self);
	firmware_handle_t fwh;
	const char *fwname;
	u_char *fw;
	size_t size;
	int error;

	fwname = (sc->mac_rev == ZYD_ZD1211) ? "zyd-zd1211" : "zyd-zd1211b";
	if ((error = firmware_open("zyd", fwname, &fwh)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "failed to open firmware %s (error=%d)\n", fwname, error);
		return;
	}
	size = firmware_get_size(fwh);
	fw = firmware_malloc(size);
	if (fw == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "failed to allocate firmware memory\n");
		firmware_close(fwh);
		return;
	}
	error = firmware_read(fwh, 0, fw, size);
	firmware_close(fwh);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "failed to read firmware (error %d)\n", error);
		firmware_free(fw, size);
		return;
	}

	error = zyd_loadfirmware(sc, fw, size);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load firmware (error=%d)\n", error);
		firmware_free(fw, size);
		return;
	}

	firmware_free(fw, size);
	sc->sc_flags |= ZD1211_FWLOADED;

	/* complete the attach process */
	if ((error = zyd_complete_attach(sc)) == 0)
		sc->attached = 1;
	return;
}

void
zyd_attach(device_t parent, device_t self, void *aux)
{
	struct zyd_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	char *devinfop;
	usb_device_descriptor_t* ddesc;
	struct ifnet *ifp = &sc->sc_if;

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	sc->sc_flags = 0;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->mac_rev = zyd_lookup(uaa->vendor, uaa->product)->rev;

	ddesc = usbd_get_device_descriptor(sc->sc_udev);
	if (UGETW(ddesc->bcdDevice) < 0x4330) {
		aprint_error_dev(self, "device version mismatch: 0x%x "
		    "(only >= 43.30 supported)\n", UGETW(ddesc->bcdDevice));
		return;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = zyd_init;
	ifp->if_ioctl = zyd_ioctl;
	ifp->if_start = zyd_start;
	ifp->if_watchdog = zyd_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	SIMPLEQ_INIT(&sc->sc_rqh);

	/* defer configrations after file system is ready to load firmware */
	config_mountroot(self, zyd_attachhook);
}

Static int
zyd_complete_attach(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	usbd_status error;
	int i;

	usb_init_task(&sc->sc_task, zyd_task, sc, 0);
	callout_init(&(sc->sc_scan_ch), 0);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 10;
	callout_init(&sc->sc_amrr_ch, 0);

	error = usbd_set_config_no(sc->sc_udev, ZYD_CONFIG_NO, 1);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(error));
		goto fail;
	}

	error = usbd_device2interface_handle(sc->sc_udev, ZYD_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "getting interface handle failed\n");
		goto fail;
	}

	if ((error = zyd_open_pipes(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not open pipes\n");
		goto fail;
	}

	if ((error = zyd_read_eeprom(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not read EEPROM\n");
		goto fail;
	}

	if ((error = zyd_rf_attach(sc, sc->rf_rev)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not attach RF\n");
		goto fail;
	}

	if ((error = zyd_hw_init(sc)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "hardware initialization failed\n");
		goto fail;
	}

	aprint_normal_dev(sc->sc_dev,
	    "HMAC ZD1211%s, FW %02x.%02x, RF %s, PA %x, address %s\n",
	    (sc->mac_rev == ZYD_ZD1211) ? "": "B",
	    sc->fw_rev >> 8, sc->fw_rev & 0xff, zyd_rf_name(sc->rf_rev),
	    sc->pa_rev, ether_sprintf(ic->ic_myaddr));

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_WEP;		/* s/w WEP */

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = zyd_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = zyd_rateset_11g;

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
	ic->ic_node_alloc = zyd_node_alloc;
	ic->ic_newassoc = zyd_newassoc;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = zyd_newstate;
	ieee80211_media_init(ic, zyd_media_change, ieee80211_media_status);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN,
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(ZYD_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(ZYD_TX_RADIOTAP_PRESENT);

	ieee80211_announce(ic);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);

fail:	return error;
}

int
zyd_detach(device_t self, int flags)
{
	struct zyd_softc *sc = device_private(self);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	int s;

	if (!sc->attached)
		return 0;

	s = splusb();

	zyd_stop(ifp, 1);
	usb_rem_task(sc->sc_udev, &sc->sc_task);
	callout_stop(&sc->sc_scan_ch);
	callout_stop(&sc->sc_amrr_ch);

	zyd_close_pipes(sc);

	sc->attached = 0;

	bpf_detach(ifp);
	ieee80211_ifdetach(ic);
	if_detach(ifp);

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    sc->sc_dev);

	return 0;
}

Static int
zyd_open_pipes(struct zyd_softc *sc)
{
	usb_endpoint_descriptor_t *edesc;
	int isize;
	usbd_status error;

	/* interrupt in */
	edesc = usbd_get_endpoint_descriptor(sc->sc_iface, 0x83);
	if (edesc == NULL)
		return EINVAL;

	isize = UGETW(edesc->wMaxPacketSize);
	if (isize == 0)	/* should not happen */
		return EINVAL;

	sc->ibuf = malloc(isize, M_USBDEV, M_NOWAIT);
	if (sc->ibuf == NULL)
		return ENOMEM;

	error = usbd_open_pipe_intr(sc->sc_iface, 0x83, USBD_SHORT_XFER_OK,
	    &sc->zyd_ep[ZYD_ENDPT_IIN], sc, sc->ibuf, isize, zyd_intr,
	    USBD_DEFAULT_INTERVAL);
	if (error != 0) {
		printf("%s: open rx intr pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/* interrupt out (not necessarily an interrupt pipe) */
	error = usbd_open_pipe(sc->sc_iface, 0x04, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_IOUT]);
	if (error != 0) {
		printf("%s: open tx intr pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/* bulk in */
	error = usbd_open_pipe(sc->sc_iface, 0x82, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_BIN]);
	if (error != 0) {
		printf("%s: open rx pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/* bulk out */
	error = usbd_open_pipe(sc->sc_iface, 0x01, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_BOUT]);
	if (error != 0) {
		printf("%s: open tx pipe failed: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	return 0;

fail:	zyd_close_pipes(sc);
	return error;
}

Static void
zyd_close_pipes(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_ENDPT_CNT; i++) {
		if (sc->zyd_ep[i] != NULL) {
			usbd_abort_pipe(sc->zyd_ep[i]);
			usbd_close_pipe(sc->zyd_ep[i]);
			sc->zyd_ep[i] = NULL;
		}
	}
	if (sc->ibuf != NULL) {
		free(sc->ibuf, M_USBDEV);
		sc->ibuf = NULL;
	}
}

Static int
zyd_alloc_tx_list(struct zyd_softc *sc)
{
	int i, error;

	sc->tx_queued = 0;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
		struct zyd_tx_data *data = &sc->tx_data[i];

		data->sc = sc;	/* backpointer for callbacks */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate tx xfer\n",
			    device_xname(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ZYD_MAX_TXBUFSZ);
		if (data->buf == NULL) {
			printf("%s: could not allocate tx buffer\n",
			    device_xname(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		/* clear Tx descriptor */
		memset(data->buf, 0, sizeof (struct zyd_tx_desc));
	}
	return 0;

fail:	zyd_free_tx_list(sc);
	return error;
}

Static void
zyd_free_tx_list(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
		struct zyd_tx_data *data = &sc->tx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}
		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}
}

Static int
zyd_alloc_rx_list(struct zyd_softc *sc)
{
	int i, error;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		data->sc = sc;	/* backpointer for callbacks */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate rx xfer\n",
			    device_xname(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ZYX_MAX_RXBUFSZ);
		if (data->buf == NULL) {
			printf("%s: could not allocate rx buffer\n",
			    device_xname(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
	}
	return 0;

fail:	zyd_free_rx_list(sc);
	return error;
}

Static void
zyd_free_rx_list(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}
	}
}

/* ARGUSED */
Static struct ieee80211_node *
zyd_node_alloc(struct ieee80211_node_table *nt __unused)
{
	struct zyd_node *zn;

	zn = malloc(sizeof (struct zyd_node), M_80211_NODE, M_NOWAIT | M_ZERO);

	return &zn->ni;
}

Static int
zyd_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		zyd_init(ifp);

	return 0;
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
Static void
zyd_next_scan(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
}

Static void
zyd_task(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;

	ostate = ic->ic_state;

	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
			/* turn link LED off */
			zyd_set_led(sc, ZYD_LED1, 0);

			/* stop data LED from blinking */
			zyd_write32(sc, sc->fwbase + ZYD_FW_LINK_STATUS, 0);
		}
		break;

	case IEEE80211_S_SCAN:
		zyd_set_chan(sc, ic->ic_curchan);
		callout_reset(&sc->sc_scan_ch, hz / 5, zyd_next_scan, sc);
		break;

	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		zyd_set_chan(sc, ic->ic_curchan);
		break;

	case IEEE80211_S_RUN:
	{
		struct ieee80211_node *ni = ic->ic_bss;

		zyd_set_chan(sc, ic->ic_curchan);

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			/* turn link LED on */
			zyd_set_led(sc, ZYD_LED1, 1);

			/* make data LED blink upon Tx */
			zyd_write32(sc, sc->fwbase + ZYD_FW_LINK_STATUS, 1);

			zyd_set_bssid(sc, ni->ni_bssid);
		}

		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* fake a join to init the tx rate */
			zyd_newassoc(ni, 1);
		}

		/* start automatic rate control timer */
		if (ic->ic_fixed_rate == IEEE80211_FIXED_RATE_NONE)
			callout_reset(&sc->sc_amrr_ch, hz, zyd_amrr_timeout, sc);

		break;
	}
	}

	sc->sc_newstate(ic, sc->sc_state, -1);
}

Static int
zyd_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct zyd_softc *sc = ic->ic_ifp->if_softc;

	if (!sc->attached)
		return ENXIO;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	callout_stop(&sc->sc_scan_ch);
	callout_stop(&sc->sc_amrr_ch);

	/* do it in a process context */
	sc->sc_state = nstate;
	usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);

	return 0;
}

Static int
zyd_cmd(struct zyd_softc *sc, uint16_t code, const void *idata, int ilen,
    void *odata, int olen, u_int flags)
{
	usbd_xfer_handle xfer;
	struct zyd_cmd cmd;
	struct rq rq;
	uint16_t xferflags;
	int error;
	usbd_status uerror;
	int s = 0;

	if ((xfer = usbd_alloc_xfer(sc->sc_udev)) == NULL)
		return ENOMEM;

	cmd.code = htole16(code);
	bcopy(idata, cmd.data, ilen);

	xferflags = USBD_FORCE_SHORT_XFER;
	if (!(flags & ZYD_CMD_FLAG_READ))
		xferflags |= USBD_SYNCHRONOUS;
	else {
		s = splusb();
		rq.idata = idata;
		rq.odata = odata;
		rq.len = olen / sizeof (struct zyd_pair);
		SIMPLEQ_INSERT_TAIL(&sc->sc_rqh, &rq, rq);
	}

	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_IOUT], 0, &cmd,
	    sizeof (uint16_t) + ilen, xferflags, ZYD_INTR_TIMEOUT, NULL);
	uerror = usbd_transfer(xfer);
	if (uerror != USBD_IN_PROGRESS && uerror != 0) {
		if (flags & ZYD_CMD_FLAG_READ)
			splx(s);
		printf("%s: could not send command (error=%s)\n",
		    device_xname(sc->sc_dev), usbd_errstr(uerror));
		(void)usbd_free_xfer(xfer);
		return EIO;
	}
	if (!(flags & ZYD_CMD_FLAG_READ)) {
		(void)usbd_free_xfer(xfer);
		return 0;	/* write: don't wait for reply */
	}
	/* wait at most one second for command reply */
	error = tsleep(odata, PCATCH, "zydcmd", hz);
	if (error == EWOULDBLOCK)
		printf("%s: zyd_read sleep timeout\n", device_xname(sc->sc_dev));
	SIMPLEQ_REMOVE(&sc->sc_rqh, &rq, rq, rq);
	splx(s);

	(void)usbd_free_xfer(xfer);
	return error;
}

Static int
zyd_read16(struct zyd_softc *sc, uint16_t reg, uint16_t *val)
{
	struct zyd_pair tmp;
	int error;

	reg = htole16(reg);
	error = zyd_cmd(sc, ZYD_CMD_IORD, &reg, sizeof reg, &tmp, sizeof tmp,
	    ZYD_CMD_FLAG_READ);
	if (error == 0)
		*val = le16toh(tmp.val);
	else
		*val = 0;
	return error;
}

Static int
zyd_read32(struct zyd_softc *sc, uint16_t reg, uint32_t *val)
{
	struct zyd_pair tmp[2];
	uint16_t regs[2];
	int error;

	regs[0] = htole16(ZYD_REG32_HI(reg));
	regs[1] = htole16(ZYD_REG32_LO(reg));
	error = zyd_cmd(sc, ZYD_CMD_IORD, regs, sizeof regs, tmp, sizeof tmp,
	    ZYD_CMD_FLAG_READ);
	if (error == 0)
		*val = le16toh(tmp[0].val) << 16 | le16toh(tmp[1].val);
	else
		*val = 0;
	return error;
}

Static int
zyd_write16(struct zyd_softc *sc, uint16_t reg, uint16_t val)
{
	struct zyd_pair pair;

	pair.reg = htole16(reg);
	pair.val = htole16(val);

	return zyd_cmd(sc, ZYD_CMD_IOWR, &pair, sizeof pair, NULL, 0, 0);
}

Static int
zyd_write32(struct zyd_softc *sc, uint16_t reg, uint32_t val)
{
	struct zyd_pair pair[2];

	pair[0].reg = htole16(ZYD_REG32_HI(reg));
	pair[0].val = htole16(val >> 16);
	pair[1].reg = htole16(ZYD_REG32_LO(reg));
	pair[1].val = htole16(val & 0xffff);

	return zyd_cmd(sc, ZYD_CMD_IOWR, pair, sizeof pair, NULL, 0, 0);
}

Static int
zyd_rfwrite(struct zyd_softc *sc, uint32_t val)
{
	struct zyd_rf *rf = &sc->sc_rf;
	struct zyd_rfwrite req;
	uint16_t cr203;
	int i;

	(void)zyd_read16(sc, ZYD_CR203, &cr203);
	cr203 &= ~(ZYD_RF_IF_LE | ZYD_RF_CLK | ZYD_RF_DATA);

	req.code  = htole16(2);
	req.width = htole16(rf->width);
	for (i = 0; i < rf->width; i++) {
		req.bit[i] = htole16(cr203);
		if (val & (1 << (rf->width - 1 - i)))
			req.bit[i] |= htole16(ZYD_RF_DATA);
	}
	return zyd_cmd(sc, ZYD_CMD_RFCFG, &req, 4 + 2 * rf->width, NULL, 0, 0);
}

Static void
zyd_lock_phy(struct zyd_softc *sc)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_MISC, &tmp);
	tmp &= ~ZYD_UNLOCK_PHY_REGS;
	(void)zyd_write32(sc, ZYD_MAC_MISC, tmp);
}

Static void
zyd_unlock_phy(struct zyd_softc *sc)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_MISC, &tmp);
	tmp |= ZYD_UNLOCK_PHY_REGS;
	(void)zyd_write32(sc, ZYD_MAC_MISC, tmp);
}

/*
 * RFMD RF methods.
 */
Static int
zyd_rfmd_init(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_RFMD_PHY;
	static const uint32_t rfini[] = ZYD_RFMD_RF;
	int error;
	size_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	/* init RFMD radio */
	for (i = 0; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
}

Static int
zyd_rfmd_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;

	(void)zyd_write16(sc, ZYD_CR10, on ? 0x89 : 0x15);
	(void)zyd_write16(sc, ZYD_CR11, on ? 0x00 : 0x81);

	return 0;
}

Static int
zyd_rfmd_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_RFMD_CHANTABLE;

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);

	return 0;
}

/*
 * AL2230 RF methods.
 */
Static int
zyd_al2230_init(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_AL2230_PHY;
	static const struct zyd_phy_pair phy2230s[] = ZYD_AL2230S_PHY_INIT;
	static const uint32_t rfini[] = ZYD_AL2230_RF;
	int error;
	size_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	if (sc->rf_rev == ZYD_RF_AL2230S) {
		for (i = 0; i < __arraycount(phy2230s); i++) {
			error = zyd_write16(sc, phy2230s[i].reg,
			    phy2230s[i].val);
			if (error != 0)
				return error;
		}
	}

	/* init AL2230 radio */
	for (i = 0; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
}

Static int
zyd_al2230_init_b(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_AL2230_PHY_B;
	static const uint32_t rfini[] = ZYD_AL2230_RF_B;
	int error;
	size_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	/* init AL2230 radio */
	for (i = 0; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
}

Static int
zyd_al2230_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;
	int on251 = (sc->mac_rev == ZYD_ZD1211) ? 0x3f : 0x7f;

	(void)zyd_write16(sc, ZYD_CR11,  on ? 0x00 : 0x04);
	(void)zyd_write16(sc, ZYD_CR251, on ? on251 : 0x2f);

	return 0;
}

Static int
zyd_al2230_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2, r3;
	} rfprog[] = ZYD_AL2230_CHANTABLE;

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r3);

	(void)zyd_write16(sc, ZYD_CR138, 0x28);
	(void)zyd_write16(sc, ZYD_CR203, 0x06);

	return 0;
}

/*
 * AL7230B RF methods.
 */
Static int
zyd_al7230B_init(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini_1[] = ZYD_AL7230B_PHY_1;
	static const struct zyd_phy_pair phyini_2[] = ZYD_AL7230B_PHY_2;
	static const struct zyd_phy_pair phyini_3[] = ZYD_AL7230B_PHY_3;
	static const uint32_t rfini_1[] = ZYD_AL7230B_RF_1;
	static const uint32_t rfini_2[] = ZYD_AL7230B_RF_2;
	int error;
	size_t i;

	/* for AL7230B, PHY and RF need to be initialized in "phases" */

	/* init RF-dependent PHY registers, part one */
	for (i = 0; i < __arraycount(phyini_1); i++) {
		error = zyd_write16(sc, phyini_1[i].reg, phyini_1[i].val);
		if (error != 0)
			return error;
	}
	/* init AL7230B radio, part one */
	for (i = 0; i < __arraycount(rfini_1); i++) {
		if ((error = zyd_rfwrite(sc, rfini_1[i])) != 0)
			return error;
	}
	/* init RF-dependent PHY registers, part two */
	for (i = 0; i < __arraycount(phyini_2); i++) {
		error = zyd_write16(sc, phyini_2[i].reg, phyini_2[i].val);
		if (error != 0)
			return error;
	}
	/* init AL7230B radio, part two */
	for (i = 0; i < __arraycount(rfini_2); i++) {
		if ((error = zyd_rfwrite(sc, rfini_2[i])) != 0)
			return error;
	}
	/* init RF-dependent PHY registers, part three */
	for (i = 0; i < __arraycount(phyini_3); i++) {
		error = zyd_write16(sc, phyini_3[i].reg, phyini_3[i].val);
		if (error != 0)
			return error;
	}

	return 0;
}

Static int
zyd_al7230B_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;

	(void)zyd_write16(sc, ZYD_CR11,  on ? 0x00 : 0x04);
	(void)zyd_write16(sc, ZYD_CR251, on ? 0x3f : 0x2f);

	return 0;
}

Static int
zyd_al7230B_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_AL7230B_CHANTABLE;
	static const uint32_t rfsc[] = ZYD_AL7230B_RF_SETCHANNEL;
	int error;
	size_t i;

	(void)zyd_write16(sc, ZYD_CR240, 0x57);
	(void)zyd_write16(sc, ZYD_CR251, 0x2f);

	for (i = 0; i < __arraycount(rfsc); i++) {
		if ((error = zyd_rfwrite(sc, rfsc[i])) != 0)
			return error;
	}

	(void)zyd_write16(sc, ZYD_CR128, 0x14);
	(void)zyd_write16(sc, ZYD_CR129, 0x12);
	(void)zyd_write16(sc, ZYD_CR130, 0x10);
	(void)zyd_write16(sc, ZYD_CR38,  0x38);
	(void)zyd_write16(sc, ZYD_CR136, 0xdf);

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);
	(void)zyd_rfwrite(sc, 0x3c9000);

	(void)zyd_write16(sc, ZYD_CR251, 0x3f);
	(void)zyd_write16(sc, ZYD_CR203, 0x06);
	(void)zyd_write16(sc, ZYD_CR240, 0x08);

	return 0;
}

/*
 * AL2210 RF methods.
 */
Static int
zyd_al2210_init(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_AL2210_PHY;
	static const uint32_t rfini[] = ZYD_AL2210_RF;
	uint32_t tmp;
	int error;
	size_t i;

	(void)zyd_write32(sc, ZYD_CR18, 2);

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	/* init AL2210 radio */
	for (i = 0; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);
	(void)zyd_read32(sc, ZYD_CR_RADIO_PD, &tmp);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp & ~1);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp | 1);
	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x05);
	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x00);
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);
	(void)zyd_write32(sc, ZYD_CR18, 3);

	return 0;
}

Static int
zyd_al2210_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

Static int
zyd_al2210_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const uint32_t rfprog[] = ZYD_AL2210_CHANTABLE;
	uint32_t tmp;

	(void)zyd_write32(sc, ZYD_CR18, 2);
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);
	(void)zyd_read32(sc, ZYD_CR_RADIO_PD, &tmp);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp & ~1);
	(void)zyd_write32(sc, ZYD_CR_RADIO_PD, tmp | 1);
	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x05);

	(void)zyd_write32(sc, ZYD_CR_RFCFG, 0x00);
	(void)zyd_write16(sc, ZYD_CR47, 0x1e);

	/* actually set the channel */
	(void)zyd_rfwrite(sc, rfprog[chan - 1]);

	(void)zyd_write32(sc, ZYD_CR18, 3);

	return 0;
}

/*
 * GCT RF methods.
 */
Static int
zyd_gct_init(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_GCT_PHY;
	static const uint32_t rfini[] = ZYD_GCT_RF;
	int error;
	size_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	/* init cgt radio */
	for (i = 0; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
}

Static int
zyd_gct_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

Static int
zyd_gct_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const uint32_t rfprog[] = ZYD_GCT_CHANTABLE;

	(void)zyd_rfwrite(sc, 0x1c0000);
	(void)zyd_rfwrite(sc, rfprog[chan - 1]);
	(void)zyd_rfwrite(sc, 0x1c0008);

	return 0;
}

/*
 * Maxim RF methods.
 */
Static int
zyd_maxim_init(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM_RF;
	uint16_t tmp;
	int error;
	size_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* init maxim radio */
	for (i = 0; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
}

Static int
zyd_maxim_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

Static int
zyd_maxim_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM_RF;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_MAXIM_CHANTABLE;
	uint16_t tmp;
	int error;
	size_t i;

	/*
	 * Do the same as we do when initializing it, except for the channel
	 * values coming from the two channel tables.
	 */

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* first two values taken from the chantables */
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);

	/* init maxim radio - skipping the two first values */
	for (i = 2; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
}

/*
 * Maxim2 RF methods.
 */
Static int
zyd_maxim2_init(struct zyd_rf *rf)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM2_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM2_RF;
	uint16_t tmp;
	int error;
	size_t i;

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* init maxim2 radio */
	for (i = 0; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
}

Static int
zyd_maxim2_switch_radio(struct zyd_rf *rf, int on)
{
	/* vendor driver does nothing for this RF chip */

	return 0;
}

Static int
zyd_maxim2_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_MAXIM2_PHY;
	static const uint32_t rfini[] = ZYD_MAXIM2_RF;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_MAXIM2_CHANTABLE;
	uint16_t tmp;
	int error;
	size_t i;

	/*
	 * Do the same as we do when initializing it, except for the channel
	 * values coming from the two channel tables.
	 */

	/* init RF-dependent PHY registers */
	for (i = 0; i < __arraycount(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp & ~(1 << 4));

	/* first two values taken from the chantables */
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);

	/* init maxim2 radio - skipping the two first values */
	for (i = 2; i < __arraycount(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	(void)zyd_read16(sc, ZYD_CR203, &tmp);
	(void)zyd_write16(sc, ZYD_CR203, tmp | (1 << 4));

	return 0;
}

Static int
zyd_rf_attach(struct zyd_softc *sc, uint8_t type)
{
	struct zyd_rf *rf = &sc->sc_rf;

	rf->rf_sc = sc;

	switch (type) {
	case ZYD_RF_RFMD:
		rf->init         = zyd_rfmd_init;
		rf->switch_radio = zyd_rfmd_switch_radio;
		rf->set_channel  = zyd_rfmd_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_AL2230:
	case ZYD_RF_AL2230S:
		if (sc->mac_rev == ZYD_ZD1211B)
			rf->init = zyd_al2230_init_b;
		else
			rf->init = zyd_al2230_init;
		rf->switch_radio = zyd_al2230_switch_radio;
		rf->set_channel  = zyd_al2230_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_AL7230B:
		rf->init         = zyd_al7230B_init;
		rf->switch_radio = zyd_al7230B_switch_radio;
		rf->set_channel  = zyd_al7230B_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_AL2210:
		rf->init         = zyd_al2210_init;
		rf->switch_radio = zyd_al2210_switch_radio;
		rf->set_channel  = zyd_al2210_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_GCT:
		rf->init         = zyd_gct_init;
		rf->switch_radio = zyd_gct_switch_radio;
		rf->set_channel  = zyd_gct_set_channel;
		rf->width        = 21;	/* 21-bit RF values */
		break;
	case ZYD_RF_MAXIM_NEW:
		rf->init         = zyd_maxim_init;
		rf->switch_radio = zyd_maxim_switch_radio;
		rf->set_channel  = zyd_maxim_set_channel;
		rf->width        = 18;	/* 18-bit RF values */
		break;
	case ZYD_RF_MAXIM_NEW2:
		rf->init         = zyd_maxim2_init;
		rf->switch_radio = zyd_maxim2_switch_radio;
		rf->set_channel  = zyd_maxim2_set_channel;
		rf->width        = 18;	/* 18-bit RF values */
		break;
	default:
		printf("%s: sorry, radio \"%s\" is not supported yet\n",
		    device_xname(sc->sc_dev), zyd_rf_name(type));
		return EINVAL;
	}
	return 0;
}

Static const char *
zyd_rf_name(uint8_t type)
{
	static const char * const zyd_rfs[] = {
		"unknown", "unknown", "UW2451",   "UCHIP",     "AL2230",
		"AL7230B", "THETA",   "AL2210",   "MAXIM_NEW", "GCT",
		"AL2230S", "RALINK",  "INTERSIL", "RFMD",      "MAXIM_NEW2",
		"PHILIPS"
	};

	return zyd_rfs[(type > 15) ? 0 : type];
}

Static int
zyd_hw_init(struct zyd_softc *sc)
{
	struct zyd_rf *rf = &sc->sc_rf;
	const struct zyd_phy_pair *phyp;
	int error;

	/* specify that the plug and play is finished */
	(void)zyd_write32(sc, ZYD_MAC_AFTER_PNP, 1);

	(void)zyd_read16(sc, ZYD_FIRMWARE_BASE_ADDR, &sc->fwbase);
	DPRINTF(("firmware base address=0x%04x\n", sc->fwbase));

	/* retrieve firmware revision number */
	(void)zyd_read16(sc, sc->fwbase + ZYD_FW_FIRMWARE_REV, &sc->fw_rev);

	(void)zyd_write32(sc, ZYD_CR_GPI_EN, 0);
	(void)zyd_write32(sc, ZYD_MAC_CONT_WIN_LIMIT, 0x7f043f);

	/* disable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, 0);

	/* PHY init */
	zyd_lock_phy(sc);
	phyp = (sc->mac_rev == ZYD_ZD1211B) ? zyd_def_phyB : zyd_def_phy;
	for (; phyp->reg != 0; phyp++) {
		if ((error = zyd_write16(sc, phyp->reg, phyp->val)) != 0)
			goto fail;
	}
	zyd_unlock_phy(sc);

	/* HMAC init */
	zyd_write32(sc, ZYD_MAC_ACK_EXT, 0x00000020);
	zyd_write32(sc, ZYD_CR_ADDA_MBIAS_WT, 0x30000808);

	if (sc->mac_rev == ZYD_ZD1211) {
		zyd_write32(sc, ZYD_MAC_RETRY, 0x00000002);
	} else {
		zyd_write32(sc, ZYD_MAC_RETRY, 0x02020202);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL4, 0x007f003f);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL3, 0x007f003f);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL2, 0x003f001f);
		zyd_write32(sc, ZYD_MACB_TXPWR_CTL1, 0x001f000f);
		zyd_write32(sc, ZYD_MACB_AIFS_CTL1, 0x00280028);
		zyd_write32(sc, ZYD_MACB_AIFS_CTL2, 0x008C003C);
		zyd_write32(sc, ZYD_MACB_TXOP, 0x01800824);
	}

	zyd_write32(sc, ZYD_MAC_SNIFFER, 0x00000000);
	zyd_write32(sc, ZYD_MAC_RXFILTER, 0x00000000);
	zyd_write32(sc, ZYD_MAC_GHTBL, 0x00000000);
	zyd_write32(sc, ZYD_MAC_GHTBH, 0x80000000);
	zyd_write32(sc, ZYD_MAC_MISC, 0x000000a4);
	zyd_write32(sc, ZYD_CR_ADDA_PWR_DWN, 0x0000007f);
	zyd_write32(sc, ZYD_MAC_BCNCFG, 0x00f00401);
	zyd_write32(sc, ZYD_MAC_PHY_DELAY2, 0x00000000);
	zyd_write32(sc, ZYD_MAC_ACK_EXT, 0x00000080);
	zyd_write32(sc, ZYD_CR_ADDA_PWR_DWN, 0x00000000);
	zyd_write32(sc, ZYD_MAC_SIFS_ACK_TIME, 0x00000100);
	zyd_write32(sc, ZYD_MAC_DIFS_EIFS_SIFS, 0x0547c032);
	zyd_write32(sc, ZYD_CR_RX_PE_DELAY, 0x00000070);
	zyd_write32(sc, ZYD_CR_PS_CTRL, 0x10000000);
	zyd_write32(sc, ZYD_MAC_RTSCTSRATE, 0x02030203);
	zyd_write32(sc, ZYD_MAC_RX_THRESHOLD, 0x000c0640);
	zyd_write32(sc, ZYD_MAC_BACKOFF_PROTECT, 0x00000114);

	/* RF chip init */
	zyd_lock_phy(sc);
	error = (*rf->init)(rf);
	zyd_unlock_phy(sc);
	if (error != 0) {
		printf("%s: radio initialization failed\n",
		    device_xname(sc->sc_dev));
		goto fail;
	}

	/* init beacon interval to 100ms */
	if ((error = zyd_set_beacon_interval(sc, 100)) != 0)
		goto fail;

fail:	return error;
}

Static int
zyd_read_eeprom(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	uint16_t val;
	int i;

	/* read MAC address */
	(void)zyd_read32(sc, ZYD_EEPROM_MAC_ADDR_P1, &tmp);
	ic->ic_myaddr[0] = tmp & 0xff;
	ic->ic_myaddr[1] = tmp >>  8;
	ic->ic_myaddr[2] = tmp >> 16;
	ic->ic_myaddr[3] = tmp >> 24;
	(void)zyd_read32(sc, ZYD_EEPROM_MAC_ADDR_P2, &tmp);
	ic->ic_myaddr[4] = tmp & 0xff;
	ic->ic_myaddr[5] = tmp >>  8;

	(void)zyd_read32(sc, ZYD_EEPROM_POD, &tmp);
	sc->rf_rev = tmp & 0x0f;
	sc->pa_rev = (tmp >> 16) & 0x0f;

	/* read regulatory domain (currently unused) */
	(void)zyd_read32(sc, ZYD_EEPROM_SUBID, &tmp);
	sc->regdomain = tmp >> 16;
	DPRINTF(("regulatory domain %x\n", sc->regdomain));

	/* read Tx power calibration tables */
	for (i = 0; i < 7; i++) {
		(void)zyd_read16(sc, ZYD_EEPROM_PWR_CAL + i, &val);
		sc->pwr_cal[i * 2] = val >> 8;
		sc->pwr_cal[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_PWR_INT + i, &val);
		sc->pwr_int[i * 2] = val >> 8;
		sc->pwr_int[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_36M_CAL + i, &val);
		sc->ofdm36_cal[i * 2] = val >> 8;
		sc->ofdm36_cal[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_48M_CAL + i, &val);
		sc->ofdm48_cal[i * 2] = val >> 8;
		sc->ofdm48_cal[i * 2 + 1] = val & 0xff;

		(void)zyd_read16(sc, ZYD_EEPROM_54M_CAL + i, &val);
		sc->ofdm54_cal[i * 2] = val >> 8;
		sc->ofdm54_cal[i * 2 + 1] = val & 0xff;
	}
	return 0;
}

Static int
zyd_set_macaddr(struct zyd_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0];
	(void)zyd_write32(sc, ZYD_MAC_MACADRL, tmp);

	tmp = addr[5] << 8 | addr[4];
	(void)zyd_write32(sc, ZYD_MAC_MACADRH, tmp);

	return 0;
}

Static int
zyd_set_bssid(struct zyd_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0];
	(void)zyd_write32(sc, ZYD_MAC_BSSADRL, tmp);

	tmp = addr[5] << 8 | addr[4];
	(void)zyd_write32(sc, ZYD_MAC_BSSADRH, tmp);

	return 0;
}

Static int
zyd_switch_radio(struct zyd_softc *sc, int on)
{
	struct zyd_rf *rf = &sc->sc_rf;
	int error;

	zyd_lock_phy(sc);
	error = (*rf->switch_radio)(rf, on);
	zyd_unlock_phy(sc);

	return error;
}

Static void
zyd_set_led(struct zyd_softc *sc, int which, int on)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_TX_PE_CONTROL, &tmp);
	tmp &= ~which;
	if (on)
		tmp |= which;
	(void)zyd_write32(sc, ZYD_MAC_TX_PE_CONTROL, tmp);
}

Static int
zyd_set_rxfilter(struct zyd_softc *sc)
{
	uint32_t rxfilter;

	switch (sc->sc_ic.ic_opmode) {
	case IEEE80211_M_STA:
		rxfilter = ZYD_FILTER_BSS;
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_HOSTAP:
		rxfilter = ZYD_FILTER_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		rxfilter = ZYD_FILTER_MONITOR;
		break;
	default:
		/* should not get there */
		return EINVAL;
	}
	return zyd_write32(sc, ZYD_MAC_RXFILTER, rxfilter);
}

Static void
zyd_set_chan(struct zyd_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct zyd_rf *rf = &sc->sc_rf;
	u_int chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	zyd_lock_phy(sc);

	(*rf->set_channel)(rf, chan);

	/* update Tx power */
	(void)zyd_write32(sc, ZYD_CR31, sc->pwr_int[chan - 1]);
	(void)zyd_write32(sc, ZYD_CR68, sc->pwr_cal[chan - 1]);

	if (sc->mac_rev == ZYD_ZD1211B) {
		(void)zyd_write32(sc, ZYD_CR67, sc->ofdm36_cal[chan - 1]);
		(void)zyd_write32(sc, ZYD_CR66, sc->ofdm48_cal[chan - 1]);
		(void)zyd_write32(sc, ZYD_CR65, sc->ofdm54_cal[chan - 1]);

		(void)zyd_write32(sc, ZYD_CR69, 0x28);
		(void)zyd_write32(sc, ZYD_CR69, 0x2a);
	}

	zyd_unlock_phy(sc);
}

Static int
zyd_set_beacon_interval(struct zyd_softc *sc, int bintval)
{
	/* XXX this is probably broken.. */
	(void)zyd_write32(sc, ZYD_CR_ATIM_WND_PERIOD, bintval - 2);
	(void)zyd_write32(sc, ZYD_CR_PRE_TBTT,        bintval - 1);
	(void)zyd_write32(sc, ZYD_CR_BCN_INTERVAL,    bintval);

	return 0;
}

Static uint8_t
zyd_plcp_signal(int rate)
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

Static void
zyd_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_softc *sc = (struct zyd_softc *)priv;
	struct zyd_cmd *cmd;
	uint32_t datalen;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(
			    sc->zyd_ep[ZYD_ENDPT_IIN]);
		}
		return;
	}

	cmd = (struct zyd_cmd *)sc->ibuf;

	if (le16toh(cmd->code) == ZYD_NOTIF_RETRYSTATUS) {
		struct zyd_notif_retry *retry =
		    (struct zyd_notif_retry *)cmd->data;
		struct ieee80211com *ic = &sc->sc_ic;
		struct ifnet *ifp = &sc->sc_if;
		struct ieee80211_node *ni;

		DPRINTF(("retry intr: rate=0x%x addr=%s count=%d (0x%x)\n",
		    le16toh(retry->rate), ether_sprintf(retry->macaddr),
		    le16toh(retry->count) & 0xff, le16toh(retry->count)));

		/*
		 * Find the node to which the packet was sent and update its
		 * retry statistics.  In BSS mode, this node is the AP we're
		 * associated to so no lookup is actually needed.
		 */
		if (ic->ic_opmode != IEEE80211_M_STA) {
			ni = ieee80211_find_node(&ic->ic_scan, retry->macaddr);
			if (ni == NULL)
				return;	/* just ignore */
		} else
			ni = ic->ic_bss;

		((struct zyd_node *)ni)->amn.amn_retrycnt++;

		if (le16toh(retry->count) & 0x100)
			ifp->if_oerrors++;	/* too many retries */

	} else if (le16toh(cmd->code) == ZYD_NOTIF_IORD) {
		struct rq *rqp;

		if (le16toh(*(uint16_t *)cmd->data) == ZYD_CR_INTERRUPT)
			return;	/* HMAC interrupt */

		usbd_get_xfer_status(xfer, NULL, NULL, &datalen, NULL);
		datalen -= sizeof(cmd->code);
		datalen -= 2;	/* XXX: padding? */

		SIMPLEQ_FOREACH(rqp, &sc->sc_rqh, rq) {
			int i;

			if (sizeof(struct zyd_pair) * rqp->len != datalen)
				continue;
			for (i = 0; i < rqp->len; i++) {
				if (*(((const uint16_t *)rqp->idata) + i) !=
				    (((struct zyd_pair *)cmd->data) + i)->reg)
					break;
			}
			if (i != rqp->len)
				continue;

			/* copy answer into caller-supplied buffer */
			bcopy(cmd->data, rqp->odata,
			    sizeof(struct zyd_pair) * rqp->len);
			wakeup(rqp->odata);	/* wakeup caller */

			return;
		}
		return;	/* unexpected IORD notification */
	} else {
		printf("%s: unknown notification %x\n", device_xname(sc->sc_dev),
		    le16toh(cmd->code));
	}
}

Static void
zyd_rx_data(struct zyd_softc *sc, const uint8_t *buf, uint16_t len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	const struct zyd_plcphdr *plcp;
	const struct zyd_rx_stat *stat;
	struct mbuf *m;
	int rlen, s;

	if (len < ZYD_MIN_FRAGSZ) {
		printf("%s: frame too short (length=%d)\n",
		    device_xname(sc->sc_dev), len);
		ifp->if_ierrors++;
		return;
	}

	plcp = (const struct zyd_plcphdr *)buf;
	stat = (const struct zyd_rx_stat *)
	    (buf + len - sizeof (struct zyd_rx_stat));

	if (stat->flags & ZYD_RX_ERROR) {
		DPRINTF(("%s: RX status indicated error (%x)\n",
		    device_xname(sc->sc_dev), stat->flags));
		ifp->if_ierrors++;
		return;
	}

	/* compute actual frame length */
	rlen = len - sizeof (struct zyd_plcphdr) -
	    sizeof (struct zyd_rx_stat) - IEEE80211_CRC_LEN;

	/* allocate a mbuf to store the frame */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		printf("%s: could not allocate rx mbuf\n",
		    device_xname(sc->sc_dev));
		ifp->if_ierrors++;
		return;
	}
	if (rlen > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    device_xname(sc->sc_dev));
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = rlen;
	bcopy((const uint8_t *)(plcp + 1), mtod(m, uint8_t *), rlen);

	s = splnet();

	if (sc->sc_drvbpf != NULL) {
		struct zyd_rx_radiotap_header *tap = &sc->sc_rxtap;
		static const uint8_t rates[] = {
			/* reverse function of zyd_plcp_signal() */
			2, 4, 11, 22, 0, 0, 0, 0,
			96, 48, 24, 12, 108, 72, 36, 18
		};

		tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_rssi = stat->rssi;
		tap->wr_rate = rates[plcp->signal & 0xf];

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);
	ieee80211_input(ic, m, ni, stat->rssi, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);

	splx(s);
}

Static void
zyd_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_rx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ifnet *ifp = &sc->sc_if;
	const struct zyd_rx_desc *desc;
	int len;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->zyd_ep[ZYD_ENDPT_BIN]);

		goto skip;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (len < ZYD_MIN_RXBUFSZ) {
		printf("%s: xfer too short (length=%d)\n",
		    device_xname(sc->sc_dev), len);
		ifp->if_ierrors++;
		goto skip;
	}

	desc = (const struct zyd_rx_desc *)
	    (data->buf + len - sizeof (struct zyd_rx_desc));

	if (UGETW(desc->tag) == ZYD_TAG_MULTIFRAME) {
		const uint8_t *p = data->buf, *end = p + len;
		int i;

		DPRINTFN(3, ("received multi-frame transfer\n"));

		for (i = 0; i < ZYD_MAX_RXFRAMECNT; i++) {
			const uint16_t len16 = UGETW(desc->len[i]);

			if (len16 == 0 || p + len16 > end)
				break;

			zyd_rx_data(sc, p, len16);
			/* next frame is aligned on a 32-bit boundary */
			p += (len16 + 3) & ~3;
		}
	} else {
		DPRINTFN(3, ("received single-frame transfer\n"));

		zyd_rx_data(sc, data->buf, len);
	}

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data, NULL,
	    ZYX_MAX_RXBUFSZ, USBD_NO_COPY | USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, zyd_rxeof);
	(void)usbd_transfer(xfer);
}

Static int
zyd_tx_mgt(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct zyd_tx_desc *desc;
	struct zyd_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	int xferlen, totlen, rate;
	uint16_t pktlen;
	usbd_status error;

	data = &sc->tx_data[0];
	desc = (struct zyd_tx_desc *)data->buf;

	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? 12 : 2;

	wh = mtod(m0, struct ieee80211_frame *);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}
	}

	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	xferlen = sizeof (struct zyd_tx_desc) + m0->m_pkthdr.len;
	totlen = m0->m_pkthdr.len + IEEE80211_CRC_LEN;

	/* fill Tx descriptor */
	desc->len = htole16(totlen);

	desc->flags = ZYD_TX_FLAG_BACKOFF;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (totlen > ic->ic_rtsthreshold) {
			desc->flags |= ZYD_TX_FLAG_RTS;
		} else if (ZYD_RATE_IS_OFDM(rate) &&
		    (ic->ic_flags & IEEE80211_F_USEPROT)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				desc->flags |= ZYD_TX_FLAG_CTS_TO_SELF;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				desc->flags |= ZYD_TX_FLAG_RTS;
		}
	} else
		desc->flags |= ZYD_TX_FLAG_MULTICAST;

	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL))
		desc->flags |= ZYD_TX_FLAG_TYPE(ZYD_TX_TYPE_PS_POLL);

	desc->phy = zyd_plcp_signal(rate);
	if (ZYD_RATE_IS_OFDM(rate)) {
		desc->phy |= ZYD_TX_PHY_OFDM;
		if (ic->ic_curmode == IEEE80211_MODE_11A)
			desc->phy |= ZYD_TX_PHY_5GHZ;
	} else if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		desc->phy |= ZYD_TX_PHY_SHPREAMBLE;

	/* actual transmit length (XXX why +10?) */
	pktlen = sizeof (struct zyd_tx_desc) + 10;
	if (sc->mac_rev == ZYD_ZD1211)
		pktlen += totlen;
	desc->pktlen = htole16(pktlen);

	desc->plcp_length = (16 * totlen + rate - 1) / rate;
	desc->plcp_service = 0;
	if (rate == 22) {
		const int remainder = (16 * totlen) % 22;
		if (remainder != 0 && remainder < 7)
			desc->plcp_service |= ZYD_PLCP_LENGEXT;
	}

	if (sc->sc_drvbpf != NULL) {
		struct zyd_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len,
	    data->buf + sizeof (struct zyd_tx_desc));

	DPRINTFN(10, ("%s: sending mgt frame len=%zu rate=%u xferlen=%u\n",
	    device_xname(sc->sc_dev), (size_t)m0->m_pkthdr.len, rate, xferlen));

	m_freem(m0);	/* mbuf no longer needed */

	usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], data,
	    data->buf, xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    ZYD_TX_TIMEOUT, zyd_txeof);
	error = usbd_transfer(data->xfer);
	if (error != USBD_IN_PROGRESS && error != 0) {
		ifp->if_oerrors++;
		return EIO;
	}
	sc->tx_queued++;

	return 0;
}

Static void
zyd_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_tx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ifnet *ifp = &sc->sc_if;
	int s;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: could not transmit buffer: %s\n",
		    device_xname(sc->sc_dev), usbd_errstr(status));

		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(
			    sc->zyd_ep[ZYD_ENDPT_BOUT]);
		}
		ifp->if_oerrors++;
		return;
	}

	s = splnet();

	/* update rate control statistics */
	((struct zyd_node *)data->ni)->amn.amn_txcnt++;

	ieee80211_free_node(data->ni);
	data->ni = NULL;

	sc->tx_queued--;
	ifp->if_opackets++;

	sc->tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	zyd_start(ifp);

	splx(s);
}

Static int
zyd_tx_data(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct zyd_tx_desc *desc;
	struct zyd_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	int xferlen, totlen, rate;
	uint16_t pktlen;
	usbd_status error;

	wh = mtod(m0, struct ieee80211_frame *);

	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE)
		rate = ic->ic_bss->ni_rates.rs_rates[ic->ic_fixed_rate];
	else
		rate = ni->ni_rates.rs_rates[ni->ni_txrate];
	rate &= IEEE80211_RATE_VAL;

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	data = &sc->tx_data[0];
	desc = (struct zyd_tx_desc *)data->buf;

	data->ni = ni;

	xferlen = sizeof (struct zyd_tx_desc) + m0->m_pkthdr.len;
	totlen = m0->m_pkthdr.len + IEEE80211_CRC_LEN;

	/* fill Tx descriptor */
	desc->len = htole16(totlen);

	desc->flags = ZYD_TX_FLAG_BACKOFF;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (totlen > ic->ic_rtsthreshold) {
			desc->flags |= ZYD_TX_FLAG_RTS;
		} else if (ZYD_RATE_IS_OFDM(rate) &&
		    (ic->ic_flags & IEEE80211_F_USEPROT)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				desc->flags |= ZYD_TX_FLAG_CTS_TO_SELF;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				desc->flags |= ZYD_TX_FLAG_RTS;
		}
	} else
		desc->flags |= ZYD_TX_FLAG_MULTICAST;

	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL))
		desc->flags |= ZYD_TX_FLAG_TYPE(ZYD_TX_TYPE_PS_POLL);

	desc->phy = zyd_plcp_signal(rate);
	if (ZYD_RATE_IS_OFDM(rate)) {
		desc->phy |= ZYD_TX_PHY_OFDM;
		if (ic->ic_curmode == IEEE80211_MODE_11A)
			desc->phy |= ZYD_TX_PHY_5GHZ;
	} else if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		desc->phy |= ZYD_TX_PHY_SHPREAMBLE;

	/* actual transmit length (XXX why +10?) */
	pktlen = sizeof (struct zyd_tx_desc) + 10;
	if (sc->mac_rev == ZYD_ZD1211)
		pktlen += totlen;
	desc->pktlen = htole16(pktlen);

	desc->plcp_length = (16 * totlen + rate - 1) / rate;
	desc->plcp_service = 0;
	if (rate == 22) {
		const int remainder = (16 * totlen) % 22;
		if (remainder != 0 && remainder < 7)
			desc->plcp_service |= ZYD_PLCP_LENGEXT;
	}

	if (sc->sc_drvbpf != NULL) {
		struct zyd_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	m_copydata(m0, 0, m0->m_pkthdr.len,
	    data->buf + sizeof (struct zyd_tx_desc));

	DPRINTFN(10, ("%s: sending data frame len=%zu rate=%u xferlen=%u\n",
	    device_xname(sc->sc_dev), (size_t)m0->m_pkthdr.len, rate, xferlen));

	m_freem(m0);	/* mbuf no longer needed */

	usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], data,
	    data->buf, xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    ZYD_TX_TIMEOUT, zyd_txeof);
	error = usbd_transfer(data->xfer);
	if (error != USBD_IN_PROGRESS && error != 0) {
		ifp->if_oerrors++;
		return EIO;
	}
	sc->tx_queued++;

	return 0;
}

Static void
zyd_start(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m0;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
			bpf_mtap3(ic->ic_rawbpf, m0);
			if (zyd_tx_mgt(sc, m0, ni) != 0)
				break;
		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_POLL(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IFQ_DEQUEUE(&ifp->if_snd, m0);

			if (m0->m_len < sizeof(struct ether_header) &&
			    !(m0 = m_pullup(m0, sizeof(struct ether_header))))
				continue;

			eh = mtod(m0, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				m_freem(m0);
				continue;
			}
			bpf_mtap(ifp, m0);
			if ((m0 = ieee80211_encap(ic, m0, ni)) == NULL) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				continue;
			}
			bpf_mtap3(ic->ic_rawbpf, m0);
			if (zyd_tx_data(sc, m0, ni) != 0) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->tx_timer = 5;
		ifp->if_timer = 1;
	}
}

Static void
zyd_watchdog(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ifp->if_timer = 0;

	if (sc->tx_timer > 0) {
		if (--sc->tx_timer == 0) {
			printf("%s: device timeout\n", device_xname(sc->sc_dev));
			/* zyd_init(ifp); XXX needs a process context ? */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ic);
}

Static int
zyd_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		/* XXX re-use ether_ioctl() */
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_UP:
			zyd_init(ifp);
			break;
		case IFF_RUNNING:
			zyd_stop(ifp, 1);
			break;
		default:
			break;
		}
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) ==
		    (IFF_RUNNING | IFF_UP))
			zyd_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

Static int
zyd_init(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int i, error;

	zyd_stop(ifp, 0);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	DPRINTF(("setting MAC address to %s\n", ether_sprintf(ic->ic_myaddr)));
	error = zyd_set_macaddr(sc, ic->ic_myaddr);
	if (error != 0)
		return error;

	/* we'll do software WEP decryption for now */
	DPRINTF(("setting encryption type\n"));
	error = zyd_write32(sc, ZYD_MAC_ENCRYPTION_TYPE, ZYD_ENC_SNIFFER);
	if (error != 0)
		return error;

	/* promiscuous mode */
	(void)zyd_write32(sc, ZYD_MAC_SNIFFER,
	    (ic->ic_opmode == IEEE80211_M_MONITOR) ? 1 : 0);

	(void)zyd_set_rxfilter(sc);

	/* switch radio transmitter ON */
	(void)zyd_switch_radio(sc, 1);

	/* set basic rates */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x0003);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x1500);
	else	/* assumes 802.11b/g */
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x000f);

	/* set mandatory rates */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x000f);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x1500);
	else	/* assumes 802.11b/g */
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x150f);

	/* set default BSS channel */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	zyd_set_chan(sc, ic->ic_bss->ni_chan);

	/* enable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, ZYD_HWINT_MASK);

	/*
	 * Allocate Tx and Rx xfer queues.
	 */
	if ((error = zyd_alloc_tx_list(sc)) != 0) {
		printf("%s: could not allocate Tx list\n",
		    device_xname(sc->sc_dev));
		goto fail;
	}
	if ((error = zyd_alloc_rx_list(sc)) != 0) {
		printf("%s: could not allocate Rx list\n",
		    device_xname(sc->sc_dev));
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data,
		    NULL, ZYX_MAX_RXBUFSZ, USBD_NO_COPY | USBD_SHORT_XFER_OK,
		    USBD_NO_TIMEOUT, zyd_rxeof);
		error = usbd_transfer(data->xfer);
		if (error != USBD_IN_PROGRESS && error != 0) {
			printf("%s: could not queue Rx transfer\n",
			    device_xname(sc->sc_dev));
			goto fail;
		}
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return 0;

fail:	zyd_stop(ifp, 1);
	return error;
}

Static void
zyd_stop(struct ifnet *ifp, int disable)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);	/* free all nodes */

	sc->tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* switch radio transmitter OFF */
	(void)zyd_switch_radio(sc, 0);

	/* disable Rx */
	(void)zyd_write32(sc, ZYD_MAC_RXFILTER, 0);

	/* disable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, 0);

	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_BIN]);
	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_BOUT]);

	zyd_free_rx_list(sc);
	zyd_free_tx_list(sc);
}

Static int
zyd_loadfirmware(struct zyd_softc *sc, u_char *fw, size_t size)
{
	usb_device_request_t req;
	uint16_t addr;
	uint8_t stat;

	DPRINTF(("firmware size=%zu\n", size));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADREQ;
	USETW(req.wIndex, 0);

	addr = ZYD_FIRMWARE_START_ADDR;
	while (size > 0) {
#if 0
		const int mlen = min(size, 4096);
#else
		/*
		 * XXXX: When the transfer size is 4096 bytes, it is not
		 * likely to be able to transfer it.
		 * The cause is port or machine or chip?
		 */
		const int mlen = min(size, 64);
#endif

		DPRINTF(("loading firmware block: len=%d, addr=0x%x\n", mlen,
		    addr));

		USETW(req.wValue, addr);
		USETW(req.wLength, mlen);
		if (usbd_do_request(sc->sc_udev, &req, fw) != 0)
			return EIO;

		addr += mlen / 2;
		fw   += mlen;
		size -= mlen;
	}

	/* check whether the upload succeeded */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADSTS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof stat);
	if (usbd_do_request(sc->sc_udev, &req, &stat) != 0)
		return EIO;

	return (stat & 0x80) ? EIO : 0;
}

Static void
zyd_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct zyd_softc *sc = arg;
	struct zyd_node *zn = (struct zyd_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &zn->amn);
}

Static void
zyd_amrr_timeout(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	if (ic->ic_opmode == IEEE80211_M_STA)
		zyd_iter_func(sc, ic->ic_bss);
	else
		ieee80211_iterate_nodes(&ic->ic_sta, zyd_iter_func, sc);
	splx(s);

	callout_reset(&sc->sc_amrr_ch, hz, zyd_amrr_timeout, sc);
}

Static void
zyd_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct zyd_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int i;

	ieee80211_amrr_node_init(&sc->amrr, &((struct zyd_node *)ni)->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;
}

int
zyd_activate(device_t self, enum devact act)
{
	struct zyd_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}
