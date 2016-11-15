/*	$NetBSD: mcclock_pad32.c,v 1.16 2009/03/14 21:04:19 dsl Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mcclock_pad32.c,v 1.16 2009/03/14 21:04:19 dsl Exp $");


/*
 * mc1461818 (or compatible) clock chip driver,  for machines where each
 * byte-wide mcclock chip register is  mapped
 * into the low-order byte of a little-endian 32-bit word.
 *
 *  DECstation 2100/3100
 *  DECstation 5100
 *  DECstation 5000/200 baseboard
 *  IOCTL asic machines (Alpha  3000 series, Decstation 5000 series)
 *
 * bus-specific frontends should just declare an attach and match
 * entry, and set up a initializea switch to call the functions below.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/clock_subr.h>

#include <machine/autoconf.h>
#include <dev/dec/clockvar.h>
#include <dev/dec/mcclockvar.h>
#include <dev/ic/mc146818reg.h>

#include <dev/dec/mcclock_pad32.h>


void	mcclock_pad32_write(struct mcclock_softc *, u_int, u_int);
u_int	mcclock_pad32_read(struct mcclock_softc *, u_int);

const struct mcclock_busfns mcclock_pad32_busfns = {
	mcclock_pad32_write, mcclock_pad32_read,
};

void
mcclock_pad32_write(struct mcclock_softc *dev, u_int reg, u_int datum)
{
	struct mcclock_pad32_softc *sc = (struct mcclock_pad32_softc *)dev;

	sc->sc_dp[reg].datum = datum;
}

u_int
mcclock_pad32_read(struct mcclock_softc *dev, u_int reg)
{
	struct mcclock_pad32_softc *sc = (struct mcclock_pad32_softc *)dev;

	return (sc->sc_dp[reg].datum);
}
