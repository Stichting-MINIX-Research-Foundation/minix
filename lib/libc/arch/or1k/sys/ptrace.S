/*	$NetBSD: ptrace.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ptrace.S,v 1.1 2014/09/03 19:34:26 matt Exp $")
#endif /* LIBC_SCCS && !lint */

#define	XCALLFRAME_LR		(-1*4)
#define	XCALLFRAME_DATA		(-2*4)
#define	XCALLFRAME_ADDR		(-3*4)
#define	XCALLFRAME_PID		(-4*4)
#define	XCALLFRAME_REQUEST	(-5*4)
#define	XCALLFRAME_GP		(-6*4)
#define	XCALLFRAME_SP		(-7*4)
#define	XCALLFRAMELEN		(-XCALLFRAME_SP)

ENTRY(ptrace)
#ifdef _REENTRANT
	l.sw	XCALLFRAME_LR(r1),lr
	l.sw	XCALLFRAME_REQUEST(r1),r3
	l.sw	XCALLFRAME_PID(r1),r4
	l.sw	XCALLFRAME_ADDR(r1),r5
	l.sw	XCALLFRAME_DATA(r1),r6

#ifdef __PIC__
	l.sw	XCALLFRAME_GP(r1),r16
	PIC_GOTSETUP(r16)
#endif

	l.addi	r1,r1,-XCALLFRAMELEN
	l.jal	PLT(_C_LABEL(__errno))
	l.nop
	l.sw	0(r11),r0
	l.addi	r1,r1,XCALLFRAMELEN

#ifdef __PIC__
	l.lwz	r16,XCALLFRAME_GP(r1)
#endif

	l.lwz	r6,XCALLFRAME_DATA(r1)
	l.lwz	r4,XCALLFRAME_PID(r1)
	l.lwz	r5,XCALLFRAME_ADDR(r1)
	l.lwz	r3,XCALLFRAME_REQUEST(r1)
	l.lwz	lr,XCALLFRAME_LR(r1)
#else
#ifdef __PIC__
	l.or	r8,r9,r0
	PIC_GOTSETUP(r7)
	lwz	r7,_C_LABEL(errno)@got(r7)
	l.or	r9,r8,r0
#else
	l.movhi	r7,hi(_C_LABEL(errno))
	l.ori	r7,r7,lo(_C_LABEL(errno))
#endif /* __PIC__ */
#endif /* _REENTRANT */
	l.sw	0(r7),r0
	_DOSYSCALL(ptrace)
	l.bf	_C_LABEL(__cerror)
	l.nop
	l.jr	lr
	l.nop
END(ptrace)
