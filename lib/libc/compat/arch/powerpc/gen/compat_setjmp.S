/*	$NetBSD: compat_setjmp.S,v 1.2 2011/01/15 07:31:13 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: compat_setjmp.S,v 1.2 2011/01/15 07:31:13 matt Exp $")
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

ENTRY(setjmp)
	mr	%r6,%r3
	li	%r3,SIG_BLOCK
	li	%r4,0
	_DOSYSCALL(compat_13_sigprocmask13) # assume no error	XXX
	mflr	%r11
	mfcr	%r12
	mr	%r10,%r1
	mr	%r9,%r2
	mr	%r8,%r3
	stmw	%r8,4(%r6)
	li	%r3,0
	blr
END(setjmp)

ENTRY(longjmp)
	lmw	%r8,4(%r3)
	mr	%r6,%r4
	mtlr	%r11
	mtcr	%r12
	mr	%r2,%r9
	mr	%r1,%r10
	mr	%r4,%r8
	li	%r3,SIG_SETMASK
	_DOSYSCALL(compat_13_sigprocmask13) # assume no error	XXX
	or.	%r3,%r6,%r6
	bnelr
	li	%r3,1
	blr
END(longjmp)
