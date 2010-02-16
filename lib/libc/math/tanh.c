/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* $Header$ */

#include	<float.h>
#include	<math.h>
#include	<errno.h>
#include	"localmath.h"

double
tanh(double x)
{
	/*	Algorithm and coefficients from:
			"Software manual for the elementary functions"
			by W.J. Cody and W. Waite, Prentice-Hall, 1980
	*/

	static double p[] = {
		-0.16134119023996228053e+4,
		-0.99225929672236083313e+2,
		-0.96437492777225469787e+0
	};
	static double q[] = {
		 0.48402357071988688686e+4,
		 0.22337720718962312926e+4,
		 0.11274474380534949335e+3,
		 1.0
	};
	int 	negative = x < 0;

	if (__IsNan(x)) {
		errno = EDOM;
		return x;
	}
	if (negative) x = -x;

	if (x >= 0.5*M_LN_MAX_D) {
		x = 1.0;
	}
#define LN3D2	0.54930614433405484570e+0	/* ln(3)/2 */
	else if (x > LN3D2) {
		x = 0.5 - 1.0/(exp(x+x)+1.0);
		x += x;
	}
	else {
		/* ??? avoid underflow ??? */
		double g = x*x;
		x += x * g * POLYNOM2(g, p)/POLYNOM3(g, q);
	}
	return negative ? -x : x;
}
