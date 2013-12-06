/*	$NetBSD: cerror.S,v 1.5 2013/09/12 15:36:15 joerg Exp $	*/

#include <machine/asm.h>
#include "SYS.h"

#ifndef _REENTRANT
	.globl	_C_LABEL(errno)
#endif
#ifdef __PIC__
	.protected _C_LABEL(__cerror)
#endif

ENTRY(__cerror)
#ifdef _REENTRANT
	mflr	%r0
	stwu	%r1,-16(%r1)		# allocate new stack frame
	stw	%r0,20(%r1)
	stw	%r31,8(%r1)
	mr	%r31,%r3		# stash away in callee-saved register
	bl	PIC_PLT(_C_LABEL(__errno))
	stw	%r31,0(%r3)
	lwz	%r0,20(%r1)
	lwz	%r31,8(%r1)
	mtlr	%r0
	la	%r1,16(%r1)
#else
#ifdef __PIC__
	mflr	%r10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	%r4
	lwz	%r4,_C_LABEL(errno)@got(%r4)
	stw	%r3,0(%r4)
	mtlr	%r10
#else
	lis	%r4,_C_LABEL(errno)@ha
	stw	%r3,_C_LABEL(errno)@l(%r4)
#endif /* __PIC__ */
#endif /* _REENTRANT */
	li	%r3,-1
	li	%r4,-1
	blr
