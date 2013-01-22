/*	$NetBSD: log4.c,v 1.1.1.2 2008/05/18 14:29:47 aymeric Exp $ */

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: log4.c,v 10.3 2002/06/08 21:00:33 skimo Exp";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/*
 * The log consists of records, each containing a type byte and a variable
 * length byte string, as follows:
 *
 *	LOG_CURSOR_INIT		MARK
 *	LOG_CURSOR_END		MARK
 *	LOG_LINE_APPEND_F 	db_recno_t		char *
 *	LOG_LINE_APPEND_B 	db_recno_t		char *
 *	LOG_LINE_DELETE_F	db_recno_t		char *
 *	LOG_LINE_DELETE_B	db_recno_t		char *
 *	LOG_LINE_RESET_F	db_recno_t		char *
 *	LOG_LINE_RESET_B	db_recno_t		char *
 *	LOG_MARK		LMARK
 *
 * We do before image physical logging.  This means that the editor layer
 * MAY NOT modify records in place, even if simply deleting or overwriting
 * characters.  Since the smallest unit of logging is a line, we're using
 * up lots of space.  This may eventually have to be reduced, probably by
 * doing logical logging, which is a much cooler database phrase.
 *
 * The implementation of the historic vi 'u' command, using roll-forward and
 * roll-back, is simple.  Each set of changes has a LOG_CURSOR_INIT record,
 * followed by a number of other records, followed by a LOG_CURSOR_END record.
 * LOG_LINE_RESET records come in pairs.  The first is a LOG_LINE_RESET_B
 * record, and is the line before the change.  The second is LOG_LINE_RESET_F,
 * and is the line after the change.  Roll-back is done by backing up to the
 * first LOG_CURSOR_INIT record before a change.  Roll-forward is done in a
 * similar fashion.
 *
 * The 'U' command is implemented by rolling backward to a LOG_CURSOR_END
 * record for a line different from the current one.  It should be noted that
 * this means that a subsequent 'u' command will make a change based on the
 * new position of the log's cursor.  This is okay, and, in fact, historic vi
 * behaved that way.
 */

static int	log_cursor1 __P((SCR *, int));

/*
 * log_init --
 *	Initialize the logging subsystem.
 *
 * PUBLIC: int log_init __P((SCR *, EXF *));
 */
int
log_init(SCR *sp, EXF *ep)
{
	DB_LOGC *logc;
	DBT data;
	size_t nlen;

	/*
	 * !!!
	 * ep MAY NOT BE THE SAME AS sp->ep, DON'T USE THE LATTER.
	 *
	 * Initialize the buffer.  The logging subsystem has its own
	 * buffers because the global ones are almost by definition
	 * going to be in use when the log runs.
	 */
	sp->wp->l_lp = NULL;
	sp->wp->l_len = 0;
	ep->l_cursor.lno = 1;		/* XXX Any valid recno. */
	ep->l_cursor.cno = 0;
	ep->l_high = ep->l_cur = 1;

	if ((sp->db_error = ep->env->log_cursor(ep->env, &logc, 0)) 
		    != 0) {
		msgq(sp, M_DBERR, "env->log_cursor");
		F_SET(ep, F_NOLOG);
		return (1);
	}
	nlen = 1024;
retry:
	BINC_GOTO(sp, sp->wp->l_lp, sp->wp->l_len, nlen);
	memset(&data, 0, sizeof(data));
	data.data = sp->wp->l_lp;
	data.ulen = sp->wp->l_len;
	data.flags = DB_DBT_USERMEM;
	switch ((sp->db_error = 
	    logc->get(logc, &ep->lsn_first, &data, DB_LAST))) {
	case ENOMEM:
		nlen = data.size;
		goto retry;
	default:
alloc_err:
		msgq(sp, M_DBERR, "logc->get");
		F_SET(ep, F_NOLOG);
		return (1);
	case 0:
		;
	}
	MEMCPY(&ep->lsn_cur, &ep->lsn_first, 1);
	MEMCPY(&ep->lsn_high, &ep->lsn_first, 1);
	logc->close(logc, 0);

	ep->l_win = NULL;
	/*LOCK_INIT(sp->wp, ep);*/

	return (0);
}

/*
 * log_end --
 *	Close the logging subsystem.
 *
 * PUBLIC: int log_end __P((SCR *, EXF *));
 */
int
log_end(SCR *sp, EXF *ep)
{
	/*
	 * !!!
	 * ep MAY NOT BE THE SAME AS sp->ep, DON'T USE THE LATTER.
	 */
	/*LOCK_END(sp->wp, ep);*/
	if (sp->wp->l_lp != NULL) {
		free(sp->wp->l_lp);
		sp->wp->l_lp = NULL;
	}
	sp->wp->l_len = 0;
	ep->l_cursor.lno = 1;		/* XXX Any valid recno. */
	ep->l_cursor.cno = 0;
	ep->l_high = ep->l_cur = 1;
	return (0);
}

/*
 * log_cursor --
 *	Log the current cursor position, starting an event.
 *
 * PUBLIC: int log_cursor __P((SCR *));
 */
int
log_cursor(SCR *sp)
{
	EXF *ep;

	ep = sp->ep;
	if (F_ISSET(ep, F_NOLOG))
		return (0);

	/*
	 * If any changes were made since the last cursor init,
	 * put out the ending cursor record.
	 */
	if (ep->l_cursor.lno == OOBLNO) {
		if (ep->l_win && ep->l_win != sp->wp)
			return 0;
		ep->l_cursor.lno = sp->lno;
		ep->l_cursor.cno = sp->cno;
		ep->l_win = NULL;
		return (log_cursor1(sp, LOG_CURSOR_END));
	}
	ep->l_cursor.lno = sp->lno;
	ep->l_cursor.cno = sp->cno;
	return (0);
}

/*
 * log_cursor1 --
 *	Actually push a cursor record out.
 */
static int
log_cursor1(SCR *sp, int type)
{
	DBT data, key;
	EXF *ep;

	ep = sp->ep;

	/*
	if (type == LOG_CURSOR_INIT &&
	    LOCK_TRY(sp->wp, ep))
		return 1;
	*/

	if (type == LOG_CURSOR_INIT && 
	    (sp->db_error = __vi_log_truncate(ep)) != 0) {
		msgq(sp, M_DBERR, "truncate");
		return 1;
	}
	if ((sp->db_error = 
		__vi_cursor_log(ep->env, NULL, &ep->lsn_cur, 0, type, 
			    ep->l_cursor.lno, ep->l_cursor.cno)) != 0) {
		msgq(sp, M_DBERR, "cursor_log");
		return 1;
	}
	if (type == LOG_CURSOR_END) {
		MEMCPY(&ep->lsn_high, &ep->lsn_cur, 1);
		/* XXXX should not be needed */
		ep->env->log_flush(ep->env, NULL);
	}

#if defined(DEBUG) && 0
	vtrace(sp, "%lu: %s: %u/%u\n", ep->l_cur,
	    type == LOG_CURSOR_INIT ? "log_cursor_init" : "log_cursor_end",
	    sp->lno, sp->cno);
#endif
	/* Reset high water mark. */
	ep->l_high = ++ep->l_cur;

	/*
	if (type == LOG_CURSOR_END)
		LOCK_UNLOCK(sp->wp, ep);
	*/
	return (0);
}

/*
 * log_line --
 *	Log a line change.
 *
 * PUBLIC: int log_line __P((SCR *, db_recno_t, u_int));
 */
int
log_line(SCR *sp, db_recno_t lno, u_int action)
{
	DBT data, key;
	EXF *ep;
	size_t len;
	CHAR_T *lp;
	db_recno_t lcur;

	ep = sp->ep;
	if (F_ISSET(ep, F_NOLOG))
		return (0);

	/*
	 * XXX
	 *
	 * Kluge for vi.  Clear the EXF undo flag so that the
	 * next 'u' command does a roll-back, regardless.
	 */
	F_CLR(ep, F_UNDO);

	/* Put out one initial cursor record per set of changes. */
	if (ep->l_cursor.lno != OOBLNO) {
		if (log_cursor1(sp, LOG_CURSOR_INIT))
			return (1);
		ep->l_cursor.lno = OOBLNO;
		ep->l_win = sp->wp;
	} /*else if (ep->l_win != sp->wp) {
		printf("log_line own: %p, this: %p\n", ep->l_win, sp->wp);
		return 1;
	}*/

	if ((sp->db_error = 
		__vi_change_log(ep->env, NULL, &ep->lsn_cur, 0, action, 
			    lno)) != 0) {
		msgq(sp, M_DBERR, "change_log");
		return 1;
	}

#if defined(DEBUG) && 0
	switch (action) {
	case LOG_LINE_APPEND_F:
		vtrace(sp, "%u: log_line: append_f: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_APPEND_B:
		vtrace(sp, "%u: log_line: append_b: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_DELETE_F:
		vtrace(sp, "%lu: log_line: delete_f: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_DELETE_B:
		vtrace(sp, "%lu: log_line: delete_b: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_RESET_F:
		vtrace(sp, "%lu: log_line: reset_f: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_RESET_B:
		vtrace(sp, "%lu: log_line: reset_b: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	}
#endif
	/* Reset high water mark. */
	ep->l_high = ++ep->l_cur;

	return (0);
}

/*
 * log_mark --
 *	Log a mark position.  For the log to work, we assume that there
 *	aren't any operations that just put out a log record -- this
 *	would mean that undo operations would only reset marks, and not
 *	cause any other change.
 *
 * PUBLIC: int log_mark __P((SCR *, LMARK *));
 */
int
log_mark(SCR *sp, LMARK *lmp)
{
	DBT data, key;
	EXF *ep;

	ep = sp->ep;
	if (F_ISSET(ep, F_NOLOG))
		return (0);

	/* Put out one initial cursor record per set of changes. */
	if (ep->l_cursor.lno != OOBLNO) {
		if (log_cursor1(sp, LOG_CURSOR_INIT))
			return (1);
		ep->l_cursor.lno = OOBLNO;
		ep->l_win = sp->wp;
	}

	if ((sp->db_error = 
		__vi_mark_log(ep->env, NULL, &ep->lsn_cur, 0, 
			    lmp)) != 0) {
		msgq(sp, M_DBERR, "cursor_log");
		return 1;
	}

#if defined(DEBUG) && 0
	vtrace(sp, "%lu: mark %c: %lu/%u\n",
	    ep->l_cur, lmp->name, lmp->lno, lmp->cno);
#endif
	/* Reset high water mark. */
	ep->l_high = ++ep->l_cur;
	return (0);
}

/*
 * Log_backward --
 *	Roll the log backward one operation.
 *
 * PUBLIC: int log_backward __P((SCR *, MARK *));
 */
int
log_backward(SCR *sp, MARK *rp)
{
	EXF *ep;
	LMARK lm;
	MARK m;
	db_recno_t lno;
	int didop;
	u_char *p;
	size_t size;

	ep = sp->ep;
	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_ERR,
		    "010|Logging not being performed, undo not possible");
		return (1);
	}

	if (log_compare(&ep->lsn_cur, &ep->lsn_first) <= 0) {
		msgq(sp, M_BERR, "011|No changes to undo");
		return (1);
	}
	return __vi_log_traverse(sp, UNDO_BACKWARD, rp);
}

/*
 * Log_setline --
 *	Reset the line to its original appearance.
 *
 * XXX
 * There's a bug in this code due to our not logging cursor movements
 * unless a change was made.  If you do a change, move off the line,
 * then move back on and do a 'U', the line will be restored to the way
 * it was before the original change.
 *
 * PUBLIC: int log_setline __P((SCR *));
 */
int
log_setline(SCR *sp)
{
	EXF *ep;
	LMARK lm;
	MARK m;
	db_recno_t lno;
	u_char *p;
	size_t size;

	ep = sp->ep;
	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_ERR,
		    "012|Logging not being performed, undo not possible");
		return (1);
	}

	if (log_compare(&ep->lsn_cur, &ep->lsn_first) <= 0) {
		msgq(sp, M_BERR, "011|No changes to undo");
		return (1);
	}
	return __vi_log_traverse(sp, UNDO_SETLINE, &m);
}

/*
 * Log_forward --
 *	Roll the log forward one operation.
 *
 * PUBLIC: int log_forward __P((SCR *, MARK *));
 */
int
log_forward(SCR *sp, MARK *rp)
{
	EXF *ep;
	LMARK lm;
	MARK m;
	db_recno_t lno;
	int didop;
	u_char *p;
	size_t size;

	ep = sp->ep;
	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_ERR,
	    "013|Logging not being performed, roll-forward not possible");
		return (1);
	}

	if (log_compare(&ep->lsn_cur, &ep->lsn_high) >= 0) {
		msgq(sp, M_BERR, "014|No changes to re-do");
		return (1);
	}
	return __vi_log_traverse(sp, UNDO_FORWARD, rp);
}
