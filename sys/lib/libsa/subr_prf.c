/*	$NetBSD: subr_prf.c,v 1.27 2014/08/30 14:24:02 tsutsui Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)printf.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Scaled down version of printf(3).
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stdint.h>		/* XXX: for intptr_t */

#include "stand.h"

#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
#define INTMAX_T	longlong_t
#define UINTMAX_T	u_longlong_t
#else
#define INTMAX_T	long
#define UINTMAX_T	u_long
#endif

#if 0 /* XXX: abuse intptr_t until the situation with ptrdiff_t is clear */
#define PTRDIFF_T	ptrdiff_t
#else
#define PTRDIFF_T	intptr_t
#endif

#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
static void kprintn(void (*)(int), UINTMAX_T, int, int, int);
#else
static void kprintn(void (*)(int), UINTMAX_T, int);
#endif
static void sputchar(int);
static void kdoprnt(void (*)(int), const char *, va_list);

static char *sbuf, *ebuf;
#if defined(__minix)
/* vsnprintf: add support for returning the amount of characters that would have been
 * written if the buffer was large enough */
static int  scount;
#endif /* defined(__minix) */

const char hexdigits[16] = "0123456789abcdef";

#define LONG		0x01
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
#define LLONG		0x02
#endif

#if defined(__minix)
#define HEXCAP		0x100
#endif /* defined(__minix) */

#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
#define ALT		0x04
#define SPACE		0x08
#define LADJUST		0x10
#define SIGN		0x20
#define ZEROPAD		0x40
#define NEGATIVE	0x80
#define KPRINTN(base)	kprintn(put, ul, base, lflag, width)
#define RADJUSTZEROPAD()					\
do {								\
	if ((lflag & (ZEROPAD|LADJUST)) == ZEROPAD) {		\
		while (width-- > 0)				\
			put('0');				\
	}							\
} while (/*CONSTCOND*/0)
#define LADJUSTPAD()						\
do {								\
	if (lflag & LADJUST) {					\
		while (width-- > 0)				\
			put(' ');				\
	}							\
} while (/*CONSTCOND*/0)
#define RADJUSTPAD()						\
do {								\
	if ((lflag & (ZEROPAD|LADJUST)) == 0) {			\
		while (width-- > 0)				\
			put(' ');				\
	}							\
} while (/*CONSTCOND*/0)
#else	/* LIBSA_PRINTF_WIDTH_SUPPORT */
#define KPRINTN(base)	kprintn(put, ul, base)
#define RADJUSTZEROPAD()	/**/
#define LADJUSTPAD()		/**/
#define RADJUSTPAD()		/**/
#endif	/* LIBSA_PRINTF_WIDTH_SUPPORT */

#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
#define KPRINT(base)						\
do {								\
	ul = (lflag & LLONG)					\
	    ? va_arg(ap, u_longlong_t)				\
	    : (lflag & LONG)					\
		? va_arg(ap, u_long)				\
		: va_arg(ap, u_int);				\
	KPRINTN(base);						\
} while (/*CONSTCOND*/0)
#else	/* LIBSA_PRINTF_LONGLONG_SUPPORT */
#define KPRINT(base)						\
do {								\
	ul = (lflag & LONG)					\
	    ? va_arg(ap, u_long) : va_arg(ap, u_int);		\
	KPRINTN(base);						\
} while (/*CONSTCOND*/0)
#endif	/* LIBSA_PRINTF_LONGLONG_SUPPORT */

static void
sputchar(int c)
{
#if defined(__minix)
	scount++; /* increase scount regardless */
	if (!sbuf) return; /* hanlde NULL sbuf  */
#endif /* defined(__minix) */

	if (sbuf < ebuf)
		*sbuf++ = c;
}

void
vprintf(const char *fmt, va_list ap)
{

	kdoprnt(putchar, fmt, ap);
#if defined(__minix) && defined(LIBSA_PRINTF_WIDTH_SUPPORT)
	/* BJG: our libminc kputc() relies on a 0 to flush the diag buffer. */
	putchar(0);
#endif /* defined(__minix) && defined(LIBSA_PRINTF_WIDTH_SUPPORT) */
}

int
vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{

	sbuf = buf;
	ebuf = buf + size - 1;
#if defined(__minix)
	scount = 0; /* use scount to keep track of written items */
#endif /* defined(__minix) */

	kdoprnt(sputchar, fmt, ap);

#if defined(__minix)
	if (sbuf){ /* handle case where sbuf == NULL */
		*sbuf = '\0';
	}
#if defined(_MINIX_MAGIC)
	sbuf = ebuf = NULL; /* leave no dangling pointers */
#endif
	return scount;
#else /* __minix is not defined */
	*sbuf = '\0';
	return sbuf - buf;
#endif  /* defined(__minix) */
}

static void
kdoprnt(void (*put)(int), const char *fmt, va_list ap)
{
	char *p;
	int ch;
	UINTMAX_T ul;
	int lflag;
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
	int width;
	char *q;
#if defined(__minix)
	int max;
#endif /* defined(__minix) */
#endif

	for (;;) {
		while ((ch = *fmt++) != '%') {
			if (ch == '\0')
				return;
			put(ch);
		}
		lflag = 0;
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
		width = 0;
#if defined(__minix)
		max = -1;
#endif /* defined(__minix) */
#endif
reswitch:
		switch (ch = *fmt++) {
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
#if defined(__minix)
		case '.':
			if (*fmt == '*') {
				max = va_arg(ap, int);
				fmt++;
			} else for (max = 0; *fmt >= '0' && *fmt <= '9'; fmt++)
				max = max * 10 + *fmt - '0';
			goto reswitch;
#endif /* defined(__minix) */
		case '#':
			lflag |= ALT;
			goto reswitch;
		case ' ':
			lflag |= SPACE;
			goto reswitch;
		case '-':
			lflag |= LADJUST;
			goto reswitch;
		case '+':
			lflag |= SIGN;
			goto reswitch;
		case '0':
			lflag |= ZEROPAD;
			goto reswitch;
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			for (;;) {
				width *= 10;
				width += ch - '0';
				ch = *fmt;
				if ((unsigned)ch - '0' > 9)
					break;
				++fmt;
			}
#endif
			goto reswitch;
		case 'l':
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
			if (*fmt == 'l') {
				++fmt;
				lflag |= LLONG;
			} else
#endif
				lflag |= LONG;
			goto reswitch;
		case 'j':
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
			if (sizeof(intmax_t) == sizeof(long long))
				lflag |= LLONG;
			else
#endif
			if (sizeof(intmax_t) == sizeof(long))
				lflag |= LONG;
			goto reswitch;
		case 't':
			if (sizeof(PTRDIFF_T) == sizeof(long))
				lflag |= LONG;
			goto reswitch;
		case 'z':
			if (sizeof(ssize_t) == sizeof(long))
				lflag |= LONG;
			goto reswitch;
		case 'c':
			ch = va_arg(ap, int);
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
			--width;
#endif
			RADJUSTPAD();
			put(ch & 0xFF);
			LADJUSTPAD();
			break;
		case 's':
			p = va_arg(ap, char *);
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
			for (q = p; *q != '\0'; ++q)
				continue;
#if defined(__minix)
			if (max >= 0 && q - p > max)
				q = &p[max];
#endif /* defined(__minix) */
			width -= q - p;
#endif
			RADJUSTPAD();
#if defined(LIBSA_PRINTF_WIDTH_SUPPORT) && defined(__minix)
			while ((max < 0 || max-- > 0) &&
			    (ch = (unsigned char)*p++))
#else /* !defined(LIBSA_PRINTF_WIDTH_SUPPORT) || !defined(__minix) */
			while ((ch = (unsigned char)*p++))
#endif /* !defined(LIBSA_PRINTF_WIDTH_SUPPORT) || !defined(__minix) */
				put(ch);
			LADJUSTPAD();
			break;
		case 'd':
			ul =
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
			(lflag & LLONG) ? va_arg(ap, longlong_t) :
#endif
			(lflag & LONG) ? va_arg(ap, long) : va_arg(ap, int);
			if ((INTMAX_T)ul < 0) {
				ul = -(INTMAX_T)ul;
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
				lflag |= NEGATIVE;
#else
				put('-');
#endif
			}
			KPRINTN(10);
			break;
		case 'o':
			KPRINT(8);
			break;
		case 'u':
			KPRINT(10);
			break;
		case 'p':
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
			lflag |= (LONG|ALT);
#else
			put('0');
			put('x');
#endif
			/* FALLTHROUGH */
		case 'x':
			KPRINT(16);
			break;
#if defined(__minix)
		case 'X':
			lflag |= HEXCAP;
			KPRINT(16);
			break;
#endif /* defined(__minix) */
		default:
			if (ch == '\0')
				return;
			put(ch);
			break;
		}
	}
}

static void
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
kprintn(void (*put)(int), UINTMAX_T ul, int base, int lflag, int width)
#else
kprintn(void (*put)(int), UINTMAX_T ul, int base)
#endif
{
					/* hold a INTMAX_T in base 8 */
	char *p, buf[(sizeof(INTMAX_T) * NBBY / 3) + 1 + 2 /* ALT + SIGN */];
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
	char *q;
#endif

	p = buf;
	do {
		*p++ = hexdigits[ul % base];
#if defined(__minix)
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
		/* LSC: Quick hack to support capital hex printout. */
		if ((lflag & HEXCAP) && (*(p-1) >= 'a') && (*(p-1) <= 'z')) {
			*(p-1) -= 32;
		}
#endif
#endif /* defined(__minix) */
	} while (ul /= base);
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
	q = p;
	if (lflag & ALT && *(p - 1) != '0') {
		if (base == 8) {
			*p++ = '0';
		} else if (base == 16) {
			*p++ = 'x';
			*p++ = '0';
		}
	}
	if (lflag & NEGATIVE)
		*p++ = '-';
	else if (lflag & SIGN)
		*p++ = '+';
	else if (lflag & SPACE)
		*p++ = ' ';
	width -= p - buf;
	if (lflag & ZEROPAD) {
		while (p > q)
			put(*--p);
	}
#endif
	RADJUSTPAD();
	RADJUSTZEROPAD();
	do {
		put(*--p);
	} while (p > buf);
	LADJUSTPAD();
}
