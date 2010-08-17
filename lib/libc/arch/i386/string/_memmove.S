/*	_memmove()					Author: Kees J. Bot */
/*								2 Jan 1994 */

/* void *_memmove(void *s1, const void *s2, size_t n) */
/*	Copy a chunk of memory.  Handle overlap. */
/* */
#include <machine/asm.h>

ENTRY(_memmove)
	push	%ebp
	movl	%esp, %ebp
	push	%esi
	push	%edi
	movl	8(%ebp), %edi	/* String s1 */
	movl	12(%ebp), %esi	/* String s2 */
	movl	16(%ebp), %ecx	/* Length */
	movl	%edi, %eax
	subl	%esi, %eax
	cmpl	%ecx, %eax
	jb	downwards	/* if (s2 - s1) < n then copy downwards */
LABEL(_memcpy)
	cld	/* Clear direction bit: upwards */
	cmpl	$16, %ecx
	jb	upbyte	/* Don't bother being smart with short arrays */
	movl	%esi, %eax
	orl	%edi, %eax
	testb	$1, %al
	jne	upbyte	/* Bit 0 set, use byte copy */
	testb	$2, %al
	jne	upword	/* Bit 1 set, use word copy */
uplword:
	shrdl	$2, %ecx, %eax	/* Save low 2 bits of ecx in eax */
	shrl	$2, %ecx

	rep movsl	/* Copy longwords. */
	shldl	$2, %eax, %ecx	/* Restore excess count */
upword:
	shrl	$1, %ecx

	rep movsw	/* Copy words */
	adcl	%ecx, %ecx	/* One more byte? */
upbyte:
	rep movsb	/* Copy bytes */
done:
	movl	8(%ebp), %eax	/* Absolutely noone cares about this value */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret

/* Handle bad overlap by copying downwards, don't bother to do word copies. */
downwards:
	std	/* Set direction bit: downwards */
	leal	-1(%esi,%ecx,1), %esi
	leal	-1(%edi,%ecx,1), %edi

	rep movsb	/* Copy bytes */
	cld
	jmp	done
