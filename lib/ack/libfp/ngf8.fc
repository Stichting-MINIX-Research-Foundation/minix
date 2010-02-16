/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
		NEGATE A FLOATING POINT (NGF 8)
*/
/********************************************************/

#include "FP_types.h"
#include "get_put.h"

#define OFF ((FL_MSL_AT_LOW_ADDRESS ? 0 : 4) + (FL_MSW_AT_LOW_ADDRESS ? 0 : 2) + (FL_MSB_AT_LOW_ADDRESS ? 0 : 1))

void
ngf8(f)
DOUBLE	f;
{
	unsigned char	*p;

	if (f.d[0] != 0 || f.d[1] != 0) {
		p = (unsigned char *) &f + OFF;
		*p ^= 0x80;
	}
}
