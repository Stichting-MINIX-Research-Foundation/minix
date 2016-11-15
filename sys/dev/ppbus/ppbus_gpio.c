/* $NetBSD: ppbus_gpio.c,v 1.1 2008/04/29 14:07:37 cegger Exp $ */

/*
 *  Copyright (c) 2008, Hans Rosenfeld <rosenfeld@grumpf.hope-2000.org>
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ppbus_gpio.c,v 1.1 2008/04/29 14:07:37 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/kernel.h>

#include <dev/gpio/gpiovar.h>

#include <dev/ppbus/ppbus_conf.h>
#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_io.h>

static int  gpio_ppbus_open(void *, device_t);
static void gpio_ppbus_close(void *, device_t);
static int  gpio_ppbus_pin_read(void *, int);
static void gpio_ppbus_pin_write(void *, int, int);
static void gpio_ppbus_pin_ctl(void *, int, int);


#define PORT(r, b, i) { PPBUS_R##r##TR, PPBUS_W##r##TR, b, i}
static const struct {
	u_char rreg;
	u_char wreg;
	u_char bit;
	u_char inv;
} ppbus_port[PPBUS_NPINS] = {	/* parallel port wiring: */
	PORT(C, 0, 1),		/*     1: /C0  Output    */
	PORT(D, 0, 0),		/*     2:  D0  Output    */
	PORT(D, 1, 0),		/*     3:  D1  Output    */
	PORT(D, 2, 0),		/*     4:  D2  Output    */
	PORT(D, 3, 0),		/*     5:  D3  Output    */
	PORT(D, 4, 0),		/*     6:  D4  Output    */
	PORT(D, 5, 0),		/*     7:  D5  Output    */
	PORT(D, 6, 0),		/*     8:  D6  Output    */
	PORT(D, 7, 0),		/*     9:  D7  Output    */
	PORT(S, 6, 0),		/*    10:  S6  Input     */
	PORT(S, 7, 1),		/*    11: /S7  Input     */
	PORT(S, 5, 0),		/*    12:  S5  Input     */
	PORT(S, 4, 0),		/*    13:  S4  Input     */
	PORT(C, 1, 1),		/*    14: /C1  Output    */
	PORT(S, 3, 0),		/*    15:  S3  Input     */
	PORT(C, 2, 0),		/*    16:  C2  Output    */
	PORT(C, 3, 1),		/*    17: /C3  Output    */
};				/* 18-25: GND            */

void
gpio_ppbus_attach(struct ppbus_softc *sc)
{
	struct gpiobus_attach_args gba;
	gpio_pin_t *pin;
	int i;

	for (pin = &sc->sc_gpio_pins[0], i = 0; i < PPBUS_NPINS; pin++, i++) {
		pin->pin_num = i;
		
		if (((i >= 9) && (i <= 12)) || (i == 14)) {
			pin->pin_caps = GPIO_PIN_INPUT;
			pin->pin_flags = GPIO_PIN_INPUT;
			pin->pin_state = gpio_ppbus_pin_read(sc, i);
		} else {
			pin->pin_caps = GPIO_PIN_OUTPUT;
			pin->pin_flags = GPIO_PIN_OUTPUT;
			pin->pin_state = GPIO_PIN_LOW;
			gpio_ppbus_pin_write(sc, i, pin->pin_state);
		}

		gpio_ppbus_pin_ctl(sc, i, pin->pin_flags);
	}

	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_gc_open = gpio_ppbus_open;
	sc->sc_gpio_gc.gp_gc_close = gpio_ppbus_close;
	sc->sc_gpio_gc.gp_pin_read = gpio_ppbus_pin_read;
	sc->sc_gpio_gc.gp_pin_write = gpio_ppbus_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = gpio_ppbus_pin_ctl;

	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = PPBUS_NPINS;

	config_found_ia(sc->sc_dev, "gpiobus", &gba, gpiobus_print);
}

static int
gpio_ppbus_open(void *arg, device_t dev)
{
	struct ppbus_softc *sc = arg;

	return ppbus_request_bus(sc->sc_dev, dev, PPBUS_WAIT|PPBUS_INTR, (hz));
}

static void
gpio_ppbus_close(void *arg, device_t dev)
{
	struct ppbus_softc *sc = arg;

	(void) ppbus_release_bus(sc->sc_dev, dev, PPBUS_WAIT|PPBUS_INTR, (hz));
}

static int
gpio_ppbus_pin_read(void *arg, int pin)
{
	struct ppbus_softc *sc = arg;
	u_char port = ppbus_io(sc->sc_dev, ppbus_port[pin].rreg, NULL, 0, 0);

	return ((port >> ppbus_port[pin].bit) & 1) ^ ppbus_port[pin].inv;
}

static void
gpio_ppbus_pin_write(void *arg, int pin, int value)
{
	struct ppbus_softc *sc = arg;
	u_char port = ppbus_io(sc->sc_dev, ppbus_port[pin].rreg, NULL, 0, 0);

	value ^= ppbus_port[pin].inv;
	value <<= ppbus_port[pin].bit;
	port &= ~(1 << ppbus_port[pin].bit);
	port |= value;

	ppbus_io(sc->sc_dev, ppbus_port[pin].wreg, NULL, 0, port);
}

static void
gpio_ppbus_pin_ctl(void *arg, int pin, int flags)
{
	/* can't change parallel port pin configuration */
}
