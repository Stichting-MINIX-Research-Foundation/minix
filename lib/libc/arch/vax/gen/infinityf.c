/*	$NetBSD: infinityf.c,v 1.1 2003/10/25 22:31:20 kleink Exp $	*/

/*
 * infinityf.c - max. value representable in VAX F_floating  -- public domain.
 * This is _not_ infinity.
 */

#include <math.h>

const union __float_u __infinityf =
	{ { 0xff, 0x7f, 0xff, 0xff } };
