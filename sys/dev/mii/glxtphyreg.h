/*	$NetBSD: glxtphyreg.h,v 1.2 2008/04/28 20:23:53 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _DEV_MII_GLXTPHYREG_H_
#define	_DEV_MII_GLXTPHYREG_H_

/*
 * LXT-1000 registers.
 */

#define	MII_GLXTPHY_PCR		0x10	/* port configuration register */
#define	PCR_TX_DIS		(1U << 13)  /* transmit disable */
#define	PCR_BYP_SCR		(1U << 12)  /* bypass scrambler */
#define	PCR_BYP_4B5B		(1U << 11)  /* bypass 4b/5b encoder */
#define	PCR_JAB_DIS		(1U << 10)  /* disable jabber */
#define	PCR_SQE			(1U << 9)   /* enable heartbeat */
#define	PCR_TP_LOOPBACK		(1U << 8)   /* disable TP loopback */
#define	PCR_SMART_SPEED		(1U << 7)   /* enable SmartSpeed */
#define	PCR_PRE_EN		(1U << 5)   /* preamble enable */
#define	PCR_10_SERIAL		(1U << 3)   /* 10Mb/s serial mode */
#define	PCR_AN_ISOLATE		(1U << 2)   /* autoneg. isolate */
#define	PCR_TBI			(1U << 1)   /* use ten-bit interface */

#define	MII_GLXTPHY_QSR		0x11	/* quick status register */
#define	QSR_SPEED_get(x)	(((x) >> 14) & 0x3)
#define	QSR_TX_STATUS		(1U << 13)  /* transmit active */
#define	QSR_RX_STATUS		(1U << 12)  /* receive active */
#define	QSR_COL_STATUS		(1U << 11)  /* collision active */
#define	QSR_LINK		(1U << 10)  /* link up */
#define	QSR_DUPLEX		(1U << 9)   /* full-duplex */
#define	QSR_AN			(1U << 8)   /* autoneg. enabled */
#define	QSR_ACOMP		(1U << 7)   /* autoneg. complete */
#define	QSR_LINE_LENGTH_get(x)	(((x) >> 4) & 0x7)
#define	QSR_PAUSE		(1U << 3)   /* partner can pause */
#define	QSR_APAUSE		(1U << 2)   /* partner can asym-pause */
#define	QSR_EVENT		(1U << 0)   /* event has occurred */

#define	SPEED_10_SERIAL		0
#define	SPEED_10_MII		1
#define	SPEED_100		2
#define	SPEED_1000		3

#define	MII_GLXT_IER		0x12	/* interrupt enable register */
#define	IER_AN_FAULT		(1U << 13)  /* autoneg fault */
#define	IER_CROSS		(1U << 11)  /* crossover MDIX */
#define	IER_POLARITY		(1U << 10)  /* polarity change */
#define	IER_SMRT		(1U << 9)   /* SmartSpeed event */
#define	IER_CNTR		(1U << 8)   /* counter full */
#define	IER_AN			(1U << 7)   /* autoneg complete */
#define	IER_SPEED		(1U << 6)   /* speed change */
#define	IER_DUPLEX		(1U << 5)   /* duplex change */
#define	IER_LINK		(1U << 4)   /* link change */
#define	IER_INTEN		(1U << 1)   /* enable interrupts */
#define	IER_TINT		(1U << 0)   /* force interrupt */

#define	MII_GLXT_ISR		0x13	/* interrupt status register */
	/* See IER bits. */

#define	MII_GLXT_LEDCFG		0x14	/* LED configuration register */
#define	LEDCFG_LEDC(x)		((x) << 14) /* collision */
#define	LEDCFG_LEDR(x)		((x) << 12) /* receive */
#define	LEDCFG_LEDT(x)		((x) << 10) /* transmit */
#define	LEDCFG_LEDG(x)		((x) << 8)  /* gigabit */
#define	LEDCFG_LEDS(x)		((x) << 6)  /* speed */
#define	LEDCFG_LEDL(x)		((x) << 4)  /* link */
#define	LEDCFG_LEDF(x)		((x) << 2)  /* full-duplex */
#define	LEDCFG_PULSESTRETCH	(1U << 1)
#define	LEDCFG_LEDFREQ		(1U << 0)

#define	LED_MODE_INDICATION	0	    /* indicate LED's event */
#define	LED_MODE_BLINK		1	    /* blink */
#define	LED_MODE_ON		2	    /* force on */
#define	LED_MODE_OFF		3	    /* force off */

#define	MII_GLXT_PORTCR		0x15	/* port control register */
#define	PORTCR_TX_TCLK		(1U << 15)  /* enable TX_TCLK */
#define	PORTCR_ALT_NP		(1U << 13)  /* alternet next-page feature */

#endif /* _DEV_MII_GLXTPHYREG_H_ */
