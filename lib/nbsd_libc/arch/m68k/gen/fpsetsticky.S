/*	$NetBSD: fpsetsticky.S,v 1.7 2002/01/13 21:45:45 thorpej Exp $	*/

/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpsetsticky, _fpsetsticky)
ENTRY(_fpsetsticky)
#else
ENTRY(fpsetsticky)
#endif
	movel %d2,%sp@-
	fmovel %fpsr,%d1
	movel %sp@(8),%d2
	bfextu %d1{#24:#5},%d0
	bfins %d2,%d1{#24:#5}
	movel %sp@+,%d2
	fmovel %d1,%fpsr
	rts
