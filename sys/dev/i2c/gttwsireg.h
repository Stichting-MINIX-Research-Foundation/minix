/* $NetBSD: gttwsireg.h,v 1.3 2014/09/11 11:14:44 jmcneill Exp $ */

/*
 * Copyright (c) 2008 Eiji Kawauchi.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _GTTWSIREG_H_
#define _GTTWSIREG_H_

#include "opt_gttwsi.h"

#define GTTWSI_SIZE		0x100

#if defined(GTTWSI_ALLWINNER)
#define TWSI_SLAVEADDR		0x00
#define TWSI_EXTEND_SLAVEADDR	0x04
#define TWSI_DATA		0x08
#define TWSI_CONTROL		0x0c
#define TWSI_STATUS		0x10
#define TWSI_BAUDRATE		0x14
#define TWSI_SOFTRESET		0x18
#else
#define	TWSI_SLAVEADDR		0x00
#define	TWSI_EXTEND_SLAVEADDR	0x10
#define	TWSI_DATA		0x04
#define	TWSI_CONTROL		0x08
#define	TWSI_STATUS		0x0c	/* for read */
#define	TWSI_BAUDRATE		0x0c	/* for write */
#define	TWSI_SOFTRESET		0x1c
#endif

#define	SLAVEADDR_GCE_MASK	0x01
#define	SLAVEADDR_SADDR_MASK	0xfe

#define	EXTEND_SLAVEADDR_MASK	0xff

#define	DATA_MASK		0xff

#define	CONTROL_ACK		(1<<2)
#define	CONTROL_IFLG		(1<<3)
#define	CONTROL_STOP		(1<<4)
#define	CONTROL_START		(1<<5)
#define	CONTROL_TWSIEN		(1<<6)
#define	CONTROL_INTEN		(1<<7)

#define	STAT_BE		0x00	/* Bus Error */
#define	STAT_SCT	0x08	/* Start condition transmitted */
#define	STAT_RSCT	0x10	/* Repeated start condition transmitted */
#define	STAT_AWBT_AR	0x18	/* Address + write bit transd, ack recvd */
#define	STAT_AWBT_ANR	0x20	/* Address + write bit transd, ack not recvd */
#define	STAT_MTDB_AR	0x28	/* Master transd data byte, ack recvd */
#define	STAT_MTDB_ANR	0x30	/* Master transd data byte, ack not recvd */
#define	STAT_MLADADT	0x38	/* Master lost arbitr during addr or data tx */
#define	STAT_ARBT_AR	0x40	/* Address + read bit transd, ack recvd */
#define	STAT_ARBT_ANR	0x48	/* Address + read bit transd, ack not recvd */
#define	STAT_MRRD_AT	0x50	/* Master received read data, ack transd */
#define	STAT_MRRD_ANT	0x58	/* Master received read data, ack not transd */
#define	STAT_SAWBT_AR	0xd0	/* Second addr + write bit transd, ack recvd */
#define	STAT_SAWBT_ANR	0xd8	/* S addr + write bit transd, ack not recvd */
#define	STAT_SARBT_AR	0xe0	/* Second addr + read bit transd, ack recvd */
#define	STAT_SARBT_ANR	0xe8	/* S addr + read bit transd, ack not recvd */
#define	STAT_NRS	0xf8	/* No relevant status */

#define	SOFTRESET_VAL		0		/* reset value */

#define TWSI_RETRY_COUNT	1000		/* retry loop count */
#define TWSI_RETRY_DELAY	1		/* retry delay */
#define	TWSI_STAT_DELAY		1		/* poll status delay */
#define	TWSI_READ_DELAY		2		/* read delay */
#define	TWSI_WRITE_DELAY	2		/* write delay */

#endif	/* _GTTWSIREG_H_ */
