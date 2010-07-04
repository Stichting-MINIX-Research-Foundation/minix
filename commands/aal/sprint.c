/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <system.h>
#include "param.h"
#include "format.h"

/*VARARGS*/
/*FORMAT1v $
	%s = char *
	%l = long
	%c = int
	%[uxbo] = unsigned int
	%d = int
$ */
char *
#if __STDC__
sprint(char *buf, char *fmt, ...)
#else
sprint(buf, fmt, va_alist)
	char *buf, *fmt;
	va_dcl
#endif
{
	va_list args;

#if __STDC__
	va_start(args, fmt);
#else
	va_start(args);
#endif
	buf[_format(buf, fmt, args)] = '\0';
	va_end(args);
	return buf;
}
