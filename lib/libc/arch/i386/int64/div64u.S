/*	div64u() - 64 bit divided by unsigned giving unsigned long */
/*							Author: Kees J. Bot */
/*								7 Dec 1995 */
.text
.globl	_div64u, _rem64u

_div64u:
/* unsigned long div64u(u64_t i, unsigned j); */
	xorl	%edx, %edx
	movl	8(%esp), %eax	/* i = (ih<<32) + il */
	divl	12(%esp)	/* ih = q * j + r */
	movl	4(%esp), %eax
	divl	12(%esp)	/* i / j = (q<<32) + ((r<<32) + il) / j */
	ret

_rem64u:
/* unsigned rem64u(u64_t i, unsigned j); */
	pop	%ecx
	call	_div64u
	movl	%edx, %eax
	jmp	*%ecx

/* */
/* $PchId: div64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
