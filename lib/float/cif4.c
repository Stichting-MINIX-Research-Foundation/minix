/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	CONVERT INTEGER TO SINGLE (CIF n 4)

	THIS ROUTINE WORKS BY FILLING AN EXTENDED
	WITH THE INTEGER VALUE IN EXTENDED FORMAT
	AND USES COMPACT() TO PUT IT INTO THE PROPER
	FLOATING POINT PRECISION.
*/

#include "FP_types.h"

void
cif4(ss,src)
int	ss;	/* source size */
long	src;	/* largest possible integer to convert */
{
	EXTEND	buf;
	short	*ipt;
	long	i_src;
	SINGLE	*result;

	zrf_ext(&buf);
	if (ss == sizeof(long))	{
		buf.exp = 31;
		i_src = src;
		result = (SINGLE *) &src;
	}
	else	{
		ipt = (short *) &src;
		i_src = (long) *ipt;
		buf.exp = 15;
		result = (SINGLE *) &ss;
	}
	if (i_src == 0)	{
		*result = (SINGLE) 0L;
		return;
	}
			/* ESTABLISHED THAT src != 0	*/
			/* adjust exponent field	*/
	buf.sign = (i_src < 0) ? 0x8000 : 0;
			/* clear sign bit of integer	*/
			/* move to mantissa field	*/
	buf.m1 = (i_src < 0) ? -i_src : i_src;
			/* adjust mantissa field	*/
	if (ss != sizeof(long))
		buf.m1 <<= 16;
	nrm_ext(&buf);		/* adjust mantissa field	*/
	compact(&buf, result,sizeof(SINGLE));	/* put on stack */
}
