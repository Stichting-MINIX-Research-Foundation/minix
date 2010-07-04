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
#include "write.h"

/*VARARGS*/
/*FORMAT0v $
	%s = char *
	%l = long
	%c = int
	%[uxbo] = unsigned int
	%d = int
$ */
void
#if __STDC__
print(char *fmt, ...)
#else
print(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list args;
	char buf[SSIZE];

#if __STDC__
	va_start(args, fmt);
#else
	va_start(args);
#endif
	sys_write(STDOUT, buf, _format(buf, fmt, args));
	va_end(args);
}
