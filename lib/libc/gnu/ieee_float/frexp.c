/*
libc/ieee_float/frexp.c

Created:	Oct 14, 1993 by Philip Homburg <philip@cs.vu.nl>

Implementation of frexp that directly manipulates the exponent bits in an
ieee float
*/

#include <sys/types.h>
#include <math.h>

#include "ieee_float.h"

double frexp(value, eptr)
double value;
int *eptr;
{
	struct f64 *f64p;
	int exp, exp_bias;
	double factor;

	f64p= (struct f64 *)&value;
	exp_bias= 0;

	exp= F64_GET_EXP(f64p);
	if (exp == F64_EXP_MAX)
	{	/* Either infinity or Nan */
		*eptr= 0;
		return value;
	}
	if (exp == 0)
	{
		/* Either 0 or denormal */
		if (F64_GET_MANT_LOW(f64p) == 0 &&
			F64_GET_MANT_HIGH(f64p) == 0)
		{
			*eptr= 0;
			return value;
		}

		/* Multiply by 2^64 */
		factor= 65536.0;	/* 2^16 */
		factor *= factor;	/* 2^32 */
		factor *= factor;	/* 2^64 */
		value *= factor;
		exp_bias= 64;
		exp= F64_GET_EXP(f64p);
	}

	exp= exp - F64_EXP_BIAS - exp_bias + 1;
	*eptr= exp;
	F64_SET_EXP(f64p, F64_EXP_BIAS-1);

	return value;
}

/*
 * $PchId: frexp.c,v 1.3 1996/02/22 21:01:39 philip Exp $
 */
