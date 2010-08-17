/*	ex64*() - extract low or high 32 bits of a 64 bit number */
/*							Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <machine/asm.h>

ENTRY(ex64lo)
/* unsigned long ex64lo(u64_t i); */
	movl	4(%esp), %eax
	ret

ENTRY(ex64hi)
/* unsigned long ex64hi(u64_t i); */
	movl	8(%esp), %eax
	ret

/* */
/* $PchId: ex64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
