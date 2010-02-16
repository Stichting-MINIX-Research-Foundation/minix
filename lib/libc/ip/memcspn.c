/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<string.h>

size_t
memcspn(const char *string, size_t strlen, const char *notin, size_t notinlen)
{
	register const char *s1, *s2;
	int i,j;

	for (s1 = string, i = 0; i<strlen; s1++, i++) {
		for(s2 = notin, j = 0; *s2 != *s1 && j < notinlen; s2++, j++)
			/* EMPTY */ ;
		if (j != notinlen)
			break;
	}
	return s1 - string;
}
