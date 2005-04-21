/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include	<stdlib.h>
#include	<errno.h>

double
atof(const char *nptr)
{
	double d;
	int e = errno;

	d = strtod(nptr, (char **) NULL);
	errno = e;
	return d;
}
