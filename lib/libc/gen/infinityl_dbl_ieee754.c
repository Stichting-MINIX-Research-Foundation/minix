/*	$NetBSD: infinityl_dbl_ieee754.c,v 1.1 2003/10/25 22:31:20 kleink Exp $	*/

/*
 * IEEE-compatible infinityl.c -- public domain.
 * For platforms where long double == double.
 */

#include <float.h>
#include <math.h>
#include <machine/endian.h>

#if LDBL_MANT_DIG != DBL_MANT_DIG
#error double / long double mismatch
#endif

const union __long_double_u __infinityl =
#if BYTE_ORDER == BIG_ENDIAN
	{ { 0x7f, 0xf0, 0, 0, 0, 0,    0,    0 } };
#else
	{ {    0,    0, 0, 0, 0, 0, 0xf0, 0x7f } };
#endif
