/*	mul64u() - unsigned long by unsigned multiply giving 64 bit result */
/*							Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <minix/compiler.h>
#include <machine/asm.h>

ENTRY(mul64u)
/* u64_t mul64u(unsigned long i, unsigned j); */
	movl	4(%esp), %ecx
	movl	8(%esp), %eax
	mull	12(%esp)
	movl	%eax, (%ecx)
	movl	%edx, 4(%ecx)
	movl	%ecx, %eax
	ret BYTES_TO_POP_ON_STRUCT_RETURN

/* */
/* $PchId: mul64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
