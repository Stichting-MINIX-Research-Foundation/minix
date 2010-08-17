/*	cv64u() - 64 bit converted to unsigned		Author: Kees J. Bot */
/*								7 Dec 1995 */
#include <machine/asm.h>

ENTRY(cv64u)
/* unsigned cv64u(u64_t i); */
ENTRY(cv64ul)
/* unsigned long cv64ul(u64_t i); */
	movl	4(%esp), %eax
	cmpl	$0, 8(%esp)	/* return ULONG_MAX if really big */
	je	0f
	movl	$-1, %eax
0:
	ret

/* */
/* $PchId: cv64u.ack.s,v 1.2 1996/04/11 18:59:57 philip Exp $ */
