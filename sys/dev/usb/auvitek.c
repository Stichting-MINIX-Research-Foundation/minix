/* $NetBSD: auvitek.c,v 1.9 2014/08/09 13:33:43 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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
 * Auvitek AU0828 USB controller
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvitek.c,v 1.9 2014/08/09 13:33:43 jmcneill Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/auvitekreg.h>
#include <dev/usb/auvitekvar.h>

static int	auvitek_match(device_t, cfdata_t, void *);
static void	auvitek_attach(device_t, device_t, void *);
static int	auvitek_detach(device_t, int);
static int	auvitek_rescan(device_t, const char *, const int *);
static void	auvitek_childdet(device_t, device_t);
static int	auvitek_activate(device_t, enum devact);

CFATTACH_DECL2_NEW(auvitek, sizeof(struct auvitek_softc),
    auvitek_match, auvitek_attach, auvitek_detach, auvitek_activate,
    auvitek_rescan, auvitek_childdet);

static const struct {
	uint16_t		vendor;
	uint16_t		product;
	const char *		name;
	enum auvitek_board	board;
} auvitek_devices[] = {
	{ 0x2040, 0x7200,
	  "WinTV HVR-950Q", AUVITEK_BOARD_HVR_950Q },
	{ 0x2040, 0x7240,
	  "WinTV HVR-850", AUVITEK_BOARD_HVR_850 },
};

static int
auvitek_match(device_t parent, cfdata_t match, void *opaque)
{
	struct usb_attach_arg *uaa = opaque;
	unsigned int i;

	for (i = 0; i < __arraycount(auvitek_devices); i++) {
		if (auvitek_devices[i].vendor == uaa->vendor &&
		    auvitek_devices[i].product == uaa->product)
			return UMATCH_VENDOR_PRODUCT;
	}

	return UMATCH_NONE;
}

static void
auvitek_attach(device_t parent, device_t self, void *opaque)
{
	struct auvitek_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = opaque;
	usbd_device_handle dev = uaa->device;
	usb_endpoint_descriptor_t *ed;
	usbd_status err;
	unsigned int i;
	uint8_t nep;

	aprint_naive("\n");
	aprint_normal(": AU0828\n");

	sc->sc_dev = self;
	sc->sc_udev = dev;
	sc->sc_uport = uaa->port;

	for (i = 0; i < __arraycount(auvitek_devices); i++) {
		if (auvitek_devices[i].vendor == uaa->vendor &&
		    auvitek_devices[i].product == uaa->product)
			break;
	}
	KASSERT(i != __arraycount(auvitek_devices));
	sc->sc_descr = auvitek_devices[i].name;
	sc->sc_board = auvitek_devices[i].board;

	sc->sc_dying = sc->sc_running = 0;

	mutex_init(&sc->sc_subdev_lock, MUTEX_DEFAULT, IPL_NONE);

	err = usbd_set_config_index(dev, 0, 1);
	if (err) {
		aprint_error_dev(self, "couldn't set config index: %s\n",
		    usbd_errstr(err));
		return;
	}
	err = usbd_device2interface_handle(dev, 0, &sc->sc_isoc_iface);
	if (err) {
		aprint_error_dev(self, "couldn't get interface handle: %s\n",
		    usbd_errstr(err));
		return;
	}
	err = usbd_device2interface_handle(dev, 3, &sc->sc_bulk_iface);
	if (err) {
		aprint_error_dev(self, "couldn't get interface handle: %s\n",
		    usbd_errstr(err));
		return;
	}

	sc->sc_ax.ax_sc = sc->sc_ab.ab_sc = sc;
	sc->sc_ax.ax_endpt = sc->sc_ab.ab_endpt = -1;

	err = usbd_set_interface(sc->sc_isoc_iface, AUVITEK_XFER_ALTNO);
	if (err) {
		aprint_error_dev(self, "couldn't set interface: %s\n",
		    usbd_errstr(err));
		return;
	}

	nep = 0;
	usbd_endpoint_count(sc->sc_isoc_iface, &nep);
	for (i = 0; i < nep; i++) {
		int dir, type;

		ed = usbd_interface2endpoint_descriptor(sc->sc_isoc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "couldn't read endpoint descriptor %d\n", i);
			continue;
		}

		dir = UE_GET_DIR(ed->bEndpointAddress);
		type = UE_GET_XFERTYPE(ed->bmAttributes);

		if (dir == UE_DIR_IN && type == UE_ISOCHRONOUS &&
		    sc->sc_ax.ax_endpt == -1) {
			sc->sc_ax.ax_endpt = ed->bEndpointAddress;
			sc->sc_ax.ax_maxpktlen =
			    UE_GET_SIZE(UGETW(ed->wMaxPacketSize)) *
			    (UE_GET_TRANS(UGETW(ed->wMaxPacketSize)) + 1);
		}
	}

	err = usbd_set_interface(sc->sc_isoc_iface, 0);
	if (err) {
		aprint_error_dev(self, "couldn't set interface: %s\n",
		    usbd_errstr(err));
		return;
	}

	if (sc->sc_ax.ax_endpt == -1) {
		aprint_error_dev(self, "couldn't find isoc endpoint\n");
		sc->sc_dying = 1;
		return;
	}
	if (sc->sc_ax.ax_maxpktlen == 0) {
		aprint_error_dev(self, "couldn't determine packet length\n");
		sc->sc_dying = 1;
		return;
	}

	aprint_debug_dev(self, "isoc endpoint 0x%02x size %d\n",
	    sc->sc_ax.ax_endpt, sc->sc_ax.ax_maxpktlen);

	nep = 0;
	usbd_endpoint_count(sc->sc_bulk_iface, &nep);
	for (i = 0; i < nep; i++) {
		int dir, type;

		ed = usbd_interface2endpoint_descriptor(sc->sc_bulk_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "couldn't read endpoint descriptor %d\n", i);
			continue;
		}

		dir = UE_GET_DIR(ed->bEndpointAddress);
		type = UE_GET_XFERTYPE(ed->bmAttributes);

		if (dir == UE_DIR_IN && type == UE_BULK &&
		    sc->sc_ab.ab_endpt == -1) {
			sc->sc_ab.ab_endpt = ed->bEndpointAddress;
		}
	}

	if (sc->sc_ab.ab_endpt == -1) {
		aprint_error_dev(self, "couldn't find bulk endpoint\n");
		sc->sc_dying = 1;
		return;
	}

	for (i = 0; i < AUVITEK_NBULK_XFERS; i++) {
		sc->sc_ab.ab_bx[i].bx_sc = sc;
		sc->sc_ab.ab_bx[i].bx_xfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_ab.ab_bx[i].bx_xfer == NULL) {
			aprint_error_dev(self, "couldn't allocate xfer\n");
			sc->sc_dying = 1;
			return;
		}
		sc->sc_ab.ab_bx[i].bx_buffer = usbd_alloc_buffer(
		    sc->sc_ab.ab_bx[i].bx_xfer, AUVITEK_BULK_BUFLEN);
		if (sc->sc_ab.ab_bx[i].bx_buffer == NULL) {
			aprint_error_dev(self,
			    "couldn't allocate xfer buffer\n");
			sc->sc_dying = 1;
			return;
		}
	}

	aprint_debug_dev(self, "bulk endpoint 0x%02x size %d\n",
	    sc->sc_ab.ab_endpt, AUVITEK_BULK_BUFLEN);

	auvitek_board_init(sc);

	auvitek_i2c_attach(sc);

	sc->sc_au8522 = au8522_open(self, &sc->sc_i2c, 0x8e >> 1,
	    auvitek_board_get_if_frequency(sc));
	if (sc->sc_au8522 == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't initialize decoder\n");
		sc->sc_dying = 1;
		return;
	}

	config_mountroot(self, auvitek_attach_tuner);

	auvitek_video_attach(sc);
	auvitek_audio_attach(sc);
	auvitek_dtv_attach(sc);
}

void
auvitek_attach_tuner(device_t self)
{
	struct auvitek_softc *sc = device_private(self);

	mutex_enter(&sc->sc_subdev_lock);
	if (sc->sc_xc5k == NULL) {
		sc->sc_xc5k = xc5k_open(sc->sc_dev, &sc->sc_i2c, 0xc2 >> 1,
		    auvitek_board_tuner_reset, sc,
		    auvitek_board_get_if_frequency(sc),
		    FE_ATSC);
	}
	mutex_exit(&sc->sc_subdev_lock);
}

static int
auvitek_detach(device_t self, int flags)
{
	struct auvitek_softc *sc = device_private(self);
	unsigned int i;

	sc->sc_dying = 1;

	pmf_device_deregister(self);

	auvitek_dtv_detach(sc, flags);
	auvitek_audio_detach(sc, flags);
	auvitek_video_detach(sc, flags);

	if (sc->sc_xc5k)
		xc5k_close(sc->sc_xc5k);
	if (sc->sc_au8522)
		au8522_close(sc->sc_au8522);

	auvitek_i2c_detach(sc, flags);

	mutex_destroy(&sc->sc_subdev_lock);

	for (i = 0; i < AUVITEK_NBULK_XFERS; i++) {
		if (sc->sc_ab.ab_bx[i].bx_xfer)
			usbd_free_xfer(sc->sc_ab.ab_bx[i].bx_xfer);
	}

	return 0;
}

int
auvitek_activate(device_t self, enum devact act)
{
	struct auvitek_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return 0;
	}
}

static int
auvitek_rescan(device_t self, const char *ifattr, const int *locs)
{
	struct auvitek_softc *sc = device_private(self);

	auvitek_video_rescan(sc, ifattr, locs);
	auvitek_dtv_rescan(sc, ifattr, locs);
	auvitek_i2c_rescan(sc, ifattr, locs);

	return 0;
}

static void
auvitek_childdet(device_t self, device_t child)
{
	struct auvitek_softc *sc = device_private(self);

	auvitek_video_childdet(sc, child);
	auvitek_audio_childdet(sc, child);
	auvitek_dtv_childdet(sc, child);
}

uint8_t
auvitek_read_1(struct auvitek_softc *sc, uint16_t reg)
{
	usb_device_request_t req;
	usbd_status err;
	int actlen;
	uint8_t data;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AU0828_CMD_REQUEST_IN;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, sizeof(data));

	KERNEL_LOCK(1, curlwp);
	err = usbd_do_request_flags(sc->sc_udev, &req, &data, 0,
	    &actlen, USBD_DEFAULT_TIMEOUT);
	KERNEL_UNLOCK_ONE(curlwp);

	if (err)
		printf("%s: read failed: %s\n", device_xname(sc->sc_dev),
		    usbd_errstr(err));

	return data;
}

void
auvitek_write_1(struct auvitek_softc *sc, uint16_t reg, uint8_t data)
{
	usb_device_request_t req;
	usbd_status err;
	int actlen;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AU0828_CMD_REQUEST_OUT;
	USETW(req.wValue, data);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	KERNEL_LOCK(1, curlwp);
	err = usbd_do_request_flags(sc->sc_udev, &req, NULL, 0,
	    &actlen, USBD_DEFAULT_TIMEOUT);
	KERNEL_UNLOCK_ONE(curlwp);

	if (err)
		printf("%s: write failed: %s\n", device_xname(sc->sc_dev),
		    usbd_errstr(err));
}

MODULE(MODULE_CLASS_DRIVER, auvitek, "au8522,xc5k");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
auvitek_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_auvitek,
		    cfattach_ioconf_auvitek, cfdata_ioconf_auvitek);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_auvitek,
		    cfattach_ioconf_auvitek, cfdata_ioconf_auvitek);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
