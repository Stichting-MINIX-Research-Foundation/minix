/*	$NetBSD: flt_rounds_softfloat.S,v 1.6 2014/03/18 18:20:37 riastradh Exp $	*/

/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.

 * Broken by Bruce O'Neel Aug 4 2003
	gcc no longer seems to export fpCCR so this fails.
	for now just return 0.
 */

#include <machine/asm.h>

	.text
#if 0
	/* NB: this is tied to the gcc-2.95 lb1sf68.asm: */
_map:
	.byte 1		/* round to nearest */
	.byte 0		/* round to zero */
	.byte 2		/* round to positive infinity */
	.byte 3		/* round to negative infinity */
#endif

ENTRY(__flt_rounds)
	/* lea	_C_LABEL(_fpCCR),%a0	| check the rounding mode */
	/* movew	6(%a0),%d0	| rounding mode in d0 */
	/* lea	_map,%a0 */
	moveb	#0,%d0
	rts
END(__flt_rounds)
