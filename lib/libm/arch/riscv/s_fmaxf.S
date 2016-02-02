/* $NetBSD: s_fmaxf.S,v 1.2 2015/03/27 23:35:40 matt Exp $ */

/* The contents of this file are in the public domain. */

#include <machine/asm.h>

ENTRY(fmaxf)
	fmax.s	fa0, fa0, fa1
	ret
END(fmaxf)
