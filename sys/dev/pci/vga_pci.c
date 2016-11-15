/*	$NetBSD: vga_pci.c,v 1.54 2012/01/30 19:41:23 drochner Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: vga_pci.c,v 1.54 2012/01/30 19:41:23 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>

#include <dev/isa/isareg.h>	/* For legacy VGA address ranges */

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/pci/wsdisplay_pci.h>

#include "opt_vga.h"

#ifdef VGA_POST
#  if defined(__i386__) || defined(__amd64__)
#    include "acpica.h"
#  endif
#include <x86/vga_post.h>
#endif

#define	NBARS		6	/* number of PCI BARs */

struct vga_bar {
	bus_addr_t vb_base;
	bus_size_t vb_size;
	pcireg_t vb_type;
	int vb_flags;
};

struct vga_pci_softc {
	struct vga_softc sc_vga;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;

	struct vga_bar sc_bars[NBARS];
	struct vga_bar sc_rom;

#ifdef VGA_POST
	struct vga_post *sc_posth;
#endif

	struct pci_attach_args sc_paa;
};

static int	vga_pci_match(device_t, cfdata_t, void *);
static void	vga_pci_attach(device_t, device_t, void *);
static int	vga_pci_rescan(device_t, const char *, const int *);
static int	vga_pci_lookup_quirks(struct pci_attach_args *);
static bool	vga_pci_resume(device_t dv, const pmf_qual_t *);

CFATTACH_DECL2_NEW(vga_pci, sizeof(struct vga_pci_softc),
    vga_pci_match, vga_pci_attach, NULL, NULL, vga_pci_rescan, NULL);

static int	vga_pci_ioctl(void *, u_long, void *, int, struct lwp *);
static paddr_t	vga_pci_mmap(void *, off_t, int);

static const struct vga_funcs vga_pci_funcs = {
	vga_pci_ioctl,
	vga_pci_mmap,
};

static const struct {
	int id;
	int quirks;
} vga_pci_quirks[] = {
	{PCI_ID_CODE(PCI_VENDOR_SILMOTION, PCI_PRODUCT_SILMOTION_SM712),
	 VGA_QUIRK_NOFASTSCROLL},
	{PCI_ID_CODE(PCI_VENDOR_CYRIX, PCI_PRODUCT_CYRIX_CX5530_VIDEO),
	 VGA_QUIRK_NOFASTSCROLL},
};

static const struct {
	int vid;
	int quirks;
} vga_pci_vquirks[] = {
	{PCI_VENDOR_ATI, VGA_QUIRK_ONEFONT},
};

static int
vga_pci_lookup_quirks(struct pci_attach_args *pa)
{
	int i;

	for (i = 0; i < sizeof(vga_pci_quirks) / sizeof (vga_pci_quirks[0]);
	     i++) {
		if (vga_pci_quirks[i].id == pa->pa_id)
			return (vga_pci_quirks[i].quirks);
	}
	for (i = 0; i < sizeof(vga_pci_vquirks) / sizeof (vga_pci_vquirks[0]);
	     i++) {
		if (vga_pci_vquirks[i].vid == PCI_VENDOR(pa->pa_id))
			return (vga_pci_vquirks[i].quirks);
	}
	return (0);
}

static int
vga_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	int potential;

	potential = 0;

	/*
	 * If it's prehistoric/vga or display/vga, we might match.
	 * For the console device, this is just a sanity check.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_PREHISTORIC &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PREHISTORIC_VGA)
		potential = 1;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	     PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA)
		potential = 1;

	if (!potential)
		return (0);

	/* check whether it is disabled by firmware */
	if ((pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    != (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
		return (0);

	/* If it's the console, we have a winner! */
	if (vga_is_console(pa->pa_iot, WSDISPLAY_TYPE_PCIVGA))
		return (1);

	/*
	 * If we might match, make sure that the card actually looks OK.
	 */
	if (!vga_common_probe(pa->pa_iot, pa->pa_memt))
		return (0);

	return (1);
}

static void
vga_pci_attach(device_t parent, device_t self, void *aux)
{
	struct vga_pci_softc *psc = device_private(self);
	struct vga_softc *sc = &psc->sc_vga;
	struct pci_attach_args *pa = aux;
	int bar, reg;

	sc->sc_dev = self;
	psc->sc_pc = pa->pa_pc;
	psc->sc_pcitag = pa->pa_tag;
	psc->sc_paa = *pa;

	pci_aprint_devinfo(pa, NULL);

	/*
	 * Gather info about all the BARs.  These are used to allow
	 * the X server to map the VGA device.
	 */
	for (bar = 0; bar < NBARS; bar++) {
		reg = PCI_MAPREG_START + (bar * 4);
		if (!pci_mapreg_probe(psc->sc_pc, psc->sc_pcitag, reg,
				      &psc->sc_bars[bar].vb_type)) {
			/* there is no valid mapping register */
			continue;
		}
		if (PCI_MAPREG_TYPE(psc->sc_bars[bar].vb_type) ==
		    PCI_MAPREG_TYPE_IO) {
			/* Don't bother fetching I/O BARs. */
			continue;
		}
#ifndef __LP64__
		if (PCI_MAPREG_MEM_TYPE(psc->sc_bars[bar].vb_type) ==
		    PCI_MAPREG_MEM_TYPE_64BIT) {
			/* XXX */
			aprint_error_dev(self,
			    "WARNING: ignoring 64-bit BAR @ 0x%02x\n", reg);
			bar++;
			continue;
		}
#endif
		if (pci_mapreg_info(psc->sc_pc, psc->sc_pcitag, reg,
		     psc->sc_bars[bar].vb_type,
		     &psc->sc_bars[bar].vb_base,
		     &psc->sc_bars[bar].vb_size,
		     &psc->sc_bars[bar].vb_flags))
			aprint_error_dev(self,
			    "WARNING: strange BAR @ 0x%02x\n", reg);
	}

	/* XXX Expansion ROM? */

	vga_common_attach(sc, pa->pa_iot, pa->pa_memt, WSDISPLAY_TYPE_PCIVGA,
			  vga_pci_lookup_quirks(pa), &vga_pci_funcs);

#ifdef VGA_POST
	psc->sc_posth = vga_post_init(pa->pa_bus, pa->pa_device, pa->pa_function);
	if (psc->sc_posth == NULL)
		aprint_error_dev(self, "WARNING: could not prepare POST handler\n");
#endif

	/*
	 * XXX Do not use the generic PCI framework for now as
	 * XXX it would power down the device when the console
	 * XXX is still using it.
	 */
	if (!pmf_device_register(self, NULL, vga_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
	config_found_ia(self, "drm", aux, vga_drm_print);
}

static int
vga_pci_rescan(device_t self, const char *ifattr, const int *locators)
{
	struct vga_pci_softc *psc = device_private(self);

	config_found_ia(self, "drm", &psc->sc_paa, vga_drm_print);

	return 0;
}

static bool
vga_pci_resume(device_t dv, const pmf_qual_t *qual)
{
#if defined(VGA_POST) && NACPICA > 0
	extern int acpi_md_vbios_reset;
#endif
	struct vga_pci_softc *sc = device_private(dv);

	vga_resume(&sc->sc_vga);

#if defined(VGA_POST) && NACPICA > 0
	if (sc->sc_posth != NULL && acpi_md_vbios_reset == 2)
		vga_post_call(sc->sc_posth);
#endif

	return true;
}

int
vga_pci_cnattach(bus_space_tag_t iot, bus_space_tag_t memt,
    pci_chipset_tag_t pc, int bus, int device,
    int function)
{

	return (vga_cnattach(iot, memt, WSDISPLAY_TYPE_PCIVGA, 0));
}

int
vga_drm_print(void *aux, const char *pnp)
{
	if (pnp)
		aprint_normal("drm at %s", pnp);
	return (UNCONF);
}


static int
vga_pci_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct vga_config *vc = v;
	struct vga_pci_softc *psc = (void *) vc->softc;

	switch (cmd) {
	/* PCI config read/write passthrough. */
	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(psc->sc_pc, psc->sc_pcitag,
		    cmd, data, flag, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(vc->softc->sc_dev,
		    psc->sc_pc, psc->sc_pcitag, data);

	default:
		return EPASSTHROUGH;
	}
}

static paddr_t
vga_pci_mmap(void *v, off_t offset, int prot)
{
	struct vga_config *vc = v;
	struct vga_pci_softc *psc = (void *) vc->softc;
	struct vga_bar *vb;
	int bar;

	for (bar = 0; bar < NBARS; bar++) {
		vb = &psc->sc_bars[bar];
		if (vb->vb_size == 0)
			continue;
		if (offset >= vb->vb_base &&
		    offset < (vb->vb_base + vb->vb_size)) {
			/* XXX This the right thing to do with flags? */
			return (bus_space_mmap(vc->hdl.vh_memt, vb->vb_base,
			    (offset - vb->vb_base), prot, vb->vb_flags));
		}
	}

	/* XXX Expansion ROM? */

	/*
	 * Allow mmap access to the legacy ISA hole.  This is where
	 * the legacy video BIOS will be located, and also where
	 * the legacy VGA display buffer is located.
	 *
	 * XXX Security implications, here?
	 */
	if (offset >= IOM_BEGIN && offset < IOM_END)
		return (bus_space_mmap(vc->hdl.vh_memt, IOM_BEGIN,
		    (offset - IOM_BEGIN), prot, 0));

#ifdef PCI_MAGIC_IO_RANGE
	/* allow to map our IO space on non-x86 machines */
	if ((offset >= PCI_MAGIC_IO_RANGE) &&
	    (offset < PCI_MAGIC_IO_RANGE + 0x10000)) {
		return bus_space_mmap(vc->hdl.vh_iot,
		    offset - PCI_MAGIC_IO_RANGE,
		    0, prot, BUS_SPACE_MAP_LINEAR);	
	}
#endif
	
	/* Range not found. */
	return (-1);
}
