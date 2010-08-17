/*	add64u() - unsigned to 64 bit addition		Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <minix/compiler.h>
#include <machine/asm.h>

ENTRY(add64u)
/* u64_t add64u(u64_t i, unsigned j); */
ENTRY(add64ul)
/* u64_t add64ul(u64_t i, unsigned long j); */
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	addl	16(%esp), %edx
	movl	%edx, (%eax)
	movl	12(%esp), %edx
	adcl	$0, %edx
	movl	%edx, 4(%eax)
	ret BYTES_TO_POP_ON_STRUCT_RETURN

/* */
/* $PchId: add64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
