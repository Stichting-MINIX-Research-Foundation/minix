/ __setjmp.gnu.s
/
/ Created:	Oct 14, 1993 by Philip Homburg <philip@cs.vu.nl>
#include <machine/asm.h>

ENTRY(__setjmp)
	movl	4(%esp), %eax		/* jmp_buf */
	movl	%edx, 28(%eax)		/* save edx */
	movl	0(%esp), %edx
	movl	%edx, 8(%eax)		/* save program counter */
	movl	%esp, 12(%eax)		/* save stack pointer */
	movl	%ebp, 16(%eax)		/* save frame pointer */
	movl	%ebx, 20(%eax)
	movl	%ecx, 24(%eax)
	movl	%esi, 32(%eax)
	movl	%edi, 36(%eax)
	
	movl	8(%esp), %edx		/* save mask? */
	movl	%edx, 0(%eax)		/* save whether to restore mask */
	testl	%edx, %edx
	jz		1f
	leal	4(%eax), %edx		/* pointer to sigset_t */
	push	%edx
	call	_C_LABEL(__newsigset)	/* save mask	 */
	addl	$4, %esp
1:
	movl	$0, %eax
	ret

/ $PchId: __setjmp.gnu.s,v 1.4 1996/03/12 19:30:54 philip Exp $
