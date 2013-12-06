/*	$NetBSD: sigsetjmp.S,v 1.7 2013/09/12 15:36:16 joerg Exp $	*/
/*
 * Copyright (c) 1995 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "SYS.h"

#ifdef __PIC__

ENTRY(__sigsetjmp14)
	PIC_PROLOGUE(%g1, %g2)		! %g1 = _GLOBAL_OFFSET_TABLE
	tst	%o1
	bnz	1f
	 st	%o1, [%o0 + 56]		! jmpbuf[JBLEN] = savemask

	!! if (savemask == 0) goto _setjmp
	set	_C_LABEL(_setjmp), %g2
	ld	[%g1 + %g2], %g1
	jmp %g1
	 nop

	!! if (savemask != 0) goto setjmp
1:	set	_C_LABEL(__setjmp14), %g2
	ld	[%g1 + %g2], %g1
	jmp %g1
	 nop


ENTRY(__siglongjmp14)
	PIC_PROLOGUE(%g1, %g2)		! %g1 = _GLOBAL_OFFSET_TABLE
	ld	[%o0 + 56], %g2		! restoremask = jmpbuf[JBLEN]
	tst	%g2
	bnz	1f
	 nop

	!! if (restoremask == 0) goto _longjmp
	set	_C_LABEL(_longjmp), %g2
	ld	[%g1 + %g2], %g1
	jmp %g1
	 nop

	!! if (restoremask != 0) goto longjmp
1:	set	_C_LABEL(__longjmp14), %g2
	ld	[%g1 + %g2], %g1
	jmp %g1
	 nop

#else /* !__PIC__ */

ENTRY(__sigsetjmp14)
	cmp %o1,0
	be _C_LABEL(_setjmp)
	st %o1,[%o0+56]		! jmpbuf[JBLEN]
	nop
	ba,a _C_LABEL(__setjmp14)
	unimp 0

ENTRY(__siglongjmp14)
	ld [%o0+56],%g1
	cmp %g1,0
	be _C_LABEL(_longjmp)
	nop
	ba,a _C_LABEL(__longjmp14)
	unimp 0

#endif /* !__PIC__ */
