/* $Header$ */

#include	<stdio.h>
#include	<stdarg.h>
#include	<limits.h>
#include	"loc_incl.h"

int
vsnprintf(char *s, size_t n, const char *format, va_list arg)
{
	int retval;
	FILE tmp_stream;

	tmp_stream._fd     = -1;
	tmp_stream._flags  = _IOWRITE + _IONBF + _IOWRITING;
	tmp_stream._buf    = (unsigned char *) s;
	tmp_stream._ptr    = (unsigned char *) s;
	tmp_stream._count  = n-1;

	retval = _doprnt(format, arg, &tmp_stream);
	if(n > 0) {
		tmp_stream._count  = 1;
		(void) putc('\0',&tmp_stream);
	}

	return retval;
}

