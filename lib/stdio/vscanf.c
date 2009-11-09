/*
 * vscanf.c - read formatted input from the standard input stream
 */

#include	<stdio.h>
#include	<stdarg.h>
#include	"loc_incl.h"

int
vscanf(const char *format, va_list ap)
{
	return _doscan(stdin, format, ap);
}
