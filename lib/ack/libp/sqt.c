/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */

/* $Header$ */
#define __NO_DEFS
#include <pc_err.h>
extern	_trp();

#define NITER	5

static double
Ldexp(fl,exp)
	double fl;
	int exp;
{
	extern double _fef();
	int sign = 1;
	int currexp;

	if (fl<0) {
		fl = -fl;
		sign = -1;
	}
	fl = _fef(fl,&currexp);
	exp += currexp;
	if (exp > 0) {
		while (exp>30) {
			fl *= (double) (1L << 30);
			exp -= 30;
		}
		fl *= (double) (1L << exp);
	}
	else	{
		while (exp<-30) {
			fl /= (double) (1L << 30);
			exp += 30;
		}
		fl /= (double) (1L << -exp);
	}
	return sign * fl;
}

double
_sqt(x)
	double x;
{
	extern double _fef();
	int exponent;
	double val;

	if (x <= 0) {
		if (x < 0) _trp(ESQT);
		return 0;
	}

	val = _fef(x, &exponent);
	if (exponent & 1) {
		exponent--;
		val *= 2;
	}
	val = Ldexp(val + 1.0, exponent/2 - 1);
	/* was: val = (val + 1.0)/2.0; val = Ldexp(val, exponent/2); */
	for (exponent = NITER - 1; exponent >= 0; exponent--) {
		val = (val + x / val) / 2.0;
	}
	return val;
}
