/* $NetBSD: gpiolock.c,v 1.3 2009/12/06 22:33:44 dyoung Exp $ */

/*
 * Copyright (c) 2009 Marc Balmer <marc@msys.ch>
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
 * Driver for multi-position keylocks on GPIO pins
 */

#include "opt_keylock.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <dev/gpio/gpiovar.h>
#include <dev/keylock.h>

#define GPIOLOCK_MAXPINS	4
#define GPIOLOCK_MINPINS	2

struct gpiolock_softc {
	void *			sc_gpio;
	struct gpio_pinmap	sc_map;
	int			_map[GPIOLOCK_MAXPINS];

	int			sc_npins;
	int			sc_data;
	int			sc_dying;
};

int gpiolock_match(device_t, cfdata_t, void *);
void gpiolock_attach(device_t, device_t, void *);
int gpiolock_detach(device_t, int);
int gpiolock_activate(device_t, enum devact);
int gpiolock_position(void *);

CFATTACH_DECL_NEW(gpiolock, sizeof(struct gpiolock_softc),
	gpiolock_match, gpiolock_attach, gpiolock_detach, gpiolock_activate);

extern struct cfdriver gpiolock_cd;

int
gpiolock_match(device_t parent, cfdata_t cf,
    void *aux)
{
	struct gpio_attach_args *ga = aux;
	int npins;

	if (strcmp(ga->ga_dvname, cf->cf_name))
		return 0;

	if (ga->ga_offset == -1)
		return 0;

	/* Check number of pins */
	npins = gpio_npins(ga->ga_mask);
	if (npins < GPIOLOCK_MINPINS || npins > GPIOLOCK_MAXPINS) {
		aprint_debug("%s: invalid pin mask 0x%02x\n", cf->cf_name,
		    ga->ga_mask);
		return 0;
	}

	return 1;
}

void
gpiolock_attach(device_t parent, device_t self, void *aux)
{
	struct gpiolock_softc *sc = device_private(self);
	struct gpio_attach_args *ga = aux;
	int pin, caps;

	sc->sc_npins = gpio_npins(ga->ga_mask);

	/* Map pins */
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->_map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		aprint_error(": can't map pins\n");
		return;
	}

	/* Configure data pins */
	for (pin = 0; pin < sc->sc_npins; pin++) {
		caps = gpio_pin_caps(sc->sc_gpio, &sc->sc_map, pin);
		if (!(caps & GPIO_PIN_INPUT)) {
			aprint_error(": data pin is unable to read input\n");
			goto fail;
		}
		aprint_normal(" [%d]", sc->sc_map.pm_map[pin]);
		sc->sc_data = GPIO_PIN_INPUT;
		gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, pin, sc->sc_data);
	}

#ifdef KEYLOCK
	/* Register keylock */
	if (keylock_register(self, sc->sc_npins, gpiolock_position)) {
		aprint_error(": can't register keylock\n");
		goto fail;
	}
#endif
	pmf_device_register(self, NULL, NULL);

	aprint_normal("\n");
	return;

fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
gpiolock_detach(device_t self, int flags)
{
	struct gpiolock_softc *sc = device_private(self);

	pmf_device_deregister(self);
#ifdef KEYLOCK
	keylock_unregister(self, gpiolock_position);
#endif
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);

	return 0;
}

int
gpiolock_activate(device_t self, enum devact act)
{
	struct gpiolock_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}

}

int
gpiolock_position(void *arg)
{
	struct gpiolock_softc *sc = device_private((device_t)arg);
	int pos, pin;

	for (pos = pin = 0; pin < sc->sc_npins; pin++) {
		if (gpio_pin_read(sc->sc_gpio, &sc->sc_map, pin) ==
		    GPIO_PIN_HIGH)
			pos = pin + 1;
	}
	return pos;
}

