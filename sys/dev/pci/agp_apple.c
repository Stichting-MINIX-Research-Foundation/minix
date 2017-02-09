/*	$NetBSD: agp_apple.c,v 1.7 2014/11/02 00:05:03 christos Exp $ */

/*-
 * Copyright (c) 2007 Michael Lorenz
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agp_apple.c,v 1.7 2014/11/02 00:05:03 christos Exp $");
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

#include <sys/bus.h>

#define	APPLE_UNINORTH_GART_BASE	0x8c
#define	APPLE_UNINORTH_GART_BASE_ADDR	0x90
#define APPLE_UNINORTH_GART_CTRL	0x94
#define APPLE_UNINORTH_GART_INVAL	0x00000001
#define APPLE_UNINORTH_GART_ENABLE	0x00000100
#define APPLE_UNINORTH_GART_2XRESET	0x00010000
#define APPLE_UNINORTH_GART_PERFRD	0x00080000

static u_int32_t agp_apple_get_aperture(struct agp_softc *);
static int agp_apple_set_aperture(struct agp_softc *, u_int32_t);
static int agp_apple_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_apple_unbind_page(struct agp_softc *, off_t);
static void agp_apple_flush_tlb(struct agp_softc *);

static struct agp_methods agp_apple_methods = {
	agp_apple_get_aperture,
	agp_apple_set_aperture,
	agp_apple_bind_page,
	agp_apple_unbind_page,
	agp_apple_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};

struct agp_apple_softc {
	u_int32_t	initial_aperture; /* aperture size at startup */
	struct agp_gatt *gatt;
};

int
agp_apple_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct agp_softc *sc = device_private(self);
	struct agp_apple_softc *asc;
	struct agp_gatt *gatt;

	asc = malloc(sizeof *asc, M_AGP, M_NOWAIT|M_ZERO);
	if (asc == NULL) {
		aprint_error(": can't allocate chipset-specific softc\n");
		return ENOMEM;
	}
	sc->as_chipc = asc;
	sc->as_methods = &agp_apple_methods;
	pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, &sc->as_capoff,
	    NULL);

	sc->as_apaddr = 0;
	sc->as_apsize = 8 * 1024 * 1024;
	sc->as_apflags = 0;
	sc->as_pc = pa->pa_pc;
	sc->as_tag = pa->pa_tag;
	sc->as_apt = pa->pa_memt;

	asc->initial_aperture = sc->as_apsize;

	for (;;) {
		gatt = agp_alloc_gatt(sc);
		if (gatt)
			break;
		sc->as_apsize = sc->as_apsize >> 1;
		if (sc->as_apsize == 0) {
			aprint_error(": can't set aperture size\n");
			return ENOMEM;
		}
	}
	asc->gatt = gatt;

	/* Install the gatt. */
	aprint_error("gatt: %08jx %ju MB\n", (uintmax_t)gatt->ag_physical,
	    (uintmax_t)(sc->as_apsize >> 20));
	pci_conf_write(pa->pa_pc, pa->pa_tag, APPLE_UNINORTH_GART_BASE,
	    (gatt->ag_physical & 0xfffff000) |
	    (sc->as_apsize >> 22));

	/* Enable the aperture. */
	pci_conf_write(pa->pa_pc, pa->pa_tag, APPLE_UNINORTH_GART_CTRL,
	    APPLE_UNINORTH_GART_ENABLE);
	pci_conf_write(pa->pa_pc, pa->pa_tag, APPLE_UNINORTH_GART_CTRL,
	    APPLE_UNINORTH_GART_ENABLE | APPLE_UNINORTH_GART_INVAL);
	pci_conf_write(pa->pa_pc, pa->pa_tag, APPLE_UNINORTH_GART_CTRL,
	    APPLE_UNINORTH_GART_ENABLE);
	return 0;
}

static u_int32_t
agp_apple_get_aperture(struct agp_softc *sc)
{
#if 0
	u_int32_t apsize = 0;

	aprint_error("%s: ", __func__);
	apsize = pci_conf_read(sc->as_pc, sc->as_tag,
	    APPLE_UNINORTH_GART_BASE) & 0x0000ffff;
	aprint_error("%08x\n", apsize);
	return (apsize << 22);
#else
	return sc->as_apsize;
#endif
}

static int
agp_apple_set_aperture(struct agp_softc *sc, u_int32_t aperture)
{
	pcireg_t reg;

	aprint_error("%s: %08x\n", __func__, aperture);
	reg = pci_conf_read(sc->as_pc, sc->as_tag, APPLE_UNINORTH_GART_BASE)
	    & 0xfffff000;
	reg |= ((aperture >> 22) & 0xfff);
	pci_conf_write(sc->as_pc, sc->as_tag, APPLE_UNINORTH_GART_BASE, reg);
	sc->as_apsize = aperture;
	return 0;
}

static int
agp_apple_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_apple_softc *asc = sc->as_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = htole32(physical);
	return 0;
}

static int
agp_apple_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_apple_softc *asc = sc->as_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return 0;
}

static void
agp_apple_flush_tlb(struct agp_softc *sc)
{

	pci_conf_write(sc->as_pc, sc->as_tag, APPLE_UNINORTH_GART_CTRL,
	    APPLE_UNINORTH_GART_ENABLE);
	pci_conf_write(sc->as_pc, sc->as_tag, APPLE_UNINORTH_GART_CTRL,
	    APPLE_UNINORTH_GART_ENABLE | APPLE_UNINORTH_GART_INVAL);
	pci_conf_write(sc->as_pc, sc->as_tag, APPLE_UNINORTH_GART_CTRL,
	    APPLE_UNINORTH_GART_ENABLE);
}
