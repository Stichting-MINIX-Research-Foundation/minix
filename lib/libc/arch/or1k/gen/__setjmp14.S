/*	$NetBSD: __setjmp14.S,v 1.1 2014/09/03 19:34:25 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: __setjmp14.S,v 1.1 2014/09/03 19:34:25 matt Exp $")
#endif

/*
 * C library -- _setjmp, _longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v?v:1)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(__setjmp14)
	l.sw	JB_PC(r3), lr		/* save return address */
	l.sw	JB_LR(r3), lr		/* save return address */
	l.sw	JB_SP(r3), r1		/* save stack pointer */
	l.sw	JB_FP(r3), r2		/* save frame pointer */
	l.sw	JB_R10(r3), r10		/* save callee saved register */
	l.sw	JB_R14(r3), r14		/* save callee saved register */
	l.sw	JB_R16(r3), r16		/* save callee saved register */
	l.sw	JB_R18(r3), r18		/* save callee saved register */
	l.sw	JB_R20(r3), r20		/* save callee saved register */
	l.sw	JB_R22(r3), r22		/* save callee saved register */
	l.sw	JB_R24(r3), r24		/* save callee saved register */
	l.sw	JB_R26(r3), r26		/* save callee saved register */
	l.sw	JB_R28(r3), r28		/* save callee saved register */
	l.sw	JB_R30(r3), r30		/* save callee saved register */
	l.addi	r4, r0, 1
	l.sw	JB_MAGIC(r3), r4	/* note we saved sigmask */
	l.addi	r5, r3, JB_SIGMASK	# &sigmask
	l.xor	r4, r4, r4
	l.addi	r3, r0, SIG_BLOCK
	_DOSYSCALL(__sigprocmask14)	# assume no error	XXX
	l.xor	r11, r11, r11
	l.jr	lr
	l.nop
END(__setjmp14)

ENTRY(__longjmp14)
	l.lwz	lr, JB_LR(r3)		/* get return address */
	l.lwz	r1, JB_SP(r3)		/* get stack pointer */
	l.lwz	r2, JB_FP(r3)		/* get frame pointer */
	l.lwz	r10, JB_R10(r3)		/* get callee saved register */
	l.lwz	r14, JB_R14(r3)		/* get callee saved register */
	l.lwz	r16, JB_R16(r3)		/* get callee saved register */
	l.lwz	r18, JB_R18(r3)		/* get callee saved register */
	l.lwz	r20, JB_R20(r3)		/* get callee saved register */
	l.lwz	r22, JB_R22(r3)		/* get callee saved register */
	l.lwz	r24, JB_R24(r3)		/* get callee saved register */
	l.lwz	r26, JB_R26(r3)		/* get callee saved register */
	l.lwz	r28, JB_R28(r3)		/* get callee saved register */
	l.lwz	r30, JB_R30(r3)		/* get callee saved register */
	l.ori	r11, r4, 0
	l.xor	r5, r5, r5
	l.addi	r4, r3, JB_SIGMASK	# &sigmask
	l.addi	r3, r0, SIG_SETMASK
	_DOSYSCALL(__sigprocmask14)	# assume no error	XXX
	l.addi	r6, r0, 1
	l.sfeqi	r11, 0
	l.cmov	r11, r6, r11
	l.jr	lr
	l.nop
END(__longjmp14)
