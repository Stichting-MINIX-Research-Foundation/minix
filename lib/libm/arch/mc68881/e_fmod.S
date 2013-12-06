/*	$NetBSD: e_fmod.S,v 1.4 2003/07/26 19:25:04 salo Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: e_fmod.S,v 1.4 2003/07/26 19:25:04 salo Exp $")

ENTRY(__ieee754_fmod)
	fmoved	%sp@(4),%fp0
	fmodd	%sp@(12),%fp0
#ifndef __SVR4_ABI__
	fmoved	%fp0,%sp@-
	movel	%sp@+,%d0
	movel	%sp@+,%d1
#endif
	rts
