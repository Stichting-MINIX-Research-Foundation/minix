/*	$NetBSD: bus_private.h,v 1.14 2011/09/01 15:10:31 christos Exp $	*/
/*	NetBSD: bus.h,v 1.8 2005/03/09 19:04:46 matt Exp	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2001 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#if !defined(_X86_BUS_PRIVATE_H_)
#define	_X86_BUS_PRIVATE_H_

/*
 * Cookie used for bounce buffers. A pointer to one of these it stashed in
 * the DMA map.
 */
struct x86_bus_dma_cookie {
	int	id_flags;		/* flags; see below */

	/*
	 * Information about the original buffer used during
	 * DMA map syncs.  Note that origibuflen is only used
	 * for ID_BUFTYPE_LINEAR.
	 */
	void	*id_origbuf;		/* pointer to orig buffer if
					   bouncing */
	bus_size_t id_origbuflen;	/* ...and size */
	int	id_buftype;		/* type of buffer */

	void	*id_bouncebuf;		/* pointer to the bounce buffer */
	bus_size_t id_bouncebuflen;	/* ...and size */
	int	id_nbouncesegs;		/* number of valid bounce segs */
	bus_dma_segment_t id_bouncesegs[0]; /* array of bounce buffer
					       physical memory segments */
};

/* id_flags */
#define	X86_DMA_MIGHT_NEED_BOUNCE	0x01	/* may need bounce buffers */
#define	X86_DMA_HAS_BOUNCE		0x02	/* has bounce buffers */
#define	X86_DMA_IS_BOUNCING		0x04	/* is bouncing current xfer */

/* id_buftype */
#define	X86_DMA_BUFTYPE_INVALID		0
#define	X86_DMA_BUFTYPE_LINEAR		1
#define	X86_DMA_BUFTYPE_MBUF		2
#define	X86_DMA_BUFTYPE_UIO		3
#define	X86_DMA_BUFTYPE_RAW		4

/*
 * default address translation macros, which are appropriate where
 * paddr_t == bus_addr_t.
 */

#if !defined(_BUS_PHYS_TO_BUS)
#define _BUS_PHYS_TO_BUS(pa)	((bus_addr_t)(pa))
#endif /* !defined(_BUS_PHYS_TO_BUS) */

#if !defined(_BUS_BUS_TO_PHYS)
#define _BUS_BUS_TO_PHYS(ba)	((paddr_t)(ba))
#endif /* !defined(_BUS_BUS_TO_PHYS) */

#if !defined(_BUS_VM_PAGE_TO_BUS)
#define	_BUS_VM_PAGE_TO_BUS(pg)	_BUS_PHYS_TO_BUS(VM_PAGE_TO_PHYS(pg))
#endif /* !defined(_BUS_VM_PAGE_TO_BUS) */

#if !defined(_BUS_BUS_TO_VM_PAGE)
#define	_BUS_BUS_TO_VM_PAGE(ba)	PHYS_TO_VM_PAGE(ba)
#endif /* !defined(_BUS_BUS_TO_VM_PAGE) */

#if !defined(_BUS_PMAP_ENTER)
#define _BUS_PMAP_ENTER(pmap, va, ba, prot, flags) \
    pmap_enter(pmap, va, ba, prot, flags)
#endif /* _BUS_PMAP_ENTER */

#if !defined(_BUS_VIRT_TO_BUS)
#include <uvm/uvm.h>

static __inline bus_addr_t _bus_virt_to_bus(struct pmap *, vaddr_t);
#define	_BUS_VIRT_TO_BUS(pm, va) _bus_virt_to_bus((pm), (va))

static __inline bus_addr_t
_bus_virt_to_bus(struct pmap *pm, vaddr_t va)
{
	paddr_t pa;

	if (!pmap_extract(pm, va, &pa)) {
		panic("_bus_virt_to_bus");
	}

	return _BUS_PHYS_TO_BUS(pa);
}
#endif /* !defined(_BUS_VIRT_TO_BUS) */

/*
 * by default, the end address of RAM visible on bus is the same as the
 * largest physical address.
 */
#ifndef _BUS_AVAIL_END
#define _BUS_AVAIL_END (avail_end)
#endif

struct x86_bus_dma_tag {
	bus_dma_tag_t				bdt_super;
	/* bdt_present: bitmap indicating overrides present (1) in *this* tag,
	 * bdt_exists: bitmap indicating overrides present (1) in *this* tag
	 * or in an ancestor's tag (follow bdt_super to ancestors)
	 */
	uint64_t				bdt_present;
	uint64_t				bdt_exists;
	const struct bus_dma_overrides		*bdt_ov;
	void					*bdt_ctx;
	/*
	 * The `bounce threshold' is checked while we are loading
	 * the DMA map.  If the physical address of the segment
	 * exceeds the threshold, an error will be returned.  The
	 * caller can then take whatever action is necessary to
	 * bounce the transfer.  If this value is 0, it will be
	 * ignored.
	 */
	int        _tag_needs_free;
	bus_addr_t _bounce_thresh;
	bus_addr_t _bounce_alloc_lo;
	bus_addr_t _bounce_alloc_hi;
	int	(*_may_bounce)(bus_dma_tag_t, bus_dmamap_t, int, int *);
};

#endif /* !defined(_X86_BUS_PRIVATE_H_) */
