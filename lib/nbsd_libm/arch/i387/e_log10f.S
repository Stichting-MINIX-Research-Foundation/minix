/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_log10f.S,v 1.3 2003/07/26 19:24:59 salo Exp $")

ENTRY(__ieee754_log10f)
	XMM_ONE_ARG_FLOAT_PROLOGUE
	fldlg2
	flds	ARG_FLOAT_ONE
	fyl2x
	XMM_FLOAT_EPILOGUE
	ret
