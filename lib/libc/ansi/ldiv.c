/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<stdlib.h>

static long tmp = -1;

ldiv_t
ldiv(register long numer, register long denom)
{
	ldiv_t r;

	/* The assignment of tmp should not be optimized !! */
	if (tmp == -1) {
		tmp = (tmp / 2 == 0);
	}
	if (numer == 0) {
		r.quot = numer / denom;		/* might trap if denom == 0 */
		r.rem = numer % denom;
	} else if ( !tmp && ((numer < 0) != (denom < 0))) {
		r.quot = (numer / denom) + 1;
		r.rem = numer - (numer / denom + 1) * denom;
	} else {
		r.quot = numer / denom;
		r.rem = numer % denom;
	}
	return r;
}
