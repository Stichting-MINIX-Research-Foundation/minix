/*	$NetBSD: gencat.c,v 1.36 2013/11/27 17:38:11 apb Exp $	*/

/*
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: gencat.c,v 1.36 2013/11/27 17:38:11 apb Exp $");
#endif

/***********************************************************
Copyright 1990, by Alfalfa Software Incorporated, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that Alfalfa's name not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

ALPHALPHA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
ALPHALPHA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

If you make any modifications, bugfixes or other changes to this software
we'd appreciate it if you could send a copy to us so we can keep things
up-to-date.  Many thanks.
				Kee Hinckley
				Alfalfa Software, Inc.
				267 Allston St., #3
				Cambridge, MA 02139  USA
				nazgul@alfalfa.com

******************************************************************/

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#define _NLS_PRIVATE

#include <sys/types.h>
#include <sys/queue.h>

#include <netinet/in.h>	/* Needed by arpa/inet.h on NetBSD */
#include <arpa/inet.h>	/* Needed for htonl() on POSIX systems */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <nl_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef NL_SETMAX
#define NL_SETMAX 255
#endif
#ifndef NL_MSGMAX
#define NL_MSGMAX 2048
#endif

struct _msgT {
	long    msgId;
	char   *str;
        LIST_ENTRY(_msgT) entries;
};

struct _setT {
	long    setId;
        LIST_HEAD(msghead, _msgT) msghead;
        LIST_ENTRY(_setT) entries;
};

static LIST_HEAD(sethead, _setT) sethead = LIST_HEAD_INITIALIZER(sethead);
static struct _setT *curSet;

static const char *curfile;
static char *curline = NULL;
static long lineno = 0;

static	char   *cskip(char *);
__dead static	void	error(const char *);
static	char   *get_line(int);
static	char   *getmsg(int, char *, char);
static	void	warning(const char *, const char *);
static	char   *wskip(char *);
static	char   *xstrdup(const char *);
static	void   *xmalloc(size_t);
static	void   *xrealloc(void *, size_t);

static void	MCParse(int fd);
static void	MCReadCat(int fd);
static void	MCWriteCat(int fd);
static void	MCDelMsg(int msgId);
static void	MCAddMsg(int msgId, const char *msg);
static void	MCAddSet(int setId);
static void	MCDelSet(int setId);
__dead static void	usage(void);

#define CORRUPT			"corrupt message catalog"
#define NOMEMORY		"out of memory"

static void
usage(void)
{
	fprintf(stderr, "usage: %s catfile [msgfile|- ...]\n", getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	int     ofd, ifd;
	char   *catfile = NULL;
	int     c;
	int	updatecat = 0;

	while ((c = getopt(argc, argv, "")) != -1) {
		switch (c) {
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		/* NOTREACHED */
	}
	catfile = *argv++;

	if ((catfile[0] == '-') && (catfile[1] == '\0')) {
		ofd = STDOUT_FILENO;
	} else {
		ofd = open(catfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
		if (ofd < 0) {
			if (errno == EEXIST) {
				if ((ofd = open(catfile, O_RDWR)) < 0) {
					err(1, "Unable to open %s", catfile);
					/* NOTREACHED */
				}
			} else {
				err(1, "Unable to create new %s", catfile);
				/* NOTREACHED */
			}
			curfile = catfile;
			updatecat = 1;
			MCReadCat(ofd);
			if (lseek(ofd, (off_t)0, SEEK_SET) == (off_t)-1) {
				err(1, "Unable to seek on %s", catfile);
				/* NOTREACHED */
			}
		}
	}

	if (argc < 2 || (((*argv)[0] == '-') && ((*argv)[1] == '\0'))) {
		if (argc > 2)
			usage();
			/* NOTREACHED */
		MCParse(STDIN_FILENO);
	} else {
		for (; *argv; argv++) {
			if ((ifd = open(*argv, O_RDONLY)) < 0)
				err(1, "Unable to read %s", *argv);
			curfile = *argv;
			lineno = 0;
			MCParse(ifd);
			close(ifd);
		}
	}

	if (updatecat) {
		if (ftruncate(ofd, 0) != 0) {
			err(1, "Unable to truncate %s", catfile);
			/* NOTREACHED */
		}
	}

	MCWriteCat(ofd);
	exit(0);
}

static void
warning(const char *cptr, const char *msg)
{
	if (lineno) {
		fprintf(stderr, "%s: %s on line %ld, %s\n",
			getprogname(), msg, lineno, curfile);
		fprintf(stderr, "%s\n", curline);
		if (cptr) {
			char   *tptr;
			for (tptr = curline; tptr < cptr; ++tptr)
				putc(' ', stderr);
			fprintf(stderr, "^\n");
		}
	} else {
		fprintf(stderr, "%s: %s, %s\n", getprogname(), msg, curfile);
	}
}

static void
error(const char *msg)
{
	warning(NULL, msg);
	exit(1);
}

static void *
xmalloc(size_t len)
{
	void   *p;

	if ((p = malloc(len)) == NULL)
		errx(1, NOMEMORY);
	return (p);
}

static void *
xrealloc(void *ptr, size_t size)
{
	if ((ptr = realloc(ptr, size)) == NULL)
		errx(1, NOMEMORY);
	return (ptr);
}

static char *
xstrdup(const char *str)
{
	char *nstr;

	if ((nstr = strdup(str)) == NULL)
		errx(1, NOMEMORY);
	return (nstr);
}

static char *
get_line(int fd)
{
	static long curlen = BUFSIZ;
	static char buf[BUFSIZ], *bptr = buf, *bend = buf;
	char   *cptr, *cend;
	long    buflen;

	if (!curline) {
		curline = xmalloc(curlen);
	}
	++lineno;

	cptr = curline;
	cend = curline + curlen;
	for (;;) {
		for (; bptr < bend && cptr < cend; ++cptr, ++bptr) {
			if (*bptr == '\n') {
				*cptr = '\0';
				++bptr;
				return (curline);
			} else
				*cptr = *bptr;
		}
		if (cptr == cend) {
			cptr = curline = xrealloc(curline, curlen *= 2);
			cend = curline + curlen;
		}
		if (bptr == bend) {
			buflen = read(fd, buf, BUFSIZ);
			if (buflen <= 0) {
				if (cptr > curline) {
					*cptr = '\0';
					return (curline);
				}
				return (NULL);
			}
			bend = buf + buflen;
			bptr = buf;
		}
	}
}

static char *
wskip(char *cptr)
{
	if (!*cptr || !isspace((unsigned char) *cptr)) {
		warning(cptr, "expected a space");
		return (cptr);
	}
	while (*cptr && isspace((unsigned char) *cptr))
		++cptr;
	return (cptr);
}

static char *
cskip(char *cptr)
{
	if (!*cptr || isspace((unsigned char) *cptr)) {
		warning(cptr, "wasn't expecting a space");
		return (cptr);
	}
	while (*cptr && !isspace((unsigned char) *cptr))
		++cptr;
	return (cptr);
}

static char *
getmsg(int fd, char *cptr, char quote)
{
	static char *msg = NULL;
	static size_t msglen = 0;
	size_t  clen, i;
	int     in_quote = 0;
	char   *tptr;

	if (quote && *cptr == quote) {
		++cptr;
		in_quote = 1;
	} 

	clen = strlen(cptr) + 1;
	if (clen > msglen) {
		if (msglen)
			msg = xrealloc(msg, clen);
		else
			msg = xmalloc(clen);
		msglen = clen;
	}
	tptr = msg;

	while (*cptr) {
		if (quote && *cptr == quote) {
			char   *tmp;
			tmp = cptr + 1;
			if (!in_quote) {
				/* XXX hard error? */
				warning(cptr, "unexpected quote character, ignoring");
				*tptr++ = *cptr++;
			} else {
				cptr++;
				/* don't use wskip() */
				while (*cptr && isspace((unsigned char) *cptr))
#ifndef _BACKWARDS_COMPAT
					cptr++;
#else
					*tptr++ = *cptr++;
#endif
				/* XXX hard error? */
				if (*cptr)
					warning(tmp, "unexpected extra characters, ignoring");
				in_quote = 0;
#ifndef _BACKWARDS_COMPAT
				break;
#endif
			}
		} else {
			if (*cptr == '\\') {
				++cptr;
				switch (*cptr) {
				case '\0':
					cptr = get_line(fd);
					if (!cptr)
						error("premature end of file");
					msglen += strlen(cptr);
					i = tptr - msg;
					msg = xrealloc(msg, msglen);
					tptr = msg + i;
					break;
				case 'n':
					*tptr++ = '\n';
					++cptr;
					break;
				case 't':
					*tptr++ = '\t';
					++cptr;
					break;
				case 'v':
					*tptr++ = '\v';
					++cptr;
					break;
				case 'b':
					*tptr++ = '\b';
					++cptr;
					break;
				case 'r':
					*tptr++ = '\r';
					++cptr;
					break;
				case 'f':
					*tptr++ = '\f';
					++cptr;
					break;
				case '\\':
					*tptr++ = '\\';
					++cptr;
					break;
				default:
					if (quote && *cptr == quote) {
						*tptr++ = *cptr++;
					} else if (isdigit((unsigned char) *cptr)) {
						*tptr = 0;
						for (i = 0; i < 3; ++i) {
							if (!isdigit((unsigned char) *cptr))
								break;
							if (*cptr > '7')
								warning(cptr, "octal number greater than 7?!");
							*tptr *= 8;
							*tptr += (*cptr - '0');
							++cptr;
						}
					} else {
						warning(cptr, "unrecognized escape sequence");
					}
					break;
				}
			} else {
				*tptr++ = *cptr++;
			}
		}
	}

	if (in_quote)
		warning(cptr, "unterminated quoted message, ignoring");

	*tptr = '\0';
	return (msg);
}

static void
MCParse(int fd)
{
	char   *cptr, *str;
	int	msgid = 0;
	int     setid = 0;
	char    quote = 0;

	while ((cptr = get_line(fd))) {
		if (*cptr == '$') {
			++cptr;
			if (strncmp(cptr, "set", 3) == 0) {
				cptr += 3;
				cptr = wskip(cptr);
				setid = atoi(cptr);
				MCAddSet(setid);
				msgid = 0;
			} else if (strncmp(cptr, "delset", 6) == 0) {
				cptr += 6;
				cptr = wskip(cptr);
				setid = atoi(cptr);
				MCDelSet(setid);
			} else if (strncmp(cptr, "quote", 5) == 0) {
				cptr += 5;
				if (!*cptr)
					quote = 0;
				else {
					cptr = wskip(cptr);
					if (!*cptr)
						quote = 0;
					else
						quote = *cptr;
				}
			} else if (isspace((unsigned char) *cptr)) {
				;
			} else {
				if (*cptr) {
					cptr = wskip(cptr);
					if (*cptr)
						warning(cptr, "unrecognized line");
				}
			}
		} else {
			/*
			 * First check for (and eat) empty lines....
			 */
			if (!*cptr)
				continue;
			/*
			 * We have a digit? Start of a message. Else,
			 * syntax error.
			 */
			if (isdigit((unsigned char) *cptr)) {
				msgid = atoi(cptr);
				cptr = cskip(cptr);
				if (*cptr) {
					cptr = wskip(cptr);
					if (!*cptr) {
						MCAddMsg(msgid, "");
						continue;
					}
				}
			} else {
				warning(cptr, "neither blank line nor start of a message id");
				continue;
			}
			/*
			 * If no set directive specified, all messages
			 * shall be in default message set NL_SETD.
			 */
			if (setid == 0) {
				setid = NL_SETD;
				MCAddSet(setid);
			}
			/*
			 * If we have a message ID, but no message,
			 * then this means "delete this message id
			 * from the catalog".
			 */
			if (!*cptr) {
				MCDelMsg(msgid);
			} else {
				str = getmsg(fd, cptr, quote);
				MCAddMsg(msgid, str);
			}
		}
	}
}

static void
MCReadCat(int fd)
{
	void   *msgcat;		/* message catalog data */
	struct _nls_cat_hdr cat_hdr;
	struct _nls_set_hdr *set_hdr;
	struct _nls_msg_hdr *msg_hdr;
	char   *strings;
	ssize_t	n;
	int	m, s;
	int	msgno, setno;

	n = read(fd, &cat_hdr, sizeof(cat_hdr));
	if (n < (ssize_t)sizeof(cat_hdr)) {
		if (n == 0)
			return;		/* empty file */
		else if (n == -1)
			err(1, "header read");
		else
			errx(1, CORRUPT);
	}
	if (ntohl((uint32_t)cat_hdr.__magic) != _NLS_MAGIC)
		errx(1, "%s: bad magic number (%#x)", CORRUPT, cat_hdr.__magic);

	cat_hdr.__mem = ntohl(cat_hdr.__mem);

	cat_hdr.__nsets = ntohl(cat_hdr.__nsets);
	cat_hdr.__msg_hdr_offset = ntohl(cat_hdr.__msg_hdr_offset);
	cat_hdr.__msg_txt_offset = ntohl(cat_hdr.__msg_txt_offset);
	if ((cat_hdr.__mem < 0) ||
	    (cat_hdr.__msg_hdr_offset < 0) ||
	    (cat_hdr.__msg_txt_offset < 0) ||
	    (cat_hdr.__mem < (int32_t)(cat_hdr.__nsets * sizeof(struct _nls_set_hdr))) ||
	    (cat_hdr.__mem < cat_hdr.__msg_hdr_offset) ||
	    (cat_hdr.__mem < cat_hdr.__msg_txt_offset))
		errx(1, "%s: catalog header", CORRUPT);

	msgcat = xmalloc(cat_hdr.__mem);

	n = read(fd, msgcat, cat_hdr.__mem);
	if (n < cat_hdr.__mem) {
		if (n == -1)
			err(1, "data read");
		else
			errx(1, CORRUPT);
	}

	set_hdr = (struct _nls_set_hdr *)msgcat;
	msg_hdr = (struct _nls_msg_hdr *)((char *)msgcat +
	    cat_hdr.__msg_hdr_offset);
	strings = (char *)msgcat + cat_hdr.__msg_txt_offset;

	setno = 0;
	for (s = 0; s < cat_hdr.__nsets; s++, set_hdr++) {
		set_hdr->__setno = ntohl(set_hdr->__setno);
		if (set_hdr->__setno < setno)
			errx(1, "%s: bad set number (%d)",
		       	     CORRUPT, set_hdr->__setno);
		setno = set_hdr->__setno;

		MCAddSet(setno);

		set_hdr->__nmsgs = ntohl(set_hdr->__nmsgs);
		set_hdr->__index = ntohl(set_hdr->__index);
		if (set_hdr->__nmsgs < 0 || set_hdr->__index < 0)
			errx(1, "%s: set header", CORRUPT);

		/* Get the data */
		msgno = 0;
		for (m = 0; m < set_hdr->__nmsgs; m++, msg_hdr++) {
			msg_hdr->__msgno = ntohl(msg_hdr->__msgno);
			msg_hdr->__offset = ntohl(msg_hdr->__offset);
			if (msg_hdr->__msgno < msgno)
				errx(1, "%s: bad message number (%d)",
				     CORRUPT, msg_hdr->__msgno);
		        if ((msg_hdr->__offset < 0) ||
			    ((strings + msg_hdr->__offset) >
			     ((char *)msgcat + cat_hdr.__mem)))
				errx(1, "%s: message header", CORRUPT);

			msgno = msg_hdr->__msgno;
			MCAddMsg(msgno, strings + msg_hdr->__offset);
		}
	}
	free(msgcat);
}

/*
 * Write message catalog.
 *
 * The message catalog is first converted from its internal to its
 * external representation in a chunk of memory allocated for this
 * purpose.  Then the completed catalog is written.  This approach
 * avoids additional housekeeping variables and/or a lot of seeks
 * that would otherwise be required.
 */
static void
MCWriteCat(int fd)
{
	int     nsets;		/* number of sets */
	int     nmsgs;		/* number of msgs */
	int     string_size;	/* total size of string pool */
	int     msgcat_size;	/* total size of message catalog */
	void   *msgcat;		/* message catalog data */
	struct _nls_cat_hdr *cat_hdr;
	struct _nls_set_hdr *set_hdr;
	struct _nls_msg_hdr *msg_hdr;
	char   *strings;
	struct _setT *set;
	struct _msgT *msg;
	int     msg_index;
	int     msg_offset;

	/* determine number of sets, number of messages, and size of the
	 * string pool */
	nsets = 0;
	nmsgs = 0;
	string_size = 0;

	LIST_FOREACH(set, &sethead, entries) {
		nsets++;

		LIST_FOREACH(msg, &set->msghead, entries) {
			nmsgs++;
			string_size += strlen(msg->str) + 1;
		}
	}

#ifdef DEBUG
	printf("number of sets: %d\n", nsets);
	printf("number of msgs: %d\n", nmsgs);
	printf("string pool size: %d\n", string_size);
#endif

	/* determine size and then allocate buffer for constructing external
	 * message catalog representation */
	msgcat_size = sizeof(struct _nls_cat_hdr)
	    + (nsets * sizeof(struct _nls_set_hdr))
	    + (nmsgs * sizeof(struct _nls_msg_hdr))
	    + string_size;

	msgcat = xmalloc(msgcat_size);
	memset(msgcat, '\0', msgcat_size);

	/* fill in msg catalog header */
	cat_hdr = (struct _nls_cat_hdr *) msgcat;
	cat_hdr->__magic = htonl(_NLS_MAGIC);
	cat_hdr->__nsets = htonl(nsets);
	cat_hdr->__mem = htonl(msgcat_size - sizeof(struct _nls_cat_hdr));
	cat_hdr->__msg_hdr_offset =
	    htonl(nsets * sizeof(struct _nls_set_hdr));
	cat_hdr->__msg_txt_offset =
	    htonl(nsets * sizeof(struct _nls_set_hdr) +
	    nmsgs * sizeof(struct _nls_msg_hdr));

	/* compute offsets for set & msg header tables and string pool */
	set_hdr = (struct _nls_set_hdr *) ((char *) msgcat +
	    sizeof(struct _nls_cat_hdr));
	msg_hdr = (struct _nls_msg_hdr *) ((char *) msgcat +
	    sizeof(struct _nls_cat_hdr) +
	    nsets * sizeof(struct _nls_set_hdr));
	strings = (char *) msgcat +
	    sizeof(struct _nls_cat_hdr) +
	    nsets * sizeof(struct _nls_set_hdr) +
	    nmsgs * sizeof(struct _nls_msg_hdr);

	msg_index = 0;
	msg_offset = 0;
	LIST_FOREACH(set, &sethead, entries) {

		nmsgs = 0;
		LIST_FOREACH(msg, &set->msghead, entries) {
			int32_t     msg_len = strlen(msg->str) + 1;

			msg_hdr->__msgno = htonl(msg->msgId);
			msg_hdr->__msglen = htonl(msg_len);
			msg_hdr->__offset = htonl(msg_offset);

			memcpy(strings, msg->str, msg_len);
			strings += msg_len;
			msg_offset += msg_len;

			nmsgs++;
			msg_hdr++;
		}

		set_hdr->__setno = htonl(set->setId);
		set_hdr->__nmsgs = htonl(nmsgs);
		set_hdr->__index = htonl(msg_index);
		msg_index += nmsgs;
		set_hdr++;
	}

	/* write out catalog.  XXX: should this be done in small chunks? */
	write(fd, msgcat, msgcat_size);
}

static void
MCAddSet(int setId)
{
	struct _setT *p, *q;

	if (setId <= 0) {
		error("setId's must be greater than zero");
		/* NOTREACHED */
	}
	if (setId > NL_SETMAX) {
		error("setId exceeds limit");
		/* NOTREACHED */
	}

	p = LIST_FIRST(&sethead);
	q = NULL;
	for (; p != NULL && p->setId < setId; q = p, p = LIST_NEXT(p, entries))
		continue;

	if (p && p->setId == setId) {
		;
	} else {
		p = xmalloc(sizeof(struct _setT));
		memset(p, '\0', sizeof(struct _setT));
		LIST_INIT(&p->msghead);

		p->setId = setId;

		if (q == NULL) {
			LIST_INSERT_HEAD(&sethead, p, entries);
		} else {
			LIST_INSERT_AFTER(q, p, entries);
		}
	}

	curSet = p;
}

static void
MCAddMsg(int msgId, const char *str)
{
	struct _msgT *p, *q;

	if (!curSet)
		error("can't specify a message when no set exists");

	if (msgId <= 0) {
		error("msgId's must be greater than zero");
		/* NOTREACHED */
	}
	if (msgId > NL_MSGMAX) {
		error("msgID exceeds limit");
		/* NOTREACHED */
	}

	p = LIST_FIRST(&curSet->msghead);
	q = NULL;
	for (; p != NULL && p->msgId < msgId; q = p, p = LIST_NEXT(p, entries))
		continue;

	if (p && p->msgId == msgId) {
		free(p->str);
	} else {
		p = xmalloc(sizeof(struct _msgT));
		memset(p, '\0', sizeof(struct _msgT));

		if (q == NULL) {
			LIST_INSERT_HEAD(&curSet->msghead, p, entries);
		} else {
			LIST_INSERT_AFTER(q, p, entries);
		}
	}

	p->msgId = msgId;
	p->str = xstrdup(str);
}

static void
MCDelSet(int setId)
{
	struct _setT *set;
	struct _msgT *msg;

	if (setId <= 0) {
		error("setId's must be greater than zero");
		/* NOTREACHED */
	}
	if (setId > NL_SETMAX) {
		error("setId exceeds limit");
		/* NOTREACHED */
	}

	set = LIST_FIRST(&sethead);
	for (; set != NULL && set->setId < setId; set = LIST_NEXT(set, entries))
		continue;

	if (set && set->setId == setId) {
		LIST_REMOVE(set, entries);
		while ((msg = LIST_FIRST(&set->msghead)) != NULL) {
			LIST_REMOVE(msg, entries);
			free(msg->str);
			free(msg);
		}
		free(set);
		return;
	}
	warning(NULL, "specified set doesn't exist");
}

static void
MCDelMsg(int msgId)
{
	struct _msgT *msg;

	if (!curSet)
		error("you can't delete a message before defining the set");

	msg = LIST_FIRST(&curSet->msghead);
	for (; msg != NULL && msg->msgId < msgId; msg = LIST_NEXT(msg, entries))
		continue;

	if (msg && msg->msgId == msgId) {
		LIST_REMOVE(msg, entries);
		free(msg->str);
		free(msg);
		return;
	}
	warning(NULL, "specified msg doesn't exist");
}
