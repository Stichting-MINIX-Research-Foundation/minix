/*	outsb() - Output a byte array		Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outsb(U16_t port, void *buf, size_t count); */

.text
.globl	_outsb
_outsb:
	push	%ebp
	movl	%esp, %ebp
	cld
	push	%esi
	movl	8(%ebp), %edx	/* port */
	movl	12(%ebp), %esi	/* buf */
	movl	16(%ebp), %ecx	/* byte count */
	rep outsb	/* output many bytes */
	pop	%esi
	pop	%ebp
	ret
