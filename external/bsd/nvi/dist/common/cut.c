/*	$NetBSD: cut.c,v 1.4 2014/01/26 21:43:45 christos Exp $ */
/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: cut.c,v 10.18 2001/06/25 15:19:09 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:09 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: cut.c,v 1.4 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void	cb_rotate __P((SCR *));

/*
 * cut --
 *	Put a range of lines/columns into a TEXT buffer.
 *
 * There are two buffer areas, both found in the global structure.  The first
 * is the linked list of all the buffers the user has named, the second is the
 * unnamed buffer storage.  There is a pointer, too, which is the current
 * default buffer, i.e. it may point to the unnamed buffer or a named buffer
 * depending on into what buffer the last text was cut.  Logically, in both
 * delete and yank operations, if the user names a buffer, the text is cut
 * into it.  If it's a delete of information on more than a single line, the
 * contents of the numbered buffers are rotated up one, the contents of the
 * buffer named '9' are discarded, and the text is cut into the buffer named
 * '1'.  The text is always cut into the unnamed buffer.
 *
 * In all cases, upper-case buffer names are the same as lower-case names,
 * with the exception that they cause the buffer to be appended to instead
 * of replaced.  Note, however, that if text is appended to a buffer, the
 * default buffer only contains the appended text, not the entire contents
 * of the buffer.
 *
 * !!!
 * The contents of the default buffer would disappear after most operations
 * in historic vi.  It's unclear that this is useful, so we don't bother.
 *
 * When users explicitly cut text into the numeric buffers, historic vi became
 * genuinely strange.  I've never been able to figure out what was supposed to
 * happen.  It behaved differently if you deleted text than if you yanked text,
 * and, in the latter case, the text was appended to the buffer instead of
 * replacing the contents.  Hopefully it's not worth getting right, and here
 * we just treat the numeric buffers like any other named buffer.
 *
 * PUBLIC: int cut __P((SCR *, ARG_CHAR_T *, MARK *, MARK *, int));
 */
int
cut(SCR *sp, ARG_CHAR_T *namep, MARK *fm, MARK *tm, int flags)
{
	CB *cbp;
	ARG_CHAR_T name = '\0';
	db_recno_t lno;
	int append, copy_one, copy_def;

	/*
	 * If the user specified a buffer, put it there.  (This may require
	 * a copy into the numeric buffers.  We do the copy so that we don't
	 * have to reference count and so we don't have to deal with things
	 * like appends to buffers that are used multiple times.)
	 *
	 * Otherwise, if it's supposed to be put in a numeric buffer (usually
	 * a delete) put it there.  The rules for putting things in numeric
	 * buffers were historically a little strange.  There were three cases.
	 *
	 *	1: Some motions are always line mode motions, which means
	 *	   that the cut always goes into the numeric buffers.
	 *	2: Some motions aren't line mode motions, e.g. d10w, but
	 *	   can cross line boundaries.  For these commands, if the
	 *	   cut crosses a line boundary, it goes into the numeric
	 *	   buffers.  This includes most of the commands.
	 *	3: Some motions aren't line mode motions, e.g. d`<char>,
	 *	   but always go into the numeric buffers, regardless.  This
	 *	   was the commands: % ` / ? ( ) N n { } -- and nvi adds ^A.
	 *
	 * Otherwise, put it in the unnamed buffer.
	 */
	append = copy_one = copy_def = 0;
	if (namep != NULL) {
		name = *namep;
		if (LF_ISSET(CUT_NUMREQ) || (LF_ISSET(CUT_NUMOPT) &&
		    (LF_ISSET(CUT_LINEMODE) || fm->lno != tm->lno))) {
			copy_one = 1;
			cb_rotate(sp);
		}
		if ((append = ISUPPER(name))) {
			if (!copy_one)
				copy_def = 1;
			name = TOLOWER(name);
		}
namecb:		CBNAME(sp, cbp, name);
	} else if (LF_ISSET(CUT_NUMREQ) || (LF_ISSET(CUT_NUMOPT) &&
	    (LF_ISSET(CUT_LINEMODE) || fm->lno != tm->lno))) {
		name = L('1');
		cb_rotate(sp);
		goto namecb;
	} else
		cbp = &sp->wp->dcb_store;

copyloop:
	/*
	 * If this is a new buffer, create it and add it into the list.
	 * Otherwise, if it's not an append, free its current contents.
	 */
	if (cbp == NULL) {
		CALLOC_RET(sp, cbp, CB *, 1, sizeof(CB));
		cbp->name = name;
		TAILQ_INIT(&cbp->textq);
		LIST_INSERT_HEAD(&sp->wp->cutq, cbp, q);
	} else if (!append) {
		text_lfree(&cbp->textq);
		cbp->len = 0;
		cbp->flags = 0;
	}


	/* In line mode, it's pretty easy, just cut the lines. */
	if (LF_ISSET(CUT_LINEMODE)) {
		cbp->flags |= CB_LMODE;
		for (lno = fm->lno; lno <= tm->lno; ++lno)
			if (cut_line(sp, lno, 0, ENTIRE_LINE, cbp))
				goto cut_line_err;
	} else {
		/*
		 * Get the first line.  A length of ENTIRE_LINE causes cut_line
		 * to cut from the MARK to the end of the line.
		 */
		if (cut_line(sp, fm->lno, fm->cno, fm->lno != tm->lno ?
		    ENTIRE_LINE : (tm->cno - fm->cno) + 1, cbp))
			goto cut_line_err;

		/* Get the intermediate lines. */
		for (lno = fm->lno; ++lno < tm->lno;)
			if (cut_line(sp, lno, 0, ENTIRE_LINE, cbp))
				goto cut_line_err;

		/* Get the last line. */
		if (tm->lno != fm->lno &&
		    cut_line(sp, lno, 0, tm->cno + 1, cbp))
			goto cut_line_err;
	}

	append = 0;		/* Only append to the named buffer. */
	sp->wp->dcbp = cbp;	/* Repoint the default buffer on each pass. */

	if (copy_one) {		/* Copy into numeric buffer 1. */
		name = L('1');
		CBNAME(sp, cbp, name);
		copy_one = 0;
		goto copyloop;
	}
	if (copy_def) {		/* Copy into the default buffer. */
		cbp = &sp->wp->dcb_store;
		copy_def = 0;
		goto copyloop;
	}
	return (0);

cut_line_err:	
	text_lfree(&cbp->textq);
	cbp->len = 0;
	cbp->flags = 0;
	return (1);
}

/*
 * cb_rotate --
 *	Rotate the numbered buffers up one.
 */
static void
cb_rotate(SCR *sp)
{
	CB *cbp, *del_cbp;

	del_cbp = NULL;
	LIST_FOREACH(cbp, &sp->wp->cutq, q)
		switch(cbp->name) {
		case L('1'):
			cbp->name = L('2');
			break;
		case L('2'):
			cbp->name = L('3');
			break;
		case L('3'):
			cbp->name = L('4');
			break;
		case L('4'):
			cbp->name = L('5');
			break;
		case L('5'):
			cbp->name = L('6');
			break;
		case L('6'):
			cbp->name = L('7');
			break;
		case L('7'):
			cbp->name = L('8');
			break;
		case L('8'):
			cbp->name = L('9');
			break;
		case L('9'):
			del_cbp = cbp;
			break;
		}
	if (del_cbp != NULL) {
		LIST_REMOVE(del_cbp, q);
		text_lfree(&del_cbp->textq);
		free(del_cbp);
	}
}

/*
 * cut_line --
 *	Cut a portion of a single line.
 *
 * PUBLIC: int cut_line __P((SCR *, db_recno_t, size_t, size_t, CB *));
 */
int
cut_line(SCR *sp, db_recno_t lno, size_t fcno, size_t clen, CB *cbp)
{
	TEXT *tp;
	size_t len;
	CHAR_T *p;

	/* Get the line. */
	if (db_get(sp, lno, DBG_FATAL, &p, &len))
		return (1);

	/* Create a TEXT structure that can hold the entire line. */
	if ((tp = text_init(sp, NULL, 0, len)) == NULL)
		return (1);

	/*
	 * If the line isn't empty and it's not the entire line,
	 * copy the portion we want, and reset the TEXT length.
	 */
	if (len != 0) {
		if (clen == ENTIRE_LINE)
			clen = len - fcno;
		MEMCPYW(tp->lb, p + fcno, clen);
		tp->len = clen;
	}

	/* Append to the end of the cut buffer. */
	TAILQ_INSERT_TAIL(&cbp->textq, tp, q);
	cbp->len += tp->len;

	return (0);
}

/*
 * cut_close --
 *	Discard all cut buffers.
 *
 * PUBLIC: void cut_close __P((WIN *));
 */
void
cut_close(WIN *wp)
{
	CB *cbp;

	/* Free cut buffer list. */
	while ((cbp = LIST_FIRST(&wp->cutq)) != NULL) {
		if (!TAILQ_EMPTY(&cbp->textq))
			text_lfree(&cbp->textq);
		LIST_REMOVE(cbp, q);
		free(cbp);
	}

	/* Free default cut storage. */
	cbp = &wp->dcb_store;
	if (!TAILQ_EMPTY(&cbp->textq))
		text_lfree(&cbp->textq);
}

/*
 * text_init --
 *	Allocate a new TEXT structure.
 *
 * PUBLIC: TEXT *text_init __P((SCR *, const CHAR_T *, size_t, size_t));
 */
TEXT *
text_init(SCR *sp, const CHAR_T *p, size_t len, size_t total_len)
{
	TEXT *tp;

	CALLOC(sp, tp, TEXT *, 1, sizeof(TEXT));
	if (tp == NULL)
		return (NULL);
	/* ANSI C doesn't define a call to malloc(3) for 0 bytes. */
	if ((tp->lb_len = total_len * sizeof(CHAR_T)) != 0) {
		MALLOC(sp, tp->lb, CHAR_T *, tp->lb_len * sizeof(CHAR_T));
		if (tp->lb == NULL) {
			free(tp);
			return (NULL);
		}
		if (p != NULL && len != 0)
			MEMCPYW(tp->lb, p, len);
	}
	tp->len = len;
	return (tp);
}

/*
 * text_lfree --
 *	Free a chain of text structures.
 *
 * PUBLIC: void text_lfree __P((TEXTH *));
 */
void
text_lfree(TEXTH *headp)
{
	TEXT *tp;

	while ((tp = TAILQ_FIRST(headp)) != NULL) {
		TAILQ_REMOVE(headp, tp, q);
		text_free(tp);
	}
}

/*
 * text_free --
 *	Free a text structure.
 *
 * PUBLIC: void text_free __P((TEXT *));
 */
void
text_free(TEXT *tp)
{
	if (tp->lb != NULL)
		free(tp->lb);
	free(tp);
}
