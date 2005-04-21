/* ELLE - Copyright 1982, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */

/* EEFD		Display control functions
 */

#include "elle.h"


#if FX_NEWWIN
/* EFUN: "New Window" */
/* 	Clear current window and set as requested.
 *		^L - clear current window and redisplay it (default top)
 *		<arg>^L - select new window so that current line is
 *			the <arg>'th from top of window (0 = top line)
 *		^U^L - clear current line and redisplay.
 */
f_newwin()
{	register int i, n;
	register struct window *w;

	d_fixcur();		/* Ensure screen vars correct */
	w = cur_win;
	if (exp_p)
	  {	if((n = exp) == 4 && exp_p == 4		/* CTRL-U? */
		  && (i = d_line(cur_dot)) >= 0)	/* On valid line? */
		  {	d_lupd(w, i);			/* Update it */
			return;
		  }
	  }
	else			/* No argument given */
	  {	redp(RD_SCREEN);	/* Clear whole screen (later just window? */
#if IMAGEN
		return;
#else
		n = (ev_nwpct*w->w_ht)/100;	/* Set new window using % */
#endif /*-IMAGEN*/
	  }
	if (n < 0) n = 0;		/* Ensure # is reasonable */
	else if (n >= w->w_ht)
		n = w->w_ht - 1;
	d_fgoloff(-n);			/* Go up given # of lines */
	w->w_topldot = e_dot();		/* Set new top-line dot */
	e_gocur();			/* Move back to cur_dot */
	redp(RD_FIXWIN);		/* Say to re-hack window */
}
#endif /*FX_NEWWIN*/

#if FX_NSCREEN
/* EFUN: "Next Screen" */
f_nscreen()
{	d_screen( exp);
}
#endif /*FX_NSCREEN*/

#if FX_PSCREEN
/* EFUN: "Previous Screen" */
f_pscreen()
{	d_screen(-exp);
}
#endif /*FX_PSCREEN*/

#if FX_OTHNSCREEN
/* EFUN: "Other New Screen" (not EMACS) - from IMAGEN config */
f_othnscreen()
{
	if (! oth_win)
		return;
	f_othwind();
	if (exp_p)			/* With arg, back up */
		d_screen(-1);
	else
		d_screen(1);
	f_othwind();
	redp(RD_WINDS);			/* Look at all windows */
}
#endif /*FX_OTHNSCREEN*/


#if FX_LWINDBORD
/* EFUN: "Line to Window Border" (not EMACS) - from IMAGEN config */
f_lwindbord()
{
	if (exp_p)
		/* With arg, means "to bottom" */
		exp = cur_win->w_ht - 1;
	else
		/* Else, to top */
		exp = 0;

	/* Just a "front end" for ^L */
	exp_p = 1;
	f_newwin();
}
#endif /*FX_LWINDBORD*/

#if FX_SCUPWIND
/* EFUN: "Scroll Window Up" (not EMACS) - from IMAGEN config */
f_scupwind()
{
	scroll_win(exp);
}
#endif /*FX_SCUPWIND*/

#if FX_SCDNWIND
/* EFUN: "Scroll Window Down" (not EMACS) - from IMAGEN config */
f_scdnwind()
{
	scroll_win(-exp);
}
#endif /*FX_SCDNWIND*/


#if FX_MVWTOP
/* EFUN: "Move to Window Top" (not EMACS) - from IMAGEN config */
f_mvwtop()
{
	extern moveborder();
	moveborder(1);
}
#endif /*FX_MVWTOP*/

#if FX_MVWBOT
/* EFUN: "Move to Window Bottom" (not EMACS) - from IMAGEN config */
f_mvwbot()
{
	extern moveborder();
	moveborder(0);
}
#endif /*FX_MVWBOT*/



#if FX_NSCREEN || FX_PSCREEN || FX_OTHNSCREEN
/* Move to new loc by N screenfuls.
 * If moving downward, keep bottom 2 lines of current screen on top of next.
 * If moving up, keep top 2 lines of current screen on bottom of next.
 */
d_screen(rep)
int rep;
{
	register int i;
	register struct window *w;
	chroff newdot;

	w = cur_win;
	if((i = w->w_ht - 2) <= 0)	/* Just-in-case check */
		i = 1;
	if((i *= rep) == 0)
		return;
	d_fixcur();			/* Ensure window fixed up */
	e_go(w->w_topldot);		/* Start at top of screen */
	d_fgoloff(i);

	/* Find where we are now, and make that the new top of window. */
	if((newdot = e_dot()) != e_blen())	/* If not at EOF, */
		w->w_topldot = newdot;	/* set new top of window! */
	else w->w_topldot = 0;		/* Else let fix_wind select top. */

	e_setcur();			/* Ensure cur_dot set to real loc */
#if IMAGEN
	redp(RD_WINRES|RD_REDO);	/* HINT: just repaint screen */
#else
	redp(RD_FIXWIN|RD_MOVE);
#endif /*-IMAGEN*/
}
#endif /*FX_NSCREEN || FX_PSCREEN || FX_OTHNSCREEN*/

#if FX_SCUPWIND || FX_SCDNWIND	/* If want scroll-window function */
scroll_win(n)
register int n;
{	register struct window *w = cur_win;
	chroff savdot;

	if (n == 0) return;
	d_fixcur();		/* Ensure screen vars for win all set up */
	e_go(w->w_topldot);	/* Go to top of current window */
	d_fgoloff(n);		/* Move given # of display lines */
	w->w_topldot = e_dot();	/* Set new top of window */
	redp(RD_FIXWIN);	/* Say new window needs fixing up */

	/* Now adjust position of current dot so it is still within window */
	if (n > 0)
	  {	/* Moving screen text "up" (win down) */
		if (cur_dot < w->w_topldot)	/* See if scrolled off top */
			e_setcur();		/* yes, make dot be win top */
	  }
	else {	/* Moving screen text "down" (win up) */
		savdot = cur_dot;	/* Save since must temporarily */
		e_setcur();		/* set current dot within window, */
		d_fixcur();		/* so screen can be fixed up. */
		if (inwinp(w, savdot))	/* Now see if old dot in new win */
			cur_dot = savdot;	/* Yes, just restore it! */
		else			/* No, make it beg of bottom line. */
			cur_dot = scr[w->w_pos + w->w_ht - 1]->sl_boff;
	  }
	e_gocur();		/* Make current pos be cur_dot */
}
#endif /* FX_SC%%WIND */

#if FX_MVWTOP || FX_MVWBOT	/* Guts for above two functions */
static
moveborder(top)
int top;
{
	d_fixcur();		/* Ensure current win screen image fixed up */
	e_gosetcur(top ? cur_win->w_topldot
			: scr[cur_win->w_pos + cur_win->w_ht - 1]->sl_boff);

	redp(RD_MOVE);		/* Should only be cursor adjustment */
}
#endif /*FX_MVW%%%*/

/* Given a line and a position in that line, return the xpos.
 * NOTE CAREFULLY that when line extends over several screen lines,
 * the value returned is the screen X position even though it
 * may be some lines down from the start of the logical line!
 * Also note this won't work very well if tabs exist on the extra
 * lines.  This rtn should not be used for cursor positioning.
 * Also note: d_ncols() will never return -1 meaning EOL because the
 * setup guarantees there is no EOL within the range checked.
 */
d_curind()	/* Find current indentation */
{	indtion(e_dot());
}
indtion(lin)
chroff lin;
{	register int i, col;
	chroff savdot;
	chroff nchars;

	savdot = e_dot();		/* Save current position */
	e_go(lin);			/* Go to line caller wants */
	e_gobol();			/* Go to its beginning */
	col = 0;			/* Start at left margin */
	if((nchars = lin - e_dot()) > 0)
	    do {
		if(nchars < (i = scr_wd0))
			i = nchars;
		if((col = d_ncols(i, col)) < 0)	/* Hit edge of screen? */
			col = 0;		/* Reset to left margin */
	    } while((nchars -= i) > 0);
	e_go(savdot);			/* Restore current position */
	return(col);
}

/* ININDEX - How many positions in lin must we go to get to xpos?
 * Returns -1 if can't be done.  Assumes "lin" is at beginning of a line!
 */

inindex (lin, xpos)
chroff lin;
int   xpos;
{	register int col, x;
	chroff savdot;
	char tmp[MAXLINE+MAXCHAR];
	extern int sctreol;		/* From EEDISP */

	if((x = xpos) <= 0) return(0);
	if(x >= MAXLINE) return(-1);	/* ?!? */
	col = 0;
	savdot = e_dot();
	e_go(lin);			/* Assumes this is start of line */
	col = sctrin(tmp, x, 0);	/* Translate from sb_getc input */
	if((col - x) >= 0)		/* Exact count-out or past it? */
	  {	x = e_dot() - lin;	/* Yup, win. */
		if (sctreol > 0)	/* Did we hit (and include) EOL? */
#if FX_EOLMODE			/* If so, back up over the EOL. */
			x -= eolcrlf(cur_buf) ? 2 : 1;
#else
			--x;
#endif
	  }
	else x = -1;			/* Nope, EOL or EOF hit too soon. */
	e_go(savdot);
	return(x);
}

/*
 * D_ ROUTINES - display-relative functions.  Similar to E_, but
 *	a "line" is defined as one line of the screen rather than
 *	as a logical text line.  Also, for efficiency reasons
 *	arguments are given saying how many lines to hack.
 */

d_gopl() { return(d_goloff(-1)); }
d_gonl() { return(d_goloff( 1)); }

/* D_GOLOFF(i) - Go to beginning of a display line
 * D_FGOLOFF(i) - ditto, but assumes screen image of window already fixed up.
 *	i - # of lines offset.  Negative moves up, positive down.
 *		Zero arg goes to beginning of current display line.
 *	Side effects: screen image of window is fixed up at
 *	start of routine, but is NOT updated by the move to new location.
 */
d_goloff(cnt)
int cnt;
{	d_fixcur();
	d_fgoloff(cnt);		/* Now can invoke fixed-up fast version */
}
d_fgoloff(cnt)
register int cnt;
{	register int y;
	struct scr_line l;
	char line[MAXLINE+MAXCHAR];
	int top, bot;

	/* Find current position in window, since can save time
	 * by using stuff already in fixed-up screen image.
	 */
	if((y = d_line(e_dot())) < 0)		/* Get current Y position */
	  {
		errbarf("Dot out of window");
		y = 0;
	  }
	top = cur_win->w_pos;		/* 1st line of window */
	bot = top + cur_win->w_ht;	/* 1st line not in window */
	l.sl_boff = scr[y]->sl_boff;
	l.sl_nlin = &line[0];
	l.sl_cont = 0;

	if(cnt > 0) goto down;
	
	/* Go upwards.  This is hairy because we want to be clever about
	 * huge logical lines -- avoid going all the way back to BOL.
	 */
	if((y+cnt) >= top)	/* Fits? */
		goto onscr;	/* Hurray, hack it! */
	cnt += y - top;		/* Sigh, find # lines to skip */
	y = top;
	l.sl_boff = scr[y]->sl_boff;
	e_go(l.sl_boff);

	/* Okay, here's the hairy part.  Must go backwards from top
	 * line; if no EOL within scr_wid*cnt chars, then simply assume one is
	 * seen.
	 */
	cnt = -cnt;
	d_backup(cnt);
	return;		/* Really should re-adjust stuff, but... */

	/* Go downwards.  Not too bad... */
down:
	if((y+cnt) <= bot)	/* Fits? */
		goto onscr;	/* Hurray, hack it! */
	cnt -= bot - y;		/* Sigh, find # lines can skip */
	y = bot - 1;
	l.sl_boff = scr[y]->sl_boff + scr[y]->sl_len;
	if(y > top
	  && (l.sl_cont = scr[y-1]->sl_cont))
		l.sl_line = scr[y-1]->sl_line;
	e_go(l.sl_boff);

	do {
		fix_line(&l,&l);
	  } while(--cnt > 0 && l.sl_len);
	return;

onscr:	if((y += cnt) >= bot)
	  {	--y;
		e_go(scr[y]->sl_boff + scr[y]->sl_len);
	  }
	else e_go(scr[y]->sl_boff);
}

/* D_FIXCUR() - Ensure current window is fixed up, with
 *	current location (not necessarily cur_dot)!
 * Ensure cur_dot reflects real loc so that fix_wind will work,
 * and always call fix_wind to ensure that screen image vars
 * are set properly.  Note any active redisplay flags must be carried
 * on into window redisplay flags, so fix_wind will notice them.
 */
d_fixcur()
{	register struct window *w;
	chroff savedot;

	w = cur_win;
	savedot = cur_dot;
	e_setcur();
	w->w_redp |= rd_type&RDS_WINFLGS;
	fix_wind(w);		/* Always ensure window is set up! */
	redp(w->w_redp);	/* Add back new flags */
	rd_type &= ~RDS_DOFIX;	/* and flush fix-invoking ones */
	cur_dot = savedot;	/* Restore cur_dot, no longer hacked. */
}

d_backup(nlin)		/* Try to back up by nlin screen lines */
int nlin;
{	register int cnt, n, c;
	int eolstop;

	if((cnt = nlin+1) <= 0) return;
	c = 0;
	do
	  {	n = scr_wid;
		eolstop = 0;		/* Not yet stopped at EOL */
		do {	if((c = e_rgetc()) == EOF)
				return;
			if(c == LF)
			  {
#if FX_EOLMODE
				if(eolcrlf(cur_buf))
				  {	if((c = e_rgetc()) == CR)
					  {	eolstop++;
						break;
					  }
					if(c != EOF)
						e_getc();
				  }
				else
#endif
				  {	eolstop++;
					break;
				  }
			  }
		  } while(--n);
	  } while(--cnt);
	if(eolstop)
	  {
#if FX_EOLMODE
		if(eolcrlf(cur_buf)) e_getc();	/* Skip back over CR */
#endif
		e_getc();		/* Skip back over LF */
	  }

	/* At this point, dot is guaranteed to be less than goal,
	 * which is the important thing for fix_wind, which can handle
	 * things okay if dot is off bottom of window.
	 */
	return(1);		/* Say always test result */
}
