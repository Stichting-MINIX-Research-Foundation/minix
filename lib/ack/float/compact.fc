/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	COMPACT EXTEND FORMAT INTO FLOAT OF PROPER SIZE
*/

# include "FP_bias.h"
# include "FP_shift.h"
# include "FP_trap.h"
# include "FP_types.h"
# include "get_put.h"

void
compact(f,to,size)
EXTEND	*f;
unsigned long	*to;
int	size;
{
	int	error = 0;

	if (size == sizeof(DOUBLE)) {
	/*
	 * COMPACT EXTENDED INTO DOUBLE
	 */
		DOUBLE *DBL = (DOUBLE *) (void *) to;

		if ((f->m1|(f->m2 & DBL_ZERO)) == 0L)	{
			zrf8(DBL);
			return;
		}
		f->exp += DBL_BIAS;	/* restore proper bias	*/
		if (f->exp > DBL_MAX)	{
dbl_over:			trap(EFOVFL);
			f->exp = DBL_MAX+1;
			f->m1 = 0;
			f->m2 = 0;
			if (error++)
				return;
		}
		else if (f->exp < DBL_MIN)	{
			b64_rsft(&(f->mantissa));
			if (f->exp < 0) {
				b64_sft(&(f->mantissa), -f->exp);
				f->exp = 0;
			}
			/* underflow ??? */
		}
			
		/* local CAST conversion		*/

		/* because of special format shift only 10 bits */
		/* bit shift mantissa 10 bits		*/

		/* first align within words, then do store operation */

		DBL->d[0] = f->m1 >> DBL_RUNPACK;   /* plus 22 == 32 */
		DBL->d[1] = f->m2 >> DBL_RUNPACK;   /* plus 22 == 32 */
		DBL->d[1] |= (f->m1 << DBL_LUNPACK); /* plus 10 == 32 */

		/* if not exact then round to nearest	*/
		/* on a tie, round to even */

#ifdef EXCEPTION_INEXACT
		if ((f->m2 & DBL_EXACT) != 0) {
		    INEXACT();
#endif
		    if (((f->m2 & DBL_EXACT) > DBL_ROUNDUP)
			|| ((f->m2 & DBL_EXACT) == DBL_ROUNDUP
			    && (f->m2 & (DBL_ROUNDUP << 1)))) {
			DBL->d[1]++;	/* rounding up	*/
			if (DBL->d[1] == 0L) { /* carry out	*/
			    DBL->d[0]++;

			    if (f->exp == 0 && (DBL->d[0] & ~DBL_MASK)) {
					f->exp++;
				}
			    if (DBL->d[0] & DBL_CARRYOUT) { /* carry out */
				if (DBL->d[0] & 01)
				    DBL->d[1] = CARRYBIT;
				DBL->d[0] >>= 1;
				f->exp++;
			    }
			}
			/*	check for overflow			*/
			if (f->exp > DBL_MAX)
		    		goto dbl_over;
		    }
#ifdef EXCEPTION_INEXACT
		}
#endif

		/*
		 * STORE EXPONENT AND SIGN:
		 *
		 * 1) clear leading bits (B4-B15)
		 * 2) shift and store exponent
		 */

		DBL->d[0] &= DBL_MASK;
		DBL->d[0] |= 
			((long) (f->exp << DBL_EXPSHIFT) << EXP_STORE);
		if (f->sign)
			DBL->d[0] |= CARRYBIT;

		/*
		 * STORE MANTISSA
		 */

#if FL_MSL_AT_LOW_ADDRESS
		put4(DBL->d[0], (char *) &DBL->d[0]);
		put4(DBL->d[1], (char *) &DBL->d[1]);
#else
		{ unsigned long l;
		  put4(DBL->d[1], (char *) &l);
		  put4(DBL->d[0], (char *) &DBL->d[1]);
		  DBL->d[0] = l;
		}
#endif
	}
	else {
		/*
		 * COMPACT EXTENDED INTO FLOAT
		 */
		SINGLE	*SGL;

		/* local CAST conversion		*/
		SGL = (SINGLE *) (void *) to;
		if ((f->m1 & SGL_ZERO) == 0L)	{
			*SGL = 0L;
			return;
		}
		f->exp += SGL_BIAS;	/* restore bias	*/
		if (f->exp > SGL_MAX)	{
sgl_over:			trap(EFOVFL);
			f->exp = SGL_MAX+1;
			f->m1 = 0L;
			f->m2 = 0L;
			if (error++)
				return;
		}
		else if (f->exp < SGL_MIN)	{
			b64_rsft(&(f->mantissa));
			if (f->exp < 0) {
				b64_sft(&(f->mantissa), -f->exp);
				f->exp = 0;
			}
			/* underflow ??? */
		}

		/* shift mantissa and store	*/
		*SGL = (f->m1 >> SGL_RUNPACK);

		/* check for rounding to nearest	*/
		/* on a tie, round to even		*/
#ifdef EXCEPTION_INEXACT
		if (f->m2 != 0 ||
		    (f->m1 & SGL_EXACT) != 0L) {
			INEXACT();
#endif
		        if (((f->m1 & SGL_EXACT) > SGL_ROUNDUP)
			    || ((f->m1 & SGL_EXACT) == SGL_ROUNDUP
			        && (f->m1 & (SGL_ROUNDUP << 1)))) {
				(*SGL)++;
				if (f->exp == 0 && (*SGL & ~SGL_MASK)) {
					f->exp++;
				}
			/* check normal */
				if (*SGL & SGL_CARRYOUT)	{
					*SGL >>= 1;
					f->exp++;
				}
				if (f->exp > SGL_MAX)
					goto sgl_over;
			}
#ifdef EXCEPTION_INEXACT
		}
#endif

		/*
		 * STORE EXPONENT AND SIGN:
		 *
		 * 1) clear leading bit of fraction
		 * 2) shift and store exponent
		 */

		*SGL &= SGL_MASK; /* B23-B31 are 0 */
		*SGL |= ((long) (f->exp << SGL_EXPSHIFT) << EXP_STORE);
		if (f->sign)
			*SGL |= CARRYBIT;

		/*
		 * STORE MANTISSA
		 */

		put4(*SGL, (char *) &SGL);
	}
}
