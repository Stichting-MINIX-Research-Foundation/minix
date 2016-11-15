/*	$NetBSD: if_athn_usb.c,v 1.8 2015/02/21 10:42:15 nonaka Exp $	*/
/*	$OpenBSD: if_athn_usb.c,v 1.12 2013/01/14 09:50:31 jsing Exp $	*/

/*-
 * Copyright (c) 2011 Damien Bergamini <damien.bergamini@free.fr>
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
 * USB front-end for Atheros AR9271 and AR7010 chipsets.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_athn_usb.c,v 1.8 2015/02/21 10:42:15 nonaka Exp $");

#ifdef	_KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>

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

#include <netinet/if_inarp.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/ic/athnreg.h>
#include <dev/ic/athnvar.h>
#include <dev/ic/arn9285.h>
#include <dev/usb/if_athn_usb.h>

#define ATHN_USB_SOFTC(sc)	((struct athn_usb_softc *)(sc))
#define ATHN_USB_NODE(ni)	((struct athn_usb_node *)(ni))

#define IS_UP_AND_RUNNING(ifp) \
	(((ifp)->if_flags & IFF_UP) && ((ifp)->if_flags & IFF_RUNNING))

#define athn_usb_wmi_cmd(sc, cmd_id) \
	athn_usb_wmi_xcmd(sc, cmd_id, NULL, 0, NULL)

Static int	athn_usb_activate(device_t, enum devact);
Static int	athn_usb_detach(device_t, int);
Static int	athn_usb_match(device_t, cfdata_t, void *);
Static void	athn_usb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(athn_usb, sizeof(struct athn_usb_softc), athn_usb_match,
    athn_usb_attach, athn_usb_detach, athn_usb_activate);

Static int	athn_usb_alloc_rx_list(struct athn_usb_softc *);
Static int	athn_usb_alloc_tx_cmd(struct athn_usb_softc *);
Static int	athn_usb_alloc_tx_list(struct athn_usb_softc *);
Static void	athn_usb_attachhook(device_t);
Static void	athn_usb_bcneof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static void	athn_usb_close_pipes(struct athn_usb_softc *);
Static int	athn_usb_create_hw_node(struct athn_usb_softc *,
		    struct ar_htc_target_sta *);
Static int	athn_usb_create_node(struct athn_usb_softc *,
		    struct ieee80211_node *);
Static void	athn_usb_do_async(struct athn_usb_softc *,
		    void (*)(struct athn_usb_softc *, void *), void *, int);
Static void	athn_usb_free_rx_list(struct athn_usb_softc *);
Static void	athn_usb_free_tx_cmd(struct athn_usb_softc *);
Static void	athn_usb_free_tx_list(struct athn_usb_softc *);
Static int	athn_usb_htc_connect_svc(struct athn_usb_softc *, uint16_t,
		    uint8_t, uint8_t, uint8_t *);
Static int	athn_usb_htc_msg(struct athn_usb_softc *, uint16_t, void *,
		    int);
Static int	athn_usb_htc_setup(struct athn_usb_softc *);
Static int	athn_usb_init(struct ifnet *);
Static void	athn_usb_intr(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static int	athn_usb_ioctl(struct ifnet *, u_long, void *);
Static int	athn_usb_load_firmware(struct athn_usb_softc *);
Static const struct athn_usb_type *
		athn_usb_lookup(int, int);
Static int	athn_usb_media_change(struct ifnet *);
Static void	athn_usb_newassoc(struct ieee80211_node *, int);
Static void	athn_usb_newassoc_cb(struct athn_usb_softc *, void *);
Static int	athn_usb_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
Static void	athn_usb_newstate_cb(struct athn_usb_softc *, void *);
Static void	athn_usb_node_cleanup(struct ieee80211_node *);
Static void	athn_usb_node_cleanup_cb(struct athn_usb_softc *, void *);
Static int	athn_usb_open_pipes(struct athn_usb_softc *);
Static uint32_t	athn_usb_read(struct athn_softc *, uint32_t);
Static int	athn_usb_remove_hw_node(struct athn_usb_softc *, uint8_t *);
Static void	athn_usb_rx_enable(struct athn_softc *);
Static void	athn_usb_rx_frame(struct athn_usb_softc *, struct mbuf *);
Static void	athn_usb_rx_radiotap(struct athn_softc *, struct mbuf *,
		    struct ar_rx_status *);
Static void	athn_usb_rx_wmi_ctrl(struct athn_usb_softc *, uint8_t *, size_t);
Static void	athn_usb_rxeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static void	athn_usb_start(struct ifnet *);
Static void	athn_usb_stop(struct ifnet *);
Static void	athn_usb_swba(struct athn_usb_softc *);
Static int	athn_usb_switch_chan(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
Static void	athn_usb_task(void *);
Static int	athn_usb_tx(struct athn_softc *, struct mbuf *,
		    struct ieee80211_node *, struct athn_usb_tx_data *);
Static void	athn_usb_txeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static void	athn_usb_updateslot(struct ifnet *);
Static void	athn_usb_updateslot_cb(struct athn_usb_softc *, void *);
Static void	athn_usb_wait_async(struct athn_usb_softc *);
Static void	athn_usb_wait_cmd(struct athn_usb_softc *);
Static void	athn_usb_wait_msg(struct athn_usb_softc *);
Static void	athn_usb_wait_wmi(struct athn_usb_softc *);
Static void	athn_usb_watchdog(struct ifnet *);
Static int	athn_usb_wmi_xcmd(struct athn_usb_softc *, uint16_t, void *,
		    int, void *);
Static void	athn_usb_wmieof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static void	athn_usb_write(struct athn_softc *, uint32_t, uint32_t);
Static void	athn_usb_write_barrier(struct athn_softc *);

/************************************************************************
 * unused/notyet declarations
 */
#ifdef unused
Static int	athn_usb_read_rom(struct athn_softc *);
#endif /* unused */

#ifdef notyet_edca
Static void	athn_usb_updateedca(struct ieee80211com *);
Static void	athn_usb_updateedca_cb(struct athn_usb_softc *, void *);
#endif /* notyet_edca */

#ifdef notyet
Static int	athn_usb_ampdu_tx_start(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
Static void	athn_usb_ampdu_tx_start_cb(struct athn_usb_softc *, void *);
Static void	athn_usb_ampdu_tx_stop(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
Static void	athn_usb_ampdu_tx_stop_cb(struct athn_usb_softc *, void *);
Static void	athn_usb_delete_key(struct ieee80211com *,
		    struct ieee80211_node *, struct ieee80211_key *);
Static void	athn_usb_delete_key_cb(struct athn_usb_softc *, void *);
Static int	athn_usb_set_key(struct ieee80211com *,
		    struct ieee80211_node *, struct ieee80211_key *);
Static void	athn_usb_set_key_cb(struct athn_usb_softc *, void *);
#endif /* notyet */
/************************************************************************/

struct athn_usb_type {
	struct usb_devno	devno;
	u_int			flags;
};

Static const struct athn_usb_type *
athn_usb_lookup(int vendor, int product)
{
	static const struct athn_usb_type athn_usb_devs[] = {
#define _D(v,p,f) \
		{{ USB_VENDOR_##v, USB_PRODUCT_##p }, ATHN_USB_FLAG_##f }

		_D( ACCTON,	ACCTON_AR9280,		AR7010 ),
		_D( ACTIONTEC,	ACTIONTEC_AR9287,	AR7010 ),
		_D( ATHEROS2,	ATHEROS2_AR9271_1,	NONE ),
		_D( ATHEROS2,	ATHEROS2_AR9271_2,	NONE ),
		_D( ATHEROS2,	ATHEROS2_AR9271_3,	NONE ),
		_D( ATHEROS2,	ATHEROS2_AR9280,	AR7010 ),
		_D( ATHEROS2,	ATHEROS2_AR9287,	AR7010 ),
		_D( AZUREWAVE,	AZUREWAVE_AR9271_1,	NONE ),
		_D( AZUREWAVE,	AZUREWAVE_AR9271_2,	NONE ),
		_D( AZUREWAVE,	AZUREWAVE_AR9271_3,	NONE ),
		_D( AZUREWAVE,	AZUREWAVE_AR9271_4,	NONE ),
		_D( AZUREWAVE,	AZUREWAVE_AR9271_5,	NONE ),
		_D( AZUREWAVE,	AZUREWAVE_AR9271_6,	NONE ),
		_D( DLINK2,	DLINK2_AR9271,	  	NONE ),
		_D( LITEON,	LITEON_AR9271,	  	NONE ),
		_D( NETGEAR,	NETGEAR_WNA1100,	NONE ),
		_D( NETGEAR,	NETGEAR_WNDA3200,	AR7010 ),
		_D( VIA,	VIA_AR9271,		NONE )
#undef _D
	};

	return (const void *)usb_lookup(athn_usb_devs, vendor, product);
}

Static int
athn_usb_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return athn_usb_lookup(uaa->vendor, uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

Static void
athn_usb_attach(device_t parent, device_t self, void *aux)
{
	struct athn_usb_softc *usc;
	struct athn_softc *sc;
	struct usb_attach_arg *uaa;
	int error;

	usc = device_private(self);
	sc = &usc->usc_sc;
	uaa = aux;
	sc->sc_dev = self;
	usc->usc_udev = uaa->device;

	aprint_naive("\n");
	aprint_normal("\n");

	DPRINTFN(DBG_FN, sc, "\n");

	usc->usc_athn_attached = 0;
	usc->usc_flags = athn_usb_lookup(uaa->vendor, uaa->product)->flags;
	sc->sc_flags |= ATHN_FLAG_USB;
#ifdef notyet
	/* Check if it is a combo WiFi+Bluetooth (WB193) device. */
	if (strncmp(product, "wb193", 5) == 0)
		sc->sc_flags |= ATHN_FLAG_BTCOEX3WIRE;
#endif

	sc->sc_ops.read = athn_usb_read;
	sc->sc_ops.write = athn_usb_write;
	sc->sc_ops.write_barrier = athn_usb_write_barrier;

	mutex_init(&usc->usc_task_mtx, MUTEX_DEFAULT, IPL_NET);
	mutex_init(&usc->usc_tx_mtx, MUTEX_DEFAULT, IPL_NONE);

	usb_init_task(&usc->usc_task, athn_usb_task, usc, 0);

	if (usbd_set_config_no(usc->usc_udev, 1, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set configuration no\n");
		goto fail;
	}

	/* Get the first interface handle. */
	error = usbd_device2interface_handle(usc->usc_udev, 0, &usc->usc_iface);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not get interface handle\n");
		goto fail;
	}

	if (athn_usb_open_pipes(usc) != 0)
		goto fail;

	/* Allocate xfer for firmware commands. */
	if (athn_usb_alloc_tx_cmd(usc) != 0)
		goto fail;

	config_mountroot(self, athn_usb_attachhook);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, usc->usc_udev, sc->sc_dev);
	return;

 fail:
	athn_usb_free_tx_cmd(usc);
	athn_usb_close_pipes(usc);
	usb_rem_task(usc->usc_udev, &usc->usc_task);
	mutex_destroy(&usc->usc_tx_mtx);
	mutex_destroy(&usc->usc_task_mtx);
}

Static void
athn_usb_node_cleanup_cb(struct athn_usb_softc *usc, void *arg)
{
	uint8_t sta_index = *(uint8_t *)arg;

	DPRINTFN(DBG_FN, usc, "\n");
	DPRINTFN(DBG_NODES, usc, "removing node %u\n", sta_index);
	athn_usb_remove_hw_node(usc, &sta_index);
}

Static void
athn_usb_node_cleanup(struct ieee80211_node *ni)
{
	struct athn_usb_softc *usc;
	struct ieee80211com *ic;
	uint8_t sta_index;

	usc = ATHN_USB_SOFTC(ni->ni_ic->ic_ifp->if_softc);
	ic = &ATHN_SOFTC(usc)->sc_ic;

	DPRINTFN(DBG_FN, usc, "\n");

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		sta_index = ATHN_NODE(ni)->sta_index;
		if (sta_index != 0)
			athn_usb_do_async(usc, athn_usb_node_cleanup_cb,
			    &sta_index, sizeof(sta_index));
	}
	usc->usc_node_cleanup(ni);
}

Static void
athn_usb_attachhook(device_t arg)
{
	struct athn_usb_softc *usc = device_private(arg);
	struct athn_softc *sc = &usc->usc_sc;
	struct athn_ops *ops = &sc->sc_ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	size_t i;
	int s, error;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, usc, "\n");

	/* Load firmware. */
	error = athn_usb_load_firmware(usc);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not load firmware (%d)\n", error);
		return;
	}

	/* Setup the host transport communication interface. */
	error = athn_usb_htc_setup(usc);
	if (error != 0)
		return;

	/* We're now ready to attach the bus agnostic driver. */
	s = splnet();
	ic->ic_ifp = ifp;
	ic->ic_updateslot = athn_usb_updateslot;
	sc->sc_max_aid = AR_USB_MAX_STA;  /* Firmware is limited to 8 STA */
	sc->sc_media_change = athn_usb_media_change;
	error = athn_attach(sc);
	if (error != 0) {
		splx(s);
		return;
	}
	usc->usc_athn_attached = 1;

	/* Override some operations for USB. */
	ifp->if_init = athn_usb_init;
	ifp->if_ioctl = athn_usb_ioctl;
	ifp->if_start = athn_usb_start;
	ifp->if_watchdog = athn_usb_watchdog;

	/* hooks for HostAP association and disassociation */
	ic->ic_newassoc = athn_usb_newassoc;
	usc->usc_node_cleanup = ic->ic_node_cleanup;
	ic->ic_node_cleanup = athn_usb_node_cleanup;

#ifdef notyet_edca
	ic->ic_updateedca = athn_usb_updateedca;
#endif
#ifdef notyet
	ic->ic_set_key = athn_usb_set_key;
	ic->ic_delete_key = athn_usb_delete_key;
	ic->ic_ampdu_tx_start = athn_usb_ampdu_tx_start;
	ic->ic_ampdu_tx_stop = athn_usb_ampdu_tx_stop;
#endif
	ic->ic_newstate = athn_usb_newstate;

	ops->rx_enable = athn_usb_rx_enable;
	splx(s);

	/* Reset HW key cache entries. */
	for (i = 0; i < sc->sc_kc_entries; i++)
		athn_reset_key(sc, i);

	ops->enable_antenna_diversity(sc);

#ifdef ATHN_BT_COEXISTENCE
	/* Configure bluetooth coexistence for combo chips. */
	if (sc->sc_flags & ATHN_FLAG_BTCOEX)
		athn_btcoex_init(sc);
#endif
	/* Configure LED. */
	athn_led_init(sc);

	ieee80211_announce(ic);
}

Static int
athn_usb_detach(device_t self, int flags)
{
	struct athn_usb_softc *usc = device_private(self);
	struct athn_softc *sc = &usc->usc_sc;
	int s;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splusb();
	usc->usc_dying = 1;

	athn_usb_wait_wmi(usc);
	athn_usb_wait_cmd(usc);
	athn_usb_wait_msg(usc);
	athn_usb_wait_async(usc);

	usb_rem_task(usc->usc_udev, &usc->usc_task);

	if (usc->usc_athn_attached) {
		usc->usc_athn_attached = 0;
		athn_detach(sc);
	}
	/* Abort and close Tx/Rx pipes. */
	athn_usb_close_pipes(usc);
	splx(s);

	/* Free Tx/Rx buffers. */
	athn_usb_free_rx_list(usc);
	athn_usb_free_tx_list(usc);
	athn_usb_free_tx_cmd(usc);

	mutex_destroy(&usc->usc_tx_mtx);
	mutex_destroy(&usc->usc_task_mtx);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, usc->usc_udev, sc->sc_dev);
	return 0;
}

Static int
athn_usb_activate(device_t self, enum devact act)
{
	struct athn_usb_softc *usc = device_private(self);
	struct athn_softc *sc = &usc->usc_sc;

	DPRINTFN(DBG_FN, usc, "\n");

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(sc->sc_ic.ic_ifp);
		usc->usc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

Static int
athn_usb_open_pipes(struct athn_usb_softc *usc)
{
	usb_endpoint_descriptor_t *ed;
	int isize, error;

	DPRINTFN(DBG_FN, usc, "\n");

	error = usbd_open_pipe(usc->usc_iface, AR_PIPE_TX_DATA, 0,
	    &usc->usc_tx_data_pipe);
	if (error != 0) {
		aprint_error_dev(usc->usc_dev,
		    "could not open Tx bulk pipe\n");
		goto fail;
	}

	error = usbd_open_pipe(usc->usc_iface, AR_PIPE_RX_DATA, 0,
	    &usc->usc_rx_data_pipe);
	if (error != 0) {
		aprint_error_dev(usc->usc_dev,
		    "could not open Rx bulk pipe\n");
		goto fail;
	}

	ed = usbd_get_endpoint_descriptor(usc->usc_iface, AR_PIPE_RX_INTR);
	if (ed == NULL) {
		aprint_error_dev(usc->usc_dev,
		    "could not retrieve Rx intr pipe descriptor\n");
		goto fail;
	}
	isize = UGETW(ed->wMaxPacketSize);
	if (isize == 0) {
		aprint_error_dev(usc->usc_dev,
		    "invalid Rx intr pipe descriptor\n");
		goto fail;
	}
	usc->usc_ibuf = malloc(isize, M_USBDEV, M_NOWAIT);
	if (usc->usc_ibuf == NULL) {
		aprint_error_dev(usc->usc_dev,
		    "could not allocate Rx intr buffer\n");
		goto fail;
	}
	error = usbd_open_pipe_intr(usc->usc_iface, AR_PIPE_RX_INTR,
	    USBD_SHORT_XFER_OK, &usc->usc_rx_intr_pipe, usc, usc->usc_ibuf, isize,
	    athn_usb_intr, USBD_DEFAULT_INTERVAL);
	if (error != 0) {
		aprint_error_dev(usc->usc_dev,
		    "could not open Rx intr pipe\n");
		goto fail;
	}
	error = usbd_open_pipe(usc->usc_iface, AR_PIPE_TX_INTR, 0,
	    &usc->usc_tx_intr_pipe);
	if (error != 0) {
		aprint_error_dev(usc->usc_dev,
		    "could not open Tx intr pipe\n");
		goto fail;
	}
	return 0;
 fail:
	athn_usb_close_pipes(usc);
	return error;
}

static inline void
athn_usb_kill_pipe(usbd_pipe_handle *pipeptr)
{
	usbd_pipe_handle pipe;

	CTASSERT(sizeof(pipe) == sizeof(void *));
	pipe = atomic_swap_ptr(pipeptr, NULL);
	if (pipe != NULL) {
		usbd_abort_pipe(pipe);
		usbd_close_pipe(pipe);
	}
}

Static void
athn_usb_close_pipes(struct athn_usb_softc *usc)
{
	uint8_t *ibuf;

	DPRINTFN(DBG_FN, usc, "\n");

	athn_usb_kill_pipe(&usc->usc_tx_data_pipe);
	athn_usb_kill_pipe(&usc->usc_rx_data_pipe);
	athn_usb_kill_pipe(&usc->usc_tx_intr_pipe);
	athn_usb_kill_pipe(&usc->usc_rx_intr_pipe);
	ibuf = atomic_swap_ptr(&usc->usc_ibuf, NULL);
	if (ibuf != NULL)
		free(ibuf, M_USBDEV);
}

Static int
athn_usb_alloc_rx_list(struct athn_usb_softc *usc)
{
	struct athn_usb_rx_data *data;
	size_t i;
	int error = 0;

	DPRINTFN(DBG_FN, usc, "\n");

	for (i = 0; i < ATHN_USB_RX_LIST_COUNT; i++) {
		data = &usc->usc_rx_data[i];

		data->sc = usc;	/* Backpointer for callbacks. */

		data->xfer = usbd_alloc_xfer(usc->usc_udev);
		if (data->xfer == NULL) {
			aprint_error_dev(usc->usc_dev,
			    "could not allocate xfer\n");
			error = ENOMEM;
			break;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ATHN_USB_RXBUFSZ);
		if (data->buf == NULL) {
			aprint_error_dev(usc->usc_dev,
			    "could not allocate xfer buffer\n");
			error = ENOMEM;
			break;
		}
	}
	if (error != 0)
		athn_usb_free_rx_list(usc);
	return error;
}

Static void
athn_usb_free_rx_list(struct athn_usb_softc *usc)
{
	usbd_xfer_handle xfer;
	size_t i;

	DPRINTFN(DBG_FN, usc, "\n");

	/* NB: Caller must abort pipe first. */
	for (i = 0; i < ATHN_USB_RX_LIST_COUNT; i++) {
		CTASSERT(sizeof(xfer) == sizeof(void *));
		xfer = atomic_swap_ptr(&usc->usc_rx_data[i].xfer, NULL);
		if (xfer != NULL)
			usbd_free_xfer(xfer);
	}
}

Static int
athn_usb_alloc_tx_list(struct athn_usb_softc *usc)
{
	struct athn_usb_tx_data *data;
	size_t i;
	int error = 0;

	DPRINTFN(DBG_FN, usc, "\n");

	mutex_enter(&usc->usc_tx_mtx);
	TAILQ_INIT(&usc->usc_tx_free_list);
	for (i = 0; i < ATHN_USB_TX_LIST_COUNT; i++) {
		data = &usc->usc_tx_data[i];

		data->sc = usc;	/* Backpointer for callbacks. */

		data->xfer = usbd_alloc_xfer(usc->usc_udev);
		if (data->xfer == NULL) {
			aprint_error_dev(usc->usc_dev,
			    "could not allocate xfer\n");
			error = ENOMEM;
			break;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ATHN_USB_TXBUFSZ);
		if (data->buf == NULL) {
			aprint_error_dev(usc->usc_dev,
			    "could not allocate xfer buffer\n");
			error = ENOMEM;
			break;
		}
		/* Append this Tx buffer to our free list. */
		TAILQ_INSERT_TAIL(&usc->usc_tx_free_list, data, next);
	}
	if (error != 0)
		athn_usb_free_tx_list(usc);
	mutex_exit(&usc->usc_tx_mtx);
	return error;
}

Static void
athn_usb_free_tx_list(struct athn_usb_softc *usc)
{
	usbd_xfer_handle xfer;
	size_t i;

	DPRINTFN(DBG_FN, usc, "\n");

	/* NB: Caller must abort pipe first. */
	for (i = 0; i < ATHN_USB_TX_LIST_COUNT; i++) {
		CTASSERT(sizeof(xfer) == sizeof(void *));
		xfer = atomic_swap_ptr(&usc->usc_tx_data[i].xfer, NULL);
		if (xfer != NULL)
			usbd_free_xfer(xfer);
	}
}

Static int
athn_usb_alloc_tx_cmd(struct athn_usb_softc *usc)
{
	struct athn_usb_tx_data *data = &usc->usc_tx_cmd;

	DPRINTFN(DBG_FN, usc, "\n");

	data->sc = usc;	/* Backpointer for callbacks. */

	data->xfer = usbd_alloc_xfer(usc->usc_udev);
	if (data->xfer == NULL) {
		aprint_error_dev(usc->usc_dev, "could not allocate xfer\n");
		return ENOMEM;
	}
	data->buf = usbd_alloc_buffer(data->xfer, ATHN_USB_TXCMDSZ);
	if (data->buf == NULL) {
		aprint_error_dev(usc->usc_dev,
		    "could not allocate xfer buffer\n");
		return ENOMEM;
	}
	return 0;
}

Static void
athn_usb_free_tx_cmd(struct athn_usb_softc *usc)
{
	usbd_xfer_handle xfer;

	DPRINTFN(DBG_FN, usc, "\n");

	CTASSERT(sizeof(xfer) == sizeof(void *));
	xfer = atomic_swap_ptr(&usc->usc_tx_cmd.xfer, NULL);
	if (xfer != NULL)
		usbd_free_xfer(xfer);
}

Static void
athn_usb_task(void *arg)
{
	struct athn_usb_softc *usc = arg;
	struct athn_usb_host_cmd_ring *ring = &usc->usc_cmdq;
	struct athn_usb_host_cmd *cmd;
	int s;

	DPRINTFN(DBG_FN, usc, "\n");

	/* Process host commands. */
	s = splusb();
	mutex_spin_enter(&usc->usc_task_mtx);
	while (ring->next != ring->cur) {
		cmd = &ring->cmd[ring->next];
		mutex_spin_exit(&usc->usc_task_mtx);
		splx(s);

		/* Invoke callback. */
		if (!usc->usc_dying)
			cmd->cb(usc, cmd->data);

		s = splusb();
		mutex_spin_enter(&usc->usc_task_mtx);
		ring->queued--;
		ring->next = (ring->next + 1) % ATHN_USB_HOST_CMD_RING_COUNT;
	}
	mutex_spin_exit(&usc->usc_task_mtx);
	wakeup(ring);
	splx(s);
}

Static void
athn_usb_do_async(struct athn_usb_softc *usc,
    void (*cb)(struct athn_usb_softc *, void *), void *arg, int len)
{
	struct athn_usb_host_cmd_ring *ring = &usc->usc_cmdq;
	struct athn_usb_host_cmd *cmd;
	int s;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splusb();
	mutex_spin_enter(&usc->usc_task_mtx);
	cmd = &ring->cmd[ring->cur];
	cmd->cb = cb;
	KASSERT(len <= sizeof(cmd->data));
	memcpy(cmd->data, arg, len);
	ring->cur = (ring->cur + 1) % ATHN_USB_HOST_CMD_RING_COUNT;

	/* If there is no pending command already, schedule a task. */
	if (++ring->queued == 1) {
		mutex_spin_exit(&usc->usc_task_mtx);
		usb_add_task(usc->usc_udev, &usc->usc_task, USB_TASKQ_DRIVER);
	}
	else
		mutex_spin_exit(&usc->usc_task_mtx);
	splx(s);
}

Static void
athn_usb_wait_async(struct athn_usb_softc *usc)
{

	DPRINTFN(DBG_FN, usc, "\n");

	/* Wait for all queued asynchronous commands to complete. */
	while (usc->usc_cmdq.queued > 0)
		tsleep(&usc->usc_cmdq, 0, "cmdq", 0);
}

Static int
athn_usb_load_firmware(struct athn_usb_softc *usc)
{
	struct athn_softc *sc = &usc->usc_sc;
	firmware_handle_t fwh;
	usb_device_descriptor_t *dd;
	usb_device_request_t req;
	const char *name;
	u_char *fw, *ptr;
	size_t size, remain;
	uint32_t addr;
	int s, mlen, error;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Determine which firmware image to load. */
	if (usc->usc_flags & ATHN_USB_FLAG_AR7010) {
		dd = usbd_get_device_descriptor(usc->usc_udev);
		if (UGETW(dd->bcdDevice) == 0x0202)
			name = "athn-ar7010-11";
		else
			name = "athn-ar7010";
	}
	else
		name = "athn-ar9271";

	/* Read firmware image from the filesystem. */
	if ((error = firmware_open("if_athn", name, &fwh)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "failed to open firmware file %s (%d)\n", name, error);
		return error;
	}
	size = firmware_get_size(fwh);
	fw = firmware_malloc(size);
	if (fw == NULL) {
		aprint_error_dev(usc->usc_dev,
		    "failed to allocate firmware memory\n");
		firmware_close(fwh);
		return ENOMEM;
	}
	error = firmware_read(fwh, 0, fw, size);
	firmware_close(fwh);
	if (error != 0) {
		aprint_error_dev(usc->usc_dev,
		    "failed to read firmware (error %d)\n", error);
		firmware_free(fw, size);
		return error;
	}

	/* Load firmware image. */
	ptr = fw;
	addr = AR9271_FIRMWARE >> 8;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AR_FW_DOWNLOAD;
	USETW(req.wIndex, 0);
	remain = size;
	while (remain > 0) {
		mlen = MIN(remain, 4096);

		USETW(req.wValue, addr);
		USETW(req.wLength, mlen);
		error = usbd_do_request(usc->usc_udev, &req, ptr);
		if (error != 0) {
			firmware_free(fw, size);
			return error;
		}
		addr   += mlen >> 8;
		ptr    += mlen;
		remain -= mlen;
	}
	firmware_free(fw, size);

	/* Start firmware. */
	if (usc->usc_flags & ATHN_USB_FLAG_AR7010)
		addr = AR7010_FIRMWARE_TEXT >> 8;
	else
		addr = AR9271_FIRMWARE_TEXT >> 8;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AR_FW_DOWNLOAD_COMP;
	USETW(req.wIndex, 0);
	USETW(req.wValue, addr);
	USETW(req.wLength, 0);

	s = splusb();
	usc->usc_wait_msg_id = AR_HTC_MSG_READY;
	error = usbd_do_request(usc->usc_udev, &req, NULL);
	/* Wait at most 1 second for firmware to boot. */
	if (error == 0 && usc->usc_wait_msg_id != 0)
		error = tsleep(&usc->usc_wait_msg_id, 0, "athnfw", hz);
	usc->usc_wait_msg_id = 0;
	splx(s);
	return error;
}

Static int
athn_usb_htc_msg(struct athn_usb_softc *usc, uint16_t msg_id, void *buf,
    int len)
{
	struct athn_usb_tx_data *data = &usc->usc_tx_cmd;
	struct ar_htc_frame_hdr *htc;
	struct ar_htc_msg_hdr *msg;

	if (usc->usc_dying)
		return USBD_CANCELLED;

	DPRINTFN(DBG_FN, usc, "\n");

	htc = (struct ar_htc_frame_hdr *)data->buf;
	memset(htc, 0, sizeof(*htc));
	htc->endpoint_id = 0;
	htc->payload_len = htobe16(sizeof(*msg) + len);

	msg = (struct ar_htc_msg_hdr *)&htc[1];
	msg->msg_id = htobe16(msg_id);

	memcpy(&msg[1], buf, len);

	usbd_setup_xfer(data->xfer, usc->usc_tx_intr_pipe, NULL, data->buf,
	    sizeof(*htc) + sizeof(*msg) + len,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY, ATHN_USB_CMD_TIMEOUT, NULL);
	return usbd_sync_transfer(data->xfer);
}

Static int
athn_usb_htc_setup(struct athn_usb_softc *usc)
{
	struct ar_htc_msg_config_pipe cfg;
	int s, error;

	/*
	 * Connect WMI services to USB pipes.
	 */
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_CONTROL,
	    AR_PIPE_TX_INTR, AR_PIPE_RX_INTR, &usc->usc_ep_ctrl);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_BEACON,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_bcn);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_CAB,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_cab);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_UAPSD,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_uapsd);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_MGMT,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_mgmt);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_BE,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_data[WME_AC_BE]);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_BK,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_data[WME_AC_BK]);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_VI,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_data[WME_AC_VI]);
	if (error != 0)
		return error;
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_VO,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->usc_ep_data[WME_AC_VO]);
	if (error != 0)
		return error;

	/* Set credits for WLAN Tx pipe. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.pipe_id = UE_GET_ADDR(AR_PIPE_TX_DATA);
	cfg.credits = (usc->usc_flags & ATHN_USB_FLAG_AR7010) ? 45 : 33;

	s = splusb();

	usc->usc_wait_msg_id = AR_HTC_MSG_CONF_PIPE_RSP;
	error = athn_usb_htc_msg(usc, AR_HTC_MSG_CONF_PIPE, &cfg, sizeof(cfg));
	if (error == 0 && usc->usc_wait_msg_id != 0)
		error = tsleep(&usc->usc_wait_msg_id, 0, "athnhtc", hz);
	usc->usc_wait_msg_id = 0;

	splx(s);

	if (error != 0) {
		aprint_error_dev(usc->usc_dev, "could not configure pipe\n");
		return error;
	}

	error = athn_usb_htc_msg(usc, AR_HTC_MSG_SETUP_COMPLETE, NULL, 0);
	if (error != 0) {
		aprint_error_dev(usc->usc_dev, "could not complete setup\n");
		return error;
	}
	return 0;
}

Static int
athn_usb_htc_connect_svc(struct athn_usb_softc *usc, uint16_t svc_id,
    uint8_t ul_pipe, uint8_t dl_pipe, uint8_t *endpoint_id)
{
	struct ar_htc_msg_conn_svc msg;
	struct ar_htc_msg_conn_svc_rsp rsp;
	int s, error;

	DPRINTFN(DBG_FN, usc, "\n");

	memset(&msg, 0, sizeof(msg));
	msg.svc_id = htobe16(svc_id);
	msg.dl_pipeid = UE_GET_ADDR(dl_pipe);
	msg.ul_pipeid = UE_GET_ADDR(ul_pipe);
	s = splusb();

	usc->usc_msg_conn_svc_rsp = &rsp;

	usc->usc_wait_msg_id = AR_HTC_MSG_CONN_SVC_RSP;
	error = athn_usb_htc_msg(usc, AR_HTC_MSG_CONN_SVC, &msg, sizeof(msg));
	if (error == 0 && usc->usc_wait_msg_id != 0)
		error = tsleep(&usc->usc_wait_msg_id, 0, "athnhtc", hz);
	usc->usc_wait_msg_id = 0;

	splx(s);
	if (error != 0) {
		aprint_error_dev(usc->usc_dev,
		    "error waiting for service %d connection\n", svc_id);
		return error;
	}
	if (rsp.status != AR_HTC_SVC_SUCCESS) {
		aprint_error_dev(usc->usc_dev,
		    "service %d connection failed, error %d\n",
		    svc_id, rsp.status);
		return EIO;
	}
	DPRINTFN(DBG_INIT, usc,
	    "service %d successfully connected to endpoint %d\n",
	    svc_id, rsp.endpoint_id);

	/* Return endpoint id. */
	*endpoint_id = rsp.endpoint_id;
	return 0;
}

Static void
athn_usb_wait_msg(struct athn_usb_softc *usc)
{

	DPRINTFN(DBG_FN, usc, "\n");

	while (__predict_false(usc->usc_wait_msg_id))
		tsleep(&usc->usc_wait_msg_id, 0, "athnmsg", hz);
}

Static void
athn_usb_wait_cmd(struct athn_usb_softc *usc)
{

	DPRINTFN(DBG_FN, usc, "\n");

	while (__predict_false(usc->usc_wait_cmd_id))
		tsleep(&usc->usc_wait_cmd_id, 0, "athncmd", hz);
}

Static void
athn_usb_wmieof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct athn_usb_softc *usc = priv;

	DPRINTFN(DBG_FN, usc, "\n");

	if (__predict_false(status == USBD_STALLED))
		usbd_clear_endpoint_stall_async(usc->usc_tx_intr_pipe);

	usc->usc_wmi_done = 1;
	wakeup(&usc->usc_wmi_done);
}

Static int
athn_usb_wmi_xcmd(struct athn_usb_softc *usc, uint16_t cmd_id, void *ibuf,
    int ilen, void *obuf)
{
	struct athn_usb_tx_data *data = &usc->usc_tx_cmd;
	struct ar_htc_frame_hdr *htc;
	struct ar_wmi_cmd_hdr *wmi;
	int s, error;

	if (usc->usc_dying)
		return EIO;

	DPRINTFN(DBG_FN, usc, "\n");

	htc = (struct ar_htc_frame_hdr *)data->buf;
	memset(htc, 0, sizeof(*htc));
	htc->endpoint_id = usc->usc_ep_ctrl;
	htc->payload_len = htobe16(sizeof(*wmi) + ilen);

	wmi = (struct ar_wmi_cmd_hdr *)&htc[1];
	wmi->cmd_id = htobe16(cmd_id);
	usc->usc_wmi_seq_no++;
	wmi->seq_no = htobe16(usc->usc_wmi_seq_no);

	memcpy(&wmi[1], ibuf, ilen);

	usbd_setup_xfer(data->xfer, usc->usc_tx_intr_pipe, usc, data->buf,
	    sizeof(*htc) + sizeof(*wmi) + ilen,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY, ATHN_USB_CMD_TIMEOUT,
	    athn_usb_wmieof);

	s = splusb();
	usc->usc_wmi_done = 0;
	usc->usc_wait_cmd_id = cmd_id;
	error = usbd_transfer(data->xfer);
	if (__predict_true(error == 0 || error == USBD_IN_PROGRESS)) {
		usc->usc_obuf = obuf;

		/* Wait for WMI command to complete. */
		error = tsleep(&usc->usc_wait_cmd_id, 0, "athnwmi", hz);
		usc->usc_wait_cmd_id = 0;
		athn_usb_wait_wmi(usc);
	}
	splx(s);
	return error;
}

Static void
athn_usb_wait_wmi(struct athn_usb_softc *usc)
{

	DPRINTFN(DBG_FN, usc, "\n");

	while (__predict_false(!usc->usc_wmi_done))
		tsleep(&usc->usc_wmi_done, 0, "athnwmi", 0);
}

#ifdef unused
Static int
athn_usb_read_rom(struct athn_softc *sc)
{
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	uint32_t addrs[8], vals[8], addr;
	uint16_t *eep;
	size_t i, j;
	int error = 0;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Read EEPROM by blocks of 16 bytes. */
	eep = sc->sc_eep;
	addr = AR_EEPROM_OFFSET(sc->sc_eep_base);
	for (i = 0; i < sc->sc_eep_size / 16; i++) {
		for (j = 0; j < 8; j++, addr += 4)
			addrs[j] = htobe32(addr);
		error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_REG_READ,
		    addrs, sizeof(addrs), vals);
		if (error != 0)
			break;
		for (j = 0; j < 8; j++)
			*eep++ = be32toh(vals[j]);
	}
	return error;
}
#endif /* unused */

Static uint32_t
athn_usb_read(struct athn_softc *sc, uint32_t addr)
{
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	uint32_t val;
	int error;

	if (usc->usc_dying)
		return 0;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Flush pending writes for strict consistency. */
	athn_usb_write_barrier(sc);

	addr = htobe32(addr);
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_REG_READ,
	    &addr, sizeof(addr), &val);
	if (error != 0)
		return 0xdeadbeef;
	return be32toh(val);
}

Static void
athn_usb_write(struct athn_softc *sc, uint32_t addr, uint32_t val)
{
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, sc, "\n");

	usc->usc_wbuf[usc->usc_wcount].addr = htobe32(addr);
	usc->usc_wbuf[usc->usc_wcount].val  = htobe32(val);
	if (++usc->usc_wcount == AR_MAX_WRITE_COUNT)
		athn_usb_write_barrier(sc);
}

Static void
athn_usb_write_barrier(struct athn_softc *sc)
{
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);

	if (usc->usc_dying)
		goto done;

	DPRINTFN(DBG_FN, sc, "\n");

	if (usc->usc_wcount == 0)
		return;

	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_REG_WRITE,
	    usc->usc_wbuf, usc->usc_wcount * sizeof(usc->usc_wbuf[0]), NULL);
 done:
	usc->usc_wcount = 0;	/* Always flush buffer. */
}

Static int
athn_usb_media_change(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	int error;

	if (usc->usc_dying)
		return EIO;

	DPRINTFN(DBG_FN, sc, "\n");

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET && IS_UP_AND_RUNNING(ifp)) {
		athn_usb_stop(ifp);
		error = athn_usb_init(ifp);
	}
	return error;
}

Static int
athn_usb_newstate(struct ieee80211com *ic, enum ieee80211_state nstate,
    int arg)
{
	struct athn_softc *sc = ic->ic_ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct athn_usb_cmd_newstate cmd;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Do it in a process context. */
	cmd.state = nstate;
	cmd.arg = arg;
	athn_usb_do_async(usc, athn_usb_newstate_cb, &cmd, sizeof(cmd));
	return 0;
}

Static void
athn_usb_newstate_cb(struct athn_usb_softc *usc, void *arg)
{
	struct athn_usb_cmd_newstate *cmd = arg;
	struct athn_softc *sc = &usc->usc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate, nstate;
	uint32_t reg, imask;
	int s;

	DPRINTFN(DBG_FN, sc, "\n");

	callout_stop(&sc->sc_calib_to);

	s = splnet();

	ostate = ic->ic_state;
	nstate = cmd->state;
	DPRINTFN(DBG_STM, usc, "newstate %s(%d) -> %s(%d)\n",
		    ieee80211_state_name[ostate], ostate,
		    ieee80211_state_name[nstate], nstate);

	if (ostate == IEEE80211_S_RUN) {
		uint8_t sta_index;

		sta_index = ATHN_NODE(ic->ic_bss)->sta_index;
		DPRINTFN(DBG_NODES, usc, "removing node %u\n", sta_index);
		athn_usb_remove_hw_node(usc, &sta_index);
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		athn_set_led(sc, 0);
		break;
	case IEEE80211_S_SCAN:
		/* Make the LED blink while scanning. */
		athn_set_led(sc, !sc->sc_led_state);
		(void)athn_usb_switch_chan(sc, ic->ic_curchan, NULL);
		if (!usc->usc_dying)
			callout_schedule(&sc->sc_scan_to, hz / 5);
		break;
	case IEEE80211_S_AUTH:
		athn_set_led(sc, 0);
		athn_usb_switch_chan(sc, ic->ic_curchan, NULL);
		break;
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_RUN:
		athn_set_led(sc, 1);

		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			break;

		/* Create node entry for our BSS. */
		DPRINTFN(DBG_NODES, sc, "create node for AID=0x%x\n",
		    ic->ic_bss->ni_associd);
		athn_usb_create_node(usc, ic->ic_bss);	/* XXX: handle error? */

		athn_set_bss(sc, ic->ic_bss);
		athn_usb_wmi_cmd(usc, AR_WMI_CMD_DISABLE_INTR);
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			athn_set_hostap_timers(sc);
			/* Enable software beacon alert interrupts. */
			imask = htobe32(AR_IMR_SWBA);
		}
		else
#endif
		{
			athn_set_sta_timers(sc);
			/* Enable beacon miss interrupts. */
			imask = htobe32(AR_IMR_BMISS);

			/* Stop receiving beacons from other BSS. */
			reg = AR_READ(sc, AR_RX_FILTER);
			reg = (reg & ~AR_RX_FILTER_BEACON) |
			    AR_RX_FILTER_MYBEACON;
			AR_WRITE(sc, AR_RX_FILTER, reg);
			AR_WRITE_BARRIER(sc);
		}
		athn_usb_wmi_xcmd(usc, AR_WMI_CMD_ENABLE_INTR,
		    &imask, sizeof(imask), NULL);
		break;
	}
	if (!usc->usc_dying)
		(void)sc->sc_newstate(ic, nstate, cmd->arg);
	splx(s);
}

Static void
athn_usb_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct athn_softc *sc = ic->ic_ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);

	DPRINTFN(DBG_FN, sc, "\n");

	if (ic->ic_opmode != IEEE80211_M_HOSTAP || !isnew)
		return;

	/* Do it in a process context. */
	ieee80211_ref_node(ni);
	athn_usb_do_async(usc, athn_usb_newassoc_cb, &ni, sizeof(ni));
}

Static void
athn_usb_newassoc_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211_node *ni = *(void **)arg;
	int s;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splnet();
	/* NB: Node may have left before we got scheduled. */
	if (ni->ni_associd != 0) {
		DPRINTFN(DBG_NODES, usc, "creating node for AID=0x%x\n",
		    ni->ni_associd);
		(void)athn_usb_create_node(usc, ni);	/* XXX: handle error? */
	}
	ieee80211_free_node(ni);
	splx(s);
}

#ifdef notyet
Static int
athn_usb_ampdu_tx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct athn_softc *sc = ic->ic_ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct athn_node *an = ATHN_NODE(ni);
	struct athn_usb_aggr_cmd cmd;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Do it in a process context. */
	cmd.sta_index = an->sta_index;
	cmd.tid = tid;
	athn_usb_do_async(usc, athn_usb_ampdu_tx_start_cb, &cmd, sizeof(cmd));
	return 0;
}

Static void
athn_usb_ampdu_tx_start_cb(struct athn_usb_softc *usc, void *arg)
{
	struct athn_usb_aggr_cmd *cmd = arg;
	struct ar_htc_target_aggr aggr;

	DPRINTFN(DBG_FN, usc, "\n");

	memset(&aggr, 0, sizeof(aggr));
	aggr.sta_index = cmd->sta_index;
	aggr.tidno = cmd->tid;
	aggr.aggr_enable = 1;
	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_TX_AGGR_ENABLE,
	    &aggr, sizeof(aggr), NULL);
}

Static void
athn_usb_ampdu_tx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct athn_softc *sc = ic->ic_ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct athn_node *an = ATHN_NODE(ni);
	struct athn_usb_aggr_cmd cmd;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Do it in a process context. */
	cmd.sta_index = an->sta_index;
	cmd.tid = tid;
	athn_usb_do_async(usc, athn_usb_ampdu_tx_stop_cb, &cmd, sizeof(cmd));
}

Static void
athn_usb_ampdu_tx_stop_cb(struct athn_usb_softc *usc, void *arg)
{
	struct athn_usb_aggr_cmd *cmd = arg;
	struct ar_htc_target_aggr aggr;

	DPRINTFN(DBG_FN, usc, "\n");

	memset(&aggr, 0, sizeof(aggr));
	aggr.sta_index = cmd->sta_index;
	aggr.tidno = cmd->tid;
	aggr.aggr_enable = 0;
	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_TX_AGGR_ENABLE,
	    &aggr, sizeof(aggr), NULL);
}
#endif /* notyet */

Static int
athn_usb_remove_hw_node(struct athn_usb_softc *usc, uint8_t *sta_idx)
{
	int error;

	DPRINTFN(DBG_FN, usc, "\n");

	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_NODE_REMOVE,
	    sta_idx, sizeof(*sta_idx), NULL);

	DPRINTFN(DBG_NODES, usc, "node=%u error=%d\n",
	    *sta_idx, error);
	return error;
}

Static int
athn_usb_create_hw_node(struct athn_usb_softc *usc,
    struct ar_htc_target_sta *sta)
{
	int error;

	DPRINTFN(DBG_FN, usc, "\n");

	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_NODE_CREATE,
	    sta, sizeof(*sta), NULL);

	DPRINTFN(DBG_NODES, usc, "node=%u error=%d\n",
	    sta->sta_index, error);

	return error;
}

Static int
athn_usb_create_node(struct athn_usb_softc *usc, struct ieee80211_node *ni)
{
	struct athn_node *an = ATHN_NODE(ni);
	struct ar_htc_target_sta sta;
	struct ar_htc_target_rate rate;
	int error;

	DPRINTFN(DBG_FN | DBG_NODES, usc, "AID=0x%x\n", ni->ni_associd);

	/*
	 * NB: this is called by ic_newstate and (in HOSTAP mode by)
	 * ic_newassoc.
	 *
	 * The firmware has a limit of 8 nodes.  In HOSTAP mode, we
	 * limit the AID to < 8 and use that value to index the
	 * firmware node table.  Node zero is used for the BSS.
	 *
	 * In STA mode, we simply use node 1 for the BSS.
	 */
	if (ATHN_SOFTC(usc)->sc_ic.ic_opmode == IEEE80211_M_HOSTAP)
		an->sta_index = IEEE80211_NODE_AID(ni);
	else
		an->sta_index = 1;

	/* Create node entry on target. */
	memset(&sta, 0, sizeof(sta));
	IEEE80211_ADDR_COPY(sta.macaddr, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(sta.bssid, ni->ni_bssid);

	sta.associd = htobe16(ni->ni_associd);
	sta.valid = 1;
	sta.sta_index = an->sta_index;

	sta.maxampdu = 0xffff;
#ifndef IEEE80211_NO_HT
	if (ni->ni_flags & IEEE80211_NODE_HT)
		sta.flags |= htobe16(AR_HTC_STA_HT);
#endif
	error = athn_usb_create_hw_node(usc, &sta);
	if (error)
		return error;

	/* Setup supported rates. */
	memset(&rate, 0, sizeof(rate));
	rate.sta_index = sta.sta_index;
	rate.isnew = 1;
	rate.lg_rates.rs_nrates = ni->ni_rates.rs_nrates;
	memcpy(rate.lg_rates.rs_rates, ni->ni_rates.rs_rates,
	    ni->ni_rates.rs_nrates);

#ifndef IEEE80211_NO_HT
	if (ni->ni_flags & IEEE80211_NODE_HT) {
		rate.capflags |= htobe32(AR_RC_HT_FLAG);
#ifdef notyet
		/* XXX setup HT rates */
		if (ni->ni_htcaps & IEEE80211_HTCAP_CBW20_40)
			rate.capflags |= htobe32(AR_RC_40_FLAG);
		if (ni->ni_htcaps & IEEE80211_HTCAP_SGI40)
			rate.capflags |= htobe32(AR_RC_SGI_FLAG);
		if (ni->ni_htcaps & IEEE80211_HTCAP_SGI20)
			rate.capflags |= htobe32(AR_RC_SGI_FLAG);
#endif
	}
#endif
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_RC_RATE_UPDATE,
	    &rate, sizeof(rate), NULL);
	return error;
}

Static void
athn_usb_rx_enable(struct athn_softc *sc)
{

	DPRINTFN(DBG_FN, sc, "\n");

	AR_WRITE(sc, AR_CR, AR_CR_RXE);
	AR_WRITE_BARRIER(sc);
}

Static int
athn_usb_switch_chan(struct athn_softc *sc, struct ieee80211_channel *curchan,
    struct ieee80211_channel *extchan)
{
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	uint16_t mode;
	int error;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Disable interrupts. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_DISABLE_INTR);
	if (error != 0)
		goto reset;
	/* Stop all Tx queues. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_DRAIN_TXQ_ALL);
	if (error != 0)
		goto reset;
	/* Stop Rx. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_STOP_RECV);
	if (error != 0)
		goto reset;

	/* If band or bandwidth changes, we need to do a full reset. */
	if (curchan->ic_flags != sc->sc_curchan->ic_flags ||
	    ((extchan != NULL) ^ (sc->sc_curchanext != NULL))) {
		DPRINTFN(DBG_RF, sc, "channel band switch\n");
		goto reset;
	}

	error = athn_set_chan(sc, curchan, extchan);
	if (AR_SREV_9271(sc) && error == 0)
		ar9271_load_ani(sc);
	if (error != 0) {
 reset:		/* Error found, try a full reset. */
		DPRINTFN(DBG_RF, sc, "needs a full reset\n");
		error = athn_hw_reset(sc, curchan, extchan, 0);
		if (error != 0)	/* Hopeless case. */
			return error;
	}

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_START_RECV);
	if (error != 0)
		return error;
	athn_rx_start(sc);

	mode = htobe16(IEEE80211_IS_CHAN_2GHZ(curchan) ?
	    AR_HTC_MODE_11NG : AR_HTC_MODE_11NA);
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_SET_MODE,
	    &mode, sizeof(mode), NULL);
	if (error != 0)
		return error;

	/* Re-enable interrupts. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_ENABLE_INTR);
	return error;
}

#ifdef notyet_edca
Static void
athn_usb_updateedca(struct ieee80211com *ic)
{
	struct athn_softc *sc = ic->ic_ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);

	DPRINTFN(DBG_FN, sc, "\n");

	/* Do it in a process context. */
	athn_usb_do_async(usc, athn_usb_updateedca_cb, NULL, 0);
}

Static void
athn_usb_updateedca_cb(struct athn_usb_softc *usc, void *arg)
{
	int s;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splnet();
	athn_updateedca(&usc->usc_sc.sc_ic);
	splx(s);
}
#endif /* notyet_edca */

Static void
athn_usb_updateslot(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);

	DPRINTFN(DBG_FN, sc, "\n");

	/*
	 * NB: athn_updateslog() needs to be done in a process context
	 * to avoid being called by ieee80211_reset_erp() inside a
	 * spinlock held by ieee80211_free_allnodes().
	 *
	 * XXX: calling this during the athn_attach() causes
	 * usb_insert_transfer() to produce a bunch of "not busy"
	 * messages.  Why?
	 */
	if (usc->usc_athn_attached)
		athn_usb_do_async(usc, athn_usb_updateslot_cb, NULL, 0);
}

Static void
athn_usb_updateslot_cb(struct athn_usb_softc *usc, void *arg)
{
	int s;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splnet();
	athn_updateslot(&usc->usc_sc.sc_if);
	splx(s);
}

#ifdef notyet
Static int
athn_usb_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct athn_softc *sc = ic->ic_ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct ifnet *ifp = &usc->usc_sc.sc_if;
	struct athn_usb_cmd_key cmd;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Defer setting of WEP keys until interface is brought up. */
	if (!IS_UP_AND_RUNNING(ifp))
		return 0;

	/* Do it in a process context. */
	cmd.ni = (ni != NULL) ? ieee80211_ref_node(ni) : NULL;
	cmd.key = k;
	athn_usb_do_async(usc, athn_usb_set_key_cb, &cmd, sizeof(cmd));
	return 0;
}

Static void
athn_usb_set_key_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211com *ic = &usc->usc_sc.sc_ic;
	struct athn_usb_cmd_key *cmd = arg;
	int s;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splnet();
	athn_set_key(ic, cmd->ni, cmd->key);
	if (cmd->ni != NULL)
		ieee80211_free_node(cmd->ni);
	splx(s);
}

Static void
athn_usb_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct athn_softc *sc = ic->ic_ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct ifnet *ifp = &usc->usc_sc.sc_if;
	struct athn_usb_cmd_key cmd;

	DPRINTFN(DBG_FN, sc, "\n");

	if (!(ifp->if_flags & IFF_RUNNING) ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;	/* Nothing to do. */

	/* Do it in a process context. */
	cmd.ni = (ni != NULL) ? ieee80211_ref_node(ni) : NULL;
	cmd.key = k;
	athn_usb_do_async(usc, athn_usb_delete_key_cb, &cmd, sizeof(cmd));
}

Static void
athn_usb_delete_key_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211com *ic = &usc->usc_sc.sc_ic;
	struct athn_usb_cmd_key *cmd = arg;
	int s;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splnet();
	athn_delete_key(ic, cmd->ni, cmd->key);
	if (cmd->ni != NULL)
		ieee80211_free_node(cmd->ni);
	splx(s);
}
#endif /* notyet */

#ifndef IEEE80211_STA_ONLY
Static void
athn_usb_bcneof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct athn_usb_tx_data *data = priv;
	struct athn_usb_softc *usc = data->sc;

	DPRINTFN(DBG_FN, usc, "\n");

	if (__predict_false(status == USBD_STALLED))
		usbd_clear_endpoint_stall_async(usc->usc_tx_data_pipe);
	usc->usc_tx_bcn = data;
}

/*
 * Process Software Beacon Alert interrupts.
 */
Static void
athn_usb_swba(struct athn_usb_softc *usc)
{
	struct athn_softc *sc = &usc->usc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct athn_usb_tx_data *data;
	struct ieee80211_frame *wh;
	struct ieee80211_beacon_offsets bo;
	struct ar_stream_hdr *hdr;
	struct ar_htc_frame_hdr *htc;
	struct ar_tx_bcn *bcn;
	struct mbuf *m;
	int error;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, sc, "\n");

	if (ic->ic_dtim_count == 0)
		ic->ic_dtim_count = ic->ic_dtim_period - 1;
	else
		ic->ic_dtim_count--;

	/* Make sure previous beacon has been sent. */
	if (usc->usc_tx_bcn == NULL)
		return;
	data = usc->usc_tx_bcn;

	/* Get new beacon. */
#ifdef ATHN_DEBUG
	memset(&bo, 0, sizeof(bo));
#endif
	m = ieee80211_beacon_alloc(ic, ic->ic_bss, &bo);
	if (__predict_false(m == NULL))
		return;
	/* Assign sequence number. */
	/* XXX: use non-QoS tid? */
	wh = mtod(m, struct ieee80211_frame *);
	*(uint16_t *)&wh->i_seq[0] =
	    htole16(ic->ic_bss->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
	ic->ic_bss->ni_txseqs[0]++;

	hdr = (struct ar_stream_hdr *)data->buf;
	hdr->tag = htole16(AR_USB_TX_STREAM_TAG);
	hdr->len = htole16(sizeof(*htc) + sizeof(*bcn) + m->m_pkthdr.len);

	htc = (struct ar_htc_frame_hdr *)&hdr[1];
	memset(htc, 0, sizeof(*htc));
	htc->endpoint_id = usc->usc_ep_bcn;
	htc->payload_len = htobe16(sizeof(*bcn) + m->m_pkthdr.len);

	bcn = (struct ar_tx_bcn *)&htc[1];
	memset(bcn, 0, sizeof(*bcn));
	bcn->vif_idx = 0;

	m_copydata(m, 0, m->m_pkthdr.len, (void *)&bcn[1]);

	usbd_setup_xfer(data->xfer, usc->usc_tx_data_pipe, data, data->buf,
	    sizeof(*hdr) + sizeof(*htc) + sizeof(*bcn) + m->m_pkthdr.len,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY, ATHN_USB_TX_TIMEOUT,
	    athn_usb_bcneof);

	m_freem(m);
	usc->usc_tx_bcn = NULL;
	error = usbd_transfer(data->xfer);
	if (__predict_false(error != USBD_IN_PROGRESS && error != 0))
		usc->usc_tx_bcn = data;
}
#endif

Static void
athn_usb_rx_wmi_ctrl(struct athn_usb_softc *usc, uint8_t *buf, size_t len)
{
#ifdef ATHN_DEBUG
	struct ar_wmi_evt_txrate *txrate;
#endif
	struct ar_wmi_cmd_hdr *wmi;
	uint16_t cmd_id;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, usc, "\n");

	if (__predict_false(len < sizeof(*wmi)))
		return;
	wmi = (struct ar_wmi_cmd_hdr *)buf;
	cmd_id = be16toh(wmi->cmd_id);

	if (!(cmd_id & AR_WMI_EVT_FLAG)) {
		if (usc->usc_wait_cmd_id != cmd_id)
			return;	/* Unexpected reply. */
		if (usc->usc_obuf != NULL) {
			/* Copy answer into caller supplied buffer. */
			memcpy(usc->usc_obuf, &wmi[1], len - sizeof(*wmi));
		}
		/* Notify caller of completion. */
		usc->usc_wait_cmd_id = 0;
		wakeup(&usc->usc_wait_cmd_id);
		return;
	}
	/*
	 * XXX: the Linux 2.6 and 3.7.4 kernels differ on the event numbers!
	 * See the alternate defines in if_athn_usb.h.
	 */
	switch (cmd_id & 0xfff) {
#ifndef IEEE80211_STA_ONLY
	case AR_WMI_EVT_SWBA:
		athn_usb_swba(usc);
		break;
#endif
	case AR_WMI_EVT_FATAL:
		aprint_error_dev(usc->usc_dev, "fatal firmware error\n");
		break;
	case AR_WMI_EVT_TXRATE:
#ifdef ATHN_DEBUG
		txrate = (struct ar_wmi_evt_txrate *)&wmi[1];
		DPRINTFN(DBG_TX, usc, "txrate=%d\n", be32toh(txrate->txrate));
#endif
		break;
	default:
		DPRINTFN(DBG_TX, usc, "WMI event 0x%x (%d) ignored\n", cmd_id, cmd_id);
		break;
	}
}

Static void
athn_usb_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct athn_usb_softc *usc = priv;
	struct ar_htc_frame_hdr *htc;
	struct ar_htc_msg_hdr *msg;
	uint8_t *buf = usc->usc_ibuf;
	uint16_t msg_id;
	int len;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, usc, "\n");

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTFN(DBG_INTR, usc, "intr status=%d\n", status);
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(usc->usc_rx_intr_pipe);
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	/* Skip watchdog pattern if present. */
	if (len >= 4 && *(uint32_t *)buf == htobe32(0x00c60000)) {
		buf += 4;
		len -= 4;
	}
	if (__predict_false(len < (int)sizeof(*htc)))
		return;
	htc = (struct ar_htc_frame_hdr *)buf;
	/* Skip HTC header. */
	buf += sizeof(*htc);
	len -= sizeof(*htc);

	if (htc->endpoint_id != 0) {
		if (__predict_false(htc->endpoint_id != usc->usc_ep_ctrl))
			return;
		/* Remove trailer if present. */
		if (htc->flags & AR_HTC_FLAG_TRAILER) {
			if (__predict_false(len < htc->control[0]))
				return;
			len -= htc->control[0];
		}
		athn_usb_rx_wmi_ctrl(usc, buf, len);
		return;
	}

	/*
	 * Endpoint 0 carries HTC messages.
	 */
	if (__predict_false(len < (int)sizeof(*msg)))
		return;
	msg = (struct ar_htc_msg_hdr *)buf;
	msg_id = be16toh(msg->msg_id);
	DPRINTFN(DBG_RX, usc, "Rx HTC message %d\n", msg_id);
	switch (msg_id) {
	case AR_HTC_MSG_READY:
	case AR_HTC_MSG_CONF_PIPE_RSP:
		if (usc->usc_wait_msg_id != msg_id)
			break;
		usc->usc_wait_msg_id = 0;
		wakeup(&usc->usc_wait_msg_id);
		break;
	case AR_HTC_MSG_CONN_SVC_RSP:
		if (usc->usc_wait_msg_id != msg_id)
			break;
		if (usc->usc_msg_conn_svc_rsp != NULL) {
			memcpy(usc->usc_msg_conn_svc_rsp, &msg[1],
			    sizeof(*usc->usc_msg_conn_svc_rsp));
		}
		usc->usc_wait_msg_id = 0;
		wakeup(&usc->usc_wait_msg_id);
		break;
	default:
		DPRINTFN(DBG_RX, usc, "HTC message %d ignored\n", msg_id);
		break;
	}
}

Static void
athn_usb_rx_radiotap(struct athn_softc *sc, struct mbuf *m,
    struct ar_rx_status *rs)
{
	struct athn_rx_radiotap_header *tap = &sc->sc_rxtap;
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t rate;

	DPRINTFN(DBG_FN, sc, "\n");

	tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
	tap->wr_tsft = htole64(be64toh(rs->rs_tstamp));
	tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
	tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
	tap->wr_dbm_antsignal = rs->rs_rssi;
	/* XXX noise. */
	tap->wr_antenna = rs->rs_antenna;
	rate = rs->rs_rate;
	if (rate & 0x80) {		/* HT. */
		/* Bit 7 set means HT MCS instead of rate. */
		tap->wr_rate = rate;
		if (!(rs->rs_flags & AR_RXS_FLAG_GI))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTGI;
	}
	else if (rate & 0x10) {	/* CCK. */
		if (rate & 0x04)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		switch (rate & ~0x14) {
		case 0xb: tap->wr_rate =   2; break;
		case 0xa: tap->wr_rate =   4; break;
		case 0x9: tap->wr_rate =  11; break;
		case 0x8: tap->wr_rate =  22; break;
		default:  tap->wr_rate =   0; break;
		}
	}
	else {			/* OFDM. */
		switch (rate) {
		case 0xb: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0xa: tap->wr_rate =  24; break;
		case 0xe: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xd: tap->wr_rate =  72; break;
		case 0x8: tap->wr_rate =  96; break;
		case 0xc: tap->wr_rate = 108; break;
		default:  tap->wr_rate =   0; break;
		}
	}
	bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
}

Static void
athn_usb_rx_frame(struct athn_usb_softc *usc, struct mbuf *m)
{
	struct athn_softc *sc = &usc->usc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct ar_htc_frame_hdr *htc;
	struct ar_rx_status *rs;
	uint16_t datalen;
	int s;

	DPRINTFN(DBG_FN, sc, "\n");

	if (__predict_false(m->m_len < (int)sizeof(*htc)))
		goto skip;
	htc = mtod(m, struct ar_htc_frame_hdr *);
	if (__predict_false(htc->endpoint_id == 0)) {
		DPRINTFN(DBG_RX, sc, "bad endpoint %d\n", htc->endpoint_id);
		goto skip;
	}
	if (htc->flags & AR_HTC_FLAG_TRAILER) {
		if (m->m_len < htc->control[0])
			goto skip;
		m_adj(m, -(int)htc->control[0]);
	}
	m_adj(m, sizeof(*htc));	/* Strip HTC header. */

	if (__predict_false(m->m_len < (int)sizeof(*rs)))
		goto skip;
	rs = mtod(m, struct ar_rx_status *);

	/* Make sure that payload fits. */
	datalen = be16toh(rs->rs_datalen);
	if (__predict_false(m->m_len < (int)sizeof(*rs) + datalen))
		goto skip;

	/* Ignore runt frames.  Let ACKs be seen by bpf */
	if (__predict_false(datalen <
		sizeof(struct ieee80211_frame_ack) + IEEE80211_CRC_LEN))
		goto skip;

	m_adj(m, sizeof(*rs));	/* Strip Rx status. */
	m->m_pkthdr.rcvif = ifp;

	s = splnet();

	/* Grab a reference to the source node. */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* Remove any HW padding after the 802.11 header. */
	if (!(wh->i_fc[0] & IEEE80211_FC0_TYPE_CTL)) {
		u_int hdrlen = ieee80211_anyhdrsize(wh);
		if (hdrlen & 3) {
			ovbcopy(wh, (uint8_t *)wh + 2, hdrlen);
			m_adj(m, 2);
		}
	}
	if (__predict_false(sc->sc_drvbpf != NULL))
		athn_usb_rx_radiotap(sc, m, rs);

	/* Trim 802.11 FCS after radiotap. */
	m_adj(m, -IEEE80211_CRC_LEN);

	/* Send the frame to the 802.11 layer. */
	ieee80211_input(ic, m, ni, rs->rs_rssi + AR_USB_DEFAULT_NF, 0);

	/* Node is no longer needed. */
	ieee80211_free_node(ni);
	splx(s);
	return;
 skip:
	m_freem(m);
}

Static void
athn_usb_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct athn_usb_rx_data *data = priv;
	struct athn_usb_softc *usc = data->sc;
	struct athn_usb_rx_stream *stream = &usc->usc_rx_stream;
	uint8_t *buf = data->buf;
	struct ar_stream_hdr *hdr;
	struct mbuf *m;
	uint16_t pktlen;
	int off, len;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, usc, "\n");

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTFN(DBG_RX, usc, "RX status=%d\n", status);
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(usc->usc_rx_data_pipe);
		if (status != USBD_CANCELLED)
			goto resubmit;
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (stream->left > 0) {
		if (len >= stream->left) {
			/* We have all our pktlen bytes now. */
			if (__predict_true(stream->m != NULL)) {
				memcpy(mtod(stream->m, uint8_t *) +
				    stream->moff, buf, stream->left);
				athn_usb_rx_frame(usc, stream->m);
				stream->m = NULL;
			}
			/* Next header is 32-bit aligned. */
			off = (stream->left + 3) & ~3;
			buf += off;
			len -= off;
			stream->left = 0;
		}
		else {
			/* Still need more bytes, save what we have. */
			if (__predict_true(stream->m != NULL)) {
				memcpy(mtod(stream->m, uint8_t *) +
				    stream->moff, buf, len);
				stream->moff += len;
			}
			stream->left -= len;
			goto resubmit;
		}
	}
	KASSERT(stream->left == 0);
	while (len >= (int)sizeof(*hdr)) {
		hdr = (struct ar_stream_hdr *)buf;
		if (hdr->tag != htole16(AR_USB_RX_STREAM_TAG)) {
			DPRINTFN(DBG_RX, usc, "invalid tag 0x%x\n", hdr->tag);
			break;
		}
		pktlen = le16toh(hdr->len);
		buf += sizeof(*hdr);
		len -= sizeof(*hdr);

		if (__predict_true(pktlen <= MCLBYTES)) {
			/* Allocate an mbuf to store the next pktlen bytes. */
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (__predict_true(m != NULL)) {
				m->m_pkthdr.len = m->m_len = pktlen;
				if (pktlen > MHLEN) {
					MCLGET(m, M_DONTWAIT);
					if (!(m->m_flags & M_EXT)) {
						m_free(m);
						m = NULL;
					}
				}
			}
		}
		else	/* Drop frames larger than MCLBYTES. */
			m = NULL;
		/*
		 * NB: m can be NULL, in which case the next pktlen bytes
		 * will be discarded from the Rx stream.
		 */
		if (pktlen > len) {
			/* Need more bytes, save what we have. */
			stream->m = m;	/* NB: m can be NULL. */
			if (__predict_true(stream->m != NULL)) {
				memcpy(mtod(stream->m, uint8_t *), buf, len);
				stream->moff = len;
			}
			stream->left = pktlen - len;
			goto resubmit;
		}
		if (__predict_true(m != NULL)) {
			/* We have all the pktlen bytes in this xfer. */
			memcpy(mtod(m, uint8_t *), buf, pktlen);
			athn_usb_rx_frame(usc, m);
		}

		/* Next header is 32-bit aligned. */
		off = (pktlen + 3) & ~3;
		buf += off;
		len -= off;
	}

 resubmit:
	/* Setup a new transfer. */
	usbd_setup_xfer(xfer, usc->usc_rx_data_pipe, data, data->buf,
	    ATHN_USB_RXBUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, athn_usb_rxeof);
	(void)usbd_transfer(xfer);
}

Static void
athn_usb_txeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct athn_usb_tx_data *data = priv;
	struct athn_usb_softc *usc = data->sc;
	struct athn_softc *sc = &usc->usc_sc;
	struct ifnet *ifp = &sc->sc_if;
	int s;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, usc, "\n");

	s = splnet();
	/* Put this Tx buffer back to our free list. */
	mutex_enter(&usc->usc_tx_mtx);
	TAILQ_INSERT_TAIL(&usc->usc_tx_free_list, data, next);
	mutex_exit(&usc->usc_tx_mtx);

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTFN(DBG_TX, sc, "TX status=%d\n", status);
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(usc->usc_tx_data_pipe);
		ifp->if_oerrors++;
		splx(s);
		/* XXX Why return? */
		return;
	}
	sc->sc_tx_timer = 0;
	ifp->if_opackets++;

	/* We just released a Tx buffer, notify Tx. */
	if (ifp->if_flags & IFF_OACTIVE) {
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_start(ifp);
	}
	splx(s);
}

Static int
athn_usb_tx(struct athn_softc *sc, struct mbuf *m, struct ieee80211_node *ni,
    	struct athn_usb_tx_data *data)
{
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct athn_node *an = ATHN_NODE(ni);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct ar_stream_hdr *hdr;
	struct ar_htc_frame_hdr *htc;
	struct ar_tx_frame *txf;
	struct ar_tx_mgmt *txm;
	uint8_t *frm;
	uint8_t sta_index, qid, tid;
	int error, s, xferlen;

	DPRINTFN(DBG_FN, sc, "\n");

	wh = mtod(m, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_crypto_encap(ic, ni, m);
		if (k == NULL)
			return ENOBUFS;

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m, struct ieee80211_frame *);
	}
#ifdef notyet_edca
	if (ieee80211_has_qos(wh)) {
		uint16_t qos;

		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
		qid = ieee80211_up_to_ac(ic, tid);
	}
	else
#endif /* notyet_edca */
	{
		tid = 0;
		qid = WME_AC_BE;
	}

	/* XXX Change radiotap Tx header for USB (no txrate). */
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct athn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m);
	}
	sta_index = an->sta_index;

	/* NB: We don't take advantage of USB Tx stream mode for now. */
	hdr = (struct ar_stream_hdr *)data->buf;
	hdr->tag = htole16(AR_USB_TX_STREAM_TAG);

	htc = (struct ar_htc_frame_hdr *)&hdr[1];
	memset(htc, 0, sizeof(*htc));
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_DATA) {
		htc->endpoint_id = usc->usc_ep_data[qid];

		txf = (struct ar_tx_frame *)&htc[1];
		memset(txf, 0, sizeof(*txf));
		txf->data_type = AR_HTC_NORMAL;
		txf->node_idx = sta_index;
		txf->vif_idx = 0;
		txf->tid = tid;
		if (m->m_pkthdr.len + IEEE80211_CRC_LEN > ic->ic_rtsthreshold)
			txf->flags |= htobe32(AR_HTC_TX_RTSCTS);
		else if (ic->ic_flags & IEEE80211_F_USEPROT) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				txf->flags |= htobe32(AR_HTC_TX_CTSONLY);
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				txf->flags |= htobe32(AR_HTC_TX_RTSCTS);
		}
		txf->key_idx = 0xff;
		frm = (uint8_t *)&txf[1];
	}
	else {
		htc->endpoint_id = usc->usc_ep_mgmt;

		txm = (struct ar_tx_mgmt *)&htc[1];
		memset(txm, 0, sizeof(*txm));
		txm->node_idx = sta_index;
		txm->vif_idx = 0;
		txm->key_idx = 0xff;
		frm = (uint8_t *)&txm[1];
	}
	/* Copy payload. */
	m_copydata(m, 0, m->m_pkthdr.len, (void *)frm);
	frm += m->m_pkthdr.len;

	/* Finalize headers. */
	htc->payload_len = htobe16(frm - (uint8_t *)&htc[1]);
	hdr->len = htole16(frm - (uint8_t *)&hdr[1]);
	xferlen = frm - data->buf;

	s = splnet();
	usbd_setup_xfer(data->xfer, usc->usc_tx_data_pipe, data, data->buf,
	    xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY, ATHN_USB_TX_TIMEOUT,
	    athn_usb_txeof);
	error = usbd_transfer(data->xfer);
	if (__predict_false(error != USBD_IN_PROGRESS && error != 0)) {
		splx(s);
		return error;
	}
	splx(s);
	return 0;
}

Static void
athn_usb_start(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct ieee80211com *ic = &sc->sc_ic;
	struct athn_usb_tx_data *data;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if (usc->usc_dying)
		return;

	DPRINTFN(DBG_FN, sc, "\n");

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	data = NULL;
	for (;;) {
		mutex_enter(&usc->usc_tx_mtx);
		if (data == NULL && !TAILQ_EMPTY(&usc->usc_tx_free_list)) {
			data = TAILQ_FIRST(&usc->usc_tx_free_list);
			TAILQ_REMOVE(&usc->usc_tx_free_list, data, next);
		}
		mutex_exit(&usc->usc_tx_mtx);

		if (data == NULL) {
			ifp->if_flags |= IFF_OACTIVE;
			return;
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

		if (athn_usb_tx(sc, m, ni, data) != 0) {
			m_freem(m);
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}
		data = NULL;
		m_freem(m);
		ieee80211_free_node(ni);
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}

	/* Return the Tx buffer to the free list */
	mutex_enter(&usc->usc_tx_mtx);
	TAILQ_INSERT_TAIL(&usc->usc_tx_free_list, data, next);
	mutex_exit(&usc->usc_tx_mtx);
}

Static void
athn_usb_watchdog(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "\n");

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			/* athn_usb_init(ifp); XXX needs a process context! */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}
	ieee80211_watchdog(&sc->sc_ic);
}

Static int
athn_usb_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	if (usc->usc_dying)
		return EIO;

	DPRINTFN(DBG_FN, sc, "cmd=0x%08lx\n", cmd);

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;

		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_UP | IFF_RUNNING:
			break;
		case IFF_UP:
			error = athn_usb_init(ifp);
			break;
		case IFF_RUNNING:
			athn_usb_stop(ifp);
			break;
		case 0:
		default:
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
			if (IS_UP_AND_RUNNING(ifp))
				athn_usb_switch_chan(sc, ic->ic_curchan, NULL);
			error = 0;
		}
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
		break;
	}
	if (error == ENETRESET) {
		error = 0;
		if (IS_UP_AND_RUNNING(ifp) &&
		    ic->ic_roaming != IEEE80211_ROAMING_MANUAL) {
			athn_usb_stop(ifp);
			error = athn_usb_init(ifp);
		}
	}
	splx(s);
	return error;
}

Static int
athn_usb_init(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct athn_ops *ops = &sc->sc_ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *curchan, *extchan;
	struct athn_usb_rx_data *data;
	struct ar_htc_target_vif hvif;
	struct ar_htc_target_sta sta;
	struct ar_htc_cap_target hic;
	uint16_t mode;
	size_t i;
	int error;

	if (usc->usc_dying)
		return USBD_CANCELLED;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Init host async commands ring. */
	mutex_spin_enter(&usc->usc_task_mtx);
	usc->usc_cmdq.cur = usc->usc_cmdq.next = usc->usc_cmdq.queued = 0;
	mutex_spin_exit(&usc->usc_task_mtx);

	/* Allocate Tx/Rx buffers. */
	error = athn_usb_alloc_rx_list(usc);
	if (error != 0)
		goto fail;
	error = athn_usb_alloc_tx_list(usc);
	if (error != 0)
		goto fail;
	/* Steal one buffer for beacons. */
	mutex_enter(&usc->usc_tx_mtx);
	usc->usc_tx_bcn = TAILQ_FIRST(&usc->usc_tx_free_list);
	TAILQ_REMOVE(&usc->usc_tx_free_list, usc->usc_tx_bcn, next);
	mutex_exit(&usc->usc_tx_mtx);

	curchan = ic->ic_curchan;
	extchan = NULL;

	/* In case a new MAC address has been configured. */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));

	error = athn_set_power_awake(sc);
	if (error != 0)
		goto fail;

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_FLUSH_RECV);
	if (error != 0)
		goto fail;

	error = athn_hw_reset(sc, curchan, extchan, 1);
	if (error != 0)
		goto fail;

	ops->set_txpower(sc, curchan, extchan);

	mode = htobe16(IEEE80211_IS_CHAN_2GHZ(curchan) ?
	    AR_HTC_MODE_11NG : AR_HTC_MODE_11NA);
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_SET_MODE,
	    &mode, sizeof(mode), NULL);
	if (error != 0)
		goto fail;

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_ATH_INIT);
	if (error != 0)
		goto fail;

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_START_RECV);
	if (error != 0)
		goto fail;

	athn_rx_start(sc);

	/* Create main interface on target. */
	memset(&hvif, 0, sizeof(hvif));
	hvif.index = 0;
	IEEE80211_ADDR_COPY(hvif.myaddr, ic->ic_myaddr);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		hvif.opmode = htobe32(AR_HTC_M_STA);
		break;
	case IEEE80211_M_MONITOR:
		hvif.opmode = htobe32(AR_HTC_M_MONITOR);
		break;
#ifndef IEEE80211_STA_ONLY
	case IEEE80211_M_IBSS:
		hvif.opmode = htobe32(AR_HTC_M_IBSS);
		break;
	case IEEE80211_M_AHDEMO:
		hvif.opmode = htobe32(AR_HTC_M_AHDEMO);
		break;
	case IEEE80211_M_HOSTAP:
		hvif.opmode = htobe32(AR_HTC_M_HOSTAP);
		break;
#endif
	}
	hvif.rtsthreshold = htobe16(ic->ic_rtsthreshold);
	DPRINTFN(DBG_INIT, sc, "creating VAP\n");
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_VAP_CREATE,
	    &hvif, sizeof(hvif), NULL);
	if (error != 0)
		goto fail;

	/* Create a fake node to send management frames before assoc. */
	memset(&sta, 0, sizeof(sta));
	IEEE80211_ADDR_COPY(sta.macaddr, ic->ic_myaddr);
	sta.sta_index = 0;
	sta.is_vif_sta = 1;
	sta.vif_index = hvif.index;
	sta.maxampdu = 0xffff;

	DPRINTFN(DBG_INIT | DBG_NODES, sc, "creating default node %u\n",
	    sta.sta_index);
	error = athn_usb_create_hw_node(usc, &sta);
	if (error != 0)
		goto fail;

	/* Update target capabilities. */
	memset(&hic, 0, sizeof(hic));
	hic.flags = htobe32(0x400c2400);
	hic.flags_ext = htobe32(0x00106080);
	hic.ampdu_limit = htobe32(0x0000ffff);
	hic.ampdu_subframes = 20;
	hic.protmode = 1;	/* XXX */
	hic.lg_txchainmask = sc->sc_txchainmask;
	hic.ht_txchainmask = sc->sc_txchainmask;
	DPRINTFN(DBG_INIT, sc, "updating target configuration\n");
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_TARGET_IC_UPDATE,
	    &hic, sizeof(hic), NULL);
	if (error != 0)
		goto fail;

	/* Queue Rx xfers. */
	for (i = 0; i < ATHN_USB_RX_LIST_COUNT; i++) {
		data = &usc->usc_rx_data[i];

		usbd_setup_xfer(data->xfer, usc->usc_rx_data_pipe, data, data->buf,
		    ATHN_USB_RXBUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, athn_usb_rxeof);
		error = usbd_transfer(data->xfer);
		if (error != 0 && error != USBD_IN_PROGRESS)
			goto fail;
	}
	/* We're ready to go. */
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

#ifdef notyet
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		/* Install WEP keys. */
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			athn_usb_set_key(ic, NULL, &ic->ic_nw_keys[i]);
	}
#endif
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		ic->ic_max_aid = AR_USB_MAX_STA;  /* Firmware is limited to 8 STA */
	else
		ic->ic_max_aid = sc->sc_max_aid;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	athn_usb_wait_async(usc);
	return 0;
 fail:
	athn_usb_stop(ifp);
	return error;
}

Static void
athn_usb_stop(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = ATHN_USB_SOFTC(sc);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ar_htc_target_vif hvif;
	struct mbuf *m;
	uint8_t sta_index;
	int s;

	DPRINTFN(DBG_FN, sc, "\n");

	s = splusb();
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	athn_usb_wait_async(usc);
	splx(s);

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	callout_stop(&sc->sc_scan_to);
	callout_stop(&sc->sc_calib_to);

	/* Abort Tx/Rx. */
	usbd_abort_pipe(usc->usc_tx_data_pipe);
	usbd_abort_pipe(usc->usc_rx_data_pipe);

	/* Free Tx/Rx buffers. */
	athn_usb_free_tx_list(usc);
	athn_usb_free_rx_list(usc);

	/* Flush Rx stream. */
	CTASSERT(sizeof(m) == sizeof(void *));
	m = atomic_swap_ptr(&usc->usc_rx_stream.m, NULL);
	m_freem(m);
	usc->usc_rx_stream.left = 0;

	/* Remove main interface. */
	memset(&hvif, 0, sizeof(hvif));
	hvif.index = 0;
	IEEE80211_ADDR_COPY(hvif.myaddr, ic->ic_myaddr);
	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_VAP_REMOVE,
	    &hvif, sizeof(hvif), NULL);

	/* Remove default node. */
	sta_index = 0;
	DPRINTFN(DBG_NODES, usc, "removing node %u\n", sta_index);
	(void)athn_usb_remove_hw_node(usc, &sta_index);

	(void)athn_usb_wmi_cmd(usc, AR_WMI_CMD_DISABLE_INTR);
	(void)athn_usb_wmi_cmd(usc, AR_WMI_CMD_DRAIN_TXQ_ALL);
	(void)athn_usb_wmi_cmd(usc, AR_WMI_CMD_STOP_RECV);

	athn_reset(sc, 0);
	athn_init_pll(sc, NULL);
	athn_set_power_awake(sc);
	athn_reset(sc, 1);
	athn_init_pll(sc, NULL);
	athn_set_power_sleep(sc);
}

MODULE(MODULE_CLASS_DRIVER, if_athn_usb, "bpf");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
if_athn_usb_modcmd(modcmd_t cmd, void *aux)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_if_athn_usb,
		    cfattach_ioconf_if_athn_usb, cfdata_ioconf_if_athn_usb);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_if_athn_usb,
		    cfattach_ioconf_if_athn_usb, cfdata_ioconf_if_athn_usb);
#endif
		return error;
	default:
		return ENOTTY;
	}
}
