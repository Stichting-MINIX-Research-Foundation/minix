/*	$NetBSD: agp_via.c,v 1.21 2011/02/19 20:07:02 jmcneill Exp $	*/

/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 *	$FreeBSD: src/sys/pci/agp_via.c,v 1.3 2001/07/05 21:28:47 jhb Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agp_via.c,v 1.21 2011/02/19 20:07:02 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/agpio.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>
#include <dev/pci/pcidevs.h>

#include <sys/bus.h>

static u_int32_t agp_via_get_aperture(struct agp_softc *);
static int agp_via_set_aperture(struct agp_softc *, u_int32_t);
static int agp_via_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_via_unbind_page(struct agp_softc *, off_t);
static void agp_via_flush_tlb(struct agp_softc *);

static struct agp_methods agp_via_methods = {
	agp_via_get_aperture,
	agp_via_set_aperture,
	agp_via_bind_page,
	agp_via_unbind_page,
	agp_via_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};

struct agp_via_softc {
	u_int32_t	initial_aperture; /* aperture size at startup */
	struct agp_gatt *gatt;
	int		*regs;
};

#define REG_GARTCTRL	0
#define REG_APSIZE	1
#define REG_ATTBASE	2

static int via_v2_regs[] =
	{ AGP_VIA_GARTCTRL, AGP_VIA_APSIZE, AGP_VIA_ATTBASE };
static int via_v3_regs[] =
	{ AGP3_VIA_GARTCTRL, AGP3_VIA_APSIZE, AGP3_VIA_ATTBASE };

int
agp_via_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct agp_softc *sc = device_private(self);
	struct agp_via_softc *asc;
	struct agp_gatt *gatt;
	pcireg_t agpsel, capval;

	asc = malloc(sizeof *asc, M_AGP, M_NOWAIT|M_ZERO);
	if (asc == NULL) {
		aprint_error(": can't allocate chipset-specific softc\n");
		return ENOMEM;
	}
	sc->as_chipc = asc;
	sc->as_methods = &agp_via_methods;
	pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, &sc->as_capoff,
	    &capval);

	if (PCI_CAP_AGP_MAJOR(capval) >= 3) {
		agpsel = pci_conf_read(pa->pa_pc, pa->pa_tag, AGP_VIA_AGPSEL);
		if ((agpsel & (1 << 9)) == 0) {
			asc->regs = via_v3_regs;
			aprint_debug(" (v3)");
		} else {
			asc->regs = via_v2_regs;
			aprint_debug(" (v2 compat mode)");
		}
	} else {
		asc->regs = via_v2_regs;
		aprint_debug(" (v2)");
	}

	if (agp_map_aperture(pa, sc, AGP_APBASE) != 0) {
		aprint_error(": can't map aperture\n");
		free(asc, M_AGP);
		return ENXIO;
	}

	asc->initial_aperture = AGP_GET_APERTURE(sc);

	for (;;) {
		gatt = agp_alloc_gatt(sc);
		if (gatt)
			break;

		/*
		 * Probably contigmalloc failure. Try reducing the
		 * aperture so that the gatt size reduces.
		 */
		if (AGP_SET_APERTURE(sc, AGP_GET_APERTURE(sc) / 2)) {
			agp_generic_detach(sc);
			aprint_error(": can't set aperture size\n");
			return ENOMEM;
		}
	}
	asc->gatt = gatt;

	if (asc->regs == via_v2_regs) {
		/* Install the gatt. */
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_ATTBASE],
				 gatt->ag_physical | 3);
		/* Enable the aperture. */
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_GARTCTRL],
				 0x0000000f);
	} else {
		pcireg_t gartctrl;
		/* Install the gatt. */
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_ATTBASE],
				 gatt->ag_physical);
		/* Enable the aperture. */
		gartctrl = pci_conf_read(pa->pa_pc, pa->pa_tag,
				 asc->regs[REG_GARTCTRL]);
		pci_conf_write(pa->pa_pc, pa->pa_tag, asc->regs[REG_GARTCTRL],
				 gartctrl | (3 << 7));
	}

	return 0;
}

#if 0
static int
agp_via_detach(struct agp_softc *sc)
{
	struct agp_via_softc *asc = sc->as_chipc;
	int error;

	error = agp_generic_detach(sc);
	if (error)
		return error;

	pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_GARTCTRL], 0);
	pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_ATTBASE], 0);
	AGP_SET_APERTURE(sc, asc->initial_aperture);
	agp_free_gatt(sc, asc->gatt);

	return 0;
}
#endif

static u_int32_t
agp_via_get_aperture(struct agp_softc *sc)
{
	struct agp_via_softc *asc = sc->as_chipc;
	u_int32_t apsize;

	if (asc->regs == via_v2_regs) {
		apsize = pci_conf_read(sc->as_pc, sc->as_tag,
				asc->regs[REG_APSIZE]) & 0xff;

		/*
		 * The size is determined by the number of low bits of
		 * register APBASE which are forced to zero. The low 20 bits
		 * are always forced to zero and each zero bit in the apsize
		 * field just read forces the corresponding bit in the 27:20
		 * to be zero. We calculate the aperture size accordingly.
		 */
		return (((apsize ^ 0xff) << 20) | ((1 << 20) - 1)) + 1;
	} else {
		apsize = pci_conf_read(sc->as_pc, sc->as_tag,
				asc->regs[REG_APSIZE]) & 0xfff;
		switch (apsize) {
		case 0x800:
			return 0x80000000;
		case 0xc00:
			return 0x40000000;
		case 0xe00:
			return 0x20000000;
		case 0xf00:
			return 0x10000000;
		case 0xf20:
			return 0x08000000;
		case 0xf30:
			return 0x04000000;
		case 0xf38:
			return 0x02000000;
		case 0xf3c:
			return 0x01000000;
		case 0xf3e:
			return 0x00800000;
		case 0xf3f:
			return 0x00400000;
		default:
			aprint_error_dev(sc->as_dev,
			    "invalid aperture setting 0x%x\n", apsize);
			return 0;
		}
	}
}

static int
agp_via_set_aperture(struct agp_softc *sc, u_int32_t aperture)
{
	struct agp_via_softc *asc = sc->as_chipc;
	u_int32_t apsize, key;
	pcireg_t reg;

	if (asc->regs == via_v2_regs) {
		/*
		 * Reverse the magic from get_aperture.
		 */
		apsize = ((aperture - 1) >> 20) ^ 0xff;

		/*
		 * Double check for sanity.
		 */
		if ((((apsize ^ 0xff) << 20) | ((1 << 20) - 1)) + 1 != aperture)
			return EINVAL;

		reg = pci_conf_read(sc->as_pc, sc->as_tag,
		    asc->regs[REG_APSIZE]);
		reg &= ~0xff;
		reg |= apsize;
		pci_conf_write(sc->as_pc, sc->as_tag,
		    asc->regs[REG_APSIZE], reg);
	} else {
		switch (aperture) {
		case 0x80000000:
			key = 0x800;
			break;
		case 0x40000000:
			key = 0xc00;
			break;
		case 0x20000000:
			key = 0xe00;
			break;
		case 0x10000000:
			key = 0xf00;
			break;
		case 0x08000000:
			key = 0xf20;
			break;
		case 0x04000000:
			key = 0xf30;
			break;
		case 0x02000000:
			key = 0xf38;
			break;
		case 0x01000000:
			key = 0xf3c;
			break;
		case 0x00800000:
			key = 0xf3e;
			break;
		case 0x00400000:
			key = 0xf3f;
			break;
		default:
			aprint_error_dev(sc->as_dev,
			    "invalid aperture size (%dMB)\n",
			    aperture / 1024 / 1024);
			return EINVAL;
		}
		reg = pci_conf_read(sc->as_pc, sc->as_tag, asc->regs[REG_APSIZE]);
		reg &= ~0xfff;
		reg |= key;
		pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_APSIZE], reg);
	}

	return 0;
}

static int
agp_via_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_via_softc *asc = sc->as_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical;
	return 0;
}

static int
agp_via_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_via_softc *asc = sc->as_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return 0;
}

static void
agp_via_flush_tlb(struct agp_softc *sc)
{
	struct agp_via_softc *asc = sc->as_chipc;
	pcireg_t gartctrl;

	if (asc->regs == via_v2_regs) {
		pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_GARTCTRL],
				0x8f);
		pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_GARTCTRL],
				0x0f);
	} else {
		gartctrl = pci_conf_read(sc->as_pc, sc->as_tag,
					 asc->regs[REG_GARTCTRL]);
		pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_GARTCTRL],
			       gartctrl & ~(1 << 7));
		pci_conf_write(sc->as_pc, sc->as_tag, asc->regs[REG_GARTCTRL],
			       gartctrl);
	}
}
