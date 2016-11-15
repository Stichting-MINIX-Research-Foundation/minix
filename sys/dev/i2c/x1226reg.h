/*	$NetBSD: x1226reg.h,v 1.4 2013/08/07 19:38:45 soren Exp $	*/

/*
 * Copyright (c) 2003 Shigeyuki Fukushima.
 * All rights reserved.
 *
 * Written by Shigeyuki Fukushima for the NetBSD project.
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
 *      Shigeyuki Fukushima.
 * 4. The name of Shigeyuki Fukushima may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SHIGEYUKI FUKUSHIMA ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL SHIGEYUKI FUKUSHIMA
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Xicor X1226 RTC registers
 */

#ifndef _DEV_I2C_X1226REG_H_
#define _DEV_I2C_X1226REG_H_

/*
 * Xicor X1226 RTC I2C Address:
 *
 *	110 1111
 */
#define	X1226_ADDRMASK		0x3ff
#define	X1226_ADDR		0x6f

/* XICOR X1226 Device Identifier */
#define X1226_DEVID_CCR		0x6f
#define X1226_DEVID_EEPROM	0x57

/* Watchdog RTC registers */
#define	X1226_REG_Y2K		0x37	/* bcd century (19/20) */
#define	X1226_REG_DW		0x36	/* bcd ay of week (0-6) */
#define	X1226_REG_YR		0x35	/* bcd year (0-99) */
#define	X1226_REG_MO		0x34	/* bcd onth (1-12) */
#define	X1226_REG_DT		0x33	/* bcd ay (1-31) */
#define	X1226_REG_HR		0x32	/* bcd our (0-23) */
#define	X1226_REG_MN		0x31	/* bcd inute (0-59) */
#define	X1226_REG_SC		0x30	/* bcd econd (0-59) */
#define	X1226_REG_RTC_BASE	0x30
#define	X1226_REG_RTC_SIZE	((X1226_REG_Y2K - X1226_REG_RTC_BASE) + 1)
/* Watchdog RTC registers mask */
#define	X1226_REG_Y2K_MASK	0x39
#define	X1226_REG_DW_MASK	0x07
#define	X1226_REG_YR_MASK	0xff
#define	X1226_REG_MO_MASK	0x1f
#define	X1226_REG_DT_MASK	0x3f
#define	X1226_REG_HR12_MASK	0x1f
#define	X1226_REG_HR24_MASK	0x3f
#define	X1226_REG_MN_MASK	0x7f
#define	X1226_REG_SC_MASK	0x7f

#define	X1226_REG_SR		0x3f
#define	X1226_CTRL_DTR		0x13
#define	X1226_CTRL_ATR		0x12
#define	X1226_CTRL_INT		0x11
#define	X1226_CTRL_BL		0x10

/* NVRAM size (512 x 8 bit) */
#define X1226_NVRAM_START	0x0040
#define X1226_NVRAM_END		0x00FF
#define X1226_NVRAM_SIZE	((X1226_NVRAM_END - X1226_NVRAM_START) + 1)

/* XICOR X1226 RTC flags */
#define X1226_FLAG_SR_RTCF	0x01
#define X1226_FLAG_SR_WEL	0x02
#define X1226_FLAG_SR_RWEL	0x04
#define X1226_FLAG_SR_AL0	0x20
#define X1226_FLAG_SR_AL1	0x40
#define X1226_FLAG_SR_BAT	0x80
#define X1226_FLAG_HR_12HPM	0x20
#define X1226_FLAG_HR_24H	0x80


#endif /* _DEV_I2C_X1226REG_H_ */
