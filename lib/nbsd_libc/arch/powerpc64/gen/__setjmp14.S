/*	$NetBSD: __setjmp14.S,v 1.2 2009/03/08 18:57:12 he Exp $	*/

#include <sys/syscall.h>

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$NetBSD: __setjmp14.S,v 1.2 2009/03/08 18:57:12 he Exp $"
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
	mr	%r6,%r3
	li	%r3,1			# SIG_BLOCK
	li	%r4,0
	addi	%r5,%r6,100		# &sigmask
	li	%r0,SYS___sigprocmask14
	sc				# assume no error	XXX
	mflr	%r11
	mfcr	%r12
	mr	%r10,%r1
	mr	%r9,%r2

	std	%r8,8(%r6)		# save r8-r31
	std	%r9,16(%r6)
	std	%r10,24(%r6)
	std	%r11,32(%r6)
	std	%r12,40(%r6)
	std	%r13,48(%r6)
	std	%r14,56(%r6)
	std	%r15,64(%r6)
	std	%r16,72(%r6)
	std	%r17,80(%r6)
	std	%r18,88(%r6)
	std	%r19,96(%r6)
	std	%r20,104(%r6)
	std	%r21,112(%r6)
	std	%r22,120(%r6)
	std	%r23,128(%r6)
	std	%r24,136(%r6)
	std	%r25,144(%r6)
	std	%r26,152(%r6)
	std	%r27,160(%r6)
	std	%r28,168(%r6)
	std	%r29,176(%r6)
	std	%r30,184(%r6)
	std	%r31,192(%r6)

	li	%r3,0
	blr

ENTRY(__longjmp14)
	ld	%r8,8(%r6)		# load r8-r31
	ld	%r9,16(%r6)
	ld	%r10,24(%r6)
	ld	%r11,32(%r6)
	ld	%r12,40(%r6)
	ld	%r13,48(%r6)
	ld	%r14,56(%r6)
	ld	%r15,64(%r6)
	ld	%r16,72(%r6)
	ld	%r17,80(%r6)
	ld	%r18,88(%r6)
	ld	%r19,96(%r6)				   
	ld	%r20,104(%r6)
	ld	%r21,112(%r6)
	ld	%r22,120(%r6)
	ld	%r23,128(%r6)
	ld	%r24,136(%r6)
	ld	%r25,144(%r6)
	ld	%r26,152(%r6)
	ld	%r27,160(%r6)
	ld	%r28,168(%r6)
	ld	%r29,176(%r6)
	ld	%r30,184(%r6)
	ld	%r31,192(%r6)

	mr	%r6,%r4
	mtlr	%r11
	mtcr	%r12
	mr	%r2,%r9
	mr	%r1,%r10
	addi	%r4,%r3,100		# &sigmask
	li	%r3,3			# SIG_SETMASK
	li	%r5,0
	li	%r0,SYS___sigprocmask14
	sc				# assume no error	XXX
	or.	%r3,%r6,%r6
	bnelr
	li	%r3,1
	blr
