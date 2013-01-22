/*	$NetBSD: ip_read.c,v 1.1.1.2 2008/05/18 14:31:24 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: ip_read.c,v 8.23 2001/06/25 15:19:24 skimo Exp (Berkeley) Date: 2001/06/25 15:19:24";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
 
#include "../common/common.h"
#include "../ex/script.h"
#include "../ipc/ip.h"
#include "extern.h"

extern GS *__global_list;

VIPFUNLIST const vipfuns[] = {
/* VI_C_BOL	    Cursor to start of line. */
    {"",    E_IPCOMMAND},
/* VI_C_BOTTOM	 2	/* Cursor to bottom. */
    {"",    E_IPCOMMAND},
/* VI_C_DEL	 3	/* Cursor delete. */
    {"",    E_IPCOMMAND},
/* VI_C_DOWN	    Cursor down N lines: IPO_INT. */
    {"1",   E_IPCOMMAND},
/* VI_C_EOL	 5	/* Cursor to end of line. */
    {"",    E_IPCOMMAND},
/* VI_C_INSERT	 6	/* Cursor: enter insert mode. */
    {"",    E_IPCOMMAND},
/* VI_C_LEFT	 7	/* Cursor left. */
    {"",    E_IPCOMMAND},
/* VI_C_PGDOWN	 8	/* Cursor down N pages: IPO_INT. */
    {"1",   E_IPCOMMAND},
/* VI_C_PGUP	 9	/* Cursor up N lines: IPO_INT. */
    {"1",   E_IPCOMMAND},
/* VI_C_RIGHT	10	/* Cursor right. */
    {"",    E_IPCOMMAND},
/* VI_C_SEARCH	11	/* Cursor: search: IPO_INT, IPO_STR. */
    {"a1",  E_IPCOMMAND},
/* VI_C_SETTOP	12	/* Cursor: set screen top line: IPO_INT. */
    {"1",   E_IPCOMMAND},
/* VI_C_TOP	13	/* Cursor to top. */
    {"",    E_IPCOMMAND},
/* VI_C_UP		14	/* Cursor up N lines: IPO_INT. */
    {"1",   E_IPCOMMAND},
/* VI_EDIT		15	/* Edit a file: IPO_STR. */
    {"a",   E_IPCOMMAND},
/* VI_EDITOPT	16	/* Edit option: 2 * IPO_STR, IPO_INT. */
    {"ab1", E_IPCOMMAND},
/* VI_EDITSPLIT	17	/* Split to a file: IPO_STR. */
    {"a",   E_IPCOMMAND},
/* VI_EOF		18	/* End of input (NOT ^D). */
    {"",    E_EOF},
/* VI_ERR		19	/* Input error. */
    {"",    E_ERR},
/* VI_FLAGS	    Flags */
    {"1",   E_FLAGS},
/* VI_INTERRUPT	20	/* Interrupt. */
    {"",    E_INTERRUPT},
/* VI_MOUSE_MOVE	21	/* Mouse click move: IPO_INT, IPO_INT. */
    {"12",  E_IPCOMMAND},
/* VI_QUIT		22	/* Quit. */
    {"",    E_IPCOMMAND},
/* VI_RESIZE	    Screen resize: IPO_INT, IPO_INT. */
    {"12",  E_WRESIZE},
/* VI_SEL_END	24	/* Select end: IPO_INT, IPO_INT. */
    {"12",  E_IPCOMMAND},
/* VI_SEL_START	25	/* Select start: IPO_INT, IPO_INT. */
    {"12",  E_IPCOMMAND},
/* VI_SIGHUP	26	/* SIGHUP. */
    {"",    E_SIGHUP},
/* VI_SIGTERM	27	/* SIGTERM. */
    {"",    E_SIGTERM},
/* VI_STRING	    Input string: IPO_STR. */
    {"a",   E_STRING},
/* VI_TAG		29	/* Tag. */
    {"",    E_IPCOMMAND},
/* VI_TAGAS	30	/* Tag to a string: IPO_STR. */
    {"a",   E_IPCOMMAND},
/* VI_TAGSPLIT	31	/* Split to a tag. */
    {"",    E_IPCOMMAND},
/* VI_UNDO		32	/* Undo. */
    {"",    E_IPCOMMAND},
/* VI_WQ		33	/* Write and quit. */
    {"",    E_IPCOMMAND},
/* VI_WRITE	34	/* Write. */
    {"",    E_IPCOMMAND},
/* VI_WRITEAS	35	/* Write as another file: IPO_STR. */
    {"a",   E_IPCOMMAND},
/* VI_EVENT_SUP */
};

typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_TIMEOUT } input_t;

static input_t	ip_read __P((SCR *, IP_PRIVATE *, struct timeval *, int, int*));
static int	ip_resize __P((SCR *, u_int32_t, u_int32_t));
static int	ip_trans __P((SCR *, IP_PRIVATE *, EVENT *));

/*
 * ip_event --
 *	Return a single event.
 *
 * PUBLIC: int ip_event __P((SCR *, EVENT *, u_int32_t, int));
 */
int
ip_event(SCR *sp, EVENT *evp, u_int32_t flags, int ms)
{
	return ip_wevent(sp->wp, sp, evp, flags, ms);
}

/*
 * XXX probably better to require new_window to send size
 *     so we never have to call ip_wevent with sp == NULL
 *
 * ip_wevent --
 *	Return a single event.
 *
 * PUBLIC: int ip_wevent __P((WIN *, SCR *, EVENT *, u_int32_t, int));
 */
int
ip_wevent(WIN *wp, SCR *sp, EVENT *evp, u_int32_t flags, int ms)
{
	IP_PRIVATE *ipp;
	struct timeval t, *tp;
	int termread;
	int nr;

	if (LF_ISSET(EC_INTERRUPT)) {		/* XXX */
		evp->e_event = E_TIMEOUT;
		return (0);
	}

	ipp = sp == NULL ? WIPP(wp) : IPP(sp);

	/* Discard the last command. */
	if (ipp->iskip != 0) {
		ipp->iblen -= ipp->iskip;
		memmove(ipp->ibuf, ipp->ibuf + ipp->iskip, ipp->iblen);
		ipp->iskip = 0;
	}

	termread = F_ISSET(ipp, IP_IN_EX) ||
		    (sp && F_ISSET(sp, SC_SCR_EXWROTE));

	/* Process possible remaining commands */
	if (!termread && ipp->iblen >= IPO_CODE_LEN && ip_trans(sp, ipp, evp))
		return 0;

	/* Set timer. */
	if (ms == 0)
		tp = NULL;
	else {
		t.tv_sec = ms / 1000;
		t.tv_usec = (ms % 1000) * 1000;
		tp = &t;
	}

	/* Read input events. */
	for (;;) {
		switch (ip_read(sp, ipp, tp, termread, &nr)) {
		case INP_OK:
			if (termread) {
				evp->e_csp = ipp->tbuf;
				evp->e_len = nr;
				evp->e_event = E_STRING;
			} else if (!ip_trans(sp, ipp, evp))
				continue;
			break;
		case INP_EOF:
			evp->e_event = E_EOF;
			break;
		case INP_ERR:
			evp->e_event = E_ERR;
			break;
		case INP_TIMEOUT:
			evp->e_event = E_TIMEOUT;
			break;
		default:
			abort();
		}
		break;
	}
	return (0);
}

/*
 * ip_read --
 *	Read characters from the input.
 */
static input_t
ip_read(SCR *sp, IP_PRIVATE *ipp, struct timeval *tp, int termread, int *nr)
{
	struct timeval poll;
	GS *gp;
	fd_set rdfd;
	input_t rval;
	size_t blen;
	int maxfd;
	char *bp;
	int fd;
	CHAR_T *wp;
	size_t wlen;

	gp = sp == NULL ? __global_list : sp->gp;
	bp = ipp->ibuf + ipp->iblen;
	blen = sizeof(ipp->ibuf) - ipp->iblen;
	fd = termread ? ipp->t_fd : ipp->i_fd;

	/*
	 * 1: A read with an associated timeout, e.g., trying to complete
	 *    a map sequence.  If input exists, we fall into #2.
	 */
	FD_ZERO(&rdfd);
	poll.tv_sec = 0;
	poll.tv_usec = 0;
	if (tp != NULL) {
		FD_SET(fd, &rdfd);
		switch (select(fd + 1,
		    &rdfd, NULL, NULL, tp == NULL ? &poll : tp)) {
		case 0:
			return (INP_TIMEOUT);
		case -1:
			goto err;
		default:
			break;
		}
	}
	
	/*
	 * 2: Wait for input.
	 *
	 * Select on the command input and scripting window file descriptors.
	 * It's ugly that we wait on scripting file descriptors here, but it's
	 * the only way to keep from locking out scripting windows.
	 */
	if (sp != NULL && F_ISSET(gp, G_SCRWIN)) {
		FD_ZERO(&rdfd);
		FD_SET(fd, &rdfd);
		maxfd = fd;
		if (sscr_check_input(sp, &rdfd, maxfd))
			goto err;
	}

	/*
	 * 3: Read the input.
	 */
	switch (*nr = read(fd, termread ? (char *)ipp->tbuf : bp, 
			      termread ? sizeof(ipp->tbuf)/sizeof(CHAR_T) 
				       : blen)) {
	case  0:				/* EOF. */
		rval = INP_EOF;
		break;
	case -1:				/* Error or interrupt. */
err:	        rval = INP_ERR;
		msgq(sp, M_SYSERR, "input");
		break;
	default:				/* Input characters. */
		if (!termread) ipp->iblen += *nr;
		else {
			CHAR2INT(sp, (char *)ipp->tbuf, *nr, wp, wlen);
			MEMMOVEW(ipp->tbuf, wp, wlen);
		}
		rval = INP_OK;
		break;
	}
	return (rval);
}

/*
 * ip_trans --
 *	Translate messages into events.
 */
static int
ip_trans(SCR *sp, IP_PRIVATE *ipp, EVENT *evp)
{
	u_int32_t skip, val;
	char *fmt;
	CHAR_T *wp;
	size_t wlen;

	if (ipp->ibuf[0] == CODE_OOB ||
	    ipp->ibuf[0] >= VI_EVENT_SUP)
	{
		/*
		 * XXX: Protocol is out of sync?
		 */
		abort();
	}
	fmt = vipfuns[ipp->ibuf[0]-1].format;
	evp->e_event = vipfuns[ipp->ibuf[0]-1].e_event;
	evp->e_ipcom = ipp->ibuf[0];

	for (skip = IPO_CODE_LEN; *fmt != '\0'; ++fmt)
		switch (*fmt) {
		case '1':
		case '2':
			if (ipp->iblen < skip + IPO_INT_LEN)
				return (0);
			memcpy(&val, ipp->ibuf + skip, IPO_INT_LEN);
			val = ntohl(val);
			if (*fmt == '1')
				evp->e_val1 = val;
			else
				evp->e_val2 = val;
			skip += IPO_INT_LEN;
			break;
		case 'a':
		case 'b':
			if (ipp->iblen < skip + IPO_INT_LEN)
				return (0);
			memcpy(&val, ipp->ibuf + skip, IPO_INT_LEN);
			val = ntohl(val);
			skip += IPO_INT_LEN;
			if (ipp->iblen < skip + val)
				return (0);
			if (*fmt == 'a') {
				CHAR2INT(sp, ipp->ibuf + skip, val,
					 wp, wlen);
				MEMCPYW(ipp->tbuf, wp, wlen);
				evp->e_str1 = ipp->tbuf;
				evp->e_len1 = wlen;
			} else {
				CHAR2INT(sp, ipp->ibuf + skip, val,
					 wp, wlen);
				MEMCPYW(ipp->tbuf, wp, wlen);
				evp->e_str2 = ipp->tbuf;
				evp->e_len2 = wlen;
			}
			skip += val;
			break;
		}

	ipp->iskip = skip;

	if (evp->e_event == E_WRESIZE)
		(void)ip_resize(sp, evp->e_val1, evp->e_val2);

	return (1);
}

/* 
 * ip_resize --
 *	Reset the options for a resize event.
 */
static int
ip_resize(SCR *sp, u_int32_t lines, u_int32_t columns)
{
	GS *gp;
	int rval;

	/*
	 * XXX
	 * The IP screen has to know the lines and columns before anything
	 * else happens.  So, we may not have a valid SCR pointer, and we
	 * have to deal with that.
	 */
	if (sp == NULL) {
		gp = __global_list;
		OG_VAL(gp, GO_LINES) = OG_D_VAL(gp, GO_LINES) = lines;
		OG_VAL(gp, GO_COLUMNS) = OG_D_VAL(gp, GO_COLUMNS) = columns;
		return (0);
	}

	rval = api_opts_set(sp, L("lines"), NULL, lines, 0);
	if (api_opts_set(sp, L("columns"), NULL, columns, 0))
		rval = 1;
	return (rval);
}
