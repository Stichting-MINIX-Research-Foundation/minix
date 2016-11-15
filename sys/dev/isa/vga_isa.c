/* $NetBSD: vga_isa.c,v 1.21 2008/03/14 22:12:08 cube Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: vga_isa.c,v 1.21 2008/03/14 22:12:08 cube Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/isa/isavar.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/isa/vga_isavar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

int	vga_isa_match(device_t, cfdata_t, void *);
void	vga_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(vga_isa, sizeof(struct vga_softc),
    vga_isa_match, vga_isa_attach, NULL, NULL);

int
vga_isa_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* If values are hardwired to something that they can't be, punt. */
	if (ia->ia_nio < 1 ||
	    (ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT &&
	     ia->ia_io[0].ir_addr != 0x3b0))
		return (0);

	if (ia->ia_niomem < 1 ||
	    (ia->ia_iomem[0].ir_addr != ISA_UNKNOWN_IOMEM &&
	     ia->ia_iomem[0].ir_addr != 0xa0000))
		return (0);
	if (ia->ia_iomem[0].ir_size != 0 &&
	    ia->ia_iomem[0].ir_size != 0x20000)
		return (0);

	if (ia->ia_nirq > 0 &&
	    ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ)
		return (0);

	if (ia->ia_ndrq > 0 &&
	    ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ)
		return (0);

	if (!vga_is_console(ia->ia_iot, WSDISPLAY_TYPE_ISAVGA) &&
	    !vga_common_probe(ia->ia_iot, ia->ia_memt))
		return (0);

	ia->ia_nio = 1;
	ia->ia_io[0].ir_addr = 0x3b0;	/* XXX mono 0x3b0 color 0x3c0 */
	ia->ia_io[0].ir_size = 0x30;	/* XXX 0x20 */

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_addr = 0xa0000;
	ia->ia_iomem[0].ir_size = 0x20000;

	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

	return (2);	/* more than generic pcdisplay */
}

void
vga_isa_attach(device_t parent, device_t self, void *aux)
{
	struct vga_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;

	sc->sc_dev = self;
	aprint_normal("\n");

	vga_common_attach(sc, ia->ia_iot, ia->ia_memt, WSDISPLAY_TYPE_ISAVGA,
	    0, NULL);
}

int
vga_isa_cnattach(bus_space_tag_t iot, bus_space_tag_t memt)
{

	return (vga_cnattach(iot, memt, WSDISPLAY_TYPE_ISAVGA, 1));
}
