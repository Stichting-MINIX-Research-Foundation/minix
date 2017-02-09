/*	$NetBSD: mcclock_pad32.h,v 1.10 2000/02/11 02:36:16 simonb Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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
 * mc1461818 (or compatible) clock driver, for machines where
 * each byte-wide mcclock chip register is  mapped into the low-order
 * byte of a little-endian 32-bit word,
 *  DECstation 2100/3100
 *  DECstation 5100
 *  DECstation 5000/200 baseboard
 *  IOCTL asic machines (Alpha  3000 series, Decstation 5000 series)
 */
struct mcclock_pad32_clockdatum {
	u_char	datum;
	char	pad[3];
};

/*
 * Device softc used by bus-specific front-end.
 */
struct mcclock_pad32_softc {
	struct mcclock_softc	sc_mcclock;
	struct mcclock_pad32_clockdatum *sc_dp;
};

/* register read/write functions */
extern const struct mcclock_busfns mcclock_pad32_busfns;
