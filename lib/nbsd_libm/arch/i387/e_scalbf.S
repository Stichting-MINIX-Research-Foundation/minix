/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_scalbf.S,v 1.3 2003/07/26 19:25:00 salo Exp $")

ENTRY(__ieee754_scalbf)
	XMM_TWO_ARG_FLOAT_PROLOGUE
	flds	ARG_FLOAT_TWO
	flds	ARG_FLOAT_ONE
	fscale
	XMM_FLOAT_EPILOGUE
	ret
