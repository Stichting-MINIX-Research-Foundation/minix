/*	$NetBSD: i2c_bitbang.c,v 1.13 2010/04/25 00:35:58 tsutsui Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Common module for bit-bang'ing an I2C bus.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i2c_bitbang.c,v 1.13 2010/04/25 00:35:58 tsutsui Exp $");

#include <sys/param.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>

#define	SETBITS(x)	ops->ibo_set_bits(v, (x))
#define	DIR(x)		ops->ibo_set_dir(v, (x))
#define	READ		ops->ibo_read_bits(v)

#define	SDA		ops->ibo_bits[I2C_BIT_SDA]	/* i2c signal */
#define	SCL		ops->ibo_bits[I2C_BIT_SCL]	/* i2c signal */
#define	OUTPUT		ops->ibo_bits[I2C_BIT_OUTPUT]	/* SDA is output */
#define	INPUT		ops->ibo_bits[I2C_BIT_INPUT]	/* SDA is input */

#ifndef SCL_BAIL_COUNT
#define SCL_BAIL_COUNT 1000
#endif

static inline int i2c_wait_for_scl(void *, i2c_bitbang_ops_t);

static inline int
i2c_wait_for_scl(void *v, i2c_bitbang_ops_t ops)
{
	int bail = 0;

	while (((READ & SCL) == 0) && (bail < SCL_BAIL_COUNT)) {
		delay(1);
		bail++;
	}
	if (bail == SCL_BAIL_COUNT) {
		i2c_bitbang_send_stop(v, 0, ops);
		return EIO;
	}
	return 0;
}

/*ARGSUSED*/
int
i2c_bitbang_send_start(void *v, int flags, i2c_bitbang_ops_t ops)
{

	/* start condition: put SDA H->L edge during SCL=H */

	DIR(OUTPUT);
	SETBITS(SDA | SCL);
	delay(5);		/* bus free time (4.7 us) */
	SETBITS(  0 | SCL);
	if (i2c_wait_for_scl(v, ops) != 0)
		return EIO;
	delay(4);		/* start hold time (4.0 us) */

	/* leave SCL=L and SDA=L to avoid unexpected start/stop condition */
	SETBITS(  0 |   0);

	return 0;
}

/*ARGSUSED*/
int
i2c_bitbang_send_stop(void *v, int flags, i2c_bitbang_ops_t ops)
{

	/* stop condition: put SDA L->H edge during SCL=H */

	/* assume SCL=L, SDA=L here */
	DIR(OUTPUT);
	SETBITS(  0 | SCL);
	delay(4);		/* stop setup time (4.0 us) */
	SETBITS(SDA | SCL);

	return 0;
}

int
i2c_bitbang_initiate_xfer(void *v, i2c_addr_t addr, int flags,
    i2c_bitbang_ops_t ops)
{

	if (addr < 0x80) {
		uint8_t i2caddr;

		/* disallow the 10-bit address prefix */
		if ((addr & 0x78) == 0x78)
			return EINVAL;
		i2caddr = (addr << 1) | ((flags & I2C_F_READ) ? 1 : 0);
		(void) i2c_bitbang_send_start(v, flags, ops);

		return (i2c_bitbang_write_byte(v, i2caddr,
			    flags & ~I2C_F_STOP, ops));

	} else if (addr < 0x400) {
		uint16_t	i2caddr;
		int		rv;

		i2caddr = (addr << 1) | ((flags & I2C_F_READ) ? 1 : 0) |
		    0xf000;

		(void) i2c_bitbang_send_start(v, flags, ops);
		rv = i2c_bitbang_write_byte(v, i2caddr >> 8,
		    flags & ~I2C_F_STOP, ops);
		/* did a slave ack the 10-bit prefix? */
		if (rv != 0)
			return rv;

		/* send the lower 7-bits (+ read/write mode) */
		return (i2c_bitbang_write_byte(v, i2caddr & 0xff,
			    flags & ~I2C_F_STOP, ops));

	} else
		return EINVAL;
}

int
i2c_bitbang_read_byte(void *v, uint8_t *valp, int flags, i2c_bitbang_ops_t ops)
{
	int i;
	uint8_t val = 0;
	uint32_t bit;

	/* assume SCL=L, SDA=L here */

	DIR(INPUT);

	for (i = 0; i < 8; i++) {
		val <<= 1;

		/* data is set at SCL H->L edge */
		/* SDA is set here because DIR() is INPUT */
		SETBITS(SDA |   0);
		delay(5);	/* clock low time (4.7 us) */

		/* read data at SCL L->H edge */
		SETBITS(SDA | SCL);
		if (i2c_wait_for_scl(v, ops) != 0)
			return EIO;
		if (READ & SDA)
			val |= 1;
		delay(4);	/* clock high time (4.0 us) */
	}
	/* set SCL H->L before set SDA direction OUTPUT */
	SETBITS(SDA |   0);

	/* set ack after SCL H->L edge */
	bit = (flags & I2C_F_LAST) ? SDA : 0;
	DIR(OUTPUT);
	SETBITS(bit |   0);
	delay(5);	/* clock low time (4.7 us) */

	/* ack is checked at SCL L->H edge */
	SETBITS(bit | SCL);
	if (i2c_wait_for_scl(v, ops) != 0)
		return EIO;
	delay(4);	/* clock high time (4.0 us) */

	/* set SCL H->L for next data; don't change SDA here */
	SETBITS(bit |   0);

	/* leave SCL=L and SDA=L to avoid unexpected start/stop condition */
	SETBITS(  0 |   0);


	if ((flags & (I2C_F_STOP | I2C_F_LAST)) == (I2C_F_STOP | I2C_F_LAST))
		(void) i2c_bitbang_send_stop(v, flags, ops);

	*valp = val;
	return 0;
}

int
i2c_bitbang_write_byte(void *v, uint8_t val, int flags, i2c_bitbang_ops_t ops)
{
	uint32_t bit;
	uint8_t mask;
	int error;

	/* assume at SCL=L, SDA=L here */

	DIR(OUTPUT);

	for (mask = 0x80; mask != 0; mask >>= 1) {
		bit = (val & mask) ? SDA : 0;

		/* set data after SCL H->L edge */
		SETBITS(bit |   0);
		delay(5);	/* clock low time (4.7 us) */

		/* data is fetched at SCL L->H edge */
		SETBITS(bit | SCL);
		if (i2c_wait_for_scl(v, ops))
			return EIO;
		delay(4);	/* clock high time (4.0 us) */

		/* put SCL H->L edge; don't change SDA here */
		SETBITS(bit |   0);
	}

	/* ack is set at H->L edge */
	DIR(INPUT);
	delay(5);	/* clock low time (4.7 us) */

	/* read ack at L->H edge */
	/* SDA is set here because DIR() is INPUT */
	SETBITS(SDA | SCL);
	if (i2c_wait_for_scl(v, ops) != 0)
		return EIO;
	error = (READ & SDA) ? EIO : 0;
	delay(4);	/* clock high time (4.0 us) */

	/* set SCL H->L before set SDA direction OUTPUT */
	SETBITS(SDA |   0);
	DIR(OUTPUT);
	/* leave SCL=L and SDA=L to avoid unexpected start/stop condition */
	SETBITS(  0 |   0);

	if (flags & I2C_F_STOP)
		(void) i2c_bitbang_send_stop(v, flags, ops);

	return error;
}
