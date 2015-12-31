/*	$NetBSD: v_at.c,v 1.4 2014/01/26 21:43:45 christos Exp $ */
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
static const char sccsid[] = "Id: v_at.c,v 10.11 2001/06/25 15:19:30 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:30 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: v_at.c,v 1.4 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../common/common.h"
#include "vi.h"

/*
 * v_at -- @
 *	Execute a buffer.
 *
 * PUBLIC: int v_at __P((SCR *, VICMD *));
 */
int
v_at(SCR *sp, VICMD *vp)
{
	CB *cbp;
	ARG_CHAR_T name;
	TEXT *tp;
	size_t len;
	char nbuf[20];
	CHAR_T wbuf[20];
	const CHAR_T *wp;
	size_t wlen;

	/*
	 * !!!
	 * Historically, [@*]<carriage-return> and [@*][@*] executed the most
	 * recently executed buffer in ex mode.  In vi mode, only @@ repeated
	 * the last buffer.  We change historic practice and make @* work from
	 * vi mode as well, it's simpler and more consistent.
	 *
	 * My intent is that *[buffer] will, in the future, pass the buffer to
	 * whatever interpreter is loaded.
	 */
	name = F_ISSET(vp, VC_BUFFER) ? vp->buffer : '@';
	if (name == '@' || name == '*') {
		if (!F_ISSET(sp, SC_AT_SET)) {
			ex_emsg(sp, NULL, EXM_NOPREVBUF);
			return (1);
		}
		name = sp->at_lbuf;
	}
	F_SET(sp, SC_AT_SET);

	CBNAME(sp, cbp, name);
	if (cbp == NULL) {
		ex_emsg(sp, (char *)KEY_NAME(sp, name), EXM_EMPTYBUF);
		return (1);
	}

	/* Save for reuse. */
	sp->at_lbuf = name;

	/*
	 * The buffer is executed in vi mode, while in vi mode, so simply
	 * push it onto the terminal queue and continue.
	 *
	 * !!!
	 * Historic practice is that if the buffer was cut in line mode,
	 * <newlines> were appended to each line as it was pushed onto
	 * the stack.  If the buffer was cut in character mode, <newlines>
	 * were appended to all lines but the last one.
	 *
	 * XXX
	 * Historic practice is that execution of an @ buffer could be
	 * undone by a single 'u' command, i.e. the changes were grouped
	 * together.  We don't get this right; I'm waiting for the new DB
	 * logging code to be available.
	 */
	TAILQ_FOREACH_REVERSE(tp, &cbp->textq, _texth, q) {
		static CHAR_T nl[] = { '\n', 0 };
		if (((F_ISSET(cbp, CB_LMODE) ||
		    TAILQ_NEXT(tp, q) != NULL) &&
		    v_event_push(sp, NULL, nl, 1, 0)) ||
		    v_event_push(sp, NULL, tp->lb, tp->len, 0))
			return (1);
	}

	/*
	 * !!!
	 * If any count was supplied, it applies to the first command in the
	 * at buffer.
	 */
	if (F_ISSET(vp, VC_C1SET)) {
		len = snprintf(nbuf, sizeof(nbuf), "%lu", vp->count);
		CHAR2INT(sp, nbuf, len, wp, wlen);
		MEMCPYW(wbuf, wp, wlen);
		if (v_event_push(sp, NULL, wp, wlen, 0))
			return (1);
	}
	return (0);
}
