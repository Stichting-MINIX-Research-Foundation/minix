/*	$NetBSD: api.c,v 1.2 2008/12/05 22:51:42 christos Exp $ */

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: api.c,v 8.40 2002/06/08 19:30:33 skimo Exp (Berkeley) Date: 2002/06/08 19:30:33";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ex/tag.h"

extern GS *__global_list;			/* XXX */

/*
 * api_fscreen --
 *	Return a pointer to the screen specified by the screen id
 *	or a file name.
 *
 * PUBLIC: SCR *api_fscreen __P((int, char *));
 */
SCR *
api_fscreen(int id, char *name)
{
	GS *gp;
	SCR *tsp;
	WIN *wp;

	gp = __global_list;

	/* Search the displayed lists. */
	for (wp = gp->dq.cqh_first;
	    wp != (void *)&gp->dq; wp = wp->q.cqe_next)
		for (tsp = wp->scrq.cqh_first;
		    tsp != (void *)&wp->scrq; tsp = tsp->q.cqe_next)
		if (name == NULL) {
			if (id == tsp->id)
				return (tsp);
		} else if (!strcmp(name, tsp->frp->name))
			return (tsp);

	/* Search the hidden list. */
	for (tsp = gp->hq.cqh_first;
	    tsp != (void *)&gp->hq; tsp = tsp->q.cqe_next)
		if (name == NULL) {
			if (id == tsp->id)
				return (tsp);
		} else if (!strcmp(name, tsp->frp->name))
			return (tsp);
	return (NULL);
}

/*
 * api_aline --
 *	Append a line.
 *
 * PUBLIC: int api_aline __P((SCR *, db_recno_t, char *, size_t));
 */
int
api_aline(SCR *sp, db_recno_t lno, char *line, size_t len)
{
	size_t wblen;
	const CHAR_T *wbp;

	CHAR2INT(sp, line, len, wbp, wblen);

	return (db_append(sp, 1, lno, wbp, wblen));
}

/*
 * api_extend --
 *	Extend file.
 *
 * PUBLIC: int api_extend __P((SCR *, db_recno_t));
 */
int 
api_extend(SCR *sp, db_recno_t lno)
{
	db_recno_t lastlno;
	if (db_last(sp, &lastlno))
	    return 1;
	while(lastlno < lno)
	    if (db_append(sp, 1, lastlno++, NULL, 0))
		return 1;
	return 0;
}

/*
 * api_dline --
 *	Delete a line.
 *
 * PUBLIC: int api_dline __P((SCR *, db_recno_t));
 */
int
api_dline(SCR *sp, db_recno_t lno)
{
	if (db_delete(sp, lno))
		return 1;
	/* change current line if deleted line is that one
	 * or one berfore that
	 */
	if (sp->lno >= lno && sp->lno > 1)
		sp->lno--;
	return 0;
}

/*
 * api_gline --
 *	Get a line.
 *
 * PUBLIC: int api_gline __P((SCR *, db_recno_t, CHAR_T **, size_t *));
 */
int
api_gline(SCR *sp, db_recno_t lno, CHAR_T **linepp, size_t *lenp)
{
	int isempty;

	if (db_eget(sp, lno, linepp, lenp, &isempty)) {
		if (isempty)
			msgq(sp, M_ERR, "209|The file is empty");
		return (1);
	}
	return (0);
}

/*
 * api_iline --
 *	Insert a line.
 *
 * PUBLIC: int api_iline __P((SCR *, db_recno_t, CHAR_T *, size_t));
 */
int
api_iline(SCR *sp, db_recno_t lno, CHAR_T *line, size_t len)
{
	return (db_insert(sp, lno, line, len));
}

/*
 * api_lline --
 *	Return the line number of the last line in the file.
 *
 * PUBLIC: int api_lline __P((SCR *, db_recno_t *));
 */
int
api_lline(SCR *sp, db_recno_t *lnop)
{
	return (db_last(sp, lnop));
}

/*
 * api_sline --
 *	Set a line.
 *
 * PUBLIC: int api_sline __P((SCR *, db_recno_t, CHAR_T *, size_t));
 */
int
api_sline(SCR *sp, db_recno_t lno, CHAR_T *line, size_t len)
{
	return (db_set(sp, lno, line, len));
}

/*
 * api_getmark --
 *	Get the mark.
 *
 * PUBLIC: int api_getmark __P((SCR *, int, MARK *));
 */
int
api_getmark(SCR *sp, int markname, MARK *mp)
{
	return (mark_get(sp, (ARG_CHAR_T)markname, mp, M_ERR));
}

/*
 * api_setmark --
 *	Set the mark.
 *
 * PUBLIC: int api_setmark __P((SCR *, int, MARK *));
 */
int
api_setmark(SCR *sp, int markname, MARK *mp)
{
	return (mark_set(sp, (ARG_CHAR_T)markname, mp, 1));
}

/*
 * api_nextmark --
 *	Return the first mark if next not set, otherwise return the
 *	subsequent mark.
 *
 * PUBLIC: int api_nextmark __P((SCR *, int, char *));
 */
int
api_nextmark(SCR *sp, int next, char *namep)
{
	LMARK *mp;

	mp = sp->ep->marks.lh_first;
	if (next)
		for (; mp != NULL; mp = mp->q.le_next)
			if (mp->name == *namep) {
				mp = mp->q.le_next;
				break;
			}
	if (mp == NULL)
		return (1);
	*namep = mp->name;
	return (0);
}

/*
 * api_getcursor --
 *	Get the cursor.
 *
 * PUBLIC: int api_getcursor __P((SCR *, MARK *));
 */
int
api_getcursor(SCR *sp, MARK *mp)
{
	mp->lno = sp->lno;
	mp->cno = sp->cno;
	return (0);
}

/*
 * api_setcursor --
 *	Set the cursor.
 *
 * PUBLIC: int api_setcursor __P((SCR *, MARK *));
 */
int
api_setcursor(SCR *sp, MARK *mp)
{
	size_t len;

	if (db_get(sp, mp->lno, DBG_FATAL, NULL, &len))
		return (1);
	if (mp->cno > len) {
		msgq(sp, M_ERR, "Cursor set to nonexistent column");
		return (1);
	}

	/* Set the cursor. */
	sp->lno = mp->lno;
	sp->cno = mp->cno;
	return (0);
}

/*
 * api_emessage --
 *	Print an error message.
 *
 * PUBLIC: void api_emessage __P((SCR *, char *));
 */
void
api_emessage(SCR *sp, char *text)
{
	msgq(sp, M_ERR, "%s", text);
}

/*
 * api_imessage --
 *	Print an informational message.
 *
 * PUBLIC: void api_imessage __P((SCR *, char *));
 */
void
api_imessage(SCR *sp, char *text)
{
	msgq(sp, M_INFO, "%s", text);
}

/*
 * api_edit
 *	Create a new screen and return its id 
 *	or edit a new file in the current screen.
 *
 * PUBLIC: int api_edit __P((SCR *, char *, SCR **, int));
 */
int
api_edit(SCR *sp, char *file, SCR **spp, int newscreen)
{
	EXCMD cmd;
	size_t wlen;
	const CHAR_T *wp;

	if (file) {
		ex_cinit(sp, &cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0);
		CHAR2INT(sp, file, strlen(file) + 1, wp, wlen);
		argv_exp0(sp, &cmd, wp, wlen - 1 /* terminating 0 */);
	} else
		ex_cinit(sp, &cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0);
	if (newscreen)
		cmd.flags |= E_NEWSCREEN;		/* XXX */
	if (cmd.cmd->fn(sp, &cmd))
		return (1);
	*spp = sp->nextdisp;
	return (0);
}

/*
 * api_escreen
 *	End a screen.
 *
 * PUBLIC: int api_escreen __P((SCR *));
 */
int
api_escreen(SCR *sp)
{
	EXCMD cmd;

	/*
	 * XXX
	 * If the interpreter exits anything other than the current
	 * screen, vi isn't going to update everything correctly.
	 */
	ex_cinit(sp, &cmd, C_QUIT, 0, OOBLNO, OOBLNO, 0);
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_swscreen --
 *    Switch to a new screen.
 *
 * PUBLIC: int api_swscreen __P((SCR *, SCR *));
 */
int
api_swscreen(SCR *sp, SCR *new)
{
	/*
	 * XXX
	 * If the interpreter switches from anything other than the
	 * current screen, vi isn't going to update everything correctly.
	 */
	sp->nextdisp = new;
	F_SET(sp, SC_SSWITCH);

	return (0);
}

/*
 * api_map --
 *	Map a key.
 *
 * PUBLIC: int api_map __P((SCR *, char *, char *, size_t));
 */
int
api_map(SCR *sp, char *name, char *map, size_t len)
{
	EXCMD cmd;
	size_t wlen;
	const CHAR_T *wp;

	ex_cinit(sp, &cmd, C_MAP, 0, OOBLNO, OOBLNO, 0);
	CHAR2INT(sp, name, strlen(name) + 1, wp, wlen);
	argv_exp0(sp, &cmd, wp, wlen - 1);
	CHAR2INT(sp, map, len, wp, wlen);
	argv_exp0(sp, &cmd, wp, wlen);
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_unmap --
 *	Unmap a key.
 *
 * PUBLIC: int api_unmap __P((SCR *, char *));
 */
int 
api_unmap(SCR *sp, char *name)
{
	EXCMD cmd;
	size_t wlen;
	const CHAR_T *wp;

	ex_cinit(sp, &cmd, C_UNMAP, 0, OOBLNO, OOBLNO, 0);
	CHAR2INT(sp, name, strlen(name) + 1, wp, wlen);
	argv_exp0(sp, &cmd, wp, wlen - 1);
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_opts_get --
 *	Return a option value as a string, in allocated memory.
 *	If the option is of type boolean, boolvalue is (un)set
 *	according to the value; otherwise boolvalue is -1.
 *
 * PUBLIC: int api_opts_get __P((SCR *, CHAR_T *, char **, int *));
 */
int
api_opts_get(SCR *sp, const CHAR_T *name, char **value, int *boolvalue)
{
	OPTLIST const *op;
	int offset;

	if ((op = opts_search(name)) == NULL) {
		opts_nomatch(sp, name);
		return (1);
	}

	offset = op - optlist;
	if (boolvalue != NULL)
		*boolvalue = -1;
	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		MALLOC_RET(sp, *value, char *, STRLEN(op->name) + 2 + 1);
		(void)sprintf(*value,
		    "%s"WS, O_ISSET(sp, offset) ? "" : "no", op->name);
		if (boolvalue != NULL)
			*boolvalue = O_ISSET(sp, offset);
		break;
	case OPT_NUM:
		MALLOC_RET(sp, *value, char *, 20);
		(void)sprintf(*value, "%lu", (u_long)O_VAL(sp, offset));
		break;
	case OPT_STR:
		if (O_STR(sp, offset) == NULL) {
			MALLOC_RET(sp, *value, char *, 2);
			value[0] = '\0';
		} else {
			MALLOC_RET(sp,
			    *value, char *, strlen(O_STR(sp, offset)) + 1);
			(void)sprintf(*value, "%s", O_STR(sp, offset));
		}
		break;
	}
	return (0);
}

/*
 * api_opts_set --
 *	Set options.
 *
 * PUBLIC: int api_opts_set __P((SCR *, CHAR_T *, char *, u_long, int));
 */
int
api_opts_set(SCR *sp, const CHAR_T *name, 
	     const char *str_value, u_long num_value, int bool_value)
{
	ARGS *ap[2], a, b;
	OPTLIST const *op;
	int rval;
	size_t blen;
	CHAR_T *bp;

	if ((op = opts_search(name)) == NULL) {
		opts_nomatch(sp, name);
		return (1);
	}

	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		GET_SPACE_RETW(sp, bp, blen, 64);
		a.len = SPRINTF(bp, 64, L("%s"WS), bool_value ? "" : "no", name);
		break;
	case OPT_NUM:
		GET_SPACE_RETW(sp, bp, blen, 64);
		a.len = SPRINTF(bp, 64, L(""WS"=%lu"), name, num_value);
		break;
	case OPT_STR:
		GET_SPACE_RETW(sp, bp, blen, 1024);
		a.len = SPRINTF(bp, 1024, L(""WS"=%s"), name, str_value);
		break;
	default:
		bp = NULL;
		break;
	}

	a.bp = bp;
	b.len = 0;
	b.bp = NULL;
	ap[0] = &a;
	ap[1] = &b;
	rval = opts_set(sp, ap, NULL);

	FREE_SPACEW(sp, bp, blen);

	return (rval);
}

/*
 * api_run_str --
 *      Execute a string as an ex command.
 *
 * PUBLIC: int api_run_str __P((SCR *, char *));
 */
int     
api_run_str(SCR *sp, char *cmd)
{
	size_t wlen;
	const CHAR_T *wp;

	CHAR2INT(sp, cmd, strlen(cmd)+1, wp, wlen);
	return (ex_run_str(sp, NULL, wp, wlen - 1, 0, 0));
}

/*
 * PUBLIC: TAGQ * api_tagq_new __P((SCR*, char*));
 */
TAGQ *
api_tagq_new(SCR *sp, char *tag)
{
	TAGQ *tqp;
	size_t len;

	/* Allocate and initialize the tag queue structure. */
	len = strlen(tag);
	CALLOC_GOTO(sp, tqp, TAGQ *, 1, sizeof(TAGQ) + len + 1);
	CIRCLEQ_INIT(&tqp->tagq);
	tqp->tag = tqp->buf;
	memcpy(tqp->tag, tag, (tqp->tlen = len) + 1);

	return tqp;

alloc_err:
	return (NULL);
}

/*
 * PUBLIC: void api_tagq_add __P((SCR*, TAGQ*, char*, char *, char *));
 */
void
api_tagq_add(SCR *sp, TAGQ *tqp, char *filename, char *search, char *msg)
{
	TAG *tp;
	const CHAR_T *wp;
	size_t wlen;
	size_t flen = strlen(filename);
	size_t slen = strlen(search);
	size_t mlen = strlen(msg);

	CALLOC_GOTO(sp, tp, TAG *, 1, 
		    sizeof(TAG) - 1 + flen + 1 + 
		    (slen + 1 + mlen + 1) * sizeof(CHAR_T));
	tp->fname = (char *)tp->buf;
	memcpy(tp->fname, filename, flen + 1);
	tp->fnlen = flen;
	tp->search = (CHAR_T *)((char *)tp->fname + flen + 1);
	CHAR2INT(sp, search, slen + 1, wp, wlen);
	MEMCPYW(tp->search, wp, wlen);
	tp->slen = slen;
	tp->msg = tp->search + slen + 1;
	CHAR2INT(sp, msg, mlen + 1, wp, wlen);
	MEMCPYW(tp->msg, wp, wlen);
	tp->mlen = mlen;
	CIRCLEQ_INSERT_TAIL(&tqp->tagq, tp, q);

alloc_err:
	return;
}

/*
 * PUBLIC: int api_tagq_push __P((SCR*, TAGQ**));
 */
int
api_tagq_push(SCR *sp, TAGQ **tqpp)
{
	TAGQ *tqp;

	tqp = *tqpp;

	*tqpp = 0;

	/* Check to see if we found anything. */
	if (tqp->tagq.cqh_first == (void *)&tqp->tagq) {
		free(tqp);
		return 0;
	}

	tqp->current = tqp->tagq.cqh_first;

	if (tagq_push(sp, tqp, 0, 0))
		return 1;

	return (0);
}

/*
 * PUBLIC: void api_tagq_free __P((SCR*, TAGQ*));
 */
void
api_tagq_free(SCR *sp, TAGQ *tqp)
{
	if (tqp)
		tagq_free(sp, tqp);
}
