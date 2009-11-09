/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	CONVERTS FLOATING POINT TO EXTENDED FORMAT

	Two sizes of FLOATING Point are known:
		SINGLE and DOUBLE
*/
/********************************************************/
/*
	It is not required to normalize in extended
	format, but it has been chosen to do so.
	Extended Format is as follows (at exit):

->sign	S000 0000 | 0000 0000		<SIGN>
->exp	0EEE EEEE | EEEE EEEE		<EXPONENT>
->m1	LFFF FFFF | FFFF FFFF		<L.Fraction>
	FFFF FFFF | FFFF FFFF		<Fraction>
->m2	FFFF FFFF | FFFF FFFF		<Fraction>
	FFFF F000 | 0000 0000		<Fraction>
*/
/********************************************************/

#include "FP_bias.h"
#include "FP_shift.h"
#include "FP_types.h"
#include "get_put.h"
/********************************************************/

void
extend(from,to,size)
unsigned long	*from;
EXTEND	*to;
int	size;
{
	register char *cpt1;
	unsigned long	tmp;
	int	leadbit = 0;

	cpt1 = (char *) from;

#if FL_MSL_AT_LOW_ADDRESS
#if FL_MSW_AT_LOW_ADDRESS
	to->exp = uget2(cpt1);
#else
	to->exp = uget2(cpt1+2);
#endif
#else
#if FL_MSW_AT_LOW_ADDRESS
	to->exp = uget2(cpt1+(size == sizeof(DOUBLE) ? 4 : 0));
#else
	to->exp = uget2(cpt1+(size == sizeof(DOUBLE) ? 6 : 2));
#endif
#endif
	to->sign = (to->exp & 0x8000);	/* set sign bit */
	to->exp ^= to->sign;
	if (size == sizeof(DOUBLE))
		to->exp >>= DBL_EXPSHIFT;
	else
		to->exp >>= SGL_EXPSHIFT;
	if (to->exp > 0)
		leadbit++;	/* will set Lead bit later	*/
	else to->exp++;

	if (size == sizeof(DOUBLE))	{
#if FL_MSL_AT_LOW_ADDRESS
		to->m1 = get4(cpt1);
		cpt1 += 4;
		tmp = get4(cpt1);
#else
		tmp = get4(cpt1);
		cpt1 += 4;
		to->m1 = get4(cpt1);
#endif
		if (to->exp == 1 && to->m1 == 0 && tmp == 0) {
			to->exp = 0;
			to->sign = 0;
			to->m1 = 0;
			to->m2 = 0;
			return;
		}
		to->m1 <<= DBL_M1LEFT;		/* shift	*/
		to->exp -= DBL_BIAS;		/* remove bias	*/
		to->m1 |= (tmp>>DBL_RPACK);	/* plus 10 == 32	*/
		to->m2 = (tmp<<DBL_LPACK);	/* plus 22 == 32	*/
	}
	else	{	/* size == sizeof(SINGLE)		*/
		to->m1 = get4(cpt1);
		to->m1  <<= SGL_M1LEFT;	/* shift	*/
		if (to->exp == 1 && to->m1 == 0) {
			to->exp = 0;
			to->sign = 0;
			to->m1 = 0;
			to->m2 = 0;
			return;
		}
		to->exp -= SGL_BIAS;		/* remove bias	*/
		to->m2 = 0L;
	}

	to->m1 |= NORMBIT;				/* set bit L	*/
	if (leadbit == 0) {		/* set or clear Leading Bit	*/
		to->m1 &= ~NORMBIT;			/* clear bit L	*/
		nrm_ext(to);				/* and normalize */
	}
}
