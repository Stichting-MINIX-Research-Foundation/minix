/* $NetBSD: umcs.c,v 1.8 2014/08/23 21:37:56 martin Exp $ */
/* $FreeBSD: head/sys/dev/usb/serial/umcs.c 260559 2014-01-12 11:44:28Z hselasky $ */

/*-
 * Copyright (c) 2010 Lev Serebryakov <lev@FreeBSD.org>.
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
 */

/*
 * This driver supports several multiport USB-to-RS232 serial adapters driven
 * by MosChip mos7820 and mos7840, bridge chips.
 * The adapters are sold under many different brand names.
 *
 * Datasheets are available at MosChip www site at
 * http://www.moschip.com.  The datasheets don't contain full
 * programming information for the chip.
 *
 * It is nornal to have only two enabled ports in devices, based on
 * quad-port mos7840.
 *
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umcs.c,v 1.8 2014/08/23 21:37:56 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/device.h>
#include <sys/kmem.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/ucomvar.h>

#include "umcs.h"

#if 0
#define	DPRINTF(ARG)	printf ARG
#else
#define	DPRINTF(ARG)
#endif

/*
 * Two-port devices (both with 7820 chip and 7840 chip configured as two-port)
 * have ports 0 and 2, with ports 1 and 3 omitted.
 * So, PHYSICAL port numbers on two-port device will be 0 and 2.
 *
 * We use an array of the following struct, indexed by ucom port index,
 * and include the physical port number in it.
 */
struct umcs7840_softc_oneport {
	device_t sc_port_ucom;		/* ucom subdevice */
	unsigned int sc_port_phys;	/* physical port number */
	uint8_t	sc_port_lcr;		/* local line control register */
	uint8_t	sc_port_mcr;		/* local modem control register */
};

struct umcs7840_softc {
	device_t sc_dev;		/* ourself */
	usbd_interface_handle sc_iface; /* the usb interface */
	usbd_device_handle sc_udev;	/* the usb device */
	usbd_pipe_handle sc_intr_pipe;	/* interrupt pipe */
	uint8_t *sc_intr_buf;		/* buffer for interrupt xfer */
	unsigned int sc_intr_buflen;	/* size of buffer */
	struct usb_task sc_change_task;	/* async status changes */
	volatile uint32_t sc_change_mask;	/* mask of port changes */
	struct umcs7840_softc_oneport sc_ports[UMCS7840_MAX_PORTS];
					/* data for each port */
	uint8_t	sc_numports;		/* number of ports (subunits) */
	bool sc_init_done;		/* special one time init in open */
	bool sc_dying;			/* we have been deactivated */
};

static int umcs7840_get_reg(struct umcs7840_softc *sc, uint8_t reg, uint8_t *data);
static int umcs7840_set_reg(struct umcs7840_softc *sc, uint8_t reg, uint8_t data);
static int umcs7840_get_UART_reg(struct umcs7840_softc *sc, uint8_t portno, uint8_t reg, uint8_t *data);
static int umcs7840_set_UART_reg(struct umcs7840_softc *sc, uint8_t portno, uint8_t reg, uint8_t data);
static int umcs7840_calc_baudrate(uint32_t rate, uint16_t *divisor, uint8_t *clk);
static void umcs7840_dtr(struct umcs7840_softc *sc, int portno, bool onoff);
static void umcs7840_rts(struct umcs7840_softc *sc, int portno, bool onoff);
static void umcs7840_break(struct umcs7840_softc *sc, int portno, bool onoff);

static int umcs7840_match(device_t, cfdata_t, void *);
static void umcs7840_attach(device_t, device_t, void *);
static int umcs7840_detach(device_t, int);
static void umcs7840_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status);
static void umcs7840_change_task(void *arg);
static int umcs7840_activate(device_t, enum devact); 
static void umcs7840_childdet(device_t, device_t);

static void umcs7840_get_status(void *, int, u_char *, u_char *);
static void umcs7840_set(void *, int, int, int);
static int umcs7840_param(void *, int, struct termios *);
static int umcs7840_port_open(void *sc, int portno);
static void umcs7840_port_close(void *sc, int portno);

struct ucom_methods umcs7840_methods = {
	.ucom_get_status = umcs7840_get_status,
	.ucom_set = umcs7840_set,
	.ucom_param = umcs7840_param,
	.ucom_open = umcs7840_port_open,
	.ucom_close = umcs7840_port_close,
};

static const struct usb_devno umcs7840_devs[] = {
	{ USB_VENDOR_MOSCHIP,		USB_PRODUCT_MOSCHIP_MCS7703 },
	{ USB_VENDOR_MOSCHIP,		USB_PRODUCT_MOSCHIP_MCS7810 },
	{ USB_VENDOR_MOSCHIP,		USB_PRODUCT_MOSCHIP_MCS7820 },
	{ USB_VENDOR_MOSCHIP,		USB_PRODUCT_MOSCHIP_MCS7840 },
	{ USB_VENDOR_ATEN,		USB_PRODUCT_ATEN_UC2324 }
};
#define umcs7840_lookup(v, p) usb_lookup(umcs7840_devs, v, p)

CFATTACH_DECL2_NEW(umcs, sizeof(struct umcs7840_softc), umcs7840_match,
    umcs7840_attach, umcs7840_detach, umcs7840_activate, NULL,
    umcs7840_childdet);

static inline int
umcs7840_reg_sp(int phyport)
{
	KASSERT(phyport >= 0 && phyport < 4);
	switch (phyport) {
	default:
	case 0:	return MCS7840_DEV_REG_SP1;
	case 1:	return MCS7840_DEV_REG_SP2;
	case 2:	return MCS7840_DEV_REG_SP3;
	case 3:	return MCS7840_DEV_REG_SP4;
	}
}

static inline int
umcs7840_reg_ctrl(int phyport)
{
	KASSERT(phyport >= 0 && phyport < 4);
	switch (phyport) {
	default:
	case 0:	return MCS7840_DEV_REG_CONTROL1;
	case 1:	return MCS7840_DEV_REG_CONTROL2;
	case 2:	return MCS7840_DEV_REG_CONTROL3;
	case 3:	return MCS7840_DEV_REG_CONTROL4;
	}
}

static int
umcs7840_match(device_t dev, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	return (umcs7840_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

static void
umcs7840_attach(device_t parent, device_t self, void * aux)
{
	struct umcs7840_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	struct ucom_attach_args uca;
	int error, i, intr_addr;
	uint8_t data;

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;

	if (usbd_set_config_index(sc->sc_udev, MCS7840_CONFIG_INDEX, 1) != 0) {
		aprint_error(": could not set configuration no\n");
		return;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, MCS7840_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		aprint_error(": could not get interface handle\n");
		return;
	}

	/*
	 * Get number of ports
	 * Documentation (full datasheet) says, that number of ports is
	 * set as MCS7840_DEV_MODE_SELECT24S bit in MODE R/Only
	 * register. But vendor driver uses these undocumented
	 * register & bit.
	 *
	 * Experiments show, that MODE register can have `0'
	 * (4 ports) bit on 2-port device, so use vendor driver's way.
	 *
	 * Also, see notes in header file for these constants.
	 */
	umcs7840_get_reg(sc, MCS7840_DEV_REG_GPIO, &data);
	if (data & MCS7840_DEV_GPIO_4PORTS) {
		sc->sc_numports = 4;
		/* physical port no are : 0, 1, 2, 3 */
	} else {
		if (uaa->product == USB_PRODUCT_MOSCHIP_MCS7810)
			sc->sc_numports = 1;
		else {
			sc->sc_numports = 2;
			/* physical port no are : 0 and 2 */
		}
	}
	devinfop = usbd_devinfo_alloc(dev, 0);
	aprint_normal(": %s\n", devinfop);
	usbd_devinfo_free(devinfop);
	aprint_verbose_dev(self, "found %d active ports\n", sc->sc_numports);

	if (!umcs7840_get_reg(sc, MCS7840_DEV_REG_MODE, &data)) {
		aprint_verbose_dev(self, "On-die confguration: RST: active %s, "
		    "HRD: %s, PLL: %s, POR: %s, Ports: %s, EEPROM write %s, "
		    "IrDA is %savailable\n",
		    (data & MCS7840_DEV_MODE_RESET) ? "low" : "high",
		    (data & MCS7840_DEV_MODE_SER_PRSNT) ? "yes" : "no",
		    (data & MCS7840_DEV_MODE_PLLBYPASS) ? "bypassed" : "avail",
		    (data & MCS7840_DEV_MODE_PORBYPASS) ? "bypassed" : "avail",
		    (data & MCS7840_DEV_MODE_SELECT24S) ? "2" : "4",
		    (data & MCS7840_DEV_MODE_EEPROMWR) ? "enabled" : "disabled",
		    (data & MCS7840_DEV_MODE_IRDA) ? "" : "not ");
	}

	/*
	 * Set up the interrupt pipe
	 */
	id = usbd_get_interface_descriptor(sc->sc_iface);
	intr_addr = -1;
	for (i = 0 ; i < id->bNumEndpoints ; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) continue;
		if (UE_GET_DIR(ed->bEndpointAddress) != UE_DIR_IN
		    || UE_GET_XFERTYPE(ed->bmAttributes) != UE_INTERRUPT)
			continue;
		sc->sc_intr_buflen = UGETW(ed->wMaxPacketSize);
		intr_addr = ed->bEndpointAddress;
		break;
	}
	if (intr_addr < 0) {
		aprint_error_dev(self, "interrupt pipe not found\n");
		return;
	}
	sc->sc_intr_buf = kmem_alloc(sc->sc_intr_buflen, KM_SLEEP);

	error = usbd_open_pipe_intr(sc->sc_iface, intr_addr,
		    USBD_SHORT_XFER_OK, &sc->sc_intr_pipe, sc, sc->sc_intr_buf,
		    sc->sc_intr_buflen, umcs7840_intr, 100);
	if (error) {
		aprint_error_dev(self, "cannot open interrupt pipe "
		    "(addr %d)\n", intr_addr);
		return;
	}

	usb_init_task(&sc->sc_change_task, umcs7840_change_task, sc,
	    USB_TASKQ_MPSAFE);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    sc->sc_dev);

	memset(&uca, 0, sizeof uca);
	uca.ibufsize = 256;
	uca.obufsize = 256;
	uca.ibufsizepad = 256;
	uca.opkthdrlen = 0;
	uca.device = sc->sc_udev;
	uca.iface = sc->sc_iface;
	uca.methods = &umcs7840_methods;
	uca.arg = sc;

	for (i = 0; i < sc->sc_numports; i++) {
		uca.bulkin = uca.bulkout = -1;

		/*
		 * On four port cards, endpoints are 0/1 for first,
		 * 2/3 for second, ...
		 * On two port cards, they are 0/1 for first, 4/5 for second.
		 * On single port, just 0/1 will be used.
		 */
		int phyport = i * (sc->sc_numports == 2 ? 2 : 1);

		ed = usbd_interface2endpoint_descriptor(sc->sc_iface,
			phyport*2);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "no bulk in endpoint found for %d\n", i);
			return;
		}
		uca.bulkin = ed->bEndpointAddress;

		ed = usbd_interface2endpoint_descriptor(sc->sc_iface,
			phyport*2 + 1);
		if (ed == NULL) {
			aprint_error_dev(self,
			    "no bulk out endpoint found for %d\n", i);
			return;
		}
		uca.bulkout = ed->bEndpointAddress;
		uca.portno = i;
		DPRINTF(("port %d physical port %d bulk-in %d bulk-out %d\n",
		    i, phyport, uca.bulkin, uca.bulkout));

		sc->sc_ports[i].sc_port_phys = phyport;
		sc->sc_ports[i].sc_port_ucom =
		    config_found_sm_loc(self, "ucombus", NULL, &uca,
					    ucomprint, ucomsubmatch);
	}
}

static int
umcs7840_get_reg(struct umcs7840_softc *sc, uint8_t reg, uint8_t *data)
{
	usb_device_request_t req;
	int err;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = MCS7840_RDREQ;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, UMCS7840_READ_LENGTH);

	err = usbd_do_request(sc->sc_udev, &req, data);
	if (err)
		aprint_normal_dev(sc->sc_dev,
		    "Reading register %d failed: %s\n", reg, usbd_errstr(err));
	return err;
}

static int
umcs7840_set_reg(struct umcs7840_softc *sc, uint8_t reg, uint8_t data)
{
	usb_device_request_t req;
	int err;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MCS7840_WRREQ;
	USETW(req.wValue, data);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, 0);
	if (err)
		aprint_normal_dev(sc->sc_dev, "Writing register %d failed: %s\n", reg, usbd_errstr(err));

	return err;
}

static int
umcs7840_get_UART_reg(struct umcs7840_softc *sc, uint8_t portno,
	uint8_t reg, uint8_t *data)
{
	usb_device_request_t req;
	uint16_t wVal;
	int err;

	/* portno is port number */
	wVal = ((uint16_t)(portno + 1)) << 8;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = MCS7840_RDREQ;
	USETW(req.wValue, wVal);
	USETW(req.wIndex, reg);
	USETW(req.wLength, UMCS7840_READ_LENGTH);

	err = usbd_do_request(sc->sc_udev, &req, data);
	if (err)
		aprint_normal_dev(sc->sc_dev, "Reading UART %d register %d failed: %s\n", portno, reg, usbd_errstr(err));
	return err;
}

static int
umcs7840_set_UART_reg(struct umcs7840_softc *sc, uint8_t portno, uint8_t reg, uint8_t data)
{
	usb_device_request_t req;
	int err;
	uint16_t wVal;

	/* portno is the physical port number */
	wVal = ((uint16_t)(portno + 1)) << 8 | data;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MCS7840_WRREQ;
	USETW(req.wValue, wVal);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->sc_udev, &req, NULL);
	if (err)
		aprint_error_dev(sc->sc_dev,
		    "Writing UART %d register %d failed: %s\n",
		    portno, reg, usbd_errstr(err));
	return err;
}

static int
umcs7840_set_baudrate(struct umcs7840_softc *sc, uint8_t portno,
	uint32_t rate)
{
	int err;
	uint16_t divisor;
	uint8_t clk;
	uint8_t data;
	uint8_t physport = sc->sc_ports[portno].sc_port_phys;
	int spreg = umcs7840_reg_sp(physport);

	if (umcs7840_calc_baudrate(rate, &divisor, &clk)) {
		DPRINTF(("Port %d bad speed: %d\n", portno, rate));
		return (-1);
	}
	if (divisor == 0 || (clk & MCS7840_DEV_SPx_CLOCK_MASK) != clk) {
		DPRINTF(("Port %d bad speed calculation: %d\n", portno,
		    rate));
		return (-1);
	}
	DPRINTF(("Port %d set speed: %d (%02x / %d)\n", portno, rate, clk, divisor));

	/* Set clock source for standard BAUD frequences */
	err = umcs7840_get_reg(sc, spreg, &data);
	if (err)
		return err;
	data &= MCS7840_DEV_SPx_CLOCK_MASK;
	data |= clk;
	err = umcs7840_set_reg(sc, spreg, data);
	if (err)
		return err;

	/* Set divider */
	sc->sc_ports[portno].sc_port_lcr |= MCS7840_UART_LCR_DIVISORS;
	err = umcs7840_set_UART_reg(sc, physport, MCS7840_UART_REG_LCR, sc->sc_ports[portno].sc_port_lcr);
	if (err)
		return err;

	err = umcs7840_set_UART_reg(sc, physport, MCS7840_UART_REG_DLL, (uint8_t)(divisor & 0xff));
	if (err)
		return err;
	err = umcs7840_set_UART_reg(sc, physport, MCS7840_UART_REG_DLM, (uint8_t)((divisor >> 8) & 0xff));
	if (err)
		return err;

	/* Turn off access to DLL/DLM registers of UART */
	sc->sc_ports[portno].sc_port_lcr &= ~MCS7840_UART_LCR_DIVISORS;
	err = umcs7840_set_UART_reg(sc, physport, MCS7840_UART_REG_LCR, sc->sc_ports[portno].sc_port_lcr);
	if (err)
		return err;
	return (0);
}

static int
umcs7840_calc_baudrate(uint32_t rate, uint16_t *divisor, uint8_t *clk)
{
	/* Maximum speeds for standard frequences, when PLL is not used */
	static const uint32_t umcs7840_baudrate_divisors[] =
	    {0, 115200, 230400, 403200, 460800, 806400, 921600,
	     1572864, 3145728,};
	static const uint8_t umcs7840_baudrate_divisors_len =
	     __arraycount(umcs7840_baudrate_divisors);
	uint8_t i = 0;

	if (rate > umcs7840_baudrate_divisors[umcs7840_baudrate_divisors_len - 1])
		return (-1);

	for (i = 0; i < umcs7840_baudrate_divisors_len - 1
	     && !(rate > umcs7840_baudrate_divisors[i]
	     && rate <= umcs7840_baudrate_divisors[i + 1]); ++i);
	*divisor = umcs7840_baudrate_divisors[i + 1] / rate;
	/* 0x00 .. 0x70 */
	*clk = i << MCS7840_DEV_SPx_CLOCK_SHIFT;
	return (0);
}

static int 
umcs7840_detach(device_t self, int flags)
{
	struct umcs7840_softc *sc = device_private(self);
	int rv = 0, i;

	sc->sc_dying = true;

	/* close interrupt pipe */
	if (sc->sc_intr_pipe != NULL) {
		rv = usbd_abort_pipe(sc->sc_intr_pipe);
		if (rv)
			aprint_error_dev(sc->sc_dev,
			    "abort interrupt pipe failed: %s\n",
			    usbd_errstr(rv));
		rv = usbd_close_pipe(sc->sc_intr_pipe);
		if (rv)
			aprint_error_dev(sc->sc_dev,
			    "failed to close interrupt pipe: %s\n",
			    usbd_errstr(rv));
		kmem_free(sc->sc_intr_buf, sc->sc_intr_buflen);
		sc->sc_intr_pipe = NULL;
	}
	usb_rem_task(sc->sc_udev, &sc->sc_change_task);

	/* detach children */
	for (i = 0; i < sc->sc_numports; i++) {
		if (sc->sc_ports[i].sc_port_ucom) {
			rv = config_detach(sc->sc_ports[i].sc_port_ucom,
			    flags);
			if (rv)
				break;
		}
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   sc->sc_dev);

	return rv;
}

int
umcs7840_activate(device_t self, enum devact act)
{
	struct umcs7840_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = true;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static void
umcs7840_childdet(device_t self, device_t child)
{
	struct umcs7840_softc *sc = device_private(self);
	int i;

	for (i = 0; i < sc->sc_numports; i++) {
		if (child == sc->sc_ports[i].sc_port_ucom) {
			sc->sc_ports[i].sc_port_ucom = NULL;
			return;
		}
	}
}

static void
umcs7840_get_status(void *self, int portno, u_char *lsr, u_char *msr)
{
	struct umcs7840_softc *sc = self;
	uint8_t pn = sc->sc_ports[portno].sc_port_phys;
	uint8_t	hw_lsr = 0;	/* local line status register */
	uint8_t	hw_msr = 0;	/* local modem status register */

	if (sc->sc_dying)
		return;

	/* Read LSR & MSR */
	umcs7840_get_UART_reg(sc, pn, MCS7840_UART_REG_LSR, &hw_lsr);
	umcs7840_get_UART_reg(sc, pn, MCS7840_UART_REG_MSR, &hw_msr);

	*lsr = hw_lsr;
	*msr = hw_msr;
}

static void
umcs7840_set(void *self, int portno, int reg, int onoff)
{
	struct umcs7840_softc *sc = self;

	if (sc->sc_dying)
		return;

	switch (reg) {
	case UCOM_SET_DTR:
		umcs7840_dtr(sc, portno, onoff);
		break;
	case UCOM_SET_RTS:
		umcs7840_rts(sc, portno, onoff);
		break;
	case UCOM_SET_BREAK:
		umcs7840_break(sc, portno, onoff);
		break;
	default:
		break;
	}
}

static int
umcs7840_param(void *self, int portno, struct termios *t)
{
	struct umcs7840_softc *sc = self;
	int pn = sc->sc_ports[portno].sc_port_phys;
	uint8_t lcr = sc->sc_ports[portno].sc_port_lcr;
	uint8_t mcr = sc->sc_ports[portno].sc_port_mcr;

	if (t->c_cflag & CSTOPB) {
		lcr |= MCS7840_UART_LCR_STOPB2;
	} else {
		lcr |= MCS7840_UART_LCR_STOPB1;
	}

	lcr &= ~MCS7840_UART_LCR_PARITYMASK;
	if (t->c_cflag & PARENB) {
		lcr |= MCS7840_UART_LCR_PARITYON;
		if (t->c_cflag & PARODD) {
			lcr = MCS7840_UART_LCR_PARITYODD;
		} else {
			lcr = MCS7840_UART_LCR_PARITYEVEN;
		}
	} else {
		lcr &= ~MCS7840_UART_LCR_PARITYON;
	}

	lcr &= ~MCS7840_UART_LCR_DATALENMASK;
	switch (t->c_cflag & CSIZE) {
	case CS5:
		lcr |= MCS7840_UART_LCR_DATALEN5;
		break;
	case CS6:
		lcr |= MCS7840_UART_LCR_DATALEN6;
		break;
	case CS7:
		lcr |= MCS7840_UART_LCR_DATALEN7;
		break;
	case CS8:
		lcr |= MCS7840_UART_LCR_DATALEN8;
		break;
	}

	if (t->c_cflag & CRTSCTS)
		mcr |= MCS7840_UART_MCR_CTSRTS;
	else
		mcr &= ~MCS7840_UART_MCR_CTSRTS;

	if (t->c_cflag & CLOCAL)
		mcr &= ~MCS7840_UART_MCR_DTRDSR;
	else
		mcr |= MCS7840_UART_MCR_DTRDSR;

	sc->sc_ports[portno].sc_port_lcr = lcr;
	umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_LCR,
	    sc->sc_ports[pn].sc_port_lcr);

	sc->sc_ports[portno].sc_port_mcr = mcr;
	umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_MCR,
	    sc->sc_ports[pn].sc_port_mcr);

	if (umcs7840_set_baudrate(sc, portno, t->c_ospeed))
		return EIO;

	return 0;
}

static void
umcs7840_dtr(struct umcs7840_softc *sc, int portno, bool onoff)
{
	int pn = sc->sc_ports[portno].sc_port_phys;
	uint8_t mcr = sc->sc_ports[portno].sc_port_mcr;

	if (onoff)
		mcr |= MCS7840_UART_MCR_DTR;
	else
		mcr &= ~MCS7840_UART_MCR_DTR;

	sc->sc_ports[portno].sc_port_mcr = mcr;
	umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_MCR,
	    sc->sc_ports[pn].sc_port_mcr);
}

static void
umcs7840_rts(struct umcs7840_softc *sc, int portno, bool onoff)
{
	int pn = sc->sc_ports[portno].sc_port_phys;
	uint8_t mcr = sc->sc_ports[portno].sc_port_mcr;

	if (onoff)
		mcr |= MCS7840_UART_MCR_RTS;
	else
		mcr &= ~MCS7840_UART_MCR_RTS;

	sc->sc_ports[portno].sc_port_mcr = mcr;
	umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_MCR,
	    sc->sc_ports[pn].sc_port_mcr);
}

static void
umcs7840_break(struct umcs7840_softc *sc, int portno, bool onoff)
{
	int pn = sc->sc_ports[portno].sc_port_phys;
	uint8_t lcr = sc->sc_ports[portno].sc_port_lcr;

	if (onoff)
		lcr |= MCS7840_UART_LCR_BREAK;
	else
		lcr &= ~MCS7840_UART_LCR_BREAK;

	sc->sc_ports[portno].sc_port_lcr = lcr;
	umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_LCR,
	    sc->sc_ports[pn].sc_port_lcr);
}

static int
umcs7840_port_open(void *self, int portno)
{
	struct umcs7840_softc *sc = self;
	int pn = sc->sc_ports[portno].sc_port_phys;
	int spreg = umcs7840_reg_sp(pn);
	int ctrlreg = umcs7840_reg_ctrl(pn);
	uint8_t data;

	if (sc->sc_dying)
		return EIO;

	/* If it very first open, finish global configuration */
	if (!sc->sc_init_done) {
		if (umcs7840_get_reg(sc, MCS7840_DEV_REG_CONTROL1, &data))
			return EIO;
		data |= MCS7840_DEV_CONTROL1_DRIVER_DONE;
		if (umcs7840_set_reg(sc, MCS7840_DEV_REG_CONTROL1, data))
			return EIO;
		sc->sc_init_done = 1;
	}

	/* Toggle reset bit on-off */
	if (umcs7840_get_reg(sc, spreg, &data))
		return EIO;
	data |= MCS7840_DEV_SPx_UART_RESET;
	if (umcs7840_set_reg(sc, spreg, data))
		return EIO;
	data &= ~MCS7840_DEV_SPx_UART_RESET;
	if (umcs7840_set_reg(sc, spreg, data))
		return EIO;

	/* Set RS-232 mode */
	if (umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_SCRATCHPAD,
	    MCS7840_UART_SCRATCHPAD_RS232))
		return EIO;

	/* Disable RX on time of initialization */
	if (umcs7840_get_reg(sc, ctrlreg, &data))
		return EIO;
	data |= MCS7840_DEV_CONTROLx_RX_DISABLE;
	if (umcs7840_set_reg(sc, ctrlreg, data))
		return EIO;

	/* Disable all interrupts */
	if (umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_IER, 0))
		return EIO;

	/* Reset FIFO -- documented */
	if (umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_FCR, 0))
		return EIO;
	if (umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_FCR,
	    MCS7840_UART_FCR_ENABLE | MCS7840_UART_FCR_FLUSHRHR |
	    MCS7840_UART_FCR_FLUSHTHR | MCS7840_UART_FCR_RTL_1_14))
		return EIO;

	/* Set 8 bit, no parity, 1 stop bit -- documented */
	sc->sc_ports[pn].sc_port_lcr =
	    MCS7840_UART_LCR_DATALEN8 | MCS7840_UART_LCR_STOPB1;
	if (umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_LCR,
	    sc->sc_ports[pn].sc_port_lcr))
		return EIO;

	/*
	 * Enable DTR/RTS on modem control, enable modem interrupts --
	 * documented
	 */
	sc->sc_ports[pn].sc_port_mcr = MCS7840_UART_MCR_DTR
	    | MCS7840_UART_MCR_RTS | MCS7840_UART_MCR_IE;
	if (umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_MCR,
	    sc->sc_ports[pn].sc_port_mcr))
		return EIO;

	/* Clearing Bulkin and Bulkout FIFO */
	if (umcs7840_get_reg(sc, spreg, &data))
		return EIO;
	data |= MCS7840_DEV_SPx_RESET_OUT_FIFO | MCS7840_DEV_SPx_RESET_IN_FIFO;
	if (umcs7840_set_reg(sc, spreg, data))
		return EIO;
	data &= ~(MCS7840_DEV_SPx_RESET_OUT_FIFO
	    | MCS7840_DEV_SPx_RESET_IN_FIFO);
	if (umcs7840_set_reg(sc, spreg, data))
		return EIO;

	/* Set speed 9600 */
	if (umcs7840_set_baudrate(sc, portno, 9600))
		return EIO;


	/* Finally enable all interrupts -- documented */
	/*
	 * Copied from vendor driver, I don't know why we should read LCR
	 * here
	 */
	if (umcs7840_get_UART_reg(sc, pn, MCS7840_UART_REG_LCR,
	    &sc->sc_ports[pn].sc_port_lcr))
		return EIO;
	if (umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_IER,
	    MCS7840_UART_IER_RXSTAT | MCS7840_UART_IER_MODEM))
		return EIO;

	/* Enable RX */
	if (umcs7840_get_reg(sc, ctrlreg, &data))
		return EIO;
	data &= ~MCS7840_DEV_CONTROLx_RX_DISABLE;
	if (umcs7840_set_reg(sc, ctrlreg, data))
		return EIO;
	return 0;
}

static void
umcs7840_port_close(void *self, int portno)
{
	struct umcs7840_softc *sc = self;
	int pn = sc->sc_ports[portno].sc_port_phys;
	int ctrlreg = umcs7840_reg_ctrl(pn);
	uint8_t data;

	if (sc->sc_dying)
		return;

	umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_MCR, 0);
	umcs7840_set_UART_reg(sc, pn, MCS7840_UART_REG_IER, 0);

	/* Disable RX */
	if (umcs7840_get_reg(sc, ctrlreg, &data))
		return;
	data |= MCS7840_DEV_CONTROLx_RX_DISABLE;
	if (umcs7840_set_reg(sc, ctrlreg, data))
		return;
}

static void
umcs7840_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct umcs7840_softc *sc = priv;
	u_char *buf = sc->sc_intr_buf;
	int actlen;
	int subunit;

	if (status == USBD_NOT_STARTED || status == USBD_CANCELLED
	    || status == USBD_IOERROR)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev,
		    "umcs7840_intr: abnormal status: %s\n",
		    usbd_errstr(status));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &actlen, NULL);
	if (actlen == 5 || actlen == 13) {
		uint32_t change_mask = 0;
		/* Check status of all ports */
		for (subunit = 0; subunit < sc->sc_numports; subunit++) {
			uint8_t pn = sc->sc_ports[subunit].sc_port_phys;
			if (buf[pn] & MCS7840_UART_ISR_NOPENDING)
				continue;
			DPRINTF(("Port %d has pending interrupt: %02x "
			    "(FIFO: %02x)\n", pn,
			    buf[pn] & MCS7840_UART_ISR_INTMASK,
			    buf[pn] & (~MCS7840_UART_ISR_INTMASK)));
			switch (buf[pn] & MCS7840_UART_ISR_INTMASK) {
			case MCS7840_UART_ISR_RXERR:
			case MCS7840_UART_ISR_RXHASDATA:
			case MCS7840_UART_ISR_RXTIMEOUT:
			case MCS7840_UART_ISR_MSCHANGE:
				change_mask |= (1U << subunit);
				break;
			default:
				/* Do nothing */
				break;
			}
		}

		if (change_mask != 0) {
			atomic_or_32(&sc->sc_change_mask, change_mask);
			usb_add_task(sc->sc_udev, &sc->sc_change_task,
			    USB_TASKQ_DRIVER);
		}
	} else {
		aprint_error_dev(sc->sc_dev,
		   "Invalid interrupt data length %d", actlen);
	}
}

static void
umcs7840_change_task(void *arg)
{
	struct umcs7840_softc *sc = arg;
	uint32_t change_mask;
	int i;

	change_mask = atomic_swap_32(&sc->sc_change_mask, 0);
	for (i = 0; i < sc->sc_numports; i++) {
		if (ISSET(change_mask, (1U << i)))
			ucom_status_change(device_private(
			    sc->sc_ports[i].sc_port_ucom));
	}
}
