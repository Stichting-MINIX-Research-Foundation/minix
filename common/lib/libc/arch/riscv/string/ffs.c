/* $NetBSD: ffs.c,v 1.1 2014/09/19 17:36:24 matt Exp $ */

#include <sys/cdefs.h>

__strong_alias(ffs, __ffssi2);
int	__ffssi2(int);

int
__ffssi2(int i)
{
	return i == 0 ? 0 : __buildint_ctz(i) + 1;
}
