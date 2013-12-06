/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_logbl.S,v 1.1 2011/08/03 14:13:07 joerg Exp $")

ENTRY(logbl)
	fldt	ARG_LONG_DOUBLE_ONE
	fxtract
	fstp	%st
	ret
