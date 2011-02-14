/*	$NetBSD: fpgetsticky.S,v 1.7 2002/01/13 21:45:44 thorpej Exp $	*/

/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetsticky, _fpgetsticky)
ENTRY(_fpgetsticky)
#else
ENTRY(fpgetsticky)
#endif
	fmovel %fpsr,%d0
	bfextu %d0{#24:#5},%d0
	rts
