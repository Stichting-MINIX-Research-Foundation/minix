/*	$NetBSD: brk.S,v 1.12 2011/01/16 02:43:10 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: brk.S,v 1.12 2011/01/16 02:43:10 matt Exp $")
#endif /* LIBC_SCCS && !lint */

	.globl	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__minbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.data
_C_LABEL(__minbrk):
	.long	_C_LABEL(_end)		# XXX not used yet

	.text
ENTRY(_brk)
#if defined(PIC)
	mflr	%r10
	PIC_GOTSETUP(%r9)
	mtlr	%r10
	lwz	%r5,_C_LABEL(_end)@got(%r9)
#else
	lis	%r5,_C_LABEL(_end)@ha	# r5 = &_end
	addi	%r5,%r5,_C_LABEL(_end)@l
#endif
	cmplw	%r5,%r3			# if (&_end <= r3)
	bgt	0f
	mr	%r5,%r3			# r5 = r3
0:
	mr	%r3,%r5			# new break value
	_DOSYSCALL(break)		# assume, that r5 is kept
	bso	1f
#ifdef PIC
	lwz	%r6,_C_LABEL(__curbrk)@got(%r9)
	stw	%r5,0(%r6)
#else
	lis	%r6,_C_LABEL(__curbrk)@ha	# record new break
	stw	%r5,_C_LABEL(__curbrk)@l(%r6)
#endif
	blr				# return 0

1:
	b	_C_LABEL(__cerror)
END(_brk)
