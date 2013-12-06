/*	$NetBSD: _setjmp.S,v 1.16 2013/11/30 20:20:42 joerg Exp $	*/

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
 * C library -- _setjmp, _longjmp
 *
 *	_longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	_setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is NOT restored.
 *
 * Note: r0 is the return value
 *       r1-r3,ip are scratch registers in functions
 */

ENTRY(_setjmp)
	ldr	r1, .L_setjmp_magic

#if defined(__ARM_EABI__) && (!defined(__thumb__) || defined(_ARM_ARCH_T2))
	ldr	r2, .Lfpu_present
#ifdef __PIC__
	add	r2, r2, pc	/* pc = &.LPIC0 */
#endif
	ldr	r2, [r2]
.LPIC0:
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r2, 1f
#else
	cmp	r2, #0		/* do we have a FPU? */
	beq	1f		/*   no, don't save VFP registers */
#endif

	orrs	r1, r1, #(_JB_MAGIC__SETJMP ^ _JB_MAGIC__SETJMP_VFP)
				/* change magic to VFP magic */
	adds	r2, r0, #(_JB_REG_D8 * 4)
	vstmia	r2, {d8-d15}
	vmrs	r2, fpscr
	str	r2, [r0, #(_JB_REG_FPSCR * 4)]
1:
#endif /* __ARM_EABI__ && (_ARM_ARCH_T2 || !__thumb__) */

	str	r1, [r0]

	adds	r0, r0, #(_JB_REG_R4 * 4)
	/* Store integer registers */
#if !defined(__thumb__) || defined(_ARCH_ARCH_T2)
        stmia	r0, {r4-r14}
#else
        stmia	r0!, {r4-r7}
	mov	r1, r8
	mov	r2, r9
	mov	r3, r10
	stmia	r0!, {r1-r3}
	mov	r2, r11
	mov	r3, r12
	stmia	r0!, {r2-r3}
	mov	r2, sp
	mov	r3, lr
	stmia	r0!, {r2-r3}
#endif

        movs	r0, #0
        RET

#if defined(__ARM_EABI__) && (!defined(__thumb__) || defined(_ARM_ARCH_T2))
	.align	0
.Lfpu_present:
	.word	REL_SYM(_libc_arm_fpu_present, .LPIC0)
#endif /* __ARM_EABI__ && (_ARM_ARCH_T2 || !__thumb__) */
END(_setjmp)

ENTRY(_longjmp)
	ldr	r2, [r0]			/* get magic from jmp_buf */
#if !defined(__thumb__) || defined(_ARM_ARCH_T2)
	bics	r3, r2, #(_JB_MAGIC__SETJMP ^ _JB_MAGIC__SETJMP_VFP)
						/* ignore VFP-ness of magic */
	ldr	ip, .L_setjmp_magic		/* load magic */
#else
	ldr	r3, .L_setjmp_magic		/* load magic */
	mov	ip, r3
	movs	r3, #(_JB_MAGIC__SETJMP ^ _JB_MAGIC__SETJMP_VFP)
	ands	r3, r3, r2
	eors	r3, r3, r2			/* ignore VFP-ness of magic */
#endif
	cmp	ip, r3				/* magic correct? */
	bne	botch				/*   no, botch */

#if defined(__ARM_EABI__) && (!defined(__thumb__) || defined(_ARM_ARCH_T2))
	cmp	r3, r2				/* did magic change? */
	beq	1f				/*   no, don't restore VFP */
	adds	r3, r0, #(_JB_REG_D8 * 4)
	vldmia	r3, {d8-d15}
	ldr	r3, [r0, #(_JB_REG_FPSCR * 4)]
	vmsr	fpscr, r3
1:
#endif /* __ARM_EABI__ && (_ARM_ARCH_T2 || !__thumb__) */

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
	mov	r1, r3
	adds	r0, r0, #4		/* skip r12 */
#endif
	ldmia	r0!, {r2-r3}		/* r2 = sp, r3 = lr */

	/* Validate sp and r14 */
#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r2, botch
#else
	cmp	r2, #0
	beq	botch
#endif
	mov	sp, r2

#if defined(__thumb__) && defined(_ARM_ARCH_T2)
	cbz	r3, botch
#else
	cmp	r3, #0
	beq	botch
#endif
	mov	lr, r3

	/* Set return value */
	movs	r0, r1
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
1:	b	1b		/* Cannot get here */

	.align	0
.L_setjmp_magic:
	.word	_JB_MAGIC__SETJMP
END(_longjmp)
