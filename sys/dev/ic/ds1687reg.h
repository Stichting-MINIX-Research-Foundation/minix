/*	$NetBSD: ds1687reg.h,v 1.10 2008/05/04 12:50:38 martin Exp $ 	*/

/*
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rafal K. Boni.
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
 * Originally based on mc146818reg.h, with the following license:
 *
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Definitions for the Dallas Semiconductor DS1687 Real Time Clock.
 *
 * The DS1687 and follow-on RTC chips are Y2k-compliant successors to the
 * DS1287, which in turn is register-compatible with the MC146818 and/or
 * MC146818A RTCs.
 *
 * Plucked right from the Dallas Semicomductor specs available at:
 *	http://pdfserv.maxim-ic.com/arpdf/DS1685-DS1687.pdf
 *
 * The DS1686 contains 14 basic clock-related registers and 50 bytes of
 * user RAM laid out for compatibility with the register layout of the
 * DS1287/MC14818 chips.  It also includes an extended mode which allows
 * access to these same basic registers as well an an extended register
 * set and NVRAM area; this extended register set includes a century
 * register for Y2k compliant date storage.
 *
 * Since the locations of these ports and the method used to access them
 * can be machine-dependent, the low-level details of reading and writing
 * writing the RTC's registers are handled by machine-specific functions.
 *
 * The Dallas chip can store time-of-day and alarm data in BCD or binary;
 * this setting applies to *all* values stored in the clock chip and a
 * change from one mode to the other requires *all* of the clock data to
 * be re-written.   The "hours" time-of-year and alarm registers can be
 * stored either in an AM/PM or a 24-hour format; the format is set
 * globally and changing it requires re-writing both the hours time-of-
 * year and alarm registers.  In AM/PM mode, the hour must be in the
 * range of 1-12 (and stored as either BCD or binary), with the high-
 * bit cleared to indicate AM and set to indicate PM.  In 24-hour mode,
 * hours must be in the range 0-23.
 *
 * In order to support extended features like the century register and
 * an embedded silicon serial number while keeping backwards compatibility
 * with the DS1287/MC146818, the DS1687 provides a bank-switching method
 * which allows the user to switch the RTC between a "compatible" mode in
 * bank 0 and an extended mode in bank 1.
 *
 * Both banks provide access to the 14 timekeeping/alarm registers and
 * to 50 bytes of user RAM.  In addition, bank 0 provides access to an
 * additional 64 bytes of user RAM in the upper half of the RTC address
 * space.
 *
 * Bank 1, on the other hand, provides access to an extended register set,
 * including a silicon serial number -- including a model ID byte, century
 * register for Y2k compatibility and memory address/data registers which
 * allow indirect access to a larger extended user RAM address space.  It
 * is worth noting that the extended user RAM is distinct from the "basic"
 * 114 bytes of user RAM which are accesible in bank 0.
 */

/*
 * The registers, and the bits within each register.
 */

#define	DS1687_SEC	0x00	/* Time of year: seconds (0-59) */
#define DS1687_ASEC	0x01	/* Alarm: seconds */
#define	DS1687_MIN	0x02	/* Time of year: minutes (0-59) */
#define	DS1687_AMIN	0x03	/* Alarm: minutes */
#define	DS1687_HOUR	0x04	/* Time of year: hour (see above) */
#define	DS1687_AHOUR	0x05	/* Alarm: hour (see above) */
#define	DS1687_DOW	0x06	/* Time of year: day of week (1-7, 1 = Sun) */
#define	DS1687_DOM	0x07	/* Time of year: day of month (1-31) */
#define	DS1687_MONTH	0x08	/* Time of year: month (1-12) */
#define	DS1687_YEAR	0x09	/* Time of year: year in century (0-99) */

#define DS1687_CONTROLA	0x0a	/* Control Register A */

#define DS1687_UIP	0x80	/* Update in progress: RO */
#define DS1687_DV2	0x40	/* Countdown chain: 0 = on,  1 = reset if DV1 */
#define DS1687_DV1	0x20	/* Oscillator enable */
#define DS1687_BANK1	0x10	/* Bank select: 0 = bank0, 1 = bank1 */
#define DS1687_RATEMASK 0x0f	/* Rate select bits for sq. wave and PIE */

#define DS1687_CONTROLB	0x0b	/* Control Register B */

#define DS1687_SET	0x80	/* Clock update control: 1 = disable update */
#define DS1687_PIE	0x40	/* Periodic interrupt enable */
#define DS1687_AIE	0x20	/* Alarm interrupt enable */
#define DS1687_UIE	0x10	/* Update-ended interrupt enable */
#define DS1687_SQWE	0x08	/* Enable sq. wave output on SQW pin */
#define DS1687_BINARY	0x04	/* Data mode: 0 = BCD, 1 = binary data */
#define DS1687_24HRS	0x02	/* Hour format: 1 = 24hrs, 0 = 12hrs */
#define DS1687_DSE	0x01	/* Daylight savings enable */

#define DS1687_CONTROLC	0x0c	/* Control register C: Read-only */
				/* Note: PF, AF, UF cleared on read */

#define DS1687_IRQF	0x80	/* IRQ present: set when any IRQ is active */
#define DS1687_PF	0x40	/* Periodic interrupt: independent of PIE */
#define DS1687_AF	0x20	/* Alarm reached: independent of AIE */
#define DS1687_UF	0x10	/* Update ended: independent of UIE */

#define DS1687_CONTROLD	0x0d	/* Control register D: Read-only */

#define DS1687_VRT	0x80	/* Valid RAM and time: battery bad if 0 */

#define DS1687_NVRAM_START	0x0e	/* Start of user ram: offset 14 */
#define DS1687_NVRAM_SIZE	0x72	/* 114 bytes of user RAM */

#define DS1687_BANK1_START	0x40	/* BANK1: Start of BANK1 registers */
#define	DS1687_BANK1_CENTURY	0x48 	/* BANK1: Time of yr: Century (0-99) */
#define	DS1687_BANK1_ADATE	0x49	/* BANK1: Alarm: Date (1-31) */
#define DS1687_BANK1_XCTRL4A	0x4a
	#define DS1687_X4A_VRT	0x80	/* valid RAM / time */
	#define DS1687_X4A_INCR	0x40	/* increment status */
	#define DS1687_X4A_BME	0x20	/* burst mode enable */
	#define DS1687_X4A_PAB	0x08	/* power active bar */
	#define DS1687_X4A_RCF	0x04	/* read clear flag */
	#define DS1687_X4A_WAF	0x02	/* wakeup alarm flag */
	#define DS1687_X4A_KF	0x01	/* kickstart flag */
#define DS1687_BANK1_XCTRL4B	0x4b
	#define DS1687_X4B_ABE	0x80	/* auxillary battery enable */
	#define DS1687_X4B_E32K	0x40	/* enable 32.768kHz output */
	#define DS1687_X4B_CS	0x20	/* chrystal select */
	#define DS1687_X4B_RCE	0x10	/* RAM clear enable */
	#define DS1687_X4B_PRS	0x08	/* PAB reset select */
	#define DS1687_X4B_RCIE	0x04	/* RAM clear interrupt enable */
	#define DS1687_X4B_WIE	0x02	/* wakeup interrupt enable */
	#define DS1687_X4B_KIE	0x01	/* kickstart interrupt enable */

#define	DS1687_NBASEREGS	0x0d	/* 14 registers; CMOS follows */

/* Layout of software shadow copy of TOD registers */
#define DS1687_NHDW_TODREGS	0x0a	/* 10 basic TOD registers */
#define DS1687_NSOFT_TODREGS	0x0c	/* ...plus shadow CENTURY, ADATE */

#define	DS1687_SOFT_SEC		0x00
#define DS1687_SOFT_ASEC	0x01
#define	DS1687_SOFT_MIN		0x02
#define	DS1687_SOFT_AMIN	0x03
#define	DS1687_SOFT_HOUR	0x04
#define	DS1687_SOFT_AHOUR	0x05
#define	DS1687_SOFT_DOW		0x06
#define	DS1687_SOFT_DOM		0x07
#define	DS1687_SOFT_MONTH	0x08
#define	DS1687_SOFT_YEAR	0x09
#define DS1687_SOFT_CENTURY	0x0a
#define DS1687_SOFT_ADATE	0x0b

/*
 * RTC register/NVRAM read and write functions -- machine-dependent.
 * Appropriately manipulate RTC registers to get/put data values.
 */
u_int ds1687_read(void *, u_int);
void ds1687_write(void *, u_int, u_int);

/*
 * A collection of TOD/Alarm registers.
 */
typedef u_int ds1687_todregs[DS1687_NSOFT_TODREGS];

/*
 * Get all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DS1687_GETTOD(sc, regs)						\
	do {								\
		int i;							\
		u_int ctl;						\
									\
		/* turn off update for now */				\
		ctl = ds1687_read(sc, DS1687_CONTROLB);			\
		ds1687_write(sc, DS1687_CONTROLB, ctl | DS1687_SET);	\
									\
		/* read all of the tod/alarm regs */			\
		for (i = 0; i < DS1687_NHDW_TODREGS; i++) 		\
			(*regs)[i] = ds1687_read(sc, i);		\
									\
		(*regs)[DS1687_SOFT_CENTURY] = 				\
				ds1687_read(sc, DS1687_BANK1_CENTURY);	\
		(*regs)[DS1687_SOFT_ADATE] = 				\
				ds1687_read(sc, DS1687_BANK1_ADATE);	\
									\
		/* turn update back on */				\
		ds1687_write(sc, DS1687_CONTROLB, ctl);			\
	} while (0);

/*
 * Set all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DS1687_PUTTOD(sc, regs)						\
	do {								\
		int i;							\
		u_int ctl;						\
									\
		/* turn off update for now */				\
		ctl = ds1687_read(sc, DS1687_CONTROLB);			\
		ds1687_write(sc, DS1687_CONTROLB, ctl | DS1687_SET);	\
									\
		/* write all of the tod/alarm regs */			\
		for (i = 0; i < DS1687_NHDW_TODREGS; i++) 		\
			ds1687_write(sc, i, (*regs)[i]);		\
									\
		ds1687_write(sc, DS1687_BANK1_CENTURY,			\
					(*regs)[DS1687_SOFT_CENTURY]);	\
		ds1687_write(sc, DS1687_BANK1_ADATE,			\
					(*regs)[DS1687_SOFT_ADATE]);	\
									\
		/* turn update back on */				\
		ds1687_write(sc, DS1687_CONTROLB, ctl);			\
	} while (0);
