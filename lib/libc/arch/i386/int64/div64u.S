/*	div64u() - 64 bit divided by unsigned giving unsigned long */
/*							Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <minix/compiler.h>
#include <machine/asm.h>

ENTRY(div64u)
/* unsigned long div64u(u64_t i, unsigned j); */
	xorl	%edx, %edx
	movl	8(%esp), %eax	/* i = (ih<<32) + il */
	divl	12(%esp)	/* ih = q * j + r */
	movl	4(%esp), %eax
	divl	12(%esp)	/* i / j = (q<<32) + ((r<<32) + il) / j */
	ret

ENTRY(div64u64)
/* u64_t div64u64(u64_t i, unsigned j); */
	xorl	%edx, %edx
	movl	12(%esp), %eax	/* i = (ih<<32) + il */
	divl	16(%esp)	/* ih = q * j + r */
	movl	4(%esp), %ecx	/* get pointer to result */
	movl	%eax, 4(%ecx)	/* store high-order result */
	movl	8(%esp), %eax
	divl	16(%esp)	/* i / j = (q<<32) + ((r<<32) + il) / j */
	movl	%eax, 0(%ecx)	/* store low result */
	movl	%ecx, %eax	/* return pointer to result struct */
	ret	BYTES_TO_POP_ON_STRUCT_RETURN

ENTRY(rem64u)
/* unsigned rem64u(u64_t i, unsigned j); */
	pop	%ecx
	call	_C_LABEL(div64u)
	movl	%edx, %eax
	jmp	*%ecx

/* */
/* $PchId: div64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
