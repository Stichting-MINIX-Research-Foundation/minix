/*	cvu64() - unsigned converted to 64 bit		Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <minix/compiler.h>
#include <machine/asm.h>

ENTRY(cvu64)
/* u64_t cvu64(unsigned i); */
ENTRY(cvul64)
/* u64_t cvul64(unsigned long i); */
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movl	%edx, (%eax)
	movl	$0, 4(%eax)
	ret BYTES_TO_POP_ON_STRUCT_RETURN

/* */
/* $PchId: cvu64.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
