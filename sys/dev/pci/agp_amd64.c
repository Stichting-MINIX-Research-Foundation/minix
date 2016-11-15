/*-
 * Copyright (c) 2004, 2005 Jung-uk Kim <jkim@FreeBSD.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: agp_amd64.c,v 1.8 2015/04/04 15:08:40 riastradh Exp $");

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


#define	AMD64_MAX_MCTRL		8

/* XXX nForce3 requires secondary AGP bridge at 0:11:0. */
#define AGP_AMD64_NVIDIA_PCITAG(pc)	pci_make_tag(pc, 0, 11, 0)
/* XXX Some VIA bridge requires secondary AGP bridge at 0:1:0. */
#define AGP_AMD64_VIA_PCITAG(pc)	pci_make_tag(pc, 0, 1, 0)


static uint32_t agp_amd64_get_aperture(struct agp_softc *);
static int agp_amd64_set_aperture(struct agp_softc *, uint32_t);
static int agp_amd64_bind_page(struct agp_softc *, off_t, bus_addr_t);
static int agp_amd64_unbind_page(struct agp_softc *, off_t);
static void agp_amd64_flush_tlb(struct agp_softc *);

static void agp_amd64_apbase_fixup(struct agp_softc *);

static void agp_amd64_uli_init(struct agp_softc *);
static int agp_amd64_uli_set_aperture(struct agp_softc *, uint32_t);

static int agp_amd64_nvidia_match(const struct pci_attach_args *, uint16_t);
static void agp_amd64_nvidia_init(struct agp_softc *);
static int agp_amd64_nvidia_set_aperture(struct agp_softc *, uint32_t);

static int agp_amd64_via_match(const struct pci_attach_args *);
static void agp_amd64_via_init(struct agp_softc *);
static int agp_amd64_via_set_aperture(struct agp_softc *, uint32_t);


struct agp_amd64_softc {
	uint32_t		initial_aperture;
	struct agp_gatt		*gatt;
	uint32_t		apbase;
	pcitag_t		ctrl_tag;	/* use NVIDIA and VIA */
	pcitag_t		mctrl_tag[AMD64_MAX_MCTRL];
	int			n_mctrl;
	int			via_agp;
};

static struct agp_methods agp_amd64_methods = {
	agp_amd64_get_aperture,
	agp_amd64_set_aperture,
	agp_amd64_bind_page,
	agp_amd64_unbind_page,
	agp_amd64_flush_tlb,
	agp_generic_enable,
	agp_generic_alloc_memory,
	agp_generic_free_memory,
	agp_generic_bind_memory,
	agp_generic_unbind_memory,
};


int
agp_amd64_match(const struct pci_attach_args *pa)
{

	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_AMD:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_AMD_AGP8151_DEV:
			return 1;
		}
		break;

	case PCI_VENDOR_SIS:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_SIS_755:
		case PCI_PRODUCT_SIS_760:
			return 1;
		}
		break;

	case PCI_VENDOR_ALI:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ALI_M1689:
			return 1;
		}
		break;

	case PCI_VENDOR_NVIDIA:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_NVIDIA_NFORCE3_PCHB:
			return agp_amd64_nvidia_match(pa,
			    PCI_PRODUCT_NVIDIA_NFORCE3_PPB2);

			/* NOTREACHED */

		case PCI_PRODUCT_NVIDIA_NFORCE3_250_PCHB:
			return agp_amd64_nvidia_match(pa,
			    PCI_PRODUCT_NVIDIA_NFORCE3_250_AGP);

			/* NOTREACHED */
		}
		break;

	case PCI_VENDOR_VIATECH:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_VIATECH_K8M800_0:
		case PCI_PRODUCT_VIATECH_K8T890_0:
		case PCI_PRODUCT_VIATECH_K8HTB_0:
		case PCI_PRODUCT_VIATECH_K8HTB:
			return 1;
		}
		break;
	}

	return 0;
}

static int
agp_amd64_nvidia_match(const struct pci_attach_args *pa, uint16_t devid)
{
	pcitag_t tag;
	pcireg_t reg;

	tag = AGP_AMD64_NVIDIA_PCITAG(pa->pa_pc);

	reg = pci_conf_read(pa->pa_pc, tag, PCI_CLASS_REG);
	if (PCI_CLASS(reg) != PCI_CLASS_BRIDGE ||
	    PCI_SUBCLASS(reg) != PCI_SUBCLASS_BRIDGE_PCI)
		return 0;

	reg = pci_conf_read(pa->pa_pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(reg) != PCI_VENDOR_NVIDIA || PCI_PRODUCT(reg) != devid)
		return 0;

	return 1;
}

static int
agp_amd64_via_match(const struct pci_attach_args *pa)
{
	pcitag_t tag;
	pcireg_t reg;

	tag = AGP_AMD64_VIA_PCITAG(pa->pa_pc);

	reg = pci_conf_read(pa->pa_pc, tag, PCI_CLASS_REG);
	if (PCI_CLASS(reg) != PCI_CLASS_BRIDGE ||
	    PCI_SUBCLASS(reg) != PCI_SUBCLASS_BRIDGE_PCI)
		return 0;

	reg = pci_conf_read(pa->pa_pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(reg) != PCI_VENDOR_VIATECH ||
	    PCI_PRODUCT(reg) != PCI_PRODUCT_VIATECH_K8HTB_AGP)
		return 0;

	return 1;
}

int
agp_amd64_attach(device_t parent, device_t self, void *aux)
{
	struct agp_softc *sc = device_private(self);
	struct agp_amd64_softc *asc;
	struct pci_attach_args *pa = aux;
	struct agp_gatt *gatt;
	pcitag_t tag;
	pcireg_t id, attbase, apctrl;
	int maxdevs, i, n;
	int error;

	asc = malloc(sizeof(struct agp_amd64_softc), M_AGP, M_NOWAIT | M_ZERO);
	if (asc == NULL) {
		aprint_error(": can't allocate softc\n");
		error = ENOMEM;
		goto fail0;
	}

	if (agp_map_aperture(pa, sc, AGP_APBASE) != 0) {
		aprint_error(": can't map aperture\n");
		error = ENXIO;
		goto fail1;
	}

	maxdevs = pci_bus_maxdevs(pa->pa_pc, 0);
	for (i = 0, n = 0; i < maxdevs && n < AMD64_MAX_MCTRL; i++) {
		tag = pci_make_tag(pa->pa_pc, 0, i, 3);
		id = pci_conf_read(pa->pa_pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(id) == PCI_VENDOR_AMD &&
		    (PCI_PRODUCT(id) == PCI_PRODUCT_AMD_AMD64_MISC ||
		     PCI_PRODUCT(id) == PCI_PRODUCT_AMD_AMD64_F10_MISC)) {
			asc->mctrl_tag[n] = tag;
			n++;
		}
	}
	if (n == 0) {
		aprint_error(": No Miscellaneous Control unit found.\n");
		error = ENXIO;
		goto fail1;
	}
	asc->n_mctrl = n;

	aprint_normal(": %d Miscellaneous Control unit(s) found.\n",
	    asc->n_mctrl);
	aprint_normal("%s", device_xname(self));

	sc->as_chipc = asc;
	sc->as_methods = &agp_amd64_methods;
	pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP, &sc->as_capoff,
	    NULL);
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
			error = ENOMEM;
			goto fail1;
		}
	}
	asc->gatt = gatt;

	switch (PCI_VENDOR(sc->as_id)) {
	case PCI_VENDOR_ALI:
		agp_amd64_uli_init(sc);
		if (agp_amd64_uli_set_aperture(sc, asc->initial_aperture)) {
			/* XXX Back out agp_amd64_uli_init?  */
			error = ENXIO;
			goto fail2;
		}
		break;

	case PCI_VENDOR_NVIDIA:
		asc->ctrl_tag = AGP_AMD64_NVIDIA_PCITAG(pa->pa_pc);
		agp_amd64_nvidia_init(sc);
		if (agp_amd64_nvidia_set_aperture(sc, asc->initial_aperture)) {
			/* XXX Back out agp_amd64_nvidia_init?  */
			error = ENXIO;
			goto fail2;
		}
		break;

	case PCI_VENDOR_VIATECH:
		asc->via_agp = agp_amd64_via_match(pa);
		if (asc->via_agp) {
			asc->ctrl_tag = AGP_AMD64_VIA_PCITAG(pa->pa_pc);
			agp_amd64_via_init(sc);
			if (agp_amd64_via_set_aperture(sc,
			    asc->initial_aperture)) {
				/* XXX Back out agp_amd64_via_init?  */
				error = ENXIO;
				goto fail2;
			}
		}
		break;
	}

	/* Install the gatt and enable aperture. */
	attbase = (uint32_t)(gatt->ag_physical >> 8) & AGP_AMD64_ATTBASE_MASK;
	for (i = 0; i < asc->n_mctrl; i++) {
		pci_conf_write(pa->pa_pc, asc->mctrl_tag[i], AGP_AMD64_ATTBASE,
		    attbase);
		apctrl = pci_conf_read(pa->pa_pc, asc->mctrl_tag[i],
		    AGP_AMD64_APCTRL);
		apctrl |= AGP_AMD64_APCTRL_GARTEN;
		apctrl &=
		    ~(AGP_AMD64_APCTRL_DISGARTCPU | AGP_AMD64_APCTRL_DISGARTIO);
		pci_conf_write(pa->pa_pc, asc->mctrl_tag[i], AGP_AMD64_APCTRL,
		    apctrl);
	}

	agp_flush_cache();

	/* Success!  */
	return 0;

fail2:	agp_free_gatt(sc, gatt);
fail1:	free(asc, M_AGP);
fail0:	agp_generic_detach(sc);
	KASSERT(error);
	return error;
}


static uint32_t agp_amd64_table[] = {
	0x02000000,	/*   32 MB */
	0x04000000,	/*   64 MB */
	0x08000000,	/*  128 MB */
	0x10000000,	/*  256 MB */
	0x20000000,	/*  512 MB */
	0x40000000,	/* 1024 MB */
	0x80000000,	/* 2048 MB */
};

#define AGP_AMD64_TABLE_SIZE \
	(sizeof(agp_amd64_table) / sizeof(agp_amd64_table[0]))

static uint32_t
agp_amd64_get_aperture(struct agp_softc *sc)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	uint32_t i;

	i = (pci_conf_read(sc->as_pc, asc->mctrl_tag[0], AGP_AMD64_APCTRL) &
		AGP_AMD64_APCTRL_SIZE_MASK) >> 1;

	if (i >= AGP_AMD64_TABLE_SIZE)
		return 0;

	return agp_amd64_table[i];
}

static int
agp_amd64_set_aperture(struct agp_softc *sc, uint32_t aperture)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	uint32_t i;
	pcireg_t apctrl;
	int j;

	for (i = 0; i < AGP_AMD64_TABLE_SIZE; i++)
		if (agp_amd64_table[i] == aperture)
			break;
	if (i >= AGP_AMD64_TABLE_SIZE)
		return EINVAL;

	for (j = 0; j < asc->n_mctrl; j++) {
		apctrl = pci_conf_read(sc->as_pc, asc->mctrl_tag[0],
		    AGP_AMD64_APCTRL);
		pci_conf_write(sc->as_pc, asc->mctrl_tag[0], AGP_AMD64_APCTRL,
		    (apctrl & ~(AGP_AMD64_APCTRL_SIZE_MASK)) | (i << 1));
	}

	switch (PCI_VENDOR(sc->as_id)) {
	case PCI_VENDOR_ALI:
		return agp_amd64_uli_set_aperture(sc, aperture);
		break;

	case PCI_VENDOR_NVIDIA:
		return agp_amd64_nvidia_set_aperture(sc, aperture);
		break;

	case PCI_VENDOR_VIATECH:
		if (asc->via_agp)
			return agp_amd64_via_set_aperture(sc, aperture);
		break;
	}

	return 0;
}

static int
agp_amd64_bind_page(struct agp_softc *sc, off_t offset, bus_addr_t physical)
{
	struct agp_amd64_softc *asc = sc->as_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] =
	    (physical & 0xfffff000) | ((physical >> 28) & 0x00000ff0) | 3;

	return 0;
}

static int
agp_amd64_unbind_page(struct agp_softc *sc, off_t offset)
{
	struct agp_amd64_softc *asc = sc->as_chipc;

	if (offset < 0 || offset >= (asc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	asc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;

	return 0;
}

static void
agp_amd64_flush_tlb(struct agp_softc *sc)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	pcireg_t cachectrl;
	int i;

	for (i = 0; i < asc->n_mctrl; i++) {
		cachectrl = pci_conf_read(sc->as_pc, asc->mctrl_tag[i],
		    AGP_AMD64_CACHECTRL);
		pci_conf_write(sc->as_pc, asc->mctrl_tag[i],
		    AGP_AMD64_CACHECTRL,
		    cachectrl | AGP_AMD64_CACHECTRL_INVGART);
	}
}

static void
agp_amd64_apbase_fixup(struct agp_softc *sc)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	uint32_t apbase;
	int i;

	apbase = pci_conf_read(sc->as_pc, sc->as_tag, AGP_APBASE);
	asc->apbase = PCI_MAPREG_MEM_ADDR(apbase);
	apbase = (asc->apbase >> 25) & AGP_AMD64_APBASE_MASK;
	for (i = 0; i < asc->n_mctrl; i++)
		pci_conf_write(sc->as_pc, asc->mctrl_tag[i], AGP_AMD64_APBASE,
		    apbase);
}

static void
agp_amd64_uli_init(struct agp_softc *sc)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	pcireg_t apbase;

	agp_amd64_apbase_fixup(sc);
	apbase = pci_conf_read(sc->as_pc, sc->as_tag, AGP_AMD64_ULI_APBASE);
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_AMD64_ULI_APBASE,
	    (apbase & 0x0000000f) | asc->apbase);
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_AMD64_ULI_HTT_FEATURE,
	    asc->apbase);
}

static int
agp_amd64_uli_set_aperture(struct agp_softc *sc, uint32_t aperture)
{
	struct agp_amd64_softc *asc = sc->as_chipc;

	switch (aperture) {
	case 0x02000000:	/*  32 MB */
	case 0x04000000:	/*  64 MB */
	case 0x08000000:	/* 128 MB */
	case 0x10000000:	/* 256 MB */
		break;
	default:
		return EINVAL;
	}

	pci_conf_write(sc->as_pc, sc->as_tag, AGP_AMD64_ULI_ENU_SCR,
	    asc->apbase + aperture - 1);

	return 0;
}

static void
agp_amd64_nvidia_init(struct agp_softc *sc)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	pcireg_t apbase;

	agp_amd64_apbase_fixup(sc);
	apbase =
	    pci_conf_read(sc->as_pc, sc->as_tag, AGP_AMD64_NVIDIA_0_APBASE);
	pci_conf_write(sc->as_pc, sc->as_tag, AGP_AMD64_NVIDIA_0_APBASE,
	    (apbase & 0x0000000f) | asc->apbase);
	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP_AMD64_NVIDIA_1_APBASE1,
	    asc->apbase);
	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP_AMD64_NVIDIA_1_APBASE2,
	    asc->apbase);
}

static int
agp_amd64_nvidia_set_aperture(struct agp_softc *sc, uint32_t aperture)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	uint32_t apsize;

	switch (aperture) {
	case 0x02000000:	apsize = 0x0f;	break;	/*  32 MB */
	case 0x04000000:	apsize = 0x0e;	break;	/*  64 MB */
	case 0x08000000:	apsize = 0x0c;	break;	/* 128 MB */
	case 0x10000000:	apsize = 0x08;	break;	/* 256 MB */
	case 0x20000000:	apsize = 0x00;	break;	/* 512 MB */
	default:
		return EINVAL;
	}

	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP_AMD64_NVIDIA_1_APSIZE,
	    (pci_conf_read(sc->as_pc, asc->ctrl_tag,
	    AGP_AMD64_NVIDIA_1_APSIZE) & 0xfffffff0) | apsize);
	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP_AMD64_NVIDIA_1_APLIMIT1,
	    asc->apbase + aperture - 1);
	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP_AMD64_NVIDIA_1_APLIMIT2,
	    asc->apbase + aperture - 1);

	return 0;
}

static void
agp_amd64_via_init(struct agp_softc *sc)
{
	struct agp_amd64_softc *asc = sc->as_chipc;

	agp_amd64_apbase_fixup(sc);
	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP3_VIA_ATTBASE,
	    asc->gatt->ag_physical);
	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP3_VIA_GARTCTRL,
	    pci_conf_read(sc->as_pc, asc->ctrl_tag, AGP3_VIA_ATTBASE) | 0x180);
}

static int
agp_amd64_via_set_aperture(struct agp_softc *sc, uint32_t aperture)
{
	struct agp_amd64_softc *asc = sc->as_chipc;
	uint32_t apsize;

	apsize = ((aperture - 1) >> 20) ^ 0xff;
	if ((((apsize ^ 0xff) << 20) | ((1 << 20) - 1)) + 1 != aperture)
		return EINVAL;
	pci_conf_write(sc->as_pc, asc->ctrl_tag, AGP3_VIA_APSIZE,
	    (pci_conf_read(sc->as_pc, asc->ctrl_tag, AGP3_VIA_APSIZE) & ~0xff) |
	    apsize);

	return 0;
}
