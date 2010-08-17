/*	bcmp()						Author: Kees J. Bot */
/*								2 Jan 1994 */

/* int bcmp(const void *s1, const void *s2, size_t n) */
/*	Compare two chunks of memory. */
/*	This is a BSD routine that escaped from the kernel.  Don't use. */
/*	(Alas it is not without some use, it reports the number of bytes */
/*	after the bytes that are equal.  So it can't be simply replaced.) */
/* */
#include <machine/asm.h>

ENTRY(bcmp)
	push	%ebp
	movl	%esp, %ebp
	push	16(%ebp)
	push	12(%ebp)
	push	8(%ebp)
	call	_C_LABEL(memcmp) /* Let memcmp do the work */
	testl	%eax, %eax
	je	equal
	subl	8(%ebp), %edx	/* Memcmp was nice enough to leave "esi" in edx */
	decl	%edx	/* Number of bytes that are equal */
	movl	16(%ebp), %eax
	subl	%edx, %eax	/* Number of bytes that are unequal */
equal:
	leave
	ret
