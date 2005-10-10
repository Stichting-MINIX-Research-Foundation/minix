/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	CONVERT INTEGER TO FLOAT (CIF n 8)

	THIS ROUTINE WORKS BY FILLING AN EXTENDED
	WITH THE INTEGER VALUE IN EXTENDED FORMAT
	AND USES COMPACT() TO PUT IT INTO THE PROPER
	FLOATING POINT PRECISION.
*/

#include "FP_types.h"

void
cif8(ss,src)
int	ss;	/* source size */
long	src;	/* largest possible integer to convert */
{
	EXTEND	buf;
	DOUBLE	*result;	/* for return value */
	short	*ipt;
	long	i_src;

	result = (DOUBLE *) ((void *) &ss);	/* always */
	zrf_ext(&buf);
	if (ss == sizeof(long))	{
		buf.exp = 31;
		i_src = src;
	}
	else	{
		ipt = (short *) &src;
		i_src = (long) *ipt;
		buf.exp = 15;
	}
	if (i_src == 0)	{
		zrf8(result);
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
	nrm_ext(&buf);
	compact(&buf,&result->d[0],8);
}
