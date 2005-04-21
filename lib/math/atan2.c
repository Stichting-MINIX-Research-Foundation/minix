/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */
/* $Header$ */

#include	<math.h>
#include	<errno.h>
#include	"localmath.h"

double
atan2(double y, double x)
{
	double absx, absy, val;

	if (x == 0 && y == 0) {
		errno = EDOM;
		return 0;
	}
	absy = y < 0 ? -y : y;
	absx = x < 0 ? -x : x;
	if (absy - absx == absy) {
		/* x negligible compared to y */
		return y < 0 ? -M_PI_2 : M_PI_2;
	}
	if (absx - absy == absx) {
		/* y negligible compared to x */
		val = 0.0;
	}
	else	val = atan(y/x);
	if (x > 0) {
		/* first or fourth quadrant; already correct */
		return val;
	}
	if (y < 0) {
		/* third quadrant */
		return val - M_PI;
	}
	return val + M_PI;
}
