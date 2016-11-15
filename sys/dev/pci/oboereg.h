/*	$NetBSD: oboereg.h,v 1.2 2008/04/28 20:23:55 martin Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jan Sparud.
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

/*
 * Toshiba OBOE SIR/FIR driver header file.
 */

#ifndef OBOE_H
#define OBOE_H

/* Registers */
/*Receive and transmit task registers (read only) */
#define OBOE_RCVT	(0x00)
#define OBOE_XMTT	(0x01)
#define OBOE_XMTT_OFFSET	0x40

/*Page pointers to the TaskFile structure */
#define OBOE_TFP2	(0x02)
#define OBOE_TFP0	(0x04)
#define OBOE_TFP1	(0x05)

/*Dunno */
#define OBOE_REG_3	(0x03)

/*Number of tasks to use in Xmit and Recv queues */
#define OBOE_NTR	(0x07)
#define OBOE_NTR_XMIT4	0x00
#define OBOE_NTR_XMIT8	0x10
#define OBOE_NTR_XMIT16	0x30
#define OBOE_NTR_XMIT32	0x70
#define OBOE_NTR_XMIT64	0xf0
#define OBOE_NTR_RECV4	0x00
#define OBOE_NTR_RECV8	0x01
#define OBOE_NTR_RECV6	0x03
#define OBOE_NTR_RECV32	0x07
#define OBOE_NTR_RECV64	0x0f

/* Dunno */
#define OBOE_REG_9	(0x09)

/* Interrupt Status Register */
#define OBOE_ISR	(0x0c)
#define OBOE_ISR_TXDONE	0x80
#define OBOE_ISR_RXDONE	0x40
#define OBOE_ISR_20	0x20
#define OBOE_ISR_10	0x10
#define OBOE_ISR_8	0x08         /*This is collision or parity or something */
#define OBOE_ISR_4	0x08
#define OBOE_ISR_2	0x08
#define OBOE_ISR_1	0x08

/*Dunno */
#define OBOE_REG_D	(0x0d)

/*Register Lock Register */
#define OBOE_LOCK	(0x0e)

/*Speed control registers */
#define OBOE_PMDL	(0x10)
#define OBOE_PMDL_SIR	0x18
#define OBOE_PMDL_MIR	0xa0
#define OBOE_PMDL_FIR	0x40

#define OBOE_SMDL	(0x18)
#define OBOE_SMDL_SIR	0x20
#define OBOE_SMDL_MIR	0x01
#define OBOE_SMDL_FIR	0x0f

#define OBOE_UDIV	(0x19)

/*Dunno */
#define OBOE_REG_11	(0x11)

/*Chip Reset Register */
#define OBOE_RST	(0x15)
#define OBOE_RST_WRAP	0x8

/*Dunno */
#define OBOE_REG_1A	(0x1a)
#define OBOE_REG_1B	(0x1b)

#define OBOE_IOSIZE	0x1f

#define IO_BAR		0x10

typedef unsigned int dword;
typedef unsigned short int word;
typedef unsigned char byte;
typedef dword Paddr;

struct OboeTask {
	uint16_t len;
	uint8_t unused;
	uint8_t control;
	uint32_t buffer;
};

#define OBOE_NTASKS 64

struct OboeTaskFile {
	struct OboeTask recv[OBOE_NTASKS];
	struct OboeTask xmit[OBOE_NTASKS];
};

#define OBOE_TASK_BUF_LEN (sizeof(struct OboeTaskFile) << 1)

/*These set the number of slots in use */
#define TX_SLOTS	8
#define RX_SLOTS	8

#define RX_BUF_SZ 	4196
#define TX_BUF_SZ	4196

/* You need also to change this, toshiba uses 4,8 and 4,4 */
/* It makes no difference if you are only going to use ONETASK mode */
/* remember each buffer use XX_BUF_SZ more _PHYSICAL_ memory */
#define OBOE_NTR_VAL 	(OBOE_NTR_XMIT8 | OBOE_NTR_RECV8)

#define OUTB(sc, val, off) \
  bus_space_write_1(sc->sc_iot, sc->sc_ioh, off, val)

#define INB(sc, off) \
  bus_space_read_1(sc->sc_iot, sc->sc_ioh, off)

#endif
