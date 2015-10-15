/*	$NetBSD: vmpagemd.h,v 1.2 2014/03/04 06:14:53 matt Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Raytheon BBN Technologies Corp and Defense Advanced Research Projects
 * Agency and which was developed by Matt Thomas of 3am Software Foundry.
 *
 * This material is based upon work supported by the Defense Advanced Research
 * Projects Agency and Space and Naval Warfare Systems Center, Pacific, under
 * Contract No. N66001-09-C-2073.
 * Approved for Public Release, Distribution Unlimited
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

#ifndef _COMMON_PMAP_TLB_VMPAGEMD_H_
#define _COMMON_PMAP_TLB_VMPAGEMD_H_

#ifdef _LOCORE
#error use assym.h instead
#endif

#ifdef _MODULE
#error this file should not be included by loadable kernel modules
#endif

#ifdef _KERNEL_OPT
#include "opt_modular.h"
#include "opt_multiprocessor.h"
#endif

#include <sys/mutex.h>

#define	__HAVE_VM_PAGE_MD

typedef struct pv_entry {
	struct pv_entry *pv_next;
	struct pmap *pv_pmap;
	vaddr_t pv_va;
} *pv_entry_t;

#define	VM_PAGEMD_REFERENCED	0x0001	/* page has been recently referenced */
#define	VM_PAGEMD_MODIFIED	0x0002	/* page has been modified */
#define	VM_PAGEMD_POOLPAGE	0x0004	/* page is used as a poolpage */
#define	VM_PAGEMD_EXECPAGE	0x0008	/* page is exec mapped */
#ifdef __PMAP_VIRTUAL_CACHE_ALIASES
#define	VM_PAGEMD_UNCACHED	0x0010	/* page is mapped uncached */
#endif

#ifdef __PMAP_VIRTUAL_CACHE_ALIASES
#define	VM_PAGEMD_CACHED_P(mdpg)	(((mdpg)->mdpg_attrs & VM_PAGEMD_UNCACHED) == 0)
#define	VM_PAGEMD_UNCACHED_P(mdpg)	(((mdpg)->mdpg_attrs & VM_PAGEMD_UNCACHED) != 0)
#endif
#define	VM_PAGEMD_MODIFIED_P(mdpg)	(((mdpg)->mdpg_attrs & VM_PAGEMD_MODIFIED) != 0)
#define	VM_PAGEMD_REFERENCED_P(mdpg)	(((mdpg)->mdpg_attrs & VM_PAGEMD_REFERENCED) != 0)
#define	VM_PAGEMD_POOLPAGE_P(mdpg)	(((mdpg)->mdpg_attrs & VM_PAGEMD_POOLPAGE) != 0)
#define	VM_PAGEMD_EXECPAGE_P(mdpg)	(((mdpg)->mdpg_attrs & VM_PAGEMD_EXECPAGE) != 0)

struct vm_page_md {
	volatile u_int mdpg_attrs;	/* page attributes */
	struct pv_entry mdpg_first;	/* pv_entry first */
#if defined(MULTIPROCESSOR) || defined(MODULAR)
	kmutex_t *mdpg_lock;		/* pv list lock */
#define	VM_PAGEMD_PVLIST_LOCK_INIT(mdpg) 	\
	(mdpg)->mdpg_lock = NULL
#define	VM_PAGEMD_PVLIST_LOCK(pg, list_change)	\
	pmap_pvlist_lock(mdpg, list_change)
#define	VM_PAGEMD_PVLIST_UNLOCK(mdpg)		\
	mutex_spin_exit((mdpg)->mdpg_lock)
#define	VM_PAGEMD_PVLIST_LOCKED_P(mdpg)		\
	mutex_owner((mdpg)->mdpg_lock)
#define	VM_PAGEMD_PVLIST_GEN(mdpg)		\
	((uint16_t)((mdpg)->mdpg_attrs >> 16))
#else
#define	VM_PAGEMD_PVLIST_LOCK_INIT(mdpg)	do { } while (/*CONSTCOND*/ 0)
#define	VM_PAGEMD_PVLIST_LOCK(mdpg, lc)	(mutex_spin_enter(&pmap_pvlist_mutex), 0)
#define	VM_PAGEMD_PVLIST_UNLOCK(mdpg)	mutex_spin_exit(&pmap_pvlist_mutex)
#define	VM_PAGEMD_PVLIST_LOCKED_P(mdpg)	true
#define	VM_PAGEMD_PVLIST_GEN(mdpg)		(0)
#endif /* MULTIPROCESSOR || MODULAR */
};

#define VM_MDPAGE_INIT(pg)						\
do {									\
	(pg)->mdpage.mdpg_first.pv_next = NULL;				\
	(pg)->mdpage.mdpg_first.pv_pmap = NULL;				\
	(pg)->mdpage.mdpg_first.pv_va = (pg)->phys_addr;		\
	(pg)->mdpage.mdpg_attrs = 0;					\
	VM_PAGEMD_PVLIST_LOCK_INIT(&(pg)->mdpage);			\
} while (/* CONSTCOND */ 0)

#endif /* __COMMON_PMAP_TLB_VMPAGEMD_H_ */
