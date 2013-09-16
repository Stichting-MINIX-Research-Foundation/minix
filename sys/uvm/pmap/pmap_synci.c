/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

__KERNEL_RCSID(0, "$NetBSD: pmap_synci.c,v 1.1 2012/10/03 00:51:46 christos Exp $");

#define __PMAP_PRIVATE

#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/atomic.h>
#include <sys/cpu.h>
 
#include <uvm/uvm.h>

#if defined(MULTIPROCESSOR)
void
pmap_syncicache_ast(struct cpu_info *ci)
{
	struct pmap_tlb_info * const ti = ci->ci_tlb_info;

	KASSERT(kpreempt_disabled());

	uint32_t page_bitmap = atomic_swap_32(&ti->ti_synci_page_bitmap, 0);
#if 0
	printf("%s: need to sync %#x\n", __func__, page_bitmap);
#endif
	ti->ti_evcnt_synci_asts.ev_count++;
	/*
	 * If every bit is set in the bitmap, sync the entire icache.
	 */
	if (page_bitmap == pmap_tlb_synci_map_mask) {
		pmap_md_icache_sync_all();
		ti->ti_evcnt_synci_all.ev_count++;
		ti->ti_evcnt_synci_pages.ev_count += pmap_tlb_synci_page_mask+1;
		kpreempt_enable();
		return;
	}

	/*
	 * Loop through the bitmap clearing each set of indices for each page.
	 */
	for (vaddr_t va = 0;
	     page_bitmap != 0;
	     page_bitmap >>= 1, va += PAGE_SIZE) {
		if (page_bitmap & 1) {
			/*
			 * Each bit set represents a page index to be synced.
			 */
			pmap_md_icache_sync_range_index(va, PAGE_SIZE);
			ti->ti_evcnt_synci_pages.ev_count++;
		}
	}

	kpreempt_enable();
}

void
pmap_tlb_syncicache(vaddr_t va, uint32_t page_onproc)
{
	KASSERT(kpreempt_disabled());
	/*
	 * We don't sync the icache here but let ast do it for us just before
	 * returning to userspace.  We do this because we don't really know
	 * on which CPU we will return to userspace and if we synch the icache
	 * now it might not be on the CPU we need it on.  In addition, others
	 * threads might sync the icache before we get to return to userland
	 * so there's no reason for us to do it.
	 *
	 * Each TLB/cache keeps a synci sequence number which gets advanced
	 * each time that TLB/cache performs a pmap_md_sync_icache_all.  When
	 * we return to userland, we check the pmap's corresponding synci
	 * sequence number for that TLB/cache.  If they match, it means that
	 * no one has yet synched the icache so we much do it ourselves.  If
	 * they don't match someone has already synced the icache for us.
	 *
	 * There is a small chance that the generation numbers will wrap and
	 * then become equal but that's a one in 4 billion cache and will
	 * just cause an extra sync of the icache.
	 */
	const uint32_t cpu_mask = 1L << cpu_index(curcpu());
	const uint32_t page_mask =
	    1L << ((va >> PGSHIFT) & pmap_tlb_synci_page_mask);
	uint32_t onproc = 0;
	for (size_t i = 0; i < pmap_ntlbs; i++) {
		struct pmap_tlb_info * const ti = pmap_tlbs[i];
		TLBINFO_LOCK(ti);
		for (;;) {
			uint32_t old_page_bitmap = ti->ti_synci_page_bitmap;
			if (old_page_bitmap & page_mask) {
				ti->ti_evcnt_synci_duplicate.ev_count++;
				break;
			}

			uint32_t orig_page_bitmap = atomic_cas_32(
			    &ti->ti_synci_page_bitmap, old_page_bitmap,
			    old_page_bitmap | page_mask);

			if (orig_page_bitmap == old_page_bitmap) {
				if (old_page_bitmap == 0) {
					onproc |= ti->ti_cpu_mask;
				} else {
					ti->ti_evcnt_synci_deferred.ev_count++;
				}
				ti->ti_evcnt_synci_desired.ev_count++;
				break;
			}
		}
#if 0
		printf("%s: %s: %x to %x on cpus %#x\n", __func__,
		    ti->ti_name, page_mask, ti->ti_synci_page_bitmap,
		     onproc & page_onproc & ti->ti_cpu_mask);
#endif
		TLBINFO_UNLOCK(ti);
	}
	onproc &= page_onproc;
	if (__predict_false(onproc != 0)) {
		/*
		 * If the cpu need to sync this page, tell the current lwp
		 * to sync the icache before it returns to userspace.
		 */
		if (onproc & cpu_mask) {
			if (curcpu()->ci_flags & CPUF_USERPMAP) {
				curlwp->l_md.md_astpending = 1;	/* force call to ast() */
				curcpu()->ci_evcnt_synci_onproc_rqst.ev_count++;
			} else {
				curcpu()->ci_evcnt_synci_deferred_rqst.ev_count++;
			}
			onproc ^= cpu_mask;
		}

		/*
		 * For each cpu that is affect, send an IPI telling
		 * that CPU that the current thread needs to sync its icache.
		 * We might cause some spurious icache syncs but that's not
		 * going to break anything.
		 */
		for (u_int n = ffs(onproc);
		     onproc != 0;
		     onproc >>= n, onproc <<= n, n = ffs(onproc)) {
			cpu_send_ipi(cpu_lookup(n-1), IPI_SYNCICACHE);
		}
	}
}

void
pmap_tlb_syncicache_wanted(struct cpu_info *ci)
{
	struct pmap_tlb_info * const ti = ci->ci_tlb_info;

	KASSERT(cpu_intr_p());

	TLBINFO_LOCK(ti);

	/*
	 * We might have been notified because another CPU changed an exec
	 * page and now needs us to sync the icache so tell the current lwp
	 * to do the next time it returns to userland (which should be very
	 * soon).
	 */
	if (ti->ti_synci_page_bitmap && (ci->ci_flags & CPUF_USERPMAP)) {
		curlwp->l_md.md_astpending = 1;	/* force call to ast() */
		ci->ci_evcnt_synci_ipi_rqst.ev_count++;
	}

	TLBINFO_UNLOCK(ti);

}
#endif /* MULTIPROCESSOR */
