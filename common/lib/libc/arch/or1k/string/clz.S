/*	$NetBSD: clz.S,v 1.1 2014/09/03 19:34:25 matt Exp $ */

/*-
 * Copyright (C) 2001	Martin J. Laubach <mjl@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*----------------------------------------------------------------------*/

#include <machine/asm.h>

__RCSID("$NetBSD: clz.S,v 1.1 2014/09/03 19:34:25 matt Exp $")

ENTRY_NP(__clzsi2)
#ifdef _LP64
	l.slli	r3, r3, 32
	l.addi	r5, r0, 64
#else
	l.addi	r5, r0, 32
#endif
	l.fl1	r11, r3
	l.sub	r11, r5, r11
	l.jr	lr
	l.nop
END(__clzsi2)

ENTRY_NP(__clzdi2)
#ifdef _LP64
	l.addi	r5, r0, 64
#else
	l.sfeqi	r3, 0
	l.bf	1f
	l.nop
	l.addi	r5, r0, 32
#endif
	l.fl1	r11, r3
	l.sub	r11, r5, r11
	l.jr	lr
	l.nop
#ifndef _LP64
1:	l.addi	r5, r0, 64
	l.fl1	r11, r4
	l.sub	r11, r5, r11
	l.jr	lr
	l.nop
#endif
END(__clzdi2)
