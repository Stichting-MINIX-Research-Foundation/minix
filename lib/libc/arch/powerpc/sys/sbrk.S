/*	$NetBSD: sbrk.S,v 1.11 2011/01/16 02:43:10 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sbrk.S,v 1.11 2011/01/16 02:43:10 matt Exp $")
#endif /* LIBC_SCCS && !lint */

	.globl	_C_LABEL(__curbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

	.data
_C_LABEL(__curbrk):
	.long	_C_LABEL(_end)
	.text

ENTRY(_sbrk)
#ifdef PIC
	mflr	%r10
	PIC_GOTSETUP(%r5)
	mtlr	%r10
	lwz	%r5,_C_LABEL(__curbrk)@got(%r5)
	lwz	%r6,0(%r5)
#else
	lis	%r5,_C_LABEL(__curbrk)@ha
	lwzu	%r6,_C_LABEL(__curbrk)@l(%r5)	# r6 = old break, r5 = &curbrk
#endif
	add	%r3,%r3,%r6
	mr	%r7,%r3				# r7 = new break
	_DOSYSCALL(break)			# break(new_break)
	bso	1f
	mr	%r3,%r6				# set return value
	stw	%r7,0(%r5)			# record new break
	blr
1:
	b	_C_LABEL(__cerror)
END(_sbrk)
