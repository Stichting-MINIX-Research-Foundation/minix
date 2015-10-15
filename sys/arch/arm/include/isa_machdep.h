/*	$NetBSD: isa_machdep.h,v 1.11 2014/01/29 00:42:15 matt Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#ifndef _ARM_ISA_MACHDEP_H_
#define _ARM_ISA_MACHDEP_H_

#include <sys/bus.h>
#include <dev/isa/isadmavar.h>

/*
 * Types provided to machine-independent ISA code.
 */
struct arm32_isa_chipset {
	struct isa_dma_state ic_dmastate;
};

typedef struct arm32_isa_chipset *isa_chipset_tag_t;

struct isabus_attach_args;	/* XXX */

/*
 * Functions provided to machine-independent ISA code.
 */
void	isa_attach_hook(device_t, device_t,
	    struct isabus_attach_args *);
void	isa_detach_hook(isa_chipset_tag_t, device_t);
const struct evcnt *isa_intr_evcnt(isa_chipset_tag_t ic, int irq);
void	*isa_intr_establish(isa_chipset_tag_t ic, int irq, int type,
	    int level, int (*ih_fun)(void *), void *ih_arg);
void	isa_intr_disestablish(isa_chipset_tag_t ic, void *handler);

#define	isa_dmainit(ic, bst, dmat, d)					\
	_isa_dmainit(&(ic)->ic_dmastate, (bst), (dmat), (d))
#define	isa_dmadestroy(ic)						\
	_isa_dmadestroy(&(ic)->ic_dmastate)
#define	isa_dmacascade(ic, c)						\
	_isa_dmacascade(&(ic)->ic_dmastate, (c))
#define	isa_dmamaxsize(ic, c)						\
	_isa_dmamaxsize(&(ic)->ic_dmastate, (c))
#define	isa_dmamap_create(ic, c, s, f)					\
	_isa_dmamap_create(&(ic)->ic_dmastate, (c), (s), (f))
#define	isa_dmamap_destroy(ic, c)					\
	_isa_dmamap_destroy(&(ic)->ic_dmastate, (c))
#define	isa_dmastart(ic, c, a, n, p, f, bf)				\
	_isa_dmastart(&(ic)->ic_dmastate, (c), (a), (n), (p), (f), (bf))
#define	isa_dmaabort(ic, c)						\
	_isa_dmaabort(&(ic)->ic_dmastate, (c))
#define	isa_dmacount(ic, c)						\
	_isa_dmacount(&(ic)->ic_dmastate, (c))
#define	isa_dmafinished(ic, c)						\
	_isa_dmafinished(&(ic)->ic_dmastate, (c))
#define	isa_dmadone(ic, c)						\
	_isa_dmadone(&(ic)->ic_dmastate, (c))
#define	isa_dmafreeze(ic)						\
	_isa_dmafreeze(&(ic)->ic_dmastate)
#define	isa_dmathaw(ic)							\
	_isa_dmathaw(&(ic)->ic_dmastate)
#define	isa_dmamem_alloc(ic, c, s, ap, f)				\
	_isa_dmamem_alloc(&(ic)->ic_dmastate, (c), (s), (ap), (f))
#define	isa_dmamem_free(ic, c, a, s)					\
	_isa_dmamem_free(&(ic)->ic_dmastate, (c), (a), (s))
#define	isa_dmamem_map(ic, c, a, s, kp, f)				\
	_isa_dmamem_map(&(ic)->ic_dmastate, (c), (a), (s), (kp), (f))
#define	isa_dmamem_unmap(ic, c, k, s)					\
	_isa_dmamem_unmap(&(ic)->ic_dmastate, (c), (k), (s))
#define	isa_dmamem_mmap(ic, c, a, s, o, p, f)				\
	_isa_dmamem_mmap(&(ic)->ic_dmastate, (c), (a), (s), (o), (p), (f))
#define isa_drq_alloc(ic, c)						\
	_isa_drq_alloc(&(ic)->ic_dmastate, c)
#define isa_drq_free(ic, c)						\
	_isa_drq_free(&(ic)->ic_dmastate, c)
#define	isa_drq_isfree(ic, c)						\
	_isa_drq_isfree(&(ic)->ic_dmastate, (c))
#define	isa_malloc(ic, c, s, p, f)					\
	_isa_malloc(&(ic)->ic_dmastate, (c), (s), (p), (f))
#define	isa_free(a, p)							\
	_isa_free((a), (p))
#define	isa_mappage(m, o, p)						\
	_isa_mappage((m), (o), (p))

/*
 * ALL OF THE FOLLOWING ARE MACHINE-DEPENDENT, AND SHOULD NOT BE USED
 * BY PORTABLE CODE.
 */

extern struct arm32_bus_dma_tag isa_bus_dma_tag;

/* bus space tags */
extern struct bus_space isa_io_bs_tag;
extern struct bus_space isa_mem_bs_tag;

/* ISA chipset */
extern struct arm32_isa_chipset isa_chipset_tag;

/* for pccons.c */
#define MONO_BASE           0x3B4
#define MONO_BUF            0x000B0000
#define CGA_BASE            0x3D4
#define CGA_BUF             0x000B8000
#define VGA_BUF             0xA0000
#define VGA_BUF_LEN         (0xBFFFF - 0xA0000)

void	isa_init(vaddr_t, vaddr_t);
void	isa_io_init(vaddr_t, vaddr_t);
void	isa_dma_init(void);
vaddr_t	isa_io_data_vaddr(void);
vaddr_t	isa_mem_data_vaddr(void);
int isa_intr_alloc(isa_chipset_tag_t ic, int mask, int type, int *irq);
void	isa_intr_init(void);

/*
 * Miscellanous functions.
 */
void sysbeep(int, int);		/* beep with the system speaker */
void isa_fillw(u_int val, void *addr, size_t len);

#endif	/* _ARM_ISA_MACHDEP_H_ */
