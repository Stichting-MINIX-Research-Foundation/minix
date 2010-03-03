/*	outsw() - Output a word array		Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outsw(U16_t port, void *buf, size_t count); */

.text
.globl	_outsw
_outsw:
	push	%ebp
	movl	%esp, %ebp
	cld
	push	%esi
	movl	8(%ebp), %edx	/* port */
	movl	12(%ebp), %esi	/* buf */
	movl	16(%ebp), %ecx	/* byte count */
	shrl	$1, %ecx	/* word count */
	rep outsw	/* output many words */
	pop	%esi
	pop	%ebp
	ret
