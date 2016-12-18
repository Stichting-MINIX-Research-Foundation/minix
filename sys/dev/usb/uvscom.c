/*	$NetBSD: uvscom.c,v 1.28 2012/02/24 06:48:28 mrg Exp $	*/
/*-
 * Copyright (c) 2001-2002, Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/usb/uvscom.c,v 1.1 2002/03/18 18:23:39 joe Exp $
 */

/*
 * uvscom: SUNTAC Slipper U VS-10U driver.
 * Slipper U is a PC card to USB converter for data communication card
 * adapter.  It supports DDI Pocket's Air H" C@rd, C@rd H" 64, NTT's P-in,
 * P-in m@ater and various data communication card adapters.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvscom.c,v 1.28 2012/02/24 06:48:28 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

#ifdef UVSCOM_DEBUG
static int	uvscomdebug = 1;

#define DPRINTFN(n, x)  do { \
				if (uvscomdebug > (n)) \
					printf x; \
			} while (0)
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define	UVSCOM_CONFIG_INDEX	0
#define	UVSCOM_IFACE_INDEX	0

#define UVSCOM_INTR_INTERVAL	100	/* mS */

#define UVSCOM_UNIT_WAIT	5

/* Request */
#define UVSCOM_SET_SPEED	0x10
#define UVSCOM_LINE_CTL		0x11
#define UVSCOM_SET_PARAM	0x12
#define UVSCOM_READ_STATUS	0xd0
#define UVSCOM_SHUTDOWN		0xe0

/* UVSCOM_SET_SPEED parameters */
#define UVSCOM_SPEED_150BPS	0x00
#define UVSCOM_SPEED_300BPS	0x01
#define UVSCOM_SPEED_600BPS	0x02
#define UVSCOM_SPEED_1200BPS	0x03
#define UVSCOM_SPEED_2400BPS	0x04
#define UVSCOM_SPEED_4800BPS	0x05
#define UVSCOM_SPEED_9600BPS	0x06
#define UVSCOM_SPEED_19200BPS	0x07
#define UVSCOM_SPEED_38400BPS	0x08
#define UVSCOM_SPEED_57600BPS	0x09
#define UVSCOM_SPEED_115200BPS	0x0a

/* UVSCOM_LINE_CTL parameters */
#define UVSCOM_BREAK		0x40
#define UVSCOM_RTS		0x02
#define UVSCOM_DTR		0x01
#define UVSCOM_LINE_INIT	0x08

/* UVSCOM_SET_PARAM parameters */
#define UVSCOM_DATA_MASK	0x03
#define UVSCOM_DATA_BIT_8	0x03
#define UVSCOM_DATA_BIT_7	0x02
#define UVSCOM_DATA_BIT_6	0x01
#define UVSCOM_DATA_BIT_5	0x00

#define UVSCOM_STOP_MASK	0x04
#define UVSCOM_STOP_BIT_2	0x04
#define UVSCOM_STOP_BIT_1	0x00

#define UVSCOM_PARITY_MASK	0x18
#define UVSCOM_PARITY_EVEN	0x18
#if 0
#define UVSCOM_PARITY_UNK	0x10
#endif
#define UVSCOM_PARITY_ODD	0x08
#define UVSCOM_PARITY_NONE	0x00

/* Status bits */
#define UVSCOM_TXRDY		0x04
#define UVSCOM_RXRDY		0x01

#define UVSCOM_DCD		0x08
#define UVSCOM_NOCARD		0x04
#define UVSCOM_DSR		0x02
#define UVSCOM_CTS		0x01
#define UVSCOM_USTAT_MASK	(UVSCOM_NOCARD | UVSCOM_DSR | UVSCOM_CTS)

struct	uvscom_softc {
	device_t		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* USB device */
	usbd_interface_handle	sc_iface;	/* interface */
	int			sc_iface_number;/* interface number */

	usbd_interface_handle	sc_intr_iface;	/* interrupt interface */
	int			sc_intr_number;	/* interrupt number */
	usbd_pipe_handle	sc_intr_pipe;	/* interrupt pipe */
	u_char			*sc_intr_buf;	/* interrupt buffer */
	int			sc_isize;

	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */

	u_char			sc_lsr;		/* Local status register */
	u_char			sc_msr;		/* uvscom status register */

	uint16_t		sc_lcr;		/* Line control */
	u_char			sc_usr;		/* unit status */

	device_t		sc_subdev;	/* ucom device */
	u_char			sc_dying;	/* disconnecting */
};

/*
 * These are the maximum number of bytes transferred per frame.
 * The output buffer size cannot be increased due to the size encoding.
 */
#define UVSCOMIBUFSIZE 512
#define UVSCOMOBUFSIZE 64

Static	usbd_status uvscom_readstat(struct uvscom_softc *);
Static	usbd_status uvscom_shutdown(struct uvscom_softc *);
Static	usbd_status uvscom_reset(struct uvscom_softc *);
Static	usbd_status uvscom_set_line_coding(struct uvscom_softc *,
					   uint16_t, uint16_t);
Static	usbd_status uvscom_set_line(struct uvscom_softc *, uint16_t);
Static	usbd_status uvscom_set_crtscts(struct uvscom_softc *);
Static	void uvscom_get_status(void *, int, u_char *, u_char *);
Static	void uvscom_dtr(struct uvscom_softc *, int);
Static	void uvscom_rts(struct uvscom_softc *, int);
Static	void uvscom_break(struct uvscom_softc *, int);

Static	void uvscom_set(void *, int, int, int);
Static	void uvscom_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static	int  uvscom_param(void *, int, struct termios *);
Static	int  uvscom_open(void *, int);
Static	void uvscom_close(void *, int);

struct ucom_methods uvscom_methods = {
	uvscom_get_status,
	uvscom_set,
	uvscom_param,
	NULL, /* uvscom_ioctl, TODO */
	uvscom_open,
	uvscom_close,
	NULL,
	NULL
};

static const struct usb_devno uvscom_devs [] = {
	/* SUNTAC U-Cable type A4 */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_AS144L4 },
	/* SUNTAC U-Cable type D2 */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_DS96L },
	/* SUNTAC U-Cable type P1 */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_PS64P1 },
	/* SUNTAC Slipper U  */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_VS10U },
	/* SUNTAC Ir-Trinity */
	{ USB_VENDOR_SUNTAC, USB_PRODUCT_SUNTAC_IS96U },
};
#define uvscom_lookup(v, p) usb_lookup(uvscom_devs, v, p)

int uvscom_match(device_t, cfdata_t, void *);
void uvscom_attach(device_t, device_t, void *);
void uvscom_childdet(device_t, device_t);
int uvscom_detach(device_t, int);
int uvscom_activate(device_t, enum devact);
extern struct cfdriver uvscom_cd;
CFATTACH_DECL2_NEW(uvscom, sizeof(struct uvscom_softc), uvscom_match,
    uvscom_attach, uvscom_detach, uvscom_activate, NULL, uvscom_childdet);

int 
uvscom_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (uvscom_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void 
uvscom_attach(device_t parent, device_t self, void *aux)
{
	struct uvscom_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	usbd_status err;
	int i;
	struct ucom_attach_args uca;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dev = self;
        sc->sc_udev = dev;

	DPRINTF(("uvscom attach: sc = %p\n", sc));

	/* initialize endpoints */
	uca.bulkin = uca.bulkout = -1;
	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UVSCOM_CONFIG_INDEX, 1);
	if (err) {
		aprint_error_dev(self, "failed to set configuration, err=%s\n",
		    usbd_errstr(err));
		sc->sc_dying = 1;
		return;
	}

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(sc->sc_udev);

	if (cdesc == NULL) {
		aprint_error_dev(self,
		    "failed to get configuration descriptor\n");
		sc->sc_dying = 1;
		return;
	}

	/* get the common interface */
	err = usbd_device2interface_handle(dev, UVSCOM_IFACE_INDEX,
					   &sc->sc_iface);
	if (err) {
		aprint_error_dev(self, "failed to get interface, err=%s\n",
		    usbd_errstr(err));
		sc->sc_dying = 1;
		return;
	}

	id = usbd_get_interface_descriptor(sc->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;

	/* Find endpoints */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "no endpoint descriptor for %d\n", i);
			sc->sc_dying = 1;
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			uca.bulkin = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			uca.bulkout = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_number = ed->bEndpointAddress;
			sc->sc_isize = UGETW(ed->wMaxPacketSize);
		}
	}

	if (uca.bulkin == -1) {
		aprint_error_dev(self, "Could not find data bulk in\n");
		sc->sc_dying = 1;
		return;
	}
	if (uca.bulkout == -1) {
		aprint_error_dev(self, "Could not find data bulk out\n");
		sc->sc_dying = 1;
		return;
	}
	if (sc->sc_intr_number == -1) {
		aprint_error_dev(self, "Could not find interrupt in\n");
		sc->sc_dying = 1;
		return;
	}

	sc->sc_dtr = sc->sc_rts = 0;
	sc->sc_lcr = UVSCOM_LINE_INIT;

	uca.portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	uca.ibufsize = UVSCOMIBUFSIZE;
	uca.obufsize = UVSCOMOBUFSIZE;
	uca.ibufsizepad = UVSCOMIBUFSIZE;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = sc->sc_iface;
	uca.methods = &uvscom_methods;
	uca.arg = sc;
	uca.info = NULL;

	err = uvscom_reset(sc);

	if (err) {
		aprint_error_dev(self, "reset failed, %s\n", usbd_errstr(err));
		sc->sc_dying = 1;
		return;
	}

	DPRINTF(("uvscom: in = 0x%x out = 0x%x intr = 0x%x\n",
		 uca.bulkin, uca.bulkout, sc->sc_intr_number));

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	DPRINTF(("uplcom: in=0x%x out=0x%x intr=0x%x\n",
			uca.bulkin, uca.bulkout, sc->sc_intr_number ));
	sc->sc_subdev = config_found_sm_loc(self, "ucombus", NULL, &uca,
					    ucomprint, ucomsubmatch);

	return;
}

void
uvscom_childdet(device_t self, device_t child)
{
	struct uvscom_softc *sc = device_private(self);

	KASSERT(sc->sc_subdev == child);
	sc->sc_subdev = NULL;
}

int 
uvscom_detach(device_t self, int flags)
{
	struct uvscom_softc *sc = device_private(self);
	int rv = 0;

	DPRINTF(("uvscom_detach: sc = %p\n", sc));

	sc->sc_dying = 1;

	if (sc->sc_intr_pipe != NULL) {
		usbd_abort_pipe(sc->sc_intr_pipe);
		usbd_close_pipe(sc->sc_intr_pipe);
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}

	sc->sc_dying = 1;
	if (sc->sc_subdev != NULL)
		rv = config_detach(sc->sc_subdev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (rv);
}

int
uvscom_activate(device_t self, enum devact act)
{
	struct uvscom_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

Static usbd_status
uvscom_readstat(struct uvscom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;
	uint16_t r;

	DPRINTF(("%s: send readstat\n", device_xname(sc->sc_dev)));

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UVSCOM_READ_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->sc_udev, &req, &r);
	if (err) {
		aprint_error_dev(sc->sc_dev, "uvscom_readstat: %s\n",
		    usbd_errstr(err));
		return (err);
	}

	DPRINTF(("%s: uvscom_readstat: r = %d\n",
		 device_xname(sc->sc_dev), r));

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
uvscom_shutdown(struct uvscom_softc *sc)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("%s: send shutdown\n", device_xname(sc->sc_dev)));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_SHUTDOWN;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err) {
		aprint_error_dev(sc->sc_dev, "uvscom_shutdown: %s\n",
		   usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
uvscom_reset(struct uvscom_softc *sc)
{
	DPRINTF(("%s: uvscom_reset\n", device_xname(sc->sc_dev)));

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
uvscom_set_crtscts(struct uvscom_softc *sc)
{
	DPRINTF(("%s: uvscom_set_crtscts\n", device_xname(sc->sc_dev)));

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
uvscom_set_line(struct uvscom_softc *sc, uint16_t line)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("%s: uvscom_set_line: %04x\n",
		 device_xname(sc->sc_dev), line));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_LINE_CTL;
	USETW(req.wValue, line);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err) {
		aprint_error_dev(sc->sc_dev, "uvscom_set_line: %s\n",
		    usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
uvscom_set_line_coding(struct uvscom_softc *sc, uint16_t lsp, uint16_t ls)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("%s: uvscom_set_line_coding: %02x %02x\n",
		 device_xname(sc->sc_dev), lsp, ls));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_SET_SPEED;
	USETW(req.wValue, lsp);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err) {
		aprint_error_dev(sc->sc_dev, "uvscom_set_line_coding: %s\n",
		   usbd_errstr(err));
		return (err);
	}

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UVSCOM_SET_PARAM;
	USETW(req.wValue, ls);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err) {
		aprint_error_dev(sc->sc_dev, "uvscom_set_line_coding: %s\n",
		   usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static void
uvscom_dtr(struct uvscom_softc *sc, int onoff)
{
	DPRINTF(("%s: uvscom_dtr: onoff = %d\n",
		 device_xname(sc->sc_dev), onoff));

	if (sc->sc_dtr == onoff)
		return;			/* no change */

	sc->sc_dtr = onoff;

	if (onoff)
		SET(sc->sc_lcr, UVSCOM_DTR);
	else
		CLR(sc->sc_lcr, UVSCOM_DTR);

	uvscom_set_line(sc, sc->sc_lcr);
}

Static void
uvscom_rts(struct uvscom_softc *sc, int onoff)
{
	DPRINTF(("%s: uvscom_rts: onoff = %d\n",
		 device_xname(sc->sc_dev), onoff));

	if (sc->sc_rts == onoff)
		return;			/* no change */

	sc->sc_rts = onoff;

	if (onoff)
		SET(sc->sc_lcr, UVSCOM_RTS);
	else
		CLR(sc->sc_lcr, UVSCOM_RTS);

	uvscom_set_line(sc, sc->sc_lcr);
}

Static void
uvscom_break(struct uvscom_softc *sc, int onoff)
{
	DPRINTF(("%s: uvscom_break: onoff = %d\n",
		 device_xname(sc->sc_dev), onoff));

	if (onoff)
		uvscom_set_line(sc, SET(sc->sc_lcr, UVSCOM_BREAK));
}

Static void
uvscom_set(void *addr, int portno, int reg, int onoff)
{
	struct uvscom_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		uvscom_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		uvscom_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		uvscom_break(sc, onoff);
		break;
	default:
		break;
	}
}

Static int
uvscom_param(void *addr, int portno, struct termios *t)
{
	struct uvscom_softc *sc = addr;
	usbd_status err;
	uint16_t lsp;
	uint16_t ls;

	DPRINTF(("%s: uvscom_param: sc = %p\n",
		 device_xname(sc->sc_dev), sc));

	ls = 0;

	switch (t->c_ospeed) {
	case B150:
		lsp = UVSCOM_SPEED_150BPS;
		break;
	case B300:
		lsp = UVSCOM_SPEED_300BPS;
		break;
	case B600:
		lsp = UVSCOM_SPEED_600BPS;
		break;
	case B1200:
		lsp = UVSCOM_SPEED_1200BPS;
		break;
	case B2400:
		lsp = UVSCOM_SPEED_2400BPS;
		break;
	case B4800:
		lsp = UVSCOM_SPEED_4800BPS;
		break;
	case B9600:
		lsp = UVSCOM_SPEED_9600BPS;
		break;
	case B19200:
		lsp = UVSCOM_SPEED_19200BPS;
		break;
	case B38400:
		lsp = UVSCOM_SPEED_38400BPS;
		break;
	case B57600:
		lsp = UVSCOM_SPEED_57600BPS;
		break;
	case B115200:
		lsp = UVSCOM_SPEED_115200BPS;
		break;
	default:
		return (EIO);
	}

	if (ISSET(t->c_cflag, CSTOPB))
		SET(ls, UVSCOM_STOP_BIT_2);
	else
		SET(ls, UVSCOM_STOP_BIT_1);

	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			SET(ls, UVSCOM_PARITY_ODD);
		else
			SET(ls, UVSCOM_PARITY_EVEN);
	} else
		SET(ls, UVSCOM_PARITY_NONE);

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		SET(ls, UVSCOM_DATA_BIT_5);
		break;
	case CS6:
		SET(ls, UVSCOM_DATA_BIT_6);
		break;
	case CS7:
		SET(ls, UVSCOM_DATA_BIT_7);
		break;
	case CS8:
		SET(ls, UVSCOM_DATA_BIT_8);
		break;
	default:
		return (EIO);
	}

	err = uvscom_set_line_coding(sc, lsp, ls);
	if (err)
		return (EIO);

	if (ISSET(t->c_cflag, CRTSCTS)) {
		err = uvscom_set_crtscts(sc);
		if (err)
			return (EIO);
	}

	return (0);
}

Static int
uvscom_open(void *addr, int portno)
{
	struct uvscom_softc *sc = addr;
	int err;
	int i;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("uvscom_open: sc = %p\n", sc));

	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		DPRINTF(("uvscom_open: open interrupt pipe.\n"));

		sc->sc_usr = 0;		/* clear unit status */

		err = uvscom_readstat(sc);
		if (err) {
			DPRINTF(("%s: uvscom_open: readstat faild\n",
				 device_xname(sc->sc_dev)));
			return (EIO);
		}

		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		err = usbd_open_pipe_intr(sc->sc_iface,
					  sc->sc_intr_number,
					  USBD_SHORT_XFER_OK,
					  &sc->sc_intr_pipe,
					  sc,
					  sc->sc_intr_buf,
					  sc->sc_isize,
					  uvscom_intr,
					  UVSCOM_INTR_INTERVAL);
		if (err) {
			aprint_error_dev(sc->sc_dev,
			    "cannot open interrupt pipe (addr %d)\n",
				 sc->sc_intr_number);
			return (EIO);
		}
	} else {
		DPRINTF(("uvscom_open: did not open interrupt pipe.\n"));
	}

	if ((sc->sc_usr & UVSCOM_USTAT_MASK) == 0) {
		/* unit is not ready */

		for (i = UVSCOM_UNIT_WAIT; i > 0; --i) {
			tsleep(&err, TTIPRI, "uvsop", hz);	/* XXX */
			if (ISSET(sc->sc_usr, UVSCOM_USTAT_MASK))
				break;
		}
		if (i == 0) {
			DPRINTF(("%s: unit is not ready\n",
				 device_xname(sc->sc_dev)));
			return (EIO);
		}

		/* check PC card was inserted */
		if (ISSET(sc->sc_usr, UVSCOM_NOCARD)) {
			DPRINTF(("%s: no card\n",
				 device_xname(sc->sc_dev)));
			return (EIO);
		}
	}

	return (0);
}

Static void
uvscom_close(void *addr, int portno)
{
	struct uvscom_softc *sc = addr;
	int err;

	if (sc->sc_dying)
		return;

	DPRINTF(("uvscom_close: close\n"));

	uvscom_shutdown(sc);

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			aprint_error_dev(sc->sc_dev,
			    "abort interrupt pipe failed: %s\n",
			    usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			aprint_error_dev(sc->sc_dev,
			    "lose interrupt pipe failed: %s\n",
			    usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
}

Static void
uvscom_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct uvscom_softc *sc = priv;
	u_char *buf = sc->sc_intr_buf;
	u_char pstatus;

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		aprint_error_dev(sc->sc_dev,
		    "uvscom_intr: abnormal status: %s\n",
		    usbd_errstr(status));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	DPRINTFN(2, ("%s: uvscom status = %02x %02x\n",
		 device_xname(sc->sc_dev), buf[0], buf[1]));

	sc->sc_lsr = sc->sc_msr = 0;
	sc->sc_usr = buf[1];

	pstatus = buf[0];
	if (ISSET(pstatus, UVSCOM_TXRDY))
		SET(sc->sc_lsr, ULSR_TXRDY);
	if (ISSET(pstatus, UVSCOM_RXRDY))
		SET(sc->sc_lsr, ULSR_RXRDY);

	pstatus = buf[1];
	if (ISSET(pstatus, UVSCOM_CTS))
		SET(sc->sc_msr, UMSR_CTS);
	if (ISSET(pstatus, UVSCOM_DSR))
		SET(sc->sc_msr, UMSR_DSR);
	if (ISSET(pstatus, UVSCOM_DCD))
		SET(sc->sc_msr, UMSR_DCD);

	ucom_status_change(device_private(sc->sc_subdev));
}

Static void
uvscom_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct uvscom_softc *sc = addr;

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

