/*	$NetBSD: atomic_add_32_nv_cas.c,v 1.4 2008/04/28 20:22:52 martin Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#include "atomic_op_namespace.h"

#include <sys/atomic.h>

uint32_t
atomic_add_32_nv(volatile uint32_t *addr, int32_t val)
{
	uint32_t old, new;

	do {
		old = *addr;
		new = old + val;
	} while (atomic_cas_32(addr, old, new) != old);

	return (new);
}

#undef atomic_add_32_nv
atomic_op_alias(atomic_add_32_nv,_atomic_add_32_nv)

#undef atomic_add_int_nv
atomic_op_alias(atomic_add_int_nv,_atomic_add_32_nv)
__strong_alias(_atomic_add_int_nv,_atomic_add_32_nv)

#if !defined(_LP64)
#undef atomic_add_long_nv
atomic_op_alias(atomic_add_long_nv,_atomic_add_32_nv)
__strong_alias(_atomic_add_long_nv,_atomic_add_32_nv)

#undef atomic_add_ptr_nv
atomic_op_alias(atomic_add_ptr_nv,_atomic_add_32_nv)
__strong_alias(_atomic_add_ptr_nv,_atomic_add_32_nv)
#endif /* _LP64 */
