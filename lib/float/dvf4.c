/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	DIVIDE TWO SINGLES - SINGLE Precision (dvf 4)
*/

#include	"FP_types.h"

void
dvf4(s2,s1)
SINGLE	s1,s2;
{
	EXTEND	e1,e2;

	extend(&s1,&e1,sizeof(SINGLE));
	extend(&s2,&e2,sizeof(SINGLE));

		/* do a divide */
	div_ext(&e1,&e2);
	compact(&e1,&s1,sizeof(SINGLE));
}
