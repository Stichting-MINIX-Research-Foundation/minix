/*	$NetBSD: sbrk.S,v 1.3 2006/07/07 06:50:06 ross Exp $	*/

#include "SYS.h"

	.globl	_C_LABEL(__curbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif


	.data
_C_LABEL(__curbrk):
	.long	_C_LABEL(_end)
	.text

	.macro  LdAddr  r,sym
	.pushsection    ".toc","aw"
100:	.tc \sym[TC],\sym
	.popsection
	ld      \r,100b@toc(2)
	.endm

ENTRY(_sbrk)
	ldAddr	%r6,__curbrk
	add	%r3,%r3,%r6
	mr	%r7,%r3				# r7 = new break
	li	%r0,SYS_break
	sc					# break(new_break)
	bso	1f
	mr	%r3,%r6				# set return value
	LdAddr	%r6,__curbrk
	std	%r7,0(%r6)
	blr
1:
	b	PIC_PLT(_C_LABEL(__cerror))
