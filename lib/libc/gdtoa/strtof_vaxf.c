/* $NetBSD: strtof_vaxf.c,v 1.6 2011/07/01 03:20:06 matt Exp $ */

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

/* Adapted to VAX F_floating by Klaus Klein <kleink@netbsd.org>. */

#include "namespace.h"
#include "gdtoaimp.h"

#ifdef __weak_alias
__weak_alias(strtof, _strtof)
#endif

 float
#ifdef KR_headers
strtof(s, sp) CONST char *s; char **sp;
#else
strtof(CONST char *s, char **sp)
#endif
{
	static CONST FPI fpi = { 24, 1-128-1-24+1,  255-128-1-24+1, 1, SI };
	ULong bits[1];
	Long expt;
	int k;
	union { ULong L[1]; float f; } u;

	k = strtodg(s, sp, &fpi, &expt, bits);
	if (k == STRTOG_NoMemory) {
		errno = ERANGE;
		return HUGE_VALF;
	}
	switch(k & STRTOG_Retmask) {
	  case STRTOG_NoNumber:
	  case STRTOG_Zero:
	  default:
		u.f = 0.0;
		break;

	  case STRTOG_Normal:
		u.L[0] = ((bits[0] & 0x0000ffff) << 16)	| /* FracLo */
			 ((bits[0] & 0x007f0000) >> 16)	| /* FracHi */
			 ((expt + 128 + 1 + 23)  <<  7);  /* Exp */
		break;

	  case STRTOG_Infinite:
		u.f = HUGE_VALF;
		break;

	  }
	if (k & STRTOG_Neg)
		u.L[0] |= 0x00008000L;
	return u.f;
}
