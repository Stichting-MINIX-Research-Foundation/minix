/*
 * Written by Rui Paulo <rpaulo@NetBSD.org>, based on e_log.S.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_log2.S,v 1.1 2005/07/21 20:58:21 rpaulo Exp $")

ENTRY(__ieee754_log2)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fld1
	fldl	ARG_DOUBLE_ONE
	fyl2x
	XMM_DOUBLE_EPILOGUE
	ret
