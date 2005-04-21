/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<string.h>

void *
memchr(const void *s, register int c, register size_t n)
{
	register const unsigned char *s1 = s;

	c = (unsigned char) c;
	if (n) {
		n++;
		while (--n > 0) {
			if (*s1++ != c) continue;
			return (void *) --s1;
		}
	}
	return NULL;
}
