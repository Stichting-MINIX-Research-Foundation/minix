/*
 * vsprintf - print formatted output without ellipsis on an array
 */
/* $Header$ */

#include	<stdio.h>
#include	<stdarg.h>
#include	<limits.h>
#include	"loc_incl.h"

int
vsprintf(char *s, const char *format, va_list arg)
{
	return vsnprintf(s, INT_MAX, format, arg);
}
