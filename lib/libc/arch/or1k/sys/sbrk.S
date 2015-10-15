/*	$NetBSD: sbrk.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sbrk.S,v 1.1 2014/09/03 19:34:26 matt Exp $")
#endif /* LIBC_SCCS && !lint */

	.hidden	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__curbrk)

#ifdef WEAK_ALIAS
WEAK_ALIAS(sbrk, _sbrk)
#endif

ENTRY(_sbrk)
#ifdef __PIC__
	l.or	r8,r9,r0
	PIC_GOTSETUP(r6)
	l.or	r9,r8,r0
	l.movhi	r5,gotoffhi(_C_LABEL(__curbrk))
	l.ori	r5,r5,gotofflo(_C_LABEL(__curbrk))
	l.add	r5,r5,r6
#else
	l.movhi	r5,hi(_C_LABEL(__curbrk))
	l.ori	r5,r5,lo(_C_LABEL(__curbrk))	# r5 = old break;
#endif
	l.lwz	r6,0(r5)			# r6 = old break, r5 = &curbrk
	l.add	r3,r3,r6
	_DOSYSCALL(break)			# break(new_break)
	l.bf	_C_LABEL(__cerror)
	l.nop
	l.sw	0(r5),r3			# record new break
	l.or	r11,r6,r0			# set return value
	l.jr	lr
	l.nop
END(_sbrk)
