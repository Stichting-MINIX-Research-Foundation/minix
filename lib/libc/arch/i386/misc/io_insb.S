/*	insb() - Input a byte array			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void insb(U16_t port, void *buf, size_t count); */

.text
.globl	_insb
_insb:
	push	%ebp
	movl	%esp, %ebp
	cld
	push	%edi
	movl	8(%ebp), %edx	/* port */
	movl	12(%ebp), %edi	/* buf */
	movl	16(%ebp), %ecx	/* byte count */
	rep insb	/* input many bytes */
	pop	%edi
	pop	%ebp
	ret
