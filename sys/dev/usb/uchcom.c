/*	$NetBSD: uchcom.c,v 1.13 2014/03/15 19:20:27 martin Exp $	*/

/*
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI (tshiozak@netbsd.org).
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
__KERNEL_RCSID(0, "$NetBSD: uchcom.c,v 1.13 2014/03/15 19:20:27 martin Exp $");

/*
 * driver for WinChipHead CH341/340, the worst USB-serial chip in the world.
 */

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
#include <sys/device.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

#ifdef UCHCOM_DEBUG
#define DPRINTFN(n, x)  if (uchcomdebug > (n)) printf x
int	uchcomdebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define	UCHCOM_IFACE_INDEX	0
#define	UCHCOM_CONFIG_INDEX	0

#define UCHCOM_REV_CH340	0x0250
#define UCHCOM_INPUT_BUF_SIZE	8

#define UCHCOM_REQ_GET_VERSION	0x5F
#define UCHCOM_REQ_READ_REG	0x95
#define UCHCOM_REQ_WRITE_REG	0x9A
#define UCHCOM_REQ_RESET	0xA1
#define UCHCOM_REQ_SET_DTRRTS	0xA4

#define UCHCOM_REG_STAT1	0x06
#define UCHCOM_REG_STAT2	0x07
#define UCHCOM_REG_BPS_PRE	0x12
#define UCHCOM_REG_BPS_DIV	0x13
#define UCHCOM_REG_BPS_MOD	0x14
#define UCHCOM_REG_BPS_PAD	0x0F
#define UCHCOM_REG_BREAK1	0x05
#define UCHCOM_REG_BREAK2	0x18
#define UCHCOM_REG_LCR1		0x18
#define UCHCOM_REG_LCR2		0x25

#define UCHCOM_VER_20		0x20

#define UCHCOM_BASE_UNKNOWN	0
#define UCHCOM_BPS_MOD_BASE	20000000
#define UCHCOM_BPS_MOD_BASE_OFS	1100

#define UCHCOM_DTR_MASK		0x20
#define UCHCOM_RTS_MASK		0x40

#define UCHCOM_BRK1_MASK	0x01
#define UCHCOM_BRK2_MASK	0x40

#define UCHCOM_LCR1_MASK	0xAF
#define UCHCOM_LCR2_MASK	0x07
#define UCHCOM_LCR1_PARENB	0x80
#define UCHCOM_LCR2_PAREVEN	0x07
#define UCHCOM_LCR2_PARODD	0x06
#define UCHCOM_LCR2_PARMARK	0x05
#define UCHCOM_LCR2_PARSPACE	0x04

#define UCHCOM_INTR_STAT1	0x02
#define UCHCOM_INTR_STAT2	0x03
#define UCHCOM_INTR_LEAST	4

#define UCHCOMIBUFSIZE 256
#define UCHCOMOBUFSIZE 256

struct uchcom_softc
{
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	device_t		sc_subdev;
	usbd_interface_handle	sc_iface;
	int			sc_dying;
	/* */
	int			sc_intr_endpoint;
	int			sc_intr_size;
	usbd_pipe_handle	sc_intr_pipe;
	u_char			*sc_intr_buf;
	/* */
	uint8_t			sc_version;
	int			sc_dtr;
	int			sc_rts;
	u_char			sc_lsr;
	u_char			sc_msr;
	int			sc_lcr1;
	int			sc_lcr2;
};

struct uchcom_endpoints
{
	int		ep_bulkin;
	int		ep_bulkout;
	int		ep_intr;
	int		ep_intr_size;
};

struct uchcom_divider
{
	uint8_t		dv_prescaler;
	uint8_t		dv_div;
	uint8_t		dv_mod;
};

struct uchcom_divider_record
{
	uint32_t		dvr_high;
	uint32_t		dvr_low;
	uint32_t		dvr_base_clock;
	struct uchcom_divider	dvr_divider;
};

static const struct uchcom_divider_record dividers[] =
{
	{  307200, 307200, UCHCOM_BASE_UNKNOWN, { 7, 0xD9, 0 } },
	{  921600, 921600, UCHCOM_BASE_UNKNOWN, { 7, 0xF3, 0 } },
	{ 2999999,  23530,             6000000, { 3,    0, 0 } },
	{   23529,   2942,              750000, { 2,    0, 0 } },
	{    2941,    368,               93750, { 1,    0, 0 } },
	{     367,      1,               11719, { 0,    0, 0 } },
};
#define NUM_DIVIDERS	(sizeof (dividers) / sizeof (dividers[0]))

static const struct usb_devno uchcom_devs[] = {
	{ USB_VENDOR_WINCHIPHEAD, USB_PRODUCT_WINCHIPHEAD_CH341SER },
	{ USB_VENDOR_WINCHIPHEAD2, USB_PRODUCT_WINCHIPHEAD2_CH341 },
};
#define uchcom_lookup(v, p)	usb_lookup(uchcom_devs, v, p)

Static void	uchcom_get_status(void *, int, u_char *, u_char *);
Static void	uchcom_set(void *, int, int, int);
Static int	uchcom_param(void *, int, struct termios *);
Static int	uchcom_open(void *, int);
Static void	uchcom_close(void *, int);
Static void	uchcom_intr(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);

static int	set_config(struct uchcom_softc *);
static int	find_ifaces(struct uchcom_softc *, usbd_interface_handle *);
static int	find_endpoints(struct uchcom_softc *,
			       struct uchcom_endpoints *);
static void	close_intr_pipe(struct uchcom_softc *);


struct	ucom_methods uchcom_methods = {
	.ucom_get_status	= uchcom_get_status,
	.ucom_set		= uchcom_set,
	.ucom_param		= uchcom_param,
	.ucom_ioctl		= NULL,
	.ucom_open		= uchcom_open,
	.ucom_close		= uchcom_close,
	.ucom_read		= NULL,
	.ucom_write		= NULL,
};

int uchcom_match(device_t, cfdata_t, void *);
void uchcom_attach(device_t, device_t, void *);
void uchcom_childdet(device_t, device_t);
int uchcom_detach(device_t, int);
int uchcom_activate(device_t, enum devact);

extern struct cfdriver uchcom_cd;

CFATTACH_DECL2_NEW(uchcom,
    sizeof(struct uchcom_softc),
    uchcom_match,
    uchcom_attach,
    uchcom_detach,
    uchcom_activate,
    NULL,
    uchcom_childdet);

/* ----------------------------------------------------------------------
 * driver entry points
 */

int 
uchcom_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (uchcom_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void 
uchcom_attach(device_t parent, device_t self, void *aux)
{
	struct uchcom_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	char *devinfop;
	struct uchcom_endpoints endpoints;
	struct ucom_attach_args uca;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dev = self;
        sc->sc_udev = dev;
	sc->sc_dying = 0;
	sc->sc_dtr = sc->sc_rts = -1;
	sc->sc_lsr = sc->sc_msr = 0;

	DPRINTF(("\n\nuchcom attach: sc=%p\n", sc));

	if (set_config(sc))
		goto failed;

	switch (uaa->release) {
	case UCHCOM_REV_CH340:
		aprint_normal_dev(self, "CH340 detected\n");
		break;
	default:
		aprint_normal_dev(self, "CH341 detected\n");
		break;
	}

	if (find_ifaces(sc, &sc->sc_iface))
		goto failed;

	if (find_endpoints(sc, &endpoints))
		goto failed;

	sc->sc_intr_endpoint = endpoints.ep_intr;
	sc->sc_intr_size = endpoints.ep_intr_size;

	/* setup ucom layer */
	uca.portno = UCOM_UNK_PORTNO;
	uca.bulkin = endpoints.ep_bulkin;
	uca.bulkout = endpoints.ep_bulkout;
	uca.ibufsize = UCHCOMIBUFSIZE;
	uca.obufsize = UCHCOMOBUFSIZE;
	uca.ibufsizepad = UCHCOMIBUFSIZE;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = sc->sc_iface;
	uca.methods = &uchcom_methods;
	uca.arg = sc;
	uca.info = NULL;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   sc->sc_dev);

	sc->sc_subdev = config_found_sm_loc(self, "ucombus", NULL, &uca,
					    ucomprint, ucomsubmatch);

	return;

failed:
	sc->sc_dying = 1;
	return;
}

void
uchcom_childdet(device_t self, device_t child)
{
	struct uchcom_softc *sc = device_private(self);

	KASSERT(sc->sc_subdev == child);
	sc->sc_subdev = NULL;
}

int 
uchcom_detach(device_t self, int flags)
{
	struct uchcom_softc *sc = device_private(self);
	int rv = 0;

	DPRINTF(("uchcom_detach: sc=%p flags=%d\n", sc, flags));

	close_intr_pipe(sc);

	sc->sc_dying = 1;

	if (sc->sc_subdev != NULL)
		rv = config_detach(sc->sc_subdev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return rv;
}

int
uchcom_activate(device_t self, enum devact act)
{
	struct uchcom_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		close_intr_pipe(sc);
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static int
set_config(struct uchcom_softc *sc)
{
	usbd_status err;

	err = usbd_set_config_index(sc->sc_udev, UCHCOM_CONFIG_INDEX, 1);
	if (err) {
		aprint_error_dev(sc->sc_dev,
		    "failed to set configuration: %s\n", usbd_errstr(err));
		return -1;
	}

	return 0;
}

static int
find_ifaces(struct uchcom_softc *sc, usbd_interface_handle *riface)
{
	usbd_status err;

	err = usbd_device2interface_handle(sc->sc_udev, UCHCOM_IFACE_INDEX,
					   riface);
	if (err) {
		aprint_error("\n%s: failed to get interface: %s\n",
			device_xname(sc->sc_dev), usbd_errstr(err));
		return -1;
	}

	return 0;
}

static int
find_endpoints(struct uchcom_softc *sc, struct uchcom_endpoints *endpoints)
{
	int i, bin=-1, bout=-1, intr=-1, isize=0;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;

	id = usbd_get_interface_descriptor(sc->sc_iface);

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "no endpoint descriptor for %d\n", i);
			return -1;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			intr = ed->bEndpointAddress;
			isize = UGETW(ed->wMaxPacketSize);
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			bin = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			bout = ed->bEndpointAddress;
		}
	}

	if (intr == -1 || bin == -1 || bout == -1) {
		if (intr == -1) {
			aprint_error_dev(sc->sc_dev,
			    "no interrupt end point\n");
		}
		if (bin == -1) {
			aprint_error_dev(sc->sc_dev,
			    "no data bulk in end point\n");
		}
		if (bout == -1) {
			aprint_error_dev(sc->sc_dev,
			    "no data bulk out end point\n");
		}
		return -1;
	}
	if (isize < UCHCOM_INTR_LEAST) {
		aprint_error_dev(sc->sc_dev, "intr pipe is too short\n");
		return -1;
	}

	DPRINTF(("%s: bulkin=%d, bulkout=%d, intr=%d, isize=%d\n",
		 device_xname(sc->sc_dev), bin, bout, intr, isize));

	endpoints->ep_intr = intr;
	endpoints->ep_intr_size = isize;
	endpoints->ep_bulkin = bin;
	endpoints->ep_bulkout = bout;

	return 0;
}


/* ----------------------------------------------------------------------
 * low level i/o
 */

static __inline usbd_status
generic_control_out(struct uchcom_softc *sc, uint8_t reqno,
		    uint16_t value, uint16_t index)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = reqno;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, 0);

	return usbd_do_request(sc->sc_udev, &req, 0);
}

static __inline usbd_status
generic_control_in(struct uchcom_softc *sc, uint8_t reqno,
		   uint16_t value, uint16_t index, void *buf, int buflen,
		   int *actlen)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = reqno;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, (uint16_t)buflen);

	return usbd_do_request_flags(sc->sc_udev, &req, buf,
				     USBD_SHORT_XFER_OK, actlen,
				     USBD_DEFAULT_TIMEOUT);
}

static __inline usbd_status
write_reg(struct uchcom_softc *sc,
	  uint8_t reg1, uint8_t val1, uint8_t reg2, uint8_t val2)
{
	DPRINTF(("uchcom: write reg 0x%02X<-0x%02X, 0x%02X<-0x%02X\n",
		 (unsigned)reg1, (unsigned)val1,
		 (unsigned)reg2, (unsigned)val2));
	return generic_control_out(
		sc, UCHCOM_REQ_WRITE_REG,
		reg1|((uint16_t)reg2<<8), val1|((uint16_t)val2<<8));
}

static __inline usbd_status
read_reg(struct uchcom_softc *sc,
	 uint8_t reg1, uint8_t *rval1, uint8_t reg2, uint8_t *rval2)
{
	uint8_t buf[UCHCOM_INPUT_BUF_SIZE];
	usbd_status err;
	int actin;

	err = generic_control_in(
		sc, UCHCOM_REQ_READ_REG,
		reg1|((uint16_t)reg2<<8), 0, buf, sizeof buf, &actin);
	if (err)
		return err;

	DPRINTF(("uchcom: read reg 0x%02X->0x%02X, 0x%02X->0x%02X\n",
		 (unsigned)reg1, (unsigned)buf[0],
		 (unsigned)reg2, (unsigned)buf[1]));

	if (rval1) *rval1 = buf[0];
	if (rval2) *rval2 = buf[1];

	return USBD_NORMAL_COMPLETION;
}

static __inline usbd_status
get_version(struct uchcom_softc *sc, uint8_t *rver)
{
	uint8_t buf[UCHCOM_INPUT_BUF_SIZE];
	usbd_status err;
	int actin;

	err = generic_control_in(
		sc, UCHCOM_REQ_GET_VERSION, 0, 0, buf, sizeof buf, &actin);
	if (err)
		return err;

	if (rver) *rver = buf[0];

	return USBD_NORMAL_COMPLETION;
}

static __inline usbd_status
get_status(struct uchcom_softc *sc, uint8_t *rval)
{
	return read_reg(sc, UCHCOM_REG_STAT1, rval, UCHCOM_REG_STAT2, NULL);
}

static __inline usbd_status
set_dtrrts_10(struct uchcom_softc *sc, uint8_t val)
{
	return write_reg(sc, UCHCOM_REG_STAT1, val, UCHCOM_REG_STAT1, val);
}

static __inline usbd_status
set_dtrrts_20(struct uchcom_softc *sc, uint8_t val)
{
	return generic_control_out(sc, UCHCOM_REQ_SET_DTRRTS, val, 0);
}


/* ----------------------------------------------------------------------
 * middle layer
 */

static int
update_version(struct uchcom_softc *sc)
{
	usbd_status err;

	err = get_version(sc, &sc->sc_version);
	if (err) {
		aprint_error_dev(sc->sc_dev, "cannot get version: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	return 0;
}

static void
convert_status(struct uchcom_softc *sc, uint8_t cur)
{
	sc->sc_dtr = !(cur & UCHCOM_DTR_MASK);
	sc->sc_rts = !(cur & UCHCOM_RTS_MASK);

	cur = ~cur & 0x0F;
	sc->sc_msr = (cur << 4) | ((sc->sc_msr >> 4) ^ cur);
}

static int
update_status(struct uchcom_softc *sc)
{
	usbd_status err;
	uint8_t cur;

	err = get_status(sc, &cur);
	if (err) {
		aprint_error_dev(sc->sc_dev,
		    "cannot update status: %s\n", usbd_errstr(err));
		return EIO;
	}
	convert_status(sc, cur);

	return 0;
}


static int
set_dtrrts(struct uchcom_softc *sc, int dtr, int rts)
{
	usbd_status err;
	uint8_t val = 0;

	if (dtr) val |= UCHCOM_DTR_MASK;
	if (rts) val |= UCHCOM_RTS_MASK;

	if (sc->sc_version < UCHCOM_VER_20)
		err = set_dtrrts_10(sc, ~val);
	else
		err = set_dtrrts_20(sc, ~val);

	if (err) {
		aprint_error_dev(sc->sc_dev, "cannot set DTR/RTS: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	return 0;
}

static int
set_break(struct uchcom_softc *sc, int onoff)
{
	usbd_status err;
	uint8_t brk1, brk2;

	err = read_reg(sc, UCHCOM_REG_BREAK1, &brk1, UCHCOM_REG_BREAK2, &brk2);
	if (err)
		return EIO;
	if (onoff) {
		/* on - clear bits */
		brk1 &= ~UCHCOM_BRK1_MASK;
		brk2 &= ~UCHCOM_BRK2_MASK;
	} else {
		/* off - set bits */
		brk1 |= UCHCOM_BRK1_MASK;
		brk2 |= UCHCOM_BRK2_MASK;
	}
	err = write_reg(sc, UCHCOM_REG_BREAK1, brk1, UCHCOM_REG_BREAK2, brk2);
	if (err)
		return EIO;

	return 0;
}

static int
calc_divider_settings(struct uchcom_divider *dp, uint32_t rate)
{
	int i;
	const struct uchcom_divider_record *rp;
	uint32_t div, rem, mod;

	/* find record */
	for (i=0; i<NUM_DIVIDERS; i++) {
		if (dividers[i].dvr_high >= rate &&
		    dividers[i].dvr_low <= rate) {
			rp = &dividers[i];
			goto found;
		}
	}
	return -1;

found:
	dp->dv_prescaler = rp->dvr_divider.dv_prescaler;
	if (rp->dvr_base_clock == UCHCOM_BASE_UNKNOWN)
		dp->dv_div = rp->dvr_divider.dv_div;
	else {
		div = rp->dvr_base_clock / rate;
		rem = rp->dvr_base_clock % rate;
		if (div==0 || div>=0xFF)
			return -1;
		if ((rem<<1) >= rate)
			div += 1;
		dp->dv_div = (uint8_t)-div;
	}

	mod = UCHCOM_BPS_MOD_BASE/rate + UCHCOM_BPS_MOD_BASE_OFS;
	mod = mod + mod/2;

	dp->dv_mod = mod / 0x100;

	return 0;
}

static int
set_dte_rate(struct uchcom_softc *sc, uint32_t rate)
{
	usbd_status err;
	struct uchcom_divider dv;

	if (calc_divider_settings(&dv, rate))
		return EINVAL;

	if ((err = write_reg(sc,
			     UCHCOM_REG_BPS_PRE, dv.dv_prescaler,
			     UCHCOM_REG_BPS_DIV, dv.dv_div)) ||
	    (err = write_reg(sc,
			     UCHCOM_REG_BPS_MOD, dv.dv_mod,
			     UCHCOM_REG_BPS_PAD, 0))) {
		aprint_error_dev(sc->sc_dev, "cannot set DTE rate: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	return 0;
}

static int
set_line_control(struct uchcom_softc *sc, tcflag_t cflag)
{
	usbd_status err;
	uint8_t lcr1val = 0, lcr2val = 0;

	err = read_reg(sc, UCHCOM_REG_LCR1, &lcr1val, UCHCOM_REG_LCR2, &lcr2val);
	if (err) {
		aprint_error_dev(sc->sc_dev, "cannot get LCR: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	lcr1val &= ~UCHCOM_LCR1_MASK;
	lcr2val &= ~UCHCOM_LCR2_MASK;

	/*
	 * XXX: it is difficult to handle the line control appropriately:
	 *   - CS8, !CSTOPB and any parity mode seems ok, but
	 *   - the chip doesn't have the function to calculate parity
	 *     in !CS8 mode.
	 *   - it is unclear that the chip supports CS5,6 mode.
	 *   - it is unclear how to handle stop bits.
	 */

	switch (ISSET(cflag, CSIZE)) {
	case CS5:
	case CS6:
	case CS7:
		return EINVAL;
	case CS8:
		break;
	}

	if (ISSET(cflag, PARENB)) {
		lcr1val |= UCHCOM_LCR1_PARENB;
		if (ISSET(cflag, PARODD))
			lcr2val |= UCHCOM_LCR2_PARODD;
		else
			lcr2val |= UCHCOM_LCR2_PAREVEN;
	}

	err = write_reg(sc, UCHCOM_REG_LCR1, lcr1val, UCHCOM_REG_LCR2, lcr2val);
	if (err) {
		aprint_error_dev(sc->sc_dev, "cannot set LCR: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	return 0;
}

static int
clear_chip(struct uchcom_softc *sc)
{
	usbd_status err;

	DPRINTF(("%s: clear\n", device_xname(sc->sc_dev)));
	err = generic_control_out(sc, UCHCOM_REQ_RESET, 0, 0);
	if (err) {
		aprint_error_dev(sc->sc_dev, "cannot clear: %s\n",
		    usbd_errstr(err));
		return EIO;
	}

	return 0;
}

static int
reset_chip(struct uchcom_softc *sc)
{
	usbd_status err;
	uint8_t lcr1val, lcr2val, pre, div, mod;
	uint16_t val=0, idx=0;

	err = read_reg(sc, UCHCOM_REG_LCR1, &lcr1val, UCHCOM_REG_LCR2, &lcr2val);
	if (err)
		goto failed;

	err = read_reg(sc, UCHCOM_REG_BPS_PRE, &pre, UCHCOM_REG_BPS_DIV, &div);
	if (err)
		goto failed;

	err = read_reg(sc, UCHCOM_REG_BPS_MOD, &mod, UCHCOM_REG_BPS_PAD, NULL);
	if (err)
		goto failed;

	val |= (uint16_t)(lcr1val&0xF0) << 8;
	val |= 0x01;
	val |= (uint16_t)(lcr2val&0x0F) << 8;
	val |= 0x02;
	idx |= pre & 0x07;
	val |= 0x04;
	idx |= (uint16_t)div << 8;
	val |= 0x08;
	idx |= mod & 0xF8;
	val |= 0x10;

	DPRINTF(("%s: reset v=0x%04X, i=0x%04X\n",
		 device_xname(sc->sc_dev), val, idx));

	err = generic_control_out(sc, UCHCOM_REQ_RESET, val, idx);
	if (err)
		goto failed;

	return 0;

failed:
	printf("%s: cannot reset: %s\n",
	       device_xname(sc->sc_dev), usbd_errstr(err));
	return EIO;
}

static int
setup_comm(struct uchcom_softc *sc)
{
	int ret;

	ret = update_version(sc);
	if (ret)
		return ret;

	ret = clear_chip(sc);
	if (ret)
		return ret;

	ret = set_dte_rate(sc, TTYDEF_SPEED);
	if (ret)
		return ret;

	ret = set_line_control(sc, CS8);
	if (ret)
		return ret;

	ret = update_status(sc);
	if (ret)
		return ret;

	ret = reset_chip(sc);
	if (ret)
		return ret;

	ret = set_dte_rate(sc, TTYDEF_SPEED); /* XXX */
	if (ret)
		return ret;

	sc->sc_dtr = sc->sc_rts = 1;
	ret = set_dtrrts(sc, sc->sc_dtr, sc->sc_rts);
	if (ret)
		return ret;

	return 0;
}

static int
setup_intr_pipe(struct uchcom_softc *sc)
{
	usbd_status err;

	if (sc->sc_intr_endpoint != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_intr_buf = malloc(sc->sc_intr_size, M_USBDEV, M_WAITOK);
		err = usbd_open_pipe_intr(sc->sc_iface,
					  sc->sc_intr_endpoint,
					  USBD_SHORT_XFER_OK,
					  &sc->sc_intr_pipe, sc,
					  sc->sc_intr_buf,
					  sc->sc_intr_size,
					  uchcom_intr, USBD_DEFAULT_INTERVAL);
		if (err) {
			aprint_error_dev(sc->sc_dev,
			    "cannot open interrupt pipe: %s\n",
			    usbd_errstr(err));
			return EIO;
		}
	}
	return 0;
}

static void
close_intr_pipe(struct uchcom_softc *sc)
{
	usbd_status err;

	if (sc->sc_dying)
		return;

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			aprint_error_dev(sc->sc_dev,
			    "abort interrupt pipe failed: %s\n",
			    usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			aprint_error_dev(sc->sc_dev,
			    "close interrupt pipe failed: %s\n",
			    usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
}


/* ----------------------------------------------------------------------
 * methods for ucom
 */
void
uchcom_get_status(void *arg, int portno, u_char *rlsr, u_char *rmsr)
{
	struct uchcom_softc *sc = arg;

	if (sc->sc_dying)
		return;

	*rlsr = sc->sc_lsr;
	*rmsr = sc->sc_msr;
}

void
uchcom_set(void *arg, int portno, int reg, int onoff)
{
	struct uchcom_softc *sc = arg;

	if (sc->sc_dying)
		return;

	switch (reg) {
	case UCOM_SET_DTR:
		sc->sc_dtr = !!onoff;
		set_dtrrts(sc, sc->sc_dtr, sc->sc_rts);
		break;
	case UCOM_SET_RTS:
		sc->sc_rts = !!onoff;
		set_dtrrts(sc, sc->sc_dtr, sc->sc_rts);
		break;
	case UCOM_SET_BREAK:
		set_break(sc, onoff);
		break;
	}
}

int
uchcom_param(void *arg, int portno, struct termios *t)
{
	struct uchcom_softc *sc = arg;
	int ret;

	if (sc->sc_dying)
		return 0;

	ret = set_line_control(sc, t->c_cflag);
	if (ret)
		return ret;

	ret = set_dte_rate(sc, t->c_ospeed);
	if (ret)
		return ret;

	return 0;
}

int
uchcom_open(void *arg, int portno)
{
	int ret;
	struct uchcom_softc *sc = arg;

	if (sc->sc_dying)
		return EIO;

	ret = setup_intr_pipe(sc);
	if (ret)
		return ret;

	ret = setup_comm(sc);
	if (ret)
		return ret;

	return 0;
}

void
uchcom_close(void *arg, int portno)
{
	struct uchcom_softc *sc = arg;

	if (sc->sc_dying)
		return;

	close_intr_pipe(sc);
}


/* ----------------------------------------------------------------------
 * callback when the modem status is changed.
 */
void
uchcom_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
	    usbd_status status)
{
	struct uchcom_softc *sc = priv;
	u_char *buf = sc->sc_intr_buf;

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		DPRINTF(("%s: abnormal status: %s\n",
			 device_xname(sc->sc_dev), usbd_errstr(status)));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}
	DPRINTF(("%s: intr: 0x%02X 0x%02X 0x%02X 0x%02X "
		 "0x%02X 0x%02X 0x%02X 0x%02X\n",
		 device_xname(sc->sc_dev),
		 (unsigned)buf[0], (unsigned)buf[1],
		 (unsigned)buf[2], (unsigned)buf[3],
		 (unsigned)buf[4], (unsigned)buf[5],
		 (unsigned)buf[6], (unsigned)buf[7]));

	convert_status(sc, buf[UCHCOM_INTR_STAT1]);
	ucom_status_change(device_private(sc->sc_subdev));
}
