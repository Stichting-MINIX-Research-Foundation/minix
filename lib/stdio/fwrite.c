/*
 * fwrite.c - write a number of array elements on a file
 */
/* $Header$ */

#include	<stdio.h>

size_t
fwrite(const void *ptr, size_t size, size_t nmemb,
	    register FILE *stream)
{
	register const unsigned char *cp = ptr;
	register size_t s;
	size_t ndone = 0;

	if (size)
		while ( ndone < nmemb ) {
			s = size;
			do {
				if (putc((int)*cp, stream)
					== EOF)
					return ndone;
				cp++;
			} 
			while (--s);
			ndone++;
		}
	return ndone;
}
