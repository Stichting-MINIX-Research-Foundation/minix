/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	SUBTRACT TWO FLOATS - DOUBLE Precision (SBF 8)
*/

#include	"FP_types.h"

void
sbf8(s2,s1)
DOUBLE	s1,s2;
{
	EXTEND e1, e2;

	if (s2.d[0] == 0 && s2.d[1] == 0) {
		return;
	}
	extend(&s1.d[0],&e1,sizeof(DOUBLE));
	extend(&s2.d[0],&e2,sizeof(DOUBLE));
	sub_ext(&e1,&e2);
	compact(&e1,&s1.d[0],sizeof(DOUBLE));
}
