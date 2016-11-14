/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>

__KERNEL_RCSID(1, "$NetBSD: kern_sa_60.c,v 1.1 2012/02/19 17:40:46 matt Exp $");

#include <sys/systm.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

int
compat_60_sys_sa_register(lwp_t *l,
	const struct compat_60_sys_sa_register_args *uap,
	register_t *retval)
{
	return sys_nosys(l, uap, retval);
}

int
compat_60_sys_sa_stacks(lwp_t *l,
	const struct compat_60_sys_sa_stacks_args *uap,
	register_t *retval)
{
	return sys_nosys(l, uap, retval);
}

int
compat_60_sys_sa_enable(lwp_t *l,
	const void *uap,
	register_t *retval)
{
	return sys_nosys(l, uap, retval);
}

int
compat_60_sys_sa_setconcurrency(lwp_t *l,
	const struct compat_60_sys_sa_setconcurrency_args *uap,
	register_t *retval)
{
	return sys_nosys(l, uap, retval);
}

int
compat_60_sys_sa_yield(lwp_t *l,
	const void *uap,
	register_t *retval)
{
	return sys_nosys(l, uap, retval);
}

int
compat_60_sys_sa_preempt(lwp_t *l,
	const struct compat_60_sys_sa_preempt_args *uap,
	register_t *retval)
{
	return sys_nosys(l, uap, retval);
}
