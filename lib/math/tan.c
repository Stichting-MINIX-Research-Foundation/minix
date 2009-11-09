/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* $Header$ */

#include	<math.h>
#include	<float.h>
#include	<errno.h>
#include	"localmath.h"

double
tan(double x)
{
	/*      Algorithm and coefficients from:
			"Software manual for the elementary functions"
			by W.J. Cody and W. Waite, Prentice-Hall, 1980
	*/

	int negative = x < 0;
	int invert = 0;
	double  y;
	static double   p[] = {
		 1.0,
		-0.13338350006421960681e+0,
		 0.34248878235890589960e-2,
		-0.17861707342254426711e-4
	};
	static double   q[] = {
		 1.0,
		-0.46671683339755294240e+0,
		 0.25663832289440112864e-1,
		-0.31181531907010027307e-3,
		 0.49819433993786512270e-6
	};

	if (__IsNan(x)) {
		errno = EDOM;
		return x;
	}
	if (negative) x = -x;
 
	/* ??? avoid loss of significance, error if x is too large ??? */

	y = x * M_2_PI + 0.5;

	if (y >= DBL_MAX/M_PI_2) return 0.0;

	/*      Use extended precision to calculate reduced argument.
		Here we used 12 bits of the mantissa for a1.
		Also split x in integer part x1 and fraction part x2.
	*/
    #define A1 1.57080078125
    #define A2 -4.454455103380768678308e-6
	{
		double x1, x2;

		modf(y, &y);
		if (modf(0.5*y, &x1)) invert = 1;
		x2 = modf(x, &x1);
		x = x1 - y * A1;
		x += x2;
		x -= y * A2;
    #undef A1
    #undef A2
	}

	/* ??? avoid underflow ??? */
	y = x * x;
	x += x * y * POLYNOM2(y, p+1);
	y = POLYNOM4(y, q);
	if (negative) x = -x;
	return invert ? -y/x : x/y;
}
