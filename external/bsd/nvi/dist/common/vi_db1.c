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
static const char sccsid[] = "Id: db1.c,v 10.1 2002/03/09 12:53:57 skimo Exp  (Berkeley) Date: 2002/03/09 12:53:57 ";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "../vi/vi.h"
#include "dbinternal.h"

/*
 * db_eget --
 *	Front-end to db_get, special case handling for empty files.
 *
 * PUBLIC: int db_eget __P((SCR *, db_recno_t, CHAR_T **, size_t *, int *));
 */
int
db_eget(SCR *sp, db_recno_t lno, CHAR_T **pp, size_t *lenp, int *isemptyp)
	        
	               				/* Line number. */
	            				/* Pointer store. */
	             				/* Length store. */
	              
{
	db_recno_t l1;

	if (isemptyp != NULL)
		*isemptyp = 0;

	/* If the line exists, simply return it. */
	if (!db_get(sp, lno, 0, pp, lenp))
		return (0);

	/*
	 * If the user asked for line 0 or line 1, i.e. the only possible
	 * line in an empty file, find the last line of the file; db_last
	 * fails loudly.
	 */
	if ((lno == 0 || lno == 1) && db_last(sp, &l1))
		return (1);

	/* If the file isn't empty, fail loudly. */
	if ((lno != 0 && lno != 1) || l1 != 0) {
		db_err(sp, lno);
		return (1);
	}

	if (isemptyp != NULL)
		*isemptyp = 1;

	return (1);
}

/*
 * db_get --
 *	Look in the text buffers for a line, followed by the cache, followed
 *	by the database.
 *
 * PUBLIC: int db_get __P((SCR *, db_recno_t, u_int32_t, CHAR_T **, size_t *));
 */
int
db_get(SCR *sp, db_recno_t lno, u_int32_t flags, CHAR_T **pp, size_t *lenp)
	        
	               				/* Line number. */
	                
	            				/* Pointer store. */
	             				/* Length store. */
{
	DBT data, key;
	EXF *ep;
	TEXT *tp;
	db_recno_t l1, l2;
	const CHAR_T *wp;
	size_t wlen;
	size_t nlen;

	/*
	 * The underlying recno stuff handles zero by returning NULL, but
	 * have to have an OOB condition for the look-aside into the input
	 * buffer anyway.
	 */
	if (lno == 0)
		goto err1;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		goto err3;
	}

	if (LF_ISSET(DBG_NOCACHE))
		goto nocache;

	/*
	 * Look-aside into the TEXT buffers and see if the line we want
	 * is there.
	 */
	if (F_ISSET(sp, SC_TINPUT)) {
		l1 = TAILQ_FIRST(&sp->tiq)->lno;
		l2 = TAILQ_LAST(&sp->tiq, _texth)->lno;
		if (l1 <= lno && l2 >= lno) {
#if defined(DBDEBUG) && defined(TRACE)
			vtrace(
			    "retrieve TEXT buffer line %lu\n", (u_long)lno);
#endif
			for (tp = TAILQ_FIRST(&sp->tiq);
			    tp->lno != lno; tp = TAILQ_NEXT(tp, q));
			if (lenp != NULL)
				*lenp = tp->len;
			if (pp != NULL)
				*pp = tp->lb;
			return (0);
		}
		/*
		 * Adjust the line number for the number of lines used
		 * by the text input buffers.
		 */
		if (lno > l2)
			lno -= l2 - l1;
	}

	/* Look-aside into the cache, and see if the line we want is there. */
	if (lno == sp->c_lno) {
#if defined(DBDEBUG) && defined(TRACE)
		vtrace("retrieve cached line %lu\n", (u_long)lno);
#endif
		if (lenp != NULL)
			*lenp = sp->c_len;
		if (pp != NULL)
			*pp = sp->c_lp;
		return (0);
	}
	sp->c_lno = OOBLNO;

nocache:
	nlen = 1024;
retry:
	/* data.size contains length in bytes */
	BINC_GOTO(sp, CHAR_T, sp->c_lp, sp->c_blen, nlen);

	/* Get the line from the underlying database. */
	key.data = &lno;
	key.size = sizeof(lno);
	switch (ep->db->get(ep->db, &key, &data, 0)) {
        case -1:
		goto err2;
	case 1:
err1:		if (LF_ISSET(DBG_FATAL))
err2:			db_err(sp, lno);
alloc_err:
err3:		if (lenp != NULL)
			*lenp = 0;
		if (pp != NULL)
			*pp = NULL;
		return (1);
	case 0:
		if (data.size > nlen) {
			nlen = data.size;
			goto retry;
		} else
			memcpy(sp->c_lp, data.data, nlen);
	}

	if (FILE2INT(sp, data.data, data.size, wp, wlen)) {
	    if (!F_ISSET(sp, SC_CONV_ERROR)) {
		F_SET(sp, SC_CONV_ERROR);
		msgq(sp, M_ERR, "324|Conversion error on line %d", lno);
	    }
	    goto err3;
	}

	/* Reset the cache. */
	if (wp != data.data) {
	    BINC_GOTOW(sp, sp->c_lp, sp->c_blen, wlen);
	    MEMCPYW(sp->c_lp, wp, wlen);
	}
	sp->c_lno = lno;
	sp->c_len = wlen;

#if defined(DBDEBUG) && defined(TRACE)
	vtrace("retrieve DB line %lu\n", (u_long)lno);
#endif
	if (lenp != NULL)
		*lenp = wlen;
	if (pp != NULL)
		*pp = sp->c_lp;
	return (0);
}

/*
 * db_delete --
 *	Delete a line from the file.
 *
 * PUBLIC: int db_delete __P((SCR *, db_recno_t));
 */
int
db_delete(SCR *sp, db_recno_t lno)
{
	DBT key;
	EXF *ep;

#if defined(DBDEBUG) && defined(TRACE)
	vtrace("delete line %lu\n", (u_long)lno);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
	if (ep->l_win && ep->l_win != sp->wp) {
		ex_emsg(sp, NULL, EXM_LOCKED);
		return 1;
	}
		
	/* Update marks, @ and global commands. */
	if (mark_insdel(sp, LINE_DELETE, lno))
		return (1);
	if (ex_g_insdel(sp, LINE_DELETE, lno))
		return (1);

	/* Log change. */
	log_line(sp, lno, LOG_LINE_DELETE_B);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	sp->db_error = ep->db->del(ep->db, &key, 0);
	if (sp->db_error != 0) {
		if (sp->db_error == -1)
			sp->db_error = errno;

		msgq(sp, M_DBERR, "003|unable to delete line %lu", 
		    (u_long)lno);
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	update_cache(sp, LINE_DELETE, lno);

	/* File now modified. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, lno, LOG_LINE_DELETE_F);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_DELETE, 1));
}

/*
 * db_append --
 *	Append a line into the file.
 *
 * PUBLIC: int db_append __P((SCR *, int, db_recno_t, const CHAR_T *, size_t));
 */
int
db_append(SCR *sp, int update, db_recno_t lno, const CHAR_T *p, size_t len)
{
	DBT data, key;
	EXF *ep;
	const char *fp;
	size_t flen;
	int rval;

#if defined(DBDEBUG) && defined(TRACE)
	vtrace("append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
	if (ep->l_win && ep->l_win != sp->wp) {
		ex_emsg(sp, NULL, EXM_LOCKED);
		return 1;
	}

	/* Log before change. */
	log_line(sp, lno + 1, LOG_LINE_APPEND_B);

	INT2FILE(sp, p, len, fp, flen);
		
	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = __UNCONST(fp);
	data.size = flen;
	if (ep->db->put(ep->db, &key, &data, R_IAFTER)) {
		msgq(sp, M_DBERR, "004|unable to append to line %lu", 
			(u_long)lno);
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	update_cache(sp, LINE_INSERT, lno);

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, lno + 1, LOG_LINE_APPEND_F);

	/* Update marks, @ and global commands. */
	rval = 0;
	if (mark_insdel(sp, LINE_INSERT, lno + 1))
		rval = 1;
	if (ex_g_insdel(sp, LINE_INSERT, lno + 1))
		rval = 1;

	/*
	 * Update screen.
	 *
	 * XXX
	 * Nasty hack.  If multiple lines are input by the user, they aren't
	 * committed until an <ESC> is entered.  The problem is the screen was
	 * updated/scrolled as each line was entered.  So, when this routine
	 * is called to copy the new lines from the cut buffer into the file,
	 * it has to know not to update the screen again.
	 */
	return (scr_update(sp, lno, LINE_APPEND, update) || rval);
}

/*
 * db_insert --
 *	Insert a line into the file.
 *
 * PUBLIC: int db_insert __P((SCR *, db_recno_t, CHAR_T *, size_t));
 */
int
db_insert(SCR *sp, db_recno_t lno, CHAR_T *p, size_t len)
{
	DBT data, key;
	EXF *ep;
	const char *fp;
	size_t flen;
	int rval;

#if defined(DBDEBUG) && defined(TRACE)
	vtrace("insert before %lu: len %lu {%.*s}\n",
	    (u_long)lno, (u_long)len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
	if (ep->l_win && ep->l_win != sp->wp) {
		ex_emsg(sp, NULL, EXM_LOCKED);
		return 1;
	}

	/* Log before change. */
	log_line(sp, lno, LOG_LINE_APPEND_B);

	INT2FILE(sp, p, len, fp, flen);
		
	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = __UNCONST(fp);
	data.size = flen;
	if (ep->db->put(ep->db, &key, &data, R_IBEFORE)) {
		msgq(sp, M_SYSERR,
		    "005|unable to insert at line %lu", (u_long)lno);
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	update_cache(sp, LINE_INSERT, lno);

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, lno, LOG_LINE_APPEND_F);

	/* Update marks, @ and global commands. */
	rval = 0;
	if (mark_insdel(sp, LINE_INSERT, lno))
		rval = 1;
	if (ex_g_insdel(sp, LINE_INSERT, lno))
		rval = 1;

	/* Update screen. */
	return (scr_update(sp, lno, LINE_INSERT, 1) || rval);
}

/*
 * db_set --
 *	Store a line in the file.
 *
 * PUBLIC: int db_set __P((SCR *, db_recno_t, CHAR_T *, size_t));
 */
int
db_set(SCR *sp, db_recno_t lno, CHAR_T *p, size_t len)
{
	DBT data, key;
	EXF *ep;
	const char *fp;
	size_t flen;

#if defined(DBDEBUG) && defined(TRACE)
	vtrace("replace line %lu: len %lu {%.*s}\n",
	    (u_long)lno, (u_long)len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
	if (ep->l_win && ep->l_win != sp->wp) {
		ex_emsg(sp, NULL, EXM_LOCKED);
		return 1;
	}
		
	/* Log before change. */
	log_line(sp, lno, LOG_LINE_RESET_B);

	INT2FILE(sp, p, len, fp, flen);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = __UNCONST(fp);
	data.size = flen;
	sp->db_error =
		ep->db->put(ep->db, &key, &data, 0);
	if (sp->db_error != 0) {
		if (sp->db_error == -1)
			sp->db_error = errno;

		msgq(sp, M_DBERR, "006|unable to store line %lu", (u_long)lno);
		return (1);
	}

	/* Flush the cache, before logging or screen update. */
	update_cache(sp, LINE_RESET, lno);

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, lno, LOG_LINE_RESET_F);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_RESET, 1));
}

/*
 * db_exist --
 *	Return if a line exists.
 *
 * PUBLIC: int db_exist __P((SCR *, db_recno_t));
 */
int
db_exist(SCR *sp, db_recno_t lno)
{
	EXF *ep;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}

	if (lno == OOBLNO)
		return (0);
		
	/*
	 * Check the last-line number cache.  Adjust the cached line
	 * number for the lines used by the text input buffers.
	 */
	if (ep->c_nlines != OOBLNO)
		return (lno <= (F_ISSET(sp, SC_TINPUT) ?
		    ep->c_nlines + (TAILQ_LAST(&sp->tiq, _texth)->lno -
		    TAILQ_FIRST(&sp->tiq)->lno) : ep->c_nlines));

	/* Go get the line. */
	return (!db_get(sp, lno, 0, NULL, NULL));
}

/*
 * db_last --
 *	Return the number of lines in the file.
 *
 * PUBLIC: int db_last __P((SCR *, db_recno_t *));
 */
int
db_last(SCR *sp, db_recno_t *lnop)
{
	DBT data, key;
	EXF *ep;
	db_recno_t lno;
	const CHAR_T *wp;
	size_t wlen;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/*
	 * Check the last-line number cache.  Adjust the cached line
	 * number for the lines used by the text input buffers.
	 */
	if (ep->c_nlines != OOBLNO) {
		*lnop = ep->c_nlines;
		if (F_ISSET(sp, SC_TINPUT))
			*lnop += TAILQ_LAST(&sp->tiq, _texth)->lno -
			    TAILQ_FIRST(&sp->tiq)->lno;
		return (0);
	}

	key.data = &lno;
	key.size = sizeof(lno);

	sp->db_error = ep->db->seq(ep->db, &key, &data, R_LAST);
	switch (sp->db_error) {
	case 1:
		*lnop = 0;
		return (0);
	case -1:
		sp->db_error = errno;
alloc_err:
		msgq(sp, M_DBERR, "007|unable to get last line");
		*lnop = 0;
		return (1);
        case 0:
		;
	}

	memcpy(&lno, key.data, sizeof(lno));

	if (lno != sp->c_lno) {
	    FILE2INT(sp, data.data, data.size, wp, wlen);

	    /* Fill the cache. */
	    BINC_GOTOW(sp, sp->c_lp, sp->c_blen, wlen);
	    MEMCPYW(sp->c_lp, wp, wlen);
	    sp->c_lno = lno;
	    sp->c_len = wlen;
	}
	ep->c_nlines = lno;

	/* Return the value. */
	*lnop = (F_ISSET(sp, SC_TINPUT) &&
	    TAILQ_LAST(&sp->tiq, _texth)->lno > lno ?
	    TAILQ_LAST(&sp->tiq, _texth)->lno : lno);
	return (0);
}

/*
 * db_err --
 *	Report a line error.
 *
 * PUBLIC: void db_err __P((SCR *, db_recno_t));
 */
void
db_err(SCR *sp, db_recno_t lno)
{
	msgq(sp, M_ERR,
	    "008|Error: unable to retrieve line %lu", (u_long)lno);
}

/*
 * scr_update --
 *	Update all of the screens that are backed by the file that
 *	just changed.
 * PUBLIC: int scr_update __P((SCR *sp, db_recno_t lno,
 * PUBLIC:                      lnop_t op, int current));
 */
int
scr_update(SCR *sp, db_recno_t lno, lnop_t op, int current)
{
	EXF *ep;
	SCR *tsp;
	WIN *wp;

	if (F_ISSET(sp, SC_EX))
		return (0);

	/* XXXX goes outside of window */
	ep = sp->ep;
	if (ep->refcnt != 1)
		TAILQ_FOREACH(wp, &sp->gp->dq, q)
			TAILQ_FOREACH(tsp, &wp->scrq, q)
				if (sp != tsp && tsp->ep == ep)
					if (vs_change(tsp, lno, op))
						return (1);
	return (current ? vs_change(sp, lno, op) : 0);
}

/*
 * PUBLIC: void update_cache __P((SCR *sp, lnop_t op, db_recno_t lno));
 */
void
update_cache(SCR *sp, lnop_t op, db_recno_t lno)
{
	SCR* scrp;
	EXF *ep;

	ep = sp->ep;

	/* Flush the cache, update line count, before screen update. */
	/* The flushing is probably not needed, since it was incorrect
	 * for db_insert.  It might be better to adjust it, like
	 * marks, @ and global
	 */
	TAILQ_FOREACH(scrp, &ep->scrq, eq)
		switch (op) {
		case LINE_INSERT:
		case LINE_DELETE:
			if (lno <= scrp->c_lno)
				scrp->c_lno = OOBLNO;
			break;
		case LINE_RESET:
			if (lno == scrp->c_lno)
				scrp->c_lno = OOBLNO;
			break;
		case LINE_APPEND:
			break;
		}

	if (ep->c_nlines != OOBLNO)
		switch (op) {
		case LINE_INSERT:
			++ep->c_nlines;
			break;
		case LINE_DELETE:
			--ep->c_nlines;
			break;
		case LINE_APPEND:
		case LINE_RESET:
			break;
		}
}

/*
 * PUBLIC: int db_msg_open __P((SCR *, const char *, DB **));
 */
int db_msg_open(SCR *sp, const char *file, DB **dbp)
{
	*dbp = dbopen(file, O_NONBLOCK | O_RDONLY, 0, DB_RECNO, NULL);

	return *dbp == NULL;
}

/*
 * PUBLIC: int db_init __P((SCR *, EXF *, char *, char *, size_t, int *));
 */
int
db_init(SCR *sp, EXF *ep, char *rcv_name, char *oname, size_t psize, int *open_err)
{
	RECNOINFO oinfo;

	memset(&oinfo, 0, sizeof(RECNOINFO));
	oinfo.bval = '\n';			/* Always set. */
	/*
	 * If we are not recovering, set the pagesize and arrange to
	 * first get a snapshot of the file.
	 */
	if (rcv_name == NULL) {
		oinfo.psize = psize;
		oinfo.flags = R_SNAPSHOT;
	}
	/*
	 * Always set the btree name, otherwise we are going to be using
	 * an in-memory database for the btree.
	 */
	oinfo.bfname = ep->rcv_path;

#define _DB_OPEN_MODE	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH

	ep->db = dbopen(rcv_name == NULL ? oname : NULL,
	    O_NONBLOCK | O_RDONLY, _DB_OPEN_MODE, DB_RECNO, &oinfo);
	
	if (!ep->db) {
		msgq_str(sp,
		    M_DBERR, rcv_name == NULL ? oname : rcv_name, "%s");
		/*
		 * !!!
		 * Historically, vi permitted users to edit files that couldn't
		 * be read.  This isn't useful for single files from a command
		 * line, but it's quite useful for "vi *.c", since you can skip
		 * past files that you can't read.
		 */ 
		ep->db = NULL; /* Don't close it; it wasn't opened */

		*open_err = 1;
		return 1;
	} else {
		/*
		 * We always sync the underlying btree so that the header
		 * is written first
		 */
		ep->db->sync(ep->db, R_RECNOSYNC);
	}

	return 0;
}

const char *
db_strerror(int error)
{
	return error > 0 ? strerror(error) : "record not found";
}
