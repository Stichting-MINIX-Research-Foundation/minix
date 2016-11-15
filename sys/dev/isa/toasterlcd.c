/* $NetBSD: toasterlcd.c,v 1.11 2012/10/27 17:18:25 chs Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jesse Off.
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
__KERNEL_RCSID(0, "$NetBSD: toasterlcd.c,v 1.11 2012/10/27 17:18:25 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/select.h>

#include <sys/bus.h>
#include <machine/autoconf.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_callbacks.h>

#include <dev/ic/hd44780reg.h>
#include <dev/ic/hd44780var.h>
#include <dev/isa/tsdiovar.h>
#include <dev/isa/tsdioreg.h>

struct toasterlcd_softc {
	device_t sc_dev;
	struct hd44780_chip sc_hlcd;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_gpioh;
};

static int	toasterlcd_match(device_t, cfdata_t, void *);
static void	toasterlcd_attach(device_t, device_t, void *);

static void	toasterlcd_writereg(struct hd44780_chip *, u_int32_t, u_int32_t, u_int8_t);
static u_int8_t	toasterlcd_readreg(struct hd44780_chip *, u_int32_t, u_int32_t);

extern const struct wsdisplay_emulops hlcd_emulops;
extern const struct wsdisplay_accessops hlcd_accessops;
extern struct cfdriver toasterlcd_cd;

CFATTACH_DECL_NEW(toasterlcd, sizeof(struct toasterlcd_softc),
    toasterlcd_match, toasterlcd_attach, NULL, NULL);

static const struct wsscreen_descr toasterlcd_stdscreen = {
	"std_toasterlcd", 40, 4,
	&hlcd_emulops,
	5, 7,
	0,
};

static const struct wsscreen_descr *_toasterlcd_scrlist[] = {
	&toasterlcd_stdscreen,
};

static const struct wsscreen_list toasterlcd_screenlist = {
	sizeof(_toasterlcd_scrlist) / sizeof(struct wsscreen_descr *),
	_toasterlcd_scrlist,
};

static int
toasterlcd_match(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

#define TSDIO_GET(x)	bus_space_read_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x))

#define TSDIO_SET(x, y)	bus_space_write_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x), (y))

#define TSDIO_SETBITS(x, y)	bus_space_write_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x), TSDIO_GET(x) | (y))

#define TSDIO_CLEARBITS(x, y)	bus_space_write_1(sc->sc_iot, sc->sc_gpioh, \
	(TSDIO_ ## x), TSDIO_GET(x) & (~(y)))

static void
toasterlcd_attach(device_t parent, device_t self, void *aux)
{
	struct toasterlcd_softc *sc = device_private(self);
	struct tsdio_attach_args *taa = aux;
	struct wsemuldisplaydev_attach_args waa;

	sc->sc_iot = taa->ta_iot;
	sc->sc_gpioh = taa->ta_ioh;

	sc->sc_hlcd.sc_dev_ok = 1;
	sc->sc_hlcd.sc_cols = 40;
	sc->sc_hlcd.sc_vcols = 40;
	sc->sc_hlcd.sc_flags = HD_8BIT | HD_MULTILINE | HD_MULTICHIP;
	sc->sc_hlcd.sc_dev = self;

	sc->sc_hlcd.sc_writereg = toasterlcd_writereg;
	sc->sc_hlcd.sc_readreg = toasterlcd_readreg;
	
	TSDIO_SETBITS(DDR, 0x2);	/* Port B as outputs */
	TSDIO_CLEARBITS(DDR, 0x1);	/* Port C as inputs */
	TSDIO_CLEARBITS(PBDR, 0xd);	/* De-assert EN, De-assert RS */

	aprint_normal(": 4x40 text-mode hd44780 LCD\n");
	aprint_normal_dev(sc->sc_dev, "using port C, bits 0-7 as DB0-DB7\n");
	aprint_normal_dev(sc->sc_dev, "using port B, bits 0-3 as RS, WR, EN1, EN2\n");

	hd44780_attach_subr(&sc->sc_hlcd);

	waa.console = 0;
	waa.scrdata = &toasterlcd_screenlist;
	waa.accessops = &hlcd_accessops;
	waa.accesscookie = &sc->sc_hlcd.sc_screen;
	config_found(self, &waa, wsemuldisplaydevprint);
}

static void
toasterlcd_writereg(struct hd44780_chip *hd, u_int32_t en, u_int32_t rs, u_int8_t cmd)
{
	struct toasterlcd_softc *sc = device_private(hd->sc_dev);
	u_int8_t ctrl;

	if (hd->sc_dev_ok == 0)
		return;

	/* Step 1: Apply RS & WR, Send data */
	ctrl = TSDIO_GET(PBDR);
	TSDIO_SETBITS(DDR, 0x1); /* set port C to outputs */
	TSDIO_SET(PCDR, cmd);
	if (rs) {
		ctrl |= 0x1;	/* assert RS */
		ctrl &= ~0x2;	/* assert WR */
	} else {
		ctrl &= ~0x3;	/* assert WR, de-assert RS */
	}
	TSDIO_SET(PBDR, ctrl);

	/* Step 2: setup time delay */
	delay(1);

	/* Step 3: assert EN */
	if (en == 1) ctrl |= 0x8;
	else ctrl |= 0x4;
	TSDIO_SET(PBDR, ctrl);

	/* Step 4: pulse time delay */
	delay(1);

	/* Step 5: de-assert EN */
	if (en == 1) ctrl &= ~0x8;
	else ctrl &= ~0x4;
	TSDIO_SET(PBDR, ctrl);

	/* Step 6: hold time delay */
	delay(1);
	
	/* Step 7: de-assert WR */
	ctrl |= 0x2;
	TSDIO_SET(PBDR, ctrl); 

	/* Step 8: minimum delay till next bus-cycle */
	delay(1000);
}

static u_int8_t
toasterlcd_readreg(struct hd44780_chip *hd, u_int32_t en, u_int32_t rs)
{
	struct toasterlcd_softc *sc = device_private(hd->sc_dev);
	u_int8_t ret, ctrl;

	if (hd->sc_dev_ok == 0)
		return 0;

	/* Step 1: Apply RS & WR, Send data */
	ctrl = TSDIO_GET(PBDR);
	TSDIO_CLEARBITS(DDR, 0x1);	/* set port C to inputs */
	if (rs) {
		ctrl |= 0x3;	/* de-assert WR, assert RS */
	} else {
		ctrl |= 0x2;	/* de-assert WR */
		ctrl &= ~0x1;	/* de-assert RS */
	}
	TSDIO_SET(PBDR, ctrl);

	/* Step 2: setup time delay */
	delay(1);

	/* Step 3: assert EN */
	if (en == 1) ctrl |= 0x8;
	else ctrl |= 0x4;
	TSDIO_SET(PBDR, ctrl);

	/* Step 4: pulse time delay */
	delay(1);

	/* Step 5: de-assert EN */
	ret = TSDIO_GET(PCDR) & 0xff;
	if (en == 1) ctrl &= ~0x8;
	else ctrl &= ~0x4;
	TSDIO_SET(PBDR, ctrl);

	/* Step 6: hold time delay + min bus cycle interval*/
	delay(1000);
	return ret;
}

