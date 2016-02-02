/*	$NetBSD: atomic_or_32.S,v 1.1 2014/09/03 19:34:25 matt Exp $	*/
/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>
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

#include "atomic_op_asm.h"

ENTRY_NP(_atomic_or_32)
1:	l.lwa	r11, 0(r3)		/* load old value */
	l.or	r5, r11, r4		/* calculate new value */
	l.swa	0(r3), r5		/* try to store */
	l.bnf	1b			/*   succeed? no, try again */
	l.nop				/* -- delay slot -- */
	l.jr	lr			/* return old value */
	l.nop
END(_atomic_or_32)

ATOMIC_OP_ALIAS(atomic_or_32,_atomic_or_32)
ATOMIC_OP_ALIAS(atomic_or_uint,_atomic_or_32)
ATOMIC_OP_ALIAS(atomic_or_ulong,_atomic_or_32)
CRT_ALIAS(__sync_fetch_and_or_4,_atomic_or_32)
CRT_ALIAS(__atomic_fetch_or_4,_atomic_or_32)
STRONG_ALIAS(_atomic_or_uint,_atomic_or_32)
STRONG_ALIAS(_atomic_or_ulong,_atomic_or_32)

ENTRY_NP(_atomic_or_32_nv)
1:	l.lwa	r5, 0(r3)		/* load old value */
	l.or	r11, r5, r4		/* calculate new value */
	l.swa	0(r3), r11		/* try to store */
	l.bnf	1b			/*   succeed? no, try again */
	l.nop				/* -- delay slot -- */
	l.jr	lr			/* return old value */
	l.nop
END(_atomic_or_32_nv)

ATOMIC_OP_ALIAS(atomic_or_32_nv,_atomic_or_32_nv)
ATOMIC_OP_ALIAS(atomic_or_uint_nv,_atomic_or_32_nv)
ATOMIC_OP_ALIAS(atomic_or_ulong_nv,_atomic_or_32_nv)
CRT_ALIAS(__sync_or_and_fetch_4,_atomic_or_32_nv)
STRONG_ALIAS(_atomic_or_uint_nv,_atomic_or_32_nv)
STRONG_ALIAS(_atomic_or_ulong_nv,_atomic_or_32_nv)
