/*	$NetBSD: cerror.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: cerror.S,v 1.1 2014/09/03 19:34:26 matt Exp $")
#endif /* LIBC_SCCS && !lint */

#ifndef _REENTRANT
	.globl	_C_LABEL(errno)
#endif
#ifdef __PIC__
	.protected _C_LABEL(__cerror)
#endif

ENTRY(__cerror)
#ifdef _REENTRANT
	l.sw	-4(r1),r3
	l.sw	-8(r1),lr
	l.sw	-16(r1),r1
	l.addi	r1,r1,-16
#ifdef __PIC__
	l.sw	-12(r1),r16
	PIC_GOTSETUP(r16)
#endif
	l.jal	PLT(_C_LABEL(__errno))	# needs got in r16
	l.nop
	l.addi	r1,r1,16
#ifdef __PIC__
	l.lwz	r16,-12(r1)
#endif
	l.lwz	lr,-8(r1)
	l.lwz	r3,-4(r1)
#else
#ifdef __PIC__
	l.or	r8,r9,r0
	PIC_GOTSETUP(r4)
	l.lwz	r11,got(_C_LABEL(errno))(r4)
	l.or	r9,r8,r0
#else
	l.movi	r11,hi(_C_LABEL(errno))
	l.ori	r11,lo(_C_LABEL(errno))
#endif /* __PIC__ */
#endif /* _REENTRANT */
	l.sw	0(r11),r3
	l.addi	r11,r0,-1
	l.addi	r12,r0,-1
	l.jr	lr
	l.nop
END(__cerror)
