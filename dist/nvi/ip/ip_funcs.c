/*	$NetBSD: ip_funcs.c,v 1.1.1.2 2008/05/18 14:31:24 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: ip_funcs.c,v 8.23 2001/06/25 15:19:23 skimo Exp (Berkeley) Date: 2001/06/25 15:19:23";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "../ipc/ip.h"
#include "extern.h"

/*
 * ip_addstr --
 *	Add len bytes from the string at the cursor, advancing the cursor.
 *
 * PUBLIC: int ip_waddstr __P((SCR *, const CHAR_T *, size_t));
 */
int
ip_waddstr(SCR *sp, const CHAR_T *str, size_t len)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp;
	int iv, rval;

	ipp = IPP(sp);

	ipb.code = SI_WADDSTR;
	ipb.len1 = len * sizeof(CHAR_T);
	ipb.str1 = (char *)str;
	rval = vi_send(ipp->o_fd, "a", &ipb);
	/* XXXX */
	ipp->col += len;

	return (rval);
}

/*
 * ip_addstr --
 *	Add len bytes from the string at the cursor, advancing the cursor.
 *
 * PUBLIC: int ip_addstr __P((SCR *, const char *, size_t));
 */
int
ip_addstr(SCR *sp, const char *str, size_t len)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp;
	int iv, rval;

	ipp = IPP(sp);

	/*
	 * If ex isn't in control, it's the last line of the screen and
	 * it's a split screen, use inverse video.
	 */
	iv = 0;
	if (!F_ISSET(sp, SC_SCR_EXWROTE) &&
	    ipp->row == LASTLINE(sp) && IS_SPLIT(sp)) {
		iv = 1;
		ip_attr(sp, SA_INVERSE, 1);
	}
	ipb.code = SI_ADDSTR;
	ipb.len1 = len;
	ipb.str1 = str;
	rval = vi_send(ipp->o_fd, "a", &ipb);
	/* XXXX */
	ipp->col += len;

	if (iv)
		ip_attr(sp, SA_INVERSE, 0);
	return (rval);
}

/*
 * ip_attr --
 *	Toggle a screen attribute on/off.
 *
 * PUBLIC: int ip_attr __P((SCR *, scr_attr_t, int));
 */
int
ip_attr(SCR *sp, scr_attr_t attribute, int on)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp = IPP(sp);

	if (attribute == SA_ALTERNATE) {
		if (on) F_SET(ipp, IP_ON_ALTERNATE);
		else F_CLR(ipp, IP_ON_ALTERNATE);
	}

	ipb.code = SI_ATTRIBUTE;
	ipb.val1 = attribute;
	ipb.val2 = on;

	return (vi_send(ipp->o_fd, "12", &ipb));
}

/*
 * ip_baud --
 *	Return the baud rate.
 *
 * PUBLIC: int ip_baud __P((SCR *, u_long *));
 */
int
ip_baud(SCR *sp, u_long *ratep)
{
	*ratep = 9600;		/* XXX: Translation: fast. */
	return (0);
}

/*
 * ip_bell --
 *	Ring the bell/flash the screen.
 *
 * PUBLIC: int ip_bell __P((SCR *));
 */
int
ip_bell(SCR *sp)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp = IPP(sp);

	ipb.code = SI_BELL;

	return (vi_send(ipp->o_fd, NULL, &ipb));
}

/*
 * ip_busy --
 *	Display a busy message.
 *
 * PUBLIC: void ip_busy __P((SCR *, const char *, busy_t));
 */
void
ip_busy(SCR *sp, const char *str, busy_t bval)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp = IPP(sp);

	switch (bval) {
	case BUSY_ON:
		ipb.code = SI_BUSY_ON;
		ipb.str1 = str;
		ipb.len1 = strlen(str);
		(void)vi_send(ipp->o_fd, "a", &ipb);
		break;
	case BUSY_OFF:
		ipb.code = SI_BUSY_OFF;
		(void)vi_send(ipp->o_fd, NULL, &ipb);
		break;
	case BUSY_UPDATE:
		break;
	}
	return;
}

/*
 * ip_child --
 *	Prepare child.
 *
 * PUBLIC: int ip_child __P((SCR *));
 */
int
ip_child(SCR *sp)
{
	IP_PRIVATE *ipp = IPP(sp);

	if (ipp->t_fd != -1) {
	    dup2(ipp->t_fd, 0);
	    dup2(ipp->t_fd, 1);
	    dup2(ipp->t_fd, 2);
	    close(ipp->t_fd);
	}
	return 0;
}

/*
 * ip_clrtoeol --
 *	Clear from the current cursor to the end of the line.
 *
 * PUBLIC: int ip_clrtoeol __P((SCR *));
 */
int
ip_clrtoeol(SCR *sp)
{
	IP_BUF ipb;
 	IP_PRIVATE *ipp = IPP(sp);
 
 	/* Temporary hack until we can pass screen pointers
 	 * or name screens
 	 */
 	if (IS_VSPLIT(sp)) {
 		size_t x, y, spcnt;
 		IP_PRIVATE *ipp;
 		int error;
 
 		ipp = IPP(sp);
 		y = ipp->row;
 		x = ipp->col;
 		error = 0;
 		for (spcnt = sp->cols - x; 
 		     spcnt > 0 && ! error; --spcnt)
 			error = ip_addstr(sp, " ", 1);
 		if (sp->coff == 0)
 			error |= ip_addstr(sp, "|", 1);
 		error |= ip_move(sp, y, x);
 		return error;
 	}

	ipb.code = SI_CLRTOEOL;

	return (vi_send(ipp->o_fd, NULL, &ipb));
}

/*
 * ip_cursor --
 *	Return the current cursor position.
 *
 * PUBLIC: int ip_cursor __P((SCR *, size_t *, size_t *));
 */
int
ip_cursor(SCR *sp, size_t *yp, size_t *xp)
{
	IP_PRIVATE *ipp;

	ipp = IPP(sp);
	*yp = ipp->row;
	*xp = ipp->col;
	return (0);
}

/*
 * ip_deleteln --
 *	Delete the current line, scrolling all lines below it.
 *
 * PUBLIC: int ip_deleteln __P((SCR *));
 */
int
ip_deleteln(SCR *sp)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp = IPP(sp);

	/*
	 * This clause is required because the curses screen uses reverse
	 * video to delimit split screens.  If the screen does not do this,
	 * this code won't be necessary.
	 *
	 * If the bottom line was in reverse video, rewrite it in normal
	 * video before it's scrolled.
	 */
	if (!F_ISSET(sp, SC_SCR_EXWROTE) && IS_SPLIT(sp)) {
		ipb.code = SI_REWRITE;
		ipb.val1 = RLNO(sp, LASTLINE(sp));
		if (vi_send(ipp->o_fd, "1", &ipb))
			return (1);
	}

	/*
	 * The bottom line is expected to be blank after this operation,
	 * and other screens must support that semantic.
	 */
	ipb.code = SI_DELETELN;
	return (vi_send(ipp->o_fd, NULL, &ipb));
}

/*
 * ip_discard --
 *	Discard a screen.
 *
 * PUBLIC: int ip_discard __P((SCR *, SCR **));
 */
int
ip_discard(SCR *discardp, SCR **acquirep)
{
	return (0);
}

/* 
 * ip_ex_adjust --
 *	Adjust the screen for ex.
 *
 * PUBLIC: int ip_ex_adjust __P((SCR *, exadj_t));
 */
int
ip_ex_adjust(SCR *sp, exadj_t action)
{
	abort();
	/* NOTREACHED */
}

/*
 * ip_insertln --
 *	Push down the current line, discarding the bottom line.
 *
 * PUBLIC: int ip_insertln __P((SCR *));
 */
int
ip_insertln(SCR *sp)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp = IPP(sp);

	ipb.code = SI_INSERTLN;

	return (vi_send(ipp->o_fd, NULL, &ipb));
}

/*
 * ip_keyval --
 *	Return the value for a special key.
 *
 * PUBLIC: int ip_keyval __P((SCR *, scr_keyval_t, CHAR_T *, int *));
 */
int
ip_keyval(SCR *sp, scr_keyval_t val, CHAR_T *chp, int *dnep)
{
	/*
	 * VEOF, VERASE and VKILL are required by POSIX 1003.1-1990,
	 * VWERASE is a 4BSD extension.
	 */
	switch (val) {
	case KEY_VEOF:
		*dnep = '\004';		/* ^D */
		break;
	case KEY_VERASE:
		*dnep = '\b';		/* ^H */
		break;
	case KEY_VKILL:
		*dnep = '\025';		/* ^U */
		break;
#ifdef VWERASE
	case KEY_VWERASE:
		*dnep = '\027';		/* ^W */
		break;
#endif
	default:
		*dnep = 1;
		break;
	}
	return (0);
}

/*
 * ip_move --
 *	Move the cursor.
 *
 * PUBLIC: int ip_move __P((SCR *, size_t, size_t));
 */
int
ip_move(SCR *sp, size_t lno, size_t cno)
{
	IP_PRIVATE *ipp;
	IP_BUF ipb;

	ipp = IPP(sp);
	ipp->row = lno;
	ipp->col = cno;

	ipb.code = SI_MOVE;
	ipb.val1 = RLNO(sp, lno);
	ipb.val2 = RCNO(sp, cno);
	return (vi_send(ipp->o_fd, "12", &ipb));
}

/*
 * PUBLIC: void ip_msg __P((SCR *, mtype_t, char *, size_t));
 */
void
ip_msg(SCR *sp, mtype_t mtype, char *line, size_t len)
{
	IP_PRIVATE *ipp = IPP(sp);

	if (F_ISSET(ipp, IP_ON_ALTERNATE))
		vs_msg(sp, mtype, line, len);
	else {
		write(ipp->t_fd, line, len);
		F_CLR(sp, SC_EX_WAIT_NO);
	}
}

/*
 * ip_refresh --
 *	Refresh the screen.
 *
 * PUBLIC: int ip_refresh __P((SCR *, int));
 */
int
ip_refresh(SCR *sp, int repaint)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp;
	db_recno_t total;

	ipp = IPP(sp);

	/*
	 * If the scroll bar information has changed since we last sent
	 * it, resend it.  Currently, we send three values:
	 *
	 * top		The line number of the first line in the screen.
	 * num		The number of lines visible on the screen.
	 * total	The number of lines in the file.
	 *
	 * XXX
	 * This is a gross violation of layering... we're looking at data
	 * structures at which we have absolutely no business whatsoever
	 * looking...
	 */
	ipb.val1 = HMAP->lno;
	ipb.val2 = TMAP->lno - HMAP->lno;
	if (sp->ep != NULL && sp->ep->db != NULL)
		(void)db_last(sp, &total);
	ipb.val3 = total == 0 ? 1 : total;
	if (ipb.val1 != ipp->sb_top ||
	    ipb.val2 != ipp->sb_num || ipb.val3 != ipp->sb_total) {
		ipb.code = SI_SCROLLBAR;
		(void)vi_send(ipp->o_fd, "123", &ipb);
		ipp->sb_top = ipb.val1;
		ipp->sb_num = ipb.val2;
		ipp->sb_total = ipb.val3;
	}

	/* Refresh/repaint the screen. */
	ipb.code = repaint ? SI_REDRAW : SI_REFRESH;
	return (vi_send(ipp->o_fd, NULL, &ipb));
}

/*
 * ip_rename --
 *	Rename the file.
 *
 * PUBLIC: int ip_rename __P((SCR *, char *, int));
 */
int
ip_rename(SCR *sp, char *name, int on)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp = IPP(sp);

	ipb.code = SI_RENAME;
	ipb.str1 = name;
	ipb.len1 = name ? strlen(name) : 0;
	return (vi_send(ipp->o_fd, "a", &ipb));
}

/*
 * ip_reply --
 *	Reply to a message.
 *
 * PUBLIC: int ip_reply __P((SCR *, int, char *));
 */
int
ip_reply(SCR *sp, int status, char *msg)
{
	IP_BUF ipb;
	IP_PRIVATE *ipp = IPP(sp);

	ipb.code = SI_REPLY;
	ipb.val1 = status;
	ipb.str1 = msg == NULL ? "" : msg;
	ipb.len1 = strlen(ipb.str1);
	return (vi_send(ipp->o_fd, "1a", &ipb));
}

/*
 * ip_split --
 *	Split a screen.
 *
 * PUBLIC: int ip_split __P((SCR *, SCR *));
 */
int
ip_split(SCR *origp, SCR *newp)
{
	return (0);
}

/*
 * ip_suspend --
 *	Suspend a screen.
 *
 * PUBLIC: int ip_suspend __P((SCR *, int *));
 */
int
ip_suspend(SCR *sp, int *allowedp)
{
	*allowedp = 0;
	return (0);
}

/*      
 * ip_usage --
 *      Print out the ip usage messages.
 *
 * PUBLIC: void ip_usage __P((void));
 */
void    
ip_usage(void)
{       
#define USAGE "\
usage: vi [-eFlRrSv] [-c command] [-I ifd.ofd] [-t tag] [-w size] [file ...]\n"
        (void)fprintf(stderr, "%s", USAGE);
#undef  USAGE
}
