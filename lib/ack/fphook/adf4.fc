/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	ADD TWO FLOATS - SINGLE (ADF 4)
*/

#include	"FP_types.h"

void
adf4(s2,s1)
SINGLE	s1,s2;
{
	EXTEND	e1,e2;
	int	swap = 0;

	if (s1 == (SINGLE) 0) {
		s1 = s2;
		return;
	}
	if (s2 == (SINGLE) 0) {
		return;
	}
	extend(&s1,&e1,sizeof(SINGLE));
	extend(&s2,&e2,sizeof(SINGLE));
	add_ext(&e1,&e2);
	compact(&e1,&s1,sizeof(SINGLE));
}
