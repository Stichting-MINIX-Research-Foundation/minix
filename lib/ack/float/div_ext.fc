/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	DIVIDE EXTENDED FORMAT
*/

#include "FP_bias.h"
#include "FP_trap.h"
#include "FP_types.h"

/*
	November 15, 1984

	This is a routine to do the work.
	There are two versions: 
	One is based on the partial products method
	and makes no use possible machine instructions
	to divide (hardware dividers).
	The other is used when USE_DIVIDE is defined. It is much faster on
	machines with fast 4 byte operations.
*/
/********************************************************/

void
div_ext(e1,e2)
EXTEND	*e1,*e2;
{
	short	error = 0;
	B64		result;
	register	unsigned long	*lp;
#ifndef USE_DIVIDE
	short	count;
#else
	unsigned short u[9], v[5];
	register int j;
	register unsigned short *u_p = u;
	int maxv = 4;
#endif

	if ((e2->m1 | e2->m2) == 0) {
                /*
                 * Exception 8.2 - Divide by zero
                 */
		trap(EFDIVZ);
		e1->m1 = e1->m2 = 0L;
		e1->exp = EXT_MAX;
		return;
	}
	if ((e1->m1 | e1->m2) == 0) {	/* 0 / anything == 0 */
		e1->exp = 0;	/* make sure */
		return;
	}
#ifndef USE_DIVIDE
	/*
	 * numbers are right shifted one bit to make sure
	 * that m1 is quaranteed to be larger if its
	 * maximum bit is set
	 */
	b64_rsft(&e1->mantissa);	/* 64 bit shift right */
	b64_rsft(&e2->mantissa);	/* 64 bit shift right */
	e1->exp++;
	e2->exp++;
#endif
	/*	check for underflow, divide by zero, etc	*/
	e1->sign ^= e2->sign;
	e1->exp -= e2->exp;

#ifndef USE_DIVIDE
		/* do division of mantissas	*/
		/* uses partial product method	*/
		/* init control variables	*/

	count = 64;
	result.h_32 = 0L;
	result.l_32 = 0L;

		/* partial product division loop */

	while (count--)	{
		/* first left shift result 1 bit	*/
		/* this is ALWAYS done			*/

		b64_lsft(&result);

		/* compare dividend and divisor		*/
		/* if dividend >= divisor add a bit	*/
		/* and subtract divisior from dividend	*/

		if ( (e1->m1 < e2->m1) ||
			((e1->m1 == e2->m1) && (e1->m2 < e2->m2) ))
			;	/* null statement */
				/* i.e., don't add or subtract */
		else	{
			result.l_32++;	/* ADD	*/
			if (e2->m2 > e1->m2)
				e1->m1 -= 1;	/* carry in */
			e1->m1 -= e2->m1;	/* do SUBTRACTION */
			e1->m2 -= e2->m2;	/*    SUBTRACTION */
		}

		/*	shift dividend left one bit OR	*/
		/*	IF it equals ZERO we can break out	*/
		/*	of the loop, but still must shift	*/
		/*	the quotient the remaining count bits	*/
		/* NB	save the results of this test in error	*/
		/*	if not zero, then the result is inexact. */
		/* 	this would be reported in IEEE standard	*/

		/*	lp points to dividend			*/
		lp = &e1->m1;

		error = ((*lp | *(lp+1)) != 0L) ? 1 : 0;
		if (error)	{	/* more work */
			/*	assume max bit == 0 (see above)	*/
			b64_lsft(&e1->mantissa);
			continue;
		}
		else
			break;	/* leave loop	*/
	}	/* end of divide by subtraction loop	*/

	if (count > 0)	{
		lp = &result.h_32;
		if (count > 31) {	/* move to higher word */
			*lp = *(lp+1);
			count -= 32;
			*(lp+1) = 0L;	/* clear low word	*/
		}
		if (*lp)
			*lp <<= count;	/* shift rest of way	*/
		lp++;	/*  == &result.l_32	*/
		if (*lp) {
			result.h_32 |= (*lp >> 32-count);
			*lp <<= count;
		}
	}
#else /* USE_DIVIDE */

	u[4] = (e1->m2 & 1) << 15;
	b64_rsft(&(e1->mantissa));
	u[0] = e1->m1 >> 16;
	u[1] = e1->m1;
	u[2] = e1->m2 >> 16;
	u[3] = e1->m2;
	u[5] = 0; u[6] = 0; u[7] = 0;
	v[1] = e2->m1 >> 16;
	v[2] = e2->m1;
	v[3] = e2->m2 >> 16;
	v[4] = e2->m2;
	while (! v[maxv]) maxv--;
	result.h_32 = 0;
	result.l_32 = 0;
	lp = &result.h_32;

	/*
	 * Use an algorithm of Knuth (The art of programming, Seminumerical
	 * algorithms), to divide u by v. u and v are both seen as numbers
	 * with base 65536. 
	 */
	for (j = 0; j <= 3; j++, u_p++) {
		unsigned long q_est, temp;

		if (j == 2) lp++;
		if (u_p[0] == 0 && u_p[1] < v[1]) continue;
		temp = ((unsigned long)u_p[0] << 16) + u_p[1];
		if (u_p[0] >= v[1]) {
			q_est = 0x0000FFFFL;
		}
		else {
			q_est = temp / v[1];
		}
		temp -= q_est * v[1];
		while (temp < 0x10000 && v[2]*q_est > ((temp<<16)+u_p[2])) {
			q_est--;
			temp += v[1];
		}
		/*	Now, according to Knuth, we have an estimate of the
			quotient, that is either correct or one too big, but
			almost always correct.
		*/
		if (q_est != 0)  {
			int i;
			unsigned long k = 0;
			int borrow = 0;

			for (i = maxv; i > 0; i--) {
				unsigned long tmp = q_est * v[i] + k + borrow;
				unsigned short md = tmp;

				borrow = (md > u_p[i]);
				u_p[i] -= md;
				k = tmp >> 16;
			}
			k += borrow;
			borrow = u_p[0] < k;
			u_p[0] -= k;

			if (borrow) {
				/* So, this does not happen often; the estimate
				   was one too big; correct this
				*/
				*lp |= (j & 1) ? (q_est - 1) : ((q_est-1)<<16);
				borrow = 0;
				for (i = maxv; i > 0; i--) {
					unsigned long tmp 
					    = v[i]+(unsigned long)u_p[i]+borrow;
					
					u_p[i] = tmp;
					borrow = tmp >> 16;
				}
				u_p[0] += borrow;
			}
			else *lp |= (j & 1) ? q_est : (q_est<<16);
		}
	}
#ifdef	EXCEPTION_INEXACT
	u_p = &u[0];
	for (j = 7; j >= 0; j--) {
		if (*u_p++) {
			error = 1;
			break;
		}
	}
#endif
#endif

#ifdef  EXCEPTION_INEXACT
        if (error)      {
                /*
                 * report here exception 8.5 - Inexact
                 * from Draft 8.0 of IEEE P754:
                 * In the absence of an invalid operation exception,
                 * if the rounded result of an operation is not exact or if
                 * it overflows without a trap, then the inexact exception
                 * shall be assigned. The rounded or overflowed result
                 * shall be delivered to the destination.
                 */
                INEXACT();
#endif
	e1->mantissa = result;

	nrm_ext(e1);
	if (e1->exp < EXT_MIN)	{
		/*
		 * Exception 8.4 - Underflow
		 */
		trap(EFUNFL);	/* underflow */
		e1->exp = EXT_MIN;
		e1->m1 = e1->m2 = 0L;
		return;
	}
	if (e1->exp >= EXT_MAX) {
                /*
                 * Exception 8.3 - Overflow
                 */
                trap(EFOVFL);   /* overflow */
                e1->exp = EXT_MAX;
                e1->m1 = e1->m2 = 0L;
                return;
        }
}
