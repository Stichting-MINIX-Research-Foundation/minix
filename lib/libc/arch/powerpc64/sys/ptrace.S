/*	$NetBSD: ptrace.S,v 1.3 2014/08/23 02:24:22 matt Exp $	*/

#include "SYS.h"

ENTRY(ptrace)
#ifdef _REENTRANT
	mflr	%r0
	streg	%r0,SZREG(%r1)
	stregu	%r1,-(SZREG*6)(%r1)
	streg	%r3,(SZREG*2)(%r1)
	streg	%r4,(SZREG*3)(%r1)
	streg	%r5,(SZREG*4)(%r1)
	streg	%r6,(SZREG*5)(%r1)

	bl	PIC_PLT(_C_LABEL(__errno))
	nop
	li	%r7,0
	stint	%r7,0(%r3)

	ldreg	%r3,(SZREG*2)(%r1)
	ldreg	%r4,(SZREG*3)(%r1)
	ldreg	%r5,(SZREG*4)(%r1)
	ldreg	%r6,(SZREG*5)(%r1)
	addi	%r1,%r1,SZREG*6
	ldreg	%r0,SZREG(%r1)
	mtlr	%r0
#else
	.pushsection ".toc","aw"
.Lerrno:
	.tc	errno[TC], errno
	.popsection
	addis	%r7,%r2,_C_LABEL(errno)@toc@ha
	ldptr	%r7,_C_LABEL(errno)@toc@l(%r7)
	li	%r8,0
	stint	%r8,0(%r7)
	mtlr	0
#endif /* _REENTRANT */
	_DOSYSCALL(ptrace)
	bso	1f
	blr
1:
	BRANCH_TO_CERROR()
END(ptrace)
