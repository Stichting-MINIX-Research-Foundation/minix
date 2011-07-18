/* $NetBSD: tputs.c,v 1.2 2010/02/12 10:36:07 martin Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
__RCSID("$NetBSD: tputs.c,v 1.2 2010/02/12 10:36:07 martin Exp $");

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <term_private.h>
#include <term.h>

/*
 * The following array gives the number of tens of milliseconds per
 * character for each speed as returned by gtty.  Thus since 300
 * baud returns a 7, there are 33.3 milliseconds per char at 300 baud.
 */
static const short tmspc10[] = {
	0, 2000, 1333, 909, 743, 666, 500, 333, 166, 83, 55, 41, 20, 10, 5
};

short ospeed;
char PC;

static int
_ti_calcdelay(const char **str, int affcnt, int *mand)
{
	int i;
	
       	i = 0;
	/* Convert the delay */
	while (isdigit(*(const unsigned char *)*str))
		i = i * 10 + *(*str)++ - '0';
	i *= 10;
	if (*(*str) == '.') {
		(*str)++;
		if (isdigit(*(const unsigned char *)*str))
			i += *(*str) - '0';
		while (isdigit(*(const unsigned char *)*str))
			(*str)++;
	}
	if (*(*str) == '*') {
		(*str)++;
		i *= affcnt;
	} else if (*(*str) == '/') {
		(*str)++;
		if (mand != NULL)
			*mand = 1;
	}
	return i;
}

static void
_ti_outputdelay(int delay, short os, char pc,
    int (*outc)(int, void *), void *args)
{
	int mspc10;

	if (delay < 1 || os < 1 || (size_t)os >= __arraycount(tmspc10))
		return;
	
	mspc10 = tmspc10[os];
	delay += mspc10 / 2;
	for (delay /= mspc10; delay > 0; delay--)
		outc(pc, args);
}

static int
_ti_puts(int dodelay, int os, int pc,
    const char *str, int affcnt, int (*outc)(int, void *), void *args)
{
	int taildelay, delay, mand;

	if (str == NULL)
		return OK;

	taildelay = _ti_calcdelay(&str, affcnt, NULL);

	/* Output the string with embedded delays */
	for (; *str != '\0'; str++) {
		if (str[0] != '$' ||
		    str[1] != '<' ||
		    !(isdigit((const unsigned char)str[2]) || str[2] == '.') ||
		    strchr(str + 3, '>') == NULL)
		{
			outc(*str, args);
		} else {
			str += 2;
			mand = 0;
			delay = _ti_calcdelay(&str, affcnt, &mand);
			if (dodelay != 0 || mand != 0)
				_ti_outputdelay(delay, os, pc, outc, args);
		}
	}

	/* Delay if needed */
	if (dodelay)
		_ti_outputdelay(taildelay, os, pc, outc, args);

	return OK;
}

int
ti_puts(const TERMINAL *term, const char *str, int affcnt,
    int (*outc)(int, void *), void *args)
{
	int dodelay;
	char pc;

	_DIAGASSERT(term != NULL);
	_DIAGASSERT(str != NULL);
	_DIAGASSERT(outc != NULL);
	
	dodelay = (str == t_bell(term) ||
	    str == t_flash_screen(term) ||
	    (t_xon_xoff(term) == 0 && t_padding_baud_rate(term) != 0));

	if (t_pad_char(term) == NULL)
	    pc = '\0';
	else
	    pc = *t_pad_char(term);
	return _ti_puts(dodelay, term->_ospeed, pc,
	    str, affcnt, outc, args);
}

int
ti_putp(const TERMINAL *term, const char *str)
{

	_DIAGASSERT(term != NULL);
	_DIAGASSERT(str != NULL);
	return ti_puts(term, str, 1, (int (*)(int, void *))putchar, NULL);
}

int
tputs(const char *str, int affcnt, int (*outc)(int))
{

	_DIAGASSERT(str != NULL);
	_DIAGASSERT(outc != NULL);
	return _ti_puts(1, ospeed, PC, str, affcnt,
	    (int (*)(int, void *))outc, NULL);
}
	
int
putp(const char *str)
{

	_DIAGASSERT(str != NULL);
	return tputs(str, 1, putchar);
}
