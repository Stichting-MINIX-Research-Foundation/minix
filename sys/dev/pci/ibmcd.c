/* $NetBSD: ibmcd.c,v 1.1 2012/12/17 20:38:00 mbalmer Exp $ */

/*
 * Copyright (c) 2012 Marc Balmer <marc@msys.ch>
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
 */

/*
 * Driver for the IBM 4810 BSP cash drawer port.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/gpio/gpiovar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

/* registers */
#define IBMCD_STATUS		0x00
#define IBMCD_CONTROL		0x01

#define IBMCD_CMD_OPEN		0x6d
#define IBMCD_DO_OPEN		0x01

#define IBMCD_CLOSED		0x80
#define IBMCD_NOT_CONNECTED	0x40

/* GPIO constants */
#define IBMCD_NPINS		3
#define PIN_OPEN		0
#define	PIN_STATUS		1
#define PIN_CONNECTED		2

struct ibmcd_softc {
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_iosize;

	/* GPIO interface */
	struct gpio_chipset_tag	sc_gpio_gc;
	gpio_pin_t		sc_gpio_pins[IBMCD_NPINS];
};

static int ibmcd_match(device_t, cfdata_t, void *);
static void ibmcd_attach(device_t, device_t, void *);
static int ibmcd_detach(device_t, int);
#if (__NetBSD_Version__ >= 600000000)
static bool ibmcd_suspend(device_t, const pmf_qual_t *);
static bool ibmcd_resume(device_t, const pmf_qual_t *);
#endif
int ibmcd_gpio_pin_read(void *, int);
void ibmcd_gpio_pin_write(void *, int, int);
void ibmcd_gpio_pin_ctl(void *, int, int);

CFATTACH_DECL2_NEW(ibmcd, sizeof(struct ibmcd_softc), ibmcd_match,
    ibmcd_attach, ibmcd_detach, NULL, NULL, NULL);

static int
ibmcd_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_IBM &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_IBM_4810_BSP)
		return 1;
	return 0;
}

void
ibmcd_attach(device_t parent, device_t self, void *aux)
{
	struct ibmcd_softc *sc = device_private(self);
	struct pci_attach_args *const pa = (struct pci_attach_args *)aux;
	struct gpiobus_attach_args gba;
	pcireg_t memtype;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_iosize)) {
		aprint_error("\n");
		aprint_error_dev(self, "PCI %s region not found\n",
		    memtype == PCI_MAPREG_TYPE_IO ? "I/O" : "memory");
		return;
	}
	printf(": IBM 4810 BSP cash drawer\n");

#if (__NetBSD_Version__ >= 600000000)
	pmf_device_register(self, ibmcd_suspend, ibmcd_resume);
#endif
	/* Initialize pins array */
	sc->sc_gpio_pins[PIN_OPEN].pin_num = 0;
	sc->sc_gpio_pins[PIN_OPEN].pin_caps = GPIO_PIN_OUTPUT;
	sc->sc_gpio_pins[PIN_STATUS].pin_num = 1;
	sc->sc_gpio_pins[PIN_STATUS].pin_caps = GPIO_PIN_INPUT;
	sc->sc_gpio_pins[PIN_CONNECTED].pin_num = 2;
	sc->sc_gpio_pins[PIN_CONNECTED].pin_caps = GPIO_PIN_INPUT;

	/* Create controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = ibmcd_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = ibmcd_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = ibmcd_gpio_pin_ctl;

	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = IBMCD_NPINS;

	/* Attach GPIO framework */
	config_found_ia(self, "gpiobus", &gba, gpiobus_print);

}

static int
ibmcd_detach(device_t self, int flags)
{
	struct ibmcd_softc *sc = device_private(self);

#if (__NetBSD_Version__ >= 600000000)
	pmf_device_deregister(self);
#endif
	if (sc->sc_iosize)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
	return 0;
}

#if (__NetBSD_Version__ >= 600000000)
static bool
ibmcd_resume(device_t self, const pmf_qual_t *qual)
{
	return true;
}

static bool
ibmcd_suspend(device_t self, const pmf_qual_t *qual)
{
	return true;
}
#endif

int
ibmcd_gpio_pin_read(void *arg, int pin)
{
	struct ibmcd_softc *sc = arg;
	uint8_t data;

	data = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IBMCD_STATUS);

	switch (pin) {
	case PIN_STATUS:
		return data & IBMCD_CLOSED ? 0 : 1;
	case PIN_CONNECTED:
		return data & IBMCD_NOT_CONNECTED ? 0 : 1;
	default:
		return 0;
	}
}

void
ibmcd_gpio_pin_write(void *arg, int pin, int value)
{
	struct ibmcd_softc *sc = arg;

	if (pin != PIN_OPEN)
		return;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IBMCD_STATUS,
	    value ? IBMCD_DO_OPEN : 0);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IBMCD_CONTROL,
	    IBMCD_CMD_OPEN);
}

void
ibmcd_gpio_pin_ctl(void *arg, int pin, int flags)
{
	/* We ignore pin control requests since the pin functions are fixed. */
}

MODULE(MODULE_CLASS_DRIVER, ibmcd, "pci");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
ibmcd_modcmd(modcmd_t cmd, void *opaque)
{
	int error;

	error = 0;
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_ibmcd,
		    cfattach_ioconf_ibmcd, cfdata_ioconf_ibmcd);
		if (error)
			aprint_error("%s: unable to init component\n",
			    ibmcd_cd.cd_name);
#endif
		break;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		config_fini_component(cfdriver_ioconf_ibmcd,
		    cfattach_ioconf_ibmcd, cfdata_ioconf_ibmcd);
#endif
		break;
	default:
		error = ENOTTY;
	}
	return error;
}
