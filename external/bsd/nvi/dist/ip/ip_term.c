/*	$NetBSD: ip_term.c,v 1.4 2014/01/26 21:43:45 christos Exp $	*/
/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: ip_term.c,v 8.9 2001/06/25 15:19:24 skimo Exp  (Berkeley) Date: 2001/06/25 15:19:24 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: ip_term.c,v 1.4 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
#include <string.h>
 
#include "../common/common.h"
#include "../ipc/ip.h"

/*
 * ip_term_init --
 *	Initialize the terminal special keys.
 *
 * PUBLIC: int ip_term_init __P((SCR *));
 */
int
ip_term_init(SCR *sp)
{
	SEQ *qp;

	/*
	 * Rework any function key mappings that were set before the
	 * screen was initialized.
	 */
	LIST_FOREACH(qp, &sp->gp->seqq, q)
		if (F_ISSET(qp, SEQ_FUNCMAP))
			(void)ip_fmap(sp, qp->stype,
			    qp->input, qp->ilen, qp->output, qp->olen);
	return (0);
}

/*
 * ip_term_end --
 *	End the special keys defined by the termcap/terminfo entry.
 *
 * PUBLIC: int ip_term_end __P((GS *));
 */
int
ip_term_end(GS *gp)
{
	SEQ *qp, *nqp;

	/* Delete screen specific mappings. */
	LIST_FOREACH_SAFE(qp, &gp->seqq, q, nqp) {
		if (F_ISSET(qp, SEQ_SCREEN))
			(void)seq_mdel(qp);
	}
	return (0);
}

/*
 * ip_fmap --
 *	Map a function key.
 *
 * PUBLIC: int ip_fmap __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
 */
int
ip_fmap(SCR *sp, seq_t stype, CHAR_T *from, size_t flen, CHAR_T *to, size_t tlen)
{
	/* Bind a function key to a string sequence. */
	return (1);
}

/*
 * ip_optchange --
 *	IP screen specific "option changed" routine.
 *
 * PUBLIC: int ip_optchange __P((SCR *, int, const char *, u_long *));
 */
int
ip_optchange(SCR *sp, int offset, const char *str, u_long *valp)
{
	IP_BUF ipb;
	OPTLIST const *opt;
	IP_PRIVATE *ipp = IPP(sp);

	switch (offset) {
	case O_COLUMNS:
	case O_LINES:
		F_SET(sp->gp, G_SRESTART);
		F_CLR(sp, SC_SCR_EX | SC_SCR_VI);
		break;
	case O_TERM:
		/* Called with "ip_curses"; previously wasn't shown
		 * because switching to EX wasn't allowed
		msgq(sp, M_ERR, "The screen type may not be changed");
		*/
		return (1);
	}

	opt = optlist + offset;
	switch (opt->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
	case OPT_NUM:
		ipb.val1 = *valp;
		ipb.len2 = 0;
		break;
	case OPT_STR:
		if (str == NULL) {
			ipb.str2 = "";
			ipb.len2 = 1;
		} else {
			ipb.str2 = str;
			ipb.len2 = strlen(str) + 1;
		}
		break;
	}

	ipb.code = SI_EDITOPT;
	ipb.str1 = __UNCONST(opt->name);
	ipb.len1 = STRLEN(opt->name) * sizeof(CHAR_T);

	(void)vi_send(ipp->o_fd, "ab1", &ipb);
	return (0);
}
