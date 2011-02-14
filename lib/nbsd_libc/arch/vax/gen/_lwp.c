/*	$NetBSD: _lwp.c,v 1.1 2009/06/03 01:02:28 christos Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas
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
__RCSID("$NetBSD: _lwp.c,v 1.1 2009/06/03 01:02:28 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <inttypes.h>
#include <ucontext.h>
#include <lwp.h>
#include <stdlib.h>

void
_lwp_makecontext(ucontext_t *u, void (*start)(void *),
    void *arg, void *private, caddr_t stack_base, size_t stack_size)
{
	__greg_t *gr = u->uc_mcontext.__gregs;
	int *sp;

	getcontext(u);
	u->uc_link = NULL;

	u->uc_stack.ss_sp = stack_base;
	u->uc_stack.ss_size = stack_size;

	/* Align to a word */
	/* LINTED uintptr_t is safe */
	sp = (int *)((uintptr_t)(stack_base + stack_size) & ~0x3);
	
	/*
	 * Allocate necessary stack space for arguments including arg count
	 * and call frame
	 */
	sp -= 1 + 1 + 5;

	sp[0] = 0;			/* condition handler is null */
	sp[1] = 0x20000000;		/* make this a CALLS frame */
	sp[2] = 0;			/* saved argument pointer */
	sp[3] = 0;			/* saved frame pointer */
	sp[4] = (intptr_t)_lwp_exit + 2;/* return via _lwp_exit */
	sp[5] = 1;			/* argc */
	sp[6] = (intptr_t)arg;		/* argv */
	
	gr[_REG_AP] = (__greg_t)(sp + 5);
	gr[_REG_SP] = (__greg_t)sp;
	gr[_REG_FP] = (__greg_t)sp;
	gr[_REG_PC] = (__greg_t)start + 2;
}
