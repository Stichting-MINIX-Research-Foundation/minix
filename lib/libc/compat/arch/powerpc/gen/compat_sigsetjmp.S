/*	$NetBSD: compat_sigsetjmp.S,v 1.2 2011/01/15 07:31:13 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: compat_sigsetjmp.S,v 1.2 2011/01/15 07:31:13 matt Exp $")
#endif

ENTRY(sigsetjmp)
	mr	%r6,%r3
	or.	%r7,%r4,%r4
	beq	1f
	li	%r3,SIG_BLOCK
	li	%r4,0
	_DOSYSCALL(compat_13_sigprocmask13) # assume no error	XXX
1:
	mflr	%r11
	mfcr	%r12
	mr	%r10,%r1
	mr	%r9,%r2
	mr	%r8,%r3
	stmw	%r7,0(%r6)
	li	%r3,0
	blr
END(sigsetjmp)

ENTRY(siglongjmp)
	lmw	%r7,0(%r3)
	mr	%r6,%r4
	mtlr	%r11
	mtcr	%r12
	mr	%r2,%r9
	mr	%r1,%r10
	or.	%r7,%r7,%r7
	beq	1f
	mr	%r4,%r8
	li	%r3,SIG_SETMASK
	_DOSYSCALL(compat_13_sigprocmask13) # assume no error	XXX
1:
	or.	%r3,%r6,%r6
	bnelr
	li	%r3,1
	blr
END(siglongjmp)
