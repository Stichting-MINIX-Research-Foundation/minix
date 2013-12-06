/*	$NetBSD: compat_sigsetjmp.S,v 1.3 2013/11/30 20:54:14 joerg Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <machine/setjmp.h>

/*
 * C library -- sigsetjmp, siglongjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a, m)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(sigsetjmp)
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbnz	r1, 1f
	b	PLT_SYM(_C_LABEL(_setjmp))
1:	b	PLT_SYM(_C_LABEL(setjmp))
#elif !defined(__thumb__)
	cmp	r1, #0
	beq	PLT_SYM(_C_LABEL(_setjmp))
	b	PLT_SYM(_C_LABEL(setjmp))
#else
	push	{r3, lr}
	beq	1f
	bl	PLT_SYM(_C_LABEL(setjmp))
	pop	{r3, pc}
1:	bl	PLT_SYM(_C_LABEL(_setjmp))
	pop	{r3, pc}
#endif
END(sigsetjmp)

ENTRY(siglongjmp)
	ldr	r2, .L_setjmp_magic
	ldr	r3, [r0]
	cmp	r2, r3
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
#if defined(__thumb__)
	it	eq
#endif
	beq	PLT_SYM(_C_LABEL(_longjmp))
	b	PLT_SYM(_C_LABEL(longjmp))
#else
	push	{lr}
	sub	sp, sp, #4
	beq	1f
	bl	PIC_SYM(_C_LABEL(longjmp), PLT)
	b	2f
1:	bl	PIC_SYM(_C_LABEL(_longjmp), PLT)
2:	add	sp, sp, #4
	pop	{pc}
#endif

	.align	0
.L_setjmp_magic:
	.word	_JB_MAGIC__SETJMP
END(siglongjmp)
