/*
 * scanf.c - read formatted input from the standard input stream
 */
/* $Header$ */

#include	<stdio.h>
#include	<stdarg.h>
#include	"loc_incl.h"

int
scanf(const char *format, ...)
{
	va_list ap;
	int retval;

	va_start(ap, format);

	retval = _doscan(stdin, format, ap);

	va_end(ap);

	return retval;
}


