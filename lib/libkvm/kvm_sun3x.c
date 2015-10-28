/*	$NetBSD: kvm_sun3x.c,v 1.12 2011/09/14 12:37:55 christos Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_sparc.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: kvm_sun3x.c,v 1.12 2011/09/14 12:37:55 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * Sun3x machine dependent routines for kvm.
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

int   _kvm_sun3x_initvtop(kvm_t *);
void  _kvm_sun3x_freevtop(kvm_t *);
int   _kvm_sun3x_kvatop  (kvm_t *, vaddr_t, paddr_t *);
off_t _kvm_sun3x_pa2off  (kvm_t *, paddr_t);

struct kvm_ops _kvm_ops_sun3x = {
	_kvm_sun3x_initvtop,
	_kvm_sun3x_freevtop,
	_kvm_sun3x_kvatop,
	_kvm_sun3x_pa2off };

#define	_kvm_kvas_size(h)	\
	(-((h)->kernbase))
#define	_kvm_nkptes(h, v)	\
	(_kvm_kvas_size((h)) >> (v)->pgshift)
#define	_kvm_pg_pa(pte, h)	\
	((pte) & (h)->pg_frame)

/*
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files.  Nothing to do here.
 */
int
_kvm_sun3x_initvtop(kvm_t *kd)
{
	return 0;
}

void
_kvm_sun3x_freevtop(kvm_t *kd)
{
}

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this
 * physical address.  This routine is used only for crash dumps.
 */
int
_kvm_sun3x_kvatop(kvm_t *kd, vaddr_t va, paddr_t *pap)
{
	cpu_kcore_hdr_t *h = kd->cpu_data;
	struct sun3x_kcore_hdr *s = &h->un._sun3x;
	struct vmstate *v = kd->vmst;
	int idx, len, offset, pte;
	u_long pteva, pa;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return(0);
	}

	if (va < h->kernbase) {
		_kvm_err(kd, 0, "not a kernel address");
		return(0);
	}

	/*
	 * If this VA is in the contiguous range, short-cut.
	 * Note that this ends our recursion when we call
	 * kvm_read to access the kernel page table, which
	 * is guaranteed to be in the contiguous range.
	 */
	if (va < s->contig_end) {
		len = s->contig_end - va;
		pa = va - h->kernbase;
		goto done;
	}

	/*
	 * The KVA is beyond the contiguous range, so we must
	 * read the PTE for this KVA from the page table.
	 */
	idx = ((va - h->kernbase) >> v->pgshift);
	pteva = s->kernCbase + (idx * 4);
	if (kvm_read(kd, pteva, &pte, 4) != 4) {
		_kvm_err(kd, 0, "can not read PTE!");
		return (0);
	}
	if ((pte & s->pg_valid) == 0) {
		_kvm_err(kd, 0, "page not valid (VA=0x%lx)", va);
		return (0);
	}
	offset = va & v->pgofset;
	len = (h->page_size - offset);
	pa = _kvm_pg_pa(pte, s) + offset;

done:
	*pap = pa;
	return (len);
}

/*
 * Translate a physical address to a file-offset in the crash dump.
 */
off_t
_kvm_sun3x_pa2off(kvm_t *kd, paddr_t pa)
{
	off_t		off;
	phys_ram_seg_t	*rsp;
	cpu_kcore_hdr_t *h = kd->cpu_data;
	struct sun3x_kcore_hdr *s = &h->un._sun3x;

	off = 0;
	for (rsp = s->ram_segs; rsp->size; rsp++) {
		if (pa >= rsp->start && pa < rsp->start + rsp->size) {
			pa -= rsp->start;
			break;
		}
		off += rsp->size;
	}
	return (kd->dump_off + off + pa);
}

