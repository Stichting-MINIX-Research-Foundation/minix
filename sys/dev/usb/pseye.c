/* $NetBSD: pseye.c,v 1.21 2011/05/24 16:42:31 joerg Exp $ */

/*-
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@invisible.ca>
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
 * Sony PlayStation Eye Driver
 *
 * The only documentation we have for this part is based on a series
 * of forum postings by Jim Paris on ps2dev.org. Many thanks for
 * figuring this one out.
 *
 * URL: http://forums.ps2dev.org/viewtopic.php?t=9238
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pseye.c,v 1.21 2011/05/24 16:42:31 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/kthread.h>
#include <sys/condvar.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uvideoreg.h>

#include <dev/video_if.h>

#define PRI_PSEYE	PRI_BIO

/* Bulk-in buffer length -- make room for payload + UVC headers */
#define PSEYE_BULKIN_BUFLEN	((640 * 480 * 2) + 4096)
#define PSEYE_BULKIN_BLKLEN	2048

/* SCCB/sensor interface */
#define PSEYE_SCCB_ADDRESS	0xf1
#define PSEYE_SCCB_SUBADDR	0xf2
#define PSEYE_SCCB_WRITE	0xf3
#define PSEYE_SCCB_READ		0xf4
#define PSEYE_SCCB_OPERATION	0xf5
#define PSEYE_SCCB_STATUS	0xf6

#define PSEYE_SCCB_OP_WRITE_3	0x37
#define PSEYE_SCCB_OP_WRITE_2	0x33
#define PSEYE_SCCB_OP_READ_2	0xf9

struct pseye_softc {
	device_t		sc_dev;

	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	device_t		sc_videodev;
	char			sc_running;

	kcondvar_t		sc_cv;
	kmutex_t		sc_mtx;

	usbd_pipe_handle	sc_bulkin_pipe;
	usbd_xfer_handle	sc_bulkin_xfer;
	int			sc_bulkin;
	uint8_t			*sc_bulkin_buffer;
	int			sc_bulkin_bufferlen;

	char			sc_dying;

	char			sc_businfo[32];
};

static int	pseye_match(device_t, cfdata_t, void *);
static void	pseye_attach(device_t, device_t, void *);
static int	pseye_detach(device_t, int);
static void	pseye_childdet(device_t, device_t);
static int	pseye_activate(device_t, enum devact);

static void	pseye_init(struct pseye_softc *);
static void	pseye_sccb_init(struct pseye_softc *);
static void	pseye_stop(struct pseye_softc *);
static void	pseye_start(struct pseye_softc *);
static void	pseye_led(struct pseye_softc *, bool);
static uint8_t	pseye_getreg(struct pseye_softc *, uint16_t);
static void	pseye_setreg(struct pseye_softc *, uint16_t, uint8_t);
static void	pseye_setregv(struct pseye_softc *, uint16_t, uint8_t);
static void	pseye_sccb_setreg(struct pseye_softc *, uint8_t, uint8_t);
static bool	pseye_sccb_status(struct pseye_softc *);

static int	pseye_init_pipes(struct pseye_softc *);
static int	pseye_close_pipes(struct pseye_softc *);

static usbd_status	pseye_get_frame(struct pseye_softc *, uint32_t *);
static void	pseye_submit_payload(struct pseye_softc *, uint32_t);

/* video(9) API */
static int		pseye_open(void *, int);
static void		pseye_close(void *);
static const char *	pseye_get_devname(void *);
static const char *	pseye_get_businfo(void *);
static int		pseye_enum_format(void *, uint32_t,
					  struct video_format *);
static int		pseye_get_format(void *, struct video_format *);
static int		pseye_set_format(void *, struct video_format *);
static int		pseye_try_format(void *, struct video_format *);
static int		pseye_start_transfer(void *);
static int		pseye_stop_transfer(void *);

CFATTACH_DECL2_NEW(pseye, sizeof(struct pseye_softc),
    pseye_match, pseye_attach, pseye_detach, pseye_activate,
    NULL, pseye_childdet);

static const struct video_hw_if pseye_hw_if = {
	.open = pseye_open,
	.close = pseye_close,
	.get_devname = pseye_get_devname,
	.get_businfo = pseye_get_businfo,
	.enum_format = pseye_enum_format,
	.get_format = pseye_get_format,
	.set_format = pseye_set_format,
	.try_format = pseye_try_format,
	.start_transfer = pseye_start_transfer,
	.stop_transfer = pseye_stop_transfer,
	.control_iter_init = NULL,
	.control_iter_next = NULL,
	.get_control_desc_group = NULL,
	.get_control_group = NULL,
	.set_control_group = NULL,
};

static int
pseye_match(device_t parent, cfdata_t match, void *opaque)
{
	struct usbif_attach_arg *uaa = opaque;

	if (uaa->class != UICLASS_VENDOR)
		return UMATCH_NONE;

	if (uaa->vendor == USB_VENDOR_OMNIVISION2) {
		switch (uaa->product) {
		case USB_PRODUCT_OMNIVISION2_PSEYE:
			if (uaa->ifaceno != 0)
				return UMATCH_NONE;
			return UMATCH_VENDOR_PRODUCT;
		}
	}

	return UMATCH_NONE;
}

static void
pseye_attach(device_t parent, device_t self, void *opaque)
{
	struct pseye_softc *sc = device_private(self);
	struct usbif_attach_arg *uaa = opaque;
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id = NULL;
	usb_endpoint_descriptor_t *ed = NULL, *ed_bulkin = NULL;
	char *devinfop;
	int i;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dev = self;
	sc->sc_udev = dev;
	sc->sc_iface = uaa->iface;
	snprintf(sc->sc_businfo, sizeof(sc->sc_businfo), "usb:%08x",
	    sc->sc_udev->cookie.cookie);
	sc->sc_bulkin_bufferlen = PSEYE_BULKIN_BUFLEN;

	sc->sc_dying = sc->sc_running = 0;
	cv_init(&sc->sc_cv, device_xname(self));
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);

	id = usbd_get_interface_descriptor(sc->sc_iface);
	if (id == NULL) {
		aprint_error_dev(self, "failed to get interface descriptor\n");
		sc->sc_dying = 1;
		return;
	}

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self, "couldn't get ep %d\n", i);
			sc->sc_dying = 1;
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ed_bulkin = ed;
			break;
		}
	}

	if (ed_bulkin == NULL) {
		aprint_error_dev(self, "no bulk-in endpoint found\n");
		sc->sc_dying = 1;
		return;
	}

	sc->sc_bulkin = ed_bulkin->bEndpointAddress;

	sc->sc_bulkin_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulkin_xfer == NULL) {
		sc->sc_dying = 1;
		return;
	}
	sc->sc_bulkin_buffer = usbd_alloc_buffer(sc->sc_bulkin_xfer,
	    sc->sc_bulkin_bufferlen);
	if (sc->sc_bulkin_buffer == NULL) {
		usbd_free_xfer(sc->sc_bulkin_xfer);
		sc->sc_bulkin_xfer = NULL;
		sc->sc_dying = 1;
		return;
	}

	pseye_init(sc);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	sc->sc_videodev = video_attach_mi(&pseye_hw_if, self);
	if (sc->sc_videodev == NULL) {
		aprint_error_dev(self, "couldn't attach video layer\n");
		sc->sc_dying = 1;
		return;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    self);

}

static int
pseye_detach(device_t self, int flags)
{
	struct pseye_softc *sc = device_private(self);

	sc->sc_dying = 1;

	pmf_device_deregister(self);

	if (sc->sc_videodev != NULL) {
		config_detach(sc->sc_videodev, flags);
		sc->sc_videodev = NULL;
	}

	if (sc->sc_bulkin_xfer != NULL) {
		usbd_free_xfer(sc->sc_bulkin_xfer);
		sc->sc_bulkin_xfer = NULL;
	}

	if (sc->sc_bulkin_pipe != NULL) {
		usbd_abort_pipe(sc->sc_bulkin_pipe);
		sc->sc_bulkin_pipe = NULL;
	}

	mutex_enter(&sc->sc_mtx);
	if (sc->sc_running) {
		sc->sc_running = 0;
		cv_wait_sig(&sc->sc_cv, &sc->sc_mtx);
	}
	mutex_exit(&sc->sc_mtx);

	cv_destroy(&sc->sc_cv);
	mutex_destroy(&sc->sc_mtx);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    sc->sc_dev);

	return 0;
}

int
pseye_activate(device_t self, enum devact act)
{
	struct pseye_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static void
pseye_childdet(device_t self, device_t child)
{
	struct pseye_softc *sc = device_private(self);

	if (sc->sc_videodev) {
		KASSERT(sc->sc_videodev == child);
		sc->sc_videodev = NULL;
	}
}

/*
 * Device access
 */

static void
pseye_init(struct pseye_softc *sc)
{
	pseye_sccb_init(sc);

	pseye_setregv(sc, 0xc2, 0x0c);
	pseye_setregv(sc, 0x88, 0xf8);
	pseye_setregv(sc, 0xc3, 0x69);
	pseye_setregv(sc, 0x89, 0xff);
	pseye_setregv(sc, 0x76, 0x03);
	pseye_setregv(sc, 0x92, 0x01);
	pseye_setregv(sc, 0x93, 0x18);
	pseye_setregv(sc, 0x94, 0x10);
	pseye_setregv(sc, 0x95, 0x10);
	pseye_setregv(sc, 0xe2, 0x00);
	pseye_setregv(sc, 0xe7, 0x3e);

	pseye_setregv(sc, 0x96, 0x00);

	pseye_setreg(sc, 0x97, 0x20);
	pseye_setreg(sc, 0x97, 0x20);
	pseye_setreg(sc, 0x97, 0x20);
	pseye_setreg(sc, 0x97, 0x0a);
	pseye_setreg(sc, 0x97, 0x3f);
	pseye_setreg(sc, 0x97, 0x4a);
	pseye_setreg(sc, 0x97, 0x20);
	pseye_setreg(sc, 0x97, 0x15);
	pseye_setreg(sc, 0x97, 0x0b);

	pseye_setregv(sc, 0x8e, 0x40);
	pseye_setregv(sc, 0x1f, 0x81);
	pseye_setregv(sc, 0x34, 0x05);
	pseye_setregv(sc, 0xe3, 0x04);
	pseye_setregv(sc, 0x88, 0x00);
	pseye_setregv(sc, 0x89, 0x00);
	pseye_setregv(sc, 0x76, 0x00);
	pseye_setregv(sc, 0xe7, 0x2e);
	pseye_setregv(sc, 0x31, 0xf9);
	pseye_setregv(sc, 0x25, 0x42);
	pseye_setregv(sc, 0x21, 0xf0);

	pseye_setreg(sc, 0x1c, 0x00);
	pseye_setreg(sc, 0x1d, 0x40);
	pseye_setreg(sc, 0x1d, 0x02);	/* payload size 0x0200 * 4 == 2048 */
	pseye_setreg(sc, 0x1d, 0x00);
	pseye_setreg(sc, 0x1d, 0x02);	/* frame size 0x025800 * 4 == 614400 */
	pseye_setreg(sc, 0x1d, 0x58);
	pseye_setreg(sc, 0x1d, 0x00);

	pseye_setreg(sc, 0x1c, 0x0a);
	pseye_setreg(sc, 0x1d, 0x08);	/* enable UVC header */
	pseye_setreg(sc, 0x1d, 0x0e);

	pseye_setregv(sc, 0x8d, 0x1c);
	pseye_setregv(sc, 0x8e, 0x80);
	pseye_setregv(sc, 0xe5, 0x04);

	pseye_sccb_setreg(sc, 0x12, 0x80);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x11, 0x01);

	pseye_sccb_setreg(sc, 0x3d, 0x03);
	pseye_sccb_setreg(sc, 0x17, 0x26);
	pseye_sccb_setreg(sc, 0x18, 0xa0);
	pseye_sccb_setreg(sc, 0x19, 0x07);
	pseye_sccb_setreg(sc, 0x1a, 0xf0);
	pseye_sccb_setreg(sc, 0x32, 0x00);
	pseye_sccb_setreg(sc, 0x29, 0xa0);
	pseye_sccb_setreg(sc, 0x2c, 0xf0);
	pseye_sccb_setreg(sc, 0x65, 0x20);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x42, 0x7f);
	pseye_sccb_setreg(sc, 0x63, 0xe0);
	pseye_sccb_setreg(sc, 0x64, 0xff);
	pseye_sccb_setreg(sc, 0x66, 0x00);
	pseye_sccb_setreg(sc, 0x13, 0xf0);
	pseye_sccb_setreg(sc, 0x0d, 0x41);
	pseye_sccb_setreg(sc, 0x0f, 0xc5);
	pseye_sccb_setreg(sc, 0x14, 0x11);

	pseye_sccb_setreg(sc, 0x22, 0x7f);
	pseye_sccb_setreg(sc, 0x23, 0x03);
	pseye_sccb_setreg(sc, 0x24, 0x40);
	pseye_sccb_setreg(sc, 0x25, 0x30);
	pseye_sccb_setreg(sc, 0x26, 0xa1);
	pseye_sccb_setreg(sc, 0x2a, 0x00);
	pseye_sccb_setreg(sc, 0x2b, 0x00);
	pseye_sccb_setreg(sc, 0x6b, 0xaa);
	pseye_sccb_setreg(sc, 0x13, 0xff);

	pseye_sccb_setreg(sc, 0x90, 0x05);
	pseye_sccb_setreg(sc, 0x91, 0x01);
	pseye_sccb_setreg(sc, 0x92, 0x03);
	pseye_sccb_setreg(sc, 0x93, 0x00);
	pseye_sccb_setreg(sc, 0x94, 0x60);
	pseye_sccb_setreg(sc, 0x95, 0x3c);
	pseye_sccb_setreg(sc, 0x96, 0x24);
	pseye_sccb_setreg(sc, 0x97, 0x1e);
	pseye_sccb_setreg(sc, 0x98, 0x62);
	pseye_sccb_setreg(sc, 0x99, 0x80);
	pseye_sccb_setreg(sc, 0x9a, 0x1e);
	pseye_sccb_setreg(sc, 0x9b, 0x08);
	pseye_sccb_setreg(sc, 0x9c, 0x20);
	pseye_sccb_setreg(sc, 0x9e, 0x81);

	pseye_sccb_setreg(sc, 0xa6, 0x04);
	pseye_sccb_setreg(sc, 0x7e, 0x0c);
	pseye_sccb_setreg(sc, 0x7f, 0x16);

	pseye_sccb_setreg(sc, 0x80, 0x2a);
	pseye_sccb_setreg(sc, 0x81, 0x4e);
	pseye_sccb_setreg(sc, 0x82, 0x61);
	pseye_sccb_setreg(sc, 0x83, 0x6f);
	pseye_sccb_setreg(sc, 0x84, 0x7b);
	pseye_sccb_setreg(sc, 0x85, 0x86);
	pseye_sccb_setreg(sc, 0x86, 0x8e);
	pseye_sccb_setreg(sc, 0x87, 0x97);
	pseye_sccb_setreg(sc, 0x88, 0xa4);
	pseye_sccb_setreg(sc, 0x89, 0xaf);
	pseye_sccb_setreg(sc, 0x8a, 0xc5);
	pseye_sccb_setreg(sc, 0x8b, 0xd7);
	pseye_sccb_setreg(sc, 0x8c, 0xe8);
	pseye_sccb_setreg(sc, 0x8d, 0x20);

	pseye_sccb_setreg(sc, 0x0c, 0x90);

	pseye_setregv(sc, 0xc0, 0x50);
	pseye_setregv(sc, 0xc1, 0x3c);
	pseye_setregv(sc, 0xc2, 0x0c);

	pseye_sccb_setreg(sc, 0x2b, 0x00);
	pseye_sccb_setreg(sc, 0x22, 0x7f);
	pseye_sccb_setreg(sc, 0x23, 0x03);
	pseye_sccb_setreg(sc, 0x11, 0x01);
	pseye_sccb_setreg(sc, 0x0c, 0xd0);
	pseye_sccb_setreg(sc, 0x64, 0xff);
	pseye_sccb_setreg(sc, 0x0d, 0x41);

	pseye_sccb_setreg(sc, 0x14, 0x41);
	pseye_sccb_setreg(sc, 0x0e, 0xcd);
	pseye_sccb_setreg(sc, 0xac, 0xbf);
	pseye_sccb_setreg(sc, 0x8e, 0x00);
	pseye_sccb_setreg(sc, 0x0c, 0xd0);

	pseye_stop(sc);
}

static void
pseye_sccb_init(struct pseye_softc *sc)
{
	pseye_setregv(sc, 0xe7, 0x3a);
	pseye_setreg(sc, PSEYE_SCCB_ADDRESS, 0x60);
	pseye_setreg(sc, PSEYE_SCCB_ADDRESS, 0x60);
	pseye_setreg(sc, PSEYE_SCCB_ADDRESS, 0x60);
	pseye_setreg(sc, PSEYE_SCCB_ADDRESS, 0x42);
}

static void
pseye_stop(struct pseye_softc *sc)
{
	pseye_led(sc, false);
	pseye_setreg(sc, 0xe0, 0x09);
}

static void
pseye_start(struct pseye_softc *sc)
{
	pseye_led(sc, true);
	pseye_setreg(sc, 0xe0, 0x00);
}

static void
pseye_led(struct pseye_softc *sc, bool enabled)
{
	uint8_t val;

	val = pseye_getreg(sc, 0x21);
	pseye_setreg(sc, 0x21, val | 0x80);

	val = pseye_getreg(sc, 0x23);
	if (enabled == true)
		val |= 0x80;
	else
		val &= ~0x80;
	pseye_setreg(sc, 0x23, val);
}

static uint8_t
pseye_getreg(struct pseye_softc *sc, uint16_t reg)
{
	usb_device_request_t req;
	usbd_status err;
	uint8_t buf;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 1;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->sc_udev, &req, &buf);
	if (err) {
		aprint_error_dev(sc->sc_dev, "couldn't read reg 0x%04x: %s\n",
		    reg, usbd_errstr(err));
		return 0xff;
	}

	return buf;
}

static void
pseye_setreg(struct pseye_softc *sc, uint16_t reg, uint8_t val)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = 1;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->sc_udev, &req, &val);
	if (err)
		aprint_error_dev(sc->sc_dev, "couldn't write reg 0x%04x: %s\n",
		    reg, usbd_errstr(err));
}

static void
pseye_setregv(struct pseye_softc *sc, uint16_t reg, uint8_t val)
{
	pseye_setreg(sc, reg, val);
	if (pseye_getreg(sc, reg) != val)
		aprint_error_dev(sc->sc_dev, "couldn't verify reg 0x%04x\n",
		    reg);
}

static void
pseye_sccb_setreg(struct pseye_softc *sc, uint8_t reg, uint8_t val)
{
	pseye_setreg(sc, PSEYE_SCCB_SUBADDR, reg);
	pseye_setreg(sc, PSEYE_SCCB_WRITE, val);
	pseye_setreg(sc, PSEYE_SCCB_OPERATION, PSEYE_SCCB_OP_WRITE_3);

	if (pseye_sccb_status(sc) == false)
		aprint_error_dev(sc->sc_dev, "couldn't write sccb reg 0x%04x\n",
		    reg);
}

static bool
pseye_sccb_status(struct pseye_softc *sc)
{
	int retry = 5;
	uint8_t reg;

	while (retry-- >= 0) {
		reg = pseye_getreg(sc, PSEYE_SCCB_STATUS);
		if (reg == 0x00)
			return true;
		else if (reg == 0x04)
			return false;
	}

	aprint_error_dev(sc->sc_dev, "timeout reading sccb status\n");
	return false;
}

static usbd_status
pseye_get_frame(struct pseye_softc *sc, uint32_t *plen)
{
	if (sc->sc_dying)
		return USBD_IOERROR;

	return usbd_bulk_transfer(sc->sc_bulkin_xfer, sc->sc_bulkin_pipe,
	    USBD_SHORT_XFER_OK|USBD_NO_COPY, 1000,
	    sc->sc_bulkin_buffer, plen, "pseyerb");
}

static int
pseye_init_pipes(struct pseye_softc *sc)
{
	usbd_status err;

	if (sc->sc_dying)
		return EIO;

	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin, 0,
	    &sc->sc_bulkin_pipe);
	if (err) {
		aprint_error_dev(sc->sc_dev, "couldn't open bulk-in pipe: %s\n",
		    usbd_errstr(err));
		return ENOMEM;
	}

	pseye_start(sc);

	return 0;
}

int
pseye_close_pipes(struct pseye_softc *sc)
{
	if (sc->sc_bulkin_pipe != NULL) {
		usbd_abort_pipe(sc->sc_bulkin_pipe);
		usbd_close_pipe(sc->sc_bulkin_pipe);
		sc->sc_bulkin_pipe = NULL;
	}

	pseye_stop(sc);

	return 0;
}

static void
pseye_submit_payload(struct pseye_softc *sc, uint32_t tlen)
{
	struct video_payload payload;
	uvideo_payload_header_t *uvchdr;
	uint8_t *buf = sc->sc_bulkin_buffer;
	uint32_t len;
	uint32_t brem = (640*480*2);

	while (brem > 0 && tlen > 0) {
		len = min(tlen, PSEYE_BULKIN_BLKLEN);
		if (len < UVIDEO_PAYLOAD_HEADER_SIZE) {
			printf("pseye_submit_payload: len=%u\n", len);
			return;
		}

		uvchdr = (uvideo_payload_header_t *)buf;
		if (uvchdr->bHeaderLength != UVIDEO_PAYLOAD_HEADER_SIZE)
			goto next;
		if (uvchdr->bHeaderLength == len &&
		    !(uvchdr->bmHeaderInfo & UV_END_OF_FRAME))
			goto next;
		if (uvchdr->bmHeaderInfo & UV_ERROR)
			return;
		if ((uvchdr->bmHeaderInfo & UV_PRES_TIME) == 0)
			goto next;

		payload.data = buf + uvchdr->bHeaderLength;
		payload.size = min(brem, len - uvchdr->bHeaderLength);
		payload.frameno = UGETDW(&buf[2]);
		payload.end_of_frame = uvchdr->bmHeaderInfo & UV_END_OF_FRAME;
		video_submit_payload(sc->sc_videodev, &payload);

next:
		tlen -= len;
		buf += len;
		brem -= payload.size;
	}
}

static void
pseye_transfer_thread(void *opaque)
{
	struct pseye_softc *sc = opaque;
	uint32_t len;
	int error;

	while (sc->sc_running) {
		len = sc->sc_bulkin_bufferlen;
		error = pseye_get_frame(sc, &len);
		if (error == USBD_NORMAL_COMPLETION)
			pseye_submit_payload(sc, len);
	}

	mutex_enter(&sc->sc_mtx);
	cv_broadcast(&sc->sc_cv);
	mutex_exit(&sc->sc_mtx);

	kthread_exit(0);
}

/* video(9) API implementations */
static int
pseye_open(void *opaque, int flags)
{
	struct pseye_softc *sc = opaque;

	if (sc->sc_dying)
		return EIO;

	return pseye_init_pipes(sc);
}

static void
pseye_close(void *opaque)
{
	struct pseye_softc *sc = opaque;

	pseye_close_pipes(sc);
}

static const char *
pseye_get_devname(void *opaque)
{
	return "PlayStation Eye";
}

static const char *
pseye_get_businfo(void *opaque)
{
	struct pseye_softc *sc = opaque;

	return sc->sc_businfo;
}

static int
pseye_enum_format(void *opaque, uint32_t index, struct video_format *format)
{
	if (index != 0)
		return EINVAL;
	return pseye_get_format(opaque, format);
}

static int
pseye_get_format(void *opaque, struct video_format *format)
{
	format->pixel_format = VIDEO_FORMAT_YUY2; /* XXX actually YUYV */
	format->width = 640;
	format->height = 480;
	format->aspect_x = 4;
	format->aspect_y = 3;
	format->sample_size = format->width * format->height * 2;
	format->stride = format->width * 2;
	format->color.primaries = VIDEO_COLOR_PRIMARIES_UNSPECIFIED;
	format->color.gamma_function = VIDEO_GAMMA_FUNCTION_UNSPECIFIED;
	format->color.matrix_coeff = VIDEO_MATRIX_COEFF_UNSPECIFIED;
	format->interlace_flags = VIDEO_INTERLACE_ON;
	format->priv = 0;

	return 0;
}

static int
pseye_set_format(void *opaque, struct video_format *format)
{
#if notyet
	if (format->pixel_format != VIDEO_FORMAT_YUYV)
		return EINVAL;
	if (format->width != 640 || format->height != 480)
		return EINVAL;
#endif
	/* XXX */
	return pseye_get_format(opaque, format);
}

static int
pseye_try_format(void *opaque, struct video_format *format)
{
	return pseye_get_format(opaque, format);
}

static int
pseye_start_transfer(void *opaque)
{
	struct pseye_softc *sc = opaque;
	int err = 0;

	mutex_enter(&sc->sc_mtx);
	if (sc->sc_running == 0) {
		sc->sc_running = 1;
		err = kthread_create(PRI_PSEYE, 0, NULL, pseye_transfer_thread,
		    opaque, NULL, "%s", device_xname(sc->sc_dev));
	} else
		aprint_error_dev(sc->sc_dev, "transfer already in progress\n");
	mutex_exit(&sc->sc_mtx);

	return err;
}

static int
pseye_stop_transfer(void *opaque)
{
	struct pseye_softc *sc = opaque;

	mutex_enter(&sc->sc_mtx);
	if (sc->sc_running) {
		sc->sc_running = 0;
		cv_wait_sig(&sc->sc_cv, &sc->sc_mtx);
	}
	mutex_exit(&sc->sc_mtx);

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, pseye, NULL);

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
pseye_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_pseye,
		    cfattach_ioconf_pseye, cfdata_ioconf_pseye);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_pseye,
		    cfattach_ioconf_pseye, cfdata_ioconf_pseye);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
