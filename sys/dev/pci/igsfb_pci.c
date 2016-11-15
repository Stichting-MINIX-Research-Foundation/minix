/*	$NetBSD: igsfb_pci.c,v 1.23 2012/01/30 19:41:21 drochner Exp $ */

/*
 * Copyright (c) 2002, 2003 Valeriy E. Ushakov
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * Integraphics Systems IGA 168x and CyberPro series.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: igsfb_pci.c,v 1.23 2012/01/30 19:41:21 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>

#ifdef __sparc__  /* XXX: this doesn't belong here */
#include <machine/autoconf.h>
#endif
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>

#include <dev/ic/igsfbreg.h>
#include <dev/ic/igsfbvar.h>
#include <dev/pci/igsfb_pcivar.h>


static int	igsfb_pci_match_by_id(pcireg_t);
static int	igsfb_pci_map_regs(struct igsfb_devconfig *,
				   bus_space_tag_t, bus_space_tag_t,
				   pci_chipset_tag_t,
				   pcitag_t, pci_product_id_t);
static int	igsfb_pci_is_console(pci_chipset_tag_t, pcitag_t);

static int igsfb_pci_console = 0;
static pcitag_t igsfb_pci_constag;



static int	igsfb_pci_match(device_t, cfdata_t, void *);
static void	igsfb_pci_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(igsfb_pci, sizeof(struct igsfb_softc),
    igsfb_pci_match, igsfb_pci_attach, NULL, NULL);


static int
igsfb_pci_match_by_id(pcireg_t id)
{

	if (PCI_VENDOR(id) != PCI_VENDOR_INTEGRAPHICS)
		return 0;

	switch (PCI_PRODUCT(id)) {
	case PCI_PRODUCT_INTEGRAPHICS_IGA1682:		/* FALLTHROUGH */
	case PCI_PRODUCT_INTEGRAPHICS_CYBERPRO2000:	/* FALLTHROUGH */
	case PCI_PRODUCT_INTEGRAPHICS_CYBERPRO2010:
		return 1;
	default:
		return 0;
	}
}


int
igsfb_pci_cnattach(bus_space_tag_t iot, bus_space_tag_t memt,
		   pci_chipset_tag_t pc,
		   int bus, int device, int function)
{
	struct igsfb_devconfig *dc;
	pcitag_t tag;
	pcireg_t id;
	int ret;

	tag = pci_make_tag(pc, bus, device, function);
	id = pci_conf_read(pc, tag, PCI_ID_REG);

	if (igsfb_pci_match_by_id(id) == 0)
		return 1;

	dc = &igsfb_console_dc;
	if (igsfb_pci_map_regs(dc, iot, memt, pc, tag, PCI_PRODUCT(id)) != 0)
		return 1;

	ret = igsfb_enable(dc->dc_iot, dc->dc_iobase, dc->dc_ioflags);
	if (ret)
		return ret;

	ret = igsfb_cnattach_subr(dc);
	if (ret)
		return ret;

	igsfb_pci_console = 1;
	igsfb_pci_constag = tag;

	return 0;
}


static int
igsfb_pci_is_console(pci_chipset_tag_t pc, pcitag_t tag)
{

	return igsfb_pci_console &&
	    !memcmp(&tag, &igsfb_pci_constag, sizeof tag);
}


static int
igsfb_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	return igsfb_pci_match_by_id(pa->pa_id);
}


static void
igsfb_pci_attach(device_t parent, device_t self, void *aux)
{
	struct igsfb_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	int isconsole;

	sc->sc_dev = self;

	pci_aprint_devinfo(pa, NULL);

#if defined(__sparc__) && !defined(KRUPS_FORCE_SERIAL_CONSOLE)
	/* XXX: this doesn't belong here */
	if (PCITAG_NODE(pa->pa_tag) == prom_instance_to_package(prom_stdout()))
	{
		int b, d, f;

		pci_decompose_tag(pa->pa_pc, pa->pa_tag, &b, &d, &f);
		igsfb_pci_cnattach(pa->pa_iot, pa->pa_memt, pa->pa_pc, b,d,f);
	}
#endif

	isconsole = 0;
	if (igsfb_pci_is_console(pa->pa_pc, pa->pa_tag)) {
		sc->sc_dc = &igsfb_console_dc;
		isconsole = 1;
	} else {
		sc->sc_dc = malloc(sizeof(struct igsfb_devconfig),
				   M_DEVBUF, M_NOWAIT | M_ZERO);
		if (sc->sc_dc == NULL)
			panic("unable to allocate igsfb_devconfig");
		if (igsfb_pci_map_regs(sc->sc_dc,
			    pa->pa_iot, pa->pa_memt, pa->pa_pc,
			    pa->pa_tag, PCI_PRODUCT(pa->pa_id)) != 0)
		{
			printf("unable to map device registers\n");
			free(sc->sc_dc, M_DEVBUF);
			sc->sc_dc = NULL;
			return;
		}

		igsfb_enable(sc->sc_dc->dc_iot, sc->sc_dc->dc_iobase,
			     sc->sc_dc->dc_ioflags);
	}

	igsfb_attach_subr(sc, isconsole);
}


/*
 * Init memory and i/o bus space tags.  Map device registers.
 * Use memory space mapped i/o space access for i/o registers
 * for CyberPro cards.
 */
static int
igsfb_pci_map_regs(struct igsfb_devconfig *dc,
		   bus_space_tag_t iot, bus_space_tag_t memt,
		   pci_chipset_tag_t pc, pcitag_t tag,
		   pci_product_id_t id)
{

	dc->dc_id = id;

	/*
	 * Configure memory space first since for CyberPro we use
	 * memory-mapped i/o access.  Note that we are NOT mapping any
	 * of it yet.  (XXX: search for memory BAR?)
	 */
#define IGS_MEM_MAPREG (PCI_MAPREG_START + 0)

	dc->dc_memt = memt;
	if (pci_mapreg_info(pc, tag,
		IGS_MEM_MAPREG, PCI_MAPREG_TYPE_MEM,
		&dc->dc_memaddr, &dc->dc_memsz, &dc->dc_memflags) != 0)
	{
		printf("unable to configure memory space\n");
		return 1;
	}

	/*
	 * Configure I/O space.  On CyberPro use MMIO.  IGS 168x doesn't
	 * have a BAR for its i/o space, so we have to hardcode it.
	 */
	if (id >= PCI_PRODUCT_INTEGRAPHICS_CYBERPRO2000) {
		dc->dc_iot = dc->dc_memt;
		dc->dc_iobase = dc->dc_memaddr | IGS_MEM_MMIO_SELECT;
		dc->dc_ioflags = dc->dc_memflags;
	} else {
		dc->dc_iot = iot;
		dc->dc_iobase = 0;
		dc->dc_ioflags = 0;
	}

	/*
	 * Map I/O registers.  This is done in bus glue, not in common
	 * code because on e.g. ISA bus we'd need to access registers
	 * to obtain/program linear memory location.
	 */
	if (bus_space_map(dc->dc_iot,
			  dc->dc_iobase + IGS_REG_BASE, IGS_REG_SIZE,
			  dc->dc_ioflags,
			  &dc->dc_ioh) != 0)
	{
		printf("unable to map I/O registers\n");
		return 1;
	}

	return 0;
}
