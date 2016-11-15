/* $NetBSD: uslsa.c,v 1.19 2014/03/10 19:55:18 reinoud Exp $ */

/* from ugensa.c */

/*
 * Copyright (c) 2004, 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell <elric@netbsd.org>.
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
 * Copyright (c) 2007, 2009 Jonathan A. Kollasch.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uslsa.c,v 1.19 2014/03/10 19:55:18 reinoud Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ucomvar.h>

#include <dev/usb/uslsareg.h>

#include <fs/unicode.h>

#ifdef USLSA_DEBUG
#define DPRINTF(x)	if (uslsadebug) device_printf x
int uslsadebug = 0;
#else
#define DPRINTF(x)
#endif

struct uslsa_softc {
	device_t		sc_dev;		/* base device */
	device_t		sc_subdev;	/* ucom device */
	usbd_device_handle	sc_udev;	/* usb device */
	usbd_interface_handle	sc_iface;	/* interface */
	uint8_t			sc_ifnum;	/* interface number */
	bool			sc_dying;	/* disconnecting */
};

static void uslsa_get_status(void *sc, int, u_char *, u_char *);
static void uslsa_set(void *, int, int, int);
static int uslsa_param(void *, int, struct termios *);
static int uslsa_ioctl(void *, int, u_long, void *, int, proc_t *);

static int uslsa_open(void *, int);
static void uslsa_close(void *, int);

static int uslsa_usbd_errno(usbd_status);
static int uslsa_request_set(struct uslsa_softc *, uint8_t, uint16_t);
static int uslsa_set_flow(struct uslsa_softc *, tcflag_t, tcflag_t);

static const struct ucom_methods uslsa_methods = {
	uslsa_get_status,
	uslsa_set,
	uslsa_param,
	uslsa_ioctl,
	uslsa_open,
	uslsa_close,
	NULL,
	NULL,
};

#define USLSA_CONFIG_INDEX	0
#define USLSA_IFACE_INDEX	0
#define USLSA_BUFSIZE		256

static const struct usb_devno uslsa_devs[] = {
        { USB_VENDOR_BALTECH,           USB_PRODUCT_BALTECH_CARDREADER },
        { USB_VENDOR_DYNASTREAM,        USB_PRODUCT_DYNASTREAM_ANTDEVBOARD },
        { USB_VENDOR_JABLOTRON,         USB_PRODUCT_JABLOTRON_PC60B },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_ARGUSISP },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_CRUMB128 },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_DEGREECONT },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_DESKTOPMOBILE },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_IPLINK1220 },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_LIPOWSKY_HARP },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_LIPOWSKY_JTAG },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_LIPOWSKY_LIN },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_POLOLU },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_CP210X_1 },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_CP210X_2 },
        { USB_VENDOR_SILABS,            USB_PRODUCT_SILABS_SUNNTO },
        { USB_VENDOR_SILABS2,           USB_PRODUCT_SILABS2_DCU11CLONE },
        { USB_VENDOR_USI,               USB_PRODUCT_USI_MC60 }
};

static int uslsa_match(device_t, cfdata_t, void *);
static void uslsa_attach(device_t, device_t, void *);
static void uslsa_childdet(device_t, device_t);
static int uslsa_detach(device_t, int);
static int uslsa_activate(device_t, enum devact);

CFATTACH_DECL2_NEW(uslsa, sizeof(struct uslsa_softc), uslsa_match,
    uslsa_attach, uslsa_detach, uslsa_activate, NULL, uslsa_childdet);

static int
uslsa_match(device_t parent, cfdata_t match, void *aux)
{
	const struct usbif_attach_arg *uaa;

	uaa = aux;

	if (usb_lookup(uslsa_devs, uaa->vendor, uaa->product) != NULL) {
		return UMATCH_VENDOR_PRODUCT;
	} else {
		return UMATCH_NONE;
	}
}

static void
uslsa_attach(device_t parent, device_t self, void *aux)
{
	struct uslsa_softc *sc;
	const struct usbif_attach_arg *uaa;
	const usb_interface_descriptor_t *id;
	const usb_endpoint_descriptor_t *ed;
	char *devinfop;
	struct ucom_attach_args uca;
	int i;

	sc = device_private(self);
	uaa = aux;

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	sc->sc_iface = uaa->iface;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	id = usbd_get_interface_descriptor(sc->sc_iface);

	sc->sc_ifnum = id->bInterfaceNumber;

	uca.info = "Silicon Labs CP210x";
	uca.portno = UCOM_UNK_PORTNO;
	uca.ibufsize = USLSA_BUFSIZE;
	uca.obufsize = USLSA_BUFSIZE;
	uca.ibufsizepad = USLSA_BUFSIZE;
	uca.opkthdrlen = 0;
	uca.device = sc->sc_udev;
	uca.iface = sc->sc_iface;
	uca.methods = &uslsa_methods;
	uca.arg = sc;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	                   sc->sc_dev);

	uca.bulkin = uca.bulkout = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		int addr, dir, attr;

		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "could not read endpoint descriptor\n");
			sc->sc_dying = true;
			return;
		}
		addr = ed->bEndpointAddress;
		dir = UE_GET_DIR(ed->bEndpointAddress);
		attr = ed->bmAttributes & UE_XFERTYPE;
		if (dir == UE_DIR_IN && attr == UE_BULK) {
			uca.bulkin = addr;
		} else if (dir == UE_DIR_OUT && attr == UE_BULK) {
			uca.bulkout = addr;
		} else {
			aprint_error_dev(self, "unexpected endpoint\n");
		}
	}
	aprint_debug_dev(sc->sc_dev, "EPs: in=%#x out=%#x\n",
		uca.bulkin, uca.bulkout);
	if ((uca.bulkin == -1) || (uca.bulkout == -1)) {
		aprint_error_dev(self, "could not find endpoints\n");
		sc->sc_dying = true;
		return;
	}

	sc->sc_subdev = config_found_sm_loc(self, "ucombus", NULL, &uca,
	                                    ucomprint, ucomsubmatch);

	return;
}

static int
uslsa_activate(device_t self, enum devact act)
{
	struct uslsa_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = true;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static void
uslsa_childdet(device_t self, device_t child)
{
	struct uslsa_softc *sc = device_private(self);

	KASSERT(sc->sc_subdev == child);
	sc->sc_subdev = NULL;
}

static int
uslsa_detach(device_t self, int flags)
{
	struct uslsa_softc *sc = device_private(self);
	int rv = 0;

	DPRINTF((self, "%s(%p, %#x)\n", __func__, self, flags));

	sc->sc_dying = true;

	if (sc->sc_subdev != NULL) {
		rv = config_detach(sc->sc_subdev, flags);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	                   sc->sc_dev);

	return (rv);
}

static int
uslsa_usbd_errno(usbd_status status)
{
	switch (status) {
	case USBD_NORMAL_COMPLETION:
		return 0;
	case USBD_STALLED:
		return EINVAL;
	default:
		return EIO;
	}
}

static void
uslsa_get_status(void *vsc, int portno, u_char *lsr, u_char *msr)
{
	struct uslsa_softc *sc;
	usb_device_request_t req;
	usbd_status status;
	uint8_t mdmsts;

	sc = vsc;

	DPRINTF((sc->sc_dev, "%s(%p, %d, ....)\n", __func__, vsc, portno));

	if (sc->sc_dying) {
		return;
	}

	req.bmRequestType = UT_READ_VENDOR_INTERFACE;
	req.bRequest = SLSA_R_GET_MDMSTS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifnum);
	USETW(req.wLength, SLSA_RL_GET_MDMSTS);

	status = usbd_do_request(sc->sc_udev, &req, &mdmsts);
	if (status != USBD_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "%s: GET_MDMSTS %s\n",
		    __func__, usbd_errstr(status));
		return;
	}

	DPRINTF((sc->sc_dev, "%s: GET_MDMSTS %#x\n", __func__, mdmsts));

	if (lsr != NULL) {
		*lsr = 0;
	}

	if (msr != NULL) {
		*msr = 0;
		*msr |= ISSET(mdmsts, SLSA_MDMSTS_CTS) ? UMSR_CTS : 0;
		*msr |= ISSET(mdmsts, SLSA_MDMSTS_DSR) ? UMSR_DSR : 0;
		*msr |= ISSET(mdmsts, SLSA_MDMSTS_RI) ? UMSR_RI : 0;
		*msr |= ISSET(mdmsts, SLSA_MDMSTS_DCD) ? UMSR_DCD : 0;
	}
}

static void
uslsa_set(void *vsc, int portno, int reg, int onoff)
{
	struct uslsa_softc *sc;

	sc = vsc;

	DPRINTF((sc->sc_dev, "%s(%p, %d, %d, %d)\n", __func__, vsc, portno, reg, onoff));

	if (sc->sc_dying) {
		return;
	}

	switch (reg) {
	case UCOM_SET_DTR:
		if (uslsa_request_set(sc, SLSA_R_SET_MHS,
		    SLSA_RV_SET_MHS_DTR_MASK |
		    (onoff ? SLSA_RV_SET_MHS_DTR : 0))) {
			device_printf(sc->sc_dev, "SET_MHS/DTR failed\n");
		}
		break;
	case UCOM_SET_RTS:
		if (uslsa_request_set(sc, SLSA_R_SET_MHS,
		    SLSA_RV_SET_MHS_RTS_MASK |
		    (onoff ? SLSA_RV_SET_MHS_RTS : 0))) {
			device_printf(sc->sc_dev, "SET_MHS/RTS failed\n");
		}
		break;
	case UCOM_SET_BREAK:
		if (uslsa_request_set(sc, SLSA_R_SET_BREAK,
		    (onoff ? SLSA_RV_SET_BREAK_ENABLE :
		     SLSA_RV_SET_BREAK_DISABLE))) {
			device_printf(sc->sc_dev, "SET_BREAK failed\n");
		}
		break;
	default:
		break;
	}
}

static int
uslsa_param(void *vsc, int portno, struct termios *t)
{
	struct uslsa_softc *sc;
	usb_device_request_t req;
	usbd_status status;
	uint16_t value;
	uint32_t baud;
	int ret;

	sc = vsc;

	DPRINTF((sc->sc_dev, "%s(%p, %d, %p)\n", __func__, vsc, portno, t));

	if (sc->sc_dying) {
		return EIO;
	}

	req.bmRequestType = UT_WRITE_VENDOR_INTERFACE;
	req.bRequest = SLSA_R_SET_BAUDRATE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifnum);
	USETW(req.wLength, 4);

	baud = t->c_ospeed;
	status = usbd_do_request(sc->sc_udev, &req, &baud);
	if (status != USBD_NORMAL_COMPLETION) {
		/* fallback method for devices that don't know SET_BAUDRATE */
		/* hope we calculate it right */
		device_printf(sc->sc_dev, "%s: set baudrate %d, failed %s,"
				" using set bauddiv\n",
		    __func__, baud, usbd_errstr(status));

		value = SLSA_RV_BAUDDIV(t->c_ospeed);
		if ((ret = uslsa_request_set(sc, SLSA_R_SET_BAUDDIV, value)) != 0) {
			device_printf(sc->sc_dev, "%s: SET_BAUDDIV failed\n",
			       __func__);
			return ret;
		}
	}

	value = 0;

	if (ISSET(t->c_cflag, CSTOPB)) {
		value |= SLSA_RV_LINE_CTL_STOP_2;
	} else {
		value |= SLSA_RV_LINE_CTL_STOP_1;
	}

	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD)) {
			value |= SLSA_RV_LINE_CTL_PARITY_ODD;
		} else {
			value |= SLSA_RV_LINE_CTL_PARITY_EVEN;
		}
	} else {
		value |= SLSA_RV_LINE_CTL_PARITY_NONE;
	}

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		value |= SLSA_RV_LINE_CTL_LEN_5;
		break;
	case CS6:
		value |= SLSA_RV_LINE_CTL_LEN_6;
		break;
	case CS7:
		value |= SLSA_RV_LINE_CTL_LEN_7;
		break;
	case CS8:
		value |= SLSA_RV_LINE_CTL_LEN_8;
		break;
	}

	DPRINTF((sc->sc_dev, "%s: setting LINE_CTL to 0x%x\n",
	    __func__, value));
	if ((ret = uslsa_request_set(sc, SLSA_R_SET_LINE_CTL, value)) != 0) {
		device_printf(sc->sc_dev, "SET_LINE_CTL failed\n");
		return ret;
	}

	if ((ret = uslsa_set_flow(sc, t->c_cflag, t->c_iflag)) != 0) {
		device_printf(sc->sc_dev, "SET_LINE_CTL failed\n");
	}

	return ret;
}

static int
uslsa_ioctl(void *vsc, int portno, u_long cmd, void *data, int flag, proc_t *p)
{
	struct uslsa_softc *sc;

	sc = vsc;

	switch (cmd) {
	case TIOCMGET:
		ucom_status_change(device_private(sc->sc_subdev));
		return EPASSTHROUGH;
	default:
		return EPASSTHROUGH;
	}

	return 0;
}

static int
uslsa_open(void *vsc, int portno)
{
	struct uslsa_softc *sc;

	sc = vsc;

	DPRINTF((sc->sc_dev, "%s(%p, %d)\n", __func__, vsc, portno));

	if (sc->sc_dying) {
		return EIO;
	}

	return uslsa_request_set(sc, SLSA_R_IFC_ENABLE,
	    SLSA_RV_IFC_ENABLE_ENABLE);
}

static void
uslsa_close(void *vsc, int portno)
{
	struct uslsa_softc *sc;

	sc = vsc;

	DPRINTF((sc->sc_dev, "%s(%p, %d)\n", __func__, vsc, portno));

	if (sc->sc_dying) {
		return;
	}

	(void)uslsa_request_set(sc, SLSA_R_IFC_ENABLE,
	    SLSA_RV_IFC_ENABLE_DISABLE);
}

static int
uslsa_request_set(struct uslsa_softc * sc, uint8_t request, uint16_t value)
{
	usb_device_request_t req;
	usbd_status status;

	req.bmRequestType = UT_WRITE_VENDOR_INTERFACE;
	req.bRequest = request;
	USETW(req.wValue, value);
	USETW(req.wIndex, sc->sc_ifnum);
	USETW(req.wLength, 0);

	status = usbd_do_request(sc->sc_udev, &req, NULL);

	return uslsa_usbd_errno(status);
}

static int
uslsa_set_flow(struct uslsa_softc *sc, tcflag_t cflag, tcflag_t iflag)
{
	struct slsa_fcs fcs;
	usb_device_request_t req;
	uint32_t ulControlHandshake;
	uint32_t ulFlowReplace;
	usbd_status status;

	DPRINTF((sc->sc_dev, "%s(%p, %#x, %#x)\n", __func__, sc, cflag, iflag));

	req.bmRequestType = UT_READ_VENDOR_INTERFACE;
	req.bRequest = SLSA_R_GET_FLOW;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifnum);
	USETW(req.wLength, SLSA_RL_GET_FLOW);

	status = usbd_do_request(sc->sc_udev, &req, &fcs);
	if (status != USBD_NORMAL_COMPLETION) {
		device_printf(sc->sc_dev, "%s: GET_FLOW %s\n",
			__func__, usbd_errstr(status));
		return uslsa_usbd_errno(status);
	}

	ulControlHandshake = le32toh(fcs.ulControlHandshake);
	ulFlowReplace = le32toh(fcs.ulFlowReplace);

	if (ISSET(cflag, CRTSCTS)) {
		ulControlHandshake =
		    SERIAL_CTS_HANDSHAKE | __SHIFTIN(1, SERIAL_DTR_MASK);
		ulFlowReplace = __SHIFTIN(2, SERIAL_RTS_MASK);
	} else {
		ulControlHandshake = __SHIFTIN(1, SERIAL_DTR_MASK);
		ulFlowReplace = __SHIFTIN(1, SERIAL_RTS_MASK);
	}

	fcs.ulControlHandshake = htole32(ulControlHandshake);
	fcs.ulFlowReplace = htole32(ulFlowReplace);

	req.bmRequestType = UT_WRITE_VENDOR_INTERFACE;
	req.bRequest = SLSA_R_SET_FLOW;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifnum);
	USETW(req.wLength, SLSA_RL_SET_FLOW);

	status = usbd_do_request(sc->sc_udev, &req, &fcs);

	return uslsa_usbd_errno(status);
}
