/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
		CONVERT DOUBLE TO SINGLE (CFF 8 4)

	This routine works quite simply. A floating point
	of size 08 is converted to extended format.
	This extended variable is converted back to
	a floating point of size 04.

*/

#include	"FP_types.h"

void
cff4(src)
DOUBLE	src;	/* the source itself -	THIS TIME it's DOUBLE */
{
	EXTEND	buf;

	extend(&src.d[0],&buf,sizeof(DOUBLE));	/* no matter what */
	compact(&buf,&(src.d[1]),sizeof(SINGLE));
}
