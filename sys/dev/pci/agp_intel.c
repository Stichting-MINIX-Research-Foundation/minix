/*	$NetBSD: agp_intel.c,v 1.37 2011/04/04 20:37:56 dyoung Exp $	*/

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
 *	$FreeBSD: src/sys/pci/agp_intel.c,v 1.4 2001/07/05 21:28:47 jhb Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agp_intel.c,v 1.37 2011/04/04 20:37:56 dyoung Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/agpio.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>

#include <sys/bus.h>

struct agp_intel_softc {
	u_int32_t		initial_aperture;
					/* aperture size at startup */
	struct agp_gatt		*gatt;
	struct pci_attach_args	vga_pa;
	u_int			aperture_mask;
	int			chiptype; /* Chip type */
#define	CHIP_INTEL	0x0
#define	CHIP_I443	0x1
#define	CHIP_I840	0x2
#define	CHIP_I845	0x3
#define	CHIP_I850	0x4
#define	CHIP_I865	0x5

};

static u_int32_t agp_intel_get_aperture(struct agp_softc *);
static int agp_intel_set_aperture(struct agp_softc *, u_int32_t);
static int agp_intel_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_intel_unbind_page(struct agp_softc *, off_t);
static void agp_intel_flush_tlb(struct agp_softc *);
static int agp_intel_init(struct agp_softc *);
static bool agp_intel_resume(device_t, const pmf_qual_t *);

static struct agp_methods agp_intel_methods = {
	agp_intel_get_aperture,
	agp_intel_set_aperture,
	agp_intel_bind_page,
	agp_intel_unbind_page,
	agp_intel_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};

static int
agp_intel_vgamatch(const struct pci_attach_args *pa)
{
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82855GM_AGP:
	case PCI_PRODUCT_INTEL_82855PM_AGP:
	case PCI_PRODUCT_INTEL_82443LX_AGP:
	case PCI_PRODUCT_INTEL_82443BX_AGP:
	case PCI_PRODUCT_INTEL_82443GX_AGP:
	case PCI_PRODUCT_INTEL_82850_AGP:	/* i850/i860 */
	case PCI_PRODUCT_INTEL_82845_AGP:
	case PCI_PRODUCT_INTEL_82840_AGP:
	case PCI_PRODUCT_INTEL_82865_AGP:
	case PCI_PRODUCT_INTEL_82875P_AGP:
		return (1);
	}

	return (0);
}

int
agp_intel_attach(device_t parent, device_t self, void *aux)
{
	struct agp_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct agp_intel_softc *isc;
	struct agp_gatt *gatt;
	u_int32_t value;

	isc = malloc(sizeof *isc, M_AGP, M_NOWAIT|M_ZERO);
	if (isc == NULL) {
		aprint_error(": can't allocate chipset-specific softc\n");
		return ENOMEM;
	}

	sc->as_methods = &agp_intel_methods;
	sc->as_chipc = isc;

	if (pci_find_device(&isc->vga_pa, agp_intel_vgamatch) == 0) {
		aprint_normal(": using generic initialization for Intel AGP\n");
		aprint_normal_dev(sc->as_dev, "");
		isc->chiptype = CHIP_INTEL;
	}

	pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, &sc->as_capoff,
	    NULL);

	if (agp_map_aperture(pa, sc, AGP_APBASE) != 0) {
		aprint_error(": can't map aperture\n");
		free(isc, M_AGP);
		sc->as_chipc = NULL;
		return ENXIO;
	}

	switch (PCI_PRODUCT(isc->vga_pa.pa_id)) {
	case PCI_PRODUCT_INTEL_82443LX_AGP:
	case PCI_PRODUCT_INTEL_82443BX_AGP:
	case PCI_PRODUCT_INTEL_82443GX_AGP:
		isc->chiptype = CHIP_I443;
		break;
	case PCI_PRODUCT_INTEL_82840_AGP:
		isc->chiptype = CHIP_I840;
		break;
	case PCI_PRODUCT_INTEL_82855GM_AGP:
	case PCI_PRODUCT_INTEL_82855PM_AGP:
	case PCI_PRODUCT_INTEL_82845_AGP:
		isc->chiptype = CHIP_I845;
		break;
	case PCI_PRODUCT_INTEL_82850_AGP:
		isc->chiptype = CHIP_I850;
		break;
	case PCI_PRODUCT_INTEL_82865_AGP:
	case PCI_PRODUCT_INTEL_82875P_AGP:
		isc->chiptype = CHIP_I865;
		break;
	}

	/* Determine maximum supported aperture size. */
	value = pci_conf_read(sc->as_pc, sc->as_tag, AGP_INTEL_APSIZE);
	pci_conf_write(sc->as_pc, sc->as_tag,
		AGP_INTEL_APSIZE, APSIZE_MASK);
	isc->aperture_mask = pci_conf_read(sc->as_pc, sc->as_tag,
		AGP_INTEL_APSIZE) & APSIZE_MASK;
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_APSIZE, value);
	isc->initial_aperture = AGP_GET_APERTURE(sc);

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
			aprint_error(": failed to set aperture\n");
			return ENOMEM;
		}
	}
	isc->gatt = gatt;

	if (!pmf_device_register(self, NULL, agp_intel_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return agp_intel_init(sc);
}

static int
agp_intel_init(struct agp_softc *sc)
{
	struct agp_intel_softc *isc = sc->as_chipc;
	struct agp_gatt *gatt = isc->gatt;
	pcireg_t reg;

	/* Install the gatt. */
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_ATTBASE,
	    gatt->ag_physical);

	/* Enable the GLTB and setup the control register. */
	switch (isc->chiptype) {
	case CHIP_I443:
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL,
		    AGPCTRL_AGPRSE | AGPCTRL_GTLB);

	default:
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL,
		    pci_conf_read(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL)
			| AGPCTRL_GTLB);
	}

	/* Enable things, clear errors etc. */
	switch (isc->chiptype) {
	case CHIP_I845:
	case CHIP_I865:
		{
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_I840_MCHCFG);
		reg |= MCHCFG_AAGN;
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_I840_MCHCFG, reg);
		break;
		}
	case CHIP_I840:
	case CHIP_I850:
		{
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCMD);
		reg |= AGPCMD_AGPEN;
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCMD,
			reg);
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_I840_MCHCFG);
		reg |= MCHCFG_AAGN;
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_I840_MCHCFG,
			reg);
		break;
		}
	default:
		{
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_INTEL_NBXCFG);
		reg &= ~NBXCFG_APAE;
		reg |=  NBXCFG_AAGN;
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_NBXCFG, reg);
		}
	}

	/* Clear Error status */
	switch (isc->chiptype) {
	case CHIP_I840:
		pci_conf_write(sc->as_pc, sc->as_tag,
			AGP_INTEL_I8XX_ERRSTS, 0xc000);
		break;

	case CHIP_I845:
	case CHIP_I850:
	case CHIP_I865:
		pci_conf_write(sc->as_pc, sc->as_tag,
			AGP_INTEL_I8XX_ERRSTS, 0x00ff);
		break;

	default:
		{
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_INTEL_ERRSTS);
		/* clear error bits (write-one-to-clear) - just write back */
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_ERRSTS, reg);
		}
	}

	return (0);
}

#if 0
static int
agp_intel_detach(struct agp_softc *sc)
{
	int error;
	pcireg_t reg;
	struct agp_intel_softc *isc = sc->as_chipc;

	error = agp_generic_detach(sc);
	if (error)
		return error;

	/* XXX i845/i855PM/i840/i850E */
	reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_INTEL_NBXCFG);
	reg &= ~(1 << 9);
	printf("%s: set NBXCFG to %x\n", __func__, reg);
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_NBXCFG, reg);
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_ATTBASE, 0);
	AGP_SET_APERTURE(sc, isc->initial_aperture);
	agp_free_gatt(sc, isc->gatt);

	return 0;
}
#endif

static u_int32_t
agp_intel_get_aperture(struct agp_softc *sc)
{
	struct agp_intel_softc *isc = sc->as_chipc;
	u_int32_t apsize;

	apsize = pci_conf_read(sc->as_pc, sc->as_tag,
			AGP_INTEL_APSIZE) & isc->aperture_mask;

	/*
	 * The size is determined by the number of low bits of
	 * register APBASE which are forced to zero. The low 22 bits
	 * are always forced to zero and each zero bit in the apsize
	 * field just read forces the corresponding bit in the 27:22
	 * to be zero. We calculate the aperture size accordingly.
	 */
	return (((apsize ^ isc->aperture_mask) << 22) | ((1 << 22) - 1)) + 1;
}

static int
agp_intel_set_aperture(struct agp_softc *sc, u_int32_t aperture)
{
	struct agp_intel_softc *isc = sc->as_chipc;
	u_int32_t apsize;

	/*
	 * Reverse the magic from get_aperture.
	 */
	apsize = ((aperture - 1) >> 22) ^ isc->aperture_mask;

	/*
	 * Double check for sanity.
	 */
	if ((((apsize ^ isc->aperture_mask) << 22) |
			((1 << 22) - 1)) + 1 != aperture)
		return EINVAL;

	pci_conf_write(sc->as_pc, sc->as_tag,
		AGP_INTEL_APSIZE, apsize);

	return 0;
}

static int
agp_intel_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_intel_softc *isc = sc->as_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	isc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical | 0x17;
	return 0;
}

static int
agp_intel_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_intel_softc *isc = sc->as_chipc;

	if (offset < 0 || offset >= (isc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	isc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return 0;
}

static void
agp_intel_flush_tlb(struct agp_softc *sc)
{
	struct agp_intel_softc *isc = sc->as_chipc;
	pcireg_t reg;

	switch (isc->chiptype) {
	case CHIP_I865:
	case CHIP_I850:
	case CHIP_I845:
	case CHIP_I840:
	case CHIP_I443:
		{
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL);
		reg &= ~AGPCTRL_GTLB;
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL,
			reg);
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL,
			reg | AGPCTRL_GTLB);
		break;
		}
	default: /* XXX */
		{
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL,
			0x2200);
		pci_conf_write(sc->as_pc, sc->as_tag, AGP_INTEL_AGPCTRL,
			0x2280);
		}
	}
}

static bool
agp_intel_resume(device_t dv, const pmf_qual_t *qual)
{
	struct agp_softc *sc = device_private(dv);

	agp_intel_init(sc);
	agp_flush_cache();

	return true;
}
