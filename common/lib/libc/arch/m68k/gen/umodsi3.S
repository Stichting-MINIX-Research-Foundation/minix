/*	$NetBSD: umodsi3.S,v 1.5 2014/03/04 06:15:33 matt Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)umodsi3.s	5.1 (Berkeley) 6/7/90")(
#else
	RCSID("$NetBSD: umodsi3.S,v 1.5 2014/03/04 06:15:33 matt Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

/* unsigned % unsigned */
#ifndef __mc68010__
ENTRY(__umodsi3)
	movel	4(%sp),%d1
#ifdef __mcoldfire__
	remul	8(%sp),%d0:%d1
	divul	8(%sp),%d1
#else
	divull	8(%sp),%d0:%d1
#endif
	rts
END(__umodsi3)
#else
ENTRY(__umodsi3)
| NB: this requires that __udivsi3 preserve the %a0
| register, and that it returns the modulus in %d1:
	movel	(%sp)+, %a0	| pop the return address
	jsr	_C_LABEL(__udivsi3)
	movel	%d1, %d0	| move the modulus into %d0
	jmp	(%a0)		| return
END(__umodsi3)
#endif /* __mc68010__ */
