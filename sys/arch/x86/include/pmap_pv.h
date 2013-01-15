/*	$NetBSD: pmap_pv.h,v 1.3 2011/06/12 03:35:50 rmind Exp $	*/

/*-
 * Copyright (c)2008 YAMAMOTO Takashi,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _X86_PMAP_PV_H_
#define	_X86_PMAP_PV_H_

#include <sys/mutex.h>
#include <sys/queue.h>

struct vm_page;

/*
 * structures to track P->V mapping
 *
 * this file is intended to be minimum as it's included by <machine/vmparam.h>.
 */

/*
 * pv_pte: describe a pte
 */

struct pv_pte {
	struct vm_page *pte_ptp;	/* PTP; NULL for pmap_kernel() */
	vaddr_t pte_va;			/* VA */
};

/*
 * pv_entry: plug pv_pte into lists.
 */

struct pv_entry {
	struct pv_pte pve_pte;		/* should be the first member */
	LIST_ENTRY(pv_entry) pve_list;	/* on pv_head::pvh_list */
	SLIST_ENTRY(pv_entry) pve_hash;
};
#define	pve_next	pve_list.le_next

/*
 * pmap_page: a structure which is embedded in each vm_page.
 */

struct pmap_page {
	union {
		/* PP_EMBEDDED */
		struct pv_pte u_pte;

		/* !PP_EMBEDDED */
		struct pv_head {
			LIST_HEAD(, pv_entry) pvh_list;
		} u_head;

		/* PTPs */
		struct vm_page *u_link;
	} pp_u;
#define	pp_pte	pp_u.u_pte
#define	pp_head	pp_u.u_head
#define	pp_link	pp_u.u_link
	uint8_t pp_flags;
	uint8_t pp_attrs;	/* saved PG_M and PG_U */
};

/* pp_flags */
#define	PP_EMBEDDED	1

#define	PMAP_PAGE_INIT(pp)	/* none */

#endif /* !_X86_PMAP_PV_H_ */
