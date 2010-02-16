/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<string.h>

char *
strcat(char *ret, register const char *s2)
{
	register char *s1 = ret;

	while (*s1++ != '\0')
		/* EMPTY */ ;
	s1--;
	while (*s1++ = *s2++)
		/* EMPTY */ ;
	return ret;
}
