/*	$NetBSD: umct.c,v 1.32 2011/12/23 00:51:47 jakllsch Exp $	*/
/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org).
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
 * MCT USB-RS232 Interface Controller
 * http://www.mct.com.tw/prod/rs232.html
 * http://www.dlink.com/products/usb/dsbs25
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umct.c,v 1.32 2011/12/23 00:51:47 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>
#include <dev/usb/umct.h>

#ifdef UMCT_DEBUG
#define DPRINTFN(n, x)  if (umctdebug > (n)) printf x
int	umctdebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define	UMCT_CONFIG_INDEX	0
#define	UMCT_IFACE_INDEX	0

struct	umct_softc {
	device_t		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* USB device */
	usbd_interface_handle	sc_iface;	/* interface */
	int			sc_iface_number;	/* interface number */
	u_int16_t		sc_product;

	int			sc_intr_number;	/* interrupt number */
	usbd_pipe_handle	sc_intr_pipe;	/* interrupt pipe */
	u_char			*sc_intr_buf;	/* interrupt buffer */
	int			sc_isize;

	usb_cdc_line_state_t	sc_line_state;	/* current line state */
	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */
	u_char			sc_break;	/* set break */

	u_char			sc_status;

	device_t		sc_subdev;	/* ucom device */

	u_char			sc_dying;	/* disconnecting */

	u_char			sc_lsr;		/* Local status register */
	u_char			sc_msr;		/* umct status register */

	u_int			last_lcr;	/* keep lcr register */
};

/*
 * These are the maximum number of bytes transferred per frame.
 * The output buffer size cannot be increased due to the size encoding.
 */
#define UMCTIBUFSIZE 256
#define UMCTOBUFSIZE 256

Static	void umct_init(struct umct_softc *);
Static	void umct_set_baudrate(struct umct_softc *, u_int);
Static	void umct_set_lcr(struct umct_softc *, u_int);
Static	void umct_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static	void umct_set(void *, int, int, int);
Static	void umct_dtr(struct umct_softc *, int);
Static	void umct_rts(struct umct_softc *, int);
Static	void umct_break(struct umct_softc *, int);
Static	void umct_set_line_state(struct umct_softc *);
Static	void umct_get_status(void *, int portno, u_char *lsr, u_char *msr);
Static	int  umct_param(void *, int, struct termios *);
Static	int  umct_open(void *, int);
Static	void umct_close(void *, int);

struct	ucom_methods umct_methods = {
	umct_get_status,
	umct_set,
	umct_param,
	NULL,
	umct_open,
	umct_close,
	NULL,
	NULL,
};

static const struct usb_devno umct_devs[] = {
	/* MCT USB-232 Interface Products */
	{ USB_VENDOR_MCT, USB_PRODUCT_MCT_USB232 },
	/* Sitecom USB-232 Products */
	{ USB_VENDOR_MCT, USB_PRODUCT_MCT_SITECOM_USB232 },
	/* D-Link DU-H3SP USB BAY Hub Products */
	{ USB_VENDOR_MCT, USB_PRODUCT_MCT_DU_H3SP_USB232 },
	/* BELKIN F5U109 */
	{ USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U109 },
};
#define umct_lookup(v, p) usb_lookup(umct_devs, v, p)

int umct_match(device_t, cfdata_t, void *);
void umct_attach(device_t, device_t, void *);
void umct_childdet(device_t, device_t);
int umct_detach(device_t, int);
int umct_activate(device_t, enum devact);
extern struct cfdriver umct_cd;
CFATTACH_DECL2_NEW(umct, sizeof(struct umct_softc), umct_match,
    umct_attach, umct_detach, umct_activate, NULL, umct_childdet);

int 
umct_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (umct_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void 
umct_attach(device_t parent, device_t self, void *aux)
{
	struct umct_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usb_config_descriptor_t *cdesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;

	char *devinfop;
	usbd_status err;
	int i;
	struct ucom_attach_args uca;

	sc->sc_dev = self;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

        sc->sc_udev = dev;
	sc->sc_product = uaa->product;

	DPRINTF(("\n\numct attach: sc=%p\n", sc));

	/* initialize endpoints */
	uca.bulkin = uca.bulkout = -1;
	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UMCT_CONFIG_INDEX, 1);
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

	/* get the interface */
	err = usbd_device2interface_handle(dev, UMCT_IFACE_INDEX,
							&sc->sc_iface);
	if (err) {
		aprint_error_dev(self, "failed to get interface, err=%s\n",
		    usbd_errstr(err));
		sc->sc_dying = 1;
		return;
	}

	/* Find the bulk{in,out} and interrupt endpoints */

	id = usbd_get_interface_descriptor(sc->sc_iface);
	sc->sc_iface_number = id->bInterfaceNumber;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "no endpoint descriptor for %d\n", i);
			sc->sc_dying = 1;
			return;
		}

		/*
		 * The Bulkin endpoint is marked as an interrupt. Since
		 * we can't rely on the endpoint descriptor order, we'll
		 * check the wMaxPacketSize field to differentiate.
		 */
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT &&
		    UGETW(ed->wMaxPacketSize) != 0x2) {
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

	if (sc->sc_intr_number== -1) {
		aprint_error_dev(self, "Could not find interrupt in\n");
		sc->sc_dying = 1;
		return;
	}

	sc->sc_dtr = sc->sc_rts = 0;
	uca.portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	uca.ibufsize = UMCTIBUFSIZE;
	if (sc->sc_product == USB_PRODUCT_MCT_SITECOM_USB232)
		uca.obufsize = 16; /* device is broken */
	else
		uca.obufsize = UMCTOBUFSIZE;
	uca.ibufsizepad = UMCTIBUFSIZE;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = sc->sc_iface;
	uca.methods = &umct_methods;
	uca.arg = sc;
	uca.info = NULL;

	umct_init(sc);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	DPRINTF(("umct: in=0x%x out=0x%x intr=0x%x\n",
			uca.bulkin, uca.bulkout, sc->sc_intr_number ));
	sc->sc_subdev = config_found_sm_loc(self, "ucombus", NULL, &uca,
					    ucomprint, ucomsubmatch);

	return;
}

void
umct_childdet(device_t self, device_t child)
{
	struct umct_softc *sc = device_private(self);

	KASSERT(sc->sc_subdev == child);
	sc->sc_subdev = NULL;
}

int 
umct_detach(device_t self, int flags)
{
	struct umct_softc *sc = device_private(self);
	int rv = 0;

	DPRINTF(("umct_detach: sc=%p flags=%d\n", sc, flags));

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
umct_activate(device_t self, enum devact act)
{
	struct umct_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

void
umct_set_line_state(struct umct_softc *sc)
{
	usb_device_request_t req;
	uByte ls;

	ls = (sc->sc_dtr ? MCR_DTR : 0) |
	     (sc->sc_rts ? MCR_RTS : 0);

	DPRINTF(("umct_set_line_state: DTR=%d,RTS=%d,ls=%02x\n",
			sc->sc_dtr, sc->sc_rts, ls));

	req.bmRequestType = UMCT_SET_REQUEST;
	req.bRequest = REQ_SET_MCR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, LENGTH_SET_MCR);

	(void)usbd_do_request(sc->sc_udev, &req, &ls);
}

void
umct_set(void *addr, int portno, int reg, int onoff)
{
	struct umct_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		umct_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		umct_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		umct_break(sc, onoff);
		break;
	default:
		break;
	}
}

void
umct_dtr(struct umct_softc *sc, int onoff)
{

	DPRINTF(("umct_dtr: onoff=%d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	umct_set_line_state(sc);
}

void
umct_rts(struct umct_softc *sc, int onoff)
{
	DPRINTF(("umct_rts: onoff=%d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	umct_set_line_state(sc);
}

void
umct_break(struct umct_softc *sc, int onoff)
{
	DPRINTF(("umct_break: onoff=%d\n", onoff));

	umct_set_lcr(sc, onoff ? sc->last_lcr | LCR_SET_BREAK :
		     sc->last_lcr);
}

void
umct_set_lcr(struct umct_softc *sc, u_int data)
{
	usb_device_request_t req;
	uByte adata;

	adata = data;
	req.bmRequestType = UMCT_SET_REQUEST;
	req.bRequest = REQ_SET_LCR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_iface_number);
	USETW(req.wLength, LENGTH_SET_LCR);

	(void)usbd_do_request(sc->sc_udev, &req, &adata); /* XXX should check */
}

void
umct_set_baudrate(struct umct_softc *sc, u_int rate)
{
        usb_device_request_t req;
	uDWord arate;
	u_int val;

	if (sc->sc_product == USB_PRODUCT_MCT_SITECOM_USB232 ||
	    sc->sc_product == USB_PRODUCT_BELKIN_F5U109) {
		switch (rate) {
		case    300: val = 0x01; break;
		case    600: val = 0x02; break;
		case   1200: val = 0x03; break;
		case   2400: val = 0x04; break;
		case   4800: val = 0x06; break;
		case   9600: val = 0x08; break;
		case  19200: val = 0x09; break;
		case  38400: val = 0x0a; break;
		case  57600: val = 0x0b; break;
		case 115200: val = 0x0c; break;
		default:     val = -1; break;
		}
	} else {
		val = UMCT_BAUD_RATE(rate);
	}
	USETDW(arate, val);

        req.bmRequestType = UMCT_SET_REQUEST;
        req.bRequest = REQ_SET_BAUD_RATE;
        USETW(req.wValue, 0);
        USETW(req.wIndex, sc->sc_iface_number);
        USETW(req.wLength, LENGTH_BAUD_RATE);

        (void)usbd_do_request(sc->sc_udev, &req, arate); /* XXX should check */
}

void
umct_init(struct umct_softc *sc)
{
	umct_set_baudrate(sc, 9600);
	umct_set_lcr(sc, LCR_DATA_BITS_8 | LCR_PARITY_NONE | LCR_STOP_BITS_1);
}

int
umct_param(void *addr, int portno, struct termios *t)
{
	struct umct_softc *sc = addr;
	u_int data = 0;

	DPRINTF(("umct_param: sc=%p\n", sc));

	DPRINTF(("umct_param: BAUDRATE=%d\n", t->c_ospeed));

	if (ISSET(t->c_cflag, CSTOPB))
		data |= LCR_STOP_BITS_2;
	else
		data |= LCR_STOP_BITS_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			data |= LCR_PARITY_ODD;
		else
			data |= LCR_PARITY_EVEN;
	} else
		data |= LCR_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		data |= LCR_DATA_BITS_5;
		break;
	case CS6:
		data |= LCR_DATA_BITS_6;
		break;
	case CS7:
		data |= LCR_DATA_BITS_7;
		break;
	case CS8:
		data |= LCR_DATA_BITS_8;
		break;
	}

	umct_set_baudrate(sc, t->c_ospeed);

	sc->last_lcr = data;
	umct_set_lcr(sc, data);

	return (0);
}

int
umct_open(void *addr, int portno)
{
	struct umct_softc *sc = addr;
	int err, lcr_data;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("umct_open: sc=%p\n", sc));

	/* initialize LCR */
        lcr_data = LCR_DATA_BITS_8 | LCR_PARITY_NONE |
	    LCR_STOP_BITS_1;
        umct_set_lcr(sc, lcr_data);

	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_status = 0; /* clear status bit */
		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_intr_number,
			USBD_SHORT_XFER_OK, &sc->sc_intr_pipe, sc,
			sc->sc_intr_buf, sc->sc_isize,
			umct_intr, USBD_DEFAULT_INTERVAL);
		if (err) {
			DPRINTF(("%s: cannot open interrupt pipe (addr %d)\n",
				device_xname(sc->sc_dev), sc->sc_intr_number));
					return (EIO);
		}
	}

	return (0);
}

void
umct_close(void *addr, int portno)
{
	struct umct_softc *sc = addr;
	int err;

	if (sc->sc_dying)
		return;

	DPRINTF(("umct_close: close\n"));

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: abort interrupt pipe failed: %s\n",
				device_xname(sc->sc_dev), usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: close interrupt pipe failed: %s\n",
				device_xname(sc->sc_dev), usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
}

void
umct_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct umct_softc *sc = priv;
	u_char *tbuf = sc->sc_intr_buf;
	u_char mstatus;

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		DPRINTF(("%s: abnormal status: %s\n", device_xname(sc->sc_dev),
			usbd_errstr(status)));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	DPRINTF(("%s: umct status = MSR:%02x, LSR:%02x\n",
		 device_xname(sc->sc_dev), tbuf[0],tbuf[1]));

	sc->sc_lsr = sc->sc_msr = 0;
	mstatus = tbuf[0];
	if (ISSET(mstatus, MSR_DSR))
		sc->sc_msr |= UMSR_DSR;
	if (ISSET(mstatus, MSR_DCD))
		sc->sc_msr |= UMSR_DCD;
	ucom_status_change(device_private(sc->sc_subdev));
}

void
umct_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct umct_softc *sc = addr;

	DPRINTF(("umct_get_status:\n"));

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}
