/*	$NetBSD: brk.S,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: brk.S,v 1.1 2014/09/03 19:34:26 matt Exp $")
#endif /* LIBC_SCCS && !lint */

	.hidden	_C_LABEL(__curbrk)
	.hidden	_C_LABEL(__minbrk)
	.globl	_C_LABEL(__curbrk)
	.globl	_C_LABEL(__minbrk)
	.globl	_C_LABEL(_end)

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif
	.data
	.p2align 2
_C_LABEL(__minbrk):
	.long	_C_LABEL(_end)
_C_LABEL(__curbrk):
	.long	_C_LABEL(_end)

	.text
ENTRY(_brk)
#ifdef __PIC__
	l.or	r8,r9,r0
	PIC_GOTSETUP(r7)
	l.or	r9,r8,r0
	l.movhi	r6,gotoffhi(_C_LABEL(__minbrk))
	l.ori	r6,r6,gotofflo(_C_LABEL(__minbrk))
	l.add	r6,r6,r7
#else
	l.movhi	r6,hi(_C_LABEL(__minbrk))
	l.ori	r6,r6,lo(_C_LABEL(__minbrk))
#endif
	l.lwz	r5,0(r6)		# r5 = __minbrk
	l.sfgts	r5,r3			# if (__minbrk <= r3)
	l.cmov	r5,r5,r3		# choose larger
	l.or	r3,r5,r0		# new break value
	_DOSYSCALL(break)		# assume, that r5 is kept
	l.bf	_C_LABEL(__cerror)
	l.nop
	l.sw	__SIZEOF_POINTER__(r6),r5
	l.xor	r11, r11, r11		# return 0
	l.jr	lr			# return 0
	l.nop
END(_brk)
