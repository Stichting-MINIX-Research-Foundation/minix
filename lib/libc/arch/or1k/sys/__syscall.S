/*	$NetBSD: __syscall.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

#include <machine/asm.h>

ENTRY(__syscall)
	l.or	r13,r3,r0		/* syscall number */
	l.or	r3,r4,r0
	l.or	r4,r5,r0
	l.or	r5,r6,r0
	l.or	r6,r7,r0
	l.or	r7,r8,r0
	// at most 5 arguments can be passed via registers so the others
	// must be on the stack.
	l.lwz	r8,0(r1)
	l.lwz	r11,4(r1)		/* use return value regs for last 2 */
	l.lwz	r12,8(r1)		/* use return value regs for last 2 */
	l.sys	0
	l.bf	_C_LABEL(__cerror)
	l.nop
	l.jr	lr
	l.nop
END(__syscall)

STRONG_ALIAS(syscall, __syscall)
