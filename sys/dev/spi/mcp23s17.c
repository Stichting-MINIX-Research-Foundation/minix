/*      $NetBSD: mcp23s17.c,v 1.1 2014/04/06 17:59:39 kardel Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank Kardel.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mcp23s17.c,v 1.1 2014/04/06 17:59:39 kardel Exp $");

/* 
 * Driver for Microchip MCP23S17 GPIO
 *
 * see: http://ww1.microchip.com/downloads/en/DeviceDoc/21952b.pdf
 */

#include "gpio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/gpio.h>
#include <sys/sysctl.h>

#include <dev/gpio/gpiovar.h>

#include <dev/spi/spivar.h>

#include <dev/spi/mcp23s17.h>

/* #define MCP23S17_DEBUG */
#ifdef MCP23S17_DEBUG
int mcp23S17debug = 3;
#define DPRINTF(l, x)	do { if (l <= mcp23S17debug) { printf x; } } while (0)
#else
#define DPRINTF(l, x)
#endif

struct mcp23s17gpio_softc {
	device_t                sc_dev;
	struct spi_handle      *sc_sh; 
	uint8_t                 sc_ha;   /* hardware address */
	uint8_t                 sc_bank; /* addressing scheme */
	struct gpio_chipset_tag	sc_gpio_gc;
	gpio_pin_t		sc_gpio_pins[MCP23x17_GPIO_NPINS];
};

static int	mcp23s17gpio_match(device_t, cfdata_t, void *);
static void	mcp23s17gpio_attach(device_t, device_t, void *);

static void     mcp23s17gpio_write(struct mcp23s17gpio_softc *, uint8_t, uint8_t);

#if NGPIO > 0
static uint8_t  mcp23s17gpio_read(struct mcp23s17gpio_softc *, uint8_t);

static int      mcp23s17gpio_gpio_pin_read(void *, int);
static void     mcp23s17gpio_gpio_pin_write(void *, int, int);
static void     mcp23s17gpio_gpio_pin_ctl(void *, int, int);
#endif

CFATTACH_DECL_NEW(mcp23s17gpio, sizeof(struct mcp23s17gpio_softc),
		  mcp23s17gpio_match, mcp23s17gpio_attach, NULL, NULL);

static int
mcp23s17gpio_match(device_t parent, cfdata_t cf, void *aux)
{
	struct spi_attach_args *sa = aux;

	/* MCP23S17 has no way to detect it! */

	/* run at 10MHz */
	if (spi_configure(sa->sa_handle, SPI_MODE_0, 10000000))
		return 0;

	return 1;
}

static void
mcp23s17gpio_attach(device_t parent, device_t self, void *aux)
{
	struct mcp23s17gpio_softc *sc;
	struct spi_attach_args *sa;
#if NGPIO > 0
	int i;
	struct gpiobus_attach_args gba;
#endif

	sa = aux;
	sc = device_private(self);
	sc->sc_dev = self;
	sc->sc_sh = sa->sa_handle;
	sc->sc_bank = 0;
	sc->sc_ha = device_cfdata(sc->sc_dev)->cf_flags & 0x7;

	aprint_naive(": GPIO\n");	
	aprint_normal(": MCP23S17 GPIO (ha=%d)\n", sc->sc_ha);

	DPRINTF(1, ("%s: initialize (HAEN|SEQOP)\n", device_xname(sc->sc_dev)));

	/* basic setup */
	mcp23s17gpio_write(sc, MCP23x17_IOCONA(sc->sc_bank), MCP23x17_IOCON_HAEN|MCP23x17_IOCON_SEQOP);

#if NGPIO > 0
	for (i = 0; i < MCP23x17_GPIO_NPINS; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT |
			GPIO_PIN_OUTPUT |
			GPIO_PIN_PUSHPULL | GPIO_PIN_TRISTATE |
			GPIO_PIN_PULLUP |
			GPIO_PIN_INVIN;
		
		/* read initial state */
		sc->sc_gpio_pins[i].pin_state =
			mcp23s17gpio_gpio_pin_read(sc, i);
	}

	/* create controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = mcp23s17gpio_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = mcp23s17gpio_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = mcp23s17gpio_gpio_pin_ctl;
	
	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = MCP23x17_GPIO_NPINS;

	config_found_ia(self, "gpiobus", &gba, gpiobus_print);
#else
	aprint_normal_dev(sc->sc_dev, "no GPIO configured in kernel");
#endif
}

#if NGPIO > 0
static uint8_t
mcp23s17gpio_read(struct mcp23s17gpio_softc *sc, uint8_t addr)
{
	uint8_t buf[2];
	uint8_t val = 0;
	int rc;

	buf[0] = MCP23x17_OP_READ(sc->sc_ha);
	buf[1] = addr;

	rc = spi_send_recv(sc->sc_sh, 2, buf, 1, &val);
	if (rc != 0)
	{
		aprint_normal_dev(sc->sc_dev, "SPI send_recv failed rc=%d\n", rc); 
	}

	DPRINTF(3, ("%s: read(0x%02x) @0x%02x->0x%02x\n", device_xname(sc->sc_dev), buf[0], addr, val));
	return val;
}
#endif

static void
mcp23s17gpio_write(struct mcp23s17gpio_softc *sc, uint8_t addr, uint8_t val)
{
	uint8_t buf[3];
	int rc;

	buf[0] = MCP23x17_OP_WRITE(sc->sc_ha);
	buf[1] = addr;
	buf[2] = val;

	rc = spi_send(sc->sc_sh, 3, buf);
	if (rc != 0)
	{
		aprint_normal_dev(sc->sc_dev, "SPI send failed rc=%d\n", rc); 
	}
	DPRINTF(3, ("%s: write(0x%02x) @0x%02x<-0x%02x\n", device_xname(sc->sc_dev), buf[0], addr, val));
}

#if NGPIO > 0
/* GPIO support functions */
static int
mcp23s17gpio_gpio_pin_read(void *arg, int pin)
{
	struct mcp23s17gpio_softc *sc = arg;
	int epin = pin & MCP23x17_GPIO_NPINS_MASK;
	uint8_t data;
	uint8_t addr;
	int val;

	if (epin < 8) {
		addr = MCP23x17_GPIOA(sc->sc_bank);
	} else {
		addr = MCP23x17_GPIOB(sc->sc_bank);
		epin = pin - 8;
	}

	data = mcp23s17gpio_read(sc, addr);

	val = data & (1 << epin) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;

	DPRINTF(2, ("%s: gpio_read pin %d->%d\n", device_xname(sc->sc_dev), pin, (val == GPIO_PIN_HIGH)));
	
	return val;
}

static void
mcp23s17gpio_gpio_pin_write(void *arg, int pin, int value)
{
	struct mcp23s17gpio_softc *sc = arg;
	int epin = pin & MCP23x17_GPIO_NPINS_MASK;
	uint8_t data;
	uint8_t addr;

	if (epin < 8) {
		addr = MCP23x17_OLATA(sc->sc_bank);
	} else {
		addr = MCP23x17_OLATB(sc->sc_bank);
		epin = pin - 8;
	}

	data = mcp23s17gpio_read(sc, addr);

	if (value == GPIO_PIN_HIGH) {
		data |= 1 << epin;
	} else {
		data &= ~(1 << epin);
	}

	mcp23s17gpio_write(sc, addr, data);

	DPRINTF(2, ("%s: gpio_write pin %d<-%d\n", device_xname(sc->sc_dev), pin, (value == GPIO_PIN_HIGH)));
}

static void
mcp23s17gpio_gpio_pin_ctl(void *arg, int pin, int flags)
{
	struct mcp23s17gpio_softc *sc = arg;
	int epin = pin & MCP23x17_GPIO_NPINS_MASK;
	uint8_t bit;
	uint8_t port;
	uint8_t data;

	if (epin < 8) {
		port = 0;
	} else {
		port = 1;
		epin = pin - 8;
	}

	bit = 1 << epin;

	DPRINTF(2, ("%s: gpio_ctl pin %d flags 0x%x\n", device_xname(sc->sc_dev), pin, flags));

	if (flags & (GPIO_PIN_OUTPUT|GPIO_PIN_INPUT)) {
		data = mcp23s17gpio_read(sc, MCP23x17_IODIR(sc->sc_bank, port));
		if ((flags & GPIO_PIN_INPUT) || !(flags & GPIO_PIN_OUTPUT)) {
			/* for safety INPUT will overide output */
			data |= bit;
		} else {
			data &= ~bit;
		}
		mcp23s17gpio_write(sc, MCP23x17_IODIR(sc->sc_bank, port), data);
	}

	data = mcp23s17gpio_read(sc, MCP23x17_IPOL(sc->sc_bank, port));
	if (flags & GPIO_PIN_INVIN) {
		data |= bit;
	} else {
		data &= ~bit;
	}
	mcp23s17gpio_write(sc, MCP23x17_IPOL(sc->sc_bank, port), data);

	data = mcp23s17gpio_read(sc, MCP23x17_GPPU(sc->sc_bank, port));
	if (flags & GPIO_PIN_PULLUP) {
		data |= bit;
	} else {
		data &= ~bit;
	}
	mcp23s17gpio_write(sc, MCP23x17_GPPU(sc->sc_bank, port), data);
} 
#endif /* NGPIO > 0 */
