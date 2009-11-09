/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Ceriel J.H. Jacobs
 */

/* $Header$ */

#define __NO_DEFS
#include <math.h>

#if __STDC__
#include <pc_math.h>
#endif

static double
sinus(x, cos_flag)
	double x;
{
	/*	Algorithm and coefficients from:
			"Software manual for the elementary functions"
			by W.J. Cody and W. Waite, Prentice-Hall, 1980
	*/

	static double r[] = {
		-0.16666666666666665052e+0,
		 0.83333333333331650314e-2,
		-0.19841269841201840457e-3,
		 0.27557319210152756119e-5,
		-0.25052106798274584544e-7,
		 0.16058936490371589114e-9,
		-0.76429178068910467734e-12,
		 0.27204790957888846175e-14
	};

	double	xsqr;
	double	y;
	int	neg = 0;

	if (x < 0) {
		x = -x;
		neg = 1;
	}
	if (cos_flag) {
		neg = 0;
		y = M_PI_2 + x;
	}
	else	y = x;

	/* ??? avoid loss of significance, if y is too large, error ??? */

	y = y * M_1_PI + 0.5;

	/*	Use extended precision to calculate reduced argument.
		Here we used 12 bits of the mantissa for a1.
		Also split x in integer part x1 and fraction part x2.
	*/
#define A1 3.1416015625
#define A2 -8.908910206761537356617e-6
	{
		double x1, x2;
		extern double	_fif();

		_fif(y, 1.0,  &y);
		if (_fif(y, 0.5, &x1)) neg = !neg;
		if (cos_flag) y -= 0.5;
		x2 = _fif(x, 1.0, &x1);
		x = x1 - y * A1;
		x += x2;
		x -= y * A2;
#undef A1
#undef A2
	}

	if (x < 0) {
		neg = !neg;
		x = -x;
	}

	/* ??? avoid underflow ??? */

	y = x * x;
	x += x * y * POLYNOM7(y, r);
	return neg ? -x : x;
}

double
_sin(x)
	double x;
{
	return sinus(x, 0);
}

double
_cos(x)
	double x;
{
	if (x < 0) x = -x;
	return sinus(x, 1);
}
