/*	$NetBSD: xprintf.c,v 1.21 2010/12/16 22:52:32 joerg Exp $	 */

/*
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: xprintf.c,v 1.21 2010/12/16 22:52:32 joerg Exp $");
#endif /* not lint */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include "rtldenv.h"

#ifdef RTLD_LOADER
#define	SZ_LONG		0x01
#define	SZ_UNSIGNED	0x02
#define SZ_SIZE_T	0x04

/*
 * Non-mallocing printf, for use by malloc and rtld itself.
 * This avoids putting in most of stdio.
 *
 * deals withs formats %x, %p, %s, and %d.
 */
size_t
xvsnprintf(char *buf, size_t buflen, const char *fmt, va_list ap)
{
	char *bp = buf;
	char *const ep = buf + buflen - 4;
	int size, prec;

	while (*fmt != '\0' && bp < ep) {
		switch (*fmt) {
		case '\\':{
			if (fmt[1] != '\0')
				*bp++ = *++fmt;
			continue;
		}
		case '%':{
			size = 0;
			prec = -1;
	rflag:		switch (fmt[1]) {
			case '*':
				prec = va_arg(ap, int);
				/* FALLTHROUGH */
			case '.':
				fmt++;
				goto rflag;
			case 'l':
				size |= SZ_LONG;
				fmt++;
				goto rflag;
			case 'z':
				size |= SZ_SIZE_T;
				fmt++;
				goto rflag;
			case 'u':
				size |= SZ_UNSIGNED;
				/* FALLTHROUGH */
			case 'd':{
				long sval;
				unsigned long uval;
				char digits[sizeof(int) * 3], *dp = digits;
#define	SARG() \
(size & SZ_LONG ? va_arg(ap, long) : \
((size & SZ_SIZE_T ? (long)va_arg(ap, size_t) : \
va_arg(ap, int))))
#define	UARG() \
(size & SZ_LONG ? va_arg(ap, unsigned long) : \
((size & SZ_SIZE_T ? va_arg(ap, size_t) : \
va_arg(ap, unsigned int))))

				if (fmt[1] == 'd') {
					if (size & SZ_UNSIGNED)
						sval = UARG();
					else
						sval = SARG();
					if (sval < 0) {
						if ((sval << 1) == 0) {
							/*
							 * We can't flip the
							 * sign of this since
							 * it can't be
							 * represented as a
							 * positive number in
							 * two complement,
							 * handle the first
							 * digit. After that,
							 * it can be flipped
							 * since it is now not
							 * 2^(n-1).
							 */
							*dp++ = '0'-(sval % 10);
							sval /= 10;
						}
						*bp++ = '-';
						uval = -sval;
					} else {
						uval = sval;
					}
				} else {
					if (size & SZ_UNSIGNED)
						uval = UARG();
					else
						uval = SARG();
				}
				do {
					*dp++ = '0' + (uval % 10);
					uval /= 10;
				} while (uval != 0);
				do {
					*bp++ = *--dp;
				} while (dp != digits && bp < ep);
				fmt += 2;
				break;
			}
			case 'x':
			case 'p':{
				unsigned long val = va_arg(ap, unsigned long);
				unsigned long mask = ~(~0UL >> 4);
				int bits = sizeof(val) * 8 - 4;
				const char hexdigits[] = "0123456789abcdef";
				if (fmt[1] == 'p') {
					*bp++ = '0';
					*bp++ = 'x';
				}
				/* handle the border case */
				if (val == 0) {
					*bp++ = '0';
					fmt += 2;
					break;
				}
				/* suppress 0s */
				while ((val & mask) == 0)
					bits -= 4, mask >>= 4;

				/* emit the hex digits */
				while (bits >= 0 && bp < ep) {
					*bp++ = hexdigits[(val & mask) >> bits];
					bits -= 4, mask >>= 4;
				}
				fmt += 2;
				break;
			}
			case 's':{
				const char *str = va_arg(ap, const char *);
				int len;

				if (str == NULL)
					str = "(null)";

				if (prec < 0)
					len = strlen(str);
				else
					len = prec;
				if (ep - bp < len)
					len = ep - bp;
				memcpy(bp, str, len);
				bp += len;
				fmt += 2;
				break;
			}
			case 'c':{
				int c = va_arg(ap, int);
				*bp++ = (char)c;
				fmt += 2;
				break;
			}
			default:
				*bp++ = *fmt;
				break;
			}
			break;
		}
		default:
			*bp++ = *fmt++;
			break;
		}
	}

	*bp = '\0';
	return bp - buf;
}

void
xvprintf(const char *fmt, va_list ap)
{
	char buf[256];

	(void) write(2, buf, xvsnprintf(buf, sizeof(buf), fmt, ap));
}

void
xprintf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	xvprintf(fmt, ap);

	va_end(ap);
}

void
xsnprintf(char *buf, size_t buflen, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	xvsnprintf(buf, buflen, fmt, ap);

	va_end(ap);
}

#include "errlist_concat.h"

const char *
xstrerror(int error)
{

	if (error >= concat_nerr || error < 0) {
		static char buf[128];
		xsnprintf(buf, sizeof(buf), "Unknown error: %d", error);
		return buf;
	}
	return concat_errlist + concat_offset[error];
}

void
xerrx(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	xvprintf(fmt, ap);
	va_end(ap);
	(void) write(2, "\n", 1);

	exit(eval);
}

void
xerr(int eval, const char *fmt, ...)
{
	int saved_errno = errno;
	va_list ap;

	va_start(ap, fmt);
	xvprintf(fmt, ap);
	va_end(ap);

	xprintf(": %s\n", xstrerror(saved_errno));
	exit(eval);
}

void
xwarn(const char *fmt, ...)
{
	int saved_errno = errno;
	va_list ap;

	va_start(ap, fmt);
	xvprintf(fmt, ap);
	va_end(ap);

	xprintf(": %s\n", xstrerror(saved_errno));
	errno = saved_errno;
}

void
xwarnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	xvprintf(fmt, ap);
	va_end(ap);
	(void) write(2, "\n", 1);
}

#ifdef DEBUG
void
xassert(const char *file, int line, const char *failedexpr)
{

	xprintf("assertion \"%s\" failed: file \"%s\", line %d\n",
		failedexpr, file, line);
	abort();
	/* NOTREACHED */
}
#endif
#endif
