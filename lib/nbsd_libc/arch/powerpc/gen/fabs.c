/*	$NetBSD: fabs.c,v 1.4 2005/12/24 23:10:08 perry Exp $	*/

#include <math.h>

double
fabs(double x)
{
#ifdef _SOFT_FLOAT
	if (x < 0)
		x = -x;
#else
	__asm volatile("fabs %0,%1" : "=f"(x) : "f"(x));
#endif
	return (x);
}
