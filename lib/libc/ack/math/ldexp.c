/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<math.h>
#include	<float.h>
#include	<errno.h>

double
ldexp(double fl, int exp)
{
	int sign = 1;
	int currexp;

	if (isnan(fl)) {
		errno = EDOM;
		return fl;
	}
	if (fl == 0.0) return 0.0;
	if (fl<0) {
		fl = -fl;
		sign = -1;
	}
	if (fl > DBL_MAX) {		/* for infinity */
		errno = ERANGE;
		return sign * fl;
	}
	fl = frexp(fl,&currexp);
	exp += currexp;
	if (exp > 0) {
		if (exp > DBL_MAX_EXP) {
			errno = ERANGE;
			return sign * HUGE_VAL;
		}
		while (exp>30) {
			fl *= (double) (1L << 30);
			exp -= 30;
		}
		fl *= (double) (1L << exp);
	}
	else	{
		/* number need not be normalized */
		if (exp < DBL_MIN_EXP - DBL_MANT_DIG) {
			return 0.0;
		}
		while (exp<-30) {
			fl /= (double) (1L << 30);
			exp += 30;
		}
		fl /= (double) (1L << -exp);
	}
	return sign * fl;
}
