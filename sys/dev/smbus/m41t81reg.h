/* $NetBSD: m41t81reg.h,v 1.4 2005/12/11 12:23:56 christos Exp $ */

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

#ifndef _DEV_SMBUS_M41T81REG_H_
#define	_DEV_SMBUS_M41T81REG_H_

/* The M41T81 appears at a fixed addresses on the SMBus */
#define	 M41T81_SLAVEADDR	0x68

#define	M41T81_PARTSEC		0x00	/* Partial seconds (0.01s resolution) - BCD */
#define	M41T81_SEC		0x01	/* Seconds - BCD */
#define	 M41T81_SEC_ST		  0x80	  /* Stop Bit */
#define	M41T81_MIN		0x02	/* Minutes - BCD */
#define	M41T81_HOUR		0x03	/* Century/Hours - BCD */
#define	 M41T81_HOUR_MASK	  0x3f	  /* Mask for hours */
#define	 M41T81_HOUR_CB		  0x40	  /* Century Bit */
#define	 M41T81_HOUR_CEB	  0x80	  /* Century Enable Bit */
#define	M41T81_DAY		0x04	/* Day of Week */
#define	M41T81_DATE		0x05	/* Date: Day of Month - BCD */
#define	M41T81_MON		0x06	/* Month - BCD */
#define	M41T81_YEAR		0x07	/* Year - BCD */
#define	M41T81_CTL		0x08	/* Control */
#define	 M41T81_CTL_CAL		  0x1f	  /* Calibration mask */
#define	 M41T81_CTL_S		  0x20	  /* Sign Bit */
#define	 M41T81_CTL_FT		  0x40	  /* Frequency Test Bit */
#define	 M41T81_CTL_OUT		  0x80	  /* Output Level */
#define	M41T81_WDOG		0x09	/* Watchdog */
#define	 M41T81_WDOG_RB		  0x03	  /* Watchdog Resolution */
#define	 M41T81_WDOG_RB_1_16	  0x00	  /* 1/16th second */
#define	 M41T81_WDOG_RB_1_4	  0x01	  /* 1/4th second */
#define	 M41T81_WDOG_RB_1	  0x02	  /* 1 second */
#define	 M41T81_WDOG_RB_4	  0x03	  /* 4 seconds */
#define	 M41T81_WDOG_BMB_SHIFT	  2	  /* Watchdog Multiplier */
#define	 M41T81_WDOG_BMB_MASK	  0x7c
#define	M41T81_ALM_MON		0x0a	/* Alarm Month - BCD */
#define	 M41T81_ALM_MON_ABE	  0x20	  /* Alarm in Battery Back-up Mode Enable Bit */
#define	 M41T81_ALM_MON_SQWE	  0x40	  /* Square Wave Enable */
#define	 M41T81_ALM_MON_AFE	  0x80	  /* Alarm Flag Enable Flag */
#define	M41T81_ALM_DATE		0x0b	/* Alarm Date - BCD */
#define	 M41T81_ALM_DATE_RPT5	  0x40	  /* Alarm Repeat Mode Bit 5 */
#define	 M41T81_ALM_DATE_RPT4	  0x80	  /* Alarm Repeat Mode Bit 4 */
#define	M41T81_ALM_HOUR		0x0c	/* Alarm Hour - BCD */
#define	 M41T81_ALM_HOUR_HT	  0x40	  /* Half Update Bit */
#define	 M41T81_ALM_HOUR_RPT3	  0x80	  /* Alarm Repeat Mode Bit 3 */
#define	M41T81_ALM_MIN		0x0d	/* Alarm Minutes - BCD */
#define	 M41T81_ALM_MIN_RPT2	  0x80	  /* Alarm Repeat Mode Bit 2 */
#define	M41T81_ALM_SEC		0x0e	/* Alarm Seconds - BCD */
#define	 M41T81_ALM_SEC_RPT1	  0x80	  /* Alarm Repeat Mode Bit 1 */
#define	M41T81_FLAGS		0x0f
#define	M41T81_SQW		0x13	/* Square Wave Frequency */
#define	 M41T81_SQW_RS_SHIFT	  4	  /* SQW Frequency */
#define	 M41T81_SQW_RS_MASK	  0x0f	  /* For a non-zero value 'v',
					     the square wave frequency
					     is 2^(16-v) Hz */

#endif /* _DEV_SMBUS_M41T81REG_H_ */
