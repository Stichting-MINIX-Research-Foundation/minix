/* $NetBSD: mt2131.c,v 1.5 2015/03/07 14:16:51 jmcneill Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mt2131.c,v 1.5 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/module.h>

#include <dev/i2c/mt2131var.h>

#define PWR 0x07
#define UPC_1 0x0b
#define AGC_RL 0x10
#define MISC_2 0x15

#define IF1 1220
#define IF2 44000
#define REF 16000

static const uint8_t mt2131_initstring[] = {
	0x01,
	0x50, 0x00, 0x50, 0x80, 0x00, 0x49,
	0xfa, 0x88, 0x08, 0x77, 0x41, 0x04, 0x00, 0x00, 0x00, 0x32,
	0x7f, 0xda, 0x4c, 0x00, 0x10, 0xaa, 0x78, 0x80, 0xff, 0x68,
	0xa0, 0xff, 0xdd, 0x00, 0x00
};

static const uint8_t mt2131_agcinitstring[] = {
        AGC_RL,
        0x7f, 0xc8, 0x0a, 0x5f, 0x00, 0x04
};


struct mt2131_softc {
	device_t		parent;
	i2c_tag_t		tag;
	i2c_addr_t		addr;
	uint32_t		frequency;
	uint32_t		bandwidth;
};

static int mt2131_init(struct mt2131_softc *);

static int mt2131_read(struct mt2131_softc *, uint8_t, uint8_t *);
static int mt2131_write(struct mt2131_softc *, uint8_t, uint8_t);

struct mt2131_softc *
mt2131_open(device_t parent, i2c_tag_t t, i2c_addr_t a)
{
	struct mt2131_softc *sc;
	int ret;
	uint8_t cmd, reg;

	cmd = reg = 0;

	/* get id reg */
	iic_acquire_bus(t, I2C_F_POLL);
	ret = iic_exec(t, I2C_OP_READ_WITH_STOP, a, &cmd, 1, &reg, 1, I2C_F_POLL);
	iic_release_bus(t, I2C_F_POLL);

	if (ret) {
		device_printf(parent, "%s(): read fail\n", __func__);
		return NULL;
	}

	if ((reg & 0xfe) != 0x3e) {
		device_printf(parent, "%s(): chip id %02x unknown\n",
		    __func__, reg);
		return NULL;
	}

	sc = kmem_alloc(sizeof(*sc), KM_SLEEP);
	if (sc == NULL)
                return NULL;

	sc->parent = parent;
	sc->tag = t;
	sc->addr = a;

	mt2131_init(sc);

	return sc;
}

void
mt2131_close(struct mt2131_softc *sc)
{
	kmem_free(sc, sizeof(*sc));
}

int
mt2131_tune_dtv(struct mt2131_softc *sc, const struct dvb_frontend_parameters *p)
{
	int rv, i;
	uint64_t o1, o2;
	uint64_t d1, d2;
	uint32_t r1, r2;
	uint32_t fr;
	uint8_t b[7];
	uint8_t regval;

	mt2131_init(sc);

	b[0] = 0x01;

	if(p->frequency != 0 &&
		(p->frequency < 50000000 || p->frequency > 1000000000))
		return EINVAL;

	fr = p->frequency / 1000;

	o1 = fr + IF1 * 1000;
	o2 = o1 - fr - IF2;

	d1 = (o1 * 8192)/REF;
	d2 = (o2 * 8192)/REF;

	r1 = d1/8192;
	r2 = d2/8192;

	b[1] = (d1 & 0x1fe0) >> 5;
	b[2] = (d1 & 0x001f);
	b[3] = r1;
	b[4] = (d2 & 0x1fe0) >> 5;
	b[5] = (d2 & 0x001f);
	b[6] = r2;

	iic_acquire_bus(sc->tag, I2C_F_POLL);
	rv = iic_exec(sc->tag, I2C_OP_WRITE_WITH_STOP, sc->addr, b, 7, NULL, 0, I2C_F_POLL);
	iic_release_bus(sc->tag, I2C_F_POLL);

	regval = (fr - 27501) / 55000;

	if(regval > 0x13)
		regval = 0x13;

	rv = mt2131_write(sc, UPC_1, regval);

	if (rv != 0)
		device_printf(sc->parent, "%s write failed\n", __func__);

	sc->frequency = (o1 - o2 - IF2) * 1000;

	for (i = 0; i < 100; i++) {
		kpause("mt2131", true, 1, NULL);

		rv = mt2131_read(sc, 0x08, &regval);
		if (rv != 0)
			device_printf(sc->parent, "%s read failed\n", __func__);

		if (( regval & 0x88 ) == 0x88 ) {
			return 0;
		}
	}

	device_printf(sc->parent, "mt2131 not locked, %02x\n", b[1]);

	return rv;
}

static int
mt2131_init(struct mt2131_softc *sc)
{
	int ret;

	ret = iic_acquire_bus(sc->tag, I2C_F_POLL);
	if (ret)
		return -1;
	ret = iic_exec(sc->tag, I2C_OP_WRITE_WITH_STOP, sc->addr,
	    mt2131_initstring, sizeof(mt2131_initstring), NULL, 0, I2C_F_POLL);
	if (ret)
		return -1;
	iic_release_bus(sc->tag, I2C_F_POLL);

	ret = mt2131_write(sc, UPC_1, 0x09);
	ret = mt2131_write(sc, MISC_2, 0x47);
	ret = mt2131_write(sc, PWR, 0xf2);
	ret = mt2131_write(sc, UPC_1, 0x01);

	ret = iic_acquire_bus(sc->tag, I2C_F_POLL);
	if (ret)
		return -1;
	ret = iic_exec(sc->tag, I2C_OP_WRITE_WITH_STOP, sc->addr,
	    mt2131_agcinitstring, sizeof(mt2131_agcinitstring),
	    NULL, 0, I2C_F_POLL);
	iic_release_bus(sc->tag, I2C_F_POLL);
	if (ret)
		return -1;
	
	return 0;
}

static int
mt2131_read(struct mt2131_softc *sc, uint8_t r, uint8_t *v)
{
	int ret;

	ret = iic_acquire_bus(sc->tag, I2C_F_POLL);
	if (ret)
		return ret;
	ret = iic_exec(sc->tag, I2C_OP_READ_WITH_STOP, sc->addr,
	    &r, 1, v, 1, I2C_F_POLL);

	iic_release_bus(sc->tag, I2C_F_POLL);

	return ret;
}

static int
mt2131_write(struct mt2131_softc *sc, uint8_t a, uint8_t v)
{
	int ret;
	uint8_t b[] = { a, v };

	ret = iic_acquire_bus(sc->tag, I2C_F_POLL);
	if (ret)
		return ret;

	ret = iic_exec(sc->tag, I2C_OP_READ_WITH_STOP, sc->addr,
	    b, sizeof(b), NULL, 0, I2C_F_POLL);

	iic_release_bus(sc->tag, I2C_F_POLL);

	return ret;
}

MODULE(MODULE_CLASS_DRIVER, mt2131, "i2cexec");

static int
mt2131_modcmd(modcmd_t cmd, void *priv)
{
	if (cmd == MODULE_CMD_INIT || cmd == MODULE_CMD_FINI)
		return 0;
	return ENOTTY;
}
