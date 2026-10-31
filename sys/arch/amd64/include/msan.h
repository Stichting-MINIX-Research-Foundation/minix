/*	$NetBSD: msan.h,v 1.8 2022/09/13 09:39:49 riastradh Exp $	*/

/*
 * Copyright (c) 2019-2020 Maxime Villard, m00nbsd.net
 * All rights reserved.
 *
 * This code is part of the KMSAN subsystem of the NetBSD kernel.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_AMD64_MSAN_H_
#define	_AMD64_MSAN_H_

#include <sys/ksyms.h>

#include <uvm/uvm.h>

#include <machine/pmap.h>
#include <machine/pmap_private.h>
#include <machine/pte.h>
#include <machine/vmparam.h>

#include <x86/bootspace.h>

#ifdef __HAVE_PCPU_AREA
#error "PCPU area not allowed with KMSAN"
#endif
#ifdef __HAVE_DIRECT_MAP
#error "DMAP not allowed with KMSAN"
#endif

/*
 * One big shadow, divided in two sub-shadows (SHAD and ORIG), themselves
 * divided in two regions (MAIN and KERN).
 */

#define __MD_SHADOW_SIZE	0x20000000000ULL	/* 4 * NBPD_L4 */
#define __MD_SHADOW_START	(VA_SIGN_NEG((L4_SLOT_KMSAN * NBPD_L4)))
#define __MD_SHADOW_END		(__MD_SHADOW_START + __MD_SHADOW_SIZE)

#define __MD_SHAD_MAIN_START	(__MD_SHADOW_START)
#define __MD_SHAD_KERN_START	(__MD_SHADOW_START + 0x8000000000ULL)

#define __MD_ORIG_MAIN_START	(__MD_SHAD_KERN_START + 0x8000000000ULL)
#define __MD_ORIG_KERN_START	(__MD_ORIG_MAIN_START + 0x8000000000ULL)

#define __MD_PTR_BASE		0xFFFFFFFF80000000ULL
#define __MD_ORIG_TYPE		__BITS(31,28)

static inline int8_t *
kmsan_md_addr_to_shad(const void *addr)
{
	vaddr_t va = (vaddr_t)addr;

	if (va >= vm_min_kernel_address && va < vm_max_kernel_address) {
		return (int8_t *)(__MD_SHAD_MAIN_START + (va - vm_min_kernel_address));
	} else if (va >= KERNBASE) {
		return (int8_t *)(__MD_SHAD_KERN_START + (va - KERNBASE));
	} else {
		panic("%s: impossible, va=%p", __func__, (void *)va);
	}
}

static inline int8_t *
kmsan_md_addr_to_orig(const void *addr)
{
	vaddr_t va = (vaddr_t)addr;

	if (va >= vm_min_kernel_address && va < vm_max_kernel_address) {
		return (int8_t *)(__MD_ORIG_MAIN_START + (va - vm_min_kernel_address));
	} else if (va >= KERNBASE) {
		return (int8_t *)(__MD_ORIG_KERN_START + (va - KERNBASE));
	} else {
		panic("%s: impossible, va=%p", __func__, (void *)va);
	}
}

static inline bool
kmsan_md_unsupported(vaddr_t addr)
{
	return (addr >= (vaddr_t)PTE_BASE &&
	    addr < ((vaddr_t)PTE_BASE + NBPD_L4));
}

static inline paddr_t
__md_palloc(void)
{
	/* The page is zeroed. */
	return pmap_get_physpage();
}

static inline paddr_t
__md_palloc_large(void)
{
	struct pglist pglist;
	int ret;

	if (!uvm.page_init_done)
		return 0;

	kmsan_init_arg(sizeof(psize_t) + 4 * sizeof(paddr_t) +
	    sizeof(struct pglist *) + 2 * sizeof(int));
	ret = uvm_pglistalloc(NBPD_L2, 0, ~0UL, NBPD_L2, 0,
	    &pglist, 1, 0);
	if (ret != 0)
		return 0;

	/* The page may not be zeroed. */
	return VM_PAGE_TO_PHYS(TAILQ_FIRST(&pglist));
}

static void
kmsan_md_shadow_map_page(vaddr_t va)
{
	const pt_entry_t pteflags = PTE_W | pmap_pg_nx | PTE_P;
	paddr_t pa;

	KASSERT(va >= __MD_SHADOW_START && va < __MD_SHADOW_END);

	if (!pmap_valid_entry(L4_BASE[pl4_i(va)])) {
		pa = __md_palloc();
		L4_BASE[pl4_i(va)] = pa | pteflags;
	}
	if (!pmap_valid_entry(L3_BASE[pl3_i(va)])) {
		pa = __md_palloc();
		L3_BASE[pl3_i(va)] = pa | pteflags;
	}
	if (!pmap_valid_entry(L2_BASE[pl2_i(va)])) {
		if ((pa = __md_palloc_large()) != 0) {
			L2_BASE[pl2_i(va)] = pa | pteflags | PTE_PS |
			    pmap_pg_g;
			__insn_barrier();
			__builtin_memset((void *)va, 0, NBPD_L2);
			return;
		}
		pa = __md_palloc();
		L2_BASE[pl2_i(va)] = pa | pteflags;
	} else if (L2_BASE[pl2_i(va)] & PTE_PS) {
		return;
	}
	if (!pmap_valid_entry(L1_BASE[pl1_i(va)])) {
		pa = __md_palloc();
		L1_BASE[pl1_i(va)] = pa | pteflags | pmap_pg_g;
	}
}

static void
kmsan_md_init(void)
{
	extern struct bootspace bootspace;
	size_t i;

	CTASSERT((__MD_SHADOW_SIZE / NBPD_L4) == NL4_SLOT_KMSAN);

	/* Kernel. */
	for (i = 0; i < BTSPACE_NSEGS; i++) {
		if (bootspace.segs[i].type == BTSEG_NONE) {
			continue;
		}
		kmsan_shadow_map((void *)bootspace.segs[i].va,
		    bootspace.segs[i].sz);
	}

	/* Boot region. */
	kmsan_shadow_map((void *)bootspace.boot.va, bootspace.boot.sz);

	/* Module map. */
	kmsan_shadow_map((void *)bootspace.smodule,
	    (size_t)(bootspace.emodule - bootspace.smodule));

	/* The bootstrap spare va. */
	kmsan_shadow_map((void *)bootspace.spareva, PAGE_SIZE);
}

static inline msan_orig_t
kmsan_md_orig_encode(int type, uintptr_t ptr)
{
	msan_orig_t ret;

	ret = (ptr & 0xFFFFFFFF) & ~__MD_ORIG_TYPE;
	ret |= __SHIFTIN(type, __MD_ORIG_TYPE);

	return ret;
}

static inline void
kmsan_md_orig_decode(msan_orig_t orig, int *type, uintptr_t *ptr)
{
	*type = __SHIFTOUT(orig, __MD_ORIG_TYPE);
	*ptr = (uintptr_t)(orig & ~__MD_ORIG_TYPE) | __MD_PTR_BASE;
}

static inline bool
kmsan_md_is_pc(uintptr_t ptr)
{
	extern uint8_t __rodata_start;

	return (ptr < (uintptr_t)&__rodata_start);
}

static inline bool
__md_unwind_end(const char *name)
{
	if (!strcmp(name, "syscall") ||
	    !strcmp(name, "alltraps") ||
	    !strcmp(name, "handle_syscall") ||
	    !strncmp(name, "Xtrap", 5) ||
	    !strncmp(name, "Xintr", 5) ||
	    !strncmp(name, "Xhandle", 7) ||
	    !strncmp(name, "Xresume", 7) ||
	    !strncmp(name, "Xstray", 6) ||
	    !strncmp(name, "Xhold", 5) ||
	    !strncmp(name, "Xrecurse", 8) ||
	    !strcmp(name, "Xdoreti") ||
	    !strncmp(name, "Xsoft", 5)) {
		return true;
	}

	return false;
}

static void
kmsan_md_unwind(void)
{
	uint64_t *rbp, rip;
	const char *mod;
	const char *sym;
	size_t nsym;
	int error;

	rbp = (uint64_t *)__builtin_frame_address(0);
	nsym = 0;

	while (1) {
		/* 8(%rbp) contains the saved %rip. */
		rip = *(rbp + 1);

		if (rip < KERNBASE) {
			break;
		}
		error = ksyms_getname(&mod, &sym, (vaddr_t)rip, KSYMS_PROC);
		if (error) {
			break;
		}
		kmsan_printf("#%zu %p in %s <%s>\n", nsym, (void *)rip, sym, mod);
		if (__md_unwind_end(sym)) {
			break;
		}

		rbp = (uint64_t *)*(rbp);
		if (rbp == 0) {
			break;
		}
		nsym++;

		if (nsym >= 15) {
			break;
		}
	}
}

#endif	/* _AMD64_MSAN_H_ */
