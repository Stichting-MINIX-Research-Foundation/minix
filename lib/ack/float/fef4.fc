/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	SEPERATE INTO EXPONENT AND FRACTION (FEF 4)
*/

#include	"FP_types.h"

void
fef4(r,s1)
SINGLE	s1;
struct fef4_returns	*r;
{
	EXTEND	buf;
	register struct fef4_returns	*p = r;	/* make copy; r might refer
						   to itself (see table)
						*/

	extend(&s1,&buf,sizeof(SINGLE));
	if (buf.exp == 0 && buf.m1 == 0 && buf.m2 == 0) {
		p->e = 0;
	}
	else {
		p->e = buf.exp+1;
		buf.exp = -1;
	}
	compact(&buf,&p->f,sizeof(SINGLE));
}
