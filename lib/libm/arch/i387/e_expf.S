/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"


RCSID("$NetBSD: e_expf.S,v 1.6 2008/06/24 17:27:56 drochner Exp $")

/* e^x = 2^(x * log2(e)) */
ENTRY(__ieee754_expf)
	XMM_ONE_ARG_FLOAT_PROLOGUE

	/*
	 * catch +/-Inf and NaN arguments
	 */
	movl	ARG_FLOAT_ONE,%eax
	andl	$0x7fffffff,%eax
	cmpl	$0x7f800000,%eax
	jae	x_Inf_or_NaN

	flds	ARG_FLOAT_ONE
	fldl2e
	fmulp				/* x * log2(e) */
	fld	%st(0)
	frndint				/* int(x * log2(e)) */
	fsubr	%st(0),%st(1)		/* fract(x * log2(e)) */
	fxch
	f2xm1				/* 2^(fract(x * log2(e))) - 1 */
	fld1
	faddp				/* 2^(fract(x * log2(e))) */
	fscale				/* e^x */
	fstp	%st(1)
	XMM_FLOAT_EPILOGUE
	ret

x_Inf_or_NaN:
	/*
	 * Return 0 if x is -Inf.  Otherwise just return x, although the
	 * C version would return (x + x) (Real Indefinite) if x is a NaN.
	 */
	movl	ARG_FLOAT_ONE,%eax
	cmpl	$0xff800000,%eax
	jne	x_not_minus_Inf
	fldz
	XMM_FLOAT_EPILOGUE
	ret

x_not_minus_Inf:
	flds	ARG_FLOAT_ONE
	XMM_FLOAT_EPILOGUE
	ret
