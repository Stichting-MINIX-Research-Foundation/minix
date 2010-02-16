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
atan(double x)
{
	/*      Algorithm and coefficients from:
			"Software manual for the elementary functions"
			by W.J. Cody and W. Waite, Prentice-Hall, 1980
	*/

	static double p[] = {
		-0.13688768894191926929e+2,
		-0.20505855195861651981e+2,
		-0.84946240351320683534e+1,
		-0.83758299368150059274e+0
	};
	static double q[] = {
		 0.41066306682575781263e+2,
		 0.86157349597130242515e+2,
		 0.59578436142597344465e+2,
		 0.15024001160028576121e+2,
		 1.0
	};
	static double a[] = {
		0.0,
		0.52359877559829887307710723554658381,  /* pi/6 */
		M_PI_2,
		1.04719755119659774615421446109316763   /* pi/3 */
	};

	int     neg = x < 0;
	int     n;
	double  g;

	if (__IsNan(x)) {
		errno = EDOM;
		return x;
	}
	if (neg) {
		x = -x;
	}
	if (x > 1.0) {
		x = 1.0/x;
		n = 2;
	}
	else    n = 0;

	if (x > 0.26794919243112270647) {       /* 2-sqtr(3) */
		n = n + 1;
		x = (((0.73205080756887729353*x-0.5)-0.5)+x)/
			(1.73205080756887729353+x);
	}

	/* ??? avoid underflow ??? */

	g = x * x;
	x += x * g * POLYNOM3(g, p) / POLYNOM4(g, q);
	if (n > 1) x = -x;
	x += a[n];
	return neg ? -x : x;
}
