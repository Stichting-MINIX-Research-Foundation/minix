/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<stdlib.h>

void *
bsearch(register const void *key, register const void *base,
	register size_t nmemb, register size_t size,
	int (*compar)(const void *, const void *))
{
	register const void *mid_point;
	register int  cmp;

	while (nmemb > 0) {
		mid_point = (char *)base + size * (nmemb >> 1);
		if ((cmp = (*compar)(key, mid_point)) == 0)
			return (void *)mid_point;
		if (cmp >= 0) {
			base  = (char *)mid_point + size;
			nmemb = (nmemb - 1) >> 1;
		} else
			nmemb >>= 1;
	}
	return (void *)NULL;
}
