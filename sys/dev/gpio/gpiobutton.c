/* $NetBSD: gpiobutton.c,v 1.3 2015/10/04 18:35:44 jmcneill Exp $ */

/*-
 * Copyright (c) 2015 Jared D. McNeill <jmcneill@invisible.ca>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "locators.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gpiobutton.c,v 1.3 2015/10/04 18:35:44 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/gpio.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_taskq.h>

#include <dev/gpio/gpiovar.h>

#define GPIOBUTTON_POLL_INTERVAL	mstohz(200)

#define GPIOBUTTON_POLARITY_MASK	0x80
#define GPIOBUTTON_POLARITY_ACTIVE_LOW	0
#define GPIOBUTTON_POLARITY_ACTIVE_HIGH	1
#define GPIOBUTTON_TYPE_MASK		0x0f
#define GPIOBUTTON_TYPE_POWER		1
#define GPIOBUTTON_TYPE_SLEEP		2

static int	gpiobutton_match(device_t, cfdata_t, void *);
static void	gpiobutton_attach(device_t, device_t, void *);

struct gpiobutton_softc {
	device_t		sc_dev;
	void			*sc_gpio;
	struct gpio_pinmap	sc_map;
	int			sc_pinmap[1];
	bool			sc_active_high;

	struct sysmon_pswitch	sc_smpsw;

	callout_t		sc_tick;
	bool			sc_state;
};

static bool	gpiobutton_is_pressed(struct gpiobutton_softc *);
static void	gpiobutton_tick(void *);
static void	gpiobutton_task(void *);

CFATTACH_DECL_NEW(gpiobutton, sizeof(struct gpiobutton_softc),
	gpiobutton_match, gpiobutton_attach, NULL, NULL);

static int
gpiobutton_match(device_t parent, cfdata_t cf, void *aux)
{
	struct gpio_attach_args * const ga = aux;

	if (strcmp(ga->ga_dvname, cf->cf_name) != 0)
		return 0;

	if (ga->ga_offset == -1 || gpio_npins(ga->ga_mask) != 1)
		return 0;

	const u_int type = __SHIFTOUT(ga->ga_flags, GPIOBUTTON_TYPE_MASK);

	switch (type) {
	case GPIOBUTTON_TYPE_POWER:
	case GPIOBUTTON_TYPE_SLEEP:
		return 1;
	default:
		return 0;
	}
}

static void
gpiobutton_attach(device_t parent, device_t self, void *aux)
{
	struct gpiobutton_softc * const sc = device_private(self);
	struct gpio_attach_args * const ga = aux;
	const char *desc;
	int caps;

	const u_int type = __SHIFTOUT(ga->ga_flags, GPIOBUTTON_TYPE_MASK);
	const u_int pol = __SHIFTOUT(ga->ga_flags, GPIOBUTTON_POLARITY_MASK);

	sc->sc_dev = self;
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->sc_pinmap;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		aprint_error(": couldn't map pins\n");
		return;
	}
	sc->sc_active_high = pol == GPIOBUTTON_POLARITY_ACTIVE_HIGH;

	caps = gpio_pin_caps(sc->sc_gpio, &sc->sc_map, 0);
	if ((caps & GPIO_PIN_INPUT) == 0) {
		aprint_error(": pin is not an input pin\n");
		return;
	}

	gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_INPUT);

	sc->sc_smpsw.smpsw_name = device_xname(self);
	switch (type) {
	case GPIOBUTTON_TYPE_POWER:
		sc->sc_smpsw.smpsw_type = PSWITCH_TYPE_POWER;
		desc = "Power";
		break;
	case GPIOBUTTON_TYPE_SLEEP:
		sc->sc_smpsw.smpsw_type = PSWITCH_TYPE_SLEEP;
		desc = "Sleep";
		break;
	default:
		panic("%s: impossible", __func__);
	}

	aprint_naive("\n");
	aprint_normal(": %s button\n", desc);

	sysmon_pswitch_register(&sc->sc_smpsw);

	callout_init(&sc->sc_tick, CALLOUT_MPSAFE);
	callout_setfunc(&sc->sc_tick, gpiobutton_tick, sc);

	gpiobutton_tick(sc);
}

static bool
gpiobutton_is_pressed(struct gpiobutton_softc *sc)
{
	int val;

	val = gpio_pin_read(sc->sc_gpio, &sc->sc_map, 0);
	if (!sc->sc_active_high)
		val = !val;

	return val;
}

static void
gpiobutton_tick(void *priv)
{
	struct gpiobutton_softc * const sc = priv;

	const bool new_state = gpiobutton_is_pressed(sc);
	if (new_state != sc->sc_state) {
		sc->sc_state = new_state;
		sysmon_task_queue_sched(0, gpiobutton_task, sc);
	}
	callout_schedule(&sc->sc_tick, GPIOBUTTON_POLL_INTERVAL);
}

static void
gpiobutton_task(void *priv)
{
	struct gpiobutton_softc * const sc = priv;

	sysmon_pswitch_event(&sc->sc_smpsw,
	    sc->sc_state ? PSWITCH_EVENT_PRESSED : PSWITCH_EVENT_RELEASED);
}
