/*	$NetBSD: sigsetjmp.S,v 1.10 2011/04/30 19:39:38 martin Exp $	*/
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

/*
 * We use the sc_npc field inside the sigcontext to store the save mask
 */
#define	MASK_OFFSET	0x68

#ifdef PIC
#ifdef BIGPIC
	
ENTRY(__sigsetjmp14)
	PIC_PROLOGUE(%g1,%o2)		! %g1 = _GLOBAL_OFFSET_TABLE
	st	%o1,[%o0+MASK_OFFSET]	! end of jmpbuf
	sethi	%hi(_C_LABEL(__setjmp14)), %o2
	sethi	%hi(_C_LABEL(_setjmp)), %o3
	or	%o2, %lo(_C_LABEL(__setjmp14)), %o2
	or	%o3, %lo(_C_LABEL(_setjmp)), %o3
	brz	%o1, 1f	
	 ldx	[%g1+%o3], %o3		! if (%o1 == 0) goto _setjmp;
	ldx	[%g1+%o2], %o3		! else goto __setjmp;
1:
	jmp	%o3
	 nop
	
ENTRY(__siglongjmp14)
	PIC_PROLOGUE(%g1,%o2)		! %g1 = _GLOBAL_OFFSET_TABLE
	ld	[%o0+MASK_OFFSET], %o2	! end of jmpbuf
	sethi	%hi(_C_LABEL(__longjmp14)), %o3 
	sethi	%hi(_C_LABEL(_longjmp)), %o4
	or	%o3, %lo(_C_LABEL(__longjmp14)), %o3
	or	%o4, %lo(_C_LABEL(_longjmp)), %o4
	brz,a	%o2, 1f	
	 ldx	[%g1+%o4], %o4		! if (%o2 == 0) goto _longjmp;
	ldx	[%g1+%o3], %o4		! else goto __longjmp;
1:
	jmp	%o4	
	 nop
	unimp 0
#else

ENTRY(__sigsetjmp14)
	PIC_PROLOGUE(%g1,%o2)			! %g1 = _GLOBAL_OFFSET_TABLE
	cmp	%o1,0
	st	%o1,[%o0+MASK_OFFSET]		! end of jmpbuf
	bne,a	1f
	 ldx	[%g1+_C_LABEL(__setjmp14)], %g1	! if (%o1 != 0) goto __setjmp;
	ldx	[%g1+_C_LABEL(_setjmp)], %g1	! else goto _setjmp;
1:
	jmp %g1
	 nop

ENTRY(__siglongjmp14)
	PIC_PROLOGUE(%g1,%o2)			! %g1 = _GLOBAL_OFFSET_TABLE
	ld	[%o0+MASK_OFFSET],%o2		! end of jmpbuf
	cmp	%o2,0
	bne,a	1f
	 ldx	[%g1+_C_LABEL(__longjmp14)], %g1 ! if (%o2 != 0) goto __longjmp;
	ldx	[%g1+_C_LABEL(_longjmp)], %g1	! else goto _longjmp;
1:
	jmp %g1
	 nop
	unimp 0
#endif /* BIGPIC */
#else /* PIC */

ENTRY(__sigsetjmp14)
	cmp	%o1,0
	be	_C_LABEL(_setjmp)
	 st	%o1,[%o0+MASK_OFFSET]	! end of jmpbuf
	ba,a	_C_LABEL(__setjmp14)
	unimp 0

ENTRY(__siglongjmp14)
	ld	[%o0+MASK_OFFSET],%g1
	cmp	%g1,0
	be	_C_LABEL(_longjmp)
	 nop
	ba,a	_C_LABEL(__longjmp14)
	unimp 0

#endif /* PIC */
