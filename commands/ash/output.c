/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)output.c	8.1 (Berkeley) 5/31/93";
#endif
#endif /* not lint */

/*
 * Shell output routines.  We use our own output routines because:
 *	When a builtin command is interrupted we have to discard
 *		any pending output.
 *	When a builtin command appears in back quotes, we want to
 *		save the output of the command in a region obtained
 *		via malloc, rather than doing a fork and reading the
 *		output of the command via a pipe.
 *	Our output routines may be smaller than the stdio routines.
 */

#include <stdio.h>	/* defines BUFSIZ */
#include "shell.h"
#include "syntax.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "var.h"
#ifdef __STDC__
#include "stdarg.h"
#else
#include <varargs.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


#define OUTBUFSIZ BUFSIZ
#define BLOCK_OUT -2		/* output to a fixed block of memory */
#define MEM_OUT -3		/* output to dynamically allocated memory */
#define OUTPUT_ERR 01		/* error occurred on output */


struct output output = {NULL, 0, NULL, OUTBUFSIZ, 1, 0};
struct output errout = {NULL, 0, NULL, 100, 2, 0};
struct output memout = {NULL, 0, NULL, 0, MEM_OUT, 0};
struct output *out1 = &output;
struct output *out2 = &errout;



#ifdef mkinit

INCLUDE "output.h"
INCLUDE "memalloc.h"

RESET {
	out1 = &output;
	out2 = &errout;
	if (memout.buf != NULL) {
		ckfree(memout.buf);
		memout.buf = NULL;
	}
}

#endif


#ifdef notdef	/* no longer used */
/*
 * Set up an output file to write to memory rather than a file.
 */

void
open_mem(block, length, file)
	char *block;
	int length;
	struct output *file;
	{
	file->nextc = block;
	file->nleft = --length;
	file->fd = BLOCK_OUT;
	file->flags = 0;
}
#endif


void
out1str(p)
	const char *p;
	{
	outstr(p, out1);
}

void
out1qstr(const char *p)
{
	outqstr(p, out1);
}


void
out2str(const char *p)
{
	outstr(p, out2);
}


void
outstr(p, file)
	register const char *p;
	register struct output *file;
	{
	while (*p)
		outc(*p++, file);
	if (file == out2)
		flushout(file);
}

/* Like outstr(), but quote for re-input into the shell. */
void
outqstr(const char *p, struct output *file)
{
	char ch;

	if (p[strcspn(p, "|&;<>()$`\\\"'")] == '\0' && (!ifsset() ||
	    p[strcspn(p, ifsval())] == '\0')) {
		outstr(p, file);
		return;
	}

	out1c('\'');
	while ((ch = *p++) != '\0') {
		switch (ch) {
		case '\'':
			/*
			 * Can't quote single quotes inside single quotes;
			 * close them, write escaped single quote, open again.
			 */
			outstr("'\\''", file);
			break;
		default:
			outc(ch, file);
		}
	}
	out1c('\'');
}


char out_junk[16];


void
emptyoutbuf(dest)
	struct output *dest;
	{
	int offset;

	if (dest->fd == BLOCK_OUT) {
		dest->nextc = out_junk;
		dest->nleft = sizeof out_junk;
		dest->flags |= OUTPUT_ERR;
	} else if (dest->buf == NULL) {
		INTOFF;
		dest->buf = ckmalloc(dest->bufsize);
		dest->nextc = dest->buf;
		dest->nleft = dest->bufsize;
		INTON;
	} else if (dest->fd == MEM_OUT) {
		offset = dest->bufsize;
		INTOFF;
		dest->bufsize <<= 1;
		dest->buf = ckrealloc(dest->buf, dest->bufsize);
		dest->nleft = dest->bufsize - offset;
		dest->nextc = dest->buf + offset;
		INTON;
	} else {
		flushout(dest);
	}
	dest->nleft--;
}


void
flushall() {
	flushout(&output);
	flushout(&errout);
}


void
flushout(dest)
	struct output *dest;
	{

	if (dest->buf == NULL || dest->nextc == dest->buf || dest->fd < 0)
		return;
	if (xwrite(dest->fd, dest->buf, dest->nextc - dest->buf) < 0)
		dest->flags |= OUTPUT_ERR;
	dest->nextc = dest->buf;
	dest->nleft = dest->bufsize;
}


void
freestdout() {
	INTOFF;
	if (output.buf) {
		ckfree(output.buf);
		output.buf = NULL;
		output.nleft = 0;
	}
	INTON;
}


#ifdef __STDC__
void
outfmt(struct output *file, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	doformat(file, fmt, ap);
	va_end(ap);
}


void
out1fmt(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	doformat(out1, fmt, ap);
	va_end(ap);
}

void
dbgprintf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	doformat(out2, fmt, ap);
	va_end(ap);
	flushout(out2);
}

void
fmtstr(char *outbuf, int length, const char *fmt, ...)
{
	va_list ap;
	struct output strout;

	va_start(ap, fmt);
	strout.nextc = outbuf;
	strout.nleft = length;
	strout.fd = BLOCK_OUT;
	strout.flags = 0;
	doformat(&strout, fmt, ap);
	outc('\0', &strout);
	if (strout.flags & OUTPUT_ERR)
		outbuf[length - 1] = '\0';
	va_end(ap);
}

#else /* not __STDC__ */

void
outfmt(va_alist)
	va_dcl
	{
	va_list ap;
	struct output *file;
	char *fmt;

	va_start(ap);
	file = va_arg(ap, struct output *);
	fmt = va_arg(ap, char *);
	doformat(file, fmt, ap);
	va_end(ap);
}


void
out1fmt(va_alist)
	va_dcl
	{
	va_list ap;
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap, char *);
	doformat(out1, fmt, ap);
	va_end(ap);
}

void
dbgprintf(va_alist)
	va_dcl
	{
	va_list ap;
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap, char *);
	doformat(out2, fmt, ap);
	va_end(ap);
	flushout(out2);
}

void
fmtstr(va_alist)
	va_dcl
	{
	va_list ap;
	struct output strout;
	char *outbuf;
	int length;
	char *fmt;

	va_start(ap);
	outbuf = va_arg(ap, char *);
	length = va_arg(ap, int);
	fmt = va_arg(ap, char *);
	strout.nextc = outbuf;
	strout.nleft = length;
	strout.fd = BLOCK_OUT;
	strout.flags = 0;
	doformat(&strout, fmt, ap);
	outc('\0', &strout);
	if (strout.flags & OUTPUT_ERR)
		outbuf[length - 1] = '\0';
}
#endif /* __STDC__ */


/*
 * Formatted output.  This routine handles a subset of the printf formats:
 * - Formats supported: d, u, o, X, s, and c.
 * - The x format is also accepted but is treated like X.
 * - The l modifier is accepted.
 * - The - and # flags are accepted; # only works with the o format.
 * - Width and precision may be specified with any format except c.
 * - An * may be given for the width or precision.
 * - The obsolete practice of preceding the width with a zero to get
 *   zero padding is not supported; use the precision field.
 * - A % may be printed by writing %% in the format string.
 */

#define TEMPSIZE 24

#ifdef __STDC__
static const char digit[16] = "0123456789ABCDEF";
#else
static const char digit[17] = "0123456789ABCDEF";
#endif


void
doformat(struct output *dest, const char *f, va_list ap)
{
	register char c;
	char temp[TEMPSIZE];
	int flushleft;
	int sharp;
	int width;
	int prec;
	int islong;
	char *p;
	int sign;
	long l;
	unsigned long num;
	unsigned base;
	int len;
	int size;
	int pad;

	while ((c = *f++) != '\0') {
		if (c != '%') {
			outc(c, dest);
			continue;
		}
		flushleft = 0;
		sharp = 0;
		width = 0;
		prec = -1;
		islong = 0;
		for (;;) {
			if (*f == '-')
				flushleft++;
			else if (*f == '#')
				sharp++;
			else
				break;
			f++;
		}
		if (*f == '*') {
			width = va_arg(ap, int);
			f++;
		} else {
			while (is_digit(*f)) {
				width = 10 * width + digit_val(*f++);
			}
		}
		if (*f == '.') {
			if (*++f == '*') {
				prec = va_arg(ap, int);
				f++;
			} else {
				prec = 0;
				while (is_digit(*f)) {
					prec = 10 * prec + digit_val(*f++);
				}
			}
		}
		if (*f == 'l') {
			islong++;
			f++;
		}
		switch (*f) {
		case 'd':
			if (islong)
				l = va_arg(ap, long);
			else
				l = va_arg(ap, int);
			sign = 0;
			num = l;
			if (l < 0) {
				num = -l;
				sign = 1;
			}
			base = 10;
			goto number;
		case 'u':
			base = 10;
			goto uns_number;
		case 'o':
			base = 8;
			goto uns_number;
		case 'x':
			/* we don't implement 'x'; treat like 'X' */
		case 'X':
			base = 16;
uns_number:	  /* an unsigned number */
			sign = 0;
			if (islong)
				num = va_arg(ap, unsigned long);
			else
				num = va_arg(ap, unsigned int);
number:		  /* process a number */
			p = temp + TEMPSIZE - 1;
			*p = '\0';
			while (num) {
				*--p = digit[num % base];
				num /= base;
			}
			len = (temp + TEMPSIZE - 1) - p;
			if (prec < 0)
				prec = 1;
			if (sharp && *f == 'o' && prec <= len)
				prec = len + 1;
			pad = 0;
			if (width) {
				size = len;
				if (size < prec)
					size = prec;
				size += sign;
				pad = width - size;
				if (flushleft == 0) {
					while (--pad >= 0)
						outc(' ', dest);
				}
			}
			if (sign)
				outc('-', dest);
			prec -= len;
			while (--prec >= 0)
				outc('0', dest);
			while (*p)
				outc(*p++, dest);
			while (--pad >= 0)
				outc(' ', dest);
			break;
		case 's':
			p = va_arg(ap, char *);
			pad = 0;
			if (width) {
				len = strlen(p);
				if (prec >= 0 && len > prec)
					len = prec;
				pad = width - len;
				if (flushleft == 0) {
					while (--pad >= 0)
						outc(' ', dest);
				}
			}
			prec++;
			while (--prec != 0 && *p)
				outc(*p++, dest);
			while (--pad >= 0)
				outc(' ', dest);
			break;
		case 'c':
			c = va_arg(ap, int);
			outc(c, dest);
			break;
		default:
			outc(*f, dest);
			break;
		}
		f++;
	}
}



/*
 * Version of write which resumes after a signal is caught.
 */

int
xwrite(fd, buf, nbytes)
	int fd;
	char *buf;
	int nbytes;
	{
	int ntry;
	int i;
	int n;

	n = nbytes;
	ntry = 0;
	for (;;) {
		i = write(fd, buf, n);
		if (i > 0) {
			if ((n -= i) <= 0)
				return nbytes;
			buf += i;
			ntry = 0;
		} else if (i == 0) {
			if (++ntry > 10)
				return nbytes - n;
		} else if (errno != EINTR) {
			return -1;
		}
	}
}

/*
 * $PchId: output.c,v 1.6 2006/05/22 12:46:03 philip Exp $
 */
