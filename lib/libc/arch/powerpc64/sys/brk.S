/*	$NetBSD: brk.S,v 1.4 2011/10/29 16:03:33 christos Exp $	*/

#include "SYS.h"

	.globl	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__minbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.macro  LdAddr  r,sym
	.pushsection    ".toc","aw"
100:	.tc \sym[TC],\sym
	.popsection
	ld      \r,100b@toc(2)
	.endm

        .data
_C_LABEL(__minbrk):
	.long   _C_LABEL(_end)      
	.text

ENTRY(_brk)
	LdAddr	%r5,_end
	cmpld	%r5,%r3			# if (&_end <= r3)
	bgt	0f
	mr	%r5,%r3			# r5 = r3
0:
	mr	%r3,%r5			# new break value
	li	%r0,SYS_break
	sc				# assume, that r5 is kept
	bso	1f
	LdAddr	%r6,__curbrk
	std	%r5,0(%r6)
	blr				# return 0

1:
	b	PIC_PLT(_C_LABEL(__cerror))

