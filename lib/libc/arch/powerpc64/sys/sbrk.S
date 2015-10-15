/*	$NetBSD: sbrk.S,v 1.5 2014/08/23 02:24:22 matt Exp $	*/

#include "SYS.h"

	.hidden	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__curbrk)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

ENTRY(_sbrk)
	addis	%r8,%r2,_C_LABEL(__curbrk)@toc@ha
	ldptru	%r6,_C_LABEL(__curbrk)@toc@l(%r8)
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
