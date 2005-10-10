/ _setjmp.gnu.s
/
/ Created:	Oct 14, 1993 by Philip Homburg <philip@cs.vu.nl>

.text
.globl ___setjmp
___setjmp:
	movl	4(%esp), %eax		# jmp_buf
	movl	%ebx, 20(%eax)		# save ebx
	movl	0(%esp), %ebx
	movl	%ebx, 8(%eax)		# save program counter
	movl	%esp, 12(%eax)		# save stack pointer
	movl	%ebp, 16(%eax)		# save frame pointer
	movl	20(%eax), %ebx 		# restore ebx
	movl	%ecx, 24(%eax)
	movl	%edx, 28(%eax)
	movl	%esi, 32(%eax)
	movl	%edi, 36(%eax)
	
	movl	8(%esp), %ebx		# save mask?
	movl	%ebx, 0(%eax)		# save whether to restore mask
	testl	%ebx, %ebx
	jz		1f
	leal	4(%eax), %ebx		# pointer to sigset_t
	pushl	%ebx
	call	___newsigset		# save mask	
	addl	$4, %esp
1:
	movl	$0, %eax
	ret

/ $PchId: _setjmp.gnu.s,v 1.4 1996/03/12 19:30:54 philip Exp $
