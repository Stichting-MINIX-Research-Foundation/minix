/*	from $OpenBSD$ */

/*
 * ported to NetBSD by Frank Kardel
 * (http://permalink.gmane.org/gmane.os.openbsd.tech/31317)
 *
 * Copyright (c) 2013 Matt Dainty <matt <at> bodgit-n-scarper.com>
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

/*
 * Soekris net6501 GPIO and LEDs as implemented by the onboard Xilinx FPGA 
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: soekrisgpio.c,v 1.2 2013/06/10 07:14:02 kardel Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/isa/isavar.h>

#include <sys/gpio.h>
#include <dev/gpio/gpiovar.h>

#include "gpio.h"

#define	SOEKRIS_BASE		0x680	/* Base address of FPGA I/O */
#define	SOEKRIS_IOSIZE		32	/* I/O region size */

#define	SOEKRIS_NPINS		16	/* Number of Pins */
#define	SOEKRIS_GPIO_INPUT	0x000	/* Current state of pins */
#define	SOEKRIS_GPIO_OUTPUT	0x004	/* Set state of output pins */
#define	SOEKRIS_GPIO_RESET	0x008	/* Reset output pins */
#define	SOEKRIS_GPIO_SET	0x00c	/* Set output pins */
#define	SOEKRIS_GPIO_DIR	0x010	/* Direction, set for output */

#define	SOEKRIS_NLEDS		2	/* Number of LEDs */
#define	SOEKRIS_LED_ERROR	0x01c	/* Offset to error LED */
#define	SOEKRIS_LED_READY	0x01d	/* Offset to ready LED */

const u_int soekris_led_offset[SOEKRIS_NLEDS] = {
	SOEKRIS_LED_ERROR, SOEKRIS_LED_READY
};

struct soekris_softc {
	device_t		 sc_dev;

	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;

	struct gpio_chipset_tag	 sc_gpio_gc;
	gpio_pin_t		 sc_gpio_pins[SOEKRIS_NPINS];

	/* Fake GPIO device for the LEDs */
	struct gpio_chipset_tag	 sc_led_gc;
	gpio_pin_t		 sc_led_pins[SOEKRIS_NLEDS];
};

static int	 soekris_match(device_t, cfdata_t , void *);
static void	 soekris_attach(device_t, device_t, void *);
static int	 soekris_detach(device_t, int);
static int	 soekris_gpio_read(void *, int);
static void	 soekris_gpio_write(void *, int, int);
static void	 soekris_gpio_ctl(void *, int, int);
static int	 soekris_led_read(void *, int);
static void	 soekris_led_write(void *, int, int);
static void	 soekris_led_ctl(void *, int, int);

CFATTACH_DECL_NEW(soekrisgpio,
		  sizeof(struct soekris_softc),
		  soekris_match,
		  soekris_attach,
		  soekris_detach,
		  NULL);

static int
soekris_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	int iobase;

	if (ia->ia_nio < 1)
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	iobase = ia->ia_io[0].ir_addr;

	/* Need some sort of heuristic to match the Soekris net6501 */

	if (iobase != SOEKRIS_BASE || bus_space_map(ia->ia_iot,
	    iobase, SOEKRIS_IOSIZE, 0, &ioh) != 0)
		return (0);

	bus_space_unmap(ia->ia_iot, ioh, SOEKRIS_IOSIZE);

	ia->ia_io[0].ir_size = SOEKRIS_IOSIZE;

	return (1);
}

static void
soekris_attach(device_t parent, device_t self, void *aux)
{
	struct soekris_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	struct gpiobus_attach_args gba1, gba2;
	u_int data;
	int i;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, ia->ia_io[0].ir_size, 0,
	    &sc->sc_ioh) != 0) {
		aprint_normal(": can't map i/o space\n");
		return;
	}

	aprint_normal("\n");

	sc->sc_dev = self;
	sc->sc_iot = ia->ia_iot;

	data = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SOEKRIS_GPIO_DIR);

	for (i = 0; i < SOEKRIS_NPINS; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT |
		    GPIO_PIN_OUTPUT;
		sc->sc_gpio_pins[i].pin_flags = (data & (1 << i)) ?
		    GPIO_PIN_OUTPUT : GPIO_PIN_INPUT;
		sc->sc_gpio_pins[i].pin_state = soekris_gpio_read(sc, i);
	}

	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = soekris_gpio_read;
	sc->sc_gpio_gc.gp_pin_write = soekris_gpio_write;
	sc->sc_gpio_gc.gp_pin_ctl = soekris_gpio_ctl;

	gba1.gba_gc = &sc->sc_gpio_gc;
	gba1.gba_pins = sc->sc_gpio_pins;
	gba1.gba_npins = SOEKRIS_NPINS;

	for (i = 0; i < SOEKRIS_NLEDS; i++) {
		sc->sc_led_pins[i].pin_num = i;
		sc->sc_led_pins[i].pin_caps = GPIO_PIN_OUTPUT;
		sc->sc_led_pins[i].pin_flags = GPIO_PIN_OUTPUT;
		sc->sc_led_pins[i].pin_state = soekris_led_read(sc, i);
	}

	sc->sc_led_gc.gp_cookie = sc;
	sc->sc_led_gc.gp_pin_read = soekris_led_read;
	sc->sc_led_gc.gp_pin_write = soekris_led_write;
	sc->sc_led_gc.gp_pin_ctl = soekris_led_ctl;

	gba2.gba_gc = &sc->sc_led_gc;
	gba2.gba_pins = sc->sc_led_pins;
	gba2.gba_npins = SOEKRIS_NLEDS;

#if NGPIO > 0
	(void)config_found(sc->sc_dev, &gba1, gpiobus_print);
	(void)config_found(sc->sc_dev, &gba2, gpiobus_print);
#endif
}

static int
soekris_detach(device_t self, int flags)
{
	struct soekris_softc *sc = device_private(self);

	bus_space_unmap(sc->sc_iot, sc->sc_ioh, SOEKRIS_IOSIZE);
	return 0;
}

static int
soekris_gpio_read(void *arg, int pin)
{
	struct soekris_softc *sc = arg;
	u_int16_t data;

	data = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SOEKRIS_GPIO_INPUT);

	return (data & (1 << pin)) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
}

static void
soekris_gpio_write(void *arg, int pin, int value)
{
	struct soekris_softc *sc = arg;
	u_int16_t data;

	data = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SOEKRIS_GPIO_INPUT);

	if (value == GPIO_PIN_LOW)
		data &= ~(1 << pin);
	else if (value == GPIO_PIN_HIGH)
		data |= (1 << pin);

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, SOEKRIS_GPIO_OUTPUT, data);
}

static void
soekris_gpio_ctl(void *arg, int pin, int flags)
{
	struct soekris_softc *sc = arg;
	u_int16_t data;

	data = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SOEKRIS_GPIO_DIR);

	if (flags & GPIO_PIN_INPUT)
		data &= ~(1 << pin);
	if (flags & GPIO_PIN_OUTPUT)
		data |= (1 << pin);

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, SOEKRIS_GPIO_DIR, data);
}

static int
soekris_led_read(void *arg, int pin)
{
	struct soekris_softc *sc = arg;
	u_int8_t value;

	value = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    soekris_led_offset[pin]);

	return (value & 0x1) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
}

static void
soekris_led_write(void *arg, int pin, int value)
{
	struct soekris_softc *sc = arg;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, soekris_led_offset[pin],
	    value);
}

static void
soekris_led_ctl(void *arg, int pin, int flags)
{
}
