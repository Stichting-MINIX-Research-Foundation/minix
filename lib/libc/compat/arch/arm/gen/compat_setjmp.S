/*	$NetBSD: compat_setjmp.S,v 1.4 2013/11/30 20:54:14 joerg Exp $	*/

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
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(setjmp)
	/* Block all signals and retrieve the old signal mask */
	push	{r0, lr}
	movs	r0, #0x00000000

	bl	PLT_SYM(_C_LABEL(sigblock))
	mov	r1, r0

	/* Store signal mask */
	str	r1, [r0, #(_JB_SIGMASK * 4)]

	ldr	r1, .Lsetjmp_magic
	str	r1, [r0]

	/* Store integer registers */
	adds	r0, r0, #(_JB_REG_R4 * 4)
#ifdef __thumb__
#ifdef _ARM_ARCH_7
        stmia	r0!, {r4-r12}
	str	sp, [r0, #0]
	str	lr, [r0, #4]
#else
        stmia	r0!, {r4-r7}
	mov	r2, r8
	mov	r3, r9
	stmia	r0!, {r2-r3}
	mov	r1, r10
	mov	r2, r11
	mov	r3, r12
	stmia	r0!, {r1-r3}
	mov	r2, sp
	mov	r3, lr
	stmia	r0!, {r2-r3}
#endif
#else
        stmia	r0, {r4-lr}
#endif
        movs	r0, #0
	pop	{r3, pc}
END(setjmp)

ENTRY(longjmp)
	ldr	r2, .Lsetjmp_magic
	ldr	r3, [r0]
	cmp	r2, r3
	bne	botch

	/* Fetch signal mask */
	ldr	r2, [r0, #(_JB_SIGMASK * 4)]

	/* Set signal mask */
	push	{r0, r1}	/* don't care about lr */

	mov	r0, r2
	bl	PLT_SYM(_C_LABEL(sigsetmask))

	pop	{r0, r1} 

	/* Restore integer registers */
	adds	r0, r0, #(_JB_REG_R4 * 4)
#if !defined(__thumb__) || defined(_ARM_ARCH_7)
        ldmia	r0!, {r4-r12}
#else
        ldmia	r0!, {r4-r7}
        ldmia	r0!, {r2-r3}
	mov	r8, r2
	mov	r9, r3
        ldmia	r0!, {r1-r3}
	mov	r10, r2
	mov	r11, r3
        ldmia	r0!, {r2}
	mov	r12, r2
#endif

	ldmia	r0!, {r2-r3}	/* r2 = sp, r3 = lr */

	/* Validate sp */
	cmp	r2, #0
	beq	botch
	mov	sp, r2

	/* Validate lr */
	cmp	r3, #0
	beq	botch
	mov	lr, r3

	/* Set return value */
	mov	r0, r1
	cmp	r0, #0
#ifdef __thumb__
	bne	1f
	movs	r0, #1
1:
#else
	moveq	r0, #1
#endif
	RET

	/* validation failed, die die die. */
botch:
	bl	PLT_SYM(_C_LABEL(longjmperror))
	bl	PLT_SYM(_C_LABEL(abort))
2:	b	2b		/* Cannot get here */

	.align	0
.Lsetjmp_magic:
	.word	_JB_MAGIC_SETJMP
END(longjmp)
