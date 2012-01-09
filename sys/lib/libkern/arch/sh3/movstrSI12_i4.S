/*	$NetBSD: movstrSI12_i4.S,v 1.2 2009/01/07 22:15:18 uwe Exp $	*/

#include <machine/asm.h>


#ifdef __ELF__
	.hidden __movstrSI12_i4, __movmemSI12_i4
#endif


NENTRY(__movstrSI12_i4)
	mov.l	@r5, r0
	mov.l	@(4, r5), r1
	mov.l	@(8, r5), r2
	mov.l	r0, @r4
	mov.l	r1, @(4, r4)
	rts
	 mov.l	r2, @(8, r4)

/* gcc4 uses movmem, older versions use movstr */
STRONG_ALIAS(__movmemSI12_i4, __movstrSI12_i4)
