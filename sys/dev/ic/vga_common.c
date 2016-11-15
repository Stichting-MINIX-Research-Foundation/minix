/* $NetBSD: vga_common.c,v 1.11 2009/02/19 00:39:26 jmcneill Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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
__KERNEL_RCSID(0, "$NetBSD: vga_common.c,v 1.11 2009/02/19 00:39:26 jmcneill Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/bus.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>

/*
 * The following functions implement back-end configuration grabbing
 * and attachment.
 */
int
vga_common_probe(bus_space_tag_t iot, bus_space_tag_t memt)
{
	bus_space_handle_t ioh_vga, ioh_6845, memh;
	u_int8_t regval;
	u_int16_t vgadata;
	int gotio_vga, gotio_6845, gotmem, mono, rv;
	int dispoffset;

	gotio_vga = gotio_6845 = gotmem = rv = 0;

	if (bus_space_map(iot, 0x3c0, 0x10, 0, &ioh_vga))
		goto bad;
	gotio_vga = 1;

	/* read "misc output register" */
	regval = bus_space_read_1(iot, ioh_vga, VGA_MISC_DATAR);
	mono = !(regval & 1);

	if (bus_space_map(iot, (mono ? 0x3b0 : 0x3d0), 0x10, 0, &ioh_6845))
		goto bad;
	gotio_6845 = 1;

	if (bus_space_map(memt, 0xa0000, 0x20000, 0, &memh))
		goto bad;
	gotmem = 1;

	dispoffset = (mono ? 0x10000 : 0x18000);

	vgadata = bus_space_read_2(memt, memh, dispoffset);
	bus_space_write_2(memt, memh, dispoffset, 0xa55a);
	if (bus_space_read_2(memt, memh, dispoffset) != 0xa55a)
		goto bad;
	bus_space_write_2(memt, memh, dispoffset, vgadata);

	/*
	 * check if this is really a VGA
	 * (try to write "Color Select" register as XFree86 does)
	 * XXX check before if at least EGA?
	 */
	/* reset state */
	(void) bus_space_read_1(iot, ioh_6845, 10);
	bus_space_write_1(iot, ioh_vga, VGA_ATC_INDEX,
	    20 | 0x20); /* colselect | enable */
	regval = bus_space_read_1(iot, ioh_vga, VGA_ATC_DATAR);
	/* toggle the implemented bits */
	bus_space_write_1(iot, ioh_vga, VGA_ATC_DATAW, regval ^ 0x0f);
	bus_space_write_1(iot, ioh_vga, VGA_ATC_INDEX, 20 | 0x20);
	/* read back */
	if (bus_space_read_1(iot, ioh_vga, VGA_ATC_DATAR) != (regval ^ 0x0f))
		goto bad;
	/* restore contents */
	bus_space_write_1(iot, ioh_vga, VGA_ATC_DATAW, regval);

	rv = 1;
bad:
	if (gotio_vga)
		bus_space_unmap(iot, ioh_vga, 0x10);
	if (gotio_6845)
		bus_space_unmap(iot, ioh_6845, 0x10);
	if (gotmem)
		bus_space_unmap(memt, memh, 0x20000);

	return (rv);
}
