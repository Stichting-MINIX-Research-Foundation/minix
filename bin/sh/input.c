/*	$NetBSD: input.c,v 1.46 2013/10/30 08:38:40 mrg Exp $	*/

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
static char sccsid[] = "@(#)input.c	8.3 (Berkeley) 6/9/95";
#else
__RCSID("$NetBSD: input.c,v 1.46 2013/10/30 08:38:40 mrg Exp $");
#endif
#endif /* not lint */

#include <stdio.h>	/* defines BUFSIZ */
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
 * This file implements the input routines used by the parser.
 */

#include "shell.h"
#include "redir.h"
#include "syntax.h"
#include "input.h"
#include "output.h"
#include "options.h"
#include "memalloc.h"
#include "error.h"
#include "alias.h"
#include "parser.h"
#include "myhistedit.h"

#define EOF_NLEFT -99		/* value of parsenleft when EOF pushed back */

MKINIT
struct strpush {
	struct strpush *prev;	/* preceding string on stack */
	char *prevstring;
	int prevnleft;
	int prevlleft;
	struct alias *ap;	/* if push was associated with an alias */
};

/*
 * The parsefile structure pointed to by the global variable parsefile
 * contains information about the current file being read.
 */

MKINIT
struct parsefile {
	struct parsefile *prev;	/* preceding file on stack */
	int linno;		/* current line */
	int fd;			/* file descriptor (or -1 if string) */
	int nleft;		/* number of chars left in this line */
	int lleft;		/* number of chars left in this buffer */
	char *nextc;		/* next char in buffer */
	char *buf;		/* input buffer */
	struct strpush *strpush; /* for pushing strings at this level */
	struct strpush basestrpush; /* so pushing one is fast */
};


int plinno = 1;			/* input line number */
int parsenleft;			/* copy of parsefile->nleft */
MKINIT int parselleft;		/* copy of parsefile->lleft */
char *parsenextc;		/* copy of parsefile->nextc */
MKINIT struct parsefile basepf;	/* top level input file */
MKINIT char basebuf[BUFSIZ];	/* buffer for top level input file */
struct parsefile *parsefile = &basepf;	/* current input file */
int init_editline = 0;		/* editline library initialized? */
int whichprompt;		/* 1 == PS1, 2 == PS2 */

STATIC void pushfile(void);
static int preadfd(void);

#ifdef mkinit
INCLUDE <stdio.h>
INCLUDE "input.h"
INCLUDE "error.h"

INIT {
	basepf.nextc = basepf.buf = basebuf;
}

RESET {
	if (exception != EXSHELLPROC)
		parselleft = parsenleft = 0;	/* clear input buffer */
	popallfiles();
}

SHELLPROC {
	popallfiles();
}
#endif


/*
 * Read a line from the script.
 */

char *
pfgets(char *line, int len)
{
	char *p = line;
	int nleft = len;
	int c;

	while (--nleft > 0) {
		c = pgetc_macro();
		if (c == PEOF) {
			if (p == line)
				return NULL;
			break;
		}
		*p++ = c;
		if (c == '\n')
			break;
	}
	*p = '\0';
	return line;
}



/*
 * Read a character from the script, returning PEOF on end of file.
 * Nul characters in the input are silently discarded.
 */

int
pgetc(void)
{
	return pgetc_macro();
}


static int
preadfd(void)
{
	int nr;
	char *buf =  parsefile->buf;
	parsenextc = buf;

retry:
#ifndef SMALL
	if (parsefile->fd == 0 && el) {
		static const char *rl_cp;
		static int el_len;

		if (rl_cp == NULL)
			rl_cp = el_gets(el, &el_len);
		if (rl_cp == NULL)
			nr = el_len == 0 ? 0 : -1;
		else {
			nr = el_len;
			if (nr > BUFSIZ - 8)
				nr = BUFSIZ - 8;
			memcpy(buf, rl_cp, nr);
			if (nr != el_len) {
				el_len -= nr;
				rl_cp += nr;
			} else
				rl_cp = 0;
		}

	} else
#endif
		nr = read(parsefile->fd, buf, BUFSIZ - 8);


	if (nr <= 0) {
                if (nr < 0) {
                        if (errno == EINTR)
                                goto retry;
                        if (parsefile->fd == 0 && errno == EWOULDBLOCK) {
                                int flags = fcntl(0, F_GETFL, 0);
                                if (flags >= 0 && flags & O_NONBLOCK) {
                                        flags &=~ O_NONBLOCK;
                                        if (fcntl(0, F_SETFL, flags) >= 0) {
						out2str("sh: turning off NDELAY mode\n");
                                                goto retry;
                                        }
                                }
                        }
                }
                nr = -1;
	}
	return nr;
}

/*
 * Refill the input buffer and return the next input character:
 *
 * 1) If a string was pushed back on the input, pop it;
 * 2) If an EOF was pushed back (parsenleft == EOF_NLEFT) or we are reading
 *    from a string so we can't refill the buffer, return EOF.
 * 3) If the is more stuff in this buffer, use it else call read to fill it.
 * 4) Process input up to the next newline, deleting nul characters.
 */

int
preadbuffer(void)
{
	char *p, *q;
	int more;
#ifndef SMALL
	int something;
#endif
	char savec;

	if (parsefile->strpush) {
		popstring();
		if (--parsenleft >= 0)
			return (*parsenextc++);
	}
	if (parsenleft == EOF_NLEFT || parsefile->buf == NULL)
		return PEOF;
	flushout(&output);
	flushout(&errout);

again:
	if (parselleft <= 0) {
		if ((parselleft = preadfd()) == -1) {
			parselleft = parsenleft = EOF_NLEFT;
			return PEOF;
		}
	}

	q = p = parsenextc;

	/* delete nul characters */
#ifndef SMALL
	something = 0;
#endif
	for (more = 1; more;) {
		switch (*p) {
		case '\0':
			p++;	/* Skip nul */
			goto check;

		case '\t':
		case ' ':
			break;

		case '\n':
			parsenleft = q - parsenextc;
			more = 0; /* Stop processing here */
			break;

		default:
#ifndef SMALL
			something = 1;
#endif
			break;
		}

		*q++ = *p++;
check:
		if (--parselleft <= 0) {
			parsenleft = q - parsenextc - 1;
			if (parsenleft < 0)
				goto again;
			*q = '\0';
			more = 0;
		}
	}

	savec = *q;
	*q = '\0';

#ifndef SMALL
	if (parsefile->fd == 0 && hist && something) {
		HistEvent he;
		INTOFF;
		history(hist, &he, whichprompt == 1? H_ENTER : H_APPEND,
		    parsenextc);
		INTON;
	}
#endif

	if (vflag) {
		out2str(parsenextc);
		flushout(out2);
	}

	*q = savec;

	return *parsenextc++;
}

/*
 * Undo the last call to pgetc.  Only one character may be pushed back.
 * PEOF may be pushed back.
 */

void
pungetc(void)
{
	parsenleft++;
	parsenextc--;
}

/*
 * Push a string back onto the input at this current parsefile level.
 * We handle aliases this way.
 */
void
pushstring(char *s, int len, void *ap)
{
	struct strpush *sp;

	INTOFF;
/*debugprintf("*** calling pushstring: %s, %d\n", s, len);*/
	if (parsefile->strpush) {
		sp = ckmalloc(sizeof (struct strpush));
		sp->prev = parsefile->strpush;
		parsefile->strpush = sp;
	} else
		sp = parsefile->strpush = &(parsefile->basestrpush);
	sp->prevstring = parsenextc;
	sp->prevnleft = parsenleft;
	sp->prevlleft = parselleft;
	sp->ap = (struct alias *)ap;
	if (ap)
		((struct alias *)ap)->flag |= ALIASINUSE;
	parsenextc = s;
	parsenleft = len;
	INTON;
}

void
popstring(void)
{
	struct strpush *sp = parsefile->strpush;

	INTOFF;
	parsenextc = sp->prevstring;
	parsenleft = sp->prevnleft;
	parselleft = sp->prevlleft;
/*debugprintf("*** calling popstring: restoring to '%s'\n", parsenextc);*/
	if (sp->ap)
		sp->ap->flag &= ~ALIASINUSE;
	parsefile->strpush = sp->prev;
	if (sp != &(parsefile->basestrpush))
		ckfree(sp);
	INTON;
}

/*
 * Set the input to take input from a file.  If push is set, push the
 * old input onto the stack first.
 */

void
setinputfile(const char *fname, int push)
{
	unsigned char magic[4];
	int fd;
	int fd2;

	INTOFF;
	if ((fd = open(fname, O_RDONLY)) < 0)
		error("Can't open %s", fname);

	/* Since the message "Syntax error: "(" unexpected" is not very
	 * helpful, we check if the file starts with the ELF magic to
	 * avoid that message. The first lseek tries to make sure that
	 * we can later rewind the file.
	 */
	if (lseek(fd, 0, SEEK_SET) == 0) {
		if (read(fd, magic, 4) == 4) {
			if (memcmp(magic, "\177ELF", 4) == 0)
				error("Cannot execute ELF binary %s", fname);
		}
		if (lseek(fd, 0, SEEK_SET) != 0)
			error("Cannot rewind the file %s", fname);
	}

	if (fd < 10) {
		fd2 = copyfd(fd, 10, 0);
		close(fd);
		if (fd2 < 0)
			error("Out of file descriptors");
		fd = fd2;
	}
	setinputfd(fd, push);
	INTON;
}


/*
 * Like setinputfile, but takes an open file descriptor.  Call this with
 * interrupts off.
 */

void
setinputfd(int fd, int push)
{
	(void) fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (push) {
		pushfile();
		parsefile->buf = ckmalloc(BUFSIZ);
	}
	if (parsefile->fd > 0)
		close(parsefile->fd);
	parsefile->fd = fd;
	if (parsefile->buf == NULL)
		parsefile->buf = ckmalloc(BUFSIZ);
	parselleft = parsenleft = 0;
	plinno = 1;
}


/*
 * Like setinputfile, but takes input from a string.
 */

void
setinputstring(char *string, int push)
{
	INTOFF;
	if (push)
		pushfile();
	parsenextc = string;
	parselleft = parsenleft = strlen(string);
	parsefile->buf = NULL;
	plinno = 1;
	INTON;
}



/*
 * To handle the "." command, a stack of input files is used.  Pushfile
 * adds a new entry to the stack and popfile restores the previous level.
 */

STATIC void
pushfile(void)
{
	struct parsefile *pf;

	parsefile->nleft = parsenleft;
	parsefile->lleft = parselleft;
	parsefile->nextc = parsenextc;
	parsefile->linno = plinno;
	pf = (struct parsefile *)ckmalloc(sizeof (struct parsefile));
	pf->prev = parsefile;
	pf->fd = -1;
	pf->strpush = NULL;
	pf->basestrpush.prev = NULL;
	parsefile = pf;
}


void
popfile(void)
{
	struct parsefile *pf = parsefile;

	INTOFF;
	if (pf->fd >= 0)
		close(pf->fd);
	if (pf->buf)
		ckfree(pf->buf);
	while (pf->strpush)
		popstring();
	parsefile = pf->prev;
	ckfree(pf);
	parsenleft = parsefile->nleft;
	parselleft = parsefile->lleft;
	parsenextc = parsefile->nextc;
	plinno = parsefile->linno;
	INTON;
}


/*
 * Return to top level.
 */

void
popallfiles(void)
{
	while (parsefile != &basepf)
		popfile();
}



/*
 * Close the file(s) that the shell is reading commands from.  Called
 * after a fork is done.
 *
 * Takes one arg, vfork, which tells it to not modify its global vars
 * as it is still running in the parent.
 *
 * This code is (probably) unnecessary as the 'close on exec' flag is
 * set and should be enough.  In the vfork case it is definitely wrong
 * to close the fds as another fork() may be done later to feed data
 * from a 'here' document into a pipe and we don't want to close the
 * pipe!
 */

void
closescript(int vforked)
{
	if (vforked)
		return;
	popallfiles();
	if (parsefile->fd > 0) {
		close(parsefile->fd);
		parsefile->fd = 0;
	}
}
