/*	$NetBSD: log.c,v 1.2 2008/12/05 22:51:42 christos Exp $ */

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
static const char sccsid[] = "Id: log.c,v 10.26 2002/03/02 23:12:13 skimo Exp (Berkeley) Date: 2002/03/02 23:12:13";
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
#include "dbinternal.h"

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

static int	vi_log_get __P((SCR *sp, db_recno_t *lnop, size_t *size));
static int	log_cursor1 __P((SCR *, int));
static void	log_err __P((SCR *, const char *, int));
#if defined(DEBUG) && 0
static void	log_trace __P((SCR *, const char *, db_recno_t, u_char *));
#endif

/* Try and restart the log on failure, i.e. if we run out of memory. */
#define	LOG_ERR {							\
	log_err(sp, __FILE__, __LINE__);				\
	return (1);							\
}

/* offset of CHAR_T string in log needs to be aligned on some systems
 * because it is passed to db_set as a string
 */
typedef struct {
    char    data[sizeof(u_char) /* type */ + sizeof(db_recno_t)];
    CHAR_T  str[1];
} log_t;
#define CHAR_T_OFFSET ((char *)(((log_t*)0)->str) - (char *)0)

/*
 * log_init --
 *	Initialize the logging subsystem.
 *
 * PUBLIC: int log_init __P((SCR *, EXF *));
 */
int
log_init(SCR *sp, EXF *ep)
{
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

	if (db_create(&ep->log, 0, 0) != 0 ||
	    db_open(ep->log, NULL, DB_RECNO,
			  DB_CREATE | VI_DB_THREAD, S_IRUSR | S_IWUSR) != 0) {
		msgq(sp, M_SYSERR, "009|Log file");
		F_SET(ep, F_NOLOG);
		return (1);
	}

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
	if (ep->log != NULL) {
		(void)(ep->log->close)(ep->log,DB_NOSYNC);
		ep->log = NULL;
	}
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

	BINC_RETC(sp, sp->wp->l_lp, sp->wp->l_len, sizeof(u_char) + sizeof(MARK));
	sp->wp->l_lp[0] = type;
	memmove(sp->wp->l_lp + sizeof(u_char), &ep->l_cursor, sizeof(MARK));

	memset(&key, 0, sizeof(key));
	key.data = &ep->l_cur;
	key.size = sizeof(db_recno_t);
	memset(&data, 0, sizeof(data));
	data.data = sp->wp->l_lp;
	data.size = sizeof(u_char) + sizeof(MARK);
	if (ep->log->put(ep->log, NULL, &key, &data, 0) == -1)
		LOG_ERR;

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

	switch (action) {
	/* newly added for DB4 logging */
	case LOG_LINE_APPEND_B:
	case LOG_LINE_DELETE_F:
		return 0;
	}

	/*
	 * Put out the changes.  If it's a LOG_LINE_RESET_B call, it's a
	 * special case, avoid the caches.  Also, if it fails and it's
	 * line 1, it just means that the user started with an empty file,
	 * so fake an empty length line.
	 */
	if (action == LOG_LINE_RESET_B) {
		if (db_get(sp, lno, DBG_NOCACHE, &lp, &len)) {
			static CHAR_T nul = 0;
			if (lno != 1) {
				db_err(sp, lno);
				return (1);
			}
			len = 0;
			lp = &nul;
		}
	} else
		if (db_get(sp, lno, DBG_FATAL, &lp, &len))
			return (1);
	BINC_RETC(sp,
	    sp->wp->l_lp, sp->wp->l_len, 
	    len * sizeof(CHAR_T) + CHAR_T_OFFSET);
	sp->wp->l_lp[0] = action;
	memmove(sp->wp->l_lp + sizeof(u_char), &lno, sizeof(db_recno_t));
	MEMMOVEW(sp->wp->l_lp + CHAR_T_OFFSET, lp, len);

	lcur = ep->l_cur;
	memset(&key, 0, sizeof(key));
	key.data = &lcur;
	key.size = sizeof(db_recno_t);
	memset(&data, 0, sizeof(data));
	data.data = sp->wp->l_lp;
	data.size = len * sizeof(CHAR_T) + CHAR_T_OFFSET;
	if (ep->log->put(ep->log, NULL, &key, &data, 0) == -1)
		LOG_ERR;

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

	BINC_RETC(sp, sp->wp->l_lp,
	    sp->wp->l_len, sizeof(u_char) + sizeof(LMARK));
	sp->wp->l_lp[0] = LOG_MARK;
	memmove(sp->wp->l_lp + sizeof(u_char), lmp, sizeof(LMARK));

	memset(&key, 0, sizeof(key));
	key.data = &ep->l_cur;
	key.size = sizeof(db_recno_t);
	memset(&data, 0, sizeof(data));
	data.data = sp->wp->l_lp;
	data.size = sizeof(u_char) + sizeof(LMARK);
	if (ep->log->put(ep->log, NULL, &key, &data, 0) == -1)
		LOG_ERR;

#if defined(DEBUG) && 0
	vtrace(sp, "%lu: mark %c: %lu/%u\n",
	    ep->l_cur, lmp->name, lmp->lno, lmp->cno);
#endif
	/* Reset high water mark. */
	ep->l_high = ++ep->l_cur;
	return (0);
}

/*
 * vi_log_get --
 *	Get a line from the log in log buffer.
 */
static int
vi_log_get(SCR *sp, db_recno_t *lnop, size_t *size)
{
	DBT key, data;
	size_t nlen;
	EXF *ep;

	ep = sp->ep;

	nlen = 1024;
retry:
	BINC_RETC(sp, sp->wp->l_lp, sp->wp->l_len, nlen);

	memset(&key, 0, sizeof(key));
	key.data = lnop;		/* Initialize db request. */
	key.size = sizeof(db_recno_t);
	memset(&data, 0, sizeof(data));
	data.data = sp->wp->l_lp;
	data.ulen = sp->wp->l_len;
	data.flags = DB_DBT_USERMEM;
	switch (ep->log->get(ep->log, NULL, &key, &data, 0)) {
	case ENOMEM:
		nlen = data.size;
		goto retry;
	case 0:
		*size = data.size;
		return 0;
	default:
		return 1;
	}
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

	if (ep->l_cur == 1) {
		msgq(sp, M_BERR, "011|No changes to undo");
		return (1);
	}

	if (ep->l_win && ep->l_win != sp->wp) {
		ex_emsg(sp, NULL, EXM_LOCKED);
		return 1;
	}
	ep->l_win = sp->wp;
		

	F_SET(ep, F_NOLOG);		/* Turn off logging. */

	for (didop = 0;;) {
		--ep->l_cur;
		if (vi_log_get(sp, &ep->l_cur, &size))
			LOG_ERR;
#if defined(DEBUG) && 0
		log_trace(sp, "log_backward", ep->l_cur, data.data);
#endif
		switch (*(p = (u_char *)sp->wp->l_lp)) {
		case LOG_CURSOR_INIT:
			if (didop) {
				memmove(rp, p + sizeof(u_char), sizeof(MARK));
				F_CLR(ep, F_NOLOG);
				ep->l_win = NULL;
				return (0);
			}
			break;
		case LOG_CURSOR_END:
			break;
		case LOG_LINE_APPEND_F:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
			if (db_delete(sp, lno))
				goto err;
			++sp->rptlines[L_DELETED];
			break;
		case LOG_LINE_DELETE_B:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
			if (db_insert(sp, lno, 
			    (CHAR_T *)(p + CHAR_T_OFFSET),
			    (size - CHAR_T_OFFSET) / sizeof(CHAR_T)))
				goto err;
			++sp->rptlines[L_ADDED];
			break;
		case LOG_LINE_RESET_F:
			break;
		case LOG_LINE_RESET_B:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
			if (db_set(sp, lno, 
			    (CHAR_T *)(p + CHAR_T_OFFSET),
			    (size - CHAR_T_OFFSET) / sizeof(CHAR_T)))
				goto err;
			if (sp->rptlchange != lno) {
				sp->rptlchange = lno;
				++sp->rptlines[L_CHANGED];
			}
			break;
		case LOG_MARK:
			didop = 1;
			memmove(&lm, p + sizeof(u_char), sizeof(LMARK));
			m.lno = lm.lno;
			m.cno = lm.cno;
			if (mark_set(sp, lm.name, &m, 0))
				goto err;
			break;
		default:
			abort();
		}
	}

err:	F_CLR(ep, F_NOLOG);
	ep->l_win = NULL;
	return (1);
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

	if (ep->l_cur == 1)
		return (1);

	if (ep->l_win && ep->l_win != sp->wp) {
		ex_emsg(sp, NULL, EXM_LOCKED);
		return 1;
	}
	ep->l_win = sp->wp;

	F_SET(ep, F_NOLOG);		/* Turn off logging. */

	for (;;) {
		--ep->l_cur;
		if (vi_log_get(sp, &ep->l_cur, &size))
			LOG_ERR;
#if defined(DEBUG) && 0
		log_trace(sp, "log_setline", ep->l_cur, data.data);
#endif
		switch (*(p = (u_char *)sp->wp->l_lp)) {
		case LOG_CURSOR_INIT:
			memmove(&m, p + sizeof(u_char), sizeof(MARK));
			if (m.lno != sp->lno || ep->l_cur == 1) {
				F_CLR(ep, F_NOLOG);
				ep->l_win = NULL;
				return (0);
			}
			break;
		case LOG_CURSOR_END:
			memmove(&m, p + sizeof(u_char), sizeof(MARK));
			if (m.lno != sp->lno) {
				++ep->l_cur;
				F_CLR(ep, F_NOLOG);
				ep->l_win = NULL;
				return (0);
			}
			break;
		case LOG_LINE_APPEND_F:
		case LOG_LINE_DELETE_B:
		case LOG_LINE_RESET_F:
			break;
		case LOG_LINE_RESET_B:
			memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
			if (lno == sp->lno &&
			    db_set(sp, lno, (CHAR_T *)(p + CHAR_T_OFFSET),
				(size - CHAR_T_OFFSET) / sizeof(CHAR_T)))
				goto err;
			if (sp->rptlchange != lno) {
				sp->rptlchange = lno;
				++sp->rptlines[L_CHANGED];
			}
		case LOG_MARK:
			memmove(&lm, p + sizeof(u_char), sizeof(LMARK));
			m.lno = lm.lno;
			m.cno = lm.cno;
			if (mark_set(sp, lm.name, &m, 0))
				goto err;
			break;
		default:
			abort();
		}
	}

err:	F_CLR(ep, F_NOLOG);
	ep->l_win = NULL;
	return (1);
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

	if (ep->l_cur == ep->l_high) {
		msgq(sp, M_BERR, "014|No changes to re-do");
		return (1);
	}

	if (ep->l_win && ep->l_win != sp->wp) {
		ex_emsg(sp, NULL, EXM_LOCKED);
		return 1;
	}
	ep->l_win = sp->wp;

	F_SET(ep, F_NOLOG);		/* Turn off logging. */

	for (didop = 0;;) {
		++ep->l_cur;
		if (vi_log_get(sp, &ep->l_cur, &size))
			LOG_ERR;
#if defined(DEBUG) && 0
		log_trace(sp, "log_forward", ep->l_cur, data.data);
#endif
		switch (*(p = (u_char *)sp->wp->l_lp)) {
		case LOG_CURSOR_END:
			if (didop) {
				++ep->l_cur;
				memmove(rp, p + sizeof(u_char), sizeof(MARK));
				F_CLR(ep, F_NOLOG);
				ep->l_win = NULL;
				return (0);
			}
			break;
		case LOG_CURSOR_INIT:
			break;
		case LOG_LINE_APPEND_F:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
			if (db_insert(sp, lno, 
			    (CHAR_T *)(p + CHAR_T_OFFSET),
			    (size - CHAR_T_OFFSET) / sizeof(CHAR_T)))
				goto err;
			++sp->rptlines[L_ADDED];
			break;
		case LOG_LINE_DELETE_B:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
			if (db_delete(sp, lno))
				goto err;
			++sp->rptlines[L_DELETED];
			break;
		case LOG_LINE_RESET_B:
			break;
		case LOG_LINE_RESET_F:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
			if (db_set(sp, lno, 
			    (CHAR_T *)(p + CHAR_T_OFFSET),
			    (size - CHAR_T_OFFSET) / sizeof(CHAR_T)))
				goto err;
			if (sp->rptlchange != lno) {
				sp->rptlchange = lno;
				++sp->rptlines[L_CHANGED];
			}
			break;
		case LOG_MARK:
			didop = 1;
			memmove(&lm, p + sizeof(u_char), sizeof(LMARK));
			m.lno = lm.lno;
			m.cno = lm.cno;
			if (mark_set(sp, lm.name, &m, 0))
				goto err;
			break;
		default:
			abort();
		}
	}

err:	F_CLR(ep, F_NOLOG);
	ep->l_win = NULL;
	return (1);
}

/*
 * log_err --
 *	Try and restart the log on failure, i.e. if we run out of memory.
 */
static void
log_err(SCR *sp, const char *file, int line)
{
	EXF *ep;

	msgq(sp, M_SYSERR, "015|%s/%d: log put error", tail(file), line);
	ep = sp->ep;
	(void)ep->log->close(ep->log, DB_NOSYNC);
	if (!log_init(sp, ep))
		msgq(sp, M_ERR, "267|Log restarted");
}

#if defined(DEBUG) && 0
static void
log_trace(SCR *sp, const char *msg, db_recno_t rno, u_char *p)
{
	LMARK lm;
	MARK m;
	db_recno_t lno;

	switch (*p) {
	case LOG_CURSOR_INIT:
		memmove(&m, p + sizeof(u_char), sizeof(MARK));
		vtrace(sp, "%lu: %s:  C_INIT: %u/%u\n", rno, msg, m.lno, m.cno);
		break;
	case LOG_CURSOR_END:
		memmove(&m, p + sizeof(u_char), sizeof(MARK));
		vtrace(sp, "%lu: %s:   C_END: %u/%u\n", rno, msg, m.lno, m.cno);
		break;
	case LOG_LINE_APPEND_F:
		memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
		vtrace(sp, "%lu: %s:  APPEND_F: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_APPEND_B:
		memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
		vtrace(sp, "%lu: %s:  APPEND_B: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_DELETE_F:
		memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
		vtrace(sp, "%lu: %s:  DELETE_F: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_DELETE_B:
		memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
		vtrace(sp, "%lu: %s:  DELETE_B: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_RESET_F:
		memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
		vtrace(sp, "%lu: %s: RESET_F: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_RESET_B:
		memmove(&lno, p + sizeof(u_char), sizeof(db_recno_t));
		vtrace(sp, "%lu: %s: RESET_B: %lu\n", rno, msg, lno);
		break;
	case LOG_MARK:
		memmove(&lm, p + sizeof(u_char), sizeof(LMARK));
		vtrace(sp,
		    "%lu: %s:    MARK: %u/%u\n", rno, msg, lm.lno, lm.cno);
		break;
	default:
		abort();
	}
}
#endif
