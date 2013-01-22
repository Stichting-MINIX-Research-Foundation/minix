/*	$NetBSD: ex_at.c,v 1.3 2011/11/23 19:25:28 tnozaki Exp $ */

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
static const char sccsid[] = "Id: ex_at.c,v 10.16 2001/06/25 15:19:14 skimo Exp (Berkeley) Date: 2001/06/25 15:19:14";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"

/*
 * ex_at -- :@[@ | buffer]
 *	    :*[* | buffer]
 *
 *	Execute the contents of the buffer.
 *
 * PUBLIC: int ex_at __P((SCR *, EXCMD *));
 */
int
ex_at(SCR *sp, EXCMD *cmdp)
{
	CB *cbp;
	ARG_CHAR_T name;
	EXCMD *ecp;
	RANGE *rp;
	TEXT *tp;
	size_t len;
	CHAR_T *p;

	/*
	 * !!!
	 * Historically, [@*]<carriage-return> and [@*][@*] executed the most
	 * recently executed buffer in ex mode.
	 */
	name = FL_ISSET(cmdp->iflags, E_C_BUFFER) ? cmdp->buffer : '@';
	if (name == '@' || name == '*') {
		if (!F_ISSET(sp, SC_AT_SET)) {
			ex_emsg(sp, NULL, EXM_NOPREVBUF);
			return (1);
		}
		name = sp->at_lbuf;
	}
	sp->at_lbuf = name;
	F_SET(sp, SC_AT_SET);

	CBNAME(sp, cbp, name);
	if (cbp == NULL) {
		ex_emsg(sp, (char *)KEY_NAME(sp, name), EXM_EMPTYBUF);
		return (1);
	}

	/*
	 * !!!
	 * Historically the @ command took a range of lines, and the @ buffer
	 * was executed once per line.  The historic vi could be trashed by
	 * this because it didn't notice if the underlying file changed, or,
	 * for that matter, if there were no more lines on which to operate.
	 * For example, take a 10 line file, load "%delete" into a buffer,
	 * and enter :8,10@<buffer>.
	 *
	 * The solution is a bit tricky.  If the user specifies a range, take
	 * the same approach as for global commands, and discard the command
	 * if exit or switch to a new file/screen.  If the user doesn't specify
	 * the  range, continue to execute after a file/screen switch, which
	 * means @ buffers are still useful in a multi-screen environment.
	 */
	CALLOC_RET(sp, ecp, EXCMD *, 1, sizeof(EXCMD));
	CIRCLEQ_INIT(&ecp->rq);
	CALLOC_RET(sp, rp, RANGE *, 1, sizeof(RANGE));
	rp->start = cmdp->addr1.lno;
	if (F_ISSET(cmdp, E_ADDR_DEF)) {
		rp->stop = rp->start;
		FL_SET(ecp->agv_flags, AGV_AT_NORANGE);
	} else {
		rp->stop = cmdp->addr2.lno;
		FL_SET(ecp->agv_flags, AGV_AT);
	}
	CIRCLEQ_INSERT_HEAD(&ecp->rq, rp, q);

	/*
	 * Buffers executed in ex mode or from the colon command line in vi
	 * were ex commands.  We can't push it on the terminal queue, since
	 * it has to be executed immediately, and we may be in the middle of
	 * an ex command already.  Push the command on the ex command stack.
	 * Build two copies of the command.  We need two copies because the
	 * ex parser may step on the command string when it's parsing it.
	 */
	for (len = 0, tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev)
		len += tp->len + 1;

	MALLOC_RET(sp, ecp->cp, CHAR_T *, len * 2 * sizeof(CHAR_T));
	ecp->o_cp = ecp->cp;
	ecp->o_clen = len;
	ecp->cp[len] = '\0';

	/* Copy the buffer into the command space. */
	for (p = ecp->cp + len, tp = cbp->textq.cqh_last;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_prev) {
		MEMCPYW(p, tp->lb, tp->len);
		p += tp->len;
		*p++ = '\n';
	}

	LIST_INSERT_HEAD(&sp->wp->ecq, ecp, q);
	return (0);
}
