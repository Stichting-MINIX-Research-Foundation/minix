/* $NetBSD: auvitek_board.c,v 1.3 2011/07/09 15:00:44 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * Auvitek AU0828 USB controller - board specific initialization
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvitek_board.c,v 1.3 2011/07/09 15:00:44 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/auvitekreg.h>
#include <dev/usb/auvitekvar.h>

static const struct auvitek_board_config {
	uint16_t	reset;
	uint16_t	enable;
	uint8_t		clkdiv;
} auvitek_board_config[] = {
	[AUVITEK_BOARD_HVR_850] = {
		.reset = 0x02b0,
		.enable = 0x02f0,
		.clkdiv = AU0828_I2C_CLKDIV_30,
	},
	[AUVITEK_BOARD_HVR_950Q] = {
		.reset = 0x02b0,
		.enable = 0x02f0,
		.clkdiv = AU0828_I2C_CLKDIV_30,
	},
};

void
auvitek_board_init(struct auvitek_softc *sc)
{
	uint16_t reset, enable;

	/* power up device */
	auvitek_write_1(sc, AU0828_REG_POWER_CTL, AU0828_POWER_EN);

	reset = auvitek_board_config[sc->sc_board].reset;
	enable = auvitek_board_config[sc->sc_board].enable;

	/* stash i2c clock divider in softc */
	sc->sc_i2c_clkdiv = auvitek_board_config[sc->sc_board].clkdiv;

	/* configure gpio, if requested */
	if (reset) {
		auvitek_write_1(sc, AU0828_REG_GPIO2_PINDIR, reset >> 8);
		auvitek_write_1(sc, AU0828_REG_GPIO1_PINDIR, reset & 0xff);
		auvitek_write_1(sc, AU0828_REG_GPIO2_OUTEN, 0);
		auvitek_write_1(sc, AU0828_REG_GPIO1_OUTEN, 0);
		delay(100000);
	}
	if (enable) {
		auvitek_write_1(sc, AU0828_REG_GPIO2_PINDIR, enable >> 8);
		auvitek_write_1(sc, AU0828_REG_GPIO1_PINDIR, enable & 0xff);
		auvitek_write_1(sc, AU0828_REG_GPIO2_OUTEN, enable >> 8);
		auvitek_write_1(sc, AU0828_REG_GPIO1_OUTEN, enable & 0xff);
		delay(250000);
	}
}

int
auvitek_board_tuner_reset(void *priv)
{
	struct auvitek_softc *sc = priv;
	uint8_t val;

	switch (sc->sc_board) {
	case AUVITEK_BOARD_HVR_850:
	case AUVITEK_BOARD_HVR_950Q:
		val = auvitek_read_1(sc, AU0828_REG_GPIO2_OUTEN);
		val &= ~2;
		auvitek_write_1(sc, AU0828_REG_GPIO2_OUTEN, val);
		delay(10000);
		val = auvitek_read_1(sc, AU0828_REG_GPIO2_OUTEN);
		val |= 2;
		auvitek_write_1(sc, AU0828_REG_GPIO2_OUTEN, val);
		delay(10000);
		break;
	}

	return 0;
}

unsigned int
auvitek_board_get_if_frequency(struct auvitek_softc *sc)
{
	return 6000000;	/* 6MHz */
}
