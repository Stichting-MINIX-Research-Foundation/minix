/*	$NetBSD: fabs.c,v 1.2 2006/07/05 18:07:22 ross Exp $	*/

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
