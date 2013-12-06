/*	$NetBSD: ucontext.h,v 1.6 2012/05/21 14:15:19 martin Exp $	*/

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

#ifndef _COMPAT_SYS_UCONTEXT_H_
#define _COMPAT_SYS_UCONTEXT_H_

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd32.h"
#endif

#include <compat/sys/sigtypes.h>

#if defined(COMPAT_NETBSD32) && defined(_KERNEL)

typedef struct __ucontext32       ucontext32_t;

struct __ucontext32 {
	unsigned int	uc_flags;       /* properties */
	uint32_t 	uc_link;        /* context to resume */
	sigset_t	uc_sigmask;     /* signals blocked in this context */
	stack32_t	uc_stack;       /* the stack used by this context */
	mcontext32_t	uc_mcontext;    /* machine state */
#if defined(_UC_MACHINE32_PAD)
	int		__uc_pad[_UC_MACHINE32_PAD];
#endif
};

#ifdef __UCONTEXT32_SIZE
__CTASSERT(sizeof(ucontext32_t) == __UCONTEXT32_SIZE);
#endif

#endif /* COMPAT_NETBSD32 && _KERNEL */

#ifdef _KERNEL
#ifdef COMPAT_NETBSD32
struct lwp;
void	getucontext32(struct lwp *, ucontext32_t *);
int	setucontext32(struct lwp *, const ucontext32_t *);
int	cpu_mcontext32_validate(struct lwp *, const mcontext32_t *);
void	cpu_getmcontext32(struct lwp *, mcontext32_t *, unsigned int *);
int	cpu_setmcontext32(struct lwp *, const mcontext32_t *, unsigned int);
#endif /* COMPAT_NETBSD32 */
#endif /* _KERNEL */

#endif /* !_COMPAT_SYS_UCONTEXT_H_ */
