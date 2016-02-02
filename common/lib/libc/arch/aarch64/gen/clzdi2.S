/* $NetBSD: clzdi2.S,v 1.1 2014/08/10 05:47:35 matt Exp $ */

#include <machine/asm.h>

RCSID("$NetBSD: clzdi2.S,v 1.1 2014/08/10 05:47:35 matt Exp $")

ENTRY(clzdi2)
	clz	x0, x0
	ret
END(clsdi2)
