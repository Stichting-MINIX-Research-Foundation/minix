/* $NetBSD: gpiorfkill.c,v 1.1 2015/05/29 23:17:13 jmcneill Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: gpiorfkill.c,v 1.1 2015/05/29 23:17:13 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/gpio.h>

#include <dev/gpio/gpiovar.h>

static int	gpiorfkill_match(device_t, cfdata_t, void *);
static void	gpiorfkill_attach(device_t, device_t, void *);

struct gpiorfkill_softc {
	device_t		sc_dev;
	void			*sc_gpio;
	struct gpio_pinmap	sc_map;
	int			sc_pinmap[1];

	int			sc_state;

	struct sysctllog	*sc_sysctllog;
	int			sc_sysctlnode;
};

static void	gpiorfkill_enable(struct gpiorfkill_softc *, int);
static void	gpiorfkill_sysctl_init(struct gpiorfkill_softc *);
static int	gpiorfkill_enable_helper(SYSCTLFN_PROTO);

CFATTACH_DECL_NEW(gpiorfkill, sizeof(struct gpiorfkill_softc),
	gpiorfkill_match, gpiorfkill_attach, NULL, NULL);

static int
gpiorfkill_match(device_t parent, cfdata_t cf, void *aux)
{
	struct gpio_attach_args * const ga = aux;

	if (strcmp(ga->ga_dvname, cf->cf_name) != 0)
		return 0;

	if (ga->ga_offset == -1 || gpio_npins(ga->ga_mask) != 1)
		return 0;

	return 1;
}

static void
gpiorfkill_attach(device_t parent, device_t self, void *aux)
{
	struct gpiorfkill_softc * const sc = device_private(self);
	struct gpio_attach_args * const ga = aux;
	int caps;

	sc->sc_dev = self;
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->sc_pinmap;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		aprint_error(": couldn't map pins\n");
		return;
	}

	caps = gpio_pin_caps(sc->sc_gpio, &sc->sc_map, 0);
	if ((caps & GPIO_PIN_OUTPUT) == 0) {
		aprint_error(": pin is not an output pin\n");
		return;
	}

	gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, 0, GPIO_PIN_OUTPUT);

	aprint_naive("\n");
	aprint_normal("\n");

	gpiorfkill_enable(sc, 1);
	gpiorfkill_sysctl_init(sc);
}

static void
gpiorfkill_enable(struct gpiorfkill_softc *sc, int enable)
{
	sc->sc_state = enable;
	gpio_pin_write(sc->sc_gpio, &sc->sc_map, 0, sc->sc_state);
}

static void
gpiorfkill_sysctl_init(struct gpiorfkill_softc *sc)
{
	const struct sysctlnode *node, *devnode;
	int error;

	error = sysctl_createv(&sc->sc_sysctllog, 0, NULL, &devnode,
	    0, CTLTYPE_NODE, device_xname(sc->sc_dev), NULL,
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL);
	if (error)
		goto sysctl_failed;

	error = sysctl_createv(&sc->sc_sysctllog, 0, &devnode, &node,
	    CTLFLAG_READWRITE, CTLTYPE_INT, "enable", NULL,
	    gpiorfkill_enable_helper, 0, (void *)sc, 0,
	    CTL_CREATE, CTL_EOL);
	if (error)
		goto sysctl_failed;
	sc->sc_sysctlnode = node->sysctl_num;

	return;

sysctl_failed:
	aprint_error_dev(sc->sc_dev, "couldn't create sysctl nodes (%d)\n",
	    error);
	sysctl_teardown(&sc->sc_sysctllog);
}

static int
gpiorfkill_enable_helper(SYSCTLFN_ARGS)
{
	struct gpiorfkill_softc *sc;
	struct sysctlnode node;
	int error, enable;

	node = *rnode;
	sc = node.sysctl_data;
	enable = sc->sc_state;
	node.sysctl_data = &enable;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	enable = !!enable;
	gpiorfkill_enable(sc, enable);

	return 0;
}
