/*
libc/ieee_float/ldexp.c

Created:	Oct 14, 1993 by Philip Homburg <philip@cs.vu.nl>

Implementation of ldexp that directly manipulates the exponent bits in an
ieee float
*/

#include <sys/types.h>
#include <errno.h>
#include <math.h>

#include "ieee_float.h"

double ldexp(value, exp)
double value;
int exp;
{
	struct f64 *f64p;
	int oldexp, exp_bias;
	double factor;

	f64p= (struct f64 *)&value;
	exp_bias= 0;

	oldexp= F64_GET_EXP(f64p);
	if (oldexp == F64_EXP_MAX)
	{	/* Either infinity or Nan */
		return value;
	}
	if (oldexp == 0)
	{
		/* Either 0 or denormal */
		if (F64_GET_MANT_LOW(f64p) == 0 &&
			F64_GET_MANT_HIGH(f64p) == 0)
		{
			return value;
		}
	}

	/* If exp is too large (> 2*F64_EXP_MAX) or too small
	 * (< -2*F64_EXP_MAX) return HUGE_VAL or 0. This prevents overflows
	 * in exp if exp is really weird
	 */
	if (exp >= 2*F64_EXP_MAX)
	{
		errno= ERANGE;
		return HUGE_VAL;
	}
	if (exp <= -2*F64_EXP_MAX)
	{
		errno= ERANGE;
		return 0;
	}
	
	/* Normalize a denormal */
	if (oldexp == 0)
	{
		/* Multiply by 2^64 */
		factor= 65536.0;	/* 2^16 */
		factor *= factor;	/* 2^32 */
		factor *= factor;	/* 2^64 */
		value *= factor;
		exp= -64;
		oldexp= F64_GET_EXP(f64p);
	}

	exp= oldexp + exp;
	if (exp >= F64_EXP_MAX)
	{	/* Overflow */
		errno= ERANGE;
		return HUGE_VAL;
	}
	if (exp > 0)
	{
		/* Normal */
		F64_SET_EXP(f64p, exp);
		return value;
	}
	/* Denormal, or underflow. */
	exp += 64;
	F64_SET_EXP(f64p, exp);
	/* Divide by 2^64 */
	factor= 65536.0;	/* 2^16 */
	factor *= factor;	/* 2^32 */
	factor *= factor;	/* 2^64 */
	value /= factor;
	if (value == 0.0)
	{
		/* Underflow */
		errno= ERANGE;
	}
	return value;
}

/*
 * $PchId: ldexp.c,v 1.3 1996/02/22 21:01:39 philip Exp $
 */
