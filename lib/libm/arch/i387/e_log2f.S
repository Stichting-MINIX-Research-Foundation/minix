/*
 * Written by Rui Paulo <rpaulo@NetBSD.org>, based on e_logf.S.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_log2f.S,v 1.1 2005/07/21 20:58:21 rpaulo Exp $")

ENTRY(__ieee754_log2f)
	XMM_ONE_ARG_FLOAT_PROLOGUE
	fld1
	flds	ARG_FLOAT_ONE
	fyl2x
	XMM_FLOAT_EPILOGUE
	ret
