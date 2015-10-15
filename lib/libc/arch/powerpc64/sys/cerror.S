/*	$NetBSD: cerror.S,v 1.7 2015/01/12 02:48:20 dennis Exp $	*/

/* Now inlined in the syscall stubs.  Keep code for a while for mind changes. */

#if 0
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
	streg	%r0,SF_LR(%r1)
	streg	%r31,-8(%r1)
	stptru	%r1,-(SF_SZ+16)(%r1)	# allocate new stack frame
	mr	%r31,%r3		# stash away in callee-saved register
	bl	PIC_PLT(_C_LABEL(__errno))
	nop
	stint	%r31,0(%r3)

	addi	%r1,%r1,(SF_SZ+16)
	ldreg	%r31,-8(%r1)
	ldreg	%r0,SF_LR(%r1)
	mtlr	%r0
#else
	lwz	%r4,_C_LABEL(errno)@got(%r2)
	stw	%r3,0(%r4)
#endif /* _REENTRANT */
	li	%r3,-1
	li	%r4,-1
	blr
END(__cerror)
#endif	/* 0 */
