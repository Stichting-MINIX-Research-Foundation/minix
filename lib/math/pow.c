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
pow(double x, double y)
{
	/*	Simple version for now. The Cody and Waite book has
		a very complicated, much more precise version, but
		this version has machine-dependent arrays A1 and A2,
		and I don't know yet how to solve this ???
	*/
	double dummy;
	int	result_neg = 0;

	if ((x == 0 && y == 0) ||
	    (x < 0 && modf(y, &dummy) != 0)) {
		errno = EDOM;
		return 0;
	}

	if (x == 0) return x;

	if (x < 0) {
		if (modf(y/2.0, &dummy) != 0) {
			/* y was odd */
			result_neg = 1;
		}
		x = -x;
	}
	x = log(x);

	if (x < 0) {
		x = -x;
		y = -y;
	}
	/* Beware of overflow in the multiplication */
	if (x > 1.0 && y > DBL_MAX/x) {
		errno = ERANGE;
		return result_neg ? -HUGE_VAL : HUGE_VAL;
	}

	x = exp(x * y);
	return result_neg ? -x : x;
}
