/* $NetBSD: gpiopwm.c,v 1.4 2014/02/25 18:30:09 pooka Exp $ */

/*
 * Copyright (c) 2011 Marc Balmer <marc@msys.ch>
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
 * Driver for pulsing GPIO pins in software
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/sysctl.h>

#include <dev/gpio/gpiovar.h>

#define GPIOPWM_NPINS	1

struct gpiopwm_softc {
	device_t		 sc_dev;
	void			*sc_gpio;
	struct gpio_pinmap	 sc_map;
	int			 _map[GPIOPWM_NPINS];

	callout_t		 sc_pulse;
	int			 sc_ticks_on;
	int			 sc_ticks_off;

	struct sysctllog	*sc_log;
	int			 sc_dying;
};

int gpiopwm_match(device_t, cfdata_t, void *);
void gpiopwm_attach(device_t, device_t, void *);
int gpiopwm_detach(device_t, int);
int gpiopwm_activate(device_t, enum devact);
static int gpiopwm_set_on(SYSCTLFN_ARGS);
static int gpiopwm_set_off(SYSCTLFN_ARGS);
static void gpiopwm_pulse(void *);

CFATTACH_DECL_NEW(gpiopwm, sizeof(struct gpiopwm_softc),
	gpiopwm_match, gpiopwm_attach, gpiopwm_detach, gpiopwm_activate);

extern struct cfdriver gpiopwm_cd;

int
gpiopwm_match(device_t parent, cfdata_t cf,
    void *aux)
{
	struct gpio_attach_args *ga = aux;

	if (strcmp(ga->ga_dvname, cf->cf_name))
		return 0;

	if (ga->ga_offset == -1)
		return 0;

	/* Check number of pins, must be 1 */
	if (gpio_npins(ga->ga_mask) != GPIOPWM_NPINS) {
		aprint_debug("%s: invalid pin mask 0x%02x\n", cf->cf_name,
		    ga->ga_mask);
		return 0;
	}
	return 1;
}

void
gpiopwm_attach(device_t parent, device_t self, void *aux)
{
	struct gpiopwm_softc *sc = device_private(self);
	struct gpio_attach_args *ga = aux;
	const struct sysctlnode *node;

	sc->sc_dev = self;

	/* Map pin */
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->_map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		aprint_error(": can't map pin\n");
		return;
	}
	aprint_normal(" [%d]", sc->sc_map.pm_map[0]);
	pmf_device_register(self, NULL, NULL);

	callout_init(&sc->sc_pulse, CALLOUT_MPSAFE);
	callout_setfunc(&sc->sc_pulse, gpiopwm_pulse, sc);

        sysctl_createv(&sc->sc_log, 0, NULL, &node,
            0,
            CTLTYPE_NODE, device_xname(sc->sc_dev),
            SYSCTL_DESCR("GPIO software PWM"),
            NULL, 0, NULL, 0,
            CTL_HW, CTL_CREATE, CTL_EOL);

        if (node == NULL) {
		printf(": can't create sysctl node\n");
                return;
	}

        sysctl_createv(&sc->sc_log, 0, &node, NULL,
            CTLFLAG_READWRITE,
            CTLTYPE_INT, "on",
            SYSCTL_DESCR("PWM 'on' period in ticks"),
            gpiopwm_set_on, 0, (void *)sc, 0,
	    CTL_CREATE, CTL_EOL);
        sysctl_createv(&sc->sc_log, 0, &node, NULL,
            CTLFLAG_READWRITE,
            CTLTYPE_INT, "off",
            SYSCTL_DESCR("PWM 'off' period in ticks"),
            gpiopwm_set_off, 0, (void *)sc, 0,
	    CTL_CREATE, CTL_EOL);

	aprint_normal("\n");
	return;
}

int
gpiopwm_detach(device_t self, int flags)
{
	struct gpiopwm_softc *sc = device_private(self);

	callout_halt(&sc->sc_pulse, NULL);
	callout_destroy(&sc->sc_pulse);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_LOW);

	pmf_device_deregister(self);
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);

	if (sc->sc_log != NULL) {
		sysctl_teardown(&sc->sc_log);
		sc->sc_log = NULL;
	}
	return 0;
}

static int
gpiopwm_set_on(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct gpiopwm_softc *sc;
	int val, error;

	node = *rnode;
	sc = node.sysctl_data;

	callout_halt(&sc->sc_pulse, NULL);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_LOW);
	node.sysctl_data = &val;

	val = sc->sc_ticks_on;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	sc->sc_ticks_on = val;
	if (sc->sc_ticks_on > 0 && sc->sc_ticks_off > 0) {
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_HIGH);
		callout_schedule(&sc->sc_pulse, sc->sc_ticks_on);
	}
	return 0;
}

static int
gpiopwm_set_off(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct gpiopwm_softc *sc;
	int val, error;

	node = *rnode;
	sc = node.sysctl_data;

	callout_halt(&sc->sc_pulse, NULL);
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_LOW);
	node.sysctl_data = &val;

	val = sc->sc_ticks_off;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	sc->sc_ticks_off = val;
	if (sc->sc_ticks_on > 0 && sc->sc_ticks_off > 0) {
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_HIGH);
		callout_schedule(&sc->sc_pulse, sc->sc_ticks_on);
	}
	return 0;
}

static void
gpiopwm_pulse(void *arg)
{
	struct gpiopwm_softc *sc;

	sc = arg;
	if (gpio_pin_read(sc->sc_gpio, &sc->sc_map, 0) == GPIO_PIN_HIGH) {
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_LOW);
		callout_schedule(&sc->sc_pulse, sc->sc_ticks_off);
	} else {
		gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_HIGH);
		callout_schedule(&sc->sc_pulse, sc->sc_ticks_on);
	}
}

int
gpiopwm_activate(device_t self, enum devact act)
{
	struct gpiopwm_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		return 0;
	default:
		return EOPNOTSUPP;
	}

}
