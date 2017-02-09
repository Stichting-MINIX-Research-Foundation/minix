/*	$NetBSD: agp_i810.c,v 1.118 2015/04/05 12:55:20 riastradh Exp $	*/

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
 *
 *	$FreeBSD$
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agp_i810.c,v 1.118 2015/04/05 12:55:20 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/xcall.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>
#include <dev/pci/agp_i810var.h>

#include <sys/agpio.h>

#include <sys/bus.h>

#include "agp_intel.h"

#ifdef AGP_DEBUG
#define	DPRINTF(sc, fmt, ...)						      \
	device_printf((sc)->as_dev, "%s: " fmt, __func__, ##__VA_ARGS__)
#else
#define	DPRINTF(sc, fmt, ...)	do {} while (0)
#endif

struct agp_softc *agp_i810_sc = NULL;

#define READ1(off)	bus_space_read_1(isc->bst, isc->bsh, off)
#define READ4(off)	bus_space_read_4(isc->bst, isc->bsh, off)
#define WRITE4(off,v)	bus_space_write_4(isc->bst, isc->bsh, off, v)

#define CHIP_I810 0	/* i810/i815 */
#define CHIP_I830 1	/* 830M/845G */
#define CHIP_I855 2	/* 852GM/855GM/865G */
#define CHIP_I915 3	/* 915G/915GM/945G/945GM/945GME */
#define CHIP_I965 4	/* 965Q/965PM */
#define CHIP_G33  5	/* G33/Q33/Q35 */
#define CHIP_G4X  6	/* G45/Q45 */

/* XXX hack, see below */
static bus_addr_t agp_i810_vga_regbase;
static bus_size_t agp_i810_vga_regsize;
static bus_space_tag_t agp_i810_vga_bst;
static bus_space_handle_t agp_i810_vga_bsh;

static u_int32_t agp_i810_get_aperture(struct agp_softc *);
static int agp_i810_set_aperture(struct agp_softc *, u_int32_t);
static int agp_i810_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_i810_unbind_page(struct agp_softc *, off_t);
static void agp_i810_flush_tlb(struct agp_softc *);
static int agp_i810_enable(struct agp_softc *, u_int32_t mode);
static struct agp_memory *agp_i810_alloc_memory(struct agp_softc *, int,
						vsize_t);
static int agp_i810_free_memory(struct agp_softc *, struct agp_memory *);
static int agp_i810_bind_memory(struct agp_softc *, struct agp_memory *,
		off_t);
static int agp_i810_bind_memory_dcache(struct agp_softc *, struct agp_memory *,
		off_t);
static int agp_i810_bind_memory_hwcursor(struct agp_softc *,
		struct agp_memory *, off_t);
static int agp_i810_unbind_memory(struct agp_softc *, struct agp_memory *);

static bool agp_i810_resume(device_t, const pmf_qual_t *);
static int agp_i810_init(struct agp_softc *);

static int agp_i810_setup_chipset_flush_page(struct agp_softc *);
static void agp_i810_teardown_chipset_flush_page(struct agp_softc *);
static int agp_i810_init(struct agp_softc *);

static struct agp_methods agp_i810_methods = {
	agp_i810_get_aperture,
	agp_i810_set_aperture,
	agp_i810_bind_page,
	agp_i810_unbind_page,
	agp_i810_flush_tlb,
	agp_i810_enable,
	agp_i810_alloc_memory,
	agp_i810_free_memory,
	agp_i810_bind_memory,
	agp_i810_unbind_memory,
};

int
agp_i810_write_gtt_entry(struct agp_i810_softc *isc, off_t off,
    bus_addr_t addr, int flags)
{
	u_int32_t pte;

	/*
	 * Bits 11:4 (physical start address extension) should be zero.
	 * Flag bits 3:0 should be zero too.
	 *
	 * XXX This should be a kassert -- no reason for this routine
	 * to allow failure.
	 */
	if ((addr & 0xfff) != 0)
		return EINVAL;
	KASSERT(flags == (flags & 0x7));

	pte = (u_int32_t)addr;
	/*
	 * We need to massage the pte if bus_addr_t is wider than 32 bits.
	 * The compiler isn't smart enough, hence the casts to uintmax_t.
	 */
	if (sizeof(bus_addr_t) > sizeof(u_int32_t)) {
		/* 965+ can do 36-bit addressing, add in the extra bits. */
		if (isc->chiptype == CHIP_I965 ||
		    isc->chiptype == CHIP_G33 ||
		    isc->chiptype == CHIP_G4X) {
			if (((uintmax_t)addr >> 36) != 0)
				return EINVAL;
			pte |= (addr >> 28) & 0xf0;
		} else {
			if (((uintmax_t)addr >> 32) != 0)
				return EINVAL;
		}
	}

	bus_space_write_4(isc->gtt_bst, isc->gtt_bsh,
	    4*(off >> AGP_PAGE_SHIFT), pte | flags);

	return 0;
}

void
agp_i810_post_gtt_entry(struct agp_i810_softc *isc, off_t off)
{

	/*
	 * See <https://bugs.freedesktop.org/show_bug.cgi?id=88191>.
	 * Out of paranoia, let's do the write barrier and posting
	 * read, because I don't have enough time or hardware to
	 * conduct conclusive tests.
	 */
	membar_producer();
	(void)bus_space_read_4(isc->gtt_bst, isc->gtt_bsh,
	    4*(off >> AGP_PAGE_SHIFT));
}

static void
agp_flush_cache_xc(void *a __unused, void *b __unused)
{

	agp_flush_cache();
}

void
agp_i810_chipset_flush(struct agp_i810_softc *isc)
{
	unsigned int timo = 20000; /* * 50 us = 1 s */

	switch (isc->chiptype) {
	case CHIP_I810:
		break;
	case CHIP_I830:
	case CHIP_I855:
		/*
		 * Flush all CPU caches.  If we're cold, we can't run
		 * xcalls, but there should be only one CPU up, so
		 * flushing only the local CPU's cache should suffice.
		 *
		 * XXX Come to think of it, do these chipsets appear in
		 * any multi-CPU systems?
		 */
		if (cold)
			agp_flush_cache();
		else
			xc_wait(xc_broadcast(0, &agp_flush_cache_xc,
				NULL, NULL));
		WRITE4(AGP_I830_HIC, READ4(AGP_I830_HIC) | __BIT(31));
		while (ISSET(READ4(AGP_I830_HIC), __BIT(31))) {
			if (timo-- == 0)
				break;
			DELAY(50);
		}
		break;
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
	case CHIP_G4X:
		bus_space_write_4(isc->flush_bst, isc->flush_bsh, 0, 1);
		break;
	}
}

/* XXXthorpej -- duplicated code (see arch/x86/pci/pchb.c) */
static int
agp_i810_vgamatch(const struct pci_attach_args *pa)
{

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82810_GC:
	case PCI_PRODUCT_INTEL_82810_DC100_GC:
	case PCI_PRODUCT_INTEL_82810E_GC:
	case PCI_PRODUCT_INTEL_82815_FULL_GRAPH:
	case PCI_PRODUCT_INTEL_82830MP_IV:
	case PCI_PRODUCT_INTEL_82845G_IGD:
	case PCI_PRODUCT_INTEL_82855GM_IGD:
	case PCI_PRODUCT_INTEL_82865_IGD:
	case PCI_PRODUCT_INTEL_82915G_IGD:
	case PCI_PRODUCT_INTEL_82915GM_IGD:
	case PCI_PRODUCT_INTEL_82945P_IGD:
	case PCI_PRODUCT_INTEL_82945GM_IGD:
	case PCI_PRODUCT_INTEL_82945GM_IGD_1:
	case PCI_PRODUCT_INTEL_82945GME_IGD:
	case PCI_PRODUCT_INTEL_E7221_IGD:
	case PCI_PRODUCT_INTEL_82965Q_IGD:
	case PCI_PRODUCT_INTEL_82965Q_IGD_1:
	case PCI_PRODUCT_INTEL_82965PM_IGD:
	case PCI_PRODUCT_INTEL_82965PM_IGD_1:
	case PCI_PRODUCT_INTEL_82G33_IGD:
	case PCI_PRODUCT_INTEL_82G33_IGD_1:
	case PCI_PRODUCT_INTEL_82965G_IGD:
	case PCI_PRODUCT_INTEL_82965G_IGD_1:
	case PCI_PRODUCT_INTEL_82965GME_IGD:
	case PCI_PRODUCT_INTEL_82Q35_IGD:
	case PCI_PRODUCT_INTEL_82Q35_IGD_1:
	case PCI_PRODUCT_INTEL_82Q33_IGD:
	case PCI_PRODUCT_INTEL_82Q33_IGD_1:
	case PCI_PRODUCT_INTEL_82G35_IGD:
	case PCI_PRODUCT_INTEL_82G35_IGD_1:
	case PCI_PRODUCT_INTEL_82946GZ_IGD:
	case PCI_PRODUCT_INTEL_82GM45_IGD:
	case PCI_PRODUCT_INTEL_82GM45_IGD_1:
	case PCI_PRODUCT_INTEL_82IGD_E_IGD:
	case PCI_PRODUCT_INTEL_82Q45_IGD:
	case PCI_PRODUCT_INTEL_82G45_IGD:
	case PCI_PRODUCT_INTEL_82G41_IGD:
	case PCI_PRODUCT_INTEL_82B43_IGD:
	case PCI_PRODUCT_INTEL_IRONLAKE_D_IGD:
	case PCI_PRODUCT_INTEL_IRONLAKE_M_IGD:
	case PCI_PRODUCT_INTEL_PINEVIEW_IGD:
	case PCI_PRODUCT_INTEL_PINEVIEW_M_IGD:
		return (1);
	}

	return (0);
}

static int
agp_i965_map_aperture(struct pci_attach_args *pa, struct agp_softc *sc, int reg)
{
        /*
         * Find the aperture. Don't map it (yet), this would
         * eat KVA.
         */
        if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, reg,
            PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_64BIT, &sc->as_apaddr, &sc->as_apsize,
            &sc->as_apflags) != 0)
                return ENXIO;

        sc->as_apt = pa->pa_memt;

        return 0;
}

int
agp_i810_attach(device_t parent, device_t self, void *aux)
{
	struct agp_softc *sc = device_private(self);
	struct agp_i810_softc *isc;
	int apbase, mmadr_bar, gtt_bar;
	int mmadr_type, mmadr_flags;
	bus_addr_t mmadr;
	bus_size_t mmadr_size, gtt_off;
	int error;

	isc = malloc(sizeof *isc, M_AGP, M_NOWAIT|M_ZERO);
	if (isc == NULL) {
		aprint_error(": can't allocate chipset-specific softc\n");
		error = ENOMEM;
		goto fail0;
	}
	sc->as_chipc = isc;
	sc->as_methods = &agp_i810_methods;

	if (pci_find_device(&isc->vga_pa, agp_i810_vgamatch) == 0) {
#if NAGP_INTEL > 0
		const struct pci_attach_args *pa = aux;

		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_INTEL_82840_HB:
		case PCI_PRODUCT_INTEL_82865_HB:
		case PCI_PRODUCT_INTEL_82845G_DRAM:
		case PCI_PRODUCT_INTEL_82815_FULL_HUB:
		case PCI_PRODUCT_INTEL_82855GM_MCH:
			free(isc, M_AGP);
			return agp_intel_attach(parent, self, aux);
		}
#endif
		aprint_error(": can't find internal VGA"
		    " config space\n");
		error = ENOENT;
		goto fail1;
	}

	/* XXXfvdl */
	sc->as_dmat = isc->vga_pa.pa_dmat;

	switch (PCI_PRODUCT(isc->vga_pa.pa_id)) {
	case PCI_PRODUCT_INTEL_82810_GC:
	case PCI_PRODUCT_INTEL_82810_DC100_GC:
	case PCI_PRODUCT_INTEL_82810E_GC:
	case PCI_PRODUCT_INTEL_82815_FULL_GRAPH:
		isc->chiptype = CHIP_I810;
		aprint_normal(": i810-family chipset\n");
		break;
	case PCI_PRODUCT_INTEL_82830MP_IV:
	case PCI_PRODUCT_INTEL_82845G_IGD:
		isc->chiptype = CHIP_I830;
		aprint_normal(": i830-family chipset\n");
		break;
	case PCI_PRODUCT_INTEL_82855GM_IGD:
	case PCI_PRODUCT_INTEL_82865_IGD:
		isc->chiptype = CHIP_I855;
		aprint_normal(": i855-family chipset\n");
		break;
	case PCI_PRODUCT_INTEL_82915G_IGD:
	case PCI_PRODUCT_INTEL_82915GM_IGD:
	case PCI_PRODUCT_INTEL_82945P_IGD:
	case PCI_PRODUCT_INTEL_82945GM_IGD:
	case PCI_PRODUCT_INTEL_82945GM_IGD_1:
	case PCI_PRODUCT_INTEL_82945GME_IGD:
	case PCI_PRODUCT_INTEL_E7221_IGD:
	case PCI_PRODUCT_INTEL_PINEVIEW_IGD:
	case PCI_PRODUCT_INTEL_PINEVIEW_M_IGD:
		isc->chiptype = CHIP_I915;
		aprint_normal(": i915-family chipset\n");
		break;
	case PCI_PRODUCT_INTEL_82965Q_IGD:
	case PCI_PRODUCT_INTEL_82965Q_IGD_1:
	case PCI_PRODUCT_INTEL_82965PM_IGD:
	case PCI_PRODUCT_INTEL_82965PM_IGD_1:
	case PCI_PRODUCT_INTEL_82965G_IGD:
	case PCI_PRODUCT_INTEL_82965G_IGD_1:
	case PCI_PRODUCT_INTEL_82965GME_IGD:
	case PCI_PRODUCT_INTEL_82946GZ_IGD:
	case PCI_PRODUCT_INTEL_82G35_IGD:
	case PCI_PRODUCT_INTEL_82G35_IGD_1:
		isc->chiptype = CHIP_I965;
		aprint_normal(": i965-family chipset\n");
		break;
	case PCI_PRODUCT_INTEL_82Q35_IGD:
	case PCI_PRODUCT_INTEL_82Q35_IGD_1:
	case PCI_PRODUCT_INTEL_82G33_IGD:
	case PCI_PRODUCT_INTEL_82G33_IGD_1:
	case PCI_PRODUCT_INTEL_82Q33_IGD:
	case PCI_PRODUCT_INTEL_82Q33_IGD_1:
		isc->chiptype = CHIP_G33;
		aprint_normal(": G33-family chipset\n");
		break;
	case PCI_PRODUCT_INTEL_82GM45_IGD:
	case PCI_PRODUCT_INTEL_82GM45_IGD_1:
	case PCI_PRODUCT_INTEL_82IGD_E_IGD:
	case PCI_PRODUCT_INTEL_82Q45_IGD:
	case PCI_PRODUCT_INTEL_82G45_IGD:
	case PCI_PRODUCT_INTEL_82G41_IGD:
	case PCI_PRODUCT_INTEL_82B43_IGD:
	case PCI_PRODUCT_INTEL_IRONLAKE_D_IGD:
	case PCI_PRODUCT_INTEL_IRONLAKE_M_IGD:
		isc->chiptype = CHIP_G4X;
		aprint_normal(": G4X-family chipset\n");
		break;
	}
	aprint_naive("\n");

	mmadr_type = PCI_MAPREG_TYPE_MEM;
	switch (isc->chiptype) {
	case CHIP_I915:
	case CHIP_G33:
		apbase = AGP_I915_GMADR;
		mmadr_bar = AGP_I915_MMADR;
		isc->size = 512*1024;
		gtt_bar = AGP_I915_GTTADR;
		gtt_off = ~(bus_size_t)0; /* XXXGCC */
		break;
	case CHIP_I965:
		apbase = AGP_I965_GMADR;
		mmadr_bar = AGP_I965_MMADR;
		mmadr_type |= PCI_MAPREG_MEM_TYPE_64BIT;
		if (pci_mapreg_info(isc->vga_pa.pa_pc, isc->vga_pa.pa_tag,
			AGP_I965_MMADR, mmadr_type, NULL, &isc->size, NULL))
			isc->size = 512*1024; /* XXX */
		gtt_bar = 0;
		gtt_off = AGP_I965_GTT;
		break;
	case CHIP_G4X:
		apbase = AGP_I965_GMADR;
		mmadr_bar = AGP_I965_MMADR;
		mmadr_type |= PCI_MAPREG_MEM_TYPE_64BIT;
		if (pci_mapreg_info(isc->vga_pa.pa_pc, isc->vga_pa.pa_tag,
			AGP_I965_MMADR, mmadr_type, NULL, &isc->size, NULL))
			isc->size = 512*1024; /* XXX */
		gtt_bar = 0;
		gtt_off = AGP_G4X_GTT;
		break;
	default:
		apbase = AGP_I810_GMADR;
		mmadr_bar = AGP_I810_MMADR;
		if (pci_mapreg_info(isc->vga_pa.pa_pc, isc->vga_pa.pa_tag,
			AGP_I810_MMADR, mmadr_type, NULL, &isc->size, NULL))
			isc->size = 512*1024; /* XXX */
		gtt_bar = 0;
		gtt_off = AGP_I810_GTT;
		break;
	}

	/* Map (or, rather, find the address and size of) the aperture.  */
	if (isc->chiptype == CHIP_I965 || isc->chiptype == CHIP_G4X)
		error = agp_i965_map_aperture(&isc->vga_pa, sc, apbase);
	else
		error = agp_map_aperture(&isc->vga_pa, sc, apbase);
	if (error) {
		aprint_error_dev(self, "can't map aperture: %d\n", error);
		goto fail1;
	}

	/* Map the memory-mapped I/O registers, or the non-GTT part.  */
	if (pci_mapreg_info(isc->vga_pa.pa_pc, isc->vga_pa.pa_tag, mmadr_bar,
		mmadr_type, &mmadr, &mmadr_size, &mmadr_flags)) {
		aprint_error_dev(self, "can't find MMIO registers\n");
		error = ENXIO;
		goto fail1;
	}
	if (mmadr_size < isc->size) {
		aprint_error_dev(self, "MMIO registers too small"
		    ": %"PRIuMAX" < %"PRIuMAX"\n",
		    (uintmax_t)mmadr_size, (uintmax_t)isc->size);
		error = ENXIO;
		goto fail1;
	}
	isc->bst = isc->vga_pa.pa_memt;
	error = bus_space_map(isc->bst, mmadr, isc->size, mmadr_flags,
	    &isc->bsh);
	if (error) {
		aprint_error_dev(self, "can't map MMIO registers: %d\n",
		    error);
		error = ENXIO;
		goto fail1;
	}

	/* Set up a chipset flush page if necessary.  */
	switch (isc->chiptype) {
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
	case CHIP_G4X:
		error = agp_i810_setup_chipset_flush_page(sc);
		if (error) {
			aprint_error_dev(self,
			    "can't set up chipset flush page: %d\n", error);
			goto fail2;
		}
		break;
	}

	/*
	 * XXX horrible hack to allow drm code to use our mapping
	 * of VGA chip registers
	 */
	agp_i810_vga_regbase = mmadr;
	agp_i810_vga_regsize = isc->size;
	agp_i810_vga_bst = isc->bst;
	agp_i810_vga_bsh = isc->bsh;

	/* Initialize the chipset.  */
	error = agp_i810_init(sc);
	if (error)
		goto fail3;

	/* Map the GTT, from either part of the MMIO region or its own BAR.  */
	if (gtt_bar == 0) {
		isc->gtt_bst = isc->bst;
		if ((mmadr_size - gtt_off) < isc->gtt_size) {
			aprint_error_dev(self, "GTTMMADR too small for GTT"
			    ": (%"PRIxMAX" - %"PRIxMAX") < %"PRIxMAX"\n",
			    (uintmax_t)mmadr_size,
			    (uintmax_t)gtt_off,
			    (uintmax_t)isc->gtt_size);
			error = ENXIO;
			goto fail4;
		}
		/*
		 * Map the GTT separately if we can, so that we can map
		 * it prefetchable, but in early models, there are MMIO
		 * registers before and after the GTT, so we can only
		 * take a subregion.
		 */
		if (isc->size < gtt_off)
			error = bus_space_map(isc->gtt_bst, (mmadr + gtt_off),
			    isc->gtt_size, mmadr_flags, &isc->gtt_bsh);
		else
			error = bus_space_subregion(isc->bst, isc->bsh,
			    gtt_off, isc->gtt_size, &isc->gtt_bsh);
		if (error) {
			aprint_error_dev(self, "can't map GTT: %d\n", error);
			error = ENXIO;
			goto fail4;
		}
	} else {
		bus_size_t gtt_bar_size;
		/*
		 * All chipsets with a separate BAR for the GTT, namely
		 * the i915 and G33 families, have 32-bit GTT BARs.
		 *
		 * XXX [citation needed]
		 */
		if (pci_mapreg_map(&isc->vga_pa, gtt_bar, PCI_MAPREG_TYPE_MEM,
			0,
			&isc->gtt_bst, &isc->gtt_bsh, NULL, &gtt_bar_size)) {
			aprint_error_dev(self, "can't map GTT\n");
			error = ENXIO;
			goto fail4;
		}
		if (gtt_bar_size != isc->gtt_size) {
			aprint_error_dev(self,
			    "BAR size %"PRIxMAX
			    " mismatches detected GTT size %"PRIxMAX
			    "; trusting BAR\n",
			    (uintmax_t)gtt_bar_size,
			    (uintmax_t)isc->gtt_size);
			isc->gtt_size = gtt_bar_size;
		}
	}

	/* Power management.  (XXX Nothing to save on suspend?  Fishy...)  */
	if (!pmf_device_register(self, NULL, agp_i810_resume))
		aprint_error_dev(self, "can't establish power handler\n");

	/* Match the generic AGP code's autoconf output format.  */
	aprint_normal("%s", device_xname(self));

	/* Success!  */
	return 0;

fail5: __unused
	pmf_device_deregister(self);
	if ((gtt_bar != 0) || (isc->size < gtt_off))
		bus_space_unmap(isc->gtt_bst, isc->gtt_bsh, isc->gtt_size);
	isc->gtt_size = 0;
fail4:
#if notyet
	agp_i810_fini(sc);
#endif
fail3:	switch (isc->chiptype) {
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
	case CHIP_G4X:
		agp_i810_teardown_chipset_flush_page(sc);
		break;
	}
fail2:	bus_space_unmap(isc->bst, isc->bsh, isc->size);
	isc->size = 0;
fail1:	free(isc, M_AGP);
	sc->as_chipc = NULL;
fail0:	agp_generic_detach(sc);
	KASSERT(error);
	return error;
}

/*
 * Skip pages reserved by the BIOS.  Notably, skip 0xa0000-0xfffff,
 * which includes the video BIOS at 0xc0000-0xdffff which the display
 * drivers need for video mode detection.
 *
 * XXX Is there an MI name for this, or a conventional x86 name?  Or
 * should we really use bus_dma instead?
 */
#define	PCIBIOS_MIN_MEM		0x100000

static int
agp_i810_setup_chipset_flush_page(struct agp_softc *sc)
{
	struct agp_i810_softc *const isc = sc->as_chipc;
	const pci_chipset_tag_t pc = sc->as_pc;
	const pcitag_t tag = sc->as_tag;
	pcireg_t lo, hi;
	bus_addr_t addr, minaddr, maxaddr;
	int error;

	/* We always use memory-mapped I/O.  */
	isc->flush_bst = isc->vga_pa.pa_memt;

	/* No page allocated yet.  */
	isc->flush_addr = 0;

	/* Read the PCI config register: 4-byte on gen3, 8-byte on gen>=4.  */
	if (isc->chiptype == CHIP_I915) {
		addr = pci_conf_read(pc, tag, AGP_I915_IFPADDR);
		minaddr = PCIBIOS_MIN_MEM;
		maxaddr = UINT32_MAX;
	} else {
		hi = pci_conf_read(pc, tag, AGP_I965_IFPADDR+4);
		lo = pci_conf_read(pc, tag, AGP_I965_IFPADDR);
		/*
		 * Convert to uint64_t, rather than bus_addr_t which
		 * may be 32-bit, to avoid undefined behaviour with a
		 * too-wide shift.  Since the BIOS doesn't know whether
		 * the OS will run 64-bit or with PAE, it ought to
		 * configure at most a 32-bit physical address, so
		 * let's print a warning in case that happens.
		 */
		addr = ((uint64_t)hi << 32) | lo;
		if (hi) {
			aprint_error_dev(sc->as_dev,
			    "BIOS configured >32-bit flush page address"
			    ": %"PRIx64"\n", ((uint64_t)hi << 32) | lo);
#if __i386__ && !PAE
			return EIO;
#endif
		}
		minaddr = PCIBIOS_MIN_MEM;
		maxaddr = MIN(UINT64_MAX, ~(bus_addr_t)0);
	}

	/* Allocate or map a pre-allocated a page for it.  */
	if (ISSET(addr, 1)) {
		/* BIOS allocated it for us.  Use that.  */
		error = bus_space_map(isc->flush_bst, addr & ~1, PAGE_SIZE, 0,
		    &isc->flush_bsh);
		if (error)
			return error;
	} else {
		/* None allocated.  Allocate one.  */
		error = bus_space_alloc(isc->flush_bst, minaddr, maxaddr,
		    PAGE_SIZE, PAGE_SIZE, 0, 0,
		    &isc->flush_addr, &isc->flush_bsh);
		if (error)
			return error;
		KASSERT(isc->flush_addr != 0);
		/* Write it into the PCI config register.  */
		addr = isc->flush_addr | 1;
		if (isc->chiptype == CHIP_I915) {
			pci_conf_write(pc, tag, AGP_I915_IFPADDR, addr);
		} else {
			hi = __SHIFTOUT(addr, __BITS(63, 32));
			lo = __SHIFTOUT(addr, __BITS(31, 0));
			pci_conf_write(pc, tag, AGP_I965_IFPADDR+4, hi);
			pci_conf_write(pc, tag, AGP_I965_IFPADDR, lo);
		}
	}

	/* Success!  */
	return 0;
}

static void
agp_i810_teardown_chipset_flush_page(struct agp_softc *sc)
{
	struct agp_i810_softc *const isc = sc->as_chipc;

	if (isc->flush_addr) {
		/* If we allocated a page, clear it.  */
		if (isc->chiptype == CHIP_I915) {
			pci_conf_write(sc->as_pc, sc->as_tag, AGP_I915_IFPADDR,
			    0);
		} else {
			pci_conf_write(sc->as_pc, sc->as_tag,
			    AGP_I965_IFPADDR, 0);
			pci_conf_write(sc->as_pc, sc->as_tag,
			    AGP_I965_IFPADDR + 4, 0);
		}
		isc->flush_addr = 0;
		bus_space_free(isc->flush_bst, isc->flush_bsh, PAGE_SIZE);
	} else {
		/* Otherwise, just unmap the pre-allocated page.  */
		bus_space_unmap(isc->flush_bst, isc->flush_bsh, PAGE_SIZE);
	}
}

/*
 * XXX horrible hack to allow drm code to use our mapping
 * of VGA chip registers
 */
int
agp_i810_borrow(bus_addr_t base, bus_size_t size, bus_space_handle_t *hdlp)
{

	if (agp_i810_vga_regbase == 0)
		return 0;
	if (base < agp_i810_vga_regbase)
		return 0;
	if (agp_i810_vga_regsize < size)
		return 0;
	if ((base - agp_i810_vga_regbase) > (agp_i810_vga_regsize - size))
		return 0;
	if (bus_space_subregion(agp_i810_vga_bst, agp_i810_vga_bsh,
		(base - agp_i810_vga_regbase), (agp_i810_vga_regsize - size),
		hdlp))
		return 0;
	return 1;
}

static int
agp_i810_init(struct agp_softc *sc)
{
	struct agp_i810_softc *isc;
	int error;

	isc = sc->as_chipc;

	if (isc->chiptype == CHIP_I810) {
		struct agp_gatt *gatt;
		void *virtual;
		int dummyseg;

		/* Some i810s have on-chip memory called dcache */
		if (READ1(AGP_I810_DRT) & AGP_I810_DRT_POPULATED)
			isc->dcache_size = 4 * 1024 * 1024;
		else
			isc->dcache_size = 0;

		/* According to the specs the gatt on the i810 must be 64k */
		isc->gtt_size = 64 * 1024;
		gatt = malloc(sizeof(*gatt), M_AGP, M_NOWAIT);
		if (gatt == NULL) {
			aprint_error_dev(sc->as_dev,
			    "can't malloc GATT record\n");
			error = ENOMEM;
			goto fail0;
		}
		gatt->ag_entries = isc->gtt_size / sizeof(uint32_t);
		error = agp_alloc_dmamem(sc->as_dmat, isc->gtt_size,
		    0, &gatt->ag_dmamap, &virtual, &gatt->ag_physical,
		    &gatt->ag_dmaseg, 1, &dummyseg);
		if (error) {
			aprint_error_dev(sc->as_dev,
			    "can't allocate memory for GTT: %d\n", error);
			free(gatt, M_AGP);
			goto fail0;
		}

		gatt->ag_virtual = (uint32_t *)virtual;
		gatt->ag_size = gatt->ag_entries * sizeof(uint32_t);
		memset(gatt->ag_virtual, 0, gatt->ag_size);
		agp_flush_cache();

		/* Install the GATT. */
		isc->pgtblctl = gatt->ag_physical | 1;
		WRITE4(AGP_I810_PGTBL_CTL, isc->pgtblctl);
		isc->gatt = gatt;
	} else if (isc->chiptype == CHIP_I830) {
		/* The i830 automatically initializes the 128k gatt on boot. */
		/* XXX [citation needed] */
		pcireg_t reg;
		u_int16_t gcc1;

		isc->gtt_size = 128 * 1024;

		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_I830_GCC0);
		gcc1 = (u_int16_t)(reg >> 16);
		switch (gcc1 & AGP_I830_GCC1_GMS) {
		case AGP_I830_GCC1_GMS_STOLEN_512:
			isc->stolen = (512 - 132) * 1024 / 4096;
			break;
		case AGP_I830_GCC1_GMS_STOLEN_1024:
			isc->stolen = (1024 - 132) * 1024 / 4096;
			break;
		case AGP_I830_GCC1_GMS_STOLEN_8192:
			isc->stolen = (8192 - 132) * 1024 / 4096;
			break;
		default:
			isc->stolen = 0;
			aprint_error_dev(sc->as_dev,
			    "unknown memory configuration, disabling\n");
			error = ENXIO;
			goto fail0;
		}

		if (isc->stolen > 0) {
			aprint_normal_dev(sc->as_dev,
			    "detected %dk stolen memory\n",
			    isc->stolen * 4);
		}

		/* GATT address is already in there, make sure it's enabled */
		isc->pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		isc->pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, isc->pgtblctl);
	} else if (isc->chiptype == CHIP_I855 || isc->chiptype == CHIP_I915 ||
		   isc->chiptype == CHIP_I965 || isc->chiptype == CHIP_G33 ||
		   isc->chiptype == CHIP_G4X) {
		pcireg_t reg;
		u_int32_t gtt_size, stolen;	/* XXX kilobytes */
		u_int16_t gcc1;

		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_I855_GCC1);
		gcc1 = (u_int16_t)(reg >> 16);

		isc->pgtblctl = READ4(AGP_I810_PGTBL_CTL);

		/* Stolen memory is set up at the beginning of the aperture by
                 * the BIOS, consisting of the GATT followed by 4kb for the
		 * BIOS display.
                 */
                switch (isc->chiptype) {
		case CHIP_I855:
			gtt_size = 128;
			break;
                case CHIP_I915:
			gtt_size = 256;
			break;
		case CHIP_I965:
			switch (isc->pgtblctl & AGP_I810_PGTBL_SIZE_MASK) {
			case AGP_I810_PGTBL_SIZE_128KB:
			case AGP_I810_PGTBL_SIZE_512KB:
				gtt_size = 512;
				break;
			case AGP_I965_PGTBL_SIZE_1MB:
				gtt_size = 1024;
				break;
			case AGP_I965_PGTBL_SIZE_2MB:
				gtt_size = 2048;
				break;
			case AGP_I965_PGTBL_SIZE_1_5MB:
				gtt_size = 1024 + 512;
				break;
			default:
				aprint_error_dev(sc->as_dev,
				    "bad PGTBL size\n");
				error = ENXIO;
				goto fail0;
			}
			break;
		case CHIP_G33:
			switch (gcc1 & AGP_G33_PGTBL_SIZE_MASK) {
			case AGP_G33_PGTBL_SIZE_1M:
				gtt_size = 1024;
				break;
			case AGP_G33_PGTBL_SIZE_2M:
				gtt_size = 2048;
				break;
			default:
				aprint_error_dev(sc->as_dev,
				    "bad PGTBL size\n");
				error = ENXIO;
				goto fail0;
			}
			break;
		case CHIP_G4X:
			switch (isc->pgtblctl & AGP_G4X_PGTBL_SIZE_MASK) {
			case AGP_G4X_PGTBL_SIZE_512K:
				gtt_size = 512;
				break;
			case AGP_G4X_PGTBL_SIZE_256K:
				gtt_size = 256;
				break;
			case AGP_G4X_PGTBL_SIZE_128K:
				gtt_size = 128;
				break;
			case AGP_G4X_PGTBL_SIZE_1M:
				gtt_size = 1*1024;
				break;
			case AGP_G4X_PGTBL_SIZE_2M:
				gtt_size = 2*1024;
				break;
			case AGP_G4X_PGTBL_SIZE_1_5M:
				gtt_size = 1*1024 + 512;
				break;
			default:
				aprint_error_dev(sc->as_dev,
				    "bad PGTBL size\n");
				error = ENXIO;
				goto fail0;
			}
			break;
		default:
			panic("impossible chiptype %d", isc->chiptype);
		}

		/*
		 * XXX If I'm reading the datasheets right, this stolen
		 * memory detection logic is totally wrong.
		 */
		switch (gcc1 & AGP_I855_GCC1_GMS) {
		case AGP_I855_GCC1_GMS_STOLEN_1M:
			stolen = 1024;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_4M:
			stolen = 4 * 1024;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_8M:
			stolen = 8 * 1024;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_16M:
			stolen = 16 * 1024;
			break;
		case AGP_I855_GCC1_GMS_STOLEN_32M:
			stolen = 32 * 1024;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_48M:
			stolen = 48 * 1024;
			break;
		case AGP_I915_GCC1_GMS_STOLEN_64M:
			stolen = 64 * 1024;
			break;
		case AGP_G33_GCC1_GMS_STOLEN_128M:
			stolen = 128 * 1024;
			break;
		case AGP_G33_GCC1_GMS_STOLEN_256M:
			stolen = 256 * 1024;
			break;
		case AGP_G4X_GCC1_GMS_STOLEN_96M:
			stolen = 96 * 1024;
			break;
		case AGP_G4X_GCC1_GMS_STOLEN_160M:
			stolen = 160 * 1024;
			break;
		case AGP_G4X_GCC1_GMS_STOLEN_224M:
			stolen = 224 * 1024;
			break;
		case AGP_G4X_GCC1_GMS_STOLEN_352M:
			stolen = 352 * 1024;
			break;
		default:
			aprint_error_dev(sc->as_dev,
			    "unknown memory configuration, disabling\n");
			error = ENXIO;
			goto fail0;
		}

		switch (gcc1 & AGP_I855_GCC1_GMS) {
		case AGP_I915_GCC1_GMS_STOLEN_48M:
		case AGP_I915_GCC1_GMS_STOLEN_64M:
			if (isc->chiptype != CHIP_I915 &&
			    isc->chiptype != CHIP_I965 &&
			    isc->chiptype != CHIP_G33 &&
			    isc->chiptype != CHIP_G4X)
				stolen = 0;
			break;
		case AGP_G33_GCC1_GMS_STOLEN_128M:
		case AGP_G33_GCC1_GMS_STOLEN_256M:
			if (isc->chiptype != CHIP_I965 &&
			    isc->chiptype != CHIP_G33 &&
			    isc->chiptype != CHIP_G4X)
				stolen = 0;
			break;
		case AGP_G4X_GCC1_GMS_STOLEN_96M:
		case AGP_G4X_GCC1_GMS_STOLEN_160M:
		case AGP_G4X_GCC1_GMS_STOLEN_224M:
		case AGP_G4X_GCC1_GMS_STOLEN_352M:
			if (isc->chiptype != CHIP_I965 &&
			    isc->chiptype != CHIP_G4X)
				stolen = 0;
			break;
		}

		isc->gtt_size = gtt_size * 1024;

		/* BIOS space */
		/* XXX [citation needed] */
		gtt_size += 4;

		/* XXX [citation needed] for this subtraction */
		isc->stolen = (stolen - gtt_size) * 1024 / 4096;

		if (isc->stolen > 0) {
			aprint_normal_dev(sc->as_dev,
			    "detected %dk stolen memory\n",
			    isc->stolen * 4);
		}

		/* GATT address is already in there, make sure it's enabled */
		isc->pgtblctl |= 1;
		WRITE4(AGP_I810_PGTBL_CTL, isc->pgtblctl);
	}

	/*
	 * Make sure the chipset can see everything.
	 */
	agp_flush_cache();

	/*
	 * Publish what we found for kludgey drivers (I'm looking at
	 * you, drm).
	 */
	if (agp_i810_sc == NULL)
		agp_i810_sc = sc;
	else
		aprint_error_dev(sc->as_dev, "agp already attached\n");

	/* Success!  */
	return 0;

fail0:	KASSERT(error);
	return error;
}

#if 0
static int
agp_i810_detach(struct agp_softc *sc)
{
	int error;
	struct agp_i810_softc *isc = sc->as_chipc;

	error = agp_generic_detach(sc);
	if (error)
		return error;

	switch (isc->chiptype) {
	case CHIP_I915:
	case CHIP_I965:
	case CHIP_G33:
	case CHIP_G4X:
		agp_i810_teardown_chipset_flush_page(sc);
		break;
	}

	/* Clear the GATT base. */
	if (sc->chiptype == CHIP_I810) {
		WRITE4(AGP_I810_PGTBL_CTL, 0);
	} else {
		unsigned int pgtblctl;
		pgtblctl = READ4(AGP_I810_PGTBL_CTL);
		pgtblctl &= ~1;
		WRITE4(AGP_I810_PGTBL_CTL, pgtblctl);
	}

	if (sc->chiptype == CHIP_I810) {
		agp_free_dmamem(sc->as_dmat, gatt->ag_size, gatt->ag_dmamap,
		    (void *)gatt->ag_virtual, &gatt->ag_dmaseg, 1);
		free(isc->gatt, M_AGP);
	}

	return 0;
}
#endif

static u_int32_t
agp_i810_get_aperture(struct agp_softc *sc)
{
	struct agp_i810_softc *isc = sc->as_chipc;
	pcireg_t reg;
	u_int32_t size;
	u_int16_t miscc, gcc1;

	size = 0;

	switch (isc->chiptype) {
	case CHIP_I810:
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_I810_SMRAM);
		miscc = (u_int16_t)(reg >> 16);
		if ((miscc & AGP_I810_MISCC_WINSIZE) ==
		    AGP_I810_MISCC_WINSIZE_32)
			size = 32 * 1024 * 1024;
		else
			size = 64 * 1024 * 1024;
		break;
	case CHIP_I830:
		reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_I830_GCC0);
		gcc1 = (u_int16_t)(reg >> 16);
		if ((gcc1 & AGP_I830_GCC1_GMASIZE) == AGP_I830_GCC1_GMASIZE_64)
			size = 64 * 1024 * 1024;
		else
			size = 128 * 1024 * 1024;
		break;
	case CHIP_I855:
		size = 128 * 1024 * 1024;
		break;
	case CHIP_I915:
	case CHIP_G33:
	case CHIP_G4X:
		size = sc->as_apsize;
		break;
	case CHIP_I965:
		size = 512 * 1024 * 1024;
		break;
	default:
		aprint_error(": Unknown chipset\n");
	}

	return size;
}

static int
agp_i810_set_aperture(struct agp_softc *sc __unused,
    uint32_t aperture __unused)
{

	return ENOSYS;
}

static int
agp_i810_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_i810_softc *isc = sc->as_chipc;

	if (offset < 0 || offset >= ((isc->gtt_size/4) << AGP_PAGE_SHIFT)) {
		DPRINTF(sc, "failed"
		    ": offset 0x%"PRIxMAX", shift %u, entries %"PRIuMAX"\n",
		    (uintmax_t)offset,
		    (unsigned)AGP_PAGE_SHIFT,
		    (uintmax_t)isc->gtt_size/4);
		return EINVAL;
	}

	if (isc->chiptype != CHIP_I810) {
		if ((offset >> AGP_PAGE_SHIFT) < isc->stolen) {
			DPRINTF(sc, "trying to bind into stolen memory\n");
			return EINVAL;
		}
	}

	return agp_i810_write_gtt_entry(isc, offset, physical,
	    AGP_I810_GTT_VALID);
}

static int
agp_i810_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_i810_softc *isc = sc->as_chipc;

	if (offset < 0 || offset >= ((isc->gtt_size/4) << AGP_PAGE_SHIFT))
		return EINVAL;

	if (isc->chiptype != CHIP_I810 ) {
		if ((offset >> AGP_PAGE_SHIFT) < isc->stolen) {
			DPRINTF(sc, "trying to unbind from stolen memory\n");
			return EINVAL;
		}
	}

	return agp_i810_write_gtt_entry(isc, offset, 0, 0);
}

/*
 * Writing via memory mapped registers already flushes all TLBs.
 */
static void
agp_i810_flush_tlb(struct agp_softc *sc)
{
}

static int
agp_i810_enable(struct agp_softc *sc, u_int32_t mode)
{

	return 0;
}

#define	AGP_I810_MEMTYPE_MAIN		0
#define	AGP_I810_MEMTYPE_DCACHE		1
#define	AGP_I810_MEMTYPE_HWCURSOR	2

static struct agp_memory *
agp_i810_alloc_memory(struct agp_softc *sc, int type, vsize_t size)
{
	struct agp_i810_softc *isc = sc->as_chipc;
	struct agp_memory *mem;
	int error;

	DPRINTF(sc, "AGP: alloc(%d, 0x%"PRIxMAX")\n", type, (uintmax_t)size);

	if (size <= 0)
		return NULL;
	if ((size & (AGP_PAGE_SIZE - 1)) != 0)
		return NULL;
	KASSERT(sc->as_allocated <= sc->as_maxmem);
	if (size > (sc->as_maxmem - sc->as_allocated))
		return NULL;
	if (size > ((isc->gtt_size/4) << AGP_PAGE_SHIFT))
		return NULL;

	switch (type) {
	case AGP_I810_MEMTYPE_MAIN:
		break;
	case AGP_I810_MEMTYPE_DCACHE:
		if (isc->chiptype != CHIP_I810)
			return NULL;
		if (size != isc->dcache_size)
			return NULL;
		break;
	case AGP_I810_MEMTYPE_HWCURSOR:
		if ((size != AGP_PAGE_SIZE) &&
		    (size != AGP_PAGE_SIZE*4))
			return NULL;
		break;
	default:
		return NULL;
	}

	mem = malloc(sizeof(*mem), M_AGP, M_WAITOK|M_ZERO);
	if (mem == NULL)
		goto fail0;
	mem->am_id = sc->as_nextid++;
	mem->am_size = size;
	mem->am_type = type;

	switch (type) {
	case AGP_I810_MEMTYPE_MAIN:
		error = bus_dmamap_create(sc->as_dmat, size,
		    (size >> AGP_PAGE_SHIFT) + 1, size, 0, BUS_DMA_WAITOK,
		    &mem->am_dmamap);
		if (error)
			goto fail1;
		break;
	case AGP_I810_MEMTYPE_DCACHE:
		break;
	case AGP_I810_MEMTYPE_HWCURSOR:
		mem->am_dmaseg = malloc(sizeof(*mem->am_dmaseg), M_AGP,
		    M_WAITOK);
		error = agp_alloc_dmamem(sc->as_dmat, size, 0, &mem->am_dmamap,
		    &mem->am_virtual, &mem->am_physical, mem->am_dmaseg, 1,
		    &mem->am_nseg);
		if (error) {
			free(mem->am_dmaseg, M_AGP);
			goto fail1;
		}
		(void)memset(mem->am_virtual, 0, size);
		break;
	default:
		panic("invalid agp memory type: %d", type);
	}

	TAILQ_INSERT_TAIL(&sc->as_memory, mem, am_link);
	sc->as_allocated += size;

	return mem;

fail1:	free(mem, M_AGP);
fail0:	return NULL;
}

static int
agp_i810_free_memory(struct agp_softc *sc, struct agp_memory *mem)
{

	if (mem->am_is_bound)
		return EBUSY;

	switch (mem->am_type) {
	case AGP_I810_MEMTYPE_MAIN:
		bus_dmamap_destroy(sc->as_dmat, mem->am_dmamap);
		break;
	case AGP_I810_MEMTYPE_DCACHE:
		break;
	case AGP_I810_MEMTYPE_HWCURSOR:
		agp_free_dmamem(sc->as_dmat, mem->am_size, mem->am_dmamap,
		    mem->am_virtual, mem->am_dmaseg, mem->am_nseg);
		free(mem->am_dmaseg, M_AGP);
		break;
	default:
		panic("invalid agp i810 memory type: %d", mem->am_type);
	}

	sc->as_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->as_memory, mem, am_link);
	free(mem, M_AGP);

	return 0;
}

static int
agp_i810_bind_memory(struct agp_softc *sc, struct agp_memory *mem,
    off_t offset)
{
	struct agp_i810_softc *isc = sc->as_chipc;
	uint32_t pgtblctl;
	int error;

	if (mem->am_is_bound)
		return EINVAL;

	/*
	 * XXX evil hack: the PGTBL_CTL appearently gets overwritten by the
	 * X server for mysterious reasons which leads to crashes if we write
	 * to the GTT through the MMIO window.
	 * Until the issue is solved, simply restore it.
	 */
	pgtblctl = bus_space_read_4(isc->bst, isc->bsh, AGP_I810_PGTBL_CTL);
	if (pgtblctl != isc->pgtblctl) {
		printf("agp_i810_bind_memory: PGTBL_CTL is 0x%"PRIx32
		    " - fixing\n", pgtblctl);
		bus_space_write_4(isc->bst, isc->bsh, AGP_I810_PGTBL_CTL,
		    isc->pgtblctl);
	}

	switch (mem->am_type) {
	case AGP_I810_MEMTYPE_MAIN:
		return agp_generic_bind_memory_bounded(sc, mem, offset,
		    0, (isc->gtt_size/4) << AGP_PAGE_SHIFT);
	case AGP_I810_MEMTYPE_DCACHE:
		error = agp_i810_bind_memory_dcache(sc, mem, offset);
		break;
	case AGP_I810_MEMTYPE_HWCURSOR:
		error = agp_i810_bind_memory_hwcursor(sc, mem, offset);
		break;
	default:
		panic("invalid agp i810 memory type: %d", mem->am_type);
	}
	if (error)
		return error;

	/* Success!  */
	mem->am_is_bound = 1;
	return 0;
}

static int
agp_i810_bind_memory_dcache(struct agp_softc *sc, struct agp_memory *mem,
    off_t offset)
{
	struct agp_i810_softc *const isc __diagused = sc->as_chipc;
	uint32_t i, j;
	int error;

	KASSERT(isc->chiptype == CHIP_I810);

	KASSERT((mem->am_size & (AGP_PAGE_SIZE - 1)) == 0);
	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
		error = agp_i810_write_gtt_entry(isc, offset + i,
		    i, AGP_I810_GTT_VALID | AGP_I810_GTT_I810_DCACHE);
		if (error)
			goto fail0;
	}

	/* Success!  */
	mem->am_offset = offset;
	return 0;

fail0:	for (j = 0; j < i; j += AGP_PAGE_SIZE)
		(void)agp_i810_unbind_page(sc, offset + j);
	return error;
}

static int
agp_i810_bind_memory_hwcursor(struct agp_softc *sc, struct agp_memory *mem,
    off_t offset)
{
	const bus_addr_t pa = mem->am_physical;
	uint32_t i, j;
	int error;

	KASSERT((mem->am_size & (AGP_PAGE_SIZE - 1)) == 0);
	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE) {
		error = agp_i810_bind_page(sc, offset + i, pa + i);
		if (error)
			goto fail0;
	}

	/* Success!  */
	mem->am_offset = offset;
	return 0;

fail0:	for (j = 0; j < i; j += AGP_PAGE_SIZE)
		(void)agp_i810_unbind_page(sc, offset + j);
	return error;
}

static int
agp_i810_unbind_memory(struct agp_softc *sc, struct agp_memory *mem)
{
	struct agp_i810_softc *isc __diagused = sc->as_chipc;
	u_int32_t i;

	if (!mem->am_is_bound)
		return EINVAL;

	switch (mem->am_type) {
	case AGP_I810_MEMTYPE_MAIN:
		return agp_generic_unbind_memory(sc, mem);
	case AGP_I810_MEMTYPE_DCACHE:
		KASSERT(isc->chiptype == CHIP_I810);
		/* FALLTHROUGH */
	case AGP_I810_MEMTYPE_HWCURSOR:
		for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
			(void)agp_i810_unbind_page(sc, mem->am_offset + i);
		mem->am_offset = 0;
		break;
	default:
		panic("invalid agp i810 memory type: %d", mem->am_type);
	}

	mem->am_is_bound = 0;
	return 0;
}

static bool
agp_i810_resume(device_t dv, const pmf_qual_t *qual)
{
	struct agp_softc *sc = device_private(dv);
	struct agp_i810_softc *isc = sc->as_chipc;

	/*
	 * XXX Nothing uses this!  Save on suspend, restore on resume?
	 */
	isc->pgtblctl_resume_hack = READ4(AGP_I810_PGTBL_CTL);
	agp_flush_cache();

	return true;
}
