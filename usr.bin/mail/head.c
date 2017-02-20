/*	$NetBSD: head.c,v 1.24 2013/01/16 15:21:42 christos Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)head.c	8.2 (Berkeley) 4/20/95";
#else
__RCSID("$NetBSD: head.c,v 1.24 2013/01/16 15:21:42 christos Exp $");
#endif
#endif /* not lint */

#include "rcv.h"
#include "extern.h"

/*
 * Mail -- a mail program
 *
 * Routines for processing and detecting headlines.
 */

/*
 * Match the given string (cp) against the given template (tp).
 * Return 1 if they match, 0 if they don't
 */
static int
cmatch(const char *cp, const char *tp)
{

	while (*cp && *tp)
		switch (*tp++) {
		case 'a':
			if (!islower((unsigned char)*cp++))
				return 0;
			break;
		case 'A':
			if (!isupper((unsigned char)*cp++))
				return 0;
			break;
		case ' ':
			if (*cp++ != ' ')
				return 0;
			break;
		case '0':
			if (!isdigit((unsigned char)*cp++))
				return 0;
			break;
		case 'O':
			if (*cp != ' ' && !isdigit((unsigned char)*cp))
				return 0;
			cp++;
			break;
		case ':':
			if (*cp++ != ':')
				return 0;
			break;
		case 'N':
			if (*cp++ != '\n')
				return 0;
			break;
		case '+':
			if (*cp != '+' && *cp != '-')
				return 0;
			cp++;
			break;
		}
	if (*cp || *tp)
		return 0;
	return 1;
}

/*
 * Test to see if the passed string is a ctime(3) generated
 * date string as documented in the manual.  The template
 * below is used as the criterion of correctness.
 * Also, we check for a possible trailing time zone using
 * the tmztype template.
 */

/*
 * 'A'	An upper case char
 * 'a'	A lower case char
 * ' '	A space
 * '0'	A digit
 * 'O'	An optional digit or space
 * ':'	A colon
 * 'N'	A new line
 * '+'	A plus or minus sign
 */
static struct cmatch_data {
	size_t		tlen;
	char const	*tdata;
} const	cmatch_data[] = {
#define TSZ(a)	(sizeof(a) - 1), a
	{ TSZ("Aaa Aaa O0 00:00:00 0000") },		/* BSD ctype */
	{ TSZ("Aaa Aaa O0 00:00 0000") },		/* SysV ctype */
	{ TSZ("Aaa Aaa O0 00:00:00 AAA 0000") },	/* BSD tmztype */
	{ TSZ("Aaa Aaa O0 00:00 AAA 0000") },		/* SysV tmztype */
	/*
	 * RFC 822-alike From_ lines do not conform to RFC 4155, but seem to
	 * be used in the wild by UW-imap (MBX format plus)
	 */
	{ TSZ("Aaa Aaa O0 00:00:00 0000 +0000") },	/* RFC822, UT offset */
	/*
	 * RFC 822 with zone spec:
	 *    1. military,
	 *    2. UT,
	 *    3. north america time zone strings
	 * note that 1. is strictly speaking not correct as some letters are
	 * not used
	 */
	{ TSZ("Aaa Aaa O0 00:00:00 0000 A") },
	{ TSZ("Aaa Aaa O0 00:00:00 0000 AA") },
        { TSZ("Aaa Aaa O0 00:00:00 0000 AAA") },
	{ 0, NULL },
};

static int
isdate(const char date[])
{
	static size_t cmatch_minlen = 0;
	struct cmatch_data const *cmdp;
	size_t dl = strlen(date);

	if (cmatch_minlen == 0)
		for (cmdp = cmatch_data; cmdp->tdata != NULL; ++cmdp)
			cmatch_minlen = MIN(cmatch_minlen, cmdp->tlen);

	if (dl < cmatch_minlen)
		return 0;

	for (cmdp = cmatch_data; cmdp->tdata != NULL; ++cmdp)
		if (dl == cmdp->tlen && cmatch(date, cmdp->tdata))
			return 1;

	return 0;
}

static void
fail(const char linebuf[], const char reason[])
{
#ifndef FMT_PROG
	if (debug)
		(void)fprintf(stderr, "\"%s\"\nnot a header because %s\n",
		    linebuf, reason);
#endif
}

/*
 * Collect a liberal (space, tab delimited) word into the word buffer
 * passed.  Also, return a pointer to the next word following that,
 * or NULL if none follow.
 */
static const char *
nextword(const char *wp, char *wbuf)
{
	if (wp == NULL) {
		*wbuf = 0;
		return NULL;
	}
	while (*wp && !is_WSP(*wp)) {
		*wbuf++ = *wp;
		if (*wp++ == '"') {
 			while (*wp && *wp != '"')
 				*wbuf++ = *wp++;
 			if (*wp == '"')
 				*wbuf++ = *wp++;
		}
	}
	*wbuf = '\0';
	wp = skip_WSP(wp);
	if (*wp == '\0')
		return NULL;
	return wp;
}

/*
 * Copy the string on the left into the string on the right
 * and bump the right (reference) string pointer by the length.
 * Thus, dynamically allocate space in the right string, copying
 * the left string into it.
 */
static char *
copyin(const char *src, char **space)
{
	char *cp;
	char *begin;

	begin = cp = *space;
	while ((*cp++ = *src++) != '\0')
		continue;
	*space = cp;
	return begin;
}

/*
 * Split a headline into its useful components.
 * Copy the line into dynamic string space, then set
 * pointers into the copied line in the passed headline
 * structure.  Actually, it scans.
 *
 * XXX - line[], pbuf[], and word[] must be LINESIZE in length or
 * overflow can occur in nextword() or copyin().
 */
PUBLIC void
parse(const char line[], struct headline *hl, char pbuf[])
{
	const char *cp;
	char *sp;
	char word[LINESIZE];

	hl->l_from = NULL;
	hl->l_tty = NULL;
	hl->l_date = NULL;
	cp = line;
	sp = pbuf;
	/*
	 * Skip over "From" first.
	 */
	cp = nextword(cp, word);
	cp = nextword(cp, word);
	if (*word)
		hl->l_from = copyin(word, &sp);
	if (cp != NULL && cp[0] == 't' && cp[1] == 't' && cp[2] == 'y') {
		cp = nextword(cp, word);
		hl->l_tty = copyin(word, &sp);
	}
	if (cp != NULL)
		hl->l_date = copyin(cp, &sp);
}

/*
 * See if the passed line buffer is a mail header.
 * Return true if yes.  Note the extreme pains to
 * accommodate all funny formats.
 */
PUBLIC int
ishead(const char linebuf[])
{
	const char *cp;
	struct headline hl;
	char parbuf[LINESIZE];

	cp = linebuf;
	if (*cp++ != 'F' || *cp++ != 'r' || *cp++ != 'o' || *cp++ != 'm' ||
	    *cp++ != ' ')
		return 0;
	parse(linebuf, &hl, parbuf);
	if (hl.l_from == NULL || hl.l_date == NULL) {
		fail(linebuf, "No from or date field");
		return 0;
	}
	if (!isdate(hl.l_date)) {
		fail(linebuf, "Date field not legal date");
		return 0;
	}
	/*
	 * I guess we got it!
	 */
	return 1;
}
