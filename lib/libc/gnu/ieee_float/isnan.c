/*
libc/ieee_float/isnan.c

Created:	Oct 14, 1993 by Philip Homburg <philip@cs.vu.nl>

Implementation of isnan that directly tests the bits in an ieee float
*/

#define _MINIX_SOURCE

#include <sys/types.h>

#include "ieee_float.h"

int isnan(value)
double value;
{
	struct f64 *f64p;
	int exp;

	f64p= (struct f64 *)&value;
	exp= F64_GET_EXP(f64p);
	if (exp != F64_EXP_MAX)
		return 0;
	return F64_GET_MANT_LOW(f64p) != 0 || F64_GET_MANT_HIGH(f64p) != 0;
}

/*
 * $PchId: isnan.c,v 1.3 1996/02/22 21:01:39 philip Exp $
 */
