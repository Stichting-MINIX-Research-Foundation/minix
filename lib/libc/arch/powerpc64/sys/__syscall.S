/*	$NetBSD: __syscall.S,v 1.2 2014/08/23 02:24:22 matt Exp $	*/

#include "SYS.h"

	.text
	.p2align 2
ENTRY(__syscall)
	mr	%r0,%r3		/* syscall number */
	mr	%r3,%r4
	mr	%r4,%r5
	mr	%r5,%r6
	mr	%r6,%r7
	mr	%r7,%r8
	mr	%r8,%r9
	mr	%r9,%r10
	ldreg	%r10,(2*__SIZEOF_POINTER__)(%r1) /* final argument is on the stack */
	sc
	bnslr
	BRANCH_TO_CERROR()
END(__syscall)

STRONG_ALIAS(syscall, __syscall)
