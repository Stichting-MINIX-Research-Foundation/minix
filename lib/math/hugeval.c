/*
 * (c) copyright 1990 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 * Author: Hans van Eck
 */
/* $Header$ */
#include	<assert.h>
#include	<math.h>

double
__huge_val(void)
{
#if (CHIP == INTEL)
	static unsigned char ieee_infinity[] = { 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x7f };

	assert(sizeof(double) == sizeof(ieee_infinity));
	return *(double *) ieee_infinity;
#else
	return 1.0e+1000;	/* This will generate a warning */
#endif
}
