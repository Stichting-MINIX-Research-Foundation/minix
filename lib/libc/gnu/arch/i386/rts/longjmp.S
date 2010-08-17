/ longjmp.gnu.s
/
/ Created:	Oct 15, 1993 by Philip Homburg <philip@cs.vu.nl>
#include <machine/asm.h>

ENTRY(longjmp)
	movl	4(%esp), %eax		/* jmp_buf */
	cmpl	$0, 0(%eax)			/* save mask? */
	je		1f
	leal	4(%eax), %edx		/* pointer to sigset_t */
	push	%edx
	call	_C_LABEL(__oldsigset)	/* restore mask */
	addl	$4, %esp
	movl	4(%esp), %eax		/* jmp_buf */
1:
	movl	8(%esp), %ecx		/* result value */
	movl	12(%eax), %esp 		/* restore stack pointer */

	movl	8(%eax), %edx 		/* restore program counter */
	movl	%edx, 0(%esp)

	push	%ecx			/* save result code */

	movl	16(%eax), %ebp		/* restore frame pointer */
	movl	20(%eax), %ebx
	movl	24(%eax), %ecx
	movl	28(%eax), %edx
	movl	32(%eax), %esi
	movl	36(%eax), %edi
	pop	%eax
	testl	%eax, %eax
	jz	1f
	ret
1:	movl	$1, %eax
	ret

/ $PchId: longjmp.gnu.s,v 1.4 1996/03/12 19:30:02 philip Exp $
