/*	$NetBSD: ucontext.h,v 1.17 2012/09/12 02:00:54 manu Exp $	*/

/*-
 * Copyright (c) 1999, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein, and by Jason R. Thorpe.
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

#ifndef _SYS_UCONTEXT_H_
#define _SYS_UCONTEXT_H_

#include <sys/sigtypes.h>
#include <machine/mcontext.h>

typedef struct __ucontext	ucontext_t;

struct __ucontext {
	unsigned int	uc_flags;	/* properties */
	ucontext_t * 	uc_link;	/* context to resume */
	mcontext_t	uc_mcontext;	/* machine state */
	sigset_t	uc_sigmask;	/* signals blocked in this context */
	stack_t		uc_stack;	/* the stack used by this context */
#if defined(_UC_MACHINE_PAD)
	long		__uc_pad[_UC_MACHINE_PAD];
#endif
};

#ifndef _UC_UCONTEXT_ALIGN
#define _UC_UCONTEXT_ALIGN (~0)
#endif

#define UCF_SWAPPED	001 /* Context has been swapped in by swapcontext(3) */
#define UCF_IGNFPU	002 /* Ignore FPU context by get or setcontext(3) */
#define UCF_IGNSIGM	004 /* Ignore signal mask by get or setcontext(3) */

#define NCARGS 6

#ifdef __minix
__BEGIN_DECLS
void resumecontext(ucontext_t *ucp);

/* These functions get and set ucontext structure through PM/kernel. They don't
 * manipulate the stack. */
int getuctx(ucontext_t *ucp);
int setuctx(const ucontext_t *ucp);
__END_DECLS
#endif /* __minix */

#endif /* !_SYS_UCONTEXT_H_ */
