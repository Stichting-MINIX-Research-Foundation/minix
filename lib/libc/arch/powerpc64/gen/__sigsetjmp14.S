/*	$NetBSD: __sigsetjmp14.S,v 1.3 2014/08/23 02:24:22 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS)
	.text
	.asciz "$NetBSD: __sigsetjmp14.S,v 1.3 2014/08/23 02:24:22 matt Exp $"
#endif

ENTRY(__sigsetjmp14)
	mr	%r6,%r3
	or.	%r7,%r4,%r4
	beq	1f
	li	%r3,1			# SIG_BLOCK
	li	%r4,0
	addi	%r5,%r6,100		# &sigmask
	_DOSYSCALL(__sigprocmask14)	# assume no error	XXX
1:
	mflr	%r11
	mfcr	%r12
	mr	%r10,%r1
	mr	%r9,%r2
	i = 0
	.rept	32-7
	std	7+i,i*8(%r6)		# save r7-r31
	i = i + 1
	.endr
	li	%r3,0
	blr
END(__sigsetjmp14)

ENTRY(__siglongjmp14)
	i = 0
	.rept	32-7
	ld	7+i,i*8(%r3)		# load r7-r31
	i = i + 1
	.endr
	mr	%r6,%r4
	mtlr	%r11
	mtcr	%r12
	mr	%r2,%r9
	mr	%r1,%r10
	or.	%r7,%r7,%r7
	beq	1f
	addi	%r4,%r3,100		# &sigmask
	li	%r3,3			# SIG_SETMASK
	li	%r5,0
	_DOSYSCALL(__sigprocmask14)	# assume no error	XXX
1:
	or.	%r3,%r6,%r6
	bnelr
	li	%r3,1
	blr
END(__siglongjmp14)
