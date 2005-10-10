/*
 (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	SUBTRACT TWO FLOATS - SINGLE Precision (SBF 4)
*/

#include	"FP_types.h"

void
sbf4(s2,s1)
SINGLE	s1,s2;
{
	EXTEND e1,e2;

	if (s2 == (SINGLE) 0) {
		return;
	}
	extend(&s1,&e1,sizeof(SINGLE));
	extend(&s2,&e2,sizeof(SINGLE));
	sub_ext(&e1,&e2);
	compact(&e1,&s1,sizeof(SINGLE));
}
