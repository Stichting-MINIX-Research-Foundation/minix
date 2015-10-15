/*	$NetBSD: pmap_tlb.h,v 1.8 2015/04/02 06:17:52 matt Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 1987 Carnegie-Mellon University
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_COMMON_PMAP_TLB_H_
#define	_COMMON_PMAP_TLB_H_

#include <sys/evcnt.h>
#include <sys/kcpuset.h>

#if !defined(PMAP_TLB_MAX)
# if defined(MULTIPROCESSOR)
#  define PMAP_TLB_MAX		MAXCPUS
# else
#  define PMAP_TLB_MAX		1
# endif
#endif

/*
 * Per TLB (normally same as CPU) asid info
 */
struct pmap_asid_info {
	LIST_ENTRY(pmap_asid_info) pai_link;
	uint32_t	pai_asid;	/* TLB address space tag */
};

#define	TLBINFO_LOCK(ti)		mutex_spin_enter((ti)->ti_lock)
#define	TLBINFO_UNLOCK(ti)		mutex_spin_exit((ti)->ti_lock)
#define	PMAP_PAI_ASIDVALID_P(pai, ti)	((pai)->pai_asid != 0)
#define	PMAP_PAI(pmap, ti)		(&(pmap)->pm_pai[tlbinfo_index(ti)])
#define	PAI_PMAP(pai, ti)	\
	((pmap_t)((intptr_t)(pai) \
	    - offsetof(struct pmap, pm_pai[tlbinfo_index(ti)])))

enum tlb_invalidate_op {
	TLBINV_NOBODY=0,
	TLBINV_ONE=1,
	TLBINV_ALLUSER=2,
	TLBINV_ALLKERNEL=3,
	TLBINV_ALL=4
};

struct pmap_tlb_info {
	char ti_name[8];
	uint32_t ti_asids_free;		/* # of ASIDs free */
#define	tlbinfo_noasids_p(ti)	((ti)->ti_asids_free == 0)
	kmutex_t *ti_lock;
	u_int ti_wired;			/* # of wired TLB entries */
	tlb_asid_t ti_asid_hint;		/* probable next ASID to use */
	tlb_asid_t ti_asid_max;
	LIST_HEAD(, pmap_asid_info) ti_pais; /* list of active ASIDs */
#ifdef MULTIPROCESSOR
	pmap_t ti_victim;
	uint32_t ti_synci_page_bitmap;	/* page indices needing a syncicache */
#if PMAP_TLB_MAX > 1
	kcpuset_t *ti_kcpuset;		/* bitmask of CPUs sharing this TLB */
	u_int ti_index;
	enum tlb_invalidate_op ti_tlbinvop;
#define tlbinfo_index(ti)	((ti)->ti_index)
#else
#define tlbinfo_index(ti)	((void)(ti), 0)
#endif
	struct evcnt ti_evcnt_synci_asts;
	struct evcnt ti_evcnt_synci_all;
	struct evcnt ti_evcnt_synci_pages;
	struct evcnt ti_evcnt_synci_deferred;
	struct evcnt ti_evcnt_synci_desired;
	struct evcnt ti_evcnt_synci_duplicate;
#else
#define tlbinfo_index(ti)	((void)(ti), 0)
#endif
	struct evcnt ti_evcnt_asid_reinits;
	u_long ti_asid_bitmap[256 / (sizeof(u_long) * 8)];
};

#ifdef	_KERNEL
extern struct pmap_tlb_info pmap_tlb0_info;
#ifdef MULTIPROCESSOR
extern struct pmap_tlb_info *pmap_tlbs[PMAP_TLB_MAX];
extern u_int pmap_ntlbs;
#endif

#ifndef cpu_set_tlb_info
# define cpu_set_tlb_info(ci, ti)	((void)((ci)->ci_tlb_info = (ti)))
#endif
#ifndef cpu_tlb_info
# if PMAP_TLB_MAX > 1
#  define cpu_tlb_info(ci)		((ci)->ci_tlb_info)
# else
#  define cpu_tlb_info(ci)		(&pmap_tlb0_info)
# endif
#endif

#ifdef MULTIPROCESSOR
void	pmap_tlb_shootdown_process(void);
bool	pmap_tlb_shootdown_bystanders(pmap_t pmap);
void	pmap_tlb_info_attach(struct pmap_tlb_info *, struct cpu_info *);
void	pmap_md_tlb_info_attach(struct pmap_tlb_info *, struct cpu_info *);
#endif
void	pmap_tlb_info_init(struct pmap_tlb_info *);
void	pmap_tlb_info_evcnt_attach(struct pmap_tlb_info *);
void	pmap_tlb_asid_acquire(pmap_t, struct lwp *l);
void	pmap_tlb_asid_deactivate(pmap_t);
void	pmap_tlb_asid_release_all(pmap_t);
int	pmap_tlb_update_addr(pmap_t, vaddr_t, uint32_t, u_int);
#define	PMAP_TLB_NEED_IPI	0x01
#define	PMAP_TLB_INSERT		0x02
void	pmap_tlb_invalidate_addr(pmap_t, vaddr_t);
void	pmap_tlb_check(pmap_t, bool (*)(void *, vaddr_t, tlb_asid_t, pt_entry_t));
void	pmap_tlb_asid_check(void);

#endif	/* _KERNEL */
#endif	/* _COMMON_PMAP_TLB_H_ */
