/* ELLE - Copyright 1982, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*	EEQUES - Handle queries and status displays
 */
#include "elle.h"

/*
 * Ask -- ask the user for some input on the lowest line of the screen
 * 
 * The arg is a string in printf form, followed by up to three args
 * for the printf string
 *
 *        The string is read into a sort of mini buffer, only the
 *        last line of which is visible on the screen. All editing
 *        features are available to the user to edit the input string.
 *        When the delim character is typed, input is terminated and
 *        The input string is passed back to the caller.
 *	  The delim is either an escape or a cr.
 *        IT IS UP TO THE CALLER TO FREE THIS MEMORY.
 *
 * Note that the actual length of the returned string can be found
 * in the global variable ask_len.  This is a crock but allows
 * callers to hack nulls in arg strings if they want to.
 */

int chg_win();
struct window *make_mode();

static int ask_lin;	/* Saved cursor location when ask is done */
static int ask_blen;	/* Non-zero if buffer contains something */
static int ask_cnt;	/* Incremented by ask(), cleared by askclr() */

/* Table of allowed functions during ASK */
static char askftab[] = {
	FN_PFXMETA, FN_INSSELF, FN_BEGLINE, FN_ENDLINE, FN_BCHAR, FN_FCHAR,
	FN_DCHAR, FN_BDCHAR, FN_TCHARS, FN_QUOTINS, FN_UARG, FN_BKPT,
	FN_DEBUG,
	FN_GOBEG, FN_GOEND, FN_FWORD, FN_BWORD, FN_KWORD, FN_BKWORD,
	FN_UCWORD, FN_LCWORD, FN_UCIWORD, FN_ARGDIG, FN_NEWWIN, FN_KLINE,
	FN_UNKILL, FN_BKLINE,
	0
};

char *
ask (string, arg1, arg2, arg3)
char *string, *arg1, *arg2, *arg3;

{	register int i, c;
	register char *s;
	struct window *oldwin;
	char *newline;		/* where output line goes */
	char cbuf[200];			/* For prompt string creation */
	int p_length;			/* length of prompt */
	chroff anslen;			/* Length of answer */
	int funnum;			/* Crock stuff */
#if FX_FILLMODE
	extern int fill_mode;
	int ofillmode = fill_mode;	/* Gotta turn this one off */
	fill_mode = 0;
#endif /*FX_FILLMODE*/

	oldwin = cur_win;
	chg_win (ask_win);
	ed_reset();			/* Flush contents & request redisp */
	ask_lin = cur_win->w_pos;	/* Set here in case never redisp */
	ask_cnt++;			/* Bump # of times called */

  	/* copy 'string' into line */
	cbuf[0] = 0;
asklp:	sprintf (&cbuf[strlen(cbuf)], string, arg1, arg2, arg3);
	p_length = strlen(cbuf);	/* Find how long it is */

  	/* now let the user type in */
	for(;;)
	  {
		if ((rd_type & (RDS_WINFLGS|RD_MODE)) && tinwait () == 0)
		  {
			e_gobob();	/* Gross crock: insert prompt */
			e_sputz(cbuf);		/* Ugh, bletch */
			cur_dot += p_length;	/* Temporarily update loc */
			redp(RD_WINRES);	/* Do complete re-crunch */
			upd_wind((struct window *)0);		/* Don't interrupt */
			/* Ensure mode line is spiffy too.  This should
			** only have to be invoked the first time ask_win
			** redisplay is done, and never thereafter.
			*/
			if(rd_type&RD_MODE)	/* If mode also needs it, */
				fupd_wind(make_mode(user_win));	/* do it */

			upd_curs(cur_dot);
			rd_type &= ~(RDS_WINFLGS|RD_MODE);
			ask_lin = curs_lin;	/* Remember line cursor on */
			tbufls();

			e_gobob();	/* More crock: Remove prompt */
			sb_deln((SBBUF *)cur_buf,(chroff)p_length);	/* Ugh etc. */
			cur_dot -= p_length;		/* Restore loc */
			e_gocur();
		  }
		exp = 1;
		exp_p = 0;
	cont:	this_cmd = 0;
		c = cmd_read();

		if (
#if !(ICONOGRAPHICS)
			c == ESC ||
#endif /*-ICONOGRAPHICS*/
				 c == LF || c == CR)
			break;
		if (c == BELL)       /* ^G means punt.. */
		  {	chg_win(oldwin);
			ask_blen = 1;	/* Assume buffer has something */
			ding((char *)0);	/* Clear echo window */
			ask_cnt = 0;	/* Nothing for askclr to do */
#if FX_SKMAC
			km_abort();
#endif /*FX_SKMAC*/
#if FX_FILLMODE
			fill_mode = ofillmode;
#endif /*FX_FILLMODE*/
			return(0);	/* Return 0 to indicate we quit */
		  }
		/* This censoring section is a real crock! */
		funnum = cmd_idx(c);		/* Map key to command */
		while(funnum == FN_PFXMETA)	/* Allow META prefix */
			funnum = cmd_idx(c = CB_META|cmd_read());
		for(s = askftab; (i = *s&0377); ++s)
			if(funnum == i) break;
		switch(i)
		  {	default:	/* Permissible function */
				cmd_xct(c);
				break;
			case FN_NEWWIN:	/* Wants redisplay, do specially */
				clear_wind(ask_win);
				break;
			case 0:		/* Illegal function */
				ring_bell();
#if FX_SKMAC
				km_abort();
#endif /*FX_SKMAC*/
				continue;
		  }
		if(this_cmd == ARGCMD)
			goto cont;
	  }

	if((anslen = e_blen()) > 255)		/* Ridiculously long? */
	  {	strcpy(cbuf,"Huh? Try again - ");
#if FX_SKMAC
			km_abort();
#endif /*FX_SKMAC*/
		goto asklp;
	  }
	i = anslen;
	e_gobob();		/* Go to start of buffer */
	e_sputz(cbuf);		/* Re-insert prompt so buffer == screen */
	ask_blen = i + 1;	/* Say buffer has something in it */

	s = memalloc((SBMO)(i + 1));	/* Allocate fixed loc for answer */
	newline = s;		/* Return ptr to allocated string */
	ask_len = i;		/* And return (via global) length of string */
	if(i) do { *s++ = e_getc(); }
		while(--i);
	*s = '\0';		       /* make sure string terminated */
	chg_win(oldwin);
#if FX_FILLMODE
	fill_mode = ofillmode;
#endif /*FX_FILLMODE*/
	return (newline);	       /* return pointer to data */
}

/* ASKCLR - Clears the echo area (but not immediately) if the last thing
**	done to it was an ask() call.  Note that invoking a SAY routine
**	specifically causes this to be a no-op; SAYCLR must be done instead.
*/
askclr()
{
	if(ask_cnt) sayclr();	/* Zap if need be */
}

/* SAY - put up some text on bottom line.
 *	Does this intelligently; text stays up until next SAY or
 *	screen refresh.
 * SAYNOW - like SAY but forces display right away
 * SAYTOO - adds to existing stuff
 * SAYNTOO - ditto but forces output right away.
 * DING - Ring_bell then SAYNOW 
 * DINGTOO - is to DING as SAYNTOO is to SAYNOW.
 * SAYCLR - Clears echo area (but not immediately)
 */
#define SAY_NOW 01	/* Force display immediately */
#define SAY_TOO 02	/* Add to existing stuff */
#define SAY_BEL 04	/* Ding bell prior to text */
#define SAY_LEN 010	/* String length specified by 3rd arg */

say(str)	char *str; {	sayall(str, 0); }
saynow(str)	char *str; {	sayall(str, SAY_NOW); }
saytoo(str)	char *str; {	sayall(str, SAY_TOO); }
sayntoo(str)	char *str; {	sayall(str, SAY_NOW|SAY_TOO); }
ding(str)	char *str; {	sayall(str, SAY_NOW|SAY_BEL); }
dingtoo(str)	char *str; {	sayall(str, SAY_NOW|SAY_TOO|SAY_BEL); }
saylntoo(str,n)	char *str; {	sayall(str, SAY_NOW|SAY_TOO|SAY_LEN, n); }
sayclr()		   {	sayall((char *)0, 0); }

sayall(str,flags,len)
char *str;
int flags, len;
{	register struct window *w;
	register f;

	f = flags;
	w = cur_win;

	ask_cnt = 0;			/* Always reset this */
	if(str == 0 && ask_blen == 0)	/* If clearing, and buff empty */
		return;			/* nothing to do. */
	chg_win(ask_win);
	if(f&SAY_TOO)
		e_goeob();	/* Add to end of existing stuff */
	else e_reset();		/* Flush previous stuff if any */
	if(str)
	  {	if(f&SAY_LEN)		/* Insert string to post up */
			ed_nsins(str,len);
		else e_sputz(str);
	  }
	ask_blen = e_dot();	/* Remember whether buffer has something */

	e_setcur();		/* and remember to set dot */
	if(f&SAY_NOW)
	  {	if(f&SAY_BEL)
			ring_bell();
		redp(RD_WINRES);
		upd_wind((struct window *)0);
		tbufls();
	  }
	else redp(RD_WINRES);	/* Set for this window */
	chg_win(w);		/* Back to previous window */

	/* redisplay() does a special check for ask_win->w_redp, so we
	** don't need to set a global flag like RD_CHKALL.
	*/
}

/* YELLAT -- post string on specified line of screen, immediately.
 *	Differs from SAYNOW and SAYNTOO in that NO buffer
 *	manipulation is done; screen image is hacked directly.
 */

yellat(str, line)
char *str;
register int line;
{	register struct scr_line *s;

	s = scr[line];
	strncpy(s->sl_nlin, str, scr_wd0);
	s->sl_ncol = strlen(str);
#if IMAGEN
	s->sl_flg |= SL_REDO;
#endif
	upd_line(line);
	tbufls();
}

/* YELLTOO -- Append string to previous echo line of screen, immediately.
**	Uses the ask_lin variable which is set by ask().
**	Currently this function is only needed for srchint() in EESRCH.
*/
yelltoo(str)
char *str;
{	register int i;
	register struct scr_line *s;
	char nstr[MAXLINE];

	s = scr[ask_lin];
	i = s->sl_col;
	nstr[0] = 0;
	strncat(strncat(nstr, s->sl_line, i),	/* Make new string */
			str, MAXLINE - i);
	yellat(nstr, ask_lin);			/* Post it */
}

/* BARF - output a message on the bottom line of the screen,
**	bypassing everything (window, buffer, screen image).
**	Does NOT know about SAY's stuff and does not update it!
**	Use only in dire straits...
** ERRBARF - same but uses a standard error-message prefix.
*/

errbarf(str)
char *str;
{
	barf("\007ELLE Internal Error: ");
	tputz(str);
	tbufls();
}

barf(str)
char *str;
{
	ask_cnt = 0;			/* Ensure askclr() disabled */
	t_curpos(scr_ht - ECHOLINES, 0);       /* goto echo area */
	t_cleol();
	tputz(str);
	tbufls();
	curs_col = -1000;	/* Say we dunno where cursor is now */
}

#if IMAGEN
/* Same, but do it far from harm's way */
barf2(str)
char *str;
{
	t_curpos (scr_ht - 1, scr_wid - strlen(str) - 8);
	t_cleol ();
	tputz(str);
	tputz(" --M--");
	tbufls();
	tgetc();			/* Read any char & discard */
	curs_col = -1000;	/* Say we dunno where cursor is now */
}
#endif /*IMAGEN*/
