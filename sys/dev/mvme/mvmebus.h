/*	$NetBSD: mvmebus.h,v 1.4 2012/10/27 17:18:27 chs Exp $	*/

/*-
 * Copyright (c) 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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

#ifndef _MVME_MVMEBUS_H
#define _MVME_MVMEBUS_H

/*
 * VMEbus master and slave windows are described using
 * instances of this structure.
 *
 * The chip-specific code records the details of the available mappings
 * for the MVME board.
 */
struct mvmebus_range {
	vme_am_t	vr_am;		/* Address modifier (A16, A24, A32) */
	vme_datasize_t	vr_datasize;	/* Datasize: logical OR of D8,D16,D32 */
	paddr_t		vr_locstart;	/* CPU-relative address of mapping */
	paddr_t		vr_mask;	/* Mask to apply to this mapping */
	vme_addr_t	vr_vmestart;	/* VMEbus start address */
	vme_addr_t	vr_vmeend;	/* VMEbus end address */
};

/* Assigned to vr_am to specify the mapping is not valid */
#define MVMEBUS_AM_DISABLED	((vme_am_t)-1)

/* For slave mappings, these specify the slave's capabilities */
#define MVMEBUS_AM_CAP_DATA	0x0100
#define MVMEBUS_AM_CAP_PROG	0x0200
#define MVMEBUS_AM_CAP_BLK	0x0400
#define MVMEBUS_AM_CAP_BLKD64	0x0800
#define MVMEBUS_AM_CAP_USER	0x1000
#define MVMEBUS_AM_CAP_SUPER	0x2000
#define MVMEBUS_AM_HAS_CAP(x)	(((x) & 0x3f00) != 0)

#define MVMEBUS_AM2CAP(am)	(_mvmebus_am_cap[((am) & \
				    (VME_AM_MODEMASK | VME_AM_PRIVMASK))])


/*
 * This records VMEbus-specific details of a region of bus_space_map'd
 * VMEbus address space.
 */
struct mvmebus_mapresc {
	bus_space_handle_t	mr_handle;
	bus_addr_t		mr_addr;
	bus_size_t		mr_size;
	vme_am_t		mr_am;
	vme_datasize_t		mr_datasize;
	int			mr_range;
};


/*
 * This records the VMEbus-specific details of a region of phyisical
 * memory accessible through a VMEbus slave map.
 */
struct mvmebus_dmamap {
	vme_am_t		vm_am;
	vme_datasize_t		vm_datasize;
	vme_swap_t		vm_swap;
	struct mvmebus_range	*vm_slave;
};


struct mvmebus_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bust;
	bus_dma_tag_t		sc_dmat;
	short			sc_irqref[8];
	void			*sc_chip;
	int			sc_nmasters;
	struct mvmebus_range	*sc_masters;
	int			sc_nslaves;
	struct mvmebus_range	*sc_slaves;
	struct evcnt		sc_evcnt[7];
	void			(*sc_intr_establish)(void *, int, int, int, int,
				    int (*)(void *), void *, struct evcnt *);
	void			(*sc_intr_disestablish)(void *, int, int, int,
				    struct evcnt *);
	struct vme_chipset_tag	sc_vct;
	struct mvme68k_bus_dma_tag	sc_mvmedmat;
};


void	mvmebus_attach(struct mvmebus_softc *);
int	mvmebus_map(void *, vme_addr_t, vme_size_t, vme_am_t,
	    vme_datasize_t, vme_swap_t, bus_space_tag_t *,
	    bus_space_handle_t *, vme_mapresc_t *);
void	mvmebus_unmap(void *, vme_mapresc_t);
int	mvmebus_probe(void *, vme_addr_t, vme_size_t,
	    vme_am_t, vme_datasize_t,
	    int (*) (void *, bus_space_tag_t, bus_space_handle_t), void *arg);
int	mvmebus_intmap(void *, int, int, vme_intr_handle_t *);
const struct evcnt *mvmebus_intr_evcnt(void *, vme_intr_handle_t);
void *	mvmebus_intr_establish(void *, vme_intr_handle_t, int,
	    int (*) (void *), void *);
void	mvmebus_intr_disestablish(void *, vme_intr_handle_t);

int	mvmebus_dmamap_create(void *, vme_size_t, vme_am_t, vme_datasize_t,
	    vme_swap_t, int, vme_size_t, vme_addr_t, int, bus_dmamap_t *);
void	mvmebus_dmamap_destroy(void *, bus_dmamap_t);
int	mvmebus_dmamap_load(bus_dma_tag_t, bus_dmamap_t,
	    void *, bus_size_t, struct proc *, int);
int	mvmebus_dmamap_load_mbuf(bus_dma_tag_t,
	    bus_dmamap_t, struct mbuf *, int);
int	mvmebus_dmamap_load_uio(bus_dma_tag_t,
	    bus_dmamap_t, struct uio *, int);
int	mvmebus_dmamap_load_raw(bus_dma_tag_t,
	    bus_dmamap_t, bus_dma_segment_t *, int, bus_size_t, int);
void	mvmebus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	mvmebus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

int	mvmebus_dmamem_alloc(void *, vme_size_t, vme_am_t, vme_datasize_t,
	    vme_swap_t, bus_dma_segment_t *, int, int *, int);
void	mvmebus_dmamem_free(void *, bus_dma_segment_t *, int);
int	mvmebus_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int,
	    size_t, void **, int);
void	mvmebus_dmamem_unmap(bus_dma_tag_t, void *, size_t);
paddr_t	mvmebus_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *, int,
	    off_t, int, int);

extern vme_am_t _mvmebus_am_cap[];
extern const char *mvmebus_irq_name[];

#endif /* _MVME_MVMEBUS_H */
