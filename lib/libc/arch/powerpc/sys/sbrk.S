/*	$NetBSD: sbrk.S,v 1.13 2014/08/23 02:24:22 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sbrk.S,v 1.13 2014/08/23 02:24:22 matt Exp $")
#endif /* LIBC_SCCS && !lint */

	.hidden	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__curbrk)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

ENTRY(_sbrk)
#ifdef __PIC__
	mflr	%r0
	bcl	20,31,.LPIC0
.LPIC0:	mflr	%r5
	mtlr	%r0
	addis	%r8,%r5,(_C_LABEL(__curbrk)-.LPIC0)@ha
	ldptru	%r6,(_C_LABEL(__curbrk)-.LPIC0)@l(%r8)
#else
	lis	%r8,_C_LABEL(__curbrk)@ha
	ldptru	%r6,_C_LABEL(__curbrk)@l(%r8)	# r6 = old break, r5 = &curbrk
#endif
	add	%r3,%r3,%r6
	mr	%r7,%r3				# r7 = new break
	_DOSYSCALL(break)			# break(new_break)
	bso	1f
	mr	%r3,%r6				# set return value
	stptr	%r7,0(%r8)			# record new break
	blr
1:
	BRANCH_TO_CERROR()
END(_sbrk)
