/*	$NetBSD: kvm_m68k.c,v 1.19 2014/01/27 21:00:01 matt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross and Jason R. Thorpe.
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
 * Run-time kvm dispatcher for m68k machines.
 * The actual MD code is in the files:
 * kvm_m68k_cmn.c kvm_sun3.c ...
 *
 * Note: This file has to build on ALL m68k machines,
 * so do NOT include any <machine/[*].h> files here.
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/kcore.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <limits.h>
#include <nlist.h>
#include <kvm.h>
#include <db.h>

#include <m68k/kcore.h>

#include "kvm_private.h"
#include "kvm_m68k.h"

__RCSID("$NetBSD: kvm_m68k.c,v 1.19 2014/01/27 21:00:01 matt Exp $");

struct name_ops {
	const char *name;
	struct kvm_ops *ops;
};

/*
 * Match specific kcore types first, falling into a default.
 */
static struct name_ops optbl[] = {
	{ "sun2",	&_kvm_ops_sun2 },
	{ "sun3",	&_kvm_ops_sun3 },
	{ "sun3x",	&_kvm_ops_sun3x },
	{ NULL,		&_kvm_ops_cmn },
};

/*
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files.  This is where we do the dispatch work.
 */
int
_kvm_initvtop(kvm_t *kd)
{
	cpu_kcore_hdr_t *h;
	struct name_ops *nop;
	struct vmstate *vm;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof (*vm));
	if (vm == 0)
		return (-1);

	kd->vmst = vm;

	/*
	 * Use the machine name in the kcore header to determine
	 * our ops vector.  When we reach an ops vector with
	 * no name, we've found a default.
	 */
	h = kd->cpu_data;
	h->name[sizeof(h->name) - 1] = '\0';	/* sanity */
	for (nop = optbl; nop->name != NULL; nop++)
		if (strcmp(nop->name, h->name) == 0)
			break;

	vm->ops = nop->ops;

	/*
	 * Compute pgshift and pgofset.
	 */
	for (vm->pgshift = 0; (1 << vm->pgshift) < h->page_size; vm->pgshift++)
		/* nothing */ ;
	if ((1 << vm->pgshift) != h->page_size)
		goto bad;
	vm->pgofset = h->page_size - 1;

	if ((vm->ops->initvtop)(kd) < 0)
		goto bad;

	return (0);

 bad:
	kd->vmst = NULL;
	free(vm);
	return (-1);
}

void
_kvm_freevtop(kvm_t *kd)
{
	(kd->vmst->ops->freevtop)(kd);
	free(kd->vmst);
}

int
_kvm_kvatop(kvm_t *kd, vaddr_t va, paddr_t *pap)
{
	return ((kd->vmst->ops->kvatop)(kd, va, pap));
}

off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	return ((kd->vmst->ops->pa2off)(kd, pa));
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
int
_kvm_mdopen(kvm_t *kd)
{
	u_long max_uva;
	extern struct ps_strings *__ps_strings;

#if 0	/* XXX - These vary across m68k machines... */
	kd->usrstack = USRSTACK;
	kd->min_uva = VM_MIN_ADDRESS;
	kd->max_uva = VM_MAXUSER_ADDRESS;
#endif
	/* This is somewhat hack-ish, but it works. */
	max_uva = (u_long) (__ps_strings + 1);
	kd->usrstack = max_uva;
	kd->max_uva  = max_uva;
	kd->min_uva  = 0;

	return (0);
}
