/* $NetBSD: btvmeii.c,v 1.22 2012/10/27 17:18:28 chs Exp $ */

/*
 * Copyright (c) 1999
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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

/*
 * Driver for the Bit3/SBS PCI-VME adapter Model 2706.
 * Uses the common Tundra Universe code.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: btvmeii.c,v 1.22 2012/10/27 17:18:28 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/extent.h>

#include <dev/pci/ppbreg.h>

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/pci/universe_pci_var.h>

static int b3_2706_match(device_t, cfdata_t, void *);
static void b3_2706_attach(device_t, device_t, void *);

/* exported via tag structs */
int b3_2706_map_vme(void *, vme_addr_t, vme_size_t,
		      vme_am_t, vme_datasize_t, vme_swap_t,
		      bus_space_tag_t *, bus_space_handle_t *, vme_mapresc_t*);
void b3_2706_unmap_vme(void *, vme_mapresc_t);

int b3_2706_vme_probe(void *, vme_addr_t, vme_size_t, vme_am_t,
			vme_datasize_t,
			int (*)(void *, bus_space_tag_t, bus_space_handle_t),
			void *);

int b3_2706_map_vmeint(void *, int, int, vme_intr_handle_t *);
void *b3_2706_establish_vmeint(void *, vme_intr_handle_t, int,
				 int (*)(void *), void *);
void b3_2706_disestablish_vmeint(void *, void *);
void b3_2706_vmeint(void *, int, int);

int b3_2706_dmamap_create(void *, vme_size_t,
			    vme_am_t, vme_datasize_t, vme_swap_t,
			    int, vme_size_t, vme_addr_t,
			    int, bus_dmamap_t *);
void b3_2706_dmamap_destroy(void *, bus_dmamap_t);

int b3_2706_dmamem_alloc(void *, vme_size_t,
			      vme_am_t, vme_datasize_t, vme_swap_t,
			      bus_dma_segment_t *, int, int *, int);
void b3_2706_dmamem_free(void *, bus_dma_segment_t *, int);

struct b3_2706_vmemaprescs {
	int wnd;
	unsigned long pcibase, maplen;
	bus_space_handle_t handle;
	u_int32_t len;
};

struct b3_2706_vmeintrhand {
	TAILQ_ENTRY(b3_2706_vmeintrhand) ih_next;
	int (*ih_fun)(void*);
	void *ih_arg;
	int ih_level;
	int ih_vector;
	int ih_prior;
	u_long ih_count;
};

struct b3_2706_softc {
	struct univ_pci_data univdata;
	bus_space_tag_t swapt, vmet;
	bus_space_handle_t swaph;
	bus_addr_t vmepbase;

	int windowused[8];
	struct b3_2706_vmemaprescs vmemaprescs[8];
	struct extent *vmeext;
	char vmemap[EXTENT_FIXED_STORAGE_SIZE(8)];

	struct vme_chipset_tag sc_vct;

	/* list of VME interrupt handlers */
	TAILQ_HEAD(, b3_2706_vmeintrhand) intrhdls;
	int strayintrs;
};

CFATTACH_DECL_NEW(btvmeii, sizeof(struct b3_2706_softc),
    b3_2706_match, b3_2706_attach, NULL, NULL);

/*
 * The adapter consists of a DEC PCI-PCI-bridge with two
 * PCI devices behind it: A Tundra Universe as device 4 and
 * some FPGA with glue logics as device 8.
 * As long as the autoconf code doesn't provide more support
 * for dependent devices, we have to duplicate a part of the
 * "ppb" functions here.
 */

static int
b3_2706_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	int secbus;
	pcitag_t tag;
	pcireg_t id;

	if ((PCI_VENDOR(pa->pa_id) != PCI_VENDOR_DEC)
	    || (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_DEC_21152))
		return (0);

	secbus = PPB_BUSINFO_SECONDARY(pci_conf_read(pc, pa->pa_tag,
						     PPB_REG_BUSINFO));
	if (secbus == 0) {
		printf("b3_2706_match: ppb not configured\n");
		return (0);
	}

	tag = pci_make_tag(pc, secbus, 4, 0);
	id = pci_conf_read(pc, tag, PCI_ID_REG);

	if ((PCI_VENDOR(id) != PCI_VENDOR_NEWBRIDGE)
	    || (PCI_PRODUCT(id) != PCI_PRODUCT_NEWBRIDGE_CA91CX42)) {
#ifdef DEBUG
		printf("b3_2706_match: no tundra\n");
#endif
		return (0);
	}

	tag = pci_make_tag(pc, secbus, 8, 0);
	id = pci_conf_read(pc, tag, PCI_ID_REG);

	if ((PCI_VENDOR(id) != PCI_VENDOR_BIT3)
	    || (PCI_PRODUCT(id) != PCI_PRODUCT_BIT3_PCIVME2706)) {
#ifdef DEBUG
		printf("b3_2706_match: no bit3 chip\n");
#endif
		return (0);
	}

	return (5); /* beat "ppb" */
}

static void
b3_2706_attach(device_t parent, device_t self, void *aux)
{
	struct b3_2706_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	struct pci_attach_args aa;
	int secbus;
	pcireg_t intr;
	pcitag_t tag;
	bus_addr_t swappbase;
	int i;

	struct vmebus_attach_args vaa;

	aprint_naive(": VME bus adapter\n");
	aprint_normal("\n");

	secbus = PPB_BUSINFO_SECONDARY(pci_conf_read(pc, pa->pa_tag,
						     PPB_REG_BUSINFO));

	memcpy(&aa, pa, sizeof(struct pci_attach_args));
	aa.pa_device = 4;
	aa.pa_function = 0;
	aa.pa_tag = pci_make_tag(pc, secbus, 4, 0);
	aa.pa_intrswiz += 4;
	intr = pci_conf_read(pc, aa.pa_tag, PCI_INTERRUPT_REG);
	/*
	 * swizzle it based on the number of
	 * busses we're behind and our device
	 * number.
	 */
	aa.pa_intrpin =	((1 + aa.pa_intrswiz - 1) % 4) + 1;
	aa.pa_intrline = PCI_INTERRUPT_LINE(intr);

	if (univ_pci_attach(&sc->univdata, &aa, device_xname(self),
			    b3_2706_vmeint, sc)) {
		aprint_error_dev(self, "error initializing universe chip\n");
		return;
	}

	/*
	 * don't waste KVM - the byteswap register is aliased in
	 * a 512k window, we need it only once
	 */
	tag = pci_make_tag(pc, secbus, 8, 0);
	sc->swapt = pa->pa_memt;
	if (pci_mapreg_info(pc, tag, 0x10,
			    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			    &swappbase, 0, 0) ||
	    bus_space_map(sc->swapt, swappbase, 4, 0, &sc->swaph)) {
		aprint_error_dev(self, "can't map byteswap register\n");
		return;
	}
	/*
	 * Set up cycle specific byteswap mode.
	 * XXX Readback yields "all-ones" for me, and it doesn't seem
	 * to matter what I write into the register - the data don't
	 * get swapped. Adapter fault or documentation bug?
	 */
	bus_space_write_4(sc->swapt, sc->swaph, 0, 0x00000490);

	/* VME space is mapped as needed */
	sc->vmet = pa->pa_memt;
	if (pci_mapreg_info(pc, tag, 0x14,
			    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			    &sc->vmepbase, 0, 0)) {
		aprint_error_dev(self, "VME range not assigned\n");
		return;
	}
#ifdef BIT3DEBUG
	aprint_debug_dev(self, "VME window @%lx\n",
	    (long)sc->vmepbase);
#endif

	for (i = 0; i < 8; i++) {
		sc->windowused[i] = 0;
	}
	sc->vmeext = extent_create("pcivme", sc->vmepbase,
				   sc->vmepbase + 32*1024*1024 - 1, M_DEVBUF,
				   sc->vmemap, sizeof(sc->vmemap),
				   EX_NOCOALESCE);

	sc->sc_vct.cookie = self;
	sc->sc_vct.vct_probe = b3_2706_vme_probe;
	sc->sc_vct.vct_map = b3_2706_map_vme;
	sc->sc_vct.vct_unmap = b3_2706_unmap_vme;
	sc->sc_vct.vct_int_map = b3_2706_map_vmeint;
	sc->sc_vct.vct_int_establish = b3_2706_establish_vmeint;
	sc->sc_vct.vct_int_disestablish = b3_2706_disestablish_vmeint;
	sc->sc_vct.vct_dmamap_create = b3_2706_dmamap_create;
	sc->sc_vct.vct_dmamap_destroy = b3_2706_dmamap_destroy;
	sc->sc_vct.vct_dmamem_alloc = b3_2706_dmamem_alloc;
	sc->sc_vct.vct_dmamem_free = b3_2706_dmamem_free;

	vaa.va_vct = &(sc->sc_vct);
	vaa.va_bdt = pa->pa_dmat; /* XXX */
	vaa.va_slaveconfig = 0; /* XXX CSR window? */

	config_found(self, &vaa, 0);
}

#define sc ((struct b3_2706_softc*)vsc)

int
b3_2706_map_vme(void *vsc, vme_addr_t vmeaddr, vme_size_t len, vme_am_t am, vme_datasize_t datasizes, vme_swap_t swap, bus_space_tag_t *tag, bus_space_handle_t *handle, vme_mapresc_t *resc)
{
	int idx, i, wnd, res;
	unsigned long boundary, maplen, pcibase;
	vme_addr_t vmebase, vmeend;
	static int windoworder[8] = {1, 2, 3, 5, 6, 7, 0, 4};

	/* prefer windows with fine granularity for small mappings */
	wnd = -1;
	if (len <= 32*1024)
		idx = 6;
	else
		idx = 0;
	for (i = 0; i < 8; i++) {
		if (!sc->windowused[windoworder[idx]]) {
			wnd = windoworder[idx];
			sc->windowused[wnd] = 1;
			break;
		}
		idx = (idx + 1) % 8;
	}
	if (wnd == -1)
		return (ENOSPC);

	boundary = (wnd & 3) ? 64*1024 : 4*1024;

	/* first mapped address */
	vmebase = vmeaddr & ~(boundary - 1);
	/* base of last mapped page */
	vmeend = (vmeaddr + len - 1) & ~(boundary - 1);
	/* bytes in outgoing window required */
	maplen = vmeend - vmebase + boundary;

	if (extent_alloc(sc->vmeext, maplen, boundary, 0, EX_FAST, &pcibase)) {
		sc->windowused[wnd] = 0;
		return (ENOMEM);
	}

	res = univ_pci_mapvme(&sc->univdata, wnd, vmebase, maplen,
			      am, datasizes, pcibase);
	if (res) {
		extent_free(sc->vmeext, pcibase, maplen, 0);
		sc->windowused[wnd] = 0;
		return (res);
	}

	res = bus_space_map(sc->vmet, pcibase + (vmeaddr - vmebase), len,
			    0, handle);
	if (res) {
		univ_pci_unmapvme(&sc->univdata, wnd);
		extent_free(sc->vmeext, pcibase, maplen, 0);
		sc->windowused[wnd] = 0;
		return (res);
	}

	*tag = sc->vmet;

	/*
	 * save all data needed for later unmapping
	 */
	sc->vmemaprescs[wnd].wnd = wnd;
	sc->vmemaprescs[wnd].pcibase = pcibase;
	sc->vmemaprescs[wnd].maplen = maplen;
	sc->vmemaprescs[wnd].handle = *handle;
	sc->vmemaprescs[wnd].len = len;
	*resc = &sc->vmemaprescs[wnd];
	return (0);
}

void
b3_2706_unmap_vme(void *vsc, vme_mapresc_t resc)
{
	struct b3_2706_vmemaprescs *r = resc;

	bus_space_unmap(sc->vmet, r->handle, r->len);
	extent_free(sc->vmeext, r->pcibase, r->maplen, 0);

	if (!sc->windowused[r->wnd])
		panic("b3_2706_unmap_vme: bad window");
	univ_pci_unmapvme(&sc->univdata, r->wnd);
	sc->windowused[r->wnd] = 0;
}

int
b3_2706_vme_probe(void *vsc, vme_addr_t addr, vme_size_t len, vme_am_t am, vme_datasize_t datasize, int (*callback)(void *, bus_space_tag_t, bus_space_handle_t), void *cbarg)
{
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	vme_mapresc_t resc;
	int res, i;
	volatile u_int32_t dummy;

	res = b3_2706_map_vme(vsc, addr, len, am, datasize, 0,
			      &tag, &handle, &resc);
	if (res)
		return (res);

	if (univ_pci_vmebuserr(&sc->univdata, 1))
		printf("b3_2706_vme_badaddr: TA bit not clean - reset\n");

	if (callback)
		res = (*callback)(cbarg, tag, handle);
	else {
		for (i = 0; i < len;) {
			switch (datasize) {
			    case VME_D8:
				dummy = bus_space_read_1(tag, handle, i);
				i++;
				break;
			    case VME_D16:
				dummy = bus_space_read_2(tag, handle, i);
				i += 2;
				break;
			    case VME_D32:
				dummy = bus_space_read_4(tag, handle, i);
				i += 4;
				break;
			    default:
				panic("b3_2706_vme_probe: invalid datasize %x",
				      datasize);
			}
		}
	}

	if (univ_pci_vmebuserr(&sc->univdata, 0)) {
#ifdef BIT3DEBUG
		printf("b3_2706_vme_badaddr: caught TA\n");
#endif
		univ_pci_vmebuserr(&sc->univdata, 1);
		res = EIO;
	}

	b3_2706_unmap_vme(vsc, resc);
	return (res);
}

int
b3_2706_map_vmeint(void *vsc, int level, int vector, vme_intr_handle_t *handlep)
{

	*handlep = (void *)(long)((level << 8) | vector); /* XXX */
	return (0);
}

void *
b3_2706_establish_vmeint(void *vsc, vme_intr_handle_t handle, int prior, int (*func)(void *), void *arg)
{
	struct b3_2706_vmeintrhand *ih;
	long lv;
	int s;

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("b3_2706_map_vmeint: can't malloc handler info");

	lv = (long)handle; /* XXX */

	ih->ih_fun = func;
	ih->ih_arg = arg;
	ih->ih_level = lv >> 8;
	ih->ih_vector = lv & 0xff;
	ih->ih_prior = prior;
	ih->ih_count = 0;

	s = splhigh();
	TAILQ_INSERT_TAIL(&(sc->intrhdls), ih, ih_next);
	splx(s);

	return (ih);
}

void
b3_2706_disestablish_vmeint(void *vsc, void *cookie)
{
	struct b3_2706_vmeintrhand *ih = cookie;
	int s;

	if (!ih) {
		printf("b3_2706_unmap_vmeint: NULL arg\n");
		return;
	}

	s = splhigh();
	TAILQ_REMOVE(&(sc->intrhdls), ih, ih_next);
	splx(s);

	free(ih, M_DEVBUF);
}

void
b3_2706_vmeint(void *vsc, int level, int vector)
{
	struct b3_2706_vmeintrhand *ih;
	int found;

#ifdef BIT3DEBUG
	printf("b3_2706_vmeint: VME IRQ %d, vec %x\n", level, vector);
#endif
	found = 0;

	for (ih = sc->intrhdls.tqh_first; ih;
	     ih = ih->ih_next.tqe_next) {
		if ((ih->ih_level == level) &&
		    ((ih->ih_vector == -1) ||
		     (ih->ih_vector == vector))) {
			int s, res;
			/*
			 * We should raise the interrupt level
			 * to ih->ih_prior here. How to do this
			 * machine-independently?
			 * To be safe, raise to the maximum.
			 */
			s = splhigh();
			found |= (res = (*(ih->ih_fun))(ih->ih_arg));
			splx(s);
			if (res)
				ih->ih_count++;
			if (res == 1)
				break;
		}
	}
	if (!found)
		sc->strayintrs++;
}

int
b3_2706_dmamap_create(vsc, len, am, datasize, swap,
		      nsegs, segsz, bound,
		      flags, mapp)
	void *vsc;
	vme_size_t len;
	vme_am_t am;
	vme_datasize_t datasize;
	vme_swap_t swap;
	int nsegs;
	vme_size_t segsz;
	vme_addr_t bound;
	int flags;
	bus_dmamap_t *mapp;
{
	return (EINVAL);
}

void
b3_2706_dmamap_destroy(void *vsc, bus_dmamap_t map)
{
}

int
b3_2706_dmamem_alloc(void *vsc, vme_size_t len, vme_am_t am, vme_datasize_t datasizes, vme_swap_t swap, bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags)
{
	return (EINVAL);
}

void
b3_2706_dmamem_free(void *vsc, bus_dma_segment_t *segs, int nsegs)
{
}

#undef sc
