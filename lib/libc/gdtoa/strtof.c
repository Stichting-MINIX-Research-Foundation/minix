/* $NetBSD: strtof.c,v 1.7 2013/05/17 12:55:57 joerg Exp $ */

/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 1998, 2000 by Lucent Technologies
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of Lucent or any of its entities
not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

****************************************************************/

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").	*/

#include "namespace.h"
#include "gdtoaimp.h"

#include <locale.h>
#include "setlocale_local.h"

#ifdef __weak_alias
__weak_alias(strtof, _strtof)
__weak_alias(strtof_l, _strtof_l)
#endif

static float
_int_strtof_l(CONST char *s, char **sp, locale_t loc)
{
	static CONST FPI fpi0 = { 24, 1-127-24+1,  254-127-24+1, 1, SI };
	ULong bits[1];
	Long expt;
	int k;
	union { ULong L[1]; float f; } u;
#ifdef Honor_FLT_ROUNDS
#include "gdtoa_fltrnds.h"
#else
#define fpi &fpi0
#endif

	k = strtodg(s, sp, fpi, &expt, bits, loc);
	if (k == STRTOG_NoMemory) {
		errno = ERANGE;
		return HUGE_VALF;
	}
	switch(k & STRTOG_Retmask) {
	  case STRTOG_NoNumber:
	  case STRTOG_Zero:
		u.L[0] = 0;
		break;

	  case STRTOG_Normal:
	  case STRTOG_NaNbits:
		u.L[0] = (bits[0] & 0x7fffff) | ((expt + 0x7f + 23) << 23);
		break;

	  case STRTOG_Denormal:
		u.L[0] = bits[0];
		break;

	  case STRTOG_Infinite:
		u.L[0] = 0x7f800000;
		break;

	  case STRTOG_NaN:
		u.L[0] = f_QNAN;
		break;

	  default:
		u.L[0] = 0; /* for gcc warning */
		break;
	  }
	if (k & STRTOG_Neg)
		u.L[0] |= 0x80000000L;
	return u.f;
	}

float
strtof(CONST char *s, char **sp)
{
	return _int_strtof_l(s, sp, _current_locale());
}

float
strtof_l(CONST char *s, char **sp, locale_t loc)
{
	return _int_strtof_l(s, sp, loc);
}
