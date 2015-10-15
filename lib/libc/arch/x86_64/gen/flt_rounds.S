/*	$NetBSD: flt_rounds.S,v 1.7 2014/05/22 15:01:56 uebayasi Exp $	*/

#include <machine/asm.h>

/*
 * 00 0 round to zero 
 * 01 1 round to nearest
 * 10 2 round to positive infinity
 * 11 3 round to negative infinity
 */
	.text
	_ALIGN_TEXT
ENTRY(__flt_rounds)
	fnstcw	-4(%rsp)
	movl	-4(%rsp), %ecx
	shrl	$9, %ecx
	andl	$6, %ecx
	movl	$0x2d, %eax	/* 0x2d = 00.10.11.01 */
	sarl	%cl, %eax	/* 0,1,2,3 -> 1,3,2,0 */
	andl	$3, %eax
	ret
END(__flt_rounds)
