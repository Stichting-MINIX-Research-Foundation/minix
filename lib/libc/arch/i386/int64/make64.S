/*	make64() - make a 64 bit number from two 32 bit halves */
/*							Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <minix/compiler.h>
#include <machine/asm.h>

ENTRY(make64)
/* u64_t make64(unsigned long lo, unsigned long hi); */
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movl	%edx, (%eax)
	movl	12(%esp), %edx
	movl	%edx, 4(%eax)
	ret BYTES_TO_POP_ON_STRUCT_RETURN

/* */
/* $PchId: make64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
