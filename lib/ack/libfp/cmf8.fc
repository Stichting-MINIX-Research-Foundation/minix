/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	COMPARE	DOUBLES (CMF 8)
*/

#include	"FP_types.h"
#include	"get_put.h"

int
cmf8(d1,d2)
DOUBLE	d1,d2;
{
#define	SIGN(x)	(((x) < 0) ? -1 : 1)
		/*
		 * return ((d1 < d2) ? 1 : (d1 > d2) ? -1 : 0))
		 */
	long	l1,l2;
	int	sign1,sign2;
	int	rv;

#if FL_MSL_AT_LOW_ADDRESS
	l1 = get4((char *)&d1);
	l2 = get4((char *)&d2);
#else
	l1 = get4(((char *)&d1+4));
	l2 = get4(((char *)&d2+4));
#endif
	sign1 = SIGN(l1);
	sign2 = SIGN(l2);
	if (sign1 != sign2) {
		l1 &= 0x7fffffff;
		l2 &= 0x7fffffff;
		if (l1 != 0 || l2 != 0) {
			return ((sign1 > 0) ? -1 : 1);
		}
	}
	if (l1 != l2)	{	/* we can decide here */
		rv = l1 < l2 ? 1 : -1;
	}
	else	{ 		/* decide in 2nd half */
		unsigned long u1, u2;
#if FL_MSL_AT_LOW_ADDRESS
		u1 = get4(((char *)&d1 + 4));
		u2 = get4(((char *)&d2 + 4));
#else
		u1 = get4((char *)&d1);
		u2 = get4((char *)&d2);
#endif
		if (u1 == u2)
			return(0);
		if (u1 < u2) rv = 1;
		else rv = -1;
	}
	return sign1 * rv;
}
