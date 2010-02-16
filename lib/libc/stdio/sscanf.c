/*
 * sscanf - read formatted output from a string
 */
/* $Header$ */

#include	<stdio.h>
#include	<stdarg.h>
#include	<string.h>
#include	"loc_incl.h"

int sscanf(const char *s, const char *format, ...)
{
	va_list ap;
	int retval;
	FILE tmp_stream;

	va_start(ap, format);

	tmp_stream._fd     = -1;
	tmp_stream._flags  = _IOREAD + _IONBF + _IOREADING;
	tmp_stream._buf    = (unsigned char *) s;
	tmp_stream._ptr    = (unsigned char *) s;
	tmp_stream._count  = strlen(s);

	retval = _doscan(&tmp_stream, format, ap);

	va_end(ap);

	return retval;
}
