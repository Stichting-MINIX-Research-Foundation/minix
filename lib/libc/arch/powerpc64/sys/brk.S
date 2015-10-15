/*	$NetBSD: brk.S,v 1.5 2014/08/23 02:24:22 matt Exp $	*/

#include "SYS.h"


	.hidden	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__curbrk)
	.hidden	_C_LABEL(__minbrk)
	.globl	_C_LABEL(__minbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.data
	.p2align 3
_C_LABEL(__minbrk):
	.quad	_C_LABEL(_end)
_C_LABEL(__curbrk):
	.quad	_C_LABEL(_end)

ENTRY(_brk)
	addis	%r9,%r2,_C_LABEL(__minbrk)@toc@ha
	ldptru	%r5,_C_LABEL(__minbrk)@toc@l(%r9)
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
