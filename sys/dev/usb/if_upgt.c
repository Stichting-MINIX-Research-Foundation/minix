/*	$NetBSD: if_upgt.c,v 1.12 2014/05/27 13:44:25 ryoon Exp $	*/
/*	$OpenBSD: if_upgt.c,v 1.49 2010/04/20 22:05:43 tedu Exp $ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
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
__KERNEL_RCSID(0, "$NetBSD: if_upgt.c,v 1.12 2014/05/27 13:44:25 ryoon Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/intr.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_upgtvar.h>

/*
 * Driver for the USB PrismGT devices.
 *
 * For now just USB 2.0 devices with the GW3887 chipset are supported.
 * The driver has been written based on the firmware version 2.13.1.0_LM87.
 *
 * TODO's:
 * - Fix MONITOR mode (MAC filter).
 * - Add HOSTAP mode.
 * - Add IBSS mode.
 * - Support the USB 1.0 devices (NET2280, ISL3880, ISL3886 chipsets).
 *
 * Parts of this driver has been influenced by reading the p54u driver
 * written by Jean-Baptiste Note <jean-baptiste.note@m4x.org> and
 * Sebastien Bourdeauducq <lekernel@prism54.org>.
 */

#ifdef UPGT_DEBUG
int upgt_debug = 2;
#define DPRINTF(l, x...) do { if ((l) <= upgt_debug) printf(x); } while (0)
#else
#define DPRINTF(l, x...)
#endif

/*
 * Prototypes.
 */
static int	upgt_match(device_t, cfdata_t, void *);
static void	upgt_attach(device_t, device_t, void *);
static int	upgt_detach(device_t, int);
static int	upgt_activate(device_t, devact_t);

static void	upgt_attach_hook(device_t);
static int	upgt_device_type(struct upgt_softc *, uint16_t, uint16_t);
static int	upgt_device_init(struct upgt_softc *);
static int	upgt_mem_init(struct upgt_softc *);
static uint32_t	upgt_mem_alloc(struct upgt_softc *);
static void	upgt_mem_free(struct upgt_softc *, uint32_t);
static int	upgt_fw_alloc(struct upgt_softc *);
static void	upgt_fw_free(struct upgt_softc *);
static int	upgt_fw_verify(struct upgt_softc *);
static int	upgt_fw_load(struct upgt_softc *);
static int	upgt_fw_copy(char *, char *, int);
static int	upgt_eeprom_read(struct upgt_softc *);
static int	upgt_eeprom_parse(struct upgt_softc *);
static void	upgt_eeprom_parse_hwrx(struct upgt_softc *, uint8_t *);
static void	upgt_eeprom_parse_freq3(struct upgt_softc *, uint8_t *, int);
static void	upgt_eeprom_parse_freq4(struct upgt_softc *, uint8_t *, int);
static void	upgt_eeprom_parse_freq6(struct upgt_softc *, uint8_t *, int);

static int	upgt_ioctl(struct ifnet *, u_long, void *);
static int	upgt_init(struct ifnet *);
static void	upgt_stop(struct upgt_softc *);
static int	upgt_media_change(struct ifnet *);
static void	upgt_newassoc(struct ieee80211_node *, int);
static int	upgt_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
static void	upgt_newstate_task(void *);
static void	upgt_next_scan(void *);
static void	upgt_start(struct ifnet *);
static void	upgt_watchdog(struct ifnet *);
static void	upgt_tx_task(void *);
static void	upgt_tx_done(struct upgt_softc *, uint8_t *);
static void	upgt_rx_cb(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void	upgt_rx(struct upgt_softc *, uint8_t *, int);
static void	upgt_setup_rates(struct upgt_softc *);
static uint8_t	upgt_rx_rate(struct upgt_softc *, const int);
static int	upgt_set_macfilter(struct upgt_softc *, uint8_t state);
static int	upgt_set_channel(struct upgt_softc *, unsigned);
static void	upgt_set_led(struct upgt_softc *, int);
static void	upgt_set_led_blink(void *);
static int	upgt_get_stats(struct upgt_softc *);

static int	upgt_alloc_tx(struct upgt_softc *);
static int	upgt_alloc_rx(struct upgt_softc *);
static int	upgt_alloc_cmd(struct upgt_softc *);
static void	upgt_free_tx(struct upgt_softc *);
static void	upgt_free_rx(struct upgt_softc *);
static void	upgt_free_cmd(struct upgt_softc *);
static int	upgt_bulk_xmit(struct upgt_softc *, struct upgt_data *,
		    usbd_pipe_handle, uint32_t *, int);

#if 0
static void	upgt_hexdump(void *, int);
#endif
static uint32_t	upgt_crc32_le(const void *, size_t);
static uint32_t	upgt_chksum_le(const uint32_t *, size_t);

CFATTACH_DECL_NEW(upgt, sizeof(struct upgt_softc),
	upgt_match, upgt_attach, upgt_detach, upgt_activate);

static const struct usb_devno upgt_devs_1[] = {
	/* version 1 devices */
	{ USB_VENDOR_ALCATELT,		USB_PRODUCT_ALCATELT_ST120G },
	{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2862WG_V1 }
};

static const struct usb_devno upgt_devs_2[] = {
	/* version 2 devices */
	{ USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_PRISM_GT },
	{ USB_VENDOR_ALCATELT,		USB_PRODUCT_ALCATELT_ST121G },
	{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D7050 },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_WUSB54AG },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_WUSB54GV2 },
	{ USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_PRISM_GT },
	{ USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_CGWLUSB2GTST },
	{ USB_VENDOR_DELL,		USB_PRODUCT_DELL_PRISM_GT_1 },
	{ USB_VENDOR_DELL,		USB_PRODUCT_DELL_PRISM_GT_2 },
	{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DWLG122A2 },
	{ USB_VENDOR_FSC,		USB_PRODUCT_FSC_E5400 },
	{ USB_VENDOR_GLOBESPAN,		USB_PRODUCT_GLOBESPAN_PRISM_GT_1 },
	{ USB_VENDOR_GLOBESPAN,		USB_PRODUCT_GLOBESPAN_PRISM_GT_2 },
	{ USB_VENDOR_INTERSIL,		USB_PRODUCT_INTERSIL_PRISM_GT },
	{ USB_VENDOR_PHEENET,		USB_PRODUCT_PHEENET_GWU513 },
	{ USB_VENDOR_PHILIPS,		USB_PRODUCT_PHILIPS_CPWUA054 },
	{ USB_VENDOR_SHARP,		USB_PRODUCT_SHARP_RUITZ1016YCZZ },
	{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2862WG },
	{ USB_VENDOR_USR,		USB_PRODUCT_USR_USR5422 },
	{ USB_VENDOR_WISTRONNEWEB,	USB_PRODUCT_WISTRONNEWEB_UR045G },
	{ USB_VENDOR_CONEXANT,		USB_PRODUCT_CONEXANT_PRISM_GT_1 },
	{ USB_VENDOR_CONEXANT,		USB_PRODUCT_CONEXANT_PRISM_GT_2 },
	{ USB_VENDOR_ZCOM,		USB_PRODUCT_ZCOM_MD40900 },
	{ USB_VENDOR_ZCOM,		USB_PRODUCT_ZCOM_XG703A }
};

static int
firmware_load(const char *dname, const char *iname, uint8_t **ucodep,
    size_t *sizep)
{
	firmware_handle_t fh;
	int error;

	if ((error = firmware_open(dname, iname, &fh)) != 0)
		return error;
	*sizep = firmware_get_size(fh);
	if ((*ucodep = firmware_malloc(*sizep)) == NULL) {
		firmware_close(fh);
		return ENOMEM;
	}
	if ((error = firmware_read(fh, 0, *ucodep, *sizep)) != 0)
		firmware_free(*ucodep, *sizep);
	firmware_close(fh);

	return error;
}

static int
upgt_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (usb_lookup(upgt_devs_1, uaa->vendor, uaa->product) != NULL)
		return UMATCH_VENDOR_PRODUCT;

	if (usb_lookup(upgt_devs_2, uaa->vendor, uaa->product) != NULL)
		return UMATCH_VENDOR_PRODUCT;

	return UMATCH_NONE;
}

static void
upgt_attach(device_t parent, device_t self, void *aux)
{
	struct upgt_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	char *devinfop;
	int i;

	aprint_naive("\n");
	aprint_normal("\n");

	/*
	 * Attach USB device.
	 */
	sc->sc_dev = self;
	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(sc->sc_dev, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	/* check device type */
	if (upgt_device_type(sc, uaa->vendor, uaa->product) != 0)
		return;

	/* set configuration number */
	error = usbd_set_config_no(sc->sc_udev, UPGT_CONFIG_NO, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(error));
		return;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, UPGT_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not get interface handle\n");
		return;
	}

	/* find endpoints */
	id = usbd_get_interface_descriptor(sc->sc_iface);
	sc->sc_rx_no = sc->sc_tx_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "no endpoint descriptor for iface %d\n", i);
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_tx_no = ed->bEndpointAddress;
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_rx_no = ed->bEndpointAddress;

		/*
		 * 0x01 TX pipe
		 * 0x81 RX pipe
		 *
		 * Deprecated scheme (not used with fw version >2.5.6.x):
		 * 0x02 TX MGMT pipe
		 * 0x82 TX MGMT pipe
		 */
		if (sc->sc_tx_no != -1 && sc->sc_rx_no != -1)
			break;
	}
	if (sc->sc_rx_no == -1 || sc->sc_tx_no == -1) {
		aprint_error_dev(sc->sc_dev, "missing endpoint\n");
		return;
	}

	/* setup tasks and timeouts */
	usb_init_task(&sc->sc_task_newstate, upgt_newstate_task, sc, 0);
	usb_init_task(&sc->sc_task_tx, upgt_tx_task, sc, 0);
	callout_init(&sc->scan_to, 0);
	callout_setfunc(&sc->scan_to, upgt_next_scan, sc);
	callout_init(&sc->led_to, 0);
	callout_setfunc(&sc->led_to, upgt_set_led_blink, sc);

	/*
	 * Open TX and RX USB bulk pipes.
	 */
	error = usbd_open_pipe(sc->sc_iface, sc->sc_tx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_tx_pipeh);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not open TX pipe: %s\n", usbd_errstr(error));
		goto fail;
	}
	error = usbd_open_pipe(sc->sc_iface, sc->sc_rx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_rx_pipeh);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not open RX pipe: %s\n",
		    usbd_errstr(error));
		goto fail;
	}

	/*
	 * Allocate TX, RX, and CMD xfers.
	 */
	if (upgt_alloc_tx(sc) != 0)
		goto fail;
	if (upgt_alloc_rx(sc) != 0)
		goto fail;
	if (upgt_alloc_cmd(sc) != 0)
		goto fail;

	/*
	 * We need the firmware loaded from file system to complete the attach.
	 */
	config_mountroot(self, upgt_attach_hook);

	return;
fail:
	aprint_error_dev(sc->sc_dev, "%s failed\n", __func__);
}

static void
upgt_attach_hook(device_t arg)
{
	struct upgt_softc *sc = device_private(arg);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	usbd_status error;
	int i;

	/*
	 * Load firmware file into memory.
	 */
	if (upgt_fw_alloc(sc) != 0)
		goto fail;

	/*
	 * Initialize the device.
	 */
	if (upgt_device_init(sc) != 0)
		goto fail;

	/*
	 * Verify the firmware.
	 */
	if (upgt_fw_verify(sc) != 0)
		goto fail;

	/*
	 * Calculate device memory space.
	 */
	if (sc->sc_memaddr_frame_start == 0 || sc->sc_memaddr_frame_end == 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not find memory space addresses on FW\n");
		goto fail;
	}
	sc->sc_memaddr_frame_end -= UPGT_MEMSIZE_RX + 1;
	sc->sc_memaddr_rx_start = sc->sc_memaddr_frame_end + 1;

	DPRINTF(1, "%s: memory address frame start=0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_memaddr_frame_start);
	DPRINTF(1, "%s: memory address frame end=0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_memaddr_frame_end);
	DPRINTF(1, "%s: memory address rx start=0x%08x\n",
	    device_xname(sc->sc_dev), sc->sc_memaddr_rx_start);

	upgt_mem_init(sc);

	/*
	 * Load the firmware.
	 */
	if (upgt_fw_load(sc) != 0)
		goto fail;

	/*
	 * Startup the RX pipe.
	 */
	struct upgt_data *data_rx = &sc->rx_data;

	usbd_setup_xfer(data_rx->xfer, sc->sc_rx_pipeh, data_rx, data_rx->buf,
	    MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, upgt_rx_cb);
	error = usbd_transfer(data_rx->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		aprint_error_dev(sc->sc_dev,
		    "could not queue RX transfer\n");
		goto fail;
	}
	usbd_delay_ms(sc->sc_udev, 100);

	/*
	 * Read the whole EEPROM content and parse it.
	 */
	if (upgt_eeprom_read(sc) != 0)
		goto fail;
	if (upgt_eeprom_parse(sc) != 0)
		goto fail;

	/*
	 * Setup the 802.11 device.
	 */
	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;
	ic->ic_caps =
	    IEEE80211_C_MONITOR |
	    IEEE80211_C_SHPREAMBLE |
	    IEEE80211_C_SHSLOT;

	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = upgt_init;
	ifp->if_ioctl = upgt_ioctl;
	ifp->if_start = upgt_start;
	ifp->if_watchdog = upgt_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ic);
	ic->ic_newassoc = upgt_newassoc;

	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = upgt_newstate;
	ieee80211_media_init(ic, upgt_media_change, ieee80211_media_status);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN,
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(UPGT_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(UPGT_TX_RADIOTAP_PRESENT);

	aprint_normal_dev(sc->sc_dev, "address %s\n",
	    ether_sprintf(ic->ic_myaddr));

	ieee80211_announce(ic);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);

	/* device attached */
	sc->sc_flags |= UPGT_DEVICE_ATTACHED;

	return;
fail:
	aprint_error_dev(sc->sc_dev, "%s failed\n", __func__);
}

static int
upgt_detach(device_t self, int flags)
{
	struct upgt_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	DPRINTF(1, "%s: %s\n", device_xname(sc->sc_dev), __func__);

	s = splnet();

	if (ifp->if_flags & IFF_RUNNING)
		upgt_stop(sc);

	/* remove tasks and timeouts */
	usb_rem_task(sc->sc_udev, &sc->sc_task_newstate);
	usb_rem_task(sc->sc_udev, &sc->sc_task_tx);
	callout_destroy(&sc->scan_to);
	callout_destroy(&sc->led_to);

	/* abort and close TX / RX pipes */
	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
	}
	if (sc->sc_rx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_rx_pipeh);
		usbd_close_pipe(sc->sc_rx_pipeh);
	}

	/* free xfers */
	upgt_free_tx(sc);
	upgt_free_rx(sc);
	upgt_free_cmd(sc);

	/* free firmware */
	upgt_fw_free(sc);

	if (sc->sc_flags & UPGT_DEVICE_ATTACHED) {
		/* detach interface */
		bpf_detach(ifp);
		ieee80211_ifdetach(ic);
		if_detach(ifp);
	}

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	return 0;
}

static int
upgt_activate(device_t self, devact_t act)
{
	struct upgt_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static int
upgt_device_type(struct upgt_softc *sc, uint16_t vendor, uint16_t product)
{

	if (usb_lookup(upgt_devs_1, vendor, product) != NULL) {
		sc->sc_device_type = 1;
		/* XXX */
		aprint_error_dev(sc->sc_dev,
		    "version 1 devices not supported yet\n");
		return 1;
	} else
		sc->sc_device_type = 2;

	return 0;
}

static int
upgt_device_init(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	const uint8_t init_cmd[] = { 0x7e, 0x7e, 0x7e, 0x7e };
	int len;

	len = sizeof(init_cmd);
	memcpy(data_cmd->buf, init_cmd, len);
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not send device init string\n");
		return EIO;
	}
	usbd_delay_ms(sc->sc_udev, 100);

	DPRINTF(1, "%s: device initialized\n", device_xname(sc->sc_dev));

	return 0;
}

static int
upgt_mem_init(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < UPGT_MEMORY_MAX_PAGES; i++) {
		sc->sc_memory.page[i].used = 0;

		if (i == 0) {
			/*
			 * The first memory page is always reserved for
			 * command data.
			 */
			sc->sc_memory.page[i].addr =
			    sc->sc_memaddr_frame_start + MCLBYTES;
		} else {
			sc->sc_memory.page[i].addr =
			    sc->sc_memory.page[i - 1].addr + MCLBYTES;
		}

		if (sc->sc_memory.page[i].addr + MCLBYTES >=
		    sc->sc_memaddr_frame_end)
			break;

		DPRINTF(2, "%s: memory address page %d=0x%08x\n",
		    device_xname(sc->sc_dev), i, sc->sc_memory.page[i].addr);
	}

	sc->sc_memory.pages = i;

	DPRINTF(2, "%s: memory pages=%d\n",
	    device_xname(sc->sc_dev), sc->sc_memory.pages);

	return 0;
}

static uint32_t
upgt_mem_alloc(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < sc->sc_memory.pages; i++) {
		if (sc->sc_memory.page[i].used == 0) {
			sc->sc_memory.page[i].used = 1;
			return sc->sc_memory.page[i].addr;
		}
	}

	return 0;
}

static void
upgt_mem_free(struct upgt_softc *sc, uint32_t addr)
{
	int i;

	for (i = 0; i < sc->sc_memory.pages; i++) {
		if (sc->sc_memory.page[i].addr == addr) {
			sc->sc_memory.page[i].used = 0;
			return;
		}
	}

	aprint_error_dev(sc->sc_dev, "could not free memory address 0x%08x\n",
	    addr);
}


static int
upgt_fw_alloc(struct upgt_softc *sc)
{
	const char *name = "upgt-gw3887";
	int error;

	if (sc->sc_fw == NULL) {
		error = firmware_load("upgt", name, &sc->sc_fw,
		    &sc->sc_fw_size);
		if (error != 0) {
			if (error == ENOENT) {
				/*
				 * The firmware file for upgt(4) is not in
				 * the default distribution due to its lisence
				 * so explicitly notify it if the firmware file
				 * is not found.
				 */
				aprint_error_dev(sc->sc_dev,
				    "firmware file %s is not installed\n",
				    name);
				aprint_error_dev(sc->sc_dev,
				    "(it is not included in the default"
				    " distribution)\n");
				aprint_error_dev(sc->sc_dev,
				    "see upgt(4) man page for details about "
				    "firmware installation\n");
			} else {
				aprint_error_dev(sc->sc_dev,
				    "could not read firmware %s\n", name);
			}
			return EIO;
		}
	}

	DPRINTF(1, "%s: firmware %s allocated\n", device_xname(sc->sc_dev),
	    name);

	return 0;
}

static void
upgt_fw_free(struct upgt_softc *sc)
{

	if (sc->sc_fw != NULL) {
		firmware_free(sc->sc_fw, sc->sc_fw_size);
		sc->sc_fw = NULL;
		DPRINTF(1, "%s: firmware freed\n", device_xname(sc->sc_dev));
	}
}

static int
upgt_fw_verify(struct upgt_softc *sc)
{
	struct upgt_fw_bra_option *bra_option;
	uint32_t bra_option_type, bra_option_len;
	uint32_t *uc;
	int offset, bra_end = 0;

	/*
	 * Seek to beginning of Boot Record Area (BRA).
	 */
	for (offset = 0; offset < sc->sc_fw_size; offset += sizeof(*uc)) {
		uc = (uint32_t *)(sc->sc_fw + offset);
		if (*uc == 0)
			break;
	}
	for (; offset < sc->sc_fw_size; offset += sizeof(*uc)) {
		uc = (uint32_t *)(sc->sc_fw + offset);
		if (*uc != 0)
			break;
	}
	if (offset == sc->sc_fw_size) {
		aprint_error_dev(sc->sc_dev,
		    "firmware Boot Record Area not found\n");
		return EIO;
	}
	DPRINTF(1, "%s: firmware Boot Record Area found at offset %d\n",
	    device_xname(sc->sc_dev), offset);

	/*
	 * Parse Boot Record Area (BRA) options.
	 */
	while (offset < sc->sc_fw_size && bra_end == 0) {
		/* get current BRA option */
		bra_option = (struct upgt_fw_bra_option *)(sc->sc_fw + offset);
		bra_option_type = le32toh(bra_option->type);
		bra_option_len = le32toh(bra_option->len) * sizeof(*uc);

		switch (bra_option_type) {
		case UPGT_BRA_TYPE_FW:
			DPRINTF(1, "%s: UPGT_BRA_TYPE_FW len=%d\n",
			    device_xname(sc->sc_dev), bra_option_len);

			if (bra_option_len != UPGT_BRA_FWTYPE_SIZE) {
				aprint_error_dev(sc->sc_dev,
				    "wrong UPGT_BRA_TYPE_FW len\n");
				return EIO;
			}
			if (memcmp(UPGT_BRA_FWTYPE_LM86, bra_option->data,
			    bra_option_len) == 0) {
				sc->sc_fw_type = UPGT_FWTYPE_LM86;
				break;
			}
			if (memcmp(UPGT_BRA_FWTYPE_LM87, bra_option->data,
			    bra_option_len) == 0) {
				sc->sc_fw_type = UPGT_FWTYPE_LM87;
				break;
			}
			if (memcmp(UPGT_BRA_FWTYPE_FMAC, bra_option->data,
			    bra_option_len) == 0) {
				sc->sc_fw_type = UPGT_FWTYPE_FMAC;
				break;
			}
			aprint_error_dev(sc->sc_dev,
			    "unsupported firmware type\n");
			return EIO;
		case UPGT_BRA_TYPE_VERSION:
			DPRINTF(1, "%s: UPGT_BRA_TYPE_VERSION len=%d\n",
			    device_xname(sc->sc_dev), bra_option_len);
			break;
		case UPGT_BRA_TYPE_DEPIF:
			DPRINTF(1, "%s: UPGT_BRA_TYPE_DEPIF len=%d\n",
			    device_xname(sc->sc_dev), bra_option_len);
			break;
		case UPGT_BRA_TYPE_EXPIF:
			DPRINTF(1, "%s: UPGT_BRA_TYPE_EXPIF len=%d\n",
			    device_xname(sc->sc_dev), bra_option_len);
			break;
		case UPGT_BRA_TYPE_DESCR:
			DPRINTF(1, "%s: UPGT_BRA_TYPE_DESCR len=%d\n",
			    device_xname(sc->sc_dev), bra_option_len);

			struct upgt_fw_bra_descr *descr =
				(struct upgt_fw_bra_descr *)bra_option->data;

			sc->sc_memaddr_frame_start =
			    le32toh(descr->memaddr_space_start);
			sc->sc_memaddr_frame_end =
			    le32toh(descr->memaddr_space_end);

			DPRINTF(2, "%s: memory address space start=0x%08x\n",
			    device_xname(sc->sc_dev),
			    sc->sc_memaddr_frame_start);
			DPRINTF(2, "%s: memory address space end=0x%08x\n",
			    device_xname(sc->sc_dev),
			    sc->sc_memaddr_frame_end);
			break;
		case UPGT_BRA_TYPE_END:
			DPRINTF(1, "%s: UPGT_BRA_TYPE_END len=%d\n",
			    device_xname(sc->sc_dev), bra_option_len);
			bra_end = 1;
			break;
		default:
			DPRINTF(1, "%s: unknown BRA option len=%d\n",
			    device_xname(sc->sc_dev), bra_option_len);
			return EIO;
		}

		/* jump to next BRA option */
		offset += sizeof(struct upgt_fw_bra_option) + bra_option_len;
	}

	DPRINTF(1, "%s: firmware verified\n", device_xname(sc->sc_dev));

	return 0;
}

static int
upgt_fw_load(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_data *data_rx = &sc->rx_data;
	struct upgt_fw_x2_header *x2;
	const uint8_t start_fwload_cmd[] = { 0x3c, 0x0d };
	int offset, bsize, n, i, len;
	uint32_t crc;

	/* send firmware start load command */
	len = sizeof(start_fwload_cmd);
	memcpy(data_cmd->buf, start_fwload_cmd, len);
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not send start_firmware_load command\n");
		return EIO;
	}

	/* send X2 header */
	len = sizeof(struct upgt_fw_x2_header);
	x2 = (struct upgt_fw_x2_header *)data_cmd->buf;
	memcpy(x2->signature, UPGT_X2_SIGNATURE, UPGT_X2_SIGNATURE_SIZE);
	x2->startaddr = htole32(UPGT_MEMADDR_FIRMWARE_START);
	x2->len = htole32(sc->sc_fw_size);
	x2->crc = upgt_crc32_le(data_cmd->buf + UPGT_X2_SIGNATURE_SIZE,
	    sizeof(struct upgt_fw_x2_header) - UPGT_X2_SIGNATURE_SIZE -
	    sizeof(uint32_t));
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not send firmware X2 header\n");
		return EIO;
	}

	/* download firmware */
	for (offset = 0; offset < sc->sc_fw_size; offset += bsize) {
		if (sc->sc_fw_size - offset > UPGT_FW_BLOCK_SIZE)
			bsize = UPGT_FW_BLOCK_SIZE;
		else
			bsize = sc->sc_fw_size - offset;

		n = upgt_fw_copy(sc->sc_fw + offset, data_cmd->buf, bsize);

		DPRINTF(1, "%s: FW offset=%d, read=%d, sent=%d\n",
		    device_xname(sc->sc_dev), offset, n, bsize);

		if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &bsize, 0)
		    != 0) {
			aprint_error_dev(sc->sc_dev,
			    "error while downloading firmware block\n");
			return EIO;
		}

		bsize = n;
	}
	DPRINTF(1, "%s: firmware downloaded\n", device_xname(sc->sc_dev));

	/* load firmware */
	crc = upgt_crc32_le(sc->sc_fw, sc->sc_fw_size);
	*((uint32_t *)(data_cmd->buf)    ) = crc;
	*((uint8_t  *)(data_cmd->buf) + 4) = 'g';
	*((uint8_t  *)(data_cmd->buf) + 5) = '\r';
	len = 6;
	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not send load_firmware command\n");
		return EIO;
	}

	for (i = 0; i < UPGT_FIRMWARE_TIMEOUT; i++) {
		len = UPGT_FW_BLOCK_SIZE;
		memset(data_rx->buf, 0, 2);
		if (upgt_bulk_xmit(sc, data_rx, sc->sc_rx_pipeh, &len,
		    USBD_SHORT_XFER_OK) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not read firmware response\n");
			return EIO;
		}

		if (memcmp(data_rx->buf, "OK", 2) == 0)
			break;	/* firmware load was successful */
	}
	if (i == UPGT_FIRMWARE_TIMEOUT) {
		aprint_error_dev(sc->sc_dev, "firmware load failed\n");
		return EIO;
	}
	DPRINTF(1, "%s: firmware loaded\n", device_xname(sc->sc_dev));

	return 0;
}

/*
 * While copying the version 2 firmware, we need to replace two characters:
 *
 * 0x7e -> 0x7d 0x5e
 * 0x7d -> 0x7d 0x5d
 */
static int
upgt_fw_copy(char *src, char *dst, int size)
{
	int i, j;

	for (i = 0, j = 0; i < size && j < size; i++) {
		switch (src[i]) {
		case 0x7e:
			dst[j] = 0x7d;
			j++;
			dst[j] = 0x5e;
			j++;
			break;
		case 0x7d:
			dst[j] = 0x7d;
			j++;
			dst[j] = 0x5d;
			j++;
			break;
		default:
			dst[j] = src[i];
			j++;
			break;
		}
	}

	return i;
}

static int
upgt_eeprom_read(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_eeprom	*eeprom;
	int offset, block, len;

	offset = 0;
	block = UPGT_EEPROM_BLOCK_SIZE;
	while (offset < UPGT_EEPROM_SIZE) {
		DPRINTF(1, "%s: request EEPROM block (offset=%d, len=%d)\n",
		    device_xname(sc->sc_dev), offset, block);

		/*
		 * Transmit the URB containing the CMD data.
		 */
		len = sizeof(*mem) + sizeof(*eeprom) + block;

		memset(data_cmd->buf, 0, len);

		mem = (struct upgt_lmac_mem *)data_cmd->buf;
		mem->addr = htole32(sc->sc_memaddr_frame_start +
		    UPGT_MEMSIZE_FRAME_HEAD);

		eeprom = (struct upgt_lmac_eeprom *)(mem + 1);
		eeprom->header1.flags = 0;
		eeprom->header1.type = UPGT_H1_TYPE_CTRL;
		eeprom->header1.len = htole16((
		    sizeof(struct upgt_lmac_eeprom) -
		    sizeof(struct upgt_lmac_header)) + block);

		eeprom->header2.reqid = htole32(sc->sc_memaddr_frame_start);
		eeprom->header2.type = htole16(UPGT_H2_TYPE_EEPROM);
		eeprom->header2.flags = 0;

		eeprom->offset = htole16(offset);
		eeprom->len = htole16(block);

		mem->chksum = upgt_chksum_le((uint32_t *)eeprom,
		    len - sizeof(*mem));

		if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len,
		    USBD_FORCE_SHORT_XFER) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not transmit EEPROM data URB\n");
			return EIO;
		}
		if (tsleep(sc, 0, "eeprom_request", UPGT_USB_TIMEOUT)) {
			aprint_error_dev(sc->sc_dev,
			    "timeout while waiting for EEPROM data\n");
			return EIO;
		}

		offset += block;
		if (UPGT_EEPROM_SIZE - offset < block)
			block = UPGT_EEPROM_SIZE - offset;
	}

	return 0;
}

static int
upgt_eeprom_parse(struct upgt_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct upgt_eeprom_header *eeprom_header;
	struct upgt_eeprom_option *eeprom_option;
	uint16_t option_len;
	uint16_t option_type;
	uint16_t preamble_len;
	int option_end = 0;

	/* calculate eeprom options start offset */
	eeprom_header = (struct upgt_eeprom_header *)sc->sc_eeprom;
	preamble_len = le16toh(eeprom_header->preamble_len);
	eeprom_option = (struct upgt_eeprom_option *)(sc->sc_eeprom +
	    (sizeof(struct upgt_eeprom_header) + preamble_len));

	while (!option_end) {
		/* the eeprom option length is stored in words */
		option_len =
		    (le16toh(eeprom_option->len) - 1) * sizeof(uint16_t);
		option_type =
		    le16toh(eeprom_option->type);

		switch (option_type) {
		case UPGT_EEPROM_TYPE_NAME:
			DPRINTF(1, "%s: EEPROM name len=%d\n",
			    device_xname(sc->sc_dev), option_len);
			break;
		case UPGT_EEPROM_TYPE_SERIAL:
			DPRINTF(1, "%s: EEPROM serial len=%d\n",
			    device_xname(sc->sc_dev), option_len);
			break;
		case UPGT_EEPROM_TYPE_MAC:
			DPRINTF(1, "%s: EEPROM mac len=%d\n",
			    device_xname(sc->sc_dev), option_len);

			IEEE80211_ADDR_COPY(ic->ic_myaddr, eeprom_option->data);
			break;
		case UPGT_EEPROM_TYPE_HWRX:
			DPRINTF(1, "%s: EEPROM hwrx len=%d\n",
			    device_xname(sc->sc_dev), option_len);

			upgt_eeprom_parse_hwrx(sc, eeprom_option->data);
			break;
		case UPGT_EEPROM_TYPE_CHIP:
			DPRINTF(1, "%s: EEPROM chip len=%d\n",
			    device_xname(sc->sc_dev), option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ3:
			DPRINTF(1, "%s: EEPROM freq3 len=%d\n",
			    device_xname(sc->sc_dev), option_len);

			upgt_eeprom_parse_freq3(sc, eeprom_option->data,
			    option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ4:
			DPRINTF(1, "%s: EEPROM freq4 len=%d\n",
			    device_xname(sc->sc_dev), option_len);

			upgt_eeprom_parse_freq4(sc, eeprom_option->data,
			    option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ5:
			DPRINTF(1, "%s: EEPROM freq5 len=%d\n",
			    device_xname(sc->sc_dev), option_len);
			break;
		case UPGT_EEPROM_TYPE_FREQ6:
			DPRINTF(1, "%s: EEPROM freq6 len=%d\n",
			    device_xname(sc->sc_dev), option_len);

			upgt_eeprom_parse_freq6(sc, eeprom_option->data,
			    option_len);
			break;
		case UPGT_EEPROM_TYPE_END:
			DPRINTF(1, "%s: EEPROM end len=%d\n",
			    device_xname(sc->sc_dev), option_len);
			option_end = 1;
			break;
		case UPGT_EEPROM_TYPE_OFF:
			DPRINTF(1, "%s: EEPROM off without end option\n",
			    device_xname(sc->sc_dev));
			return EIO;
		default:
			DPRINTF(1, "%s: EEPROM unknown type 0x%04x len=%d\n",
			    device_xname(sc->sc_dev), option_type, option_len);
			break;
		}

		/* jump to next EEPROM option */
		eeprom_option = (struct upgt_eeprom_option *)
		    (eeprom_option->data + option_len);
	}

	return 0;
}

static void
upgt_eeprom_parse_hwrx(struct upgt_softc *sc, uint8_t *data)
{
	struct upgt_eeprom_option_hwrx *option_hwrx;

	option_hwrx = (struct upgt_eeprom_option_hwrx *)data;

	sc->sc_eeprom_hwrx = option_hwrx->rxfilter - UPGT_EEPROM_RX_CONST;

	DPRINTF(2, "%s: hwrx option value=0x%04x\n",
	    device_xname(sc->sc_dev), sc->sc_eeprom_hwrx);
}

static void
upgt_eeprom_parse_freq3(struct upgt_softc *sc, uint8_t *data, int len)
{
	struct upgt_eeprom_freq3_header *freq3_header;
	struct upgt_lmac_freq3 *freq3;
	int i, elements, flags;
	unsigned channel;

	freq3_header = (struct upgt_eeprom_freq3_header *)data;
	freq3 = (struct upgt_lmac_freq3 *)(freq3_header + 1);

	flags = freq3_header->flags;
	elements = freq3_header->elements;

	DPRINTF(2, "%s: flags=0x%02x\n", device_xname(sc->sc_dev), flags);
	DPRINTF(2, "%s: elements=%d\n", device_xname(sc->sc_dev), elements);
	__USE(flags);

	for (i = 0; i < elements; i++) {
		channel = ieee80211_mhz2ieee(le16toh(freq3[i].freq), 0);

		sc->sc_eeprom_freq3[channel] = freq3[i];

		DPRINTF(2, "%s: frequence=%d, channel=%d\n",
		    device_xname(sc->sc_dev),
		    le16toh(sc->sc_eeprom_freq3[channel].freq), channel);
	}
}

static void
upgt_eeprom_parse_freq4(struct upgt_softc *sc, uint8_t *data, int len)
{
	struct upgt_eeprom_freq4_header *freq4_header;
	struct upgt_eeprom_freq4_1 *freq4_1;
	struct upgt_eeprom_freq4_2 *freq4_2;
	int i, j, elements, settings, flags;
	unsigned channel;

	freq4_header = (struct upgt_eeprom_freq4_header *)data;
	freq4_1 = (struct upgt_eeprom_freq4_1 *)(freq4_header + 1);

	flags = freq4_header->flags;
	elements = freq4_header->elements;
	settings = freq4_header->settings;

	/* we need this value later */
	sc->sc_eeprom_freq6_settings = freq4_header->settings;

	DPRINTF(2, "%s: flags=0x%02x\n", device_xname(sc->sc_dev), flags);
	DPRINTF(2, "%s: elements=%d\n", device_xname(sc->sc_dev), elements);
	DPRINTF(2, "%s: settings=%d\n", device_xname(sc->sc_dev), settings);
	__USE(flags);

	for (i = 0; i < elements; i++) {
		channel = ieee80211_mhz2ieee(le16toh(freq4_1[i].freq), 0);

		freq4_2 = (struct upgt_eeprom_freq4_2 *)freq4_1[i].data;

		for (j = 0; j < settings; j++) {
			sc->sc_eeprom_freq4[channel][j].cmd = freq4_2[j];
			sc->sc_eeprom_freq4[channel][j].pad = 0;
		}

		DPRINTF(2, "%s: frequence=%d, channel=%d\n",
		    device_xname(sc->sc_dev),
		    le16toh(freq4_1[i].freq), channel);
	}
}

static void
upgt_eeprom_parse_freq6(struct upgt_softc *sc, uint8_t *data, int len)
{
	struct upgt_lmac_freq6 *freq6;
	int i, elements;
	unsigned channel;

	freq6 = (struct upgt_lmac_freq6 *)data;

	elements = len / sizeof(struct upgt_lmac_freq6);

	DPRINTF(2, "%s: elements=%d\n", device_xname(sc->sc_dev), elements);

	for (i = 0; i < elements; i++) {
		channel = ieee80211_mhz2ieee(le16toh(freq6[i].freq), 0);

		sc->sc_eeprom_freq6[channel] = freq6[i];

		DPRINTF(2, "%s: frequence=%d, channel=%d\n",
		    device_xname(sc->sc_dev),
		    le16toh(sc->sc_eeprom_freq6[channel].freq), channel);
	}
}

static int
upgt_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct upgt_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				upgt_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				upgt_stop(sc);
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
		    (IFF_UP | IFF_RUNNING))
			upgt_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

static int
upgt_init(struct ifnet *ifp)
{
	struct upgt_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(1, "%s: %s\n", device_xname(sc->sc_dev), __func__);

	if (ifp->if_flags & IFF_RUNNING)
		upgt_stop(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));

	/* setup device rates */
	upgt_setup_rates(sc);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return 0;
}

static void
upgt_stop(struct upgt_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;

	DPRINTF(1, "%s: %s\n", device_xname(sc->sc_dev), __func__);

	/* device down */
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* change device back to initial state */
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
}

static int
upgt_media_change(struct ifnet *ifp)
{
	struct upgt_softc *sc = ifp->if_softc;
	int error;

	DPRINTF(1, "%s: %s\n", device_xname(sc->sc_dev), __func__);

	if ((error = ieee80211_media_change(ifp) != ENETRESET))
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		/* give pending USB transfers a chance to finish */
		usbd_delay_ms(sc->sc_udev, 100);
		upgt_init(ifp);
	}

	return 0;
}

static void
upgt_newassoc(struct ieee80211_node *ni, int isnew)
{

	ni->ni_txrate = 0;
}

static int
upgt_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct upgt_softc *sc = ic->ic_ifp->if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_task_newstate);
	callout_stop(&sc->scan_to);

	/* do it in a process context */
	sc->sc_state = nstate;
	sc->sc_arg = arg;
	usb_add_task(sc->sc_udev, &sc->sc_task_newstate, USB_TASKQ_DRIVER);

	return 0;
}

static void
upgt_newstate_task(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	unsigned channel;

	mutex_enter(&sc->sc_mtx);

	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		DPRINTF(1, "%s: newstate is IEEE80211_S_INIT\n",
		    device_xname(sc->sc_dev));

		/* do not accept any frames if the device is down */
		upgt_set_macfilter(sc, IEEE80211_S_INIT);
		upgt_set_led(sc, UPGT_LED_OFF);
		break;
	case IEEE80211_S_SCAN:
		DPRINTF(1, "%s: newstate is IEEE80211_S_SCAN\n",
		    device_xname(sc->sc_dev));

		channel = ieee80211_chan2ieee(ic, ic->ic_curchan);
		upgt_set_channel(sc, channel);
		upgt_set_macfilter(sc, IEEE80211_S_SCAN);
		callout_schedule(&sc->scan_to, hz / 5);
		break;
	case IEEE80211_S_AUTH:
		DPRINTF(1, "%s: newstate is IEEE80211_S_AUTH\n",
		    device_xname(sc->sc_dev));

		channel = ieee80211_chan2ieee(ic, ic->ic_curchan);
		upgt_set_channel(sc, channel);
		break;
	case IEEE80211_S_ASSOC:
		DPRINTF(1, "%s: newstate is IEEE80211_S_ASSOC\n",
		    device_xname(sc->sc_dev));

		channel = ieee80211_chan2ieee(ic, ic->ic_curchan);
		upgt_set_channel(sc, channel);
		break;
	case IEEE80211_S_RUN:
		DPRINTF(1, "%s: newstate is IEEE80211_S_RUN\n",
		    device_xname(sc->sc_dev));

		channel = ieee80211_chan2ieee(ic, ic->ic_curchan);
		upgt_set_channel(sc, channel);

		ni = ic->ic_bss;

		/*
		 * TX rate control is done by the firmware.
		 * Report the maximum rate which is available therefore.
		 */
		ni->ni_txrate = ni->ni_rates.rs_nrates - 1;

		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			upgt_set_macfilter(sc, IEEE80211_S_RUN);
		upgt_set_led(sc, UPGT_LED_ON);
		break;
	}

	mutex_exit(&sc->sc_mtx);

	sc->sc_newstate(ic, sc->sc_state, sc->sc_arg);
}

static void
upgt_next_scan(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(2, "%s: %s\n", device_xname(sc->sc_dev), __func__);

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
}

static void
upgt_start(struct ifnet *ifp)
{
	struct upgt_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int i;

	/* don't transmit packets if interface is busy or down */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	DPRINTF(2, "%s: %s\n", device_xname(sc->sc_dev), __func__);

	for (i = 0; i < UPGT_TX_COUNT; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		if (data_tx->m != NULL)
			continue;

		IF_POLL(&ic->ic_mgtq, m);
		if (m != NULL) {
			/* management frame */
			IF_DEQUEUE(&ic->ic_mgtq, m);

			ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
			m->m_pkthdr.rcvif = NULL;

			bpf_mtap3(ic->ic_rawbpf, m);

			if ((data_tx->addr = upgt_mem_alloc(sc)) == 0) {
				aprint_error_dev(sc->sc_dev,
				    "no free prism memory\n");
				m_freem(m);
				ifp->if_oerrors++;
				break;
			}
			data_tx->ni = ni;
			data_tx->m = m;
			sc->tx_queued++;
		} else {
			/* data frame */
			if (ic->ic_state != IEEE80211_S_RUN)
				break;

			IFQ_POLL(&ifp->if_snd, m);
			if (m == NULL)
				break;

			IFQ_DEQUEUE(&ifp->if_snd, m);
			if (m->m_len < sizeof(struct ether_header) &&
			    !(m = m_pullup(m, sizeof(struct ether_header))))
				continue;

			eh = mtod(m, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				m_freem(m);
				continue;
			}

			bpf_mtap(ifp, m);

			m = ieee80211_encap(ic, m, ni);
			if (m == NULL) {
				ieee80211_free_node(ni);
				continue;
			}

			bpf_mtap3(ic->ic_rawbpf, m);

			if ((data_tx->addr = upgt_mem_alloc(sc)) == 0) {
				aprint_error_dev(sc->sc_dev,
				    "no free prism memory\n");
				m_freem(m);
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				break;
			}
			data_tx->ni = ni;
			data_tx->m = m;
			sc->tx_queued++;
		}
	}

	if (sc->tx_queued > 0) {
		DPRINTF(2, "%s: tx_queued=%d\n",
		    device_xname(sc->sc_dev), sc->tx_queued);
		/* process the TX queue in process context */
		ifp->if_timer = 5;
		ifp->if_flags |= IFF_OACTIVE;
		usb_rem_task(sc->sc_udev, &sc->sc_task_tx);
		usb_add_task(sc->sc_udev, &sc->sc_task_tx, USB_TASKQ_DRIVER);
	}
}

static void
upgt_watchdog(struct ifnet *ifp)
{
	struct upgt_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_state == IEEE80211_S_INIT)
		return;

	aprint_error_dev(sc->sc_dev, "watchdog timeout\n");

	/* TODO: what shall we do on TX timeout? */

	ieee80211_watchdog(ic);
}

static void
upgt_tx_task(void *arg)
{
	struct upgt_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct ifnet *ifp = &sc->sc_if;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_tx_desc *txdesc;
	struct mbuf *m;
	uint32_t addr;
	int i, len, pad, s;
	usbd_status error;

	mutex_enter(&sc->sc_mtx);
	upgt_set_led(sc, UPGT_LED_BLINK);
	mutex_exit(&sc->sc_mtx);

	s = splnet();

	for (i = 0; i < UPGT_TX_COUNT; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		if (data_tx->m == NULL)
			continue;

		m = data_tx->m;
		addr = data_tx->addr + UPGT_MEMSIZE_FRAME_HEAD;

		/*
		 * Software crypto.
		 */
		wh = mtod(m, struct ieee80211_frame *);

		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			k = ieee80211_crypto_encap(ic, data_tx->ni, m);
			if (k == NULL) {
				m_freem(m);
				data_tx->m = NULL;
				ieee80211_free_node(data_tx->ni);
				data_tx->ni = NULL;
				ifp->if_oerrors++;
				break;
			}

			/* in case packet header moved, reset pointer */
			wh = mtod(m, struct ieee80211_frame *);
		}

		/*
		 * Transmit the URB containing the TX data.
		 */
		memset(data_tx->buf, 0, sizeof(*mem) + sizeof(*txdesc));

		mem = (struct upgt_lmac_mem *)data_tx->buf;
		mem->addr = htole32(addr);

		txdesc = (struct upgt_lmac_tx_desc *)(mem + 1);

		/* XXX differ between data and mgmt frames? */
		txdesc->header1.flags = UPGT_H1_FLAGS_TX_DATA;
		txdesc->header1.type = UPGT_H1_TYPE_TX_DATA;
		txdesc->header1.len = htole16(m->m_pkthdr.len);

		txdesc->header2.reqid = htole32(data_tx->addr);
		txdesc->header2.type = htole16(UPGT_H2_TYPE_TX_ACK_YES);
		txdesc->header2.flags = htole16(UPGT_H2_FLAGS_TX_ACK_YES);

		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_MGT) {
			/* always send mgmt frames at lowest rate (DS1) */
			memset(txdesc->rates, 0x10, sizeof(txdesc->rates));
		} else {
			memcpy(txdesc->rates, sc->sc_cur_rateset,
			    sizeof(txdesc->rates));
		}
		txdesc->type = htole32(UPGT_TX_DESC_TYPE_DATA);
		txdesc->pad3[0] = UPGT_TX_DESC_PAD3_SIZE;

		if (sc->sc_drvbpf != NULL) {
			struct upgt_tx_radiotap_header *tap = &sc->sc_txtap;

			tap->wt_flags = 0;
			tap->wt_rate = 0;	/* TODO: where to get from? */
			tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
			tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

			bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m);
		}

		/* copy frame below our TX descriptor header */
		m_copydata(m, 0, m->m_pkthdr.len,
		    data_tx->buf + sizeof(*mem) + sizeof(*txdesc));

		/* calculate frame size */
		len = sizeof(*mem) + sizeof(*txdesc) + m->m_pkthdr.len;

		if (len & 3) {
			/* we need to align the frame to a 4 byte boundary */
			pad = 4 - (len & 3);
			memset(data_tx->buf + len, 0, pad);
			len += pad;
		}

		/* calculate frame checksum */
		mem->chksum = upgt_chksum_le((uint32_t *)txdesc,
		    len - sizeof(*mem));

		/* we do not need the mbuf anymore */
		m_freem(m);
		data_tx->m = NULL;

		ieee80211_free_node(data_tx->ni);
		data_tx->ni = NULL;

		DPRINTF(2, "%s: TX start data sending\n",
		    device_xname(sc->sc_dev));

		usbd_setup_xfer(data_tx->xfer, sc->sc_tx_pipeh, data_tx,
		    data_tx->buf, len, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
		    UPGT_USB_TIMEOUT, NULL);
		error = usbd_transfer(data_tx->xfer);
		if (error != USBD_NORMAL_COMPLETION &&
		    error != USBD_IN_PROGRESS) {
			aprint_error_dev(sc->sc_dev,
			    "could not transmit TX data URB\n");
			ifp->if_oerrors++;
			break;
		}

		DPRINTF(2, "%s: TX sent (%d bytes)\n",
		    device_xname(sc->sc_dev), len);
	}

	splx(s);

	/*
	 * If we don't regulary read the device statistics, the RX queue
	 * will stall.  It's strange, but it works, so we keep reading
	 * the statistics here.  *shrug*
	 */
	mutex_enter(&sc->sc_mtx);
	upgt_get_stats(sc);
	mutex_exit(&sc->sc_mtx);
}

static void
upgt_tx_done(struct upgt_softc *sc, uint8_t *data)
{
	struct ifnet *ifp = &sc->sc_if;
	struct upgt_lmac_tx_done_desc *desc;
	int i, s;

	s = splnet();

	desc = (struct upgt_lmac_tx_done_desc *)data;

	for (i = 0; i < UPGT_TX_COUNT; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		if (data_tx->addr == le32toh(desc->header2.reqid)) {
			upgt_mem_free(sc, data_tx->addr);
			data_tx->addr = 0;

			sc->tx_queued--;
			ifp->if_opackets++;

			DPRINTF(2, "%s: TX done: ", device_xname(sc->sc_dev));
			DPRINTF(2, "memaddr=0x%08x, status=0x%04x, rssi=%d, ",
			    le32toh(desc->header2.reqid),
			    le16toh(desc->status),
			    le16toh(desc->rssi));
			DPRINTF(2, "seq=%d\n", le16toh(desc->seq));
			break;
		}
	}

	if (sc->tx_queued == 0) {
		/* TX queued was processed, continue */
		ifp->if_timer = 0;
		ifp->if_flags &= ~IFF_OACTIVE;
		upgt_start(ifp);
	}

	splx(s);
}

static void
upgt_rx_cb(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct upgt_data *data_rx = priv;
	struct upgt_softc *sc = data_rx->sc;
	int len;
	struct upgt_lmac_header *header;
	struct upgt_lmac_eeprom *eeprom;
	uint8_t h1_type;
	uint16_t h2_type;

	DPRINTF(3, "%s: %s\n", device_xname(sc->sc_dev), __func__);

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_rx_pipeh);
		goto skip;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	/*
	 * Check what type of frame came in.
	 */
	header = (struct upgt_lmac_header *)(data_rx->buf + 4);

	h1_type = header->header1.type;
	h2_type = le16toh(header->header2.type);

	if (h1_type == UPGT_H1_TYPE_CTRL &&
	    h2_type == UPGT_H2_TYPE_EEPROM) {
		eeprom = (struct upgt_lmac_eeprom *)(data_rx->buf + 4);
		uint16_t eeprom_offset = le16toh(eeprom->offset);
		uint16_t eeprom_len = le16toh(eeprom->len);

		DPRINTF(2, "%s: received EEPROM block (offset=%d, len=%d)\n",
			device_xname(sc->sc_dev), eeprom_offset, eeprom_len);

		memcpy(sc->sc_eeprom + eeprom_offset,
		    data_rx->buf + sizeof(struct upgt_lmac_eeprom) + 4,
		    eeprom_len);

		/* EEPROM data has arrived in time, wakeup tsleep() */
		wakeup(sc);
	} else
	if (h1_type == UPGT_H1_TYPE_CTRL &&
	    h2_type == UPGT_H2_TYPE_TX_DONE) {
		DPRINTF(2, "%s: received 802.11 TX done\n",
		    device_xname(sc->sc_dev));

		upgt_tx_done(sc, data_rx->buf + 4);
	} else
	if (h1_type == UPGT_H1_TYPE_RX_DATA ||
	    h1_type == UPGT_H1_TYPE_RX_DATA_MGMT) {
		DPRINTF(3, "%s: received 802.11 RX data\n",
		    device_xname(sc->sc_dev));

		upgt_rx(sc, data_rx->buf + 4, le16toh(header->header1.len));
	} else
	if (h1_type == UPGT_H1_TYPE_CTRL &&
	    h2_type == UPGT_H2_TYPE_STATS) {
		DPRINTF(2, "%s: received statistic data\n",
		    device_xname(sc->sc_dev));

		/* TODO: what could we do with the statistic data? */
	} else {
		/* ignore unknown frame types */
		DPRINTF(1, "%s: received unknown frame type 0x%02x\n",
		    device_xname(sc->sc_dev), header->header1.type);
	}

skip:	/* setup new transfer */
	usbd_setup_xfer(xfer, sc->sc_rx_pipeh, data_rx, data_rx->buf, MCLBYTES,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, upgt_rx_cb);
	(void)usbd_transfer(xfer);
}

static void
upgt_rx(struct upgt_softc *sc, uint8_t *data, int pkglen)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct upgt_lmac_rx_desc *rxdesc;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int s;

	/* access RX packet descriptor */
	rxdesc = (struct upgt_lmac_rx_desc *)data;

	/* create mbuf which is suitable for strict alignment archs */
#define ETHER_ALIGN	0
	m = m_devget(rxdesc->data, pkglen, ETHER_ALIGN, ifp, NULL);
	if (m == NULL) {
		DPRINTF(1, "%s: could not create RX mbuf\n",
		   device_xname(sc->sc_dev));
		ifp->if_ierrors++;
		return;
	}

	s = splnet();

	if (sc->sc_drvbpf != NULL) {
		struct upgt_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
		tap->wr_rate = upgt_rx_rate(sc, rxdesc->rate);
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_antsignal = rxdesc->rssi;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	/* trim FCS */
	m_adj(m, -IEEE80211_CRC_LEN);

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* push the frame up to the 802.11 stack */
	ieee80211_input(ic, m, ni, rxdesc->rssi, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);

	splx(s);

	DPRINTF(3, "%s: RX done\n", device_xname(sc->sc_dev));
}

static void
upgt_setup_rates(struct upgt_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/*
	 * 0x01 = OFMD6   0x10 = DS1
	 * 0x04 = OFDM9   0x11 = DS2
	 * 0x06 = OFDM12  0x12 = DS5
	 * 0x07 = OFDM18  0x13 = DS11
	 * 0x08 = OFDM24
	 * 0x09 = OFDM36
	 * 0x0a = OFDM48
	 * 0x0b = OFDM54
	 */
	const uint8_t rateset_auto_11b[] =
	    { 0x13, 0x13, 0x12, 0x11, 0x11, 0x10, 0x10, 0x10 };
	const uint8_t rateset_auto_11g[] =
	    { 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x04, 0x01 };
	const uint8_t rateset_fix_11bg[] =
	    { 0x10, 0x11, 0x12, 0x13, 0x01, 0x04, 0x06, 0x07,
	      0x08, 0x09, 0x0a, 0x0b };

	if (ic->ic_fixed_rate == IEEE80211_FIXED_RATE_NONE) {
		/*
		 * Automatic rate control is done by the device.
		 * We just pass the rateset from which the device
		 * will pickup a rate.
		 */
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			memcpy(sc->sc_cur_rateset, rateset_auto_11b,
			    sizeof(sc->sc_cur_rateset));
		if (ic->ic_curmode == IEEE80211_MODE_11G ||
		    ic->ic_curmode == IEEE80211_MODE_AUTO)
			memcpy(sc->sc_cur_rateset, rateset_auto_11g,
			    sizeof(sc->sc_cur_rateset));
	} else {
		/* set a fixed rate */
		memset(sc->sc_cur_rateset, rateset_fix_11bg[ic->ic_fixed_rate],
		    sizeof(sc->sc_cur_rateset));
	}
}

static uint8_t
upgt_rx_rate(struct upgt_softc *sc, const int rate)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		if (rate < 0 || rate > 3)
			/* invalid rate */
			return 0;

		switch (rate) {
		case 0:
			return 2;
		case 1:
			return 4;
		case 2:
			return 11;
		case 3:
			return 22;
		default:
			return 0;
		}
	}

	if (ic->ic_curmode == IEEE80211_MODE_11G) {
		if (rate < 0 || rate > 11)
			/* invalid rate */
			return 0;

		switch (rate) {
		case 0:
			return 2;
		case 1:
			return 4;
		case 2:
			return 11;
		case 3:
			return 22;
		case 4:
			return 12;
		case 5:
			return 18;
		case 6:
			return 24;
		case 7:
			return 36;
		case 8:
			return 48;
		case 9:
			return 72;
		case 10:
			return 96;
		case 11:
			return 108;
		default:
			return 0;
		}
	}

	return 0;
}

static int
upgt_set_macfilter(struct upgt_softc *sc, uint8_t state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_filter *filter;
	int len;
	const uint8_t broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	/*
	 * Transmit the URB containing the CMD data.
	 */
	len = sizeof(*mem) + sizeof(*filter);

	memset(data_cmd->buf, 0, len);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	filter = (struct upgt_lmac_filter *)(mem + 1);

	filter->header1.flags = UPGT_H1_FLAGS_TX_NO_CALLBACK;
	filter->header1.type = UPGT_H1_TYPE_CTRL;
	filter->header1.len = htole16(
	    sizeof(struct upgt_lmac_filter) -
	    sizeof(struct upgt_lmac_header));

	filter->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	filter->header2.type = htole16(UPGT_H2_TYPE_MACFILTER);
	filter->header2.flags = 0;

	switch (state) {
	case IEEE80211_S_INIT:
		DPRINTF(1, "%s: set MAC filter to INIT\n",
		    device_xname(sc->sc_dev));

		filter->type = htole16(UPGT_FILTER_TYPE_RESET);
		break;
	case IEEE80211_S_SCAN:
		DPRINTF(1, "%s: set MAC filter to SCAN (bssid %s)\n",
		    device_xname(sc->sc_dev), ether_sprintf(broadcast));

		filter->type = htole16(UPGT_FILTER_TYPE_NONE);
		IEEE80211_ADDR_COPY(filter->dst, ic->ic_myaddr);
		IEEE80211_ADDR_COPY(filter->src, broadcast);
		filter->unknown1 = htole16(UPGT_FILTER_UNKNOWN1);
		filter->rxaddr = htole32(sc->sc_memaddr_rx_start);
		filter->unknown2 = htole16(UPGT_FILTER_UNKNOWN2);
		filter->rxhw = htole32(sc->sc_eeprom_hwrx);
		filter->unknown3 = htole16(UPGT_FILTER_UNKNOWN3);
		break;
	case IEEE80211_S_RUN:
		DPRINTF(1, "%s: set MAC filter to RUN (bssid %s)\n",
		    device_xname(sc->sc_dev), ether_sprintf(ni->ni_bssid));

		filter->type = htole16(UPGT_FILTER_TYPE_STA);
		IEEE80211_ADDR_COPY(filter->dst, ic->ic_myaddr);
		IEEE80211_ADDR_COPY(filter->src, ni->ni_bssid);
		filter->unknown1 = htole16(UPGT_FILTER_UNKNOWN1);
		filter->rxaddr = htole32(sc->sc_memaddr_rx_start);
		filter->unknown2 = htole16(UPGT_FILTER_UNKNOWN2);
		filter->rxhw = htole32(sc->sc_eeprom_hwrx);
		filter->unknown3 = htole16(UPGT_FILTER_UNKNOWN3);
		break;
	default:
		aprint_error_dev(sc->sc_dev,
		    "MAC filter does not know that state\n");
		break;
	}

	mem->chksum = upgt_chksum_le((uint32_t *)filter, sizeof(*filter));

	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not transmit macfilter CMD data URB\n");
		return EIO;
	}

	return 0;
}

static int
upgt_set_channel(struct upgt_softc *sc, unsigned channel)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_channel *chan;
	int len;

	DPRINTF(1, "%s: %s: %d\n", device_xname(sc->sc_dev), __func__,
	    channel);

	/*
	 * Transmit the URB containing the CMD data.
	 */
	len = sizeof(*mem) + sizeof(*chan);

	memset(data_cmd->buf, 0, len);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	chan = (struct upgt_lmac_channel *)(mem + 1);

	chan->header1.flags = UPGT_H1_FLAGS_TX_NO_CALLBACK;
	chan->header1.type = UPGT_H1_TYPE_CTRL;
	chan->header1.len = htole16(
	    sizeof(struct upgt_lmac_channel) -
	    sizeof(struct upgt_lmac_header));

	chan->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	chan->header2.type = htole16(UPGT_H2_TYPE_CHANNEL);
	chan->header2.flags = 0;

	chan->unknown1 = htole16(UPGT_CHANNEL_UNKNOWN1);
	chan->unknown2 = htole16(UPGT_CHANNEL_UNKNOWN2);
	chan->freq6 = sc->sc_eeprom_freq6[channel];
	chan->settings = sc->sc_eeprom_freq6_settings;
	chan->unknown3 = UPGT_CHANNEL_UNKNOWN3;

	memcpy(chan->freq3_1, &sc->sc_eeprom_freq3[channel].data,
	    sizeof(chan->freq3_1));

	memcpy(chan->freq4, &sc->sc_eeprom_freq4[channel],
	    sizeof(sc->sc_eeprom_freq4[channel]));

	memcpy(chan->freq3_2, &sc->sc_eeprom_freq3[channel].data,
	    sizeof(chan->freq3_2));

	mem->chksum = upgt_chksum_le((uint32_t *)chan, sizeof(*chan));

	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not transmit channel CMD data URB\n");
		return EIO;
	}

	return 0;
}

static void
upgt_set_led(struct upgt_softc *sc, int action)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_led *led;
	struct timeval t;
	int len;

	/*
	 * Transmit the URB containing the CMD data.
	 */
	len = sizeof(*mem) + sizeof(*led);

	memset(data_cmd->buf, 0, len);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	led = (struct upgt_lmac_led *)(mem + 1);

	led->header1.flags = UPGT_H1_FLAGS_TX_NO_CALLBACK;
	led->header1.type = UPGT_H1_TYPE_CTRL;
	led->header1.len = htole16(
	    sizeof(struct upgt_lmac_led) -
	    sizeof(struct upgt_lmac_header));

	led->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	led->header2.type = htole16(UPGT_H2_TYPE_LED);
	led->header2.flags = 0;

	switch (action) {
	case UPGT_LED_OFF:
		led->mode = htole16(UPGT_LED_MODE_SET);
		led->action_fix = 0;
		led->action_tmp = htole16(UPGT_LED_ACTION_OFF);
		led->action_tmp_dur = 0;
		break;
	case UPGT_LED_ON:
		led->mode = htole16(UPGT_LED_MODE_SET);
		led->action_fix = 0;
		led->action_tmp = htole16(UPGT_LED_ACTION_ON);
		led->action_tmp_dur = 0;
		break;
	case UPGT_LED_BLINK:
		if (ic->ic_state != IEEE80211_S_RUN)
			return;
		if (sc->sc_led_blink)
			/* previous blink was not finished */
			return;
		led->mode = htole16(UPGT_LED_MODE_SET);
		led->action_fix = htole16(UPGT_LED_ACTION_OFF);
		led->action_tmp = htole16(UPGT_LED_ACTION_ON);
		led->action_tmp_dur = htole16(UPGT_LED_ACTION_TMP_DUR);
		/* lock blink */
		sc->sc_led_blink = 1;
		t.tv_sec = 0;
		t.tv_usec = UPGT_LED_ACTION_TMP_DUR * 1000L;
		callout_schedule(&sc->led_to, tvtohz(&t));
		break;
	default:
		return;
	}

	mem->chksum = upgt_chksum_le((uint32_t *)led, sizeof(*led));

	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not transmit led CMD URB\n");
	}
}

static void
upgt_set_led_blink(void *arg)
{
	struct upgt_softc *sc = arg;

	/* blink finished, we are ready for a next one */
	sc->sc_led_blink = 0;
	callout_stop(&sc->led_to);
}

static int
upgt_get_stats(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;
	struct upgt_lmac_mem *mem;
	struct upgt_lmac_stats *stats;
	int len;

	/*
	 * Transmit the URB containing the CMD data.
	 */
	len = sizeof(*mem) + sizeof(*stats);

	memset(data_cmd->buf, 0, len);

	mem = (struct upgt_lmac_mem *)data_cmd->buf;
	mem->addr = htole32(sc->sc_memaddr_frame_start +
	    UPGT_MEMSIZE_FRAME_HEAD);

	stats = (struct upgt_lmac_stats *)(mem + 1);

	stats->header1.flags = 0;
	stats->header1.type = UPGT_H1_TYPE_CTRL;
	stats->header1.len = htole16(
	    sizeof(struct upgt_lmac_stats) -
	    sizeof(struct upgt_lmac_header));

	stats->header2.reqid = htole32(sc->sc_memaddr_frame_start);
	stats->header2.type = htole16(UPGT_H2_TYPE_STATS);
	stats->header2.flags = 0;

	mem->chksum = upgt_chksum_le((uint32_t *)stats, sizeof(*stats));

	if (upgt_bulk_xmit(sc, data_cmd, sc->sc_tx_pipeh, &len, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not transmit statistics CMD data URB\n");
		return EIO;
	}

	return 0;

}

static int
upgt_alloc_tx(struct upgt_softc *sc)
{
	int i;

	sc->tx_queued = 0;

	for (i = 0; i < UPGT_TX_COUNT; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		data_tx->sc = sc;

		data_tx->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data_tx->xfer == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate TX xfer\n");
			return ENOMEM;
		}

		data_tx->buf = usbd_alloc_buffer(data_tx->xfer, MCLBYTES);
		if (data_tx->buf == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate TX buffer\n");
			return ENOMEM;
		}
	}

	return 0;
}

static int
upgt_alloc_rx(struct upgt_softc *sc)
{
	struct upgt_data *data_rx = &sc->rx_data;

	data_rx->sc = sc;

	data_rx->xfer = usbd_alloc_xfer(sc->sc_udev);
	if (data_rx->xfer == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate RX xfer\n");
		return ENOMEM;
	}

	data_rx->buf = usbd_alloc_buffer(data_rx->xfer, MCLBYTES);
	if (data_rx->buf == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate RX buffer\n");
		return ENOMEM;
	}

	return 0;
}

static int
upgt_alloc_cmd(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;

	data_cmd->sc = sc;

	data_cmd->xfer = usbd_alloc_xfer(sc->sc_udev);
	if (data_cmd->xfer == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate RX xfer\n");
		return ENOMEM;
	}

	data_cmd->buf = usbd_alloc_buffer(data_cmd->xfer, MCLBYTES);
	if (data_cmd->buf == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate RX buffer\n");
		return ENOMEM;
	}

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_SOFTNET);

	return 0;
}

static void
upgt_free_tx(struct upgt_softc *sc)
{
	int i;

	for (i = 0; i < UPGT_TX_COUNT; i++) {
		struct upgt_data *data_tx = &sc->tx_data[i];

		if (data_tx->xfer != NULL) {
			usbd_free_xfer(data_tx->xfer);
			data_tx->xfer = NULL;
		}

		data_tx->ni = NULL;
	}
}

static void
upgt_free_rx(struct upgt_softc *sc)
{
	struct upgt_data *data_rx = &sc->rx_data;

	if (data_rx->xfer != NULL) {
		usbd_free_xfer(data_rx->xfer);
		data_rx->xfer = NULL;
	}

	data_rx->ni = NULL;
}

static void
upgt_free_cmd(struct upgt_softc *sc)
{
	struct upgt_data *data_cmd = &sc->cmd_data;

	if (data_cmd->xfer != NULL) {
		usbd_free_xfer(data_cmd->xfer);
		data_cmd->xfer = NULL;
	}

	mutex_destroy(&sc->sc_mtx);
}

static int
upgt_bulk_xmit(struct upgt_softc *sc, struct upgt_data *data,
    usbd_pipe_handle pipeh, uint32_t *size, int flags)
{
        usbd_status status;

	status = usbd_bulk_transfer(data->xfer, pipeh,
	    USBD_NO_COPY | flags, UPGT_USB_TIMEOUT, data->buf, size,
	    "upgt_bulk_xmit");
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "%s: error %s\n", __func__,
		    usbd_errstr(status));
		return EIO;
	}

	return 0;
}

#if 0
static void
upgt_hexdump(void *buf, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			printf("%s%5i:", i ? "\n" : "", i);
		if (i % 4 == 0)
			printf(" ");
		printf("%02x", (int)*((uint8_t *)buf + i));
	}
	printf("\n");
}
#endif

static uint32_t
upgt_crc32_le(const void *buf, size_t size)
{
	uint32_t crc;

	crc = ether_crc32_le(buf, size);

	/* apply final XOR value as common for CRC-32 */
	crc = htole32(crc ^ 0xffffffffU);

	return crc;
}

/*
 * The firmware awaits a checksum for each frame we send to it.
 * The algorithm used therefor is uncommon but somehow similar to CRC32.
 */
static uint32_t
upgt_chksum_le(const uint32_t *buf, size_t size)
{
	int i;
	uint32_t crc = 0;

	for (i = 0; i < size; i += sizeof(uint32_t)) {
		crc = htole32(crc ^ *buf++);
		crc = htole32((crc >> 5) ^ (crc << 3));
	}

	return crc;
}
