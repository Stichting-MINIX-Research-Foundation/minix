/*	$NetBSD: moscom.c,v 1.8 2012/09/23 01:08:17 chs Exp $	*/
/*	$OpenBSD: moscom.c,v 1.11 2007/10/11 18:33:14 deraadt Exp $	*/

/*
 * Copyright (c) 2006 Jonathan Gray <jsg@openbsd.org>
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
__KERNEL_RCSID(0, "$NetBSD: moscom.c,v 1.8 2012/09/23 01:08:17 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/device.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/ucomvar.h>

#define MOSCOMBUFSZ		256
#define MOSCOM_CONFIG_NO	0
#define MOSCOM_IFACE_NO		0

#define MOSCOM_READ		0x0d
#define MOSCOM_WRITE		0x0e
#define MOSCOM_UART_REG		0x0300
#define MOSCOM_VEND_REG		0x0000

#define MOSCOM_TXBUF		0x00	/* Write */
#define MOSCOM_RXBUF		0x00	/* Read */
#define MOSCOM_INT		0x01
#define MOSCOM_FIFO		0x02	/* Write */
#define MOSCOM_ISR		0x02	/* Read */
#define MOSCOM_LCR		0x03
#define MOSCOM_MCR		0x04
#define MOSCOM_LSR		0x05
#define MOSCOM_MSR		0x06
#define MOSCOM_SCRATCH		0x07
#define MOSCOM_DIV_LO		0x08
#define MOSCOM_DIV_HI		0x09
#define MOSCOM_EFR		0x0a
#define	MOSCOM_XON1		0x0b
#define MOSCOM_XON2		0x0c
#define MOSCOM_XOFF1		0x0d
#define MOSCOM_XOFF2		0x0e

#define MOSCOM_BAUDLO		0x00
#define MOSCOM_BAUDHI		0x01

#define MOSCOM_INT_RXEN		0x01
#define MOSCOM_INT_TXEN		0x02
#define MOSCOM_INT_RSEN		0x04	
#define MOSCOM_INT_MDMEM	0x08
#define MOSCOM_INT_SLEEP	0x10
#define MOSCOM_INT_XOFF		0x20
#define MOSCOM_INT_RTS		0x40	

#define MOSCOM_FIFO_EN		0x01
#define MOSCOM_FIFO_RXCLR	0x02
#define MOSCOM_FIFO_TXCLR	0x04
#define MOSCOM_FIFO_DMA_BLK	0x08
#define MOSCOM_FIFO_TXLVL_MASK	0x30
#define MOSCOM_FIFO_TXLVL_8	0x00
#define MOSCOM_FIFO_TXLVL_16	0x10
#define MOSCOM_FIFO_TXLVL_32	0x20
#define MOSCOM_FIFO_TXLVL_56	0x30
#define MOSCOM_FIFO_RXLVL_MASK	0xc0
#define MOSCOM_FIFO_RXLVL_8	0x00
#define MOSCOM_FIFO_RXLVL_16	0x40
#define MOSCOM_FIFO_RXLVL_56	0x80
#define MOSCOM_FIFO_RXLVL_80	0xc0

#define MOSCOM_ISR_MDM		0x00
#define MOSCOM_ISR_NONE		0x01
#define MOSCOM_ISR_TX		0x02
#define MOSCOM_ISR_RX		0x04
#define MOSCOM_ISR_LINE		0x06
#define MOSCOM_ISR_RXTIMEOUT	0x0c
#define MOSCOM_ISR_RX_XOFF	0x10
#define MOSCOM_ISR_RTSCTS	0x20
#define MOSCOM_ISR_FIFOEN	0xc0

#define MOSCOM_LCR_DBITS(x)	(x - 5)
#define MOSCOM_LCR_STOP_BITS_1	0x00
#define MOSCOM_LCR_STOP_BITS_2	0x04	/* 2 if 6-8 bits/char or 1.5 if 5 */
#define MOSCOM_LCR_PARITY_NONE	0x00
#define MOSCOM_LCR_PARITY_ODD	0x08
#define MOSCOM_LCR_PARITY_EVEN	0x18
#define MOSCOM_LCR_BREAK	0x40
#define MOSCOM_LCR_DIVLATCH_EN	0x80

#define MOSCOM_MCR_DTR		0x01
#define MOSCOM_MCR_RTS		0x02
#define MOSCOM_MCR_LOOP		0x04
#define MOSCOM_MCR_INTEN	0x08
#define MOSCOM_MCR_LOOPBACK	0x10
#define MOSCOM_MCR_XONANY	0x20
#define MOSCOM_MCR_IRDA_EN	0x40
#define MOSCOM_MCR_BAUD_DIV4	0x80

#define MOSCOM_LSR_RXDATA	0x01
#define MOSCOM_LSR_RXOVER	0x02
#define MOSCOM_LSR_RXPAR_ERR	0x04
#define MOSCOM_LSR_RXFRM_ERR	0x08
#define MOSCOM_LSR_RXBREAK	0x10
#define MOSCOM_LSR_TXEMPTY	0x20
#define MOSCOM_LSR_TXALLEMPTY	0x40
#define MOSCOM_LSR_TXFIFO_ERR	0x80

#define MOSCOM_MSR_CTS_CHG	0x01
#define MOSCOM_MSR_DSR_CHG	0x02
#define MOSCOM_MSR_RI_CHG	0x04
#define MOSCOM_MSR_CD_CHG	0x08
#define MOSCOM_MSR_CTS		0x10
#define MOSCOM_MSR_RTS		0x20
#define MOSCOM_MSR_RI		0x40
#define MOSCOM_MSR_CD		0x80

#define MOSCOM_BAUD_REF		115200

struct moscom_softc {
	device_t		 sc_dev;
	usbd_device_handle	 sc_udev;
	usbd_interface_handle	 sc_iface;
	device_t		 sc_subdev;

	u_char			 sc_msr;
	u_char			 sc_lsr;
	u_char			 sc_lcr;

	u_char			 sc_dying;
};

void	moscom_get_status(void *, int, u_char *, u_char *);
void	moscom_set(void *, int, int, int);
int	moscom_param(void *, int, struct termios *);
int	moscom_open(void *, int);
int	moscom_cmd(struct moscom_softc *, int, int);	

struct ucom_methods moscom_methods = {
	NULL,
	moscom_set,
	moscom_param,
	NULL,
	moscom_open,
	NULL,
	NULL,
	NULL,
};

static const struct usb_devno moscom_devs[] = {
	{ USB_VENDOR_MOSCHIP,		USB_PRODUCT_MOSCHIP_MCS7703 },
	{ USB_VENDOR_MOSCHIP,		USB_PRODUCT_MOSCHIP_MCS7840 },
	{ USB_VENDOR_ATEN,		USB_PRODUCT_ATEN_UC2324 }
};
#define moscom_lookup(v, p) usb_lookup(moscom_devs, v, p)

int moscom_match(device_t, cfdata_t, void *); 
void moscom_attach(device_t, device_t, void *); 
void moscom_childdet(device_t, device_t);
int moscom_detach(device_t, int); 
int moscom_activate(device_t, enum devact); 

CFATTACH_DECL2_NEW(moscom, sizeof(struct moscom_softc), moscom_match,
    moscom_attach, moscom_detach, moscom_activate, NULL, moscom_childdet);

int 
moscom_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (moscom_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void 
moscom_attach(device_t parent, device_t self, void *aux)
{
	struct moscom_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	struct ucom_attach_args uca;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	usbd_status error;
	int i;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dev = self;

	bzero(&uca, sizeof(uca));
	sc->sc_udev = uaa->device;

	if (usbd_set_config_index(sc->sc_udev, MOSCOM_CONFIG_NO, 1) != 0) {
		aprint_error_dev(self, "could not set configuration no\n");
		sc->sc_dying = 1;
		return;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, MOSCOM_IFACE_NO,
	    &sc->sc_iface);
	if (error != 0) {
		aprint_error_dev(self, "could not get interface handle\n");
		sc->sc_dying = 1;
		return;
	}

	id = usbd_get_interface_descriptor(sc->sc_iface);

	uca.bulkin = uca.bulkout = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "no endpoint descriptor found for %d\n", i);
			sc->sc_dying = 1;
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			uca.bulkin = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			uca.bulkout = ed->bEndpointAddress;
	}

	if (uca.bulkin == -1 || uca.bulkout == -1) {
		aprint_error_dev(self, "missing endpoint\n");
		sc->sc_dying = 1;
		return;
	}

	uca.ibufsize = MOSCOMBUFSZ;
	uca.obufsize = MOSCOMBUFSZ;
	uca.ibufsizepad = MOSCOMBUFSZ;
	uca.opkthdrlen = 0;
	uca.device = sc->sc_udev;
	uca.iface = sc->sc_iface;
	uca.methods = &moscom_methods;
	uca.arg = sc;
	uca.info = NULL;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    sc->sc_dev);
	
	sc->sc_subdev = config_found_sm_loc(self, "ucombus", NULL, &uca,
					    ucomprint, ucomsubmatch);

	return;
}

void
moscom_childdet(device_t self, device_t child)
{
	struct moscom_softc *sc = device_private(self);

	KASSERT(sc->sc_subdev == child);
	sc->sc_subdev = NULL;
}

int 
moscom_detach(device_t self, int flags)
{
	struct moscom_softc *sc = device_private(self);
	int rv = 0;

	sc->sc_dying = 1;
	if (sc->sc_subdev != NULL)
		rv = config_detach(sc->sc_subdev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return (rv);
}

int
moscom_activate(device_t self, enum devact act)
{
	struct moscom_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
moscom_open(void *vsc, int portno)
{
	struct moscom_softc *sc = vsc;
	usb_device_request_t req;

	if (sc->sc_dying)
		return (EIO);

	/* Purge FIFOs or odd things happen */
	if (moscom_cmd(sc, MOSCOM_FIFO, 0x00) != 0)
		return (EIO);
	
	if (moscom_cmd(sc, MOSCOM_FIFO, MOSCOM_FIFO_EN |
	    MOSCOM_FIFO_RXCLR | MOSCOM_FIFO_TXCLR |
	    MOSCOM_FIFO_DMA_BLK | MOSCOM_FIFO_RXLVL_MASK) != 0) 
		return (EIO);

	/* Magic tell device we're ready for data command */
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MOSCOM_WRITE;
	USETW(req.wValue, 0x08);
	USETW(req.wIndex, MOSCOM_INT);
	USETW(req.wLength, 0);
	if (usbd_do_request(sc->sc_udev, &req, NULL) != 0)
		return (EIO);

	return (0);
}

void
moscom_set(void *vsc, int portno, int reg, int onoff)
{
	struct moscom_softc *sc = vsc;
	int val;

	switch (reg) {
	case UCOM_SET_DTR:
		val = onoff ? MOSCOM_MCR_DTR : 0;
		break;
	case UCOM_SET_RTS:
		val = onoff ? MOSCOM_MCR_RTS : 0;
		break;
	case UCOM_SET_BREAK:
		val = sc->sc_lcr;
		if (onoff)
			val |= MOSCOM_LCR_BREAK;
		moscom_cmd(sc, MOSCOM_LCR, val);
		return;
	default:
		return;
	}

	moscom_cmd(sc, MOSCOM_MCR, val);
}

int
moscom_param(void *vsc, int portno, struct termios *t)
{
	struct moscom_softc *sc = (struct moscom_softc *)vsc;
	int data;

	if (t->c_ospeed <= 0 || t->c_ospeed > 115200)
		return (EINVAL);

	data = MOSCOM_BAUD_REF / t->c_ospeed;

	if (data == 0 || data > 0xffff)
		return (EINVAL);

	moscom_cmd(sc, MOSCOM_LCR, MOSCOM_LCR_DIVLATCH_EN);
	moscom_cmd(sc, MOSCOM_BAUDLO, data & 0xFF);
	moscom_cmd(sc, MOSCOM_BAUDHI, (data >> 8) & 0xFF);

	if (ISSET(t->c_cflag, CSTOPB))
		data = MOSCOM_LCR_STOP_BITS_2;
	else
		data = MOSCOM_LCR_STOP_BITS_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			data |= MOSCOM_LCR_PARITY_ODD;
		else
			data |= MOSCOM_LCR_PARITY_EVEN;
	} else
		data |= MOSCOM_LCR_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		data |= MOSCOM_LCR_DBITS(5);
		break;
	case CS6:
		data |= MOSCOM_LCR_DBITS(6);
		break;
	case CS7:
		data |= MOSCOM_LCR_DBITS(7);
		break;
	case CS8:
		data |= MOSCOM_LCR_DBITS(8);
		break;
	}

	sc->sc_lcr = data;
	moscom_cmd(sc, MOSCOM_LCR, sc->sc_lcr);

#if 0
	/* XXX flow control */
	if (ISSET(t->c_cflag, CRTSCTS))
		/*  rts/cts flow ctl */
	} else if (ISSET(t->c_iflag, IXON|IXOFF)) {
		/*  xon/xoff flow ctl */
	} else {
		/* disable flow ctl */
	}
#endif

	return (0);
}

void
moscom_get_status(void *vsc, int portno, u_char *lsr, u_char *msr)
{
	struct moscom_softc *sc = vsc;
	
	if (msr != NULL)
		*msr = sc->sc_msr;
	if (lsr != NULL)
		*lsr = sc->sc_lsr;
}

int
moscom_cmd(struct moscom_softc *sc, int reg, int val)
{
	usb_device_request_t req;
	usbd_status err;
	
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MOSCOM_WRITE;
	USETW(req.wValue, val + MOSCOM_UART_REG);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		return (EIO);
	else
		return (0);
}
