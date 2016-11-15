/* $NetBSD: gpioow.c,v 1.14 2011/09/07 07:43:05 mbalmer Exp $ */
/*	$OpenBSD: gpioow.c,v 1.1 2006/03/04 16:27:03 grange Exp $	*/

/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
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
__KERNEL_RCSID(0, "$NetBSD: gpioow.c,v 1.14 2011/09/07 07:43:05 mbalmer Exp $");

/*
 * 1-Wire bus bit-banging through GPIO pin.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/module.h>

#include <dev/gpio/gpiovar.h>

#include <dev/onewire/onewirevar.h>

#define GPIOOW_NPINS		1
#define GPIOOW_PIN_DATA		0

struct gpioow_softc {
	void *			sc_gpio;
	struct gpio_pinmap	sc_map;
	int			_map[GPIOOW_NPINS];

	struct onewire_bus	sc_ow_bus;
	device_t		sc_ow_dev;

	int			sc_data;
	int			sc_dying;
};

int	gpioow_match(device_t, cfdata_t, void *);
void	gpioow_attach(device_t, device_t, void *);
int	gpioow_detach(device_t, int);
int	gpioow_activate(device_t, enum devact);

int	gpioow_ow_reset(void *);
int	gpioow_ow_bit(void *, int);

void	gpioow_bb_rx(void *);
void	gpioow_bb_tx(void *);
int	gpioow_bb_get(void *);
void	gpioow_bb_set(void *, int);

CFATTACH_DECL_NEW(gpioow, sizeof(struct gpioow_softc),
	gpioow_match, gpioow_attach, gpioow_detach, gpioow_activate);

extern struct cfdriver gpioow_cd;

static const struct onewire_bbops gpioow_bbops = {
	gpioow_bb_rx,
	gpioow_bb_tx,
	gpioow_bb_get,
	gpioow_bb_set
};

int
gpioow_match(device_t parent, cfdata_t cf, void *aux)
{
	struct gpio_attach_args *ga = aux;

	if (strcmp(ga->ga_dvname, cf->cf_name))
		return 0;

	if (ga->ga_offset == -1)
		return 0;

	/* Check that we have enough pins */
	if (gpio_npins(ga->ga_mask) != GPIOOW_NPINS) {
		aprint_debug("%s: invalid pin mask 0x%02x\n", cf->cf_name,
		    ga->ga_mask);
		return 0;
	}
	return 1;
}

void
gpioow_attach(device_t parent, device_t self, void *aux)
{
	struct gpioow_softc *sc = device_private(self);
	struct gpio_attach_args *ga = aux;
	struct onewirebus_attach_args oba;
	int caps;

	/* Map pins */
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->_map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		aprint_error(": can't map pins\n");
		goto finish;
	}

	/* Configure data pin */
	caps = gpio_pin_caps(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA);
	if (!(caps & GPIO_PIN_OUTPUT)) {
		aprint_error(": data pin is unable to drive output\n");
		gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
		goto finish;
	}
	if (!(caps & GPIO_PIN_INPUT)) {
		aprint_error(": data pin is unable to read input\n");
		gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
		goto finish;
	}
	aprint_normal(": DATA[%d]", sc->sc_map.pm_map[GPIOOW_PIN_DATA]);
	sc->sc_data = GPIO_PIN_OUTPUT;
	if (caps & GPIO_PIN_OPENDRAIN) {
		aprint_normal(" open-drain");
		sc->sc_data |= GPIO_PIN_OPENDRAIN;
	} else if ((caps & GPIO_PIN_PUSHPULL) && (caps & GPIO_PIN_TRISTATE)) {
		aprint_normal(" push-pull tri-state");
		sc->sc_data |= GPIO_PIN_PUSHPULL;
	}
	if (caps & GPIO_PIN_PULLUP) {
		aprint_normal(" pull-up");
		sc->sc_data |= GPIO_PIN_PULLUP;
	}
	gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA, sc->sc_data);

	aprint_normal("\n");

	/* Attach 1-Wire bus */
	sc->sc_ow_bus.bus_cookie = sc;
	sc->sc_ow_bus.bus_reset = gpioow_ow_reset;
	sc->sc_ow_bus.bus_bit = gpioow_ow_bit;

	memset(&oba, 0, sizeof(oba));
	oba.oba_bus = &sc->sc_ow_bus;
	sc->sc_ow_dev = config_found(self, &oba, onewirebus_print);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error("%s: could not establish power handler\n",
		    device_xname(self));
finish:
	return;
}

int
gpioow_detach(device_t self, int flags)
{
	struct gpioow_softc *sc = device_private(self);
	int rv = 0;

	if (sc->sc_ow_dev != NULL)
		rv = config_detach(sc->sc_ow_dev, flags);

	if (!rv) {
		gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
		pmf_device_deregister(self);
	}
	return rv;
}

int
gpioow_activate(device_t self, enum devact act)
{
	struct gpioow_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
gpioow_ow_reset(void *arg)
{
	return (onewire_bb_reset(&gpioow_bbops, arg));
}

int
gpioow_ow_bit(void *arg, int value)
{
	return (onewire_bb_bit(&gpioow_bbops, arg, value));
}

void
gpioow_bb_rx(void *arg)
{
	struct gpioow_softc *sc = arg;
	int data = sc->sc_data;

	data &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_TRISTATE);
	data |= GPIO_PIN_INPUT;
	if (data & GPIO_PIN_PUSHPULL)
		data |= GPIO_PIN_TRISTATE;
	if (sc->sc_data != data) {
		sc->sc_data = data;
		gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA,
		    sc->sc_data);
	}
}

void
gpioow_bb_tx(void *arg)
{
	struct gpioow_softc *sc = arg;
	int data = sc->sc_data;

	data &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_TRISTATE);
	data |= GPIO_PIN_OUTPUT;
	if (sc->sc_data != data) {
		sc->sc_data = data;
		gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA,
		    sc->sc_data);
	}
}

int
gpioow_bb_get(void *arg)
{
	struct gpioow_softc *sc = arg;

	return (gpio_pin_read(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA) ==
	    GPIO_PIN_HIGH ? 1 : 0);
}

void
gpioow_bb_set(void *arg, int value)
{
	struct gpioow_softc *sc = arg;

	gpio_pin_write(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA,
	    value ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

MODULE(MODULE_CLASS_DRIVER, gpioow, "gpio,onewire");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
gpioow_modcmd(modcmd_t cmd, void *opaque)
{
	int error;

	error = 0;
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_gpioow,
		    cfattach_ioconf_gpioow, cfdata_ioconf_gpioow);
		if (error)
			aprint_error("%s: unable to init component\n",
			    gpioow_cd.cd_name);
#endif
		break;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		config_fini_component(cfdriver_ioconf_gpioow,
		    cfattach_ioconf_gpioow, cfdata_ioconf_gpioow);
#endif
		break;
	default:
		error = ENOTTY;
	}
	return error;
}
