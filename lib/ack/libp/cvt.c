/* $Header$ */
#ifndef NOFLOAT

#if __STDC__
#include <float.h>
#else
#include <math.h>
#define DBL_MAX	M_MAX_D
#endif

static char *cvt();
#define NDIGITS	128

char *
_ecvt(value, ndigit, decpt, sign)
	double value;
	int ndigit, *decpt, *sign;
{
	return cvt(value, ndigit, decpt, sign, 1);
}

char *
_fcvt(value, ndigit, decpt, sign)
	double value;
	int ndigit, *decpt, *sign;
{
	return cvt(value, ndigit, decpt, sign, 0);
}

static struct powers_of_10 {
	double pval;
	double rpval;
	int exp;
} p10[] = {
	1.0e32, 1.0e-32, 32,
	1.0e16, 1.0e-16, 16,
	1.0e8, 1.0e-8, 8,
	1.0e4, 1.0e-4, 4,
	1.0e2, 1.0e-2, 2,
	1.0e1, 1.0e-1, 1,
	1.0e0, 1.0e0, 0
};

static char *
cvt(value, ndigit, decpt, sign, ecvtflag)
	double value;
	int ndigit, *decpt, *sign;
{
	static char buf[NDIGITS+1];
	register char *p = buf;
	register char *pe;

	if (ndigit < 0) ndigit = 0;
	if (ndigit > NDIGITS) ndigit = NDIGITS;
	pe = &buf[ndigit];
	buf[0] = '\0';

	*sign = 0;
	if (value < 0) {
		*sign = 1;
		value = -value;
	}

	*decpt = 0;
	if (value >= DBL_MAX) {
		value = DBL_MAX;
	}
	if (value != 0.0) {
		register struct powers_of_10 *pp = &p10[0];

		if (value >= 10.0) do {
			while (value >= pp->pval) {
				value *= pp->rpval;
				*decpt += pp->exp;
			}
		} while ((++pp)->exp > 0);

		pp = &p10[0];
		if (value < 1.0) do {
			while (value * pp->pval < 10.0) {
				value *= pp->pval;
				*decpt -= pp->exp;
			}
		} while ((++pp)->exp > 0);

		(*decpt)++;	/* because now value in [1.0, 10.0) */
	}
	if (! ecvtflag) {
		/* for fcvt() we need ndigit digits behind the dot */
		pe += *decpt;
		if (pe > &buf[NDIGITS]) pe = &buf[NDIGITS];
	}
	while (p <= pe) {
		*p++ = (int)value + '0';
		value = 10.0 * (value - (int)value);
	}
	if (pe >= buf) {
		p = pe;
		*p += 5;	/* round of at the end */
		while (*p > '9') {
			*p = '0';
			if (p > buf) ++*--p;
			else {
				*p = '1';
				++*decpt;
				if (! ecvtflag) {
					/* maybe add another digit at the end,
					   because the point was shifted right
					*/
					if (pe > buf) *pe = '0';
					pe++;
				}
			}
		}
		*pe = '\0';
	}
	return buf;
}
#endif
