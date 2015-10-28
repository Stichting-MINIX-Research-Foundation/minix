/*	$NetBSD: kvm_vax.c,v 1.20 2014/02/19 20:21:22 dsl Exp $ */

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

/*
 * VAX machine dependent routines for kvm.  Hopefully, the forthcoming
 * vm code will one day obsolete this module.  Furthermore, I hope it
 * gets here soon, because this basically is an error stub! (sorry)
 * This code may not work anyway.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <nlist.h>
#include <stdlib.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>

#include <machine/vmparam.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

__RCSID("$NetBSD: kvm_vax.c,v 1.20 2014/02/19 20:21:22 dsl Exp $");

struct vmstate {
	u_long end;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct vmstate *vm;
	struct stat st;
	struct nlist nl[2];

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0)
		return (-1);

	kd->vmst = vm;

	if (fstat(kd->pmfd, &st) < 0)
		return (-1);

	/* Get end of kernel address */
	nl[0].n_name = "_end";
	nl[1].n_name = 0;
	if (kvm_nlist(kd, nl) != 0) {
		_kvm_err(kd, kd->program, "pmap_stod: no such symbol");
		return (-1);
	}
	vm->end = (u_long)nl[0].n_value;

	return (0);
}

#define VA_OFF(va) (va & (NBPG - 1))

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this
 * physical address.  This routine is used only for crash dumps.
 */
int
_kvm_kvatop(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
	u_long end;

	if (va < (u_long) KERNBASE) {
		_kvm_err(kd, 0, "invalid address (%#"PRIxVADDR"<%lx)", va,
		    (u_long)KERNBASE);
		return (0);
	}

	end = kd->vmst->end;
	if (va >= end) {
		_kvm_err(kd, 0, "invalid address (%#"PRIxVADDR"<%lx)", va,
		    end);
		return (0);
	}

	*pa = (va - (u_long) KERNBASE);
	return (end - va);
}

/*
 * Translate a physical address to a file-offset in the crash dump.
 * XXX - crash dump doesn't work anyway.
 */
off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	return(kd->dump_off + pa);
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
