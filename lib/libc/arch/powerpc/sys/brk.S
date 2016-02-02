/*	$NetBSD: brk.S,v 1.14 2014/08/23 02:24:22 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: brk.S,v 1.14 2014/08/23 02:24:22 matt Exp $")
#endif /* LIBC_SCCS && !lint */

	.hidden	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__curbrk)
	.hidden	_C_LABEL(__minbrk)
	.globl	_C_LABEL(__minbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.data
	.p2align 2
_C_LABEL(__minbrk):
	.long	_C_LABEL(_end)
_C_LABEL(__curbrk):
	.long	_C_LABEL(_end)

ENTRY(_brk)
#ifdef __PIC__
	mflr	%r0
	bcl	20,31,.LPIC0
.LPIC0:	mflr    %r9
	mtlr	%r0
	addis	%r9,%r9,(_C_LABEL(__minbrk)-.LPIC0)@ha 
	ldptru	%r5,(_C_LABEL(__minbrk)-.LPIC0)@l(%r9)	# r5 = &_end
#else
	lis	%r9,_C_LABEL(__minbrk)@ha
	ldptru	%r5,_C_LABEL(__minbrk)@l(%r9)	# r5 = &_end
#endif
	cmpptrl	%r5,%r3			# if (__minbrk <= r3)
#ifdef __PPC_ISEL__
	iselgt	%r3,%r5,%r3
#else
	bgt	0f
	mr	%r5,%r3			# r5 = r3
0:
	mr	%r3,%r5			# new break value
#endif
	_DOSYSCALL(break)		# assume that r5 is preserved
	bso	1f
	stptr	%r5,__SIZEOF_POINTER__(%r9)
	blr				# return 0

1:
	BRANCH_TO_CERROR()
END(_brk)
