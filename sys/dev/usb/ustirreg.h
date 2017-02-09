/*	$NetBSD: ustirreg.h,v 1.4 2008/04/28 20:24:01 martin Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Sainty <David.Sainty@dtsp.co.nz>
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
 * Registers definitions for SigmaTel STIr4200 USB/IrDA Bridge
 * Controller.  Documentation available at:
 *  http://www.sigmatel.com/technical_docs.htm
 *  http://extranet.sigmatel.com/library/infrared/stir4200/stir4200-ds-1-0.pdf
 */

/* Notes:
 *
 * The data sheet states that the TX and RX frames are prepended with
 * BOF characters.  This appears to be incorrect, the standard 0xff
 * characters behave as expected.
 *
 * There does not appear to be any way to get asynchronous
 * notifications from this device that data is waiting.  You simply do
 * have to poll continuously looking for a non-zero-length result.
 *
 * The SigmaTel drivers provided with the device for other operating
 * systems poll at full USB speed (1000 per second), which has a
 * significant impact on the system.
 */

/*
 * The SigmaTel device is controlled via an array of registers, with
 * generic register read/write commands.  This is a completely
 * different approach to that defined in the USB IrDA standard.
 */
#define STIR_REG_MODE		1
#define STIR_REG_BRATE		2
#define STIR_REG_CONTROL	3
#define STIR_REG_SENSITIVITY	4
#define STIR_REG_STATUS		5
#define STIR_REG_FFCNT_LSB	6
#define STIR_REG_FFCNT_MSB	7
#define STIR_REG_DPLL		8
#define STIR_REG_IRDIG		9

/* Register numbers range from zero to STIR_MAX_REG */
#define STIR_MAX_REG		15


/*
 * Mode register bits
 *
 * The MIR bit was documented in earlier revisions of the data sheet,
 * but in the current published version (version 1.0, March 2002) the
 * MIR bit is documented as "reserved".  Possibly the device has a
 * design flaw affecting the MIR data rates.
 */
#define STIR_RMODE_FIR		0x80
#define STIR_RMODE_MIR		0x40
#define STIR_RMODE_SIR		0x20
#define STIR_RMODE_ASK		0x10

/*
 * FASTRXEN can be set to enable simultaneous reads and writes.  It
 * isn't clear that this is useful, the RX and TX data is mixed into
 * the FIFO and the chip appears to get into a funny state.  In the
 * absence of good documentation about this bit, leave it disabled!
 */
#define STIR_RMODE_FASTRXEN	0x08

#define STIR_RMODE_FFRSTEN	0x04

/* FFSPRST must be set to enable the FIFO */
#define STIR_RMODE_FFSPRST	0x02

/*
 * High bit baud rate generator value, used in conjunction with the
 * BRATE register.
 */
#define STIR_RMODE_PDCLK8	0x01


/* Status register bits */
#define STIR_RSTATUS_EOFRAME	0x80
#define STIR_RSTATUS_FFUNDER	0x40
#define STIR_RSTATUS_FFOVER	0x20

/* Set in write direction, cleared in read direction */
#define STIR_RSTATUS_FFDIR	0x10

/*
 * FFCLR is write-only, and the only writable bit in the STATUS
 * register.
 */
#define STIR_RSTATUS_FFCLR	0x08

#define STIR_RSTATUS_FFEMPTY	0x04
#define STIR_RSTATUS_FFRXERR	0x02
#define STIR_RSTATUS_FFTXERR	0x01


/* Extract data from portions of registers */
#define STIR_GET_SENSITIVITY_CHIPREVISION(x) ((x) & 7)

/*
 * According to the documentation, FFCNT may be off by as much as 3
 * bytes.
 */
#define STIR_FFCNT_MARGIN	3

/*
 * The FIFO size for the device is a fixed 4k bytes
 */
#define STIR_FIFO_SIZE		0x1000

/*
 * Vendor specific device requests
 */
#define STIR_CMD_WRITEMULTIREG	0x00
#define STIR_CMD_READMULTIREG	0x01
#define STIR_CMD_READROM	0x02
#define STIR_CMD_WRITESINGLEREG	0x03

/*
 * The MSB is the MODE register setting, the LSB is the BRATE register
 * setting.
 *
 * The MIR rates (576000 and 1152000) were documented in earlier
 * revisions of the data sheet, but in the current published version
 * these data rates have disappeared.  Possibly the device has a
 * design flaw affecting the MIR data rates.
 */
#define STIR_BRMODE_4000000	0x8002
#define STIR_BRMODE_1152000	0x4001
#define STIR_BRMODE_576000	0x4003
#define STIR_BRMODE_115200	0x2009
#define STIR_BRMODE_57600	0x2013
#define STIR_BRMODE_38400	0x201d
#define STIR_BRMODE_19200	0x203b
#define STIR_BRMODE_9600	0x2077
#define STIR_BRMODE_2400	0x21df

/*
 * Extract values from STIR_BRMODE values.
 */
#define STIR_BRMODE_MODEREG(x)	((x) >> 8)
#define STIR_BRMODE_BRATEREG(x)	((x) & 0xff)

/*
 * Each transmit frame starts with the sequence:
 *
 * 0x55 0xaa LSB(Length) MSB(Length)
 */
#define STIR_OUTPUT_HEADER_SIZE		4
#define STIR_OUTPUT_HEADER_BYTE0	0x55
#define STIR_OUTPUT_HEADER_BYTE1	0xaa
