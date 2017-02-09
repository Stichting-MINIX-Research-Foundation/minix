/* $NetBSD: emdtv.c,v 1.10 2015/04/02 06:23:04 skrll Exp $ */

/*-
 * Copyright (c) 2008, 2011 Jared D. McNeill <jmcneill@invisible.ca>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: emdtv.c,v 1.10 2015/04/02 06:23:04 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/lwp.h>
#include <sys/module.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/emdtvvar.h>
#include <dev/usb/emdtvreg.h>

static int	emdtv_match(device_t, cfdata_t, void *);
static void	emdtv_attach(device_t, device_t, void *);
static int	emdtv_detach(device_t, int);
static int	emdtv_rescan(device_t, const char *, const int *);
static void	emdtv_childdet(device_t, device_t);
static int	emdtv_activate(device_t, enum devact);

static bool	emdtv_read_eeprom(struct emdtv_softc *);
static void	emdtv_board_setup(struct emdtv_softc *);

static void	emdtv_default_board_init(struct emdtv_softc *);

CFATTACH_DECL2_NEW(emdtv, sizeof(struct emdtv_softc),
    emdtv_match, emdtv_attach, emdtv_detach, emdtv_activate,
    emdtv_rescan, emdtv_childdet);

static const struct usb_devno emdtv_devices[] = {
	{ USB_VENDOR_AMD,	USB_PRODUCT_AMD_TV_WONDER_600_USB },
	{ USB_VENDOR_PINNACLE,	USB_PRODUCT_PINNACLE_PCTV800E },
};

int emdtv_debug_regs = 0;

static int
emdtv_match(device_t parent, cfdata_t match, void *opaque)
{
	struct usb_attach_arg *uaa = opaque;

	return usb_lookup(emdtv_devices, uaa->vendor, uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

static void
emdtv_attach(device_t parent, device_t self, void *opaque)
{
	struct emdtv_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = opaque;
	usbd_device_handle dev = uaa->device;
	usbd_status status;
	char *devinfo;

	devinfo = usbd_devinfo_alloc(dev, 0);
	aprint_naive("\n");
	aprint_normal(": %s\n", devinfo);
	usbd_devinfo_free(devinfo);

	sc->sc_dev = self;
	sc->sc_udev = dev;

	sc->sc_vendor = uaa->vendor;
	sc->sc_product = uaa->product;

	emdtv_i2c_attach(sc);

	emdtv_read_eeprom(sc);

	sc->sc_board = emdtv_board_lookup(sc->sc_vendor, sc->sc_product);
	if (sc->sc_board == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unsupported board 0x%04x:0x%04x\n",
		    sc->sc_vendor, sc->sc_product);
		sc->sc_dying = true;
		return;
	}

	emdtv_write_1(sc, 0x02, 0xa0, 0x23);
	if (emdtv_read_1(sc, UR_GET_STATUS, 0x05) != 0) {
		(void)emdtv_read_1(sc, 0x02, 0xa0);
		if (emdtv_read_1(sc, 0x02, 0xa0) & 0x08)
			aprint_debug_dev(sc->sc_dev,
			    "board requires manual gpio configuration\n");
	}

	emdtv_board_setup(sc);

	emdtv_gpio_ctl(sc, EMDTV_GPIO_ANALOG_ON, false);
	emdtv_gpio_ctl(sc, EMDTV_GPIO_TS1_ON, false);
	usbd_delay_ms(sc->sc_udev, 100);
	emdtv_gpio_ctl(sc, EMDTV_GPIO_ANALOG_ON, true);
	emdtv_gpio_ctl(sc, EMDTV_GPIO_TUNER1_ON, true);
	usbd_delay_ms(sc->sc_udev, 100);

	status = usbd_set_config_no(sc->sc_udev, 1, 1);
        if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(status));
		return;
	}

	status = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "couldn't find iface handle\n");
		return;
	}

	status = usbd_set_interface(sc->sc_iface, 1);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "couldn't set interface\n");
		return;
	}

	emdtv_dtv_attach(sc);
	emdtv_ir_attach(sc);
}

static int
emdtv_detach(device_t self, int flags)
{
	struct emdtv_softc *sc = device_private(self);
	usbd_status status;

	sc->sc_dying = true;

	emdtv_ir_detach(sc, flags);
	emdtv_dtv_detach(sc, flags);

	if (sc->sc_iface != NULL) {
        	status = usbd_set_interface(sc->sc_iface, 0);
		if (status != USBD_NORMAL_COMPLETION)
			aprint_error_dev(sc->sc_dev,
			    "couldn't stop stream: %s\n", usbd_errstr(status));
	}

	emdtv_i2c_detach(sc, flags);

	return 0;
}

int
emdtv_activate(device_t self, enum devact act)
{
	struct emdtv_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = true;
		break;
	}

	return 0;
}

static int
emdtv_rescan(device_t self, const char *ifattr, const int *locs)
{
	struct emdtv_softc *sc = device_private(self);

	emdtv_dtv_rescan(sc, ifattr, locs);

	return 0;
}

static void
emdtv_childdet(device_t self, device_t child)
{
	struct emdtv_softc *sc = device_private(self);

	if (child == sc->sc_cirdev)
		sc->sc_cirdev = NULL;
	if (child == sc->sc_dtvdev)
		sc->sc_dtvdev = NULL;
}

static bool
emdtv_read_eeprom(struct emdtv_softc *sc)
{
	i2c_addr_t ee = EM28XX_I2C_ADDR_EEPROM;
	uint8_t buf, *p = sc->sc_eeprom;
	struct emdtv_eeprom *eeprom = (struct emdtv_eeprom *)sc->sc_eeprom;
	int block, size = sizeof(sc->sc_eeprom);

	if (iic_exec(&sc->sc_i2c, I2C_OP_READ, ee, NULL, 0, NULL, 0, 0))
		return false;
	buf = 0;
	if (iic_exec(&sc->sc_i2c, I2C_OP_WRITE_WITH_STOP, ee, &buf, 1,
	    NULL, 0, 0))
		return false;
	while (size > 0) {
		block = min(size, 16);
		if (iic_exec(&sc->sc_i2c, I2C_OP_READ, ee, NULL, 0,
		    p, block, 0))
			return false;
		size -= block;
		p += block;
	}

	aprint_normal_dev(sc->sc_dev,
	    "id 0x%08x vendor 0x%04x product 0x%04x\n",
	    eeprom->id, eeprom->vendor, eeprom->product);

	sc->sc_vendor = eeprom->vendor;
	sc->sc_product = eeprom->product;

	return true;
}

static void
emdtv_board_setup(struct emdtv_softc *sc)
{
	switch (sc->sc_vendor) {
	case USB_VENDOR_EMPIA:
		switch (sc->sc_product) {
		case USB_PRODUCT_EMPIA_EM2883:
			emdtv_write_1(sc, UR_GET_STATUS, EM28XX_XCLK_REG, 0x97);
			emdtv_write_1(sc, UR_GET_STATUS, EM28XX_I2C_CLK_REG,
			    0x40);
			delay(10000);
			emdtv_write_1(sc, UR_GET_STATUS, 0x08, 0x2d);
			delay(10000);
			break;
		default:
			aprint_normal_dev(sc->sc_dev,
			    "unknown EMPIA board 0x%04x/0x%04x\n",
			    sc->sc_vendor, sc->sc_product);
			break;
		}
		break;
	case USB_VENDOR_AMD:
		switch (sc->sc_product) {
		case USB_PRODUCT_AMD_TV_WONDER_600_USB:
			emdtv_default_board_init(sc);
			break;
		default:
			aprint_normal_dev(sc->sc_dev,
			    "unknown AMD board 0x%04x/0x%04x\n",
			    sc->sc_vendor, sc->sc_product);
		}
		break;
	case USB_VENDOR_PINNACLE:
		switch (sc->sc_product) {
		case USB_PRODUCT_PINNACLE_PCTV800E:
			emdtv_default_board_init(sc);
			break;
		default:
			aprint_normal_dev(sc->sc_dev,
			    "unknown Pinnacle board 0x%04x/0x%04x\n",
			    sc->sc_vendor, sc->sc_product);
		}
		break;
	default:
		aprint_normal_dev(sc->sc_dev,
		    "unknown board 0x%04x:0x%04x\n",
		    sc->sc_vendor, sc->sc_product);
		break;
	}
}

/*
 * Register read/write
 */
uint8_t
emdtv_read_1(struct emdtv_softc *sc, uint8_t req, uint16_t index)
{
	uint8_t val;
	emdtv_read_multi_1(sc, req, index, &val, 1);
	return val;
}

void
emdtv_write_1(struct emdtv_softc *sc, uint8_t req, uint16_t index, uint8_t val)
{
	emdtv_write_multi_1(sc, req, index, &val, 1);
}

void
emdtv_read_multi_1(struct emdtv_softc *sc, uint8_t req, uint16_t index,
    uint8_t *datap, uint16_t count)
{
	usb_device_request_t request;
	usbd_status status;

	request.bmRequestType = UT_READ_VENDOR_DEVICE;
	request.bRequest = req;
	USETW(request.wValue, 0x0000);
	USETW(request.wIndex, index);
	USETW(request.wLength, count);

	KERNEL_LOCK(1, curlwp);
	status = usbd_do_request(sc->sc_udev, &request, datap);
	KERNEL_UNLOCK_ONE(curlwp);

	if (status != USBD_NORMAL_COMPLETION)
		aprint_error_dev(sc->sc_dev, "couldn't read %x/%x: %s\n",
		    req, index, usbd_errstr(status));

	if (emdtv_debug_regs) {
		int i;
		printf("%s [%s] c0 %02x 00 00 %02x 00 01 00 <<<",
		    __func__, status == 0 ? " OK" : "NOK", req, index);
		for (i = 0; status == 0 && i < count; i++)
			printf(" %02x", datap[i]);
		printf("\n");
	}
}

void
emdtv_write_multi_1(struct emdtv_softc *sc, uint8_t req, uint16_t index,
    const uint8_t *datap, uint16_t count)
{
	usb_device_request_t request;
	usbd_status status;

	request.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	request.bRequest = req;
	USETW(request.wValue, 0x0000);
	USETW(request.wIndex, index);
	USETW(request.wLength, count);

	KERNEL_LOCK(1, curlwp);
	status = usbd_do_request(sc->sc_udev, &request, __UNCONST(datap));
	KERNEL_UNLOCK_ONE(curlwp);

	if (status != USBD_NORMAL_COMPLETION)
		aprint_error_dev(sc->sc_dev, "couldn't read %x/%x: %s\n",
		    req, index, usbd_errstr(status));

	if (emdtv_debug_regs) {
		int i;
		printf("%s [%s] 40 %02x 00 00 %02x 00 %02x 00 >>>",
		    __func__, status == 0 ? " OK" : "NOK",
		    req, index, count);
		for (i = 0; i < count; ++i)
			printf(" %02x", datap[i]);
		printf("\n");
	}
}

bool
emdtv_gpio_ctl(struct emdtv_softc *sc, emdtv_gpio_reg_t gpioreg, bool onoff)
{
	const struct emdtv_board *eb = sc->sc_board;
	uint16_t gpio_value, reg;
	uint8_t gpio;
	uint8_t eeprom_offset = 0x3c;
	uint8_t val;

	if (sc->sc_board->eb_manual_gpio == false) {
		val = eeprom_offset + gpioreg;
		emdtv_write_1(sc, 0x03, 0xa0, val); 
		gpio_value = emdtv_read_1(sc, 0x02, 0xa0);
	} else {
		const struct emdtv_gpio_regs *r = &eb->eb_gpio_regs;
		switch (gpioreg) {
		case EMDTV_GPIO_TS1_ON:
			gpio_value = r->ts1_on;
			break;
		case EMDTV_GPIO_ANALOG_ON:
			gpio_value = r->a_on;
			break;
		case EMDTV_GPIO_TUNER1_ON:
			gpio_value = r->t1_on;
			break;
		case EMDTV_GPIO_TUNER1_RESET:
			gpio_value = r->t1_reset;
				break;
		case EMDTV_GPIO_DEMOD1_RESET:
			gpio_value = r->d1_reset;
			break;
		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown gpio reg %d\n", gpioreg);
			return false;
		}
	}

	if ((gpio_value & 0x80) == 0) {
		aprint_error_dev(sc->sc_dev,
		    "gpio reg %d not enabled\n", gpioreg);
		return false;
	}

	reg = gpio_value & 0x10 ? 0x04 : 0x08;
	gpio = emdtv_read_1(sc, UR_GET_STATUS, reg);
	if ((gpio_value & 0x40) == 0) {
		gpio &= ~((uint8_t)(1 << (gpio_value & 7)));

		if (onoff)
			gpio |= ((gpio_value >> 5) & 1) << (gpio_value & 7);
		else
			gpio |= (((gpio_value >> 5) & 1) ^ 1) <<
			    (gpio_value & 7);
		emdtv_write_1(sc, UR_GET_STATUS, reg, gpio);
	} else {
		gpio &= ~((uint8_t)(1 << (gpio_value & 0xf)));

		gpio |= ((gpio_value >> 5) & 1) << (gpio_value & 7);
		emdtv_write_1(sc, UR_GET_STATUS, reg, gpio);
		usbd_delay_ms(sc->sc_udev, 100);

		gpio &= ~((uint8_t)(1 << (gpio_value & 0xf)));
		gpio |= (((gpio_value >> 5) & 1) ^ 1) << (gpio_value & 7);
		emdtv_write_1(sc, UR_GET_STATUS, reg, gpio);
		usbd_delay_ms(sc->sc_udev, 100);
	}

	return true;
}

static void
emdtv_default_board_init(struct emdtv_softc *sc)
{
	emdtv_write_1(sc, UR_GET_STATUS, EM28XX_XCLK_REG, 0x27);
	emdtv_write_1(sc, UR_GET_STATUS, EM28XX_I2C_CLK_REG, 0x40);
	emdtv_write_1(sc, UR_GET_STATUS, 0x08, 0xff);
	emdtv_write_1(sc, UR_GET_STATUS, 0x04, 0x00);
	usbd_delay_ms(sc->sc_udev, 100);
	emdtv_write_1(sc, UR_GET_STATUS, 0x04, 0x08);
	usbd_delay_ms(sc->sc_udev, 100);
	emdtv_write_1(sc, UR_GET_STATUS, 0x08, 0xff);
	usbd_delay_ms(sc->sc_udev, 50);
	emdtv_write_1(sc, UR_GET_STATUS, 0x08, 0x2d);
	usbd_delay_ms(sc->sc_udev, 50);
	emdtv_write_1(sc, UR_GET_STATUS, 0x08, 0x3d);
	//emdtv_write_1(sc, UR_GET_STATUS, 0x0f, 0xa7);
	usbd_delay_ms(sc->sc_udev, 10);
}

MODULE(MODULE_CLASS_DRIVER, emdtv, "cir,lg3303,xc3028");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
emdtv_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_emdtv,
		    cfattach_ioconf_emdtv, cfdata_ioconf_emdtv);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_emdtv,
		    cfattach_ioconf_emdtv, cfdata_ioconf_emdtv);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
