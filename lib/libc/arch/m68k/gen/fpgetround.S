/*	$NetBSD: fpgetround.S,v 1.8 2002/01/13 21:45:44 thorpej Exp $	*/

/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#include <machine/asm.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetround, _fpgetround)
ENTRY(_fpgetround)
#else
ENTRY(fpgetround)
#endif
	fmovel %fpcr,%d0
	bfextu %d0{#26:#2},%d0
	rts
