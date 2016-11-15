/* $NetBSD: ptcd.c,v 1.3 2012/12/17 17:46:27 mbalmer Exp $ */

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
 * Driver for the Protech PS3100 cash drawer port.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <dev/gpio/gpiovar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

/*
 * To assert the cash drawer pulse, 0x00 must be written to I/O register 0x48f
 * To stop the cash drawer pulse, 0x02 must be written to I/O register 0x48f
 * To read out the current cash drawer state (sense pin), read bit 7 of
 * I/O register 0x48d
 */

#define PTCD_READ_REG	0x48d

/* register offsets, counted from PTCD_READ_REG */
#define PTCD_READ	0x00
#define PTCD_WRITE	0x02

/* Read mask */
#define PTCD_SENSE	0x80

/* Write values */
#define PTCD_OPEN	0x00
#define PTCD_CLOSE	0x02

#define PTCD_NPINS	2
#define PIN_WRITE	0
#define	PIN_READ	1

#define PTCD_ADDR_SIZE	0x03

struct ptcd_softc {
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	/* GPIO interface */
	struct gpio_chipset_tag	sc_gpio_gc;
	gpio_pin_t		sc_gpio_pins[PTCD_NPINS];
};

int ptcd_match(device_t, struct cfdata *, void *);
void ptcd_attach(device_t, device_t, void *);

int	ptcd_gpio_pin_read(void *, int);
void	ptcd_gpio_pin_write(void *, int, int);
void	ptcd_gpio_pin_ctl(void *, int, int);

CFATTACH_DECL2_NEW(ptcd, sizeof(struct ptcd_softc), ptcd_match, ptcd_attach,
    NULL, NULL, NULL, NULL);

int
ptcd_match(device_t parent, struct cfdata *match, void *aux)
{
	struct isa_attach_args *ia = aux;

	if (ia->ia_nio < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr != PTCD_READ_REG)
		return 0;

	ia->ia_io[0].ir_size = PTCD_ADDR_SIZE;
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

	return 1;
}

void
ptcd_attach(device_t parent, device_t self, void *aux)
{
	struct ptcd_softc *sc;
	struct isa_attach_args *ia = aux;
	struct gpiobus_attach_args gba;

	sc = device_private(self);

	sc->sc_iot = ia->ia_iot;
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, PTCD_ADDR_SIZE, 0,
	    &sc->sc_ioh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	aprint_normal(": Protech PS3100 cash drawer\n");

	/* Initialize pins array */
	sc->sc_gpio_pins[PIN_WRITE].pin_num = 0;
	sc->sc_gpio_pins[PIN_WRITE].pin_caps = GPIO_PIN_OUTPUT;
	sc->sc_gpio_pins[PIN_READ].pin_num = 1;
	sc->sc_gpio_pins[PIN_READ].pin_caps = GPIO_PIN_INPUT;

	/* Create controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = ptcd_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = ptcd_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = ptcd_gpio_pin_ctl;

	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = PTCD_NPINS;

	/* Attach GPIO framework */
	config_found_ia(self, "gpiobus", &gba, gpiobus_print);
}

int
ptcd_gpio_pin_read(void *arg, int pin)
{
	struct ptcd_softc *sc = arg;
	uint8_t data;

	if (pin != PIN_READ)
		return 0;

	data = bus_space_read_1(sc->sc_iot, sc->sc_ioh, PTCD_READ);
	return data & PTCD_SENSE ? 0 : 1;
}

void
ptcd_gpio_pin_write(void *arg, int pin, int value)
{
	struct ptcd_softc *sc = arg;

	if (pin != PIN_WRITE)
		return;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, PTCD_WRITE,
	    value ? PTCD_OPEN : PTCD_CLOSE);
}

void
ptcd_gpio_pin_ctl(void *arg, int pin, int flags)
{
	/* We ignore pin control requests since the pin functions are fixed. */
}
