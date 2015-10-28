/*	$NetBSD: kvm_sun3.c,v 1.15 2011/09/14 12:37:55 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_sparc.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: kvm_sun3.c,v 1.15 2011/09/14 12:37:55 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * Sun3 machine dependent routines for kvm.
 *
 * Note: This file has to build on ALL m68k machines,
 * so do NOT include any <machine / *.h> files here.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/kcore.h>

#include <unistd.h>
#include <limits.h>
#include <nlist.h>
#include <kvm.h>
#include <db.h>

#include <m68k/kcore.h>

#include "kvm_private.h"
#include "kvm_m68k.h"

int   _kvm_sun3_initvtop(kvm_t *);
void  _kvm_sun3_freevtop(kvm_t *);
int   _kvm_sun3_kvatop  (kvm_t *, vaddr_t, paddr_t *);
off_t _kvm_sun3_pa2off  (kvm_t *, paddr_t);

struct kvm_ops _kvm_ops_sun3 = {
	_kvm_sun3_initvtop,
	_kvm_sun3_freevtop,
	_kvm_sun3_kvatop,
	_kvm_sun3_pa2off };

#define	_kvm_pg_pa(v, s, pte)	\
	(((pte) & (s)->pg_frame) << (v)->pgshift)

#define	_kvm_va_segnum(s, x)	\
	((u_int)(x) >> (s)->segshift)
#define	_kvm_pte_num_mask(v)	\
	(0xf << (v)->pgshift)
#define	_kvm_va_pte_num(v, va)	\
	(((va) & _kvm_pte_num_mask((v))) >> (v)->pgshift)

/*
 * XXX Re-define these here, no other place for them.
 */
#define	NKSEG		256	/* kernel segmap entries */
#define	NPAGSEG		16	/* pages per segment */

/* Finally, our local stuff... */
struct private_vmstate {
	/* Page Map Entry Group (PMEG) */
	int   pmeg[NKSEG][NPAGSEG];
};

/*
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files. We use the MMU specific goop written at the
 * beginning of a crash dump by dumpsys()
 * Note: sun3 MMU specific!
 */
int
_kvm_sun3_initvtop(kvm_t *kd)
{
	cpu_kcore_hdr_t *h = kd->cpu_data;
	char *p;

	p = kd->cpu_data;
	p += (h->page_size - sizeof(kcore_seg_t));
	kd->vmst->private = p;

	return (0);
}

void
_kvm_sun3_freevtop(kvm_t *kd)
{
	/* This was set by pointer arithmetic, not allocation. */
	kd->vmst->private = (void*)0;
}

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this
 * physical address.  This routine is used only for crash dumps.
 */
int
_kvm_sun3_kvatop(kvm_t *kd, vaddr_t va, paddr_t *pap)
{
	cpu_kcore_hdr_t *h = kd->cpu_data;
	struct sun3_kcore_hdr *s = &h->un._sun3;
	struct vmstate *v = kd->vmst;
	struct private_vmstate *pv = v->private;
	int pte, offset;
	u_int segnum, sme, ptenum;
	paddr_t pa;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return(0);
	}

	if (va < h->kernbase) {
		_kvm_err(kd, 0, "not a kernel address");
		return(0);
	}

	/*
	 * Get the segmap entry (sme) from the kernel segmap.
	 * Note: only have segmap entries from KERNBASE to end.
	 */
	segnum = _kvm_va_segnum(s, va - h->kernbase);
	ptenum = _kvm_va_pte_num(v, va);
	offset = va & v->pgofset;

	/* The segmap entry selects a PMEG. */
	sme = s->ksegmap[segnum];
	pte = pv->pmeg[sme][ptenum];

	if ((pte & (s)->pg_valid) == 0) {
		_kvm_err(kd, 0, "page not valid (VA=%#"PRIxVADDR")", va);
		return (0);
	}
	pa = _kvm_pg_pa(v, s, pte) + offset;

	*pap = pa;
	return (h->page_size - offset);
}

/*
 * Translate a physical address to a file-offset in the crash dump.
 */
off_t
_kvm_sun3_pa2off(kvm_t *kd, paddr_t pa)
{
	return(kd->dump_off + pa);
}
