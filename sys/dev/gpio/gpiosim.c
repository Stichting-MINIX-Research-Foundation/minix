/* $NetBSD: gpiosim.c,v 1.18 2015/08/20 14:40:18 christos Exp $ */
/*      $OpenBSD: gpiosim.c,v 1.1 2008/11/23 18:46:49 mbalmer Exp $	*/

/*
 * Copyright (c) 2007 - 2011, 2013 Marc Balmer <marc@msys.ch>
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* 64 bit wide GPIO simulator  */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/ioccom.h>
#include <dev/gpio/gpiovar.h>

#include "gpiosim.h"
#include "ioconf.h"

#define	GPIOSIM_NPINS	64

struct gpiosim_softc {
	device_t		sc_dev;
	device_t		sc_gdev;	/* gpio that attaches here */
	uint64_t		sc_state;
	struct gpio_chipset_tag	sc_gpio_gc;
	gpio_pin_t		sc_gpio_pins[GPIOSIM_NPINS];

	struct sysctllog	*sc_log;
};

static int	gpiosim_match(device_t, cfdata_t, void *);
static void	gpiosim_attach(device_t, device_t, void *);
static int	gpiosim_detach(device_t, int);
static int	gpiosim_sysctl(SYSCTLFN_PROTO);

static int	gpiosim_pin_read(void *, int);
static void	gpiosim_pin_write(void *, int, int);
static void	gpiosim_pin_ctl(void *, int, int);

CFATTACH_DECL_NEW(gpiosim, sizeof(struct gpiosim_softc), gpiosim_match,
    gpiosim_attach, gpiosim_detach, NULL);

extern struct cfdriver gpiosim_cd;

static int
gpiosim_match(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

void
gpiosimattach(int num __unused)
{
	cfdata_t cf;
	int n, err;

	err = config_cfattach_attach(gpiosim_cd.cd_name, &gpiosim_ca);
	if (err)
		printf("%s: unable to register cfattach\n", gpiosim_cd.cd_name);

	for (n = 0; n < NGPIOSIM; n++) {
		cf = malloc(sizeof(*cf), M_DEVBUF, M_WAITOK);
		cf->cf_name = "gpiosim";
		cf->cf_atname = "gpiosim";
		cf->cf_unit = n;
		cf->cf_fstate = FSTATE_NOTFOUND;
		config_attach_pseudo(cf);
	}
}

static void
gpiosim_attach(device_t parent, device_t self, void *aux)
{
	struct gpiosim_softc *sc = device_private(self);
	struct gpiobus_attach_args gba;
	const struct sysctlnode *node;
	int i;

	sc->sc_dev = self;

	printf("%s", device_xname(sc->sc_dev));

	/* initialize pin array */
	for (i = 0; i < GPIOSIM_NPINS; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT |
		    GPIO_PIN_OUTPUT | GPIO_PIN_OPENDRAIN |
		    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN |
		    GPIO_PIN_INVIN | GPIO_PIN_INVOUT;

		/* read initial state */
		sc->sc_gpio_pins[i].pin_flags = GPIO_PIN_INPUT;
	}
	sc->sc_state = 0;

	/* create controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = gpiosim_pin_read;
	sc->sc_gpio_gc.gp_pin_write = gpiosim_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = gpiosim_pin_ctl;

	/* gba.gba_name = "gpio"; */
	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = GPIOSIM_NPINS;

	pmf_device_register(self, NULL, NULL);

        sysctl_createv(&sc->sc_log, 0, NULL, &node,
            0,
            CTLTYPE_NODE, device_xname(sc->sc_dev),
            SYSCTL_DESCR("GPIO simulator"),
            NULL, 0, NULL, 0,
            CTL_HW, CTL_CREATE, CTL_EOL);

        if (node == NULL) {
		printf(": can't create sysctl node\n");
                return;
	}

        sysctl_createv(&sc->sc_log, 0, &node, NULL,
            CTLFLAG_READWRITE,
            CTLTYPE_QUAD, "value",
            SYSCTL_DESCR("Current GPIO simulator value"),
            gpiosim_sysctl, 0, (void *)sc, 0,
	    CTL_CREATE, CTL_EOL);

	printf(": simulating %d pins\n", GPIOSIM_NPINS);
	sc->sc_gdev = config_found_ia(self, "gpiobus", &gba, gpiobus_print);
}

static int
gpiosim_detach(device_t self, int flags)
{
	struct gpiosim_softc *sc = device_private(self);

	/* Detach the gpio driver that attached here */
	if (sc->sc_gdev != NULL)
		config_detach(sc->sc_gdev, 0);

	pmf_device_deregister(self);
	if (sc->sc_log != NULL) {
		sysctl_teardown(&sc->sc_log);
		sc->sc_log = NULL;
	}
	return 0;
}

static int
gpiosim_sysctl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct gpiosim_softc *sc;
	uint64_t val, error;

	node = *rnode;
	sc = node.sysctl_data;

	node.sysctl_data = &val;

	val = sc->sc_state;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	sc->sc_state = val;
	return 0;
}

static int
gpiosim_pin_read(void *arg, int pin)
{
	struct gpiosim_softc *sc = arg;

	if (sc->sc_state & (1LL << pin))
		return GPIO_PIN_HIGH;
	else
		return GPIO_PIN_LOW;
}

static void
gpiosim_pin_write(void *arg, int pin, int value)
{
	struct gpiosim_softc *sc = arg;

	if (value == 0)
		sc->sc_state &= ~(1LL << pin);
	else
		sc->sc_state |= (1LL << pin);
}

static void
gpiosim_pin_ctl(void *arg, int pin, int flags)
{
	struct gpiosim_softc *sc = arg;

	sc->sc_gpio_pins[pin].pin_flags = flags;
}

MODULE(MODULE_CLASS_DRIVER, gpiosim, "gpio");

#ifdef _MODULE
static const struct cfiattrdata gpiobus_iattrdata = {
	"gpiobus", 0, { { NULL, NULL, 0 },}
};
static const struct cfiattrdata *const gpiosim_attrs[] = {
	&gpiobus_iattrdata, NULL
};
CFDRIVER_DECL(gpiosim, DV_DULL, gpiosim_attrs);
extern struct cfattach gpiosim_ca;
static int gpiosimloc[] = {
	-1,
	-1,
	-1
};
static struct cfdata gpiosim_cfdata[] = {
	{
		.cf_name = "gpiosim",
		.cf_atname = "gpiosim",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = gpiosimloc,
		.cf_flags = 0,
		.cf_pspec = NULL,
	},
	{ NULL, NULL, 0, FSTATE_NOTFOUND, NULL, 0, NULL }
};
#endif

static int
gpiosim_modcmd(modcmd_t cmd, void *opaque)
{
#ifdef _MODULE
	int error = 0;
#endif
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_cfdriver_attach(&gpiosim_cd);
		if (error)
			return error;

		error = config_cfattach_attach(gpiosim_cd.cd_name,
		    &gpiosim_ca);
		if (error) {
			config_cfdriver_detach(&gpiosim_cd);
			aprint_error("%s: unable to register cfattach\n",
			    gpiosim_cd.cd_name);
			return error;
		}
		error = config_cfdata_attach(gpiosim_cfdata, 1);
		if (error) {
			config_cfattach_detach(gpiosim_cd.cd_name,
			    &gpiosim_ca);
			config_cfdriver_detach(&gpiosim_cd);
			aprint_error("%s: unable to register cfdata\n",
			    gpiosim_cd.cd_name);
			return error;
		}
		config_attach_pseudo(gpiosim_cfdata);
#endif
		return 0;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_cfdata_detach(gpiosim_cfdata);
		if (error)
			return error;

		config_cfattach_detach(gpiosim_cd.cd_name, &gpiosim_ca);
		config_cfdriver_detach(&gpiosim_cd);
#endif
		return 0;
	case MODULE_CMD_AUTOUNLOAD:
		/* no auto-unload */
		return EBUSY;
	default:
		return ENOTTY;
	}
}
