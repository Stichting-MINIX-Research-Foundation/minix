/* $NetBSD: cx24227.c,v 1.7 2015/03/07 14:16:51 jmcneill Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: cx24227.c,v 1.7 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/module.h>

#include <dev/i2c/cx24227var.h>

/* #define CX24227_DEBUG */

struct cx24227 {
	device_t        parent;
	i2c_tag_t       tag;
	i2c_addr_t      addr;
};

static int cx24227_writereg(struct cx24227 *, uint8_t, uint16_t);
static int cx24227_readreg(struct cx24227 *, uint8_t, uint16_t *);

static int cx24227_init(struct cx24227 *);

static struct documentation_wanted {
	uint8_t		r;
	uint16_t	v;
} documentation_wanted[] = {
	{ 0x00, 0x0071, },
	{ 0x01, 0x3213, },
	{ 0x09, 0x0025, },
	{ 0x1c, 0x001d, },
	{ 0x1f, 0x002d, },
	{ 0x20, 0x001d, },
	{ 0x22, 0x0022, },
	{ 0x23, 0x0020, },
	{ 0x29, 0x110f, },
	{ 0x2a, 0x10b4, },
	{ 0x2b, 0x10ae, },
	{ 0x2c, 0x0031, },
	{ 0x31, 0x010d, },
	{ 0x32, 0x0100, },
	{ 0x44, 0x0510, },
	{ 0x54, 0x0104, },
	{ 0x58, 0x2222, },
	{ 0x59, 0x1162, },
	{ 0x5a, 0x3211, },
	{ 0x5d, 0x0370, },
	{ 0x5e, 0x0296, },
	{ 0x61, 0x0010, },
	{ 0x63, 0x4a00, },
	{ 0x65, 0x0800, },
	{ 0x71, 0x0003, },
	{ 0x72, 0x0470, },
	{ 0x81, 0x0002, },
	{ 0x82, 0x0600, },
	{ 0x86, 0x0002, },
	{ 0x8a, 0x2c38, },
	{ 0x8b, 0x2a37, },
	{ 0x92, 0x302f, },
	{ 0x93, 0x3332, },
	{ 0x96, 0x000c, },
	{ 0x99, 0x0101, },
	{ 0x9c, 0x2e37, },
	{ 0x9d, 0x2c37, },
	{ 0x9e, 0x2c37, },
	{ 0xab, 0x0100, },
	{ 0xac, 0x1003, },
	{ 0xad, 0x103f, },
	{ 0xe2, 0x0100, },
	{ 0xe3, 0x1000, },
	{ 0x28, 0x1010, },
	{ 0xb1, 0x000e, },
};


static int
cx24227_writereg(struct cx24227 *sc, uint8_t reg, uint16_t data)
{
	int error;
	uint8_t r[3];

	if (iic_acquire_bus(sc->tag, I2C_F_POLL) != 0)
		return false;

	r[0] = reg;
	r[1] = (data >> 8) & 0xff;
	r[2] = data & 0xff;
	error = iic_exec(sc->tag, I2C_OP_WRITE_WITH_STOP, sc->addr,
	    r, 3, NULL, 0, I2C_F_POLL);
	
	iic_release_bus(sc->tag, I2C_F_POLL);

	return error;
}

static int
cx24227_readreg(struct cx24227 *sc, uint8_t reg, uint16_t *data)
{
	int error;
	uint8_t r[2];

	*data = 0x0000;

	if (iic_acquire_bus(sc->tag, I2C_F_POLL) != 0)
		return -1;

	error = iic_exec(sc->tag, I2C_OP_READ_WITH_STOP, sc->addr,
			 &reg, 1, r, 2, I2C_F_POLL);

	iic_release_bus(sc->tag, I2C_F_POLL);

	*data |= r[0] << 8;
	*data |= r[1];

	return error;
}

uint16_t
cx24227_get_signal(struct cx24227 *sc)
{
	uint16_t sig = 0;

	cx24227_readreg(sc, 0xf1, &sig);
	
	return sig;
}

fe_status_t
cx24227_get_dtv_status(struct cx24227 *sc)
{
	uint16_t reg;
	fe_status_t status = 0;

	cx24227_readreg(sc, 0xf1, &reg);

	if(reg & 0x1000)
		status = FE_HAS_VITERBI | FE_HAS_CARRIER | FE_HAS_SIGNAL;
	if(reg & 0x8000)
		status |= FE_HAS_LOCK | FE_HAS_SYNC;

	return status;
}

int
cx24227_set_modulation(struct cx24227 *sc, fe_modulation_t modulation)
{
	switch (modulation) {
	case VSB_8:
	case QAM_64:
	case QAM_256:
	case QAM_AUTO:
		break;
	default:
		return EINVAL;
	}

	/* soft reset */
	cx24227_writereg(sc, 0xf5, 0x0000);
	cx24227_writereg(sc, 0xf5, 0x0001);

	switch (modulation) {
	case VSB_8:
		/* VSB8 */
		cx24227_writereg(sc, 0xf4, 0x0000);
		break;
	default:
		/* QAM */
		cx24227_writereg(sc, 0xf4, 0x0001);
		cx24227_writereg(sc, 0x85, 0x0110);
		break;
	}

	/* soft reset */
	cx24227_writereg(sc, 0xf5, 0x0000);
	cx24227_writereg(sc, 0xf5, 0x0001);

#if 0
	delay(100);

	/* open the i2c gate */
	cx24227_writereg(sc, 0xf3, 0x0001);

	/* we could tune in here? */

	/* close the i2c gate */
	cx24227_writereg(sc, 0xf3, 0x0000);

#endif
	return 0;
}

void
cx24227_enable(struct cx24227* sc, bool enable)
{
	if (enable == true) {
		cx24227_init(sc);
	}
}

struct cx24227 *
cx24227_open(device_t parent, i2c_tag_t tag, i2c_addr_t addr)
{
	struct cx24227 *sc;
	int e;
	uint16_t value;

	sc = kmem_alloc(sizeof(*sc), KM_SLEEP);
	if (sc == NULL)
		return NULL;

	sc->parent = parent;
	sc->tag = tag;
	sc->addr = addr;

	/* read chip ids */
	value = 0;
	e = cx24227_readreg(sc, 0x04, &value);
	if (e) {
		device_printf(parent, "cx24227: read failed: %d\n", e);
		kmem_free(sc, sizeof(*sc));
		return NULL;
	}
#ifdef CX24227_DEBUG
	device_printf(parent, "cx24227: chipid %04x\n", value);
#endif


	value = 0x0001; /* open the i2c gate */
	e = cx24227_writereg(sc, 0xf3, value);
#if 0
	if (e) {
		device_printf(parent, "cx24227: write failed: %d\n", e);
		kmem_free(sc, sizeof(*sc));
		return NULL;
	}
#endif

	cx24227_init(sc);

	return sc;
}

void
cx24227_close(struct cx24227 *sc)
{
	kmem_free(sc, sizeof(*sc));
}


static void
cx24227_sleepreset(struct cx24227 *sc)
{
	cx24227_writereg(sc, 0xf2, 0);
	cx24227_writereg(sc, 0xfa, 0);
}

static int
cx24227_init(struct cx24227 *sc)
{
	unsigned int i;
	uint16_t reg;

	cx24227_sleepreset(sc);

	for(i = 0; i < __arraycount(documentation_wanted); i++)
		cx24227_writereg(sc, documentation_wanted[i].r, documentation_wanted[i].v);

	/* Serial */
	cx24227_readreg(sc, 0xab, &reg);
	reg |= 0x0100;
	cx24227_writereg(sc, 0xab, reg);

	/* no spectral inversion */
	cx24227_writereg(sc, 0x1b, 0x0110);

	/* 44MHz IF */
	cx24227_writereg(sc, 0x87, 0x01be);
	cx24227_writereg(sc, 0x88, 0x0436);
	cx24227_writereg(sc, 0x89, 0x054d);

	/* GPIO on */
	cx24227_readreg(sc, 0xe3, &reg);
	reg |= 0x1100;
	cx24227_writereg(sc, 0xe3, reg);

	/* clocking */
	cx24227_readreg(sc, 0xac, &reg);
	reg &= ~0x3000;
	reg |= 0x1000;
	cx24227_writereg(sc, 0xac, reg);

	/* soft reset */
	cx24227_writereg(sc, 0xf5, 0x0000);
	cx24227_writereg(sc, 0xf5, 0x0001);
	
	/* open gate */
	cx24227_writereg(sc, 0xf3, 0x0001);

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, cx24227, "i2cexec");

static int
cx24227_modcmd(modcmd_t cmd, void *priv)
{
	if (cmd == MODULE_CMD_INIT || cmd == MODULE_CMD_FINI)
		return 0;
	return ENOTTY;
}
