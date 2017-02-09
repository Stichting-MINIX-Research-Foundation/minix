/* $NetBSD: btvmei.c,v 1.30 2014/03/29 19:28:24 christos Exp $ */

/*
 * Copyright (c) 1999
 *	Matthias Drochner.  All rights reserved.
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
 *    derived from this software without specific prior written permission.
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: btvmei.c,v 1.30 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <sys/extent.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/pci/btvmeireg.h>
#include <dev/pci/btvmeivar.h>

static int b3_617_match(device_t, cfdata_t, void *);
static void b3_617_attach(device_t, device_t, void *);
#ifdef notyet
static int b3_617_detach(device_t);
#endif
void b3_617_slaveconfig(device_t, struct vme_attach_args *);

static void b3_617_vmeintr(struct b3_617_softc *, unsigned char);

/*
 * mapping ressources, needed for deallocation
 */
struct b3_617_vmeresc {
	bus_space_handle_t handle;
	bus_size_t len;
	int firstpage, maplen;
};

CFATTACH_DECL_NEW(btvmei, sizeof(struct b3_617_softc),
    b3_617_match, b3_617_attach, NULL, NULL);

static int
b3_617_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if ((PCI_VENDOR(pa->pa_id) != PCI_VENDOR_BIT3)
	    || (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BIT3_PCIVME617))
		return (0);
	return (1);
}

static void
b3_617_attach(device_t parent, device_t self, void *aux)
{
	struct b3_617_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;

	pci_intr_handle_t ih;
	const char *intrstr;
	struct vmebus_attach_args vaa;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_pc = pc;
	sc->sc_dmat = pa->pa_dmat;

	pci_aprint_devinfo_fancy(pa, "VME bus adapter", "BIT3 PCI-VME 617", 1);

	/*
	 * Map CSR and mapping table spaces.
	 * Don't map VME window; parts are mapped as needed to
	 * save kernel virtual memory space
	 */
	if (pci_mapreg_map(pa, 0x14,
			   PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			   0, &sc->csrt, &sc->csrh, NULL, NULL) &&
	    pci_mapreg_map(pa, 0x10,
			   PCI_MAPREG_TYPE_IO,
			   0, &sc->csrt, &sc->csrh, NULL, NULL)) {
		aprint_error_dev(self, "can't map CSR space\n");
		return;
	}

	if (pci_mapreg_map(pa, 0x18,
			   PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			   0, &sc->mapt, &sc->maph, NULL, NULL)) {
		aprint_error_dev(self, "can't map map space\n");
		return;
	}

	if (pci_mapreg_info(pc, pa->pa_tag, 0x1c,
			    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			    &sc->vmepbase, 0, 0)) {
		aprint_error_dev(self, "can't get VME range\n");
		return;
	}
	sc->sc_vmet = pa->pa_memt; /* XXX needed for VME mappings */

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	/*
	 * Use a low interrupt level (the lowest?).
	 * We will raise before calling a subdevice's handler.
	 */
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, b3_617_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	if (b3_617_init(sc))
		return;

	/*
	 * set up all the tags for use by VME devices
	 */
	sc->sc_vct.cookie = self;
	sc->sc_vct.vct_probe = b3_617_vme_probe;
	sc->sc_vct.vct_map = b3_617_map_vme;
	sc->sc_vct.vct_unmap = b3_617_unmap_vme;
	sc->sc_vct.vct_int_map = b3_617_map_vmeint;
	sc->sc_vct.vct_int_establish = b3_617_establish_vmeint;
	sc->sc_vct.vct_int_disestablish = b3_617_disestablish_vmeint;
	sc->sc_vct.vct_dmamap_create = b3_617_dmamap_create;
	sc->sc_vct.vct_dmamap_destroy = b3_617_dmamap_destroy;
	sc->sc_vct.vct_dmamem_alloc = b3_617_dmamem_alloc;
	sc->sc_vct.vct_dmamem_free = b3_617_dmamem_free;

	vaa.va_vct = &(sc->sc_vct);
	vaa.va_bdt = pa->pa_dmat;
	vaa.va_slaveconfig = b3_617_slaveconfig;

	sc->csrwindow.offset = -1;
	sc->dmawindow24.offset = -1;
	sc->dmawindow32.offset = -1;
	config_found(self, &vaa, 0);
}

#ifdef notyet
static int
b3_617_detach(device_t dev)
{
	struct b3_617_softc *sc = device_private(dev);

	b3_617_halt(sc);

	if (sc->sc_ih)
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);

	bus_space_unmap(sc->sc_bc, sc->csrbase, 32);
	bus_space_unmap(sc->sc_bc, sc->mapbase, 64*1024);

	return(0);
}
#endif

void
b3_617_slaveconfig(device_t dev, struct vme_attach_args *va)
{
	struct b3_617_softc *sc = device_private(dev);
	vme_chipset_tag_t vmect;
	int i, res;
	const char *name = 0; /* XXX gcc! */

	vmect = &sc->sc_vct;
	if (!va)
		goto freeit;

#ifdef DIAGNOSTIC
	if (vmect != va->va_vct)
		panic("pcivme_slaveconfig: chipset tag?");
#endif

	for (i = 0; i < va->numcfranges; i++) {
		res = vme_space_alloc(vmect, va->r[i].offset,
				      va->r[i].size, va->r[i].am);
		if (res)
			panic("%s: can't alloc slave window %x/%x/%x",
			       device_xname(dev), va->r[i].offset,
			       va->r[i].size, va->r[i].am);

		switch (va->r[i].am & VME_AM_ADRSIZEMASK) {
			/* structure assignments! */
		    case VME_AM_A16:
			sc->csrwindow = va->r[i];
			name = "VME CSR";
			break;
		    case VME_AM_A24:
			sc->dmawindow24 = va->r[i];
			name = "A24 DMA";
			break;
		    case VME_AM_A32:
			sc->dmawindow32 = va->r[i];
			name = "A32 DMA";
			break;
		}
		printf("%s: %s window: %x-%x\n", device_xname(dev),
		       name, va->r[i].offset,
		       va->r[i].offset + va->r[i].size - 1);
	}
	return;

freeit:
	if (sc->csrwindow.offset != -1)
		vme_space_free(vmect, sc->csrwindow.offset,
			       sc->csrwindow.size, sc->csrwindow.am);
	if (sc->dmawindow32.offset != -1)
		vme_space_free(vmect, sc->dmawindow32.offset,
			       sc->dmawindow32.size, sc->dmawindow32.am);
	if (sc->dmawindow24.offset != -1)
		vme_space_free(vmect, sc->dmawindow24.offset,
			       sc->dmawindow24.size, sc->dmawindow24.am);
}

int
b3_617_reset(struct b3_617_softc *sc)
{
	unsigned char status;

	/* reset sequence, ch 5.2 */
	status = read_csr_byte(sc, LOC_STATUS);
	if (status & LSR_NO_CONNECT) {
		printf("%s: not connected\n", device_xname(sc->sc_dev));
		return (-1);
	}
	status = read_csr_byte(sc, REM_STATUS); /* discard */
	write_csr_byte(sc, LOC_CMD1, LC1_CLR_ERROR);
	status = read_csr_byte(sc, LOC_STATUS);
	if (status & LSR_CERROR_MASK) {
		char sbuf[sizeof(BIT3_LSR_BITS) + 64];

		snprintb(sbuf, sizeof(sbuf), BIT3_LSR_BITS, status);
		printf("%s: interface error, lsr=%s\n", device_xname(sc->sc_dev),
		       sbuf);
		return (-1);
	}
	return (0);
}

int
b3_617_init(struct b3_617_softc *sc)
{
	unsigned int i;

	if (b3_617_reset(sc))
		return (-1);

	/* all maps invalid */
	for (i = MR_PCI_VME; i < MR_PCI_VME + MR_PCI_VME_SIZE; i += 4)
		write_mapmem(sc, i, MR_RAM_INVALID);
	for (i = MR_VME_PCI; i < MR_VME_PCI + MR_VME_PCI_SIZE; i += 4)
		write_mapmem(sc, i, MR_RAM_INVALID);
	for (i = MR_DMA_PCI; i < MR_DMA_PCI + MR_DMA_PCI_SIZE; i += 4)
		write_mapmem(sc, i, MR_RAM_INVALID);

	/*
	 * set up scatter page allocation control
	 */
	sc->vmeext = extent_create("pcivme", MR_PCI_VME,
				   MR_PCI_VME + MR_PCI_VME_SIZE - 1,
				   sc->vmemap, sizeof(sc->vmemap),
				   EX_NOCOALESCE);
#if 0
	sc->pciext = extent_create("vmepci", MR_VME_PCI,
				   MR_VME_PCI + MR_VME_PCI_SIZE - 1,
				   sc->pcimap, sizeof(sc->pcimap),
				   EX_NOCOALESCE);
	sc->dmaext = extent_create("dmapci", MR_DMA_PCI,
				   MR_DMA_PCI + MR_DMA_PCI_SIZE - 1,
				   sc->dmamap, sizeof(sc->dmamap),
				   EX_NOCOALESCE);
#endif

	/*
	 * init int handler queue,
	 * enable interrupts if PCI interrupt available
	 */
	TAILQ_INIT(&(sc->intrhdls));
	sc->strayintrs = 0;

	if (sc->sc_ih)
		write_csr_byte(sc, LOC_INT_CTRL, LIC_INT_ENABLE);
	/* no error ints */
	write_csr_byte(sc, REM_CMD2, 0); /* enables VME IRQ */

	return (0);
}

#ifdef notyet /* for detach */
void
b3_617_halt(struct b3_617_softc *sc)
{
	/*
	 * because detach code checks for existence of children,
	 * all ressources (mappings, VME IRQs, DMA requests)
	 * should be deallocated at this point
	 */

	/* disable IRQ */
	write_csr_byte(sc, LOC_INT_CTRL, 0);
}
#endif

static void
b3_617_vmeintr(struct b3_617_softc *sc, unsigned char lstat)
{
	int level;

	for (level = 7; level >= 1; level--) {
		unsigned char vector;
		struct b3_617_vmeintrhand *ih;
		int found;

		if (!(lstat & (1 << level)))
			continue;

		write_csr_byte(sc, REM_CMD1, level);
		vector = read_csr_byte(sc, REM_IACK);

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
}

#define sc ((struct b3_617_softc*)vsc)

int
b3_617_map_vme(void *vsc, vme_addr_t vmeaddr, vme_size_t len, vme_am_t am, vme_datasize_t datasizes, vme_swap_t swap, bus_space_tag_t *tag, bus_space_handle_t *handle, vme_mapresc_t *resc)
{
	vme_addr_t vmebase, vmeend, va;
	unsigned long maplen, first, i;
	u_int32_t mapreg;
	bus_addr_t pcibase;
	int res;
	struct b3_617_vmeresc *r;

	/* first mapped address */
	vmebase = vmeaddr & ~(VME_PAGESIZE - 1);
	/* base of last mapped page */
	vmeend = (vmeaddr + len - 1) & ~(VME_PAGESIZE - 1);
	/* bytes in scatter table required */
	maplen = ((vmeend - vmebase) / VME_PAGESIZE + 1) * 4;

	if (extent_alloc(sc->vmeext, maplen, 4, 0, EX_FAST, &first))
		return (ENOMEM);

	/*
	 * set up adapter mapping registers
	 */
	mapreg = (am << MR_AMOD_SHIFT) | MR_FC_RRAM | swap;

	for (i = first, va = vmebase;
	     i < first + maplen;
	     i += 4, va += VME_PAGESIZE) {
		write_mapmem(sc, i, mapreg | va);
#ifdef BIT3DEBUG
		printf("mapreg@%lx=%x\n", i, read_mapmem(sc, i));
#endif
	}

#ifdef DIAGNOSTIC
	if (va != vmeend + VME_PAGESIZE)
		panic("b3_617_map_pci_vme: botch");
#endif
	/*
	 * map needed range in PCI space
	 */
	pcibase = sc->vmepbase + (first - MR_PCI_VME) / 4 * VME_PAGESIZE
	    + (vmeaddr & (VME_PAGESIZE - 1));

	if ((res = bus_space_map(sc->sc_vmet, pcibase, len, 0, handle))) {
		for (i = first; i < first + maplen; i += 4)
			write_mapmem(sc, i, MR_RAM_INVALID);
		extent_free(sc->vmeext, first, maplen, 0);
		return (res);
	}

	*tag = sc->sc_vmet;

	/*
	 * save all data needed for later unmapping
	 */
	r = malloc(sizeof(*r), M_DEVBUF, M_NOWAIT); /* XXX check! */
	r->handle = *handle;
	r->len = len;
	r->firstpage = first;
	r->maplen = maplen;
	*resc = r;
	return (0);
}

void
b3_617_unmap_vme(void *vsc, vme_mapresc_t resc)
{
	unsigned long i;
	struct b3_617_vmeresc *r = resc;

	/* unmap PCI window */
	bus_space_unmap(sc->sc_vmet, r->handle, r->len);

	for (i = r->firstpage; i < r->firstpage + r->maplen; i += 4)
		write_mapmem(sc, i, MR_RAM_INVALID);

	extent_free(sc->vmeext, r->firstpage, r->maplen, 0);

	free(r, M_DEVBUF);
}

int
b3_617_vme_probe(void *vsc, vme_addr_t addr, vme_size_t len, vme_am_t am, vme_datasize_t datasize, int (*callback)(void *, bus_space_tag_t, bus_space_handle_t), void *cbarg)
{
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	vme_mapresc_t resc;
	int res, i;
	volatile u_int32_t dummy;
	int status;

	res = b3_617_map_vme(vsc, addr, len, am, 0, 0,
			     &tag, &handle, &resc);
	if (res)
		return (res);

	if (read_csr_byte(sc, LOC_STATUS) & LSR_ERROR_MASK) {
		printf("b3_617_vme_badaddr: error bit not clean - resetting\n");
		write_csr_byte(sc, LOC_CMD1, LC1_CLR_ERROR);
	}

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
				panic("b3_617_vme_probe: invalid datasize %x",
				      datasize);
			}
		}
	}

	if ((status = read_csr_byte(sc, LOC_STATUS)) & LSR_ERROR_MASK) {
#ifdef BIT3DEBUG
		printf("b3_617_vme_badaddr: caught error %x\n", status);
#endif
		write_csr_byte(sc, LOC_CMD1, LC1_CLR_ERROR);
		res = EIO;
	}

	b3_617_unmap_vme(vsc, resc);
	return (res);
}

int
b3_617_map_vmeint(void *vsc, int level, int vector, vme_intr_handle_t *handlep)
{
	if (!sc->sc_ih) {
		printf("%s: b3_617_map_vmeint: no IRQ\n",
		       device_xname(sc->sc_dev));
		return (ENXIO);
	}
	/*
	 * We should check whether the interface can pass this interrupt
	 * level at all, but we don't know much about the jumper setting.
	 */
	*handlep = (void *)(long)((level << 8) | vector); /* XXX */
	return (0);
}

void *
b3_617_establish_vmeint(void *vsc, vme_intr_handle_t handle, int prior, int (*func)(void *), void *arg)
{
	struct b3_617_vmeintrhand *ih;
	long lv;
	int s;

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("b3_617_map_vmeint: can't malloc handler info");

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
b3_617_disestablish_vmeint(void *vsc, void *cookie)
{
	struct b3_617_vmeintrhand *ih = cookie;
	int s;

	if (!ih) {
		printf("b3_617_unmap_vmeint: NULL arg\n");
		return;
	}

	s = splhigh();
	TAILQ_REMOVE(&(sc->intrhdls), ih, ih_next);
	splx(s);

	free(ih, M_DEVBUF);
}

int
b3_617_intr(void *vsc)
{
	int handled = 0;

	/* follows ch. 5.5.5 (reordered for speed) */
	while (read_csr_byte(sc, LOC_INT_CTRL) & LIC_INT_PENDING) {
		unsigned char lstat;

		handled = 1;

		/* no error interrupts! */

		lstat = read_csr_byte(sc, LDMA_CMD);
		if ((lstat & LDC_DMA_DONE) && (lstat & LDC_DMA_INT_ENABLE)) {
			/* DMA done indicator flag */
			write_csr_byte(sc, LDMA_CMD, lstat & (~LDC_DMA_DONE));
#if 0
			b3_617_cntlrdma_done(sc);
#endif
			continue;
		}

		lstat = read_csr_byte(sc, LOC_INT_STATUS);
		if (lstat & LIS_CINT_MASK) {
			/* VME backplane interrupt, ch. 5.5.3 */
			b3_617_vmeintr(sc, lstat);
		}

		/* for now, ignore "mailbox interrupts" */

		lstat = read_csr_byte(sc, LOC_STATUS);
		if (lstat & LSR_PR_STATUS) {
			/* PR interrupt received from REMOTE  */
			write_csr_byte(sc, LOC_CMD1, LC1_CLR_PR_INT);
			continue;
		}

		lstat = read_csr_byte(sc, REM_STATUS);
		if (lstat & RSR_PT_STATUS) {
			/* PT interrupt is set */
			write_csr_byte(sc, REM_CMD1, RC1_CLR_PT_INT);
			continue;
		}
	}
	return (handled);
}

int
b3_617_dmamap_create(vsc, len, am, datasize, swap,
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
b3_617_dmamap_destroy(void *vsc, bus_dmamap_t map)
{
}

int
b3_617_dmamem_alloc(vsc, len, am, datasizes, swap,
		    segs, nsegs, rsegs, flags)
	void *vsc;
	vme_size_t len;
	vme_am_t am;
	vme_datasize_t datasizes;
	vme_swap_t swap;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	return (EINVAL);
}

void
b3_617_dmamem_free(void *vsc, bus_dma_segment_t *segs, int nsegs)
{
}

#undef sc
