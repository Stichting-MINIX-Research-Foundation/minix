/* $NetBSD: btvmeivar.h,v 1.5 2012/10/27 17:18:28 chs Exp $ */

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

struct b3_617_vmeintrhand {
	TAILQ_ENTRY(b3_617_vmeintrhand) ih_next;
	int (*ih_fun)(void*);
	void *ih_arg;
	int ih_level;
	int ih_vector;
	int ih_prior;
	u_long ih_count;
};

struct b3_617_softc {
	device_t sc_dev;

	/* tags passed in from PCI */
	pci_chipset_tag_t sc_pc;
	bus_space_tag_t csrt, mapt;
	bus_space_handle_t csrh, maph;
	bus_addr_t vmepbase; /* physical PCI address */
	bus_dma_tag_t sc_dmat;
	void *sc_ih;

	bus_space_tag_t sc_vmet;

	struct vme_range csrwindow, dmawindow24, dmawindow32;

	/* tags passed to VME devices */
	struct vme_chipset_tag sc_vct;

	/* list of VME interrupt handlers */
	TAILQ_HEAD(, b3_617_vmeintrhand) intrhdls;
	int strayintrs;

	/*
	 * management of adapter mapping tables
	 */
	/* max fragmentation of scatter tables */
#define NVMEMAP 20

	struct extent *vmeext;
	char vmemap[EXTENT_FIXED_STORAGE_SIZE(NVMEMAP)];
};

#define read_csr_byte(sc, reg) \
  bus_space_read_1(sc->csrt, sc->csrh, reg)
#define write_csr_byte(sc, reg, val) \
  bus_space_write_1(sc->csrt, sc->csrh, reg, val)
#define read_csr_word(sc, reg) \
  bus_space_read_2(sc->csrt, sc->csrh, reg)
#define write_csr_word(sc, reg, val) \
  bus_space_write_2(sc->csrt, sc->csrh, reg, val)

#define write_mapmem(sc, ofs, val) \
  bus_space_write_4(sc->mapt, sc->maph, ofs, val)
#define read_mapmem(sc, ofs) \
  bus_space_read_4(sc->mapt, sc->maph, ofs)

#define VME_PAGESIZE 0x1000
#define PCI_PAGESIZE 0x1000
#define DMA_PAGESIZE 0x1000

/* shared between driver parts */
int b3_617_reset(struct b3_617_softc*);
int b3_617_init(struct b3_617_softc*);
#ifdef notyet /* for detach */
void b3_617_halt(struct b3_617_softc*);
#endif
int b3_617_intr(void*);
#if 0
void b3_617_cntlrdma_done(struct b3_617_softc*);
#endif

/* exported via tag structs */
int b3_617_map_vme(void *, vme_addr_t, vme_size_t,
			vme_am_t, vme_datasize_t, vme_swap_t,
			bus_space_tag_t *, bus_space_handle_t *,
			vme_mapresc_t*);
void b3_617_unmap_vme(void *, vme_mapresc_t);

int b3_617_vme_probe(void *, vme_addr_t, vme_size_t, vme_am_t,
			  vme_datasize_t,
			  int (*)(void *, bus_space_tag_t, bus_space_handle_t),
			  void *);

int b3_617_map_vmeint(void *, int, int, vme_intr_handle_t *);
void *b3_617_establish_vmeint(void *, vme_intr_handle_t, int,
				   int (*)(void *), void *);
void b3_617_disestablish_vmeint(void *, void *);

int b3_617_dmamap_create(void *, vme_size_t,
			      vme_am_t, vme_datasize_t, vme_swap_t,
			      int, vme_size_t, vme_addr_t,
			      int, bus_dmamap_t *);
void b3_617_dmamap_destroy(void *, bus_dmamap_t);

int b3_617_dmamem_alloc(void *, vme_size_t,
			     vme_am_t, vme_datasize_t, vme_swap_t,
			     bus_dma_segment_t *, int, int *, int);
void b3_617_dmamem_free(void *, bus_dma_segment_t *, int);
