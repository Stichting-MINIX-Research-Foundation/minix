/*	$NetBSD: uftdi.c,v 1.60 2015/02/20 14:50:53 nonaka Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uftdi.c,v 1.60 2015/02/20 14:50:53 nonaka Exp $");

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

#include <dev/usb/uftdireg.h>

#ifdef UFTDI_DEBUG
#define DPRINTF(x)	if (uftdidebug) printf x
#define DPRINTFN(n,x)	if (uftdidebug>(n)) printf x
int uftdidebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UFTDI_CONFIG_INDEX	0
#define UFTDI_IFACE_INDEX	0
#define UFTDI_MAX_PORTS		4

/*
 * These are the default number of bytes transferred per frame if the
 * endpoint doesn't tell us.  The output buffer size is a hard limit
 * for devices that use a 6-bit size encoding.
 */
#define UFTDIIBUFSIZE 64
#define UFTDIOBUFSIZE 64

/*
 * Magic constants!  Where do these come from?  They're what Linux uses...
 */
#define	UFTDI_MAX_IBUFSIZE	512
#define	UFTDI_MAX_OBUFSIZE	256

struct uftdi_softc {
	device_t		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* device */
	usbd_interface_handle	sc_iface[UFTDI_MAX_PORTS];	/* interface */

	enum uftdi_type		sc_type;
	u_int			sc_hdrlen;
	u_int			sc_numports;
	u_int			sc_chiptype;

	u_char			sc_msr;
	u_char			sc_lsr;

	device_t		sc_subdev[UFTDI_MAX_PORTS];

	u_char			sc_dying;

	u_int			last_lcr;

};

Static void	uftdi_get_status(void *, int portno, u_char *lsr, u_char *msr);
Static void	uftdi_set(void *, int, int, int);
Static int	uftdi_param(void *, int, struct termios *);
Static int	uftdi_open(void *sc, int portno);
Static void	uftdi_read(void *sc, int portno, u_char **ptr,u_int32_t *count);
Static void	uftdi_write(void *sc, int portno, u_char *to, u_char *from,
			    u_int32_t *count);
Static void	uftdi_break(void *sc, int portno, int onoff);

struct ucom_methods uftdi_methods = {
	uftdi_get_status,
	uftdi_set,
	uftdi_param,
	NULL,
	uftdi_open,
	NULL,
	uftdi_read,
	uftdi_write,
};

/* 
 * The devices default to UFTDI_TYPE_8U232AM.
 * Remember to update uftdi_attach() if it should be UFTDI_TYPE_SIO instead
 */
static const struct usb_devno uftdi_devs[] = {
	{ USB_VENDOR_BBELECTRONICS, USB_PRODUCT_BBELECTRONICS_USOTL4 },
	{ USB_VENDOR_FALCOM, USB_PRODUCT_FALCOM_TWIST },
	{ USB_VENDOR_FALCOM, USB_PRODUCT_FALCOM_SAMBA },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_230X },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_232H },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_232RL },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_2232C },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_4232H },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_8U100AX },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_8U232AM },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_KW },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_YS },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_Y6 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_Y8 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_IC },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_DB9 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_RS232 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MHAM_Y9 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_COASTAL_TNCX },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CTI_485_MINI },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_CTI_NANO_485 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SEMC_DSS20 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_LK202_24_USB },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_LK204_24_USB },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_MX200_USB },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_MX4_MX5_USB },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_CFA_631 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_CFA_632 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_CFA_633 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_CFA_634 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_LCD_CFA_635 },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_OPENRD_JTAGKEY },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_BEAGLEBONE },
	{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_MAXSTREAM_PKG_U },
	{ USB_VENDOR_xxFTDI, USB_PRODUCT_xxFTDI_SHEEVAPLUG_JTAG },
	{ USB_VENDOR_INTREPIDCS, USB_PRODUCT_INTREPIDCS_VALUECAN },
	{ USB_VENDOR_INTREPIDCS, USB_PRODUCT_INTREPIDCS_NEOVI },
	{ USB_VENDOR_MELCO, USB_PRODUCT_MELCO_PCOPRS1 },
	{ USB_VENDOR_RATOC, USB_PRODUCT_RATOC_REXUSB60F },
	{ USB_VENDOR_SEALEVEL, USB_PRODUCT_SEALEVEL_USBSERIAL },
	{ USB_VENDOR_SEALEVEL, USB_PRODUCT_SEALEVEL_SEAPORT4P1 },
	{ USB_VENDOR_SEALEVEL, USB_PRODUCT_SEALEVEL_SEAPORT4P2 },
	{ USB_VENDOR_SEALEVEL, USB_PRODUCT_SEALEVEL_SEAPORT4P3 },
	{ USB_VENDOR_SEALEVEL, USB_PRODUCT_SEALEVEL_SEAPORT4P4 },
	{ USB_VENDOR_SIIG2, USB_PRODUCT_SIIG2_US2308 },
	{ USB_VENDOR_MISC, USB_PRODUCT_MISC_TELLSTICK },
	{ USB_VENDOR_MISC, USB_PRODUCT_MISC_TELLSTICK_DUO },
};
#define uftdi_lookup(v, p) usb_lookup(uftdi_devs, v, p)

int uftdi_match(device_t, cfdata_t, void *);
void uftdi_attach(device_t, device_t, void *);
void uftdi_childdet(device_t, device_t);
int uftdi_detach(device_t, int);
int uftdi_activate(device_t, enum devact);
extern struct cfdriver uftdi_cd;
CFATTACH_DECL2_NEW(uftdi, sizeof(struct uftdi_softc), uftdi_match,
    uftdi_attach, uftdi_detach, uftdi_activate, NULL, uftdi_childdet);

int 
uftdi_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	DPRINTFN(20,("uftdi: vendor=0x%x, product=0x%x\n",
		     uaa->vendor, uaa->product));

        return (uftdi_lookup(uaa->vendor, uaa->product) != NULL ?
                UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void 
uftdi_attach(device_t parent, device_t self, void *aux)
{
	struct uftdi_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_device_descriptor_t *ddesc;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	const char *devname = device_xname(self);
	int i,idx;
	usbd_status err;
	struct ucom_attach_args uca;

	DPRINTFN(10,("\nuftdi_attach: sc=%p\n", sc));

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UFTDI_CONFIG_INDEX, 1);
	if (err) {
		aprint_error("\n%s: failed to set configuration, err=%s\n",
		       devname, usbd_errstr(err));
		goto bad;
	}

	sc->sc_dev = self;
	sc->sc_udev = dev;
	sc->sc_numports = 1;
	sc->sc_type = UFTDI_TYPE_8U232AM; /* most devices are post-8U232AM */
	sc->sc_hdrlen = 0;
	if (uaa->vendor == USB_VENDOR_FTDI
	    && uaa->product == USB_PRODUCT_FTDI_SERIAL_8U100AX) {
		sc->sc_type = UFTDI_TYPE_SIO;
		sc->sc_hdrlen = 1;
	}

	ddesc = usbd_get_device_descriptor(dev);
	sc->sc_chiptype = UGETW(ddesc->bcdDevice);
	switch (sc->sc_chiptype) {
	case 0x500: /* 2232D */
	case 0x700: /* 2232H */
		sc->sc_numports = 2;
		break;
	case 0x800: /* 4232H */
		sc->sc_numports = 4;
		break;
	case 0x200: /* 232/245AM */
	case 0x400: /* 232/245BL */
	case 0x600: /* 232/245R */
	default:
		break;
	}

	for (idx = UFTDI_IFACE_INDEX; idx < sc->sc_numports; idx++) {
		err = usbd_device2interface_handle(dev, idx, &iface);
		if (err) {
			aprint_error(
			    "\n%s: failed to get interface idx=%d, err=%s\n",
			    devname, idx, usbd_errstr(err));
			goto bad;
		}

		id = usbd_get_interface_descriptor(iface);

		sc->sc_iface[idx] = iface;

		uca.bulkin = uca.bulkout = -1;
		uca.ibufsize = uca.obufsize = 0;
		for (i = 0; i < id->bNumEndpoints; i++) {
			int addr, dir, attr;
			ed = usbd_interface2endpoint_descriptor(iface, i);
			if (ed == NULL) {
				aprint_error_dev(self,
				    "could not read endpoint descriptor: %s\n",
				    usbd_errstr(err));
				goto bad;
			}

			addr = ed->bEndpointAddress;
			dir = UE_GET_DIR(ed->bEndpointAddress);
			attr = ed->bmAttributes & UE_XFERTYPE;
			if (dir == UE_DIR_IN && attr == UE_BULK) {
				uca.bulkin = addr;
				uca.ibufsize = UGETW(ed->wMaxPacketSize);
				if (uca.ibufsize >= UFTDI_MAX_IBUFSIZE)
					uca.ibufsize = UFTDI_MAX_IBUFSIZE;
			} else if (dir == UE_DIR_OUT && attr == UE_BULK) {
				uca.bulkout = addr;
				uca.obufsize = UGETW(ed->wMaxPacketSize)
				    - sc->sc_hdrlen;
				if (uca.obufsize >= UFTDI_MAX_OBUFSIZE)
					uca.obufsize = UFTDI_MAX_OBUFSIZE;
				/* Limit length if we have a 6-bit header.  */
				if ((sc->sc_hdrlen > 0) &&
				    (uca.obufsize > UFTDIOBUFSIZE))
					uca.obufsize = UFTDIOBUFSIZE;
			} else {
				aprint_error_dev(self,
				    "unexpected endpoint\n");
				goto bad;
			}
		}
		if (uca.bulkin == -1) {
			aprint_error_dev(self,
			    "Could not find data bulk in\n");
			goto bad;
		}
		if (uca.bulkout == -1) {
			aprint_error_dev(self,
			    "Could not find data bulk out\n");
			goto bad;
		}

		uca.portno = FTDI_PIT_SIOA + idx;
		/* bulkin, bulkout set above */
		if (uca.ibufsize == 0)
			uca.ibufsize = UFTDIIBUFSIZE;
		uca.ibufsizepad = uca.ibufsize;
		if (uca.obufsize == 0)
			uca.obufsize = UFTDIOBUFSIZE - sc->sc_hdrlen;
		uca.opkthdrlen = sc->sc_hdrlen;
		uca.device = dev;
		uca.iface = iface;
		uca.methods = &uftdi_methods;
		uca.arg = sc;
		uca.info = NULL;

		DPRINTF(("uftdi: in=0x%x out=0x%x isize=0x%x osize=0x%x\n",
			uca.bulkin, uca.bulkout,
			uca.ibufsize, uca.obufsize));
		sc->sc_subdev[idx] = config_found_sm_loc(self, "ucombus", NULL,
		    &uca, ucomprint, ucomsubmatch);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	return;

bad:
	DPRINTF(("uftdi_attach: ATTACH ERROR\n"));
	sc->sc_dying = 1;
	return;
}

int
uftdi_activate(device_t self, enum devact act)
{
	struct uftdi_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

void
uftdi_childdet(device_t self, device_t child)
{
	int i;
	struct uftdi_softc *sc = device_private(self);

	for (i = 0; i < sc->sc_numports; i++) {
		if (sc->sc_subdev[i] == child)
			break;
	}
	KASSERT(i < sc->sc_numports);
	sc->sc_subdev[i] = NULL;
}

int
uftdi_detach(device_t self, int flags)
{
	struct uftdi_softc *sc = device_private(self);
	int i;

	DPRINTF(("uftdi_detach: sc=%p flags=%d\n", sc, flags));
	sc->sc_dying = 1;
	for (i=0; i < sc->sc_numports; i++) {
		if (sc->sc_subdev[i] != NULL)
			config_detach(sc->sc_subdev[i], flags);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (0);
}

Static int
uftdi_open(void *vsc, int portno)
{
	struct uftdi_softc *sc = vsc;
	usb_device_request_t req;
	usbd_status err;
	struct termios t;

	DPRINTF(("uftdi_open: sc=%p\n", sc));

	if (sc->sc_dying)
		return (EIO);

	/* Perform a full reset on the device */
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_RESET;
	USETW(req.wValue, FTDI_SIO_RESET_SIO);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	/* Set 9600 baud, 2 stop bits, no parity, 8 bits */
	t.c_ospeed = 9600;
	t.c_cflag = CSTOPB | CS8;
	(void)uftdi_param(sc, portno, &t);

	/* Turn on RTS/CTS flow control */
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_FLOW_CTRL;
	USETW(req.wValue, 0);
	USETW2(req.wIndex, FTDI_SIO_RTS_CTS_HS, portno);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	return (0);
}

Static void
uftdi_read(void *vsc, int portno, u_char **ptr, u_int32_t *count)
{
	struct uftdi_softc *sc = vsc;
	u_char msr, lsr;

	DPRINTFN(15,("uftdi_read: sc=%p, port=%d count=%d\n", sc, portno,
		     *count));

	msr = FTDI_GET_MSR(*ptr);
	lsr = FTDI_GET_LSR(*ptr);

#ifdef UFTDI_DEBUG
	if (*count != 2)
		DPRINTFN(10,("uftdi_read: sc=%p, port=%d count=%d data[0]="
			    "0x%02x\n", sc, portno, *count, (*ptr)[2]));
#endif

	if (sc->sc_msr != msr ||
	    (sc->sc_lsr & FTDI_LSR_MASK) != (lsr & FTDI_LSR_MASK)) {
		DPRINTF(("uftdi_read: status change msr=0x%02x(0x%02x) "
			 "lsr=0x%02x(0x%02x)\n", msr, sc->sc_msr,
			 lsr, sc->sc_lsr));
		sc->sc_msr = msr;
		sc->sc_lsr = lsr;
		ucom_status_change(device_private(sc->sc_subdev[portno-1]));
	}

	/* Adjust buffer pointer to skip status prefix */
	*ptr += 2;
}

Static void
uftdi_write(void *vsc, int portno, u_char *to, u_char *from, u_int32_t *count)
{
	struct uftdi_softc *sc = vsc;

	DPRINTFN(10,("uftdi_write: sc=%p, port=%d count=%u data[0]=0x%02x\n",
		     vsc, portno, *count, from[0]));

	/* Make length tag and copy data */
	if (sc->sc_hdrlen > 0)
		*to = FTDI_OUT_TAG(*count, portno);

	memcpy(to + sc->sc_hdrlen, from, *count);
	*count += sc->sc_hdrlen;
}

Static void
uftdi_set(void *vsc, int portno, int reg, int onoff)
{
	struct uftdi_softc *sc = vsc;
	usb_device_request_t req;
	int ctl;

	DPRINTF(("uftdi_set: sc=%p, port=%d reg=%d onoff=%d\n", vsc, portno,
		 reg, onoff));

	switch (reg) {
	case UCOM_SET_DTR:
		ctl = onoff ? FTDI_SIO_SET_DTR_HIGH : FTDI_SIO_SET_DTR_LOW;
		break;
	case UCOM_SET_RTS:
		ctl = onoff ? FTDI_SIO_SET_RTS_HIGH : FTDI_SIO_SET_RTS_LOW;
		break;
	case UCOM_SET_BREAK:
		uftdi_break(sc, portno, onoff);
		return;
	default:
		return;
	}
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_MODEM_CTRL;
	USETW(req.wValue, ctl);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	DPRINTFN(2,("uftdi_set: reqtype=0x%02x req=0x%02x value=0x%04x "
		    "index=0x%04x len=%d\n", req.bmRequestType, req.bRequest,
		    UGETW(req.wValue), UGETW(req.wIndex), UGETW(req.wLength)));
	(void)usbd_do_request(sc->sc_udev, &req, NULL);
}

Static int
uftdi_param(void *vsc, int portno, struct termios *t)
{
	struct uftdi_softc *sc = vsc;
	usb_device_request_t req;
	usbd_status err;
	int rate, data, flow;

	DPRINTF(("uftdi_param: sc=%p\n", sc));

	if (sc->sc_dying)
		return (EIO);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_BITMODE;
	USETW(req.wValue, FTDI_BITMODE_RESET << 8 | 0x00);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	switch (sc->sc_type) {
	case UFTDI_TYPE_SIO:
		switch (t->c_ospeed) {
		case 300: rate = ftdi_sio_b300; break;
		case 600: rate = ftdi_sio_b600; break;
		case 1200: rate = ftdi_sio_b1200; break;
		case 2400: rate = ftdi_sio_b2400; break;
		case 4800: rate = ftdi_sio_b4800; break;
		case 9600: rate = ftdi_sio_b9600; break;
		case 19200: rate = ftdi_sio_b19200; break;
		case 38400: rate = ftdi_sio_b38400; break;
		case 57600: rate = ftdi_sio_b57600; break;
		case 115200: rate = ftdi_sio_b115200; break;
		default:
			return (EINVAL);
		}
		break;

	case UFTDI_TYPE_8U232AM:
		switch(t->c_ospeed) {
		case 300: rate = ftdi_8u232am_b300; break;
		case 600: rate = ftdi_8u232am_b600; break;
		case 1200: rate = ftdi_8u232am_b1200; break;
		case 2400: rate = ftdi_8u232am_b2400; break;
		case 4800: rate = ftdi_8u232am_b4800; break;
		case 9600: rate = ftdi_8u232am_b9600; break;
		case 19200: rate = ftdi_8u232am_b19200; break;
		case 38400: rate = ftdi_8u232am_b38400; break;
		case 57600: rate = ftdi_8u232am_b57600; break;
		case 115200: rate = ftdi_8u232am_b115200; break;
		case 230400: rate = ftdi_8u232am_b230400; break;
		case 460800: rate = ftdi_8u232am_b460800; break;
		case 921600: rate = ftdi_8u232am_b921600; break;
		default:
			return (EINVAL);
		}
		break;

	default:
		return (EINVAL);
	}
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_BAUD_RATE;
	USETW(req.wValue, rate);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	DPRINTFN(2,("uftdi_param: reqtype=0x%02x req=0x%02x value=0x%04x "
		    "index=0x%04x len=%d\n", req.bmRequestType, req.bRequest,
		    UGETW(req.wValue), UGETW(req.wIndex), UGETW(req.wLength)));
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	if (ISSET(t->c_cflag, CSTOPB))
		data = FTDI_SIO_SET_DATA_STOP_BITS_2;
	else
		data = FTDI_SIO_SET_DATA_STOP_BITS_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			data |= FTDI_SIO_SET_DATA_PARITY_ODD;
		else
			data |= FTDI_SIO_SET_DATA_PARITY_EVEN;
	} else
		data |= FTDI_SIO_SET_DATA_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		data |= FTDI_SIO_SET_DATA_BITS(5);
		break;
	case CS6:
		data |= FTDI_SIO_SET_DATA_BITS(6);
		break;
	case CS7:
		data |= FTDI_SIO_SET_DATA_BITS(7);
		break;
	case CS8:
		data |= FTDI_SIO_SET_DATA_BITS(8);
		break;
	}
	sc->last_lcr = data;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_DATA;
	USETW(req.wValue, data);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	DPRINTFN(2,("uftdi_param: reqtype=0x%02x req=0x%02x value=0x%04x "
		    "index=0x%04x len=%d\n", req.bmRequestType, req.bRequest,
		    UGETW(req.wValue), UGETW(req.wIndex), UGETW(req.wLength)));
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	if (ISSET(t->c_cflag, CRTSCTS)) {
		flow = FTDI_SIO_RTS_CTS_HS;
		USETW(req.wValue, 0);
	} else if (ISSET(t->c_iflag, IXON) && ISSET(t->c_iflag, IXOFF)) {
		flow = FTDI_SIO_XON_XOFF_HS;
		USETW2(req.wValue, t->c_cc[VSTOP], t->c_cc[VSTART]);
	} else {
		flow = FTDI_SIO_DISABLE_FLOW_CTRL;
		USETW(req.wValue, 0);
	}
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_FLOW_CTRL;
	USETW2(req.wIndex, flow, portno);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);

	return (0);
}

void
uftdi_get_status(void *vsc, int portno, u_char *lsr, u_char *msr)
{
	struct uftdi_softc *sc = vsc;

	DPRINTF(("uftdi_status: msr=0x%02x lsr=0x%02x\n",
		 sc->sc_msr, sc->sc_lsr));

	if (msr != NULL)
		*msr = sc->sc_msr;
	if (lsr != NULL)
		*lsr = sc->sc_lsr;
}

void
uftdi_break(void *vsc, int portno, int onoff)
{
	struct uftdi_softc *sc = vsc;
	usb_device_request_t req;
	int data;

	DPRINTF(("uftdi_break: sc=%p, port=%d onoff=%d\n", vsc, portno,
		  onoff));

	if (onoff) {
		data = sc->last_lcr | FTDI_SIO_SET_BREAK;
	} else {
		data = sc->last_lcr;
	}

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = FTDI_SIO_SET_DATA;
	USETW(req.wValue, data);
	USETW(req.wIndex, portno);
	USETW(req.wLength, 0);
	(void)usbd_do_request(sc->sc_udev, &req, NULL);
}
