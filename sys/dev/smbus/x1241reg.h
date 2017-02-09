/* $NetBSD: x1241reg.h,v 1.1 2002/11/12 01:00:59 simonb Exp $ */

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Simon Burge for Wasabi Systems, Inc.
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

#ifndef _DEV_SMBUS_X1241REG_H_
#define	_DEV_SMBUS_X1241REG_H_

/*
 * The X1241 appears at two fixed addresses on the SMBus, one each for
 * the EEPROM array and the real time clock.
 */
#define	X1241_ARRAY_SLAVEADDR	0x57
#define	X1241_RTC_SLAVEADDR	0x6f

#define	X1241REG_BL		0x10	/* Control register */
#define	X1241REG_SC		0x30	/* Seconds */
#define	X1241REG_MN		0x31	/* Minutes */
#define	X1241REG_HR		0x32	/* Hours */
#define	X1241REG_DT		0x33	/* Day of month */
#define	X1241REG_MO		0x34	/* Month */
#define	X1241REG_YR		0x35	/* Year */
#define	X1241REG_DW		0x36	/* Day of Week */
#define	X1241REG_Y2K		0x37	/* Year 2K */
#define	X1241REG_SR		0x3f	/* Status register */

/* Register bits for the status register */
#define	X1241REG_SR_BAT		0x80	/* currently on battery power */
#define	X1241REG_SR_RWEL	0x04	/* r/w latch is enabled, can write RTC */
#define	X1241REG_SR_WEL		0x02	/* r/w latch is unlocked, can enable r/w now */
#define	X1241REG_SR_RTCF	0x01	/* clock failed */

/* Register bits for the block protect register */
#define	X1241REG_BL_BP2		0x80	/* block protect 2 */
#define	X1241REG_BL_BP1		0x40	/* block protect 1 */
#define	X1241REG_BL_BP0		0x20	/* block protect 0 */
#define	X1241REG_BL_WD1		0x10	/* watchdog timeout 0 */
#define	X1241REG_BL_WD0		0x08	/* watchdog timeout 1 */

/* Register bits for the hours register */
#define	X1241REG_HR_MIL		0x80	/* military time format */

#endif /* _DEV_SMBUS_X1241REG_H_ */
