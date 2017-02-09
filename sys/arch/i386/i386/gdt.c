/*	$NetBSD: gdt.c,v 1.53 2012/02/24 08:06:07 cherry Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl, by Charles M. Hannum, and by Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gdt.c,v 1.53 2012/02/24 08:06:07 cherry Exp $");

#include "opt_multiprocessor.h"
#include "opt_xen.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/cpu.h>

#include <uvm/uvm.h>

#include <machine/gdt.h>

#ifndef XEN
int gdt_size[1];	/* total number of GDT entries */
int gdt_count[1];	/* number of GDT entries in use */
int gdt_next[1];	/* next available slot for sweeping */
int gdt_free[1];	/* next free slot; terminated with GNULL_SEL */
#else
int gdt_size[2];	/* total number of GDT entries */
int gdt_count[2];	/* number of GDT entries in use */
int gdt_next[2];	/* next available slot for sweeping */
int gdt_free[2];	/* next free slot; terminated with GNULL_SEL */
#endif

static int ldt_count;	/* number of LDTs */
static int ldt_max = 1000;/* max number of LDTs */

void gdt_init(void);
void gdt_grow(int);
int gdt_get_slot1(int);
void gdt_put_slot1(int, int);

void
update_descriptor(union descriptor *table, union descriptor *entry)
{
#ifndef XEN
	*table = *entry;
#else
	paddr_t pa;
	pt_entry_t *ptp;

	ptp = kvtopte((vaddr_t)table);
	pa = (*ptp & PG_FRAME) | ((vaddr_t)table & ~PG_FRAME);
	if (HYPERVISOR_update_descriptor(pa, entry->raw[0], entry->raw[1]))
		panic("HYPERVISOR_update_descriptor failed\n");
#endif
}

void
setgdt(int sel, const void *base, size_t limit,
    int type, int dpl, int def32, int gran)
{
	struct segment_descriptor *sd = &gdt[sel].sd;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

#ifdef XEN
	if (type == SDT_SYS386TSS) {
		/* printk("XXX TSS descriptor not supported in GDT\n"); */
		return;
	}
#endif
	setsegment(sd, base, limit, type, dpl, def32, gran);
	for (CPU_INFO_FOREACH(cii, ci)) {
		if (ci->ci_gdt != NULL)
			update_descriptor(&ci->ci_gdt[sel],
			    (union descriptor *)sd);
	}
}

/*
 * Initialize the GDT subsystem.  Called from autoconf().
 */
void
gdt_init(void)
{
	size_t max_len, min_len;
	union descriptor *old_gdt;
	struct vm_page *pg;
	vaddr_t va;
	struct cpu_info *ci = &cpu_info_primary;

	max_len = MAXGDTSIZ * sizeof(gdt[0]);
	min_len = MINGDTSIZ * sizeof(gdt[0]);

	gdt_size[0] = MINGDTSIZ;
	gdt_count[0] = NGDT;
	gdt_next[0] = NGDT;
	gdt_free[0] = GNULL_SEL;
#ifdef XEN
	max_len = max_len * 2;
	gdt_size[1] = 0;
	gdt_count[1] = MAXGDTSIZ;
	gdt_next[1] = MAXGDTSIZ;
	gdt_free[1] = GNULL_SEL;
#endif

	old_gdt = gdt;
	gdt = (union descriptor *)uvm_km_alloc(kernel_map, max_len,
	    0, UVM_KMF_VAONLY);
	for (va = (vaddr_t)gdt; va < (vaddr_t)gdt + min_len; va += PAGE_SIZE) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg == NULL) {
			panic("gdt_init: no pages");
		}
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ | VM_PROT_WRITE, 0);
	}
	pmap_update(pmap_kernel());
	memcpy(gdt, old_gdt, NGDT * sizeof(gdt[0]));
	ci->ci_gdt = gdt;
	setsegment(&ci->ci_gdt[GCPU_SEL].sd, ci, 0xfffff,
	    SDT_MEMRWA, SEL_KPL, 1, 1);

	gdt_init_cpu(ci);
}

/*
 * Allocate shadow GDT for a slave CPU.
 */
void
gdt_alloc_cpu(struct cpu_info *ci)
{
	int max_len = MAXGDTSIZ * sizeof(gdt[0]);
	int min_len = MINGDTSIZ * sizeof(gdt[0]);
	struct vm_page *pg;
	vaddr_t va;

	ci->ci_gdt = (union descriptor *)uvm_km_alloc(kernel_map, max_len,
	    0, UVM_KMF_VAONLY);
	for (va = (vaddr_t)ci->ci_gdt; va < (vaddr_t)ci->ci_gdt + min_len;
	    va += PAGE_SIZE) {
		while ((pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO))
		    == NULL) {
			uvm_wait("gdt_alloc_cpu");
		}
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ | VM_PROT_WRITE, 0);
	}
	pmap_update(pmap_kernel());
	memset(ci->ci_gdt, 0, min_len);
	memcpy(ci->ci_gdt, gdt, gdt_count[0] * sizeof(gdt[0]));
	setsegment(&ci->ci_gdt[GCPU_SEL].sd, ci, 0xfffff,
	    SDT_MEMRWA, SEL_KPL, 1, 1);
}


/*
 * Load appropriate gdt descriptor; we better be running on *ci
 * (for the most part, this is how a CPU knows who it is).
 */
void
gdt_init_cpu(struct cpu_info *ci)
{
#ifndef XEN
	struct region_descriptor region;
	size_t max_len;

	max_len = MAXGDTSIZ * sizeof(gdt[0]);
	setregion(&region, ci->ci_gdt, max_len - 1);
	lgdt(&region);
#else
	size_t len = gdt_size[0] * sizeof(gdt[0]);
	unsigned long frames[len >> PAGE_SHIFT];
	vaddr_t va;
	pt_entry_t *ptp;
	int f;

	for (va = (vaddr_t)ci->ci_gdt, f = 0;
	     va < (vaddr_t)ci->ci_gdt + len;
	     va += PAGE_SIZE, f++) {
		KASSERT(va >= VM_MIN_KERNEL_ADDRESS);
		ptp = kvtopte(va);
		frames[f] = *ptp >> PAGE_SHIFT;
		{ 
		   /* 
		    * pmap_pte_clearbits(ptp, PG_RW);
		    * but without spl(), since %fs is not setup
		    * properly yet, ie; curcpu() won't work at this
		    * point and spl() will break.
		    */
			if (HYPERVISOR_update_va_mapping((vaddr_t)va,
				*ptp & ~PG_RW, UVMF_INVLPG) < 0) {
				panic("%s page RO update failed.\n", __func__);
			}
		}
	}

	if (HYPERVISOR_set_gdt(frames, gdt_size[0]))
		panic("HYPERVISOR_set_gdt failed!\n");
	lgdt_finish();
#endif
}

#if defined(MULTIPROCESSOR) && !defined(XEN)

void
gdt_reload_cpu(struct cpu_info *ci)
{
	struct region_descriptor region;
	size_t max_len;

	max_len = MAXGDTSIZ * sizeof(gdt[0]);
	setregion(&region, ci->ci_gdt, max_len - 1);
	lgdt(&region);
}
#endif


/*
 * Grow the GDT.
 */
void
gdt_grow(int which)
{
	size_t old_len, new_len;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	struct vm_page *pg;
	vaddr_t va;

	old_len = gdt_size[which] * sizeof(gdt[0]);
	gdt_size[which] <<= 1;
	new_len = old_len << 1;

#ifdef XEN
	if (which != 0) {
		size_t max_len = MAXGDTSIZ * sizeof(gdt[0]);
		if (old_len == 0) {
			gdt_size[which] = MINGDTSIZ;
			new_len = gdt_size[which] * sizeof(gdt[0]);
		}
		for (CPU_INFO_FOREACH(cii, ci)) {
			for(va = (vaddr_t)(ci->ci_gdt) + old_len + max_len;
			    va < (vaddr_t)(ci->ci_gdt) + new_len + max_len;
			    va += PAGE_SIZE) {
				while ((pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO))
				       == NULL) {
					uvm_wait("gdt_grow");
				}
				pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
					       VM_PROT_READ | VM_PROT_WRITE, 0);
			}
		}
		return;
	}
#endif

	for (CPU_INFO_FOREACH(cii, ci)) {
		for (va = (vaddr_t)(ci->ci_gdt) + old_len;
		     va < (vaddr_t)(ci->ci_gdt) + new_len;
		     va += PAGE_SIZE) {
			while ((pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO)) ==
			    NULL) {
				uvm_wait("gdt_grow");
			}
			pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ | VM_PROT_WRITE, 0);
		}
	}

	pmap_update(pmap_kernel());
}

/*
 * Allocate a GDT slot as follows:
 * 1) If there are entries on the free list, use those.
 * 2) If there are fewer than gdt_size entries in use, there are free slots
 *    near the end that we can sweep through.
 * 3) As a last resort, we increase the size of the GDT, and sweep through
 *    the new slots.
 */

int
gdt_get_slot(void)
{

	KASSERT(mutex_owned(&cpu_lock));

	return gdt_get_slot1(0);
}

int
gdt_get_slot1(int which)
{
	int slot;
	size_t offset;

	KASSERT(mutex_owned(&cpu_lock));

	if (gdt_free[which] != GNULL_SEL) {
		slot = gdt_free[which];
		gdt_free[which] = gdt[slot].gd.gd_selector;
	} else {
		offset = which * MAXGDTSIZ * sizeof(gdt[0]);
		if (gdt_next[which] != gdt_count[which] + offset)
			panic("gdt_get_slot botch 1");
		if (gdt_next[which] - offset >= gdt_size[which]) {
			if (gdt_size[which] >= MAXGDTSIZ)
				panic("gdt_get_slot botch 2");
			gdt_grow(which);
		}
		slot = gdt_next[which]++;
	}

	gdt_count[which]++;
	return (slot);
}

/*
 * Deallocate a GDT slot, putting it on the free list.
 */
void
gdt_put_slot(int slot)
{

	KASSERT(mutex_owned(&cpu_lock));

	gdt_put_slot1(slot, 0);
}

void
gdt_put_slot1(int slot, int which)
{
	union descriptor d;
	d.raw[0] = 0;
	d.raw[1] = 0;

	KASSERT(mutex_owned(&cpu_lock));

	gdt_count[which]--;

	d.gd.gd_type = SDT_SYSNULL;
	d.gd.gd_selector = gdt_free[which];
	update_descriptor(&gdt[slot], &d);

	gdt_free[which] = slot;
}

#ifndef XEN
int
tss_alloc(const struct i386tss *tss)
{
	int slot;

	mutex_enter(&cpu_lock);
	slot = gdt_get_slot();
	setgdt(slot, tss, sizeof(struct i386tss) + IOMAPSIZE - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	mutex_exit(&cpu_lock);

	return GSEL(slot, SEL_KPL);
}

void
tss_free(int sel)
{

	mutex_enter(&cpu_lock);
	gdt_put_slot(IDXSEL(sel));
	mutex_exit(&cpu_lock);
}
#endif

int
ldt_alloc(union descriptor *ldtp, size_t len)
{
	int slot;

	KASSERT(mutex_owned(&cpu_lock));

	if (ldt_count >= ldt_max) {
		return -1;
	}
	ldt_count++;

#ifndef XEN
	slot = gdt_get_slot();
	setgdt(slot, ldtp, len - 1, SDT_SYSLDT, SEL_KPL, 0, 0);
#else
	slot = gdt_get_slot1(1);
	cpu_info_primary.ci_gdt[slot].ld.ld_base = (uint32_t)ldtp;
	cpu_info_primary.ci_gdt[slot].ld.ld_entries =
	    len / sizeof(union descriptor);
#endif

	return GSEL(slot, SEL_KPL);
}

void
ldt_free(int sel)
{
	int slot;

	KASSERT(mutex_owned(&cpu_lock));
	KASSERT(ldt_count > 0);

	slot = IDXSEL(sel);
#ifndef XEN
	gdt_put_slot(slot);
#else
	gdt_put_slot1(slot, 1);
#endif
	ldt_count--;
}
