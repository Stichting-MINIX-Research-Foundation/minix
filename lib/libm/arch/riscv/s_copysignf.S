/* $NetBSD: s_copysignf.S,v 1.2 2015/03/27 23:35:40 matt Exp $ */

/* The contents of this file are in the public domain. */

#include <machine/asm.h>

ENTRY(copysignf)
	fsgnj.s	fa0, fa0, fa1
	ret
END(copysignf)
