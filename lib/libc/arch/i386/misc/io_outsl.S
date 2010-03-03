/*	outsl() - Output a dword array		Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outsl(U16_t port, void *buf, size_t count); */

.text
.globl	_outsl
_outsl:
	push	%ebp
	movl	%esp, %ebp
	cld
	push	%esi
	movl	8(%ebp), %edx	/* port */
	movl	12(%ebp), %esi	/* buf */
	movl	16(%ebp), %ecx	/* byte count */
	shrl	$2, %ecx	/* dword count */
	rep outsl	/* output many dwords */
	pop	%esi
	pop	%ebp
	ret
