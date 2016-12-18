/*	$NetBSD: agp_sis.c,v 1.14 2010/11/13 13:52:05 uebayasi Exp $	*/

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
 *	$FreeBSD: src/sys/pci/agp_sis.c,v 1.3 2001/07/05 21:28:47 jhb Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agp_sis.c,v 1.14 2010/11/13 13:52:05 uebayasi Exp $");

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

struct agp_sis_softc {
	u_int32_t	initial_aperture; /* aperture size at startup */
	struct agp_gatt *gatt;
};

static u_int32_t agp_sis_get_aperture(struct agp_softc *);
static int agp_sis_set_aperture(struct agp_softc *, u_int32_t);
static int agp_sis_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_sis_unbind_page(struct agp_softc *, off_t);
static void agp_sis_flush_tlb(struct agp_softc *);

static struct agp_methods agp_sis_methods = {
	agp_sis_get_aperture,
	agp_sis_set_aperture,
	agp_sis_bind_page,
	agp_sis_unbind_page,
	agp_sis_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};

int
agp_sis_attach(device_t parent, device_t self, void *aux)
{
	struct agp_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct agp_sis_softc *ssc;
	struct agp_gatt *gatt;
	pcireg_t reg;

	ssc = malloc(sizeof *ssc, M_AGP, M_NOWAIT);
	if (ssc == NULL) {
		aprint_error(": can't allocate chipset-specific softc\n");
		return ENOMEM;
	}
	sc->as_methods = &agp_sis_methods;
	sc->as_chipc = ssc;
	pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, &sc->as_capoff,
	    NULL);

	if (agp_map_aperture(pa, sc, AGP_APBASE) != 0) {
		aprint_error(": can't map aperture\n");
		free(ssc, M_AGP);
		return ENXIO;
	}

	ssc->initial_aperture = AGP_GET_APERTURE(sc);

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
	ssc->gatt = gatt;

	/* Install the gatt. */
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_SIS_ATTBASE,
	    gatt->ag_physical);

	/* Enable the aperture and auto-tlb-inval */
	reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_SIS_WINCTRL);
	reg |= (0x05 << 24) | 3;
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_SIS_WINCTRL, reg);

	return 0;
}

#if 0
static int
agp_sis_detach(struct agp_softc *sc)
{
	struct agp_sis_softc *ssc = sc->as_chipc;
	pcireg_t reg;
	int error;

	error = agp_generic_detach(sc);
	if (error)
		return error;

	reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_SIS_WINCTRL);
	reg &= ~3;
	reg &= 0x00ffffff;
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_SIS_WINCTRL, reg);

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(sc, ssc->initial_aperture);

	agp_free_gatt(sc, ssc->gatt);
	return 0;
}
#endif

static u_int32_t
agp_sis_get_aperture(struct agp_softc *sc)
{
	int gws;

	/*
	 * The aperture size is equal to 4M<<gws.
	 */
	gws = (pci_conf_read(sc->as_pc, sc->as_tag, AGP_SIS_WINCTRL)&0x70) >> 4;
	return (4*1024*1024) << gws;
}

static int
agp_sis_set_aperture(struct agp_softc *sc, u_int32_t aperture)
{
	int gws;
	pcireg_t reg;

	/*
	 * Check for a power of two and make sure its within the
	 * programmable range.
	 */
	if (aperture & (aperture - 1)
	    || aperture < 4*1024*1024
	    || aperture > 256*1024*1024)
		return EINVAL;

	gws = ffs(aperture / 4*1024*1024) - 1;

	reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_SIS_WINCTRL);
	reg &= ~0x00000070;
	reg |= gws << 4;
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_SIS_WINCTRL, reg);

	return 0;
}

static int
agp_sis_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_sis_softc *ssc = sc->as_chipc;

	if (offset < 0 || offset >= (ssc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	ssc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical;
	return 0;
}

static int
agp_sis_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_sis_softc *ssc = sc->as_chipc;

	if (offset < 0 || offset >= (ssc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	ssc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return 0;
}

static void
agp_sis_flush_tlb(struct agp_softc *sc)
{
	pcireg_t reg;

	reg = pci_conf_read(sc->as_pc, sc->as_tag, AGP_SIS_TLBFLUSH);
	reg &= 0xffffff00;
	reg |= 0x02;
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_SIS_TLBFLUSH, reg);
}
