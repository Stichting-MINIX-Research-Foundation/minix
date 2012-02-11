/*	oneC_sum() - One complement`s checksum		Author: Kees J. Bot */
/*								9 May 1995 */
/* See RFC 1071, "Computing the Internet checksum" */
/* See also the C version of this code. */
#include <machine/asm.h>

ENTRY(oneC_sum)
	push	%ebp
	movl	%esp, %ebp
	push	%esi
	push	%edi
	movzwl	8(%ebp), %eax	/* Checksum of previous block */
	movl	12(%ebp), %esi	/* Data to compute checksum over */
	movl	16(%ebp), %edi	/* Number of bytes */

	xorl	%edx, %edx
	xorb	%cl, %cl
align:
	testl	$3, %esi	/* Is the data aligned? */
	je	aligned
	testl	%edi, %edi
	je	0f
	movb	(%esi), %dl	/* Rotate the first unaligned bytes */
	decl	%edi	/* into the edx register */
0:
	incl	%esi
	rorl	$8, %edx
	rorl	$8, %eax	/* Rotate the checksum likewise */
	addb	$8, %cl	/* Number of bits rotated */
	jmp	align
aligned:
	addl	%edx, %eax	/* Summate the unaligned bytes */
	adcl	$0, %eax	/* Add carry back in for one`s complement */

	jmp	add6test
_ALIGN_TEXT
add6:
	addl	(%esi), %eax	/* Six times unrolled loop, see below */
	adcl	4(%esi), %eax
	adcl	8(%esi), %eax
	adcl	12(%esi), %eax
	adcl	16(%esi), %eax
	adcl	20(%esi), %eax
	adcl	$0, %eax
	addl	$24, %esi
add6test:
	subl	$24, %edi
	jae	add6
	addl	$24, %edi

	jmp	add1test
_ALIGN_TEXT
add1:
	addl	(%esi), %eax	/* while ((edi -= 4) >= 0) */
	adcl	$0, %eax	/*	eax += *esi++; */
	addl	$4, %esi	/* edi += 4; */
add1test:
	subl	$4, %edi
	jae	add1
	addl	$4, %edi

	je	done	/* Are there extra bytes? */
	movl	(%esi), %edx	/* Load extra bytes in a full dword */
	andl	mask-4(,%edi,4), %edx	/* Mask off excess */
	addl	%edx, %eax	/* Add in the last bits */
	adcl	$0, %eax
done:
	roll	%cl, %eax	/* Undo the rotation at the beginning */
	movl	%eax, %edx
	shrl	$16, %eax
	addw	%dx, %ax	/* Add the two words in eax to form */
	adcw	$0, %ax	/* a 16 bit sum */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret

#ifdef __ACK__
.rom
#else
.data
#endif
.balign 4
mask:
.long	0x000000FF, 0x0000FFFF, 0x00FFFFFF

/* */
/* $PchId: oneC_sum.ack.s,v 1.2 1996/03/12 19:33:51 philip Exp $ */
