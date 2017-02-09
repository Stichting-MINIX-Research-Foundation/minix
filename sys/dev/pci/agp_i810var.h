/*	$NetBSD: agp_i810var.h,v 1.6 2015/03/06 22:03:06 riastradh Exp $	*/

/*-
 * Copyright (c) 2000 Doug Rabson
 * Copyright (c) 2000 Ruslan Ermilov
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>

#include <dev/pci/agpvar.h>

struct agp_i810_softc {
	struct pci_attach_args vga_pa;	/* integrated graphics device args */
	int chiptype;			/* chipset family: i810, i830, &c. */
	uint32_t stolen;		/* pages of stolen graphics memory */

	/* Memory-mapped I/O for integrated graphics device registers.  */
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	bus_size_t size;

	/* Graphics translation table.  */
	bus_space_tag_t gtt_bst;
	bus_space_handle_t gtt_bsh;
	bus_size_t gtt_size;

	/* Chipset flush page.  */
	bus_space_tag_t flush_bst;
	bus_space_handle_t flush_bsh;
	bus_addr_t flush_addr;

	/* i810-only fields.  */
	struct agp_gatt *gatt;		/* i810-only OS-allocated GTT */
	uint32_t dcache_size;		/* i810-only on-chip memory size */

	/* XXX Kludge to work around broken X servers.  */
	pcireg_t pgtblctl;

	/* XXX Vestige of unfinished powerhook?  */
	uint32_t pgtblctl_resume_hack;
};

extern struct agp_softc	*agp_i810_sc;

#define	AGP_I810_GTT_VALID		0x01
#define	AGP_I810_GTT_I810_DCACHE	0x02 /* i810-only */
#define	AGP_I810_GTT_CACHED		0x06 /* >=i830 */

int	agp_i810_write_gtt_entry(struct agp_i810_softc *, off_t, bus_addr_t,
	    int);
void	agp_i810_post_gtt_entry(struct agp_i810_softc *, off_t);
void	agp_i810_chipset_flush(struct agp_i810_softc *);
