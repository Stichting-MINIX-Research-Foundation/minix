/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	SEPERATE DOUBLE INTO EXPONENT AND FRACTION (FEF 8)
*/

#include	"FP_types.h"

void
fef8(r, s1)
DOUBLE	s1;
struct fef8_returns *r;
{
	EXTEND	buf;
	register struct fef8_returns *p = r;	/* make copy, r might refer
						   to itself (see table)
						*/

	extend(&s1.d[0],&buf,sizeof(DOUBLE));
	if (buf.exp == 0 && buf.m1 == 0 && buf.m2 == 0) {
		p->e = 0;
	}
	else {
		p->e = buf.exp + 1;
		buf.exp = -1;
	}
	compact(&buf,&p->f.d[0],sizeof(DOUBLE));
}
