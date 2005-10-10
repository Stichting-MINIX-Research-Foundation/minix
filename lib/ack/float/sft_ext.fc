/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	SHIFT TWO EXTENDED NUMBERS INTO PROPER
	ALIGNMENT FOR ADDITION (exponents are equal)
	Numbers should not be zero on entry.
*/

#include "FP_types.h"

void
sft_ext(e1,e2)
EXTEND	*e1,*e2;
{
	register	EXTEND	*s;
	register	int	diff;

	diff = e1->exp - e2->exp;

	if (!diff)
		return;	/* exponents are equal	*/

	if (diff < 0)	{ /* e2 is larger	*/
			/* shift e1		*/
		diff = -diff;
		s = e1;
	}
	else		/* e1 is larger		*/
			/* shift e2		*/
		s = e2;

	s->exp += diff;
	b64_sft(&(s->mantissa), diff);
}
