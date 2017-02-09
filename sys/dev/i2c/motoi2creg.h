/* $NetBSD: motoi2creg.h,v 1.3 2014/10/07 21:32:10 matt Exp $ */

/*-
 * Copyright (c) 2007, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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

#ifndef _DEV_I2C_MOTOI2CREG_H_
#define _DEV_I2C_MOTOI2CREG_H_

/*
 * This I2C controller is a common design used on many Motorola/Freescale
 * chip like the i.MX/MC9328, MPC8548, etc.  Different names in bit field
 * definition and not suffered from document error.
 */
#define I2CADR	0x0000	/* my own I2C addr to respond for an external master */
#define I2CFDR	0x0004	/* frequency devider */
#define I2CCR	0x0008	/* control */
#define	 CR_MEN   0x80	/* enable this HW */
#define	 CR_MIEN  0x40	/* enable interrupt */
#define	 CR_MSTA  0x20	/* 0->1 activates START, 1->0 makes STOP condition */
#define	 CR_MTX   0x10	/* 1 for Tx, 0 for Rx */
#define	 CR_TXAK  0x08	/* 1 makes no acknowledge when Rx */
#define	 CR_RSTA  0x04	/* generate repeated START condition */
#define I2CSR	0x000c	/* status */
#define	 SR_MCF   0x80	/* 0 means transfer in progress, 1 when completed */
#define	 SR_MAAS  0x40	/* 1 means addressed as slave */
#define	 SR_MBB   0x20	/* 1 before STOP condition is detected */
#define	 SR_MAL   0x10	/* arbitration was lost */
#define	 SR_MIF   0x02	/* indicates data transter completion */
#define	 SR_RXAK  0x01	/* 1 to indicate receive has completed */
#define I2CDR	0x0010	/* data */
#define	I2CDFSRR 0x0014	/* digital filter sampling rate register */

/*
 * The equation to calculate the divider frequency (from AN2919) is:
 *
 * Frequency divider = B * (A + (floor(3 * C / B) * 2))
 *
 * where (in little endian bit order, msb to lsb) FDR is split into 2 3-bit
 * fields: fA contains bits 5,1,0 and fB contains bits 4,3,2.
 *
 * A is used as an index into { 9, 10, 12, 15, 5, 6, 7, 8 } though
 * on faster machines these are doubled to { 18, 20, 24, 50, 10, 12, 14, 16 }.
 * B is either 2**(b + 1) or 2**(b + 4).
 *
 * C is the sampling rate, which may be settable via I2CDFSRR register though
 * not all implementations have it.  Regardless, we just leave it at its
 * default setting (16).  So floor(3 * C / B) * 2 becomes a precomputable
 * quantity.  Once we know its value for fB=0, we can simply shift it right
 * as fB increases since B is a power-of-2.
 */

#define FDR_A(n)		(((n) & 0x20) >> 3) | ((n) & 3))
#define FDR_B(n)		(((n) & 0x1c) >> 2)

#define	MOTOI2C_GROUP_A_VALUES	0x8765fca9U
#define	MOTOI2C_A(a)		((MOTOI2C_GROUP_A_VALUE >> (4*(a))) & 0xf)
#define	MOTOI2C_B(b)		(1 + (b))
#define	MOTOI2C_DIV(name, fdr)	\
	((name##_A(FDR_A(fdr)) + named##_C(FDR_B(fdr))) << name##_B(FDR_B(fdr)))

#define	IMX31_A(fdr_a)		MOTOI2C_A(fdr_a)
#define	IMX31_B(fdr_b)		MOTOI2C_B(fdr_b)
#define IMX31_C(fdr_b)		(6 >> (fdr_b))
#define	IMX31_DIV(fdr)		MOTOI2C_DIV(IMX31, fdr)

#define	MCF52259_A(fdr_a)	MOTOI2C_A(fdr_a)
#define	MCF52259_B(fdr_b)	MOTOI2C_B(fdr_b)
#define MCF52259_C(fdr_b)	(5 >> (fdr_b))
#define	MCF52259_DIV(fdr)	MOTOI2C_DIV(MCF52259, fdr)

#define	MPC85xx_A(fdr_a)	(MOTOI2C_A(fdr_a) << 1)
#define	MPC85xx_B(fdr_b)	(4 + (fdr_b))
#define	MPC85xx_C(fdr_b)	((6 >> (fdr_b)) & ~1)
#define	MPC85xx_DIV(fdr)	MOTOI2C_DIV(MPC85xx, fdr)

#endif /* !_DEV_I2C_MOTOI2CREG_H_ */
