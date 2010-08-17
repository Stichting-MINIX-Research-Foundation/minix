/*	memcmp()					Author: Kees J. Bot */
/*								2 Jan 1994 */

/* int memcmp(const void *s1, const void *s2, size_t n) */
/*	Compare two chunks of memory. */
/* */
#include <machine/asm.h>

ENTRY(memcmp)
	cld
	push	%ebp
	movl	%esp, %ebp
	push	%esi
	push	%edi
	movl	8(%ebp), %esi	/* String s1 */
	movl	12(%ebp), %edi	/* String s2 */
	movl	16(%ebp), %ecx	/* Length */
	cmpl	$16, %ecx
	jb	cbyte	/* Don't bother being smart with short arrays */
	movl	%esi, %eax
	orl	%edi, %eax
	testb	$1, %al
	jne	cbyte	/* Bit 0 set, use byte compare */
	testb	$2, %al
	jne	cword	/* Bit 1 set, use word compare */
clword:
	shrdl	$2, %ecx, %eax	/* Save low two bits of ecx in eax */
	shrl	$2, %ecx

	repe cmpsl	/* Compare longwords */
	subl	$4, %esi
	subl	$4, %edi
	incl	%ecx	/* Recompare the last longword */
	shldl	$2, %eax, %ecx	/* And any excess bytes */
	jmp	last
cword:
	shrdl	$1, %ecx, %eax	/* Save low bit of ecx in eax */
	shrl	$1, %ecx

	repe cmpsw	/* Compare words */
	subl	$2, %esi
	subl	$2, %edi
	incl	%ecx	/* Recompare the last word */
	shldl	$1, %eax, %ecx	/* And one more byte? */
cbyte:
	testl	%ecx, %ecx	/* Set 'Z' flag if ecx = 0 */
last:
	repe cmpsb	/* Look for the first differing byte */
	seta	%al	/* al = (s1 > s2) */
	setb	%ah	/* ah = (s1 < s2) */
	subb	%ah, %al
	movsbl	%al, %eax	/* eax = (s1 > s2) - (s1 < s2), i.e. -1, 0, 1 */
	movl	%esi, %edx	/* For bcmp() to play with */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret
