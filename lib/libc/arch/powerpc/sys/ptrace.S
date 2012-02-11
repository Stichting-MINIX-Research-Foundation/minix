/*	$NetBSD: ptrace.S,v 1.6 2011/01/16 02:43:10 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ptrace.S,v 1.6 2011/01/16 02:43:10 matt Exp $")
#endif /* LIBC_SCCS && !lint */

#define	XCALLFRAMELEN		((((2+5)*SZREG) + CALLFRAMELEN - 1) & -CALLFRAMELEN)
#define	XCALLFRAME_DATA		(XCALLFRAMELEN - 1*SZREG)
#define	XCALLFRAME_ADDR		(XCALLFRAMELEN - 2*SZREG)
#define	XCALLFRAME_PID		(XCALLFRAMELEN - 3*SZREG)
#define	XCALLFRAME_REQUEST	(XCALLFRAMELEN - 4*SZREG)
#define	XCALLFRAME_R30		(XCALLFRAMELEN - 5*SZREG)

ENTRY(ptrace)
#ifdef _REENTRANT
	mflr	%r0
	stw	%r0,CALLFRAME_LR(%r1)
	stwu	%r1,-XCALLFRAMELEN(%r1)
	stw	%r3,XCALLFRAME_REQUEST(%r1)
	stw	%r4,XCALLFRAME_PID(%r1)
	stw	%r5,XCALLFRAME_ADDR(%r1)
	stw	%r6,XCALLFRAME_DATA(%r1)
#ifdef PIC
	stw	%r30,XCALLFRAME_R30(%r1)
	PIC_TOCSETUP(ptrace, %r30)
#endif

	bl	PIC_PLT(_C_LABEL(__errno))
	li	%r7,0
	stw	%r7,0(%r3)

#ifdef PIC
	lwz	%r30,XCALLFRAME_R30(%r1)
#endif
	lwz	%r6,XCALLFRAME_DATA(%r1)
	lwz	%r4,XCALLFRAME_PID(%r1)
	lwz	%r5,XCALLFRAME_ADDR(%r1)
	lwz	%r3,XCALLFRAME_REQUEST(%r1)
	addi	%r1,%r1,XCALLFRAMELEN
	lwz	%r0,CALLFRAME_LR(%r1)
	mtlr	%r0
#else
#ifdef PIC
	mflr	%r0
	PIC_GOTSETUP(%r7)
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
	_DOSYSCALL(ptrace)
	bnslr
	b	_C_LABEL(__cerror)
