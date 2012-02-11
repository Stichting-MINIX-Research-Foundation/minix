/*	$NetBSD: flt_rounds.S,v 1.9 2000/12/04 12:02:08 is Exp $	*/

/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#include <machine/asm.h>

	.text
	.even
#if 0
/* XXX This is, effectively, an exclusive-or with 1 operation. */
_map:
	.byte 1		/* round to nearest */
	.byte 0		/* round to zero */
	.byte 3		/* round to negative infinity */
	.byte 2		/* round to positive infinity */
#endif

ENTRY(__flt_rounds)
	fmovel %fpcr,%d0
	bfextu %d0{#26:#2},%d0
	eorib #1,%d0
	rts
