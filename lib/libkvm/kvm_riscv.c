/*	$NetBSD: kvm_riscv.c,v 1.1 2014/09/19 17:36:25 matt Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
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

/*
 * OR1K machine dependent routines for kvm.
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/types.h>

#include <uvm/uvm_extern.h>

#include <db.h>
#include <limits.h>
#include <kvm.h>
#include <stdlib.h>
#include <unistd.h>

#include "kvm_private.h"

#include <sys/kcore.h>
#include <machine/kcore.h>
#include <machine/vmparam.h>

__RCSID("$NetBSD: kvm_riscv.c,v 1.1 2014/09/19 17:36:25 matt Exp $");

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

/*ARGSUSED*/
int
_kvm_initvtop(kvm_t *kd)
{

	return 0;
}

/*
 * Translate a KVA to a PA
 */
int
_kvm_kvatop(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
//	cpu_kcore_hdr_t	*cpu_kh;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return 0;
	}

	/* No hit -- no translation */
	*pa = (u_long)~0UL;
	return 0;
}

off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	cpu_kcore_hdr_t	*cpu_kh;
	phys_ram_seg_t	*ram;
	off_t		off;
	void		*e;

	cpu_kh = kd->cpu_data;
	e = (char *) kd->cpu_data + kd->cpu_dsize;
        ram = (void *)((char *)(void *)cpu_kh + ALIGN(sizeof *cpu_kh));
	off = kd->dump_off;
	do {
		if (pa >= ram->start && (pa - ram->start) < ram->size) {
			return off + (pa - ram->start);
		}
		ram++;
		off += ram->size;
	} while ((void *) ram < e && ram->size);

	_kvm_err(kd, 0, "pa2off failed for pa %#" PRIxPADDR "\n", pa);
	return (off_t) -1;
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
int
_kvm_mdopen(kvm_t *kd)
{
	kd->usrstack = USRSTACK;
	kd->min_uva = VM_MIN_ADDRESS;
	kd->max_uva = VM_MAXUSER_ADDRESS;

	return (0);
}
