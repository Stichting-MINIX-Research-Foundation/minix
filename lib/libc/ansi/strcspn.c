/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<string.h>

size_t
strcspn(const char *string, const char *notin)
{
	register const char *s1, *s2;

	for (s1 = string; *s1; s1++) {
		for(s2 = notin; *s2 != *s1 && *s2; s2++)
			/* EMPTY */ ;
		if (*s2)
			break;
	}
	return s1 - string;
}
