/* $NetBSD: r2025reg.h,v 1.1 2006/03/06 19:55:08 shige Exp $ */

/*-
 * Copyright (c) 2006 Shigeyuki Fukushima.
 * All rights reserved.
 *
 * Written by Shigeyuki Fukushima.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_I2C_R2025REG_H_
#define _DEV_I2C_R2025REG_H_

/*
 * R2025S/D Real-Time Clock
 */

/* I2C Slave Address */
#define	R2025_ADDR			0x32

/* Register size */
#define	R2025_REG_SIZE			16

#define R2025_CLK_SIZE			7	/* 7bytes: 0x0-0x6 */

/* Registers */
#define R2025_REG_SEC			0x0
#define R2025_REG_MIN			0x1
#define R2025_REG_HOUR			0x2
#define R2025_REG_WDAY			0x3
#define R2025_REG_DAY			0x4
#define R2025_REG_MON			0x5
#define R2025_REG_YEAR			0x6
#define R2025_REG_CORRECTCLOCK		0x7
#define R2025_REG_ALARMW_MIN		0x8
#define R2025_REG_ALARMW_HOUR		0x9
#define R2025_REG_ALARMW_WDAY		0xa
#define R2025_REG_ALARMD_MIN		0xb
#define R2025_REG_ALARMD_HOUR		0xc
#define R2025_REG_RESERVED		0xd
#define R2025_REG_CTRL1			0xe
#define R2025_REG_CTRL2			0xf


/* Register mask */
#define R2025_REG_SEC_MASK		0x7f
#define R2025_REG_MIN_MASK		0x7f
#define R2025_REG_HOUR_MASK		0x3f
#define R2025_REG_WDAY_MASK		0x07
#define R2025_REG_DAY_MASK		0x3f
#define R2025_REG_MON_MASK		0x1f
#define R2025_REG_YEAR_MASK		0xff
#define R2025_REG_CORRECTCLOCK_MASK	0x7f
#define R2025_REG_ALARMW_MIN_MASK	0x7f
#define R2025_REG_ALARMW_HOUR_MASK	0x3f
#define R2025_REG_ALARMW_WDAY_MASK	0x7f
#define R2025_REG_ALARMD_MIN_MASK	0x7f
#define R2025_REG_ALARMD_HOUR_MASK	0x3f
#define R2025_REG_CTRL1_MASK		0xff
#define R2025_REG_CTRL2_MASK		0xff

/* Register flag: R2025_MON */
#define R2025_REG_MON_Y1920		(1u << 7)

/* Register flag: R2025_CTRL1 */
#define R2025_REG_CTRL1_WALE		(1u << 7)
#define R2025_REG_CTRL1_DALE		(1u << 6)
#define R2025_REG_CTRL1_H1224		(1u << 5)
#define R2025_REG_CTRL1_CLEN2		(1u << 4)
#define R2025_REG_CTRL1_TEST		(1u << 3)
#define R2025_REG_CTRL1_CT2		(1u << 2)
#define R2025_REG_CTRL1_CT1		(1u << 1)
#define R2025_REG_CTRL1_CT0		(1u << 0)

/* Register flag: R2025_CTRL2 */
#define R2025_REG_CTRL2_VDSL		(1u << 7)
#define R2025_REG_CTRL2_VDET		(1u << 6)
#define R2025_REG_CTRL2_XST		(1u << 5)
#define R2025_REG_CTRL2_PON		(1u << 4)
#define R2025_REG_CTRL2_CLEN1		(1u << 3)
#define R2025_REG_CTRL2_CTFG		(1u << 2)
#define R2025_REG_CTRL2_WAFG		(1u << 1)
#define R2025_REG_CTRL2_DAFG		(1u << 0)

#endif /* _DEV_I2C_R2025REG_H_ */
