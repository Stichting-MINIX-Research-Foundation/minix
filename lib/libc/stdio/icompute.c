/*
 * icompute.c - compute an integer
 */
/* $Header$ */

#include	"loc_incl.h"

/* This routine is used in doprnt.c as well as in tmpfile.c and tmpnam.c. */

char *
_i_compute(printval_u_t val, int base, char *s, int nrdigits)
{
	int c;

	c= val % base ;
	val /= base ;
	if (val || nrdigits > 1)
		s = _i_compute(val, base, s, nrdigits - 1);
	*s++ = (c>9 ? c-10+'a' : c+'0');
	return s;
}
