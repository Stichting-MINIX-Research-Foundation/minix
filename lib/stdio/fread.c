/*
 * fread.c - read a number of members into an array
 */
/* $Header$ */

#include	<stdio.h>

size_t
fread(void *ptr, size_t size, size_t nmemb, register FILE *stream)
{
	register char *cp = ptr;
	register int c;
	size_t ndone = 0;
	register size_t s;

	if (size)
		while ( ndone < nmemb ) {
			s = size;
			do {
				if ((c = getc(stream)) != EOF)
					*cp++ = c;
				else
					return ndone;
			} while (--s);
			ndone++;
		}

	return ndone;
}
