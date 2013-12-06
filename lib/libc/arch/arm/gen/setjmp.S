/*	$NetBSD: setjmp.S,v 1.17 2013/11/30 20:20:42 joerg Exp $	*/

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

#if !defined(__SOFTFP__) && !defined(__VFP_FP__) && !defined(__ARM_PCS)
#error FPA is not supported anymore
#endif

#if defined(__ARM_EABI__) && !defined(__ARM_PCS_VFP)
	.fpu	vfp
#endif

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

ENTRY(__setjmp14)
	/* Get the signal mask. */
	push	{r0-r2, lr}
#if !defined(__thumb__)
	adds	r2, r0, #(_JB_SIGMASK * 4)
#else
	mov	r2, r0
	adds	r2, r2, #(_JB_SIGMASK * 4)
#endif
	movs	r1, #0
	movs	r0, #0
	bl	PLT_SYM(_C_LABEL(__sigprocmask14))
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	pop	{r0-r2, lr}
#else
	pop	{r0-r3}
	mov	lr, r3
#endif

	ldr	r1, .Lsetjmp_magic

#if defined(__ARM_EABI__) && (!defined(__thumb__) || defined(_ARM_ARCH_T2))
	ldr	r2, .Lfpu_present
#ifdef __PIC__
	add	r2, r2, pc	/* pc = &.LPIC0 */
#endif
	ldr	r2, [r2]
.LPIC0:
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r2, 1f		/* don't save if we don't have a FPU */
#else
	cmp	r2, #0		/* do we have a FPU? */
	beq	1f		/*   no, don't save VFP registers */
#endif

	orrs	r1, r1, #(_JB_MAGIC_SETJMP ^ _JB_MAGIC_SETJMP_VFP)
				/* change magic to VFP magic */
	adds	r2, r0, #(_JB_REG_D8 * 4)
	vstmia	r2, {d8-d15}
	vmrs	r2, fpscr
	str	r2, [r0, #(_JB_REG_FPSCR * 4)]
1:
#endif /* __ARM_EABI__ && (!__thumb__ || _ARM_ARCH_T2) */

	str	r1, [r0]		/* store magic */

	/* Store integer registers */
	adds	r0, r0, #(_JB_REG_R4 * 4)
#if !defined(__thumb__)
	stmia	r0!, {r4-lr}
#else
	stmia	r0!, {r4-r7}
	mov	r2, r8
	mov	r3, r9
	stmia	r0!, {r2-r3}
	mov	r2, r10
	mov	r3, r11
	stmia	r0!, {r2-r3}
	mov	r2, sp
	mov	r3, lr
	str	r2, [r0, #4]
	str	r3, [r0, #8]
#endif
	movs	r0, #0
	RET

#if defined(__ARM_EABI__) && (!defined(__thumb__) || defined(_ARM_ARCH_T2))
	.align	0
.Lfpu_present:
	.word	REL_SYM(_libc_arm_fpu_present, .LPIC0)
#endif /* __ARM_EABI__ && (!__thumb__ || _ARM_ARCH_T2) */
END(__setjmp14)

ENTRY(__longjmp14)
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	ldr	r2, [r0]
	ldr	ip, .Lsetjmp_magic
	bic	r3, r2, #(_JB_MAGIC_SETJMP ^ _JB_MAGIC_SETJMP_VFP)
#else
	ldr	r3, .Lsetjmp_magic
	mov	ip, r3
	ldr	r3, [r0]
	movs	r2, #(_JB_MAGIC_SETJMP ^ _JB_MAGIC_SETJMP_VFP)
	bics	r3, r2 
	/* r2 is not the magic but we don't need it since we can't do VFP */
#endif
	cmp	r3, ip
	bne	.Lbotch

	/* Restore the signal mask. */
	push	{r0-r2, lr}
	movs	r2, #0
#if !defined(__thumb__)
	adds	r1, r0, #(_JB_SIGMASK * 4)
#else
	mov	r1, r0
	adds	r1, r1, #(_JB_SIGMASK * 4)
#endif
	movs	r0, #3				/* SIG_SETMASK */
	bl	PLT_SYM(_C_LABEL(__sigprocmask14))
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	pop	{r0-r2, lr}
#else
	pop	{r0-r3}
	mov	lr, r3
#endif

#if defined(__ARM_EABI__) && (!defined(__thumb__) || defined(_ARM_ARCH_T2))
	tst	r2, #(_JB_MAGIC_SETJMP ^ _JB_MAGIC_SETJMP_VFP)
						/* is this a VFP magic? */
	beq	1f				/*   no, don't restore VFP */
	adds	r2, r0, #(_JB_REG_D8 * 4)
	vldmia	r2, {d8-d15}
	ldr	r2, [r0, #(_JB_REG_FPSCR * 4)]
	vmsr	fpscr, r2
1:
#endif /* __ARM_EABI__ && (!__thumb__ || _ARM_ARCH_T2) */

	adds	r0, r0, #(_JB_REG_R4 * 4)
	/* Restore integer registers */
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	ldmia	r0!, {r4-r12}
#else
	ldmia	r0!, {r4-r7}
	ldmia	r0!, {r2-r3}
	mov	r8, r2
	mov	r9, r3
	ldmia	r0!, {r2-r3}
	mov	r10, r2
	mov	r11, r3
	adds	r0, r0, #4	/* skip ip(r12) */
#endif

	ldmia	r0!, {r2-r3}

	/* Validate sp and lr */
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r2, .Lbotch
#else
	cmp	r2, #0
	beq	.Lbotch
#endif
	mov	sp, r2

#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r3, .Lbotch
#else
	cmp	r3, #0
	beq	.Lbotch
#endif
	mov	lr, r3

	/* Set return value */
	movs	r0, r1
#if !defined(__thumb__)
	moveq	r0, #1
#else
	bne	1f
	movs	r0, #1
1:
#endif
	RET

	/* validation failed, die die die. */
.Lbotch:
	bl	PLT_SYM(_C_LABEL(longjmperror))
	bl	PLT_SYM(_C_LABEL(abort))
1:	b	1b		/* Cannot get here */

	.align	0
.Lsetjmp_magic:
	.word	_JB_MAGIC_SETJMP
END(__longjmp14)
