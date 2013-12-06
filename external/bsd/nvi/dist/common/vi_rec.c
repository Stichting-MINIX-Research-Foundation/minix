#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "common.h"

#include "db_int.h"
#include "db_page.h"
#include <log.h>
#include "hash.h"
#include "btree.h"

#define LOG_CURSOR_HIT	    -1000

/*
 * PUBLIC: #ifdef USE_DB4_LOGGING
 */
/*
 * __vi_marker_recover --
 *	Recovery function for marker.
 *
 * PUBLIC: int __vi_marker_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_marker_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_marker_args *argp;
	int ret;

	REC_PRINT(__vi_marker_print);
	REC_NOOP_INTRO(__vi_marker_read);

	*lsnp = argp->prev_lsn;
	ret = 0;

    	REC_NOOP_CLOSE;
}

/*
 * __vi_cursor_recover --
 *	Recovery function for cursor.
 *
 * PUBLIC: int __vi_cursor_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_cursor_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_cursor_args *argp;
	int ret;
	SCR *sp;

	REC_PRINT(__vi_cursor_print);
	REC_NOOP_INTRO(__vi_cursor_read);

	sp = (SCR *)dbenv->app_private;

	*lsnp = argp->prev_lsn;
	if (sp->state.undo == UNDO_SETLINE) {
		/* Why the check for ep->l_cur ? (copied from log.c)
		 */
		ret = (argp->lno != sp->lno || 
		    (argp->opcode == LOG_CURSOR_INIT && sp->ep->l_cur == 1))
			  ? LOG_CURSOR_HIT : 0;
	}
	else {
		ret = argp->opcode == 
			(DB_UNDO(op) ? LOG_CURSOR_INIT : LOG_CURSOR_END)
			  ? LOG_CURSOR_HIT : 0;
		if (ret) {
			sp->state.pos.lno = argp->lno;
			sp->state.pos.cno = argp->cno;
		}
	}

    	REC_NOOP_CLOSE;
}

/*
 * __vi_mark_recover --
 *	Recovery function for mark.
 *
 * PUBLIC: int __vi_mark_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_mark_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_mark_args *argp;
	int ret;
	MARK m;
	SCR *sp;

	REC_PRINT(__vi_mark_print);
	REC_NOOP_INTRO(__vi_mark_read);

	sp = (SCR *)dbenv->app_private;
	*lsnp = argp->prev_lsn;
	m.lno = argp->lmp.lno;
	m.cno = argp->lmp.cno;
	ret = mark_set(sp, argp->lmp.name, &m, 0);

    	REC_NOOP_CLOSE;
}

/*
 * __vi_change_recover --
 *	Recovery function for change.
 *
 * PUBLIC: int __vi_change_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__vi_change_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__vi_change_args *argp;
	int ret;
	SCR *sp;

	REC_PRINT(__vi_change_print);
	REC_NOOP_INTRO(__vi_change_read);

	ret = 0;

	sp = (SCR *)dbenv->app_private;
	if (DB_UNDO(op) != (argp->opcode & 1))
	    switch (argp->opcode) {
	    case LOG_LINE_RESET_B:
	    case LOG_LINE_RESET_F:
		    ret = line_insdel(sp, LINE_RESET, argp->lno);
		    update_cache(sp, LINE_RESET, argp->lno);
		    ret = scr_update(sp, argp->lno, LINE_RESET, 1) || ret;
		    break;
	    case LOG_LINE_APPEND_B:
	    case LOG_LINE_DELETE_F:
		    ret = line_insdel(sp, LINE_DELETE, argp->lno);
		    update_cache(sp, LINE_DELETE, argp->lno);
		    ret = scr_update(sp, argp->lno, LINE_DELETE, 1) || ret;
		    break;
	    case LOG_LINE_DELETE_B:
	    case LOG_LINE_APPEND_F:
		    ret = line_insdel(sp, LINE_INSERT, argp->lno);
		    update_cache(sp, LINE_INSERT, argp->lno);
		    ret = scr_update(sp, argp->lno, LINE_INSERT, 1) || ret;
		    break;
	    }

	*lsnp = argp->prev_lsn;

    	REC_NOOP_CLOSE;
}

/*
 *
 * PUBLIC: int __vi_log_truncate __P((EXF *ep));
 */
int
__vi_log_truncate(EXF *ep)
{
	DB_LSN ckplsn;

	ZERO_LSN(ckplsn);
	return __log_vtruncate(ep->env, &ep->lsn_cur, &ckplsn);
	/*return __log_vtruncate(ep->env, &ep->lsn_cur, &ep->lsn_first);*/
}

/*
 *
 * PUBLIC: int __vi_log_dispatch __P((DB_ENV *dbenv, DBT *data, DB_LSN *lsn, db_recops ops));
 */
int
__vi_log_dispatch(DB_ENV *dbenv, DBT *data, DB_LSN *lsn, db_recops ops)
{
	u_int32_t rectype;
	char	s[100];

	memcpy(&rectype, data->data, sizeof(rectype));
	snprintf(s,100,"%d\n", rectype);
	return dbenv->dtab[rectype](dbenv, data, lsn, ops, NULL);
}

static int 
vi_log_get(SCR *sp, DB_LOGC *logc, DBT *data, u_int32_t which)
{
	size_t nlen;
	EXF *ep;

	ep = sp->ep;

	nlen = 1024;
retry:
	BINC_GOTO(sp, sp->wp->l_lp, sp->wp->l_len, nlen);
	memset(data, 0, sizeof(*data));
	data->data = sp->wp->l_lp;
	data->ulen = sp->wp->l_len;
	data->flags = DB_DBT_USERMEM;
	switch ((sp->db_error = logc->get(logc, &ep->lsn_cur, data, which))) {
	case ENOMEM:
		nlen = data->size;
		goto retry;
	default:
alloc_err:
		msgq(sp, M_DBERR, "logc->get");
		F_SET(ep, F_NOLOG);
		return (1);
	case 0:
		;
	}
	return 0;
}

/*
 *
 * PUBLIC: int __vi_log_traverse __P((SCR *sp, undo_t undo, MARK *));
 */
int
__vi_log_traverse(SCR *sp, undo_t undo, MARK *rp)
{
	DB_LOGC *logc;
	DBT data;
	EXF *ep;
	int	    ret;
	DB_LSN	    lsn;
	u_int32_t   which;
	db_recops   ops;

	ep = sp->ep;

	F_SET(ep, F_NOLOG);		/* Turn off logging. */

	sp->state.undo = undo;
	ep->env->app_private = sp;
	if ((sp->db_error = ep->env->log_cursor(ep->env, &logc, 0)) 
		    != 0) {
		msgq(sp, M_DBERR, "env->log_cursor");
		return (1);
	}
	if (vi_log_get(sp, logc, &data, DB_SET))
		return 1;
	if (undo == UNDO_FORWARD) {
		ops = DB_TXN_FORWARD_ROLL;
		which = DB_NEXT;
		if (vi_log_get(sp, logc, &data, DB_NEXT))
			return 1;
	} else {
		ops = DB_TXN_BACKWARD_ROLL;
		which = DB_PREV;
	}

	for (;;) {
		MEMCPY(&lsn, &ep->lsn_cur, 1);
		ret = __vi_log_dispatch(ep->env, &data, &lsn, ops);
		if (ret != 0) {
			if (ret == LOG_CURSOR_HIT)
				break;
		}

		if (vi_log_get(sp, logc, &data, which))
			return 1;
		if (undo == UNDO_SETLINE && 
		    log_compare(&ep->lsn_cur, &ep->lsn_first) <= 0) {
			/* Move to previous record without dispatching. */
			undo = UNDO_BACKWARD;
			break;
		}
	}
	if (undo == UNDO_BACKWARD)
		if (vi_log_get(sp, logc, &data, DB_PREV))
			return 1;

	logc->close(logc, 0);

	ep->env->app_private = NULL;

	MEMMOVE(rp, &sp->state.pos, 1);

	F_CLR(ep, F_NOLOG);

	return 0;
}

int
vi_db_init_recover(DB_ENV *dbenv)
{
	int	ret;

	if ((ret = __db_init_recover(dbenv)) != 0)
		return (ret);
	if ((ret = __bam_init_recover(dbenv)) != 0)
		return (ret);

	return 0;
}
/*
 * PUBLIC: #endif
 */
