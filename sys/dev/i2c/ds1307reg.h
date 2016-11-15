/*	$NetBSD: ds1307reg.h,v 1.5 2014/10/12 01:23:23 macallan Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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

#ifndef _DEV_I2C_DS1307REG_H_
#define _DEV_I2C_DS1307REG_H_

/*
 * DS1307 64x8 Serial Real-Time Clock
 */

#define	DS1307_ADDR		0x68	/* Fixed I2C Slave Address */

#define DSXXXX_SECONDS		0x00
#define DSXXXX_MINUTES		0x01
#define DSXXXX_HOURS		0x02
#define DSXXXX_DAY		0x03
#define DSXXXX_DATE		0x04
#define DSXXXX_MONTH		0x05
#define DSXXXX_YEAR		0x06
#define	DSXXXX_RTC_SIZE		7

#define DS1307_CONTROL		0x07
#define	DS1307_RTC_START	0
#define	DS1307_RTC_SIZE		DSXXXX_RTC_SIZE
#define	DS1307_NVRAM_START	0x08
#define	DS1307_NVRAM_SIZE	0x38

#define DS1339_CONTROL		0x0e
#define	DS1339_RTC_START	0
#define	DS1339_RTC_SIZE		DSXXXX_RTC_SIZE
#define	DS1339_NVRAM_START	0
#define	DS1339_NVRAM_SIZE	0

#define DS1672_CNTR1		0x00
#define DS1672_CNTR2		0x01
#define DS1672_CNTR3		0x02
#define DS1672_CNTR4		0x03
#define DS1672_CONTROL		0x04
#define DS1672_TRICKLE		0x05

#define DS1672_RTC_START	0
#define DS1672_RTC_SIZE		4
#define	DS1672_NVRAM_START	0
#define	DS1672_NVRAM_SIZE	0

#define DS3232_CONTROL		0x0e
#define DS3232_CSR		0x0f
#define	DS3232_RTC_START	0
#define	DS3232_RTC_SIZE		DSXXXX_RTC_SIZE
#define DS3232_TEMP_MSB		0x11
#define DS3232_TEMP_LSB		0x12
#define	DS3232_NVRAM_START	0x14
#define	DS3232_NVRAM_SIZE	0xec


/*
 * Bit definitions.
 */
#define	DSXXXX_SECONDS_MASK	0x7f
#define	DSXXXX_MINUTES_MASK	0x7f
#define	DSXXXX_HOURS_12HRS_MODE	(1u << 6)	/* Set for 12 hour mode */
#define	DSXXXX_HOURS_12HRS_PM	(1u << 5)	/* If 12 hr mode, set = PM */
#define	DSXXXX_HOURS_12MASK	0x1f
#define	DSXXXX_HOURS_24MASK	0x3f
#define	DSXXXX_DAY_MASK		0x07
#define	DSXXXX_DATE_MASK	0x3f
#define	DSXXXX_MONTH_MASK	0x1f
#define	DSXXXX_MONTH_CENTURY	0x80

#define	DS1307_SECONDS_CH	(1u << 7)	/* Clock Hold */
#define	DS1307_CONTROL_OUT	(1u << 7)	/* OSC/OUT pin value */
#define	DS1307_CONTROL_SQWE	(1u << 4)	/* Enable square wave output */
#define	DS1307_CONTROL_1HZ	0
#define	DS1307_CONTROL_4096HZ	1
#define	DS1307_CONTROL_8192HZ	2
#define	DS1307_CONTROL_32768HZ	3

#define	DSXXXX_CONTROL_DOSC	(1u << 7)	/* Disable Oscillator */

#endif /* _DEV_I2C_DS1307REG_H_ */
