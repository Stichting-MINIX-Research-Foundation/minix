/*	$NetBSD: ptrace.S,v 1.1 2006/07/01 16:37:20 ross Exp $	*/

#include "SYS.h"

ENTRY(ptrace)
#ifdef _REENTRANT
	mflr	%r0
	stwu	%r1,-32(%r1)
	stw	%r0,36(%r1)
	stw	%r3,8(%r1)
	stw	%r4,12(%r1)
	stw	%r5,16(%r1)
	stw	%r6,20(%r1)

	bl	PIC_PLT(_C_LABEL(__errno))
	li	%r7,0
	stw	%r7,0(%r3)

	lwz	%r3,8(%r1)
	lwz	%r4,12(%r1)
	lwz	%r5,16(%r1)
	lwz	%r0,36(%r1)
	lwz	%r6,20(%r1)
	mtlr	%r0
	la	%r1,32(%r1)
#else
#ifdef PIC
	mflr	%r0
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	%r7
	lwz	%r7,_C_LABEL(errno)@got(%r7)
	li	%r8,0
	stw	%r8,0(%r7)
	mtlr	0
#else
	lis	%r7,_C_LABEL(errno)@ha
	li	%r8,0
	stw	%r8,_C_LABEL(errno)@l(%r7)
#endif /* PIC */
#endif /* _REENTRANT */
	li	%r0,SYS_ptrace
	sc
	bso	1f
	blr
1:
	b	PIC_PLT(_C_LABEL(__cerror))
