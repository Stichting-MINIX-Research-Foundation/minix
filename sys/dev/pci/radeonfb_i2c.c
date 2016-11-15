/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Garrett D'Amore for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

/* 
 * ATI Technologies Inc. ("ATI") has not assisted in the creation of, and
 * does not endorse, this software.  ATI will not be responsible or liable
 * for any actual or alleged damage or loss caused by or in connection with
 * the use of or reliance on this software.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: radeonfb_i2c.c,v 1.2 2007/10/19 12:00:55 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>
#include <dev/i2c/ddcvar.h>

#include <dev/pci/radeonfbreg.h>
#include <dev/pci/radeonfbvar.h>

/* i2c support */
static int radeonfb_i2c_acquire_bus(void *, int);
static void radeonfb_i2c_release_bus(void *, int);
static int radeonfb_i2c_send_start(void *, int);
static int radeonfb_i2c_send_stop(void *, int);
static int radeonfb_i2c_initiate_xfer(void *, i2c_addr_t, int);
static int radeonfb_i2c_read_byte(void *, uint8_t *, int);
static int radeonfb_i2c_write_byte(void *, uint8_t, int);

/* i2c bit-bang glue */
static void radeonfb_i2cbb_set_bits(void *, uint32_t);
static void radeonfb_i2cbb_set_dir(void *, uint32_t);
static uint32_t radeonfb_i2cbb_read(void *);

/*
 * I2C bit-bang operations
 */
void
radeonfb_i2cbb_set_bits(void *cookie, uint32_t bits)
{
	struct radeonfb_i2c	*ric = (struct radeonfb_i2c *)cookie;
	struct radeonfb_softc	*sc = ric->ric_softc;

	PATCH32(sc, ric->ric_register, bits,
	    ~(RADEON_GPIO_A_0 | RADEON_GPIO_A_1));
}

void
radeonfb_i2cbb_set_dir(void *cookie, uint32_t bits)
{
	struct radeonfb_i2c	*ric = (struct radeonfb_i2c *)cookie;
	struct radeonfb_softc	*sc = ric->ric_softc;

	PATCH32(sc, ric->ric_register, bits, ~(RADEON_GPIO_EN_0));
}

uint32_t
radeonfb_i2cbb_read(void *cookie)
{
	struct radeonfb_i2c	*ric = (struct radeonfb_i2c *)cookie;
	struct radeonfb_softc	*sc = ric->ric_softc;

	/* output bit is 0 shifted, input bit is shifted 8 */
	return (GET32(sc, ric->ric_register) >> RADEON_GPIO_Y_SHIFT_0);
}

static const struct i2c_bitbang_ops radeonfb_i2cbb_ops = {
	radeonfb_i2cbb_set_bits,
	radeonfb_i2cbb_set_dir,
	radeonfb_i2cbb_read,
	{
		RADEON_GPIO_A_0,	/* SDA */
		RADEON_GPIO_A_1,	/* SCL */
		RADEON_GPIO_EN_0,	/* SDA output */
		0,			/* SDA input */
	}
};

/*
 * I2C support
 */
int
radeonfb_i2c_acquire_bus(void *cookie, int flags)
{
	struct radeonfb_i2c	*ric = (struct radeonfb_i2c *)cookie;
	struct radeonfb_softc	*sc = ric->ric_softc;
	int			i;

	/*
	 * Some hardware seems to have hardware/software combined access
	 * to the DVI I2C.  We want to use software.
	 */
	if (ric->ric_register == RADEON_GPIO_DVI_DDC) {

		/* ask for software access to I2C bus */
		SET32(sc, ric->ric_register, RADEON_GPIO_SW_USE);

		/*
		 * wait for the chip to give up access.  we don't make
		 * this a hard timeout, because some hardware might
		 * not implement this negotiation protocol
		 */
		for (i = RADEON_TIMEOUT; i; i--) {
			if (GET32(sc, ric->ric_register) & RADEON_GPIO_SW_USE)
				break;
		}
	}

	/* enable the I2C clock */
	SET32(sc, ric->ric_register, RADEON_GPIO_EN_1);

	return 0;
}

void
radeonfb_i2c_release_bus(void *cookie, int flags)
{
	struct radeonfb_i2c	*ric = (struct radeonfb_i2c *)cookie;
	struct radeonfb_softc	*sc = ric->ric_softc;

	if (ric->ric_register == RADEON_GPIO_DVI_DDC) {
		/* we no longer "want" I2C, and we're "done" with it */
		CLR32(sc, ric->ric_register, RADEON_GPIO_SW_USE);
		SET32(sc, ric->ric_register, RADEON_GPIO_SW_DONE);
	}
}

int
radeonfb_i2c_send_start(void *cookie, int flags)
{

	return i2c_bitbang_send_start(cookie, flags, &radeonfb_i2cbb_ops);
}

int
radeonfb_i2c_send_stop(void *cookie, int flags)
{

	return i2c_bitbang_send_stop(cookie, flags, &radeonfb_i2cbb_ops);
}

int
radeonfb_i2c_initiate_xfer(void *cookie, i2c_addr_t addr, int flags)
{

	return i2c_bitbang_initiate_xfer(cookie, addr, flags,
	    &radeonfb_i2cbb_ops);
}

int
radeonfb_i2c_read_byte(void *cookie, uint8_t *valp, int flags)
{

	return i2c_bitbang_read_byte(cookie, valp, flags, &radeonfb_i2cbb_ops);
}

int
radeonfb_i2c_write_byte(void *cookie, uint8_t val, int flags)
{

	return i2c_bitbang_write_byte(cookie, val, flags, &radeonfb_i2cbb_ops);
}

void
radeonfb_i2c_init(struct radeonfb_softc *sc)
{
	int	i;

	for (i = 0; i < 4; i++) {
		struct i2c_controller	*icc = &sc->sc_i2c[i].ric_controller;

		sc->sc_i2c[i].ric_softc = sc;
		icc->ic_cookie = &sc->sc_i2c[i];
		icc->ic_acquire_bus = radeonfb_i2c_acquire_bus;
		icc->ic_release_bus = radeonfb_i2c_release_bus;
		icc->ic_send_start = radeonfb_i2c_send_start;
		icc->ic_send_stop = radeonfb_i2c_send_stop;
		icc->ic_initiate_xfer = radeonfb_i2c_initiate_xfer;
		icc->ic_read_byte = radeonfb_i2c_read_byte;
		icc->ic_write_byte = radeonfb_i2c_write_byte;
	}

	/* index == ddctype (RADEON_DDC_XX) - 1 */ 
	sc->sc_i2c[0].ric_register = RADEON_GPIO_MONID;
	sc->sc_i2c[1].ric_register = RADEON_GPIO_DVI_DDC;
	sc->sc_i2c[2].ric_register = RADEON_GPIO_VGA_DDC;
	sc->sc_i2c[3].ric_register = RADEON_GPIO_CRT2_DDC;
}

int
radeonfb_i2c_read_edid(struct radeonfb_softc *sc, int ddctype, uint8_t *data)
{

	if ((ddctype < 1) || (ddctype > 4))
		return EINVAL;

	ddctype--;
	return (ddc_read_edid(&sc->sc_i2c[ddctype].ric_controller, data, 128));
}
