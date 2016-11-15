/*	$NetBSD: mk48txxreg.h,v 1.11 2011/01/04 01:28:15 matt Exp $ */
/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * Mostek MK48Txx clocks.
 *
 * The MK48T02 has 2KB of non-volatile memory. The time-of-day clock
 * registers start at offset 0x7f8.
 *
 * The MK48T08 and MK48T18 have 8KB of non-volatile memory
 *
 * The MK48T59 also has 8KB of non-volatile memory but in addition it
 * has a battery low detection bit and a power supply wakeup alarm for
 * power management.  It's at offset 0x1ff0 in the NVRAM.
 */

/*
 * Mostek MK48TXX register definitions
 */

/*
 * The first bank of eight registers at offset (nvramsz - 16) is
 * available only on more recent (which??) MK48Txx models.
 */
#define MK48TXX_IFLAGS	0	/* flags */
/*			1	   unused on MK48T59 */
#define MK48TXX_IASEC	2	/* alarm seconds (0..59; BCD) */
#define MK48TXX_IAMIN	3	/* alarm minutes (0..59; BCD) */
#define MK48TXX_IAHOUR	4	/* alarm hour (0..23; BCD) */
#define MK48TXX_IADAY	5	/* alarm day (1..31; BCD) */
#define MK48TXX_IINTR	6	/* interrupts */
#define MK48TXX_IWDOG	7	/* watchdog */
#define MK48TXX_ICSR	8	/* control register / century (DS1553) */
#define MK48TXX_ISEC	9	/* seconds (0..59; BCD) */
#define MK48TXX_IMIN	10	/* minutes (0..59; BCD) */
#define MK48TXX_IHOUR	11	/* hour (0..23; BCD) */
#define MK48TXX_IWDAY	12	/* weekday (1..7) */
#define MK48TXX_IDAY	13	/* day in month (1..31; BCD) */
#define MK48TXX_IMON	14	/* month (1..12; BCD) */
#define MK48TXX_IYEAR	15	/* year (0..99; BCD) */

/* Bits in the flags register */
#define MK48TXX_FLAGS_WDF	0x80	/* watchdog flag */
#define MK48TXX_FLAGS_ALARM	0x40	/* alarm flag */
#define MK48TXX_FLAGS_BATTLOW	0x10	/* battery low */

/* Bits in the interrupt register */
#define MK48TXX_INTR_AFE	0x80	/* alarm flag enable */
#define MK48TXX_INTR_ABE	0x20	/* alarm in battery backup enable */

/* Bits in the watchdog register */
#define MK48TXX_WDOG_WDS	0x80	/* watchdog steering */
#define MK48TXX_WDOG_BMB_MASK	0x7c	/* watchdog multiplier bits */
#define MK48TXX_WDOG_BMB_SHIFT	2
#define MK48TXX_WDOG_RES_MASK	0x03	/* watchdog resolution bits */
#define MK48TXX_WDOG_RES_1_16S	0x00	/*   1/16 seconds */
#define MK48TXX_WDOG_RES_1_4S	0x01	/*   1/4 seconds */
#define MK48TXX_WDOG_RES_1S	0x02	/*   1 second */
#define MK48TXX_WDOG_RES_4S	0x03	/*   4 seconds */

/* Bits in the control register */
#define MK48TXX_CSR_WRITE	0x80	/* want to write */
#define MK48TXX_CSR_READ	0x40	/* want to read (freeze clock) */
#define MK48TXX_CSR_CENT_MASK	0x3f	/* century mask */

/* Bit in the weekday register */
#define MK48TXX_WDAY_FT		0x40	/* freq test: toggle sec[0] at 512Hz */
					/* next two are on MK48T59 only */
#define MK48TXX_WDAY_CEB	0x20	/* century enable */
#define MK48TXX_WDAY_CB		0x10	/* century bit */

/* Bit in the seconds register */
#define MK48TXX_SEC_STOP	0x80	/* stop the oscillator */

#define MK48T02_CLKSZ		2048
#define MK48T02_CLKOFF		0x7f0

#define MK48T08_CLKSZ		8192
#define MK48T08_CLKOFF		0x1ff0

#define MK48T18_CLKSZ		8192
#define MK48T18_CLKOFF		0x1ff0

#define MK48T59_CLKSZ		8192
#define MK48T59_CLKOFF		0x1ff0

#define	DS1553_CLKSZ		8192
#define	DS1553_CLKOFF		0x1ff0
