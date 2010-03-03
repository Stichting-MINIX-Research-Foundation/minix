/*	insw() - Input a word array			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void insw(U16_t port, void *buf, size_t count); */

.text
.globl	_insw
_insw:
	push	%ebp
	movl	%esp, %ebp
	cld
	push	%edi
	movl	8(%ebp), %edx	/* port */
	movl	12(%ebp), %edi	/* buf */
	movl	16(%ebp), %ecx	/* byte count */
	shrl	$1, %ecx	/* word count */
	rep insw	/* input many words */
	pop	%edi
	pop	%ebp
	ret
