/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	ROUTINE TO MULTIPLY TWO EXTENDED FORMAT NUMBERS
*/

# include "FP_bias.h"
# include "FP_trap.h"
# include "FP_types.h"
# include "FP_shift.h"

void
mul_ext(e1,e2)
EXTEND	*e1,*e2;
{
	register int	i,j;		/* loop control	*/
	unsigned short	mp[4];		/* multiplier */
	unsigned short	mc[4];		/* multipcand */
	unsigned short	result[8];	/* result */
	register unsigned short *pres;

	/* first save the sign (XOR)			*/
	e1->sign ^= e2->sign;

	/* compute new exponent */
	e1->exp += e2->exp + 1;
	/* 128 bit multiply of mantissas			*/

		/* assign unknown long formats		*/
		/* to known unsigned word formats	*/
	mp[0] = e1->m1 >> 16;
	mp[1] = (unsigned short) e1->m1;
	mp[2] = e1->m2 >> 16;
	mp[3] = (unsigned short) e1->m2;
	mc[0] = e2->m1 >> 16;
	mc[1] = (unsigned short) e2->m1;
	mc[2] = e2->m2 >> 16;
	mc[3] = (unsigned short) e2->m2;
	for (i = 8; i--;) {
		result[i] = 0;
	}
	/*
	 *	fill registers with their components
	 */
	for(i=4, pres = &result[4];i--;pres--) if (mp[i]) {
		unsigned short k = 0;
		unsigned long mpi = mp[i];
		for(j=4;j--;) {
			unsigned long tmp = (unsigned long)pres[j] + k;
			if (mc[j]) tmp += mpi * mc[j];
			pres[j] = tmp;
			k = tmp >> 16;
		}
		pres[-1] = k;
	}
        if (! (result[0] & 0x8000)) {
                e1->exp--;
                for (i = 0; i <= 3; i++) {
                        result[i] <<= 1;
                        if (result[i+1]&0x8000) result[i] |= 1;
                }
                result[4] <<= 1;
        }

	/*
	 *	combine the registers to a total
	 */
	e1->m1 = ((unsigned long)(result[0]) << 16) + result[1];
	e1->m2 = ((unsigned long)(result[2]) << 16) + result[3];
	if (result[4] & 0x8000) {
		if (++e1->m2 == 0)
			if (++e1->m1 == 0) {
				e1->m1 = NORMBIT;
				e1->exp++;
			}
	}

					/* check for overflow	*/
	if (e1->exp >= EXT_MAX)	{
		trap(EFOVFL);
			/* if caught 			*/
			/* return signed infinity	*/
		e1->exp = EXT_MAX;
infinity:	e1->m1 = e1->m2 =0L;
		return;
	}
				/* check for underflow	*/
	if (e1->exp < EXT_MIN)	{
		trap(EFUNFL);
		e1->exp = EXT_MIN;
		goto infinity;
	}
}
