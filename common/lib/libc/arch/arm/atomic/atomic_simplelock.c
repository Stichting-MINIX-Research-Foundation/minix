/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: atomic_simplelock.c,v 1.2 2013/08/16 01:47:41 matt Exp $");

#include <sys/types.h>

#if !defined(_ARM_ARCH_T2)
/*
 * We need to use the inlines so redefine out of the way.
 */
#define	__cpu_simple_lock	__arm_simple_lock
#define	__cpu_simple_lock_try	__arm_simple_lock_try
#include <arm/lock.h>
/*
 * Now get rid of them.
 */
#undef __cpu_simple_lock
#undef __cpu_simple_lock_try

/*
 * Since we overrode lock.h we have to provide these ourselves.
 */
#ifdef __LIBPTHREAD_SOURCE__
__dso_hidden void __cpu_simple_lock(__cpu_simple_lock_t *);
__dso_hidden int __cpu_simple_lock_try(__cpu_simple_lock_t *);
#else
void __cpu_simple_lock(__cpu_simple_lock_t *);
int __cpu_simple_lock_try(__cpu_simple_lock_t *);
#endif

void
__cpu_simple_lock(__cpu_simple_lock_t *alp)
{

	__arm_simple_lock(alp);
}

int
__cpu_simple_lock_try(__cpu_simple_lock_t *alp)
{

	return __arm_simple_lock_try(alp);
}
#endif
