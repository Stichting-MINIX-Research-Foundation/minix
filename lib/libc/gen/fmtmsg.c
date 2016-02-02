/*	$NetBSD: fmtmsg.c,v 1.6 2014/09/18 13:58:20 christos Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fmtmsg.c,v 1.6 2014/09/18 13:58:20 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <fmtmsg.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int	msgverb(const char *);
static const char *	severity2str(int);
static int		writeit(FILE *, unsigned int, const char *,
			    const char *, const char *, const char *,
			    const char *);

#define MM_VERBLABEL		0x01U
#define MM_VERBSEVERITY		0x02U
#define MM_VERBTEXT		0x04U
#define MM_VERBACTION		0x08U
#define MM_VERBTAG		0x10U
#define MM_VERBALL	\
    (MM_VERBLABEL | MM_VERBSEVERITY | MM_VERBTEXT | MM_VERBACTION | \
     MM_VERBTAG)

static const struct keyword {
	size_t			len;	/* strlen(keyword) */
	const char * const	keyword;
} keywords[] = {
	{ 5,	"label"		},	/* log2(MM_VERBLABEL) */
	{ 8,	"severity"	},	/* ... */
	{ 4,	"text"		},
	{ 6,	"action"	},
	{ 3,	"tag"		}	/* log2(MM_VERBTAG) */
};

static const size_t nkeywords = sizeof (keywords) / sizeof (keywords[0]);

/*
 * Convert a colon-separated list of known keywords to a set of MM_VERB*
 * flags, defaulting to `all' if not set, empty, or in presence of unknown
 * keywords.
 */
static unsigned int
msgverb(const char *str)
{
	u_int i;
	unsigned int result;

	if (str == NULL)
		return (MM_VERBALL);

	result = 0;
	while (*str != '\0') {
		for (i = 0; i < nkeywords; i++) {
			if (memcmp(str, keywords[i].keyword, keywords[i].len)
			    == 0 &&
			    (*(str + keywords[i].len) == ':' ||
			     *(str + keywords[i].len) == '\0'))
				break;
		}
		if (i == nkeywords) {
			result = MM_VERBALL;
			break;
		}

		result |= (1 << i);
		if (*(str += keywords[i].len) == ':')
			str++;	/* Advance */
	}
	if (result == 0)
		result = MM_VERBALL;

	return (result);
}

static const char severities[][8] = {
	"",		/* MM_NONE */
	"HALT",
	"ERROR",
	"WARNING",
	"INFO"
};

static const size_t nseverities = sizeof (severities) / sizeof (severities[0]);

/*
 * Returns the string representation associated with the numerical severity
 * value, defaulting to NULL for an unknown value.
 */
static const char *
severity2str(int severity)
{
	const char *result;

	if (severity >= 0 &&
	    (u_int) severity < nseverities)
		result = severities[severity];
	else
		result = NULL;

	return (result);
}

/*
 * Format and write the message to the given stream, selecting those
 * components displayed from msgverb, returning the number of characters
 * written, or a negative value in case of an error.
 */
static int
writeit(FILE *stream, unsigned int which, const char *label,
	const char *sevstr, const char *text, const char *action,
	const char *tag)
{
	int nwritten;

	nwritten = fprintf(stream, "%s%s%s%s%s%s%s%s%s%s%s",
	    ((which & MM_VERBLABEL) && label != MM_NULLLBL) ?
	    label : "",
	    ((which & MM_VERBLABEL) && label != MM_NULLLBL) ?
	    ": " : "",
	    (which & MM_VERBSEVERITY) ?
	    sevstr : "",
	    (which & MM_VERBSEVERITY) ?
	    ": " : "",
	    ((which & MM_VERBTEXT) && text != MM_NULLTXT) ?
	    text : "",
	    ((which & MM_VERBLABEL) && label != MM_NULLLBL) ||
	    ((which & MM_VERBSEVERITY)) ||
	    ((which & MM_VERBTEXT) && text != MM_NULLTXT) ?
	    "\n" : "",
	    ((which & MM_VERBACTION) && action != MM_NULLACT) ?
	    "TO FIX: " : "",
	    ((which & MM_VERBACTION) && action != MM_NULLACT) ?
	    action : "",
	    ((which & MM_VERBACTION) && label != MM_NULLACT) ?
	    " " : "",
	    ((which & MM_VERBTAG) && tag != MM_NULLTAG) ?
	    tag : "",
	    ((which & MM_VERBACTION) && action != MM_NULLACT) ||
	    ((which & MM_VERBTAG) && tag != MM_NULLTAG) ?
	    "\n" : "");

	return (nwritten);
}

int
fmtmsg(long classification, const char *label, int severity,
	const char *text, const char *action, const char *tag)
{
	FILE *console;
	const char *p, *sevstr;
	int result;

	/* Validate label constraints, if not null. */
	if (label != MM_NULLLBL) {
		/*
		 * Two fields, separated by a colon.  The first field is up to
		 * 10 bytes, the second is up to 14 bytes.
		 */
		p = strchr(label, ':');
		if (p ==  NULL || p - label > 10 || strlen(p + 1) > 14)
			return (MM_NOTOK);
	}
	/* Validate severity argument. */
	if ((sevstr = severity2str(severity)) == NULL)
		return (MM_NOTOK);

	/*
	 * Fact in search for a better place: XSH5 does not define any
	 * functionality for `classification' bits other than the display
	 * subclassification.
	 */

	result = 0;

	if (classification & MM_PRINT) {
		if (writeit(stderr, msgverb(getenv("MSGVERB")),
		    label, sevstr, text, action, tag) < 0)
			result |= MM_NOMSG;
	}
	/* Similar to MM_PRINT but ignoring $MSGVERB. */
	if (classification & MM_CONSOLE) {
		if ((console = fopen(_PATH_CONSOLE, "we")) != NULL) {
			if (writeit(console, MM_VERBALL,
			    label, sevstr, text, action, tag) < 0)
				result |= MM_NOCON;
			/*
			 * Ignore result: does not constitute ``generate a
			 * console message.''
			 */
			(void)fclose(console);
		} else {
			result |= MM_NOCON;
		}
	}

	if (result == (MM_NOMSG | MM_NOCON))
		result = MM_NOTOK;

	return (result == 0 ? MM_OK : result);
}
