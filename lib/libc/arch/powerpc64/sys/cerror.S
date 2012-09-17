/*	$NetBSD: cerror.S,v 1.4 2012/02/27 15:47:11 matt Exp $	*/

#include <machine/asm.h>
#include "SYS.h"

#ifndef _REENTRANT
	.globl	_C_LABEL(errno)
#endif
#ifdef PIC
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
#ifdef PIC
	mflr	%r10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	%r4
	lwz	%r4,_C_LABEL(errno)@got(%r4)
	stw	%r3,0(%r4)
	mtlr	%r10
#else
	lis	%r4,_C_LABEL(errno)@ha
	stw	%r3,_C_LABEL(errno)@l(%r4)
#endif /* PIC */
#endif /* _REENTRANT */
	li	%r3,-1
	li	%r4,-1
	blr
