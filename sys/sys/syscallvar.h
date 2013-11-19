/*	$NetBSD: syscallvar.h,v 1.5 2009/06/02 23:21:38 pooka Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software developed for The NetBSD Foundation
 * by Andrew Doran.
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

#ifndef _SYS_SYSCALLVAR_H_
#define	_SYS_SYSCALLVAR_H_

#ifndef _KERNEL
#error nothing of interest to userspace here
#endif

#include <sys/systm.h>
#include <sys/proc.h>

extern struct emul emul_netbsd;

struct syscall_package {
	u_short		sp_code;
	u_short		sp_flags;
	sy_call_t	*sp_call;
};

void	syscall_init(void);
int	syscall_establish(const struct emul *, const struct syscall_package *);
int	syscall_disestablish(const struct emul *, const struct syscall_package *);

static inline int
sy_call(const struct sysent *sy, struct lwp *l, const void *uap,
	register_t *rval)
{
	int error;

	l->l_sysent = sy;
	error = (*sy->sy_call)(l, uap, rval);
	l->l_sysent = NULL;

	return error;
}

/* inclusion in the kernel currently depends on SYSCALL_DEBUG */
extern const char * const syscallnames[];

#endif	/* _SYS_SYSCALLVAR_H_ */
