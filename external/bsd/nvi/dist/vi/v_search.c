/*	$NetBSD: v_search.c,v 1.6 2014/01/26 21:43:45 christos Exp $ */
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
static const char sccsid[] = "Id: v_search.c,v 10.30 2001/09/11 20:52:46 skimo Exp  (Berkeley) Date: 2001/09/11 20:52:46 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: v_search.c,v 1.6 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "vi.h"
#include "../ipc/ip.h"

static int v_exaddr __P((SCR *, VICMD *, dir_t));
static int v_search __P((SCR *, VICMD *, CHAR_T *, size_t, u_int, dir_t));

/*
 * v_srch -- [count]?RE[? offset]
 *	Ex address search backward.
 *
 * PUBLIC: int v_searchb __P((SCR *, VICMD *));
 */
int
v_searchb(SCR *sp, VICMD *vp)
{
	return (v_exaddr(sp, vp, BACKWARD));
}

/*
 * v_searchf -- [count]/RE[/ offset]
 *	Ex address search forward.
 *
 * PUBLIC: int v_searchf __P((SCR *, VICMD *));
 */
int
v_searchf(SCR *sp, VICMD *vp)
{
	return (v_exaddr(sp, vp, FORWARD));
}

/*
 * v_exaddr --
 *	Do a vi search (which is really an ex address).
 */
static int
v_exaddr(SCR *sp, VICMD *vp, dir_t dir)
{
	static EXCMDLIST fake = { .name = L("search") };
	EXCMD *cmdp;
	WIN *wp;
	TEXT *tp;
	db_recno_t s_lno;
	size_t len, s_cno, tlen;
	int error, nb, type;
	char buf[20];
	CHAR_T *cmd, *t;
	const CHAR_T *w;
	size_t wlen;

	/*
	 * !!!
	 * If using the search command as a motion, any addressing components
	 * are lost, i.e. y/ptrn/+2, when repeated, is the same as y/ptrn/.
	 */
	if (F_ISSET(vp, VC_ISDOT))
		return (v_search(sp, vp,
		    NULL, 0, SEARCH_PARSE | SEARCH_MSG | SEARCH_SET, dir));

	/* Get the search pattern. */
	if (v_tcmd(sp, vp, dir == BACKWARD ? CH_BSEARCH : CH_FSEARCH,
	    TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT |
	    (O_ISSET(sp, O_SEARCHINCR) ? TXT_SEARCHINCR : 0)))
		return (1);

	tp = TAILQ_FIRST(&sp->tiq);

	/* If the user backspaced over the prompt, do nothing. */
	if (tp->term == TERM_BS)
		return (1);

	/*
	 * If the user was doing an incremental search, then we've already
	 * updated the cursor and moved to the right location.  Return the
	 * correct values, we're done.
	 */
	if (tp->term == TERM_SEARCH) {
		vp->m_stop.lno = sp->lno;
		vp->m_stop.cno = sp->cno;
		if (ISMOTION(vp))
			return (v_correct(sp, vp, 0));
		vp->m_final = vp->m_stop;
		return (0);
	}

	/*
	 * If the user entered <escape> or <carriage-return>, the length is
	 * 1 and the right thing will happen, i.e. the prompt will be used
	 * as a command character.
	 *
	 * Build a fake ex command structure.
	 */
	wp = sp->wp;
	wp->excmd.cp = tp->lb;
	wp->excmd.clen = tp->len;
	F_INIT(&wp->excmd, E_VISEARCH);

	/*
	 * XXX
	 * Warn if the search wraps.  This is a pretty special case, but it's
	 * nice feature that wasn't in the original implementations of ex/vi.
	 * (It was added at some point to System V's version.)  This message
	 * is only displayed if there are no keys in the queue. The problem is
	 * the command is going to succeed, and the message is informational,
	 * not an error.  If a macro displays it repeatedly, e.g., the pattern
	 * only occurs once in the file and wrapscan is set, you lose big.  For
	 * example, if the macro does something like:
	 *
	 *	:map K /pattern/^MjK
	 *
	 * Each search will display the message, but the following "/pattern/"
	 * will immediately overwrite it, with strange results.  The System V
	 * vi displays the "wrapped" message multiple times, but because it's
	 * overwritten each time, it's not as noticeable.  As we don't discard
	 * messages, it's a real problem for us.
	 */
	if (!KEYS_WAITING(sp))
		F_SET(&wp->excmd, E_SEARCH_WMSG);
		
	/* Save the current line/column. */
	s_lno = sp->lno;
	s_cno = sp->cno;

	/*
	 * !!!
	 * Historically, vi / and ? commands were full-blown ex addresses,
	 * including ';' delimiters, trailing <blank>'s, multiple search
	 * strings (separated by semi-colons) and, finally, full-blown z
	 * commands after the / and ? search strings.  (If the search was
	 * being used as a motion, the trailing z command was ignored.
	 * Also, we do some argument checking on the z command, to be sure
	 * that it's not some other random command.) For multiple search
	 * strings, leading <blank>'s at the second and subsequent strings
	 * were eaten as well.  This has some (unintended?) side-effects:
	 * the command /ptrn/;3 is legal and results in moving to line 3.
	 * I suppose you could use it to optionally move to line 3...
	 *
	 * !!!
	 * Historically, if any part of the search command failed, the cursor
	 * remained unmodified (even if ; was used).  We have to play games
	 * because the underlying ex parser thinks we're modifying the cursor
	 * as we go, but I think we're compatible with historic practice.
	 *
	 * !!!
	 * Historically, the command "/STRING/;   " failed, apparently it
	 * confused the parser.  We're not that compatible.
	 */
	cmdp = &wp->excmd;
	if (ex_range(sp, cmdp, &error))
		return (1);
	
	/*
	 * Remember where any remaining command information is, and clean
	 * up the fake ex command.
	 */
	cmd = cmdp->cp;
	len = cmdp->clen;
	wp->excmd.clen = 0;

	if (error)
		goto err2;

	/* Copy out the new cursor position and make sure it's okay. */
	switch (cmdp->addrcnt) {
	case 1:
		vp->m_stop = cmdp->addr1;
		break;
	case 2:
		vp->m_stop = cmdp->addr2;
		break;
	}
	if (!db_exist(sp, vp->m_stop.lno)) {
		ex_badaddr(sp, &fake,
		    vp->m_stop.lno == 0 ? A_ZERO : A_EOF, NUM_OK);
		goto err2;
	}

	/*
	 * !!!
	 * Historic practice is that a trailing 'z' was ignored if it was a
	 * motion command.  Should probably be an error, but not worth the
	 * effort.
	 */
	if (ISMOTION(vp))
		return (v_correct(sp, vp, F_ISSET(cmdp, E_DELTA)));
		
	/*
	 * !!!
	 * Historically, if it wasn't a motion command, a delta in the search
	 * pattern turns it into a first nonblank movement.
	 */
	nb = F_ISSET(cmdp, E_DELTA);

	/* Check for the 'z' command. */
	if (len != 0) {
		if (*cmd != 'z')
			goto err1;

		/* No blanks, just like the z command. */
		for (t = cmd + 1, tlen = len - 1; tlen > 0; ++t, --tlen)
			if (!ISDIGIT((UCHAR_T)*t))
				break;
		if (tlen &&
		    (*t == '-' || *t == '.' || *t == '+' || *t == '^')) {
			++t;
			--tlen;
			type = 1;
		} else
			type = 0;
		if (tlen)
			goto err1;

		/* The z command will do the nonblank for us. */
		nb = 0;

		/* Default to z+. */
		if (!type &&
		    v_event_push(sp, NULL, L("+"), 1, CH_NOMAP | CH_QUOTED))
			return (1);

		/* Push the user's command. */
		if (v_event_push(sp, NULL, cmd, len, CH_NOMAP | CH_QUOTED))
			return (1);

		/* Push line number so get correct z display. */
		tlen = snprintf(buf,
		    sizeof(buf), "%lu", (u_long)vp->m_stop.lno);
		CHAR2INT(sp, buf, tlen, w, wlen);
		if (v_event_push(sp, NULL, w, wlen, CH_NOMAP | CH_QUOTED))
			return (1);
		 
		/* Don't refresh until after 'z' happens. */
		F_SET(VIP(sp), VIP_S_REFRESH);
	}

	/* Non-motion commands move to the end of the range. */
	vp->m_final = vp->m_stop;
	if (nb) {
		F_CLR(vp, VM_RCM_MASK);
		F_SET(vp, VM_RCM_SETFNB);
	}
	return (0);

err1:	msgq(sp, M_ERR,
	    "188|Characters after search string, line offset and/or z command");
err2:	vp->m_final.lno = s_lno;
	vp->m_final.cno = s_cno;
	return (1);
}

/*
 * v_searchN -- N
 *	Reverse last search.
 *
 * PUBLIC: int v_searchN __P((SCR *, VICMD *));
 */
int
v_searchN(SCR *sp, VICMD *vp)
{
	dir_t dir;

	switch (sp->searchdir) {
	case BACKWARD:
		dir = FORWARD;
		break;
	case FORWARD:
		dir = BACKWARD;
		break;
	default:
		dir = sp->searchdir;
		break;
	}
	return (v_search(sp, vp, NULL, 0, SEARCH_PARSE, dir));
}

/*
 * v_searchn -- n
 *	Repeat last search.
 *
 * PUBLIC: int v_searchn __P((SCR *, VICMD *));
 */
int
v_searchn(SCR *sp, VICMD *vp)
{
	return (v_search(sp, vp, NULL, 0, SEARCH_PARSE, sp->searchdir));
}

/*
 * is_especial --
 *	Test if the character is special in an extended RE.
 */
static int
is_especial(CHAR_T c)
{
	/*
	 * !!!
	 * Right-brace is not an ERE special according to IEEE 1003.1-2001.
	 * Right-parenthesis is a special character (so quoting doesn't hurt),
	 * though it has no special meaning in this context, viz. at the
	 * beginning of the string.  So we need not quote it.  Then again,
	 * see the BUGS section in regex/re_format.7.
	 * The tilde is vi-specific, of course.
	 */
	return (STRCHR(L(".[\\()*+?{|^$~"), c) && c);
}

/*
 * Rear delimiter for word search when the keyword ends in
 * (i.e., consists of) a non-word character.  See v_searchw below.
 */
#define RE_NWSTOP	L("([^[:alnum:]_]|$)")
#define RE_NWSTOP_LEN	(SIZE(RE_NWSTOP) - 1)

/*
 * v_searchw -- [count]^A
 *	Search for the word under the cursor.
 *
 * PUBLIC: int v_searchw __P((SCR *, VICMD *));
 */
int
v_searchw(SCR *sp, VICMD *vp)
{
	size_t blen, len;
	int rval;
	CHAR_T *bp, *p;

	len = VIP(sp)->klen + MAX(RE_WSTART_LEN, 1)
	    + MAX(RE_WSTOP_LEN, RE_NWSTOP_LEN);

	GET_SPACE_RETW(sp, bp, blen, len);
	p = bp;

	/* Only the first character can be non-word, see v_curword. */
	if (inword(VIP(sp)->keyw[0]))
		p = MEMPCPY(p, RE_WSTART, RE_WSTART_LEN);
	else if (is_especial(VIP(sp)->keyw[0]))
		p = MEMPCPY(p, L("\\"), 1);

	p = MEMPCPY(p, VIP(sp)->keyw, VIP(sp)->klen);

	if (inword(p[-1]))
		p = MEMPCPY(p, RE_WSTOP, RE_WSTOP_LEN);
	else
		/*
		 * The keyword is a single non-word character.
		 * We want it to stay the same when typing ^A several times
		 * in a row, just the way the other cases behave.
		 */
		p = MEMPCPY(p, RE_NWSTOP, RE_NWSTOP_LEN);

	len = p - bp;
	rval = v_search(sp, vp, bp, len, SEARCH_SET | SEARCH_EXTEND, FORWARD);

	FREE_SPACEW(sp, bp, blen);
	return (rval);
}

/*
 * v_esearch -- <dialog box>
 *	Search command from the screen.
 *
 * PUBLIC: int v_esearch __P((SCR *, VICMD *));
 */
int
v_esearch(SCR *sp, VICMD *vp)
{
	int flags;

	LF_INIT(SEARCH_NOOPT);
	if (FL_ISSET(vp->ev.e_flags, VI_SEARCH_EXT))
		LF_SET(SEARCH_EXTEND);
	if (FL_ISSET(vp->ev.e_flags, VI_SEARCH_IC))
		LF_SET(SEARCH_IC);
	if (FL_ISSET(vp->ev.e_flags, VI_SEARCH_ICL))
		LF_SET(SEARCH_ICL);
	if (FL_ISSET(vp->ev.e_flags, VI_SEARCH_INCR))
		LF_SET(SEARCH_INCR);
	if (FL_ISSET(vp->ev.e_flags, VI_SEARCH_LIT))
		LF_SET(SEARCH_LITERAL);
	if (FL_ISSET(vp->ev.e_flags, VI_SEARCH_WR))
		LF_SET(SEARCH_WRAP);
	return (v_search(sp, vp, vp->ev.e_csp, vp->ev.e_len, flags,
	    FL_ISSET(vp->ev.e_flags, VI_SEARCH_REV) ? BACKWARD : FORWARD));
}

/*
 * v_search --
 *	The search commands.
 */
static int
v_search(SCR *sp, VICMD *vp, CHAR_T *ptrn, size_t plen, u_int flags, dir_t dir)
{
	/* Display messages. */
	LF_SET(SEARCH_MSG);

	/* If it's a motion search, offset past end-of-line is okay. */
	if (ISMOTION(vp))
		LF_SET(SEARCH_EOL);

	/*
	 * XXX
	 * Warn if the search wraps.  See the comment above, in v_exaddr().
	 */
	if (!KEYS_WAITING(sp))
		LF_SET(SEARCH_WMSG);
		
	switch (dir) {
	case BACKWARD:
		if (b_search(sp,
		    &vp->m_start, &vp->m_stop, ptrn, plen, NULL, flags))
			return (1);
		break;
	case FORWARD:
		if (f_search(sp,
		    &vp->m_start, &vp->m_stop, ptrn, plen, NULL, flags))
			return (1);
		break;
	case NOTSET:
		msgq(sp, M_ERR, "189|No previous search pattern");
		return (1);
	default:
		abort();
	}

	/* Correct motion commands, otherwise, simply move to the location. */
	if (ISMOTION(vp)) {
		if (v_correct(sp, vp, 0))
			return(1);
	} else
		vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_correct --
 *	Handle command with a search as the motion.
 *
 * !!!
 * Historically, commands didn't affect the line searched to/from if the
 * motion command was a search and the final position was the start/end
 * of the line.  There were some special cases and vi was not consistent;
 * it was fairly easy to confuse it.  For example, given the two lines:
 *
 *	abcdefghi
 *	ABCDEFGHI
 *
 * placing the cursor on the 'A' and doing y?$ would so confuse it that 'h'
 * 'k' and put would no longer work correctly.  In any case, we try to do
 * the right thing, but it's not going to exactly match historic practice.
 *
 * PUBLIC: int v_correct __P((SCR *, VICMD *, int));
 */
int
v_correct(SCR *sp, VICMD *vp, int isdelta)
{
	MARK m;
	size_t len;

	/*
	 * !!!
	 * We may have wrapped if wrapscan was set, and we may have returned
	 * to the position where the cursor started.  Historic vi didn't cope
	 * with this well.  Yank wouldn't beep, but the first put after the
	 * yank would move the cursor right one column (without adding any
	 * text) and the second would put a copy of the current line.  The
	 * change and delete commands would beep, but would leave the cursor
	 * on the colon command line.  I believe that there are macros that
	 * depend on delete, at least, failing.  For now, commands that use
	 * search as a motion component fail when the search returns to the
	 * original cursor position.
	 */
	if (vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno == vp->m_stop.cno) {
		msgq(sp, M_BERR, "190|Search wrapped to original position");
		return (1);
	}

	/*
	 * !!!
	 * Searches become line mode operations if there was a delta specified
	 * to the search pattern.
	 */
	if (isdelta)
		F_SET(vp, VM_LMODE);

	/*
	 * If the motion is in the reverse direction, switch the start and
	 * stop MARK's so that it's in a forward direction.  (There's no
	 * reason for this other than to make the tests below easier.  The
	 * code in vi.c:vi() would have done the switch.)  Both forward
	 * and backward motions can happen for any kind of search command
	 * because of the wrapscan option.
	 */
	if (vp->m_start.lno > vp->m_stop.lno ||
	    (vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno > vp->m_stop.cno)) {
		m = vp->m_start;
		vp->m_start = vp->m_stop;
		vp->m_stop = m;
	}

	/*
	 * BACKWARD:
	 *	Delete and yank commands move to the end of the range.
	 *	Ignore others.
	 *
	 * FORWARD:
	 *	Delete and yank commands don't move.  Ignore others.
	 */
	vp->m_final = vp->m_start;

	/*
	 * !!!
	 * Delta'd searches don't correct based on column positions.
	 */
	if (isdelta)
		return (0);

	/*
	 * !!!
	 * Backward searches starting at column 0, and forward searches ending
	 * at column 0 are corrected to the last column of the previous line.
	 * Otherwise, adjust the starting/ending point to the character before
	 * the current one (this is safe because we know the search had to move
	 * to succeed).
	 *
	 * Searches become line mode operations if they start at the first
	 * nonblank and end at column 0 of another line.
	 */
	if (vp->m_start.lno < vp->m_stop.lno && vp->m_stop.cno == 0) {
		if (db_get(sp, --vp->m_stop.lno, DBG_FATAL, NULL, &len))
			return (1);
		vp->m_stop.cno = len ? len - 1 : 0;
		len = 0;
		if (nonblank(sp, vp->m_start.lno, &len))
			return (1);
		if (vp->m_start.cno <= len)
			F_SET(vp, VM_LMODE);
	} else
		--vp->m_stop.cno;

	return (0);
}
