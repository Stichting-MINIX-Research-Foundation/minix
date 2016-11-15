/*	$NetBSD: m41st84reg.h,v 1.3 2005/12/11 12:21:22 christos Exp $	*/

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

#ifndef _DEV_I2C_M41ST84REG_H_
#define	_DEV_I2C_M41ST84REG_H_

#define M41ST84_ADDR	0x68

#define	M41ST84_REG_CSEC	0	/* 00-99     -- BCD Centiseconds */
#define M41ST84_REG_SEC		1	/* 00-59     -- BCD Seconds */
#define M41ST84_REG_MIN		2	/* 00-59     -- BCD Minutes */
#define M41ST84_REG_CENHR	3	/* 0-1/00-23 -- BCD Century/Hour */
#define M41ST84_REG_DAY		4	/* 01-07     -- BCD Day */
#define M41ST84_REG_DATE	5	/* 01-31     -- BCD Date */
#define M41ST84_REG_MONTH	6	/* 01-12     -- BCD Month */
#define M41ST84_REG_YEAR	7	/* 00-99     -- BCD Year */
#define M41ST84_REG_DATE_BYTES	8
#define M41ST84_REG_CONTROL	8	/* Control Register */
#define	M41ST84_REG_WATCHDOG	9	/* Watchdog Register */
#define	M41ST84_REG_AL_MONTH	10	/* 01-12     -- BCD Month */
#define	M41ST84_REG_AL_DATE	11	/* 01-31     -- BCD Date */
#define	M41ST84_REG_AL_HOUR	12	/* 00-23     -- BCD Hour */
#define	M41ST84_REG_AL_MIN	13	/* 00-59     -- BCD Minutes */
#define	M41ST84_REG_AL_SEC	14	/* 00-59     -- BCD Seconds */
#define	M41ST84_REG_FLAGS	15	/* Flags Register */
			/*	16-18	reserved	*/
#define	M41ST84_REG_SQW		19	/* Square Wave Register */
#define	M41ST84_USER_RAM	20
#define	M41ST84_USER_RAM_SIZE	43

#define M41ST84_SEC_MASK	0x7f
#define M41ST84_MIN_MASK	0x7f
#define M41ST84_HOUR_MASK	0x3f
#define M41ST84_DAY_MASK	0x07
#define M41ST84_DATE_MASK	0x3f
#define M41ST84_MONTH_MASK	0x1f
#define M41ST84_YEAR_MASK	0xff

#define	M41ST84_SEC_ST		0x80		/* clock stop bit */

#define	M41ST84_CONTROL_CALIB_MASK	0x1f
#define	M41ST84_CONTROL_S		0x20	/* sign bit */
#define	M41ST84_CONTROL_FT		0x40	/* Frequency test bit */
#define	M41ST84_CONTROL_OUT		0x80	/* Output level */

#define	M41ST84_WATCHDOG_RB_MASK	0x03	/* Watchdog resulotion bits */
#define	M41ST84_WATCHDOG_BMB_MASK	0x7c	/* Watchdog multiplier bits */
#define	M41ST84_WATCHDOG_WDS		0x80	/* Watchdog steering bit */

#define	M41ST84_AL_MONTH_ABE		0x20	/* alarm in b-backup mode en */
#define	M41ST84_AL_MONTH_SQWE		0x40	/* square wave enable */
#define	M41ST84_AL_MONTH_AFE		0x80	/* alarm flage enable */

#define	M41ST84_AL_HOUR_HT		0x40	/* Halt Update Bit */

#define	M41ST84_FLAGS_BL		0x10	/* battery low flag */
#define	M41ST84_FLAGS_AF		0x40	/* alarm flag */
#define	M41ST84_FLAGS_WDF		0x80	/* watchdog flag */

#endif /* _DEV_I2C_M41ST84REG_H_ */
