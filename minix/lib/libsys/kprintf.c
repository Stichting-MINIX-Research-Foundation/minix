/*	printf() - system services printf()		Author: Kees J. Bot
 *								15 Jan 1994
 */
#define nil 0
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>

int printf(const char *fmt, ...)
{
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vprintf(fmt, ap);
	va_end(ap);

	return n;
}

