/* $NetBSD: gcscpcibvar.h,v 1.2 2011/08/29 18:34:42 bouyer Exp $ */
/* $OpenBSD: gcscpcib.c,v 1.6 2007/11/17 17:02:47 mbalmer Exp $	*/

/*
 * Copyright (c) 2008 Yojiro UO <yuo@nui.org>
 * Copyright (c) 2007 Marc Balmer <mbalmer@openbsd.org>
 * Copyright (c) 2007 Michael Shalayeff
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

struct gcscpcib_softc {
	struct timecounter	sc_timecounter;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	/* Watchdog Timer */
	struct sysmon_wdog	sc_smw;
	int			sc_wdt_mfgpt;

	/* GPIO interface */
	bus_space_tag_t		sc_gpio_iot;
	bus_space_handle_t	sc_gpio_ioh;
	struct gpio_chipset_tag	sc_gpio_gc;
	gpio_pin_t		sc_gpio_pins[AMD553X_GPIO_NPINS];

#if 0
	/* SMbus/i2c interface */ 
	bus_space_tag_t		sc_smbus_iot;
        bus_space_handle_t	sc_smbus_ioh;
	i2c_addr_t		sc_smbus_slaveaddr; /* address of smbus slave */
	struct i2c_controller	sc_i2c;		/* i2c controller info */
	krwlock_t		sc_smbus_rwlock;
#endif
};

void gcscpcib_attach(device_t, struct gcscpcib_softc *, bus_space_tag_t, int);
#define GCSCATTACH_NO_WDT	0x0001 /* do not attach watchdog */

uint64_t gcsc_rdmsr(uint);
void     gcsc_wrmsr(uint, uint64_t);
