/*	$NetBSD: pte.h,v 1.27 2011/02/01 20:09:08 chuck Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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

/*
 * pte.h rewritten by chuck based on the jolitz version, plus random
 * info on the pentium and other processors found on the net.   the
 * goal of this rewrite is to provide enough documentation on the MMU
 * hardware that the reader will be able to understand it without having
 * to refer to a hardware manual.
 */

#ifndef _I386_PTE_H_
#define _I386_PTE_H_
#ifdef _KERNEL_OPT
#include "opt_xen.h"
#endif

/*
 * i386 MMU hardware structure (without PAE extension):
 *
 * the i386 MMU is a two-level MMU which maps 4GB of virtual memory.
 * the pagesize is 4K (4096 [0x1000] bytes), although newer pentium
 * processors can support a 4MB pagesize as well.
 *
 * the first level table (segment table?) is called a "page directory"
 * and it contains 1024 page directory entries (PDEs).   each PDE is
 * 4 bytes (an int), so a PD fits in a single 4K page.   this page is
 * the page directory page (PDP).  each PDE in a PDP maps 4MB of space 
 * (1024 * 4MB = 4GB).   a PDE contains the physical address of the 
 * second level table: the page table.   or, if 4MB pages are being used,
 * then the PDE contains the PA of the 4MB page being mapped.
 *
 * a page table consists of 1024 page table entries (PTEs).  each PTE is
 * 4 bytes (an int), so a page table also fits in a single 4K page.  a
 * 4K page being used as a page table is called a page table page (PTP).
 * each PTE in a PTP maps one 4K page (1024 * 4K = 4MB).   a PTE contains
 * the physical address of the page it maps and some flag bits (described
 * below).
 * 
 * the processor has a special register, "cr3", which points to the
 * the PDP which is currently controlling the mappings of the virtual
 * address space.
 *
 * the following picture shows the translation process for a 4K page:
 *
 * %cr3 register [PA of PDP]
 *      |
 *      |
 *      |   bits <31-22> of VA         bits <21-12> of VA   bits <11-0>
 *      |   index the PDP (0 - 1023)   index the PTP        are the page offset
 *      |         |                           |                  |
 *      |         v                           |                  |
 *      +--->+----------+                     |                  |
 *           | PD Page  |   PA of             v                  |
 *           |          |---PTP-------->+------------+           |
 *           | 1024 PDE |               | page table |--PTE--+   |
 *           | entries  |               | (aka PTP)  |       |   |
 *           +----------+               | 1024 PTE   |       |   |
 *                                      | entries    |       |   |
 *                                      +------------+       |   |
 *                                                           |   |
 *                                                bits <31-12>   bits <11-0>
 *                                                p h y s i c a l  a d d r
 *
 * the i386 caches PTEs in a TLB.   it is important to flush out old
 * TLB mappings when making a change to a mappings.   writing to the 
 * %cr3 will flush the entire TLB.    newer processors also have an
 * instruction that will invalidate the mapping of a single page (which
 * is useful if you are changing a single mappings because it preserves
 * all the cached TLB entries).
 *
 * as shows, bits 31-12 of the PTE contain PA of the page being mapped.
 * the rest of the PTE is defined as follows:
 *   bit#	name	use
 *   11		n/a	available for OS use, hardware ignores it
 *   10		n/a	available for OS use, hardware ignores it
 *   9		n/a	available for OS use, hardware ignores it
 *   8		G	global bit (see discussion below)
 *   7		PS	page size [for PDEs] (0=4k, 1=4M <if supported>) 
 *   6		D	dirty (modified) page
 *   5		A	accessed (referenced) page
 *   4		PCD	cache disable
 *   3		PWT	prevent write through (cache)
 *   2		U/S	user/supervisor bit (0=supervisor only, 1=both u&s)
 *   1		R/W	read/write bit (0=read only, 1=read-write)
 *   0		P	present (valid)
 *
 * notes: 
 *  - PS is only supported on newer processors
 *  - PTEs with the G bit are global in the sense that they are not 
 *    flushed from the TLB when %cr3 is written (to flush, use the 
 *    "flush single page" instruction).   this is only supported on
 *    newer processors.    this bit can be used to keep the kernel's
 *    TLB entries around while context switching.   since the kernel
 *    is mapped into all processes at the same place it does not make 
 *    sense to flush these entries when switching from one process'
 *    pmap to another.
 *
 * The PAE extension extends the size of the PTE to 64 bits (52bits physical
 * address) and is compatible with the amd64 PTE format. The first level
 * maps 2M, the second 1G, so a third level page table is introduced to
 * map the 4GB virtual address space. This PD has only 4 entries.
 * We can't use recursive mapping at level 3 to map the PD pages, as this
 * would eat one GB of address space. In addition, Xen imposes restrictions
 * on the entries we put in the L3 page (for example, the page pointed to by
 * the last slot can't be shared among different L3 pages), which makes 
 * handling this L3 page in the same way we do for L2 on i386 (or L4 on amd64)
 * difficult. For most things we'll just pretend to have only 2 levels,
 * with the 2 high bits of the L2 index being in fact the index in the
 * L3.
 */

#if !defined(_LOCORE)

/*
 * here we define the data types for PDEs and PTEs
 */
#ifdef PAE
typedef uint64_t pd_entry_t;		/* PDE */
typedef uint64_t pt_entry_t;		/* PTE */
#else
typedef uint32_t pd_entry_t;		/* PDE */
typedef uint32_t pt_entry_t;		/* PTE */
#endif

#endif

/*
 * now we define various for playing with virtual addresses
 */

#ifdef PAE
#define	L1_SHIFT	12
#define	L2_SHIFT	21
#define	L3_SHIFT	30
#define	NBPD_L1		(1ULL << L1_SHIFT) /* # bytes mapped by L1 ent (4K) */
#define	NBPD_L2		(1ULL << L2_SHIFT) /* # bytes mapped by L2 ent (2MB) */
#define	NBPD_L3		(1ULL << L3_SHIFT) /* # bytes mapped by L3 ent (1GB) */

#define	L3_MASK		0xc0000000
#define	L2_REALMASK	0x3fe00000
#define	L2_MASK		(L2_REALMASK | L3_MASK)
#define	L1_MASK		0x001ff000

#define	L3_FRAME	(L3_MASK)
#define	L2_FRAME	(L3_FRAME | L2_MASK)
#define	L1_FRAME	(L2_FRAME|L1_MASK)

#define	PG_FRAME	0x000ffffffffff000ULL /* page frame mask */
#define	PG_LGFRAME	0x000fffffffe00000ULL /* large (2MB) page frame mask */

/* macros to get real L2 and L3 index, from our "extended" L2 index */
#define l2tol3(idx)	((idx) >> (L3_SHIFT - L2_SHIFT))
#define l2tol2(idx)	((idx) & (L2_REALMASK >>  L2_SHIFT))

#else /* PAE */

#define	L1_SHIFT	12
#define	L2_SHIFT	22
#define	NBPD_L1		(1UL << L1_SHIFT) /* # bytes mapped by L1 ent (4K) */
#define	NBPD_L2		(1UL << L2_SHIFT) /* # bytes mapped by L2 ent (4MB) */

#define L2_MASK		0xffc00000
#define L1_MASK		0x003ff000

#define L2_FRAME	(L2_MASK)
#define L1_FRAME	(L2_FRAME|L1_MASK)

#define	PG_FRAME	0xfffff000	/* page frame mask */
#define	PG_LGFRAME	0xffc00000	/* large (4MB) page frame mask */

#endif /* PAE */
/*
 * here we define the bits of the PDE/PTE, as described above:
 *
 * XXXCDC: need to rename these (PG_u == ugly).
 */

#define	PG_V		0x00000001	/* valid entry */
#define	PG_RO		0x00000000	/* read-only page */
#define	PG_RW		0x00000002	/* read-write page */
#define	PG_u		0x00000004	/* user accessible page */
#define	PG_PROT		0x00000806	/* all protection bits */
#define PG_WT		0x00000008	/* write through */
#define	PG_N		0x00000010	/* non-cacheable */
#define	PG_U		0x00000020	/* has been used */
#define	PG_M		0x00000040	/* has been modified */
#define PG_PAT		0x00000080	/* PAT (on pte) */
#define PG_PS		0x00000080	/* 4MB page size (2MB for PAE) */
#define PG_G		0x00000100	/* global, don't TLB flush */
#define PG_AVAIL1	0x00000200	/* ignored by hardware */
#define PG_AVAIL2	0x00000400	/* ignored by hardware */
#define PG_AVAIL3	0x00000800	/* ignored by hardware */
#define PG_LGPAT	0x00001000	/* PAT on large pages */

/*
 * various short-hand protection codes
 */

#define	PG_KR		0x00000000	/* kernel read-only */
#define	PG_KW		0x00000002	/* kernel read-write */

#ifdef PAE
#define	PG_NX		0x8000000000000000ULL /* No-execute */
#else
#define	PG_NX		0		/* dummy */
#endif

#include <x86/pte.h>

#endif /* _I386_PTE_H_ */
