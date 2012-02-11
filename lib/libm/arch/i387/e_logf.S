/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_logf.S,v 1.4 2003/07/26 19:24:59 salo Exp $")

ENTRY(__ieee754_logf)
	XMM_ONE_ARG_FLOAT_PROLOGUE
	fldln2
	flds	ARG_FLOAT_ONE
	fyl2x
	XMM_FLOAT_EPILOGUE
	ret
