/*	$NetBSD: udsbr.c,v 1.22 2012/12/27 16:42:32 skrll Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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
 * Driver for the D-Link DSB-R100 FM radio.
 * I apologize for the magic hex constants, but this is what happens
 * when you have to reverse engineer the driver.
 * Parts of the code borrowed from Linux and parts from Warner Losh's
 * FreeBSD driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: udsbr.c,v 1.22 2012/12/27 16:42:32 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/radioio.h>
#include <dev/radio_if.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/usbdevs.h>

#ifdef UDSBR_DEBUG
#define DPRINTF(x)	if (udsbrdebug) printf x
#define DPRINTFN(n,x)	if (udsbrdebug>(n)) printf x
int	udsbrdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UDSBR_CONFIG_NO		1

Static	int     udsbr_get_info(void *, struct radio_info *);
Static	int     udsbr_set_info(void *, struct radio_info *);

const struct radio_hw_if udsbr_hw_if = {
	NULL, /* open */
	NULL, /* close */
	udsbr_get_info,
	udsbr_set_info,
	NULL
};

struct udsbr_softc {
 	device_t		sc_dev;
	usbd_device_handle	sc_udev;

	char			sc_mute;
	char			sc_vol;
	u_int32_t		sc_freq;

	device_t		sc_child;

	char			sc_dying;
};

Static	int	udsbr_req(struct udsbr_softc *sc, int ureq, int value,
			  int index);
Static	void	udsbr_start(struct udsbr_softc *sc);
Static	void	udsbr_stop(struct udsbr_softc *sc);
Static	void	udsbr_setfreq(struct udsbr_softc *sc, int freq);
Static	int	udsbr_status(struct udsbr_softc *sc);

int udsbr_match(device_t, cfdata_t, void *);
void udsbr_attach(device_t, device_t, void *);
void udsbr_childdet(device_t, device_t);
int udsbr_detach(device_t, int);
int udsbr_activate(device_t, enum devact);
extern struct cfdriver udsbr_cd;
CFATTACH_DECL2_NEW(udsbr, sizeof(struct udsbr_softc), udsbr_match,
    udsbr_attach, udsbr_detach, udsbr_activate, NULL, udsbr_childdet);

int 
udsbr_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	DPRINTFN(50,("udsbr_match\n"));

	if (uaa->vendor != USB_VENDOR_CYPRESS ||
	    uaa->product != USB_PRODUCT_CYPRESS_FMRADIO)
		return (UMATCH_NONE);
	return (UMATCH_VENDOR_PRODUCT);
}

void 
udsbr_attach(device_t parent, device_t self, void *aux)
{
	struct udsbr_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle	dev = uaa->device;
	char			*devinfop;
	usbd_status		err;

	DPRINTFN(10,("udsbr_attach: sc=%p\n", sc));

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, UDSBR_CONFIG_NO, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(err));
		return;
	}

	sc->sc_udev = dev;

	DPRINTFN(10, ("udsbr_attach: %p\n", sc->sc_udev));

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	sc->sc_child = radio_attach_mi(&udsbr_hw_if, sc, sc->sc_dev);

	return;
}

void
udsbr_childdet(device_t self, device_t child)
{
}

int 
udsbr_detach(device_t self, int flags)
{
	struct udsbr_softc *sc = device_private(self);
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (rv);
}

int
udsbr_activate(device_t self, enum devact act)
{
	struct udsbr_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
udsbr_req(struct udsbr_softc *sc, int ureq, int value, int index)
{
	usb_device_request_t req;
	usbd_status err;
	u_char data;

	DPRINTFN(1,("udsbr_req: ureq=0x%02x value=0x%04x index=0x%04x\n",
		    ureq, value, index));
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = ureq;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, 1);
	err = usbd_do_request(sc->sc_udev, &req, &data);
	if (err) {
		aprint_error_dev(sc->sc_dev, "request failed err=%d\n", err);
	}
	return !(data & 1);
}

void
udsbr_start(struct udsbr_softc *sc)
{
	(void)udsbr_req(sc, 0x00, 0x0000, 0x00c7);
	(void)udsbr_req(sc, 0x02, 0x0001, 0x0000);
}

void
udsbr_stop(struct udsbr_softc *sc)
{
	(void)udsbr_req(sc, 0x00, 0x0016, 0x001c);
	(void)udsbr_req(sc, 0x02, 0x0000, 0x0000);
}

void
udsbr_setfreq(struct udsbr_softc *sc, int freq)
{
	DPRINTF(("udsbr_setfreq: setfreq=%d\n", freq));
        /*
         * Freq now is in Hz.  We need to convert it to the frequency
         * that the radio wants.  This frequency is 10.7MHz above
         * the actual frequency.  We then need to convert to
         * units of 12.5kHz.  We add one to the IFM to make rounding
         * easier.
         */
        freq = (freq * 1000 + 10700001) / 12500;
	(void)udsbr_req(sc, 0x01, (freq >> 8) & 0xff, freq & 0xff);
	(void)udsbr_req(sc, 0x00, 0x0096, 0x00b7);
	usbd_delay_ms(sc->sc_udev, 240); /* wait for signal to settle */
}

int
udsbr_status(struct udsbr_softc *sc)
{
	return (udsbr_req(sc, 0x00, 0x0000, 0x0024));
}


int
udsbr_get_info(void *v, struct radio_info *ri)
{
	struct udsbr_softc *sc = v;

	ri->mute = sc->sc_mute;
	ri->volume = sc->sc_vol ? 255 : 0;
	ri->caps = RADIO_CAPS_DETECT_STEREO;
	ri->rfreq = 0;
	ri->lock = 0;
	ri->freq = sc->sc_freq;
	ri->info = udsbr_status(sc) ? RADIO_INFO_STEREO : 0;

	return (0);
}

int
udsbr_set_info(void *v, struct radio_info *ri)
{
	struct udsbr_softc *sc = v;

	sc->sc_mute = ri->mute != 0;
	sc->sc_vol = ri->volume != 0;
	sc->sc_freq = ri->freq;
	udsbr_setfreq(sc, sc->sc_freq);

	if (sc->sc_mute || sc->sc_vol == 0)
		udsbr_stop(sc);
	else
		udsbr_start(sc);

	return (0);
}
