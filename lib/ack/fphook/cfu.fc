/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
		CONVERT FLOAT TO UNSIGNED (CFU m n)

		N.B. The caller must know what it is getting.
		     A LONG is always returned. If it is an
		     integer the high byte is cleared first.
*/

#include "FP_trap.h"
#include "FP_types.h"

long
cfu(ds,ss,src)
int	ds;	/* destination size (2 or 4) */
int	ss;	/* source size	    (4 or 8) */
DOUBLE	src;	/* assume worst case */
{
	EXTEND	buf;
	long	new;
	short	newint, max_exp;

	extend(&src.d[0],&buf,ss);	/* get extended format	*/
	if (buf.exp < 0) {	/* no conversion needed	*/
		src.d[ss == 8] = 0L;
		return(0L);
	}
	max_exp = (ds << 3) - 1;
	if (buf.exp > max_exp) {
		trap(EIOVFL);	/* integer overflow	*/
		buf.exp %= max_exp;
	}
	new = buf.m1 >> (31-buf.exp);
done:
	src.d[ss == 8] = new;
	return(new);
}
