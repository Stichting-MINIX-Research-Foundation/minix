/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/********************************************************/
/*
	NORMALIZE an EXTENDED FORMAT NUMBER
*/
/********************************************************/

#include "FP_shift.h"
#include "FP_types.h"

void
nrm_ext(e1)
EXTEND	*e1;
{
		/* we assume that the mantissa != 0	*/
		/* if it is then just return		*/
		/* to let it be a problem elsewhere	*/
		/* THAT IS, The exponent is not set to	*/
		/* zero. If we don't test here an	*/
		/* infinite loop is generated when	*/
		/* mantissa is zero			*/

	if ((e1->m1 | e1->m2) == 0L)
		return;

		/* if top word is zero mov low word	*/
		/* to top word, adjust exponent value	*/
	if (e1->m1 == 0L)	{
		e1->m1 = e1->m2;
		e1->m2 = 0L;
		e1->exp -= 32;
	}
	if ((e1->m1 & NORMBIT) == 0) {
		unsigned long l = ((unsigned long)NORMBIT >> 1);
		int cnt = -1;

		while (! (l & e1->m1)) {
			l >>= 1;
			cnt--;
		}
		e1->exp += cnt;
		b64_sft(&(e1->mantissa), cnt);
	}
}
