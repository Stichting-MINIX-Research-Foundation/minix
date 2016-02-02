/*	$NetBSD: pmap.h,v 1.117 2014/04/21 19:12:11 christos Exp $	*/

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

#ifndef	_I386_PMAP_H_
#define	_I386_PMAP_H_

#if defined(_KERNEL_OPT)
#include "opt_user_ldt.h"
#include "opt_xen.h"
#endif

#include <sys/atomic.h>

#include <i386/pte.h>
#include <machine/segments.h>
#if defined(_KERNEL)
#include <machine/cpufunc.h>
#endif

#include <uvm/uvm_object.h>
#ifdef XEN
#include <xen/xenfunc.h>
#include <xen/xenpmap.h>
#endif /* XEN */

/*
 * see pte.h for a description of i386 MMU terminology and hardware
 * interface.
 *
 * a pmap describes a processes' 4GB virtual address space.  when PAE
 * is not in use, this virtual address space can be broken up into 1024 4MB
 * regions which are described by PDEs in the PDP.  the PDEs are defined as
 * follows:
 *
 * (ranges are inclusive -> exclusive, just like vm_map_entry start/end)
 * (the following assumes that KERNBASE is 0xc0000000)
 *
 * PDE#s	VA range		usage
 * 0->766	0x0 -> 0xbfc00000	user address space
 * 767		0xbfc00000->		recursive mapping of PDP (used for
 *			0xc0000000	linear mapping of PTPs)
 * 768->1023	0xc0000000->		kernel address space (constant
 *			0xffc00000	across all pmap's/processes)
 *			<end>
 *
 *
 * note: a recursive PDP mapping provides a way to map all the PTEs for
 * a 4GB address space into a linear chunk of virtual memory.  in other
 * words, the PTE for page 0 is the first int mapped into the 4MB recursive
 * area.  the PTE for page 1 is the second int.  the very last int in the
 * 4MB range is the PTE that maps VA 0xfffff000 (the last page in a 4GB
 * address).
 *
 * all pmap's PD's must have the same values in slots 768->1023 so that
 * the kernel is always mapped in every process.  these values are loaded
 * into the PD at pmap creation time.
 *
 * at any one time only one pmap can be active on a processor.  this is
 * the pmap whose PDP is pointed to by processor register %cr3.  this pmap
 * will have all its PTEs mapped into memory at the recursive mapping
 * point (slot #767 as show above).  when the pmap code wants to find the
 * PTE for a virtual address, all it has to do is the following:
 *
 * address of PTE = (767 * 4MB) + (VA / PAGE_SIZE) * sizeof(pt_entry_t)
 *                = 0xbfc00000 + (VA / 4096) * 4
 *
 * what happens if the pmap layer is asked to perform an operation
 * on a pmap that is not the one which is currently active?  in that
 * case we temporarily load this pmap, perform the operation, and mark
 * the currently active one as pending lazy reload.
 *
 * the following figure shows the effects of the recursive PDP mapping:
 *
 *   PDP (%cr3)
 *   +----+
 *   |   0| -> PTP#0 that maps VA 0x0 -> 0x400000
 *   |    |
 *   |    |
 *   | 767| -> points back to PDP (%cr3) mapping VA 0xbfc00000 -> 0xc0000000
 *   | 768| -> first kernel PTP (maps 0xc0000000 -> 0xc0400000)
 *   |    |
 *   +----+
 *
 * note that the PDE#767 VA (0xbfc00000) is defined as "PTE_BASE"
 *
 * starting at VA 0xbfc00000 the current active PDP (%cr3) acts as a
 * PTP:
 *
 * PTP#767 == PDP(%cr3) => maps VA 0xbfc00000 -> 0xc0000000
 *   +----+
 *   |   0| -> maps the contents of PTP#0 at VA 0xbfc00000->0xbfc01000
 *   |    |
 *   |    |
 *   | 767| -> maps contents of PTP#767 (the PDP) at VA 0xbfeff000
 *   | 768| -> maps contents of first kernel PTP
 *   |    |
 *   |1023|
 *   +----+
 *
 * note that mapping of the PDP at PTP#767's VA (0xbfeff000) is
 * defined as "PDP_BASE".... within that mapping there are two
 * defines:
 *   "PDP_PDE" (0xbfeffbfc) is the VA of the PDE in the PDP
 *      which points back to itself.
 *
 * - PAE support -
 * ---------------
 *
 * PAE adds another layer of indirection during address translation, breaking
 * up the translation process in 3 different levels:
 * - L3 page directory, containing 4 * 64-bits addresses (index determined by
 * bits [31:30] from the virtual address). This breaks up the address space
 * in 4 1GB regions.
 * - the PD (L2), containing 512 64-bits addresses, breaking each L3 region
 * in 512 * 2MB regions.
 * - the PT (L1), also containing 512 64-bits addresses (at L1, the size of
 * the pages is still 4K).
 *
 * The kernel virtual space is mapped by the last entry in the L3 page,
 * the first 3 entries mapping the user VA space.
 *
 * Because the L3 has only 4 entries of 1GB each, we can't use recursive
 * mappings at this level for PDP_PDE (this would eat up 2 of the 4GB
 * virtual space). There are also restrictions imposed by Xen on the
 * last entry of the L3 PD (reference count to this page cannot be
 * bigger than 1), which makes it hard to use one L3 page per pmap to
 * switch between pmaps using %cr3.
 *
 * As such, each CPU gets its own L3 page that is always loaded into its %cr3
 * (ci_pae_l3_pd in the associated cpu_info struct). We claim that the VM has
 * only a 2-level PTP (similar to the non-PAE case). L2 PD is now 4 contiguous
 * pages long (corresponding to the 4 entries of the L3), and the different
 * index/slots (like PDP_PDE) are adapted accordingly.
 * 
 * Kernel space remains in L3[3], L3[0-2] maps the user VA space. Switching
 * between pmaps consists in modifying the first 3 entries of the CPU's L3 page.
 *
 * PTE_BASE will need 4 entries in the L2 PD pages to map the L2 pages
 * recursively.
 *
 * In addition, for Xen, we can't recursively map L3[3] (Xen wants the ref
 * count on this page to be exactly one), so we use a shadow PD page for
 * the last L2 PD. The shadow page could be static too, but to make pm_pdir[]
 * contiguous we'll allocate/copy one page per pmap.
 */

/*
 * Mask to get rid of the sign-extended part of addresses.
 */
#define VA_SIGN_MASK		0
#define VA_SIGN_NEG(va)		((va) | VA_SIGN_MASK)
/*
 * XXXfvdl this one's not right.
 */
#define VA_SIGN_POS(va)		((va) & ~VA_SIGN_MASK)

/*
 * the following defines identify the slots used as described above.
 */
#ifdef PAE
#define L2_SLOT_PTE	(KERNBASE/NBPD_L2-4) /* 1532: for recursive PDP map */
#define L2_SLOT_KERN	(KERNBASE/NBPD_L2)   /* 1536: start of kernel space */
#else /* PAE */
#define L2_SLOT_PTE	(KERNBASE/NBPD_L2-1) /* 767: for recursive PDP map */
#define L2_SLOT_KERN	(KERNBASE/NBPD_L2)   /* 768: start of kernel space */
#endif /* PAE */

#define	L2_SLOT_KERNBASE L2_SLOT_KERN

#define PDIR_SLOT_KERN	L2_SLOT_KERN
#define PDIR_SLOT_PTE	L2_SLOT_PTE

/*
 * the following defines give the virtual addresses of various MMU
 * data structures:
 * PTE_BASE: the base VA of the linear PTE mappings
 * PDP_BASE: the base VA of the recursive mapping of the PDP
 * PDP_PDE: the VA of the PDE that points back to the PDP
 */

#define PTE_BASE  ((pt_entry_t *) (PDIR_SLOT_PTE * NBPD_L2))

#define L1_BASE		PTE_BASE

#define L2_BASE ((pd_entry_t *)((char *)L1_BASE + L2_SLOT_PTE * NBPD_L1))

#define PDP_PDE		(L2_BASE + PDIR_SLOT_PTE)

#define PDP_BASE	L2_BASE

/* largest value (-1 for APTP space) */
#define NKL2_MAX_ENTRIES	(NTOPLEVEL_PDES - (KERNBASE/NBPD_L2) - 1)
#define NKL1_MAX_ENTRIES	(unsigned long)(NKL2_MAX_ENTRIES * NPDPG)

#define NKL2_KIMG_ENTRIES	0	/* XXX unused */

#define NKL2_START_ENTRIES	0	/* XXX computed on runtime */
#define NKL1_START_ENTRIES	0	/* XXX unused */

#ifndef XEN
#define NTOPLEVEL_PDES		(PAGE_SIZE * PDP_SIZE / (sizeof (pd_entry_t)))
#else	/* !XEN */
#ifdef  PAE
#define NTOPLEVEL_PDES		1964	/* 1964-2047 reserved by Xen */
#else	/* PAE */
#define NTOPLEVEL_PDES		1008	/* 1008-1023 reserved by Xen */
#endif	/* PAE */
#endif  /* !XEN */
#define NPDPG			(PAGE_SIZE / sizeof (pd_entry_t))

#define PTP_MASK_INITIALIZER	{ L1_FRAME, L2_FRAME }
#define PTP_SHIFT_INITIALIZER	{ L1_SHIFT, L2_SHIFT }
#define NKPTP_INITIALIZER	{ NKL1_START_ENTRIES, NKL2_START_ENTRIES }
#define NKPTPMAX_INITIALIZER	{ NKL1_MAX_ENTRIES, NKL2_MAX_ENTRIES }
#define NBPD_INITIALIZER	{ NBPD_L1, NBPD_L2 }
#define PDES_INITIALIZER	{ L2_BASE }

#define PTP_LEVELS	2

/*
 * PG_AVAIL usage: we make use of the ignored bits of the PTE
 */

#define PG_W		PG_AVAIL1	/* "wired" mapping */
#define PG_PVLIST	PG_AVAIL2	/* mapping has entry on pvlist */
#define PG_X		PG_AVAIL3	/* executable mapping */

/*
 * Number of PTE's per cache line.  4 byte pte, 32-byte cache line
 * Used to avoid false sharing of cache lines.
 */
#ifdef PAE
#define NPTECL		4
#else
#define NPTECL		8
#endif

#include <x86/pmap.h>

#ifndef XEN
#define pmap_pa2pte(a)			(a)
#define pmap_pte2pa(a)			((a) & PG_FRAME)
#define pmap_pte_set(p, n)		do { *(p) = (n); } while (0)
#define pmap_pte_flush()		/* nothing */

#ifdef PAE
#define pmap_pte_cas(p, o, n)		atomic_cas_64((p), (o), (n))
#define pmap_pte_testset(p, n)		\
    atomic_swap_64((volatile uint64_t *)p, n)
#define pmap_pte_setbits(p, b)		\
    atomic_or_64((volatile uint64_t *)p, b)
#define pmap_pte_clearbits(p, b)	\
    atomic_and_64((volatile uint64_t *)p, ~(b))
#else /* PAE */
#define pmap_pte_cas(p, o, n)		atomic_cas_32((p), (o), (n))
#define pmap_pte_testset(p, n)		\
    atomic_swap_ulong((volatile unsigned long *)p, n)
#define pmap_pte_setbits(p, b)		\
    atomic_or_ulong((volatile unsigned long *)p, b)
#define pmap_pte_clearbits(p, b)	\
    atomic_and_ulong((volatile unsigned long *)p, ~(b))
#endif /* PAE */

#else /* XEN */
extern kmutex_t pte_lock;

static __inline pt_entry_t
pmap_pa2pte(paddr_t pa)
{
	return (pt_entry_t)xpmap_ptom_masked(pa);
}

static __inline paddr_t
pmap_pte2pa(pt_entry_t pte)
{
	return xpmap_mtop_masked(pte & PG_FRAME);
}
static __inline void
pmap_pte_set(pt_entry_t *pte, pt_entry_t npte)
{
	int s = splvm();
	xpq_queue_pte_update(xpmap_ptetomach(pte), npte);
	splx(s);
}

static __inline pt_entry_t
pmap_pte_cas(volatile pt_entry_t *ptep, pt_entry_t o, pt_entry_t n)
{
	pt_entry_t opte;

	mutex_enter(&pte_lock);
	opte = *ptep;
	if (opte == o) {
		xpq_queue_pte_update(xpmap_ptetomach(__UNVOLATILE(ptep)), n);
		xpq_flush_queue();
	}
	mutex_exit(&pte_lock);
	return opte;
}

static __inline pt_entry_t
pmap_pte_testset(volatile pt_entry_t *pte, pt_entry_t npte)
{
	pt_entry_t opte;

	mutex_enter(&pte_lock);
	opte = *pte;
	xpq_queue_pte_update(xpmap_ptetomach(__UNVOLATILE(pte)),
	    npte);
	xpq_flush_queue();
	mutex_exit(&pte_lock);
	return opte;
}

static __inline void
pmap_pte_setbits(volatile pt_entry_t *pte, pt_entry_t bits)
{
	mutex_enter(&pte_lock);
	xpq_queue_pte_update(xpmap_ptetomach(__UNVOLATILE(pte)), (*pte) | bits);
	xpq_flush_queue();
	mutex_exit(&pte_lock);
}

static __inline void
pmap_pte_clearbits(volatile pt_entry_t *pte, pt_entry_t bits)
{	
	mutex_enter(&pte_lock);
	xpq_queue_pte_update(xpmap_ptetomach(__UNVOLATILE(pte)),
	    (*pte) & ~bits);
	xpq_flush_queue();
	mutex_exit(&pte_lock);
}

static __inline void
pmap_pte_flush(void)
{
	int s = splvm();
	xpq_flush_queue();
	splx(s);
}

#endif

struct vm_map;
struct trapframe;
struct pcb;

int	pmap_exec_fixup(struct vm_map *, struct trapframe *, struct pcb *);
void	pmap_ldt_cleanup(struct lwp *);

#include <x86/pmap_pv.h>

#define	__HAVE_VM_PAGE_MD
#define	VM_MDPAGE_INIT(pg) \
	memset(&(pg)->mdpage, 0, sizeof((pg)->mdpage)); \
	PMAP_PAGE_INIT(&(pg)->mdpage.mp_pp)

struct vm_page_md {
	struct pmap_page mp_pp;
};

#endif	/* _I386_PMAP_H_ */
