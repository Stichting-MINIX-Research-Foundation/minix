/* $NetBSD: tvpll.c,v 1.6 2015/03/07 14:16:51 jmcneill Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: tvpll.c,v 1.6 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/syslog.h>
#include <sys/module.h>

#include <dev/dtv/dtvio.h>
#include <dev/i2c/i2cvar.h>
#include <dev/i2c/tvpllvar.h>

struct tvpll {
	device_t		parent;
	i2c_tag_t		tag;
	i2c_addr_t		addr;
	const struct tvpll_data *	pll;
	uint32_t		frequency;
};

static uint32_t tvpll_algo(struct tvpll *, uint8_t *,
		const struct dvb_frontend_parameters *, uint32_t *);

struct tvpll *
tvpll_open(device_t parent, i2c_tag_t t, i2c_addr_t a, struct tvpll_data *p)
{
	struct tvpll *tvpll;

	tvpll = kmem_alloc(sizeof(struct tvpll), KM_SLEEP);
        if (tvpll == NULL)
                return NULL;

	tvpll->tag = t;
	tvpll->addr = a;

	tvpll->pll = p;

	if (tvpll->pll->initdata) {
		iic_acquire_bus(tvpll->tag, I2C_F_POLL);
		(void)iic_exec(tvpll->tag, I2C_OP_WRITE_WITH_STOP, tvpll->addr,
		    &tvpll->pll->initdata[1], tvpll->pll->initdata[0],
		    NULL, 0, I2C_F_POLL);
		iic_release_bus(tvpll->tag, I2C_F_POLL);
	}

	device_printf(parent, "tvpll: %s\n", tvpll->pll->name);

	return tvpll;
}

void
tvpll_close(struct tvpll *tvpll)
{
	kmem_free(tvpll, sizeof(*tvpll));
}

static uint32_t
tvpll_algo(struct tvpll *tvpll, uint8_t *b,
    const struct dvb_frontend_parameters *p, uint32_t *fr)
{
	const struct tvpll_data *pll;
	uint32_t d;
	int i;
	
	pll = tvpll->pll;

	if(p->frequency != 0 &&
	    (p->frequency < pll->min || p->frequency > pll->max))
		return 0;

	for(i = 0; i < pll->count; i++) {
		if (p->frequency > pll->entries[i].limit)
			continue;
		else
			break;
	}

	if ( i >= pll->count)
		return EINVAL;

	d = (p->frequency + pll->iffreq) / pll->entries[i].stepsize;

	b[0] = (d >> 8) & 0xff;
	b[1] = (d >> 0) & 0xff;
	b[2] = pll->entries[i].config;
	b[3] = pll->entries[i].cb;
	b[4] = pll->entries[i].aux;

	*fr = (d * pll->entries[i].stepsize) - pll->iffreq;

	log(LOG_DEBUG, "pllw %d %02x %02x %02x %02x %02x\n", *fr, b[0], b[1], b[2], b[3], b[4]);
	return 0;
}

int
tvpll_tune_dtv(struct tvpll *tvpll,
    const struct dvb_frontend_parameters *params)
{
	int rv;
	uint32_t fr;
	uint8_t b[5], ab[2];

	fr = 0;

	if((rv = tvpll_algo(tvpll, b, params, &fr)) != 0)
		return rv;

	iic_acquire_bus(tvpll->tag, I2C_F_POLL);
	/* gate ctrl? */
	if (b[4] != TVPLL_IGNORE_AUX) {
		ab[0] = b[2] | 0x18;
		ab[1] = b[4];
		rv = iic_exec(tvpll->tag, I2C_OP_WRITE_WITH_STOP, tvpll->addr, ab, 2, NULL, 0, I2C_F_POLL);
	}
	rv = iic_exec(tvpll->tag, I2C_OP_WRITE_WITH_STOP, tvpll->addr, b, 4, NULL, 0, I2C_F_POLL);
	iic_release_bus(tvpll->tag, I2C_F_POLL);

	if (rv != 0)
		printf("%s\n", __func__);

	tvpll->frequency = fr;

	return rv;
}

MODULE(MODULE_CLASS_DRIVER, tvpll, "i2cexec");

static int
tvpll_modcmd(modcmd_t cmd, void *opaque)
{
	if (cmd == MODULE_CMD_INIT || cmd == MODULE_CMD_FINI)
		return 0;
	return ENOTTY;
}
