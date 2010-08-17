/*	diff64() - 64 bit subtraction giving unsigned 	Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <machine/asm.h>

ENTRY(diff64)
/* unsigned diff64(u64_t i, u64_t j); */
	movl	4(%esp), %eax
	subl	12(%esp), %eax
	ret

/* */
/* $PchId: diff64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
