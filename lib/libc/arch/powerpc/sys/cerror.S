/*	$NetBSD: cerror.S,v 1.11 2013/09/12 15:36:15 joerg Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: cerror.S,v 1.11 2013/09/12 15:36:15 joerg Exp $")
#endif /* LIBC_SCCS && !lint */

#ifndef _REENTRANT
	.globl	_C_LABEL(errno)
#endif
#ifdef __PIC__
	.protected _C_LABEL(__cerror)
#endif

ENTRY(__cerror)
#ifdef _REENTRANT
	mflr	%r0
	stwu	%r1,-CALLFRAMELEN(%r1)		# allocate new stack frame
	stw	%r0,CALLFRAMELEN+CALLFRAME_LR(%r1)
#ifdef __PIC__
	stw	%r30,CALLFRAME_R30(%r1)
	PIC_TOCSETUP(__cerror, %r30)
#endif
	stw	%r31,CALLFRAME_R31(%r1)
	mr	%r31,%r3		# stash away in callee-saved register
	bl	PIC_PLT(_C_LABEL(__errno))
	stw	%r31,0(%r3)
	lwz	%r0,CALLFRAMELEN+CALLFRAME_LR(%r1)
	lwz	%r31,CALLFRAME_R31(%r1)
#ifdef __PIC__
	lwz	%r30,CALLFRAME_R30(%r1)
#endif
	mtlr	%r0
	addi	%r1,%r1,CALLFRAMELEN
#else
#ifdef __PIC__
	mflr	%r10
	PIC_GOTSETUP(%r4)
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
END(__cerror)
