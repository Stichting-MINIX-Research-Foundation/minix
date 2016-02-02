/* $NetBSD: e_sqrt.S,v 1.2 2015/03/27 23:35:40 matt Exp $ */

/* The contents of this file are in the public domain. */

#include <machine/asm.h>

ENTRY(__ieee754_sqrt)
	fsqrt.d	fa0, fa0
	ret
END(__ieee754_sqrt)
