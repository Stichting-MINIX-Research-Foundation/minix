/* $NetBSD: kvm_arm.c,v 1.6 2010/09/20 23:23:16 jym Exp $	 */

/*-
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: kvm_powerpc.c,v 1.3 1997/09/19 04:00:23 thorpej Exp
 */

/*
 * arm32 machine dependent routines for kvm.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: kvm_arm.c,v 1.6 2010/09/20 23:23:16 jym Exp $");
#endif				/* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/kcore.h>
#include <sys/types.h>

#include <arm/kcore.h>
#include <arm/arm32/pte.h>

#include <stdlib.h>
#include <db.h>
#include <limits.h>
#include <kvm.h>

#include <unistd.h>

#include "kvm_private.h"

void
_kvm_freevtop(kvm_t * kd)
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

int
_kvm_initvtop(kvm_t * kd)
{
	return 0;
}

int
_kvm_kvatop(kvm_t * kd, vaddr_t va, paddr_t *pa)
{
	cpu_kcore_hdr_t *cpu_kh;
	pd_entry_t      pde;
	pt_entry_t      pte;
	paddr_t		pde_pa, pte_pa;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return (0);
	}
	cpu_kh = kd->cpu_data;

	if (cpu_kh->version != 1) {
		_kvm_err(kd, 0, "unsupported kcore structure version");
		return 0;
	}
	if (cpu_kh->flags != 0) {
		_kvm_err(kd, 0, "kcore flags not supported");
		return 0;
	}
	/*
	 * work out which L1 table we need
	 */
	if (va >= (cpu_kh->UserL1TableSize << 17))
		pde_pa = cpu_kh->PAKernelL1Table;
	else
		pde_pa = cpu_kh->PAUserL1Table;

	/*
	 * work out the offset into the L1 Table
	 */
	pde_pa += ((va >> 20) * sizeof(pd_entry_t));

	if (_kvm_pread(kd, kd->pmfd, (void *) &pde, sizeof(pd_entry_t),
		  _kvm_pa2off(kd, pde_pa)) != sizeof(pd_entry_t)) {
		_kvm_syserr(kd, 0, "could not read L1 entry");
		return (0);
	}
	/*
	 * next work out what kind of record it is
	 */
	switch (pde & L1_TYPE_MASK) {
	case L1_TYPE_S:
		*pa = (pde & L1_S_FRAME) | (va & L1_S_OFFSET);
		return L1_S_SIZE - (va & L1_S_OFFSET);
	case L1_TYPE_C:
		pte_pa = (pde & L1_C_ADDR_MASK)
			| ((va & 0xff000) >> 10);
		break;
	case L1_TYPE_F:
		pte_pa = (pde & L1_S_ADDR_MASK)
			| ((va & 0xffc00) >> 8);
		break;
	default:
		_kvm_syserr(kd, 0, "L1 entry is invalid");
		return (0);
	}

	/*
	 * locate the pte and load it
	 */
	if (_kvm_pread(kd, kd->pmfd, (void *) &pte, sizeof(pt_entry_t),
		  _kvm_pa2off(kd, pte_pa)) != sizeof(pt_entry_t)) {
		_kvm_syserr(kd, 0, "could not read L2 entry");
		return (0);
	}
	switch (pte & L2_TYPE_MASK) {
	case L2_TYPE_L:
		*pa = (pte & L2_L_FRAME) | (va & L2_L_OFFSET);
		return (L2_L_SIZE - (va & L2_L_OFFSET));
	case L2_TYPE_S:
		*pa = (pte & L2_S_FRAME) | (va & L2_S_OFFSET);
		return (L2_S_SIZE - (va & L2_S_OFFSET));
	case L2_TYPE_T:
		*pa = (pte & L2_T_FRAME) | (va & L2_T_OFFSET);
		return (L2_T_SIZE - (va & L2_T_OFFSET));
	default:
		_kvm_syserr(kd, 0, "L2 entry is invalid");
		return (0);
	}

	_kvm_err(kd, 0, "vatop not yet implemented!");
	return 0;
}

off_t
_kvm_pa2off(kvm_t * kd, u_long pa)
{
	cpu_kcore_hdr_t *cpu_kh;
	phys_ram_seg_t *ramsegs;
	off_t           off;
	int             i;

	cpu_kh = kd->cpu_data;
	ramsegs = (void *) ((char *) (void *) cpu_kh + cpu_kh->omemsegs);

	off = 0;
	for (i = 0; i < cpu_kh->nmemsegs; i++) {
		if (pa >= ramsegs[i].start &&
		    (pa - ramsegs[i].start) < ramsegs[i].size) {
			off += (pa - ramsegs[i].start);
			break;
		}
		off += ramsegs[i].size;
	}
	return (kd->dump_off + off);
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. arm)
 */
int
_kvm_mdopen(kvm_t * kd)
{
	uintptr_t       max_uva;
	extern struct ps_strings *__ps_strings;

#if 0				/* XXX - These vary across arm machines... */
	kd->usrstack = USRSTACK;
	kd->min_uva = VM_MIN_ADDRESS;
	kd->max_uva = VM_MAXUSER_ADDRESS;
#endif
	/* This is somewhat hack-ish, but it works. */
	max_uva = (uintptr_t) (__ps_strings + 1);
	kd->usrstack = max_uva;
	kd->max_uva = max_uva;
	kd->min_uva = 0;

	return (0);
}
