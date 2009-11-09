/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */

/* EEBUFF	Buffer and Window functions.
 *	Each buffer is an independent SB-string described by a
 *	buffer structure.  All buffer structures are allocated dynamically
 *	and chained together starting from buf_head.
 */

#include "elle.h"

#if FX_FILLMODE
extern int fill_mode;
#endif
#if FX_SKMAC
extern int kdef_mode;
#endif

struct buffer *make_buf(), *find_buf(), *sel_mbuf(), *sel_nbuf();
struct window *make_win();

/* EFUN: "Select Buffer" */
/*	Select old buffer or create a new one.  Defaults to previously
 *	used buffer.
 */
f_selbuffer()
{	register char *ans;
	register struct buffer *b;

	if((b = last_buf) == cur_buf)	/* If default same as current, */
		if(!(b = sel_mbuf(b)))	/* try to pick a more useful one. */
			b = sel_nbuf(cur_buf);

	ans = ask("Select buffer (%s): ",b->b_name);
	if (ans == 0)		       /* he aborted */
		return;
	if (*ans != '\0')		/* Null string => use last buff */
	  {	b = find_buf (ans);	/* Else find/create one */
		if (b == 0)
			b = make_buf (ans);
	  }
	sel_buf(b);
	chkfree(ans);
}

#if FX_SELXBUFFER
/* EFUN: "Select Existing Buffer" (not EMACS) - from IMAGEN config */

static int findstr();

f_selxbuffer()
{	register char *ans;
	register struct buffer *b;

	b = last_buf;			/* This is default */
	ans = ask("Select existing buffer (%s): ", b->b_name);
	if (ans == 0)			/* Aborted */
		return;
	if (*ans != 0)
	  {	for (b = buf_head; b != 0; b = b->b_next)
			if (findstr(ans, b->b_name))
				break;
		if (b == 0)
			ding("That isn't a substring of any buffer name!");
	  }
	chkfree(ans);
	if (b != 0)
	  {	saytoo(" => ");
		sayntoo(b->b_name);
		sel_buf(b);
	  }
}

static int
findstr(str, instr)			/* Find "str" in string "instr" */
register char *str, *instr;
{	register char *sp, *isp;

	while (*instr)
	  {	sp = str;
		isp = instr;
		while (*sp)
			if (*sp++ != *isp++)
				goto next;
		return(1);
next:		++instr;
	  }
	return(0);
}
#endif /*FX_SELXBUFFER*/


/* EFUN: "Kill Buffer"	*/
/*	Kill specified buffer - defaults to current buffer.
 * This code assumes a killed buffer will never be on a window list unless it
 * is pointed to by cur_buf or oth_win->w_buf!!!!
 */
f_kbuffer()
{	register struct buffer *b, *ob;
	register char *ans;

	if((ans = ask("Kill buffer: ")) == 0)
		return;
	if(*ans == 0) b = cur_buf;
	else if(*ans == SP) b = 0;
	else b = find_buf(ans);

	chkfree(ans);
	if(!b)
	  {	ding("No such buffer");
		return;
	  }
#if IMAGEN
	if (b->b_flags & B_PERMANENT)
	  {	ding("Permanent buffer--cannot kill!");
		return;
	  }
	if (b->b_flags & B_MODIFIED)
	  {	if ((ans == ask("Buffer is modified; are you sure? ")) == 0)
			return;
		if(upcase(*ans) != 'Y')
		  {	chkfree(ans);
			return;
		  }
		chkfree(ans);
	  }
#endif /*IMAGEN*/
	if(b == cur_buf || (oth_win && (oth_win->w_buf == b)))
	  {	ob = last_buf;
		do
		  {
			/* If default same as doomed buffer, try to pick
			 * a more useful alternative. */
			if((b == ob) && !(ob = sel_mbuf(b)))
				ob = sel_nbuf(b);

			ans = ask("Killing in-use buffer; select which other buffer (%s): ",
					ob->b_name);
			if(ans == 0) return;
			if(*ans)
			  {	if(*ans == SP) ob = 0;
				else ob = find_buf(ans);
			  }
			chkfree(ans);
			if(!ob)
			  {	ding("No such buffer");
				return;
			  }
		  } while (b == ob);

		/* B is buffer to kill, OB is buffer to replace it with */
		if(oth_win && (oth_win->w_buf == b))
		  {	f_othwind();	/* Select other one */
			chg_buf(ob);	/* Change to new buffer */
			f_othwind();
		  }
		if(cur_buf == b)
			chg_buf(ob);
	  }

	kill_buf(b);			/* Die!!!! */
	if(last_buf == b)
		last_buf = cur_buf;
}

/* EFUN: "List Buffers" */
/*	Display a list of all user buffers.  Internal buffers, whose names
 *	start with a space, are not shown.
 */
f_listbufs()
{
	register struct buffer *b;
	register char *cp;
	register int i;
	struct buffer *tbuf, *savbuf;
	char temp[20];

	/* First must set up special buffer... */
	savbuf = cur_buf;
	chg_buf(tbuf = make_buf(" **SHOW**"));
	e_sputz("Buffers in this ELLE:\n\n");
	for(b = buf_head; b; b = b->b_next)
	  {	cp = b->b_name;
		if(*cp == SP) continue;	/* Ignore internal buffs */
		e_sputz((b->b_flags&B_MODIFIED) ? "* " : "  ");
		e_sputz(cp);			/* Insert buffer name */
		dottoa(temp,ex_blen(b));	/* Get buff-length string */
		if((i = ((FNAMELEN > 14) ? 30 : 20)
			 - strlen(cp) - strlen(temp)) > 0)
			e_insn(SP, i);
		e_sputz(" (");
		e_sputz(temp);
		e_sputz(") ");
		if(cp = b->b_fn)
			e_sputz(cp);
#if IMAGEN
		if (b->b_flags & B_CMODE)
			e_sputz(" (C)");
		else if (b->b_flags & B_TEXTMODE)
			e_sputz(" (Text)");
		else
			e_sputz(" (Fundamental)");
#endif /*IMAGEN*/
		e_putc(LF);
	  }
	mk_showin(tbuf);	/* Show this buffer in temp window */
	chg_buf(savbuf);	/* Return to real current buffer */
	kill_buf(tbuf);
}

/* EFUN: "Buffer Not Modified" */
/*	Mark the current buffer as not modified.
 */
f_bufnotmod()
{
	cur_buf -> b_flags &= ~B_MODIFIED;
	redp(RD_MODE);
}

#if FX_EOLMODE
/* EFUN: "EOL CRLF Mode" (not EMACS) */
/*	Toggle the EOL mode of the current buffer.
**		LF EOL Mode means LF alone is an EOL.
**		CRLF EOL Mode means CRLF together is an EOL.
*/
f_eolmode()
{
	cur_buf->b_flags ^= B_EOLCRLF;		/* Flip this bit */
	say((cur_buf->b_flags & B_EOLCRLF)
		? "EOL Mode is CRLF"		/* If now on */
		: "EOL Mode is LF");		/* If now off */

	redp(RD_WINRES);			/* Redo window for this buf */
}
#endif /*FX_EOLMODE*/

#if FX_GOBEG
/* EFUN: "Goto Beginning" */
f_gobeg()
{	e_gobob();
	ed_setcur();
}
#endif /*FX_GOBEG*/

#if FX_GOEND
/* EFUN: "Goto End" */
f_goend()
{	e_goeob();
	ed_setcur();
}
#endif /*FX_GOEND*/

#if FX_WHATPAGE
/* EFUN: "What Page" */
/*	Extra info added as per earlier ICONOGRAPHICS "What Buffer Position"
** Reports on current position as follows:
**	Dot=<n>, Page <n>  Line <n> (line <n> of <m>)
*/
f_whatpage()
{
	register chroff cnt;
	register int c;
	register int page, line;
	int lineatp;
	char tempstr[12], *dottoa ();

        saynow("Dot=");
        dottoa(tempstr, cur_dot);
        sayntoo(tempstr);

	e_gobob();
	page = line = lineatp = 1;
	for (cnt = cur_dot; --cnt >= 0;)
		if ((c = e_getc()) == LF)
			++line;
		else if (c == FF)
		  {	++page;
			lineatp = line;
		  }

        saytoo(", Page ");
        dottoa(tempstr, (chroff)page);
        saytoo(tempstr);
	saytoo("  Line ");
        dottoa(tempstr, (chroff)(1 + line - lineatp));
        saytoo(tempstr);
	saytoo("  Col ");
        dottoa(tempstr, (chroff)indtion(cur_dot));
        saytoo(tempstr);
	saytoo("  [line ");
	dottoa(tempstr, (chroff)line);
	saytoo(tempstr);
	sayntoo(" of ");		/* Force out while scan rest */

        for(e_gocur(); e_gonl() ; ++line) ;	/* Count lines until EOF */
	c = e_rgetc();			/* Remember what last char is */
        dottoa(tempstr, (chroff)line);
        saytoo(tempstr);
        if (c != LF)		/* Check last char */
            saytoo(" (no EOL at EOF!)");
        sayntoo("]");
	e_gocur();			/* Back to original position */
}
#endif /*FX_WHATPAGE*/

init_buf ()			       /* init buffer stuff */
{
	buf_head = 0;
	lines_buf = cur_buf = make_buf(" **LINES**");	/* For sep_win */
	e_insn('-',scr_wid-2);			/* Fill with dashes */
	last_buf = cur_buf = make_buf ("Main");	/* Make Main buffer */
	init_win();				/* Now can init windows */
}

struct buffer *
make_buf(bname)	       /* create buffer "bname" if it doesn't exist */
char *bname;
{	register struct buffer *b;
	register char *name;

	b = find_buf(name = bname);
	if (b)				/* if it exists already */
		return(b);
	b = (struct buffer *) memalloc(sizeof (struct buffer));
	b -> b_next = buf_head;	       /* link it in */
	buf_head = b;
	b->b_name = strdup(name);	/* Allocate copy of name string */
	b->b_dot = 0;		/* Set dot to beg */
	sb_open(b,(SBSTR *)0);		/* Open buffer with null initial sbstring */
	b->b_fn = 0;
	b->b_flags = 0;
	b->b_mode = cur_mode;	/* Inherit current mode */
	return(b);
}


struct buffer *
find_buf(name)	       /* returns pointer to buffer of that name or 0 */
char *name;
{	register struct buffer *b = buf_head;

	while (b && strcmp(b->b_name, name))
		b = b -> b_next;
	return(b);
}

sel_buf(b)				/* select buffer, saving last */
struct buffer *b;
{
	if(b != cur_buf) last_buf = cur_buf;
	chg_buf(b);
}

chg_buf (newbuf)		       /* change current buffer to newbuf */
struct buffer *newbuf;
{	register struct buffer *obuf, *nbuf;

	if ((nbuf = newbuf) == (obuf = cur_buf))
		return;			/* Do nothing if same buffers */
	obuf->b_dot = cur_dot;
	cur_buf = nbuf;
	cur_mode = nbuf->b_mode;
	e_gosetcur(nbuf->b_dot);	/* Set cur_dot and go there */
	cur_win->w_buf = nbuf;
	cur_win->w_dot = cur_dot;
#if IMAGEN
	cur_win->w_redp = RD_WINRES|RD_REDO;
#else
	cur_win->w_redp = RD_WINRES;	/* Reset flags - do complete update */
#endif /*-IMAGEN*/
	unlk_buf(obuf);			/* Unlock previous buffer if can */
	mark_p = 0;			/* this is lazy */
	redp(RD_MODE|RD_WINRES);
}

/* See if specified buffer belongs to any active window, and
 * if not then get it into an idle, unlocked state; this helps the
 * edit package compact and swap stuff out while it's not being used.
 * Assumes proper state of dot has been stored into b_dot.
 */
unlk_buf(bufp)
struct buffer *bufp;
{	register struct buffer *b;
	register struct window *w;

	b = bufp;
	for(w = win_head; w; w = w->w_next)
		if(b == w->w_buf)
			return;		/* Buffer is actively being shown */
	sb_rewind((SBBUF *)b);		/* Return to idle state */
}

/* SEL_NBUF(buf) - Select next user buffer.  Ignores internal buffers.
 *	Arg of 0 starts at beg of buffer list.  Always returns
 *	a buffer pointer - returns argument (which may be 0)
 *	if found no other user buffers.
 *
 * SEL_MBUF(buf) - Select next modified buffer.
 *	Returns buffer ptr to "next" modified buffer, if any.
 *	Arg of 0 starts at beg of buffer list and scans all of them.
 *	Returns 0 if no other modified buffers exist (unlike SEL_NBUF!)
 *	Ignores internal buffers, whose names start with a space.
 */
/* struct buffer *buf_mptr; */
#if 0
struct buffer *
sel_mbuf(buf)
struct buffer *buf;
{	register struct buffer *b;
	register int sweep;

	sweep = 0;			/* Make 2 sweeps only */
	if(b = buf) b = b->b_next;
	do {
		if(b == 0)		/* Initialize if needed */
			b = buf_head;
		for(; b; b = b->b_next)
			if((b->b_flags & B_MODIFIED) && (*b->b_name != SP))
				return((b == buf) ? 0 : b);
	  } while(sweep++ != 0);
	return(0);
}
#endif /*COMMENT*/

struct buffer *
sel_mbuf(buf)
register struct buffer *buf;
{	register struct buffer *b, *b2;
	b = b2 = sel_nbuf(buf);
	do {	if(b == buf) break;
		if(b->b_flags & B_MODIFIED)
			return(b);
	  } while((b = sel_nbuf(b)) != b2);

	return(0);
}

struct buffer *
sel_nbuf(buf)
register struct buffer *buf;
{	register struct buffer *b;

	b = buf;
	do {
		if(!b || !(b = b->b_next))
			b = buf_head;
		if(*b->b_name != SP)
			break;
	  } while (b != buf);
	return(b);
}


kill_buf(buf)
struct buffer *buf;
{	register struct buffer *b, *b1, *bt;

	b = buf;
	b1 = 0;
	for(bt = buf_head; bt && bt != b; bt = bt -> b_next)
		b1 = bt;
	if(bt == 0)
	  {	ring_bell();
		errbarf("No such buffer");	/* Internal error */
		return;
	  }
	if (b1 == 0)
		buf_head = b->b_next;
	else
		b1->b_next = b->b_next;
	sbs_del(sb_close((SBBUF *)b));	/* Close buffer & delete sbstring */
	sb_fdcls(-1);			/* Make sweep for unused FD's */
	if(b->b_fn)
		chkfree(b->b_fn);	/* Flush filename if one */
	chkfree(b->b_name);		/* Flush name */
	chkfree((char *)b);		/* Flush buffer */
}

/* ZAP_BUFFER - Delete all of the buffer, but if it's been modified,
 *	ask first.  Returns 0 if user aborts.
 */
zap_buffer()
{
#if IMAGEN
	extern struct buffer *exec_buf;	/* in e_make.c */

	if(cur_buf != exec_buf && cur_buf -> b_flags & B_MODIFIED)
#else
	if(cur_buf -> b_flags & B_MODIFIED)
#endif /*-IMAGEN*/
		if(ask_kbuf(cur_buf) <= 0)
			return(0);		/* Aborted */
	ed_reset();		/* This takes care of redisplay too */
	mark_p = 0;
#if IMAGEN
	cur_buf->b_flags &= ~B_BACKEDUP; /* Clear backed-up flag */
#endif
	return(1);
}



/* ASK_KBUF - Ask user "are you sure?" before killing a buffer.
 *	Returns +1 if user says "yes" - OK to kill.
 *		 0 if user aborts (^G)
 *		-1 if user says "no".
 */
ask_kbuf(buf)
struct buffer *buf;
{	register struct buffer *b;
	register char *s;
	register int ans;

	b = buf;
	s = ask("Buffer %s contains changes - forget them? ", b->b_name);
	if(s == 0) return(0);
	ans = (upcase(*s) == 'Y') ? 1 : -1;
	chkfree(s);
	return(ans);
}

/* Window stuff */

/* Like EMACS, ELLE only provides at most two user windows.
 * The current user window is pointed to by user_win;
 * the "other" one is oth_win.  If oth_win == 0, there is only one user
 * window.
 */

#if FX_2MODEWINDS
int sepmode_p = 0;	/* Set true if separator window is a 2nd mode win */
#endif

/* EFUN: "Two Windows" */
/*	Divide the current window in half, put the current buffer in the
 *	other window, and go to the new window.
 */
f_2winds()
{	register int h, t;
	register struct window *w;

	if (oth_win)
	  {
#if !(IMAGEN)
		ding("Already 2 windows");
#endif /*-IMAGEN*/
		return;
	  }
	w = cur_win;
	d_fixcur();			/* Stabilize current window */
	h = (w->w_ht) / 2;
	t = w->w_pos + h;		/* Pos of dividing window */
	sep_win = make_win(t, 1, lines_buf);
					/* assume using dashes to separate */
	oth_win = make_win(t + 1, w->w_ht - (h + 1), cur_buf);
					/* Other window has balance */
#if FX_SOWIND
	oth_win->w_flags |= cur_win->w_flags&W_STANDOUT;
	sep_win->w_flags |= mode_win->w_flags&W_STANDOUT;
#endif
#if FX_2MODEWINDS
	chk2modws();			/* Update 2-mode-wind status */
#endif
	w->w_ht = h;			/* Decrease current window to half */

	/* Minimize redisplay by moving each window's dot into
	 * a currently displayed area */
	if(cur_dot < (oth_win->w_topldot = scr[t+1]->sl_boff))
		oth_win->w_dot = oth_win->w_topldot;	/* Adj bottom win */
	else					/* Adjust top window */
	  {	oth_win->w_dot = cur_dot;	/* Bottom keeps dot */
		cur_dot = scr[t-1]->sl_boff;	/* but top needs new one. */
	  }
	f_othwind();			/* switch to other window */
	redp(RD_WINDS);			/* Update all windows */
}


/* EFUN: "One Window" */
/*	Revert to using only one window; use the current buffer (unlike
 *	EMACS which always selects the top window's buffer)
 *	Ensures that current window's vars are correctly set for
 *	new dimensions (w_pos, w_ht, plus w_topldot to minimize redisplay),
 *	then kills unneeded windows.
 */
f_1wind()
{	register struct window *w;

	if (oth_win == 0)
	  {
#if (!IMAGEN)
		ding("Only 1 window");
#endif /*-IMAGEN*/
		return;
	  }
	w = cur_win;
	if(w->w_pos)		/* If not top window */
	  {	d_fixcur();		/* Ensure screen-line data correct */
		e_go(w->w_topldot);	/* Beginning from top of window, */
		d_fgoloff(-w->w_pos);	/* Move back enough lines */
		w->w_topldot = e_dot();	/* To set new start of window */
		e_gocur();		/* Then move back to orig place */
		w->w_pos = 0;
	  }
	w->w_ht += oth_win -> w_ht + 1;
	kill_win (oth_win);
	kill_win (sep_win);
	oth_win = sep_win = 0;
#if FX_2MODEWINDS
	chk2modws();	/* Update notion of whether have 2 mode winds */
#endif
	redp(RD_FIXWIN|RD_WINDS|RD_MODE); /* New topldot for this window,
					 * and check all remaining windows */
}

/* EFUN: "Other Window" */
/*	Move to the "other" user window.
 */
f_othwind ()
{	if (oth_win == 0)
	  {
#if !(IMAGEN)
		ding("Only 1 window");
#endif /*-IMAGEN*/
		return;
	  }
	chg_win(oth_win);
	oth_win = user_win;
	user_win = cur_win;
	redp(RD_MODE);
}

/* EFUN: "Grow Window" */
/*	Grow the current window - while in two window mode,
 *	increase the size of the current window by the arg
 *	and decrease the other accordingly
 */
f_growind()
{	register struct window *cw, *ow;
	register int e;

	if ((ow = oth_win) == 0)
	  {
#if !(IMAGEN)
		ding("Only 1 window");
#endif /*-IMAGEN*/
		return;
	  }
	e = exp;
	if((cw = cur_win)->w_pos != 0)	/* If current window is on bottom */
	  {	cw = ow;		/* Then fake code to think it's top */
		ow = cur_win;
		e = -e;
	  }
	if(  cw->w_ht + e < 1
	  || ow->w_ht + e < 1)
	  {	ding("Too much");
		return;
	  }
	cw -> w_ht += e;
	ow -> w_pos += e;
	ow -> w_ht -= e;
	sep_win -> w_pos += e;
	redp(RD_WINDS | RD_MODE);		/* Update all windows */
}

#if FX_SHRINKWIND
/* EFUN: "Shrink Window" (not EMACS) - from IMAGEN config */
f_shrinkwind()
{
	if (! oth_win)
		return;
	f_othwind();
	f_growind();
	f_othwind();
}
#endif /*FX_SHRINKWIND*/

#if FX_DELWIND
/* EFUN: "Delete Window" (not EMACS) - from IMAGEN config */
f_delwind()
{
	if(!oth_win)
		return;
	f_othwind();
	f_1wind();
}
#endif /*FX_DELWIND*/

#if FX_SOWIND
/* EFUN: "Standout Window" (not EMACS) */
/*	Toggles the display standout mode for the current window.
**	With argument of 4, toggles the standout mode for the non-buffer
**	parts of the screen, such as the ELLE mode line.
** (This corresponds to FS INVMOD$ in EMACS)
**	With argument of 0, turns standout mode off for all windows.
*/
/* It suffices to set the window flag bit and force a RD_WINRES for that
 * window; the redisplay code will do the rest.
*/
static void tgso_wind();

f_sowind()
{
	register struct window *w;
	switch(exp)
	  {	default:		/* Toggle current window */
			tgso_wind(cur_win);
			break;
		case 4:			/* Toggle mode & separator windows */
			tgso_wind(mode_win);
			tgso_wind(sep_win);	/* This may not exist */
			break;
		case 0:			/* Turn off standout for all winds */
			for(w = win_head; w; w = w->w_next)
				if(w->w_flags&W_STANDOUT)
					tgso_wind(w);
	  }
#if FX_2MODEWINDS
	chk2modws();	/* Update notion of whether have 2 mode winds */
#endif
}

static void
tgso_wind(w)		/* Toggle standout mode for given window */
register struct window *w;
{
	if (w == 0) return;		/* For case of no sep_win */
	if (w->w_flags & W_STANDOUT)
		w->w_flags &= ~W_STANDOUT;
	else w->w_flags |= W_STANDOUT;
	w->w_redp |= RD_WINRES;		/* Re-do this particular window */
	redp(RD_CHKALL);		/* Check all windows for changes */
}
#endif /*FX_SOWIND*/


#if FX_2MODEWINDS
/* EFUN: "Two Mode Windows" (not EMACS) */
/*	With arg, sets ev_2modws to that value (0, 1, or 2).
**	No arg, toggles current setting between 0 and 2.
*/

f_2modewinds()
{
	ev_2modws = exp_p ? exp : (ev_2modws ? 0 : 2);
	chk2modws();
}

/* CHK2MODWS - Called after anything changes which might affect
**	whether 2 mode windows are in effect or not.  Fixes up
**	sep_win to either be or not be a mode window.
*/
chk2modws()
{	register struct window *w;
	static struct buffer *sep_buf = 0;

	if(!(w = sep_win))
	  {	sepmode_p = 0;		/* Don't have 2 windows at all */
		return;
	  }
	sepmode_p = (ev_2modws == 1)
			? (mode_win->w_flags&W_STANDOUT)
			: ev_2modws;

	if(sepmode_p)		/* Turn 2-mode-winds on? */
	  {
		if(!sep_buf)
			sep_buf = make_buf(" **SEPMODE**");
		w->w_buf = sep_buf;
		w->w_flags |= W_MODE;
	  }
	else			/* Turn 2-mode-winds off */
	  {	w->w_buf = lines_buf;
		w->w_flags &= ~W_MODE;
		redp(RD_CHKALL);	/* No longer a mode win, so must */
					/* check all to ensure it's updated */
	  }
	w->w_redp |= RD_WINRES;
	redp(RD_MODE);
}
#endif /*FX_2MODEWINDS*/

init_win ()
{
	win_head = 0;
	oth_win = 0;
	user_win = make_win(0, scr_ht - (ECHOLINES+1), cur_buf); /* Main */
	mode_win = make_win(scr_ht - (ECHOLINES+1), 1, make_buf(" **MODE**"));
	ask_win  = make_win(scr_ht - ECHOLINES,     1, make_buf(" **ASK**"));
#if FX_SOWIND
	if(ev_modwso)
		mode_win->w_flags |= W_STANDOUT;
#endif

	cur_win = user_win;
}

chg_win(newwin)		       /* change current window to newwin */
struct window *newwin;
{
	cur_win->w_dot = cur_dot;	/* Save window's current dot */
	cur_win->w_redp |= rd_type&RDS_WINFLGS;	/* and its redisplay flags */
	cur_win = newwin;		/* OK, switch to new current window */
	cur_buf = newwin->w_buf;	/* Set new buffer from win */
	e_gosetcur(newwin->w_dot);	/* Set new cur_dot from win too */
			/* Note done this way to canonicalize the location
			** (may be past new EOB) and ensure SB buffer
			** internals agree with cur_dot.
			*/
	rd_type &= ~RDS_WINFLGS;	/* Remove old per-window flags */
	redp(RD_WINRES|RD_MODE);	/* Maybe caller shd handle? */
			/* Note WINRES must be set in case we are pointing
			 * to a buffer that was modified while we were in
			 * the other window!
			 */
}


struct window *
make_win (pos, ht, buf)
int pos, ht;
struct buffer *buf;
{	register struct window *w;
	register struct buffer *b;

	b = buf;
	w = (struct window *) memalloc(sizeof (struct window));
	w->w_flags = 0;
	w->w_pos = pos;
	w->w_ht = ht;
	w->w_buf = b;
	w->w_dot = b->b_dot;	/* Set dot from buffer value */
	w->w_topldot = 0;	/* Set top of window to beg of buffer */
	w->w_pct = 200;		/* Assume "ALL" */
	w->w_bmod = 0;
	w->w_emod = 0;
	w->w_oldz = 0;
	w->w_redp = RD_WINRES;	/* Window will need complete update */
	w->w_next = win_head;	/* Done, now link it in */
	win_head = w;
	return (w);
}

kill_win (win)
struct window *win;
{	register struct window *w, *w1, *kw;

	kw = win;
	w1 = 0;
	for (w = win_head; w && w != kw; w = w -> w_next)
		w1 = w;
	if (w == 0)
	  {	ring_bell();
		errbarf("No such window");	/* Internal error */
		return;
	  }
	if (w1 == 0)
		win_head = w -> w_next;
	else
		w1 -> w_next = w -> w_next;
	kw->w_buf->b_dot = (kw == cur_win) ? cur_dot : kw->w_dot;
	chkfree (kw);
#if IMAGEN		/* Not needed? */
	redp (RD_WINRES|RD_WINDS|RD_REDO);
#endif /*IMAGEN*/
}

/*
 * "Show-window" routines, used to set up, step through, and close a
 * temporary "show" window.
 * MK_SHOWIN(bufp)
 * UP_SHOWIN()
 * KL_SHOWIN()
 */

/* MK_SHOWIN(bufp) - Temporarily display a buffer
 */
mk_showin(b)
struct buffer *b;
{	register struct window *w;
	register int i;
	int moreflg, intflg;		/* Interrupt flag */
	struct window *savwin;

	/* First must set up special window... */
	savwin = cur_win;
	chg_win(w = make_win(0, scr_ht-(ECHOLINES+3), b));
 redo:
	d_fixcur();		/* Fix up screen image of current window */

	/* Find how many lines actually used, and reduce size to that */
	i = w->w_ht;
	while(--i >= 0)
	  {
		if(scr[i]->sl_boff != w->w_oldz) break;
	  }
	if(++i <= 0)
		goto skipit;	/* Punt the whole thing */
	if(!(moreflg = (i >= w->w_ht)))
		w->w_ht = i;	/* Reduce size of window */

	intflg = upd_wind(w);	/* Update the window! */
	if(!intflg)		/* Unless input waiting, add prompt. */
	  {
		yellat( moreflg ?
	"--MORE-- (type Space for more, or type any command to flush)" :
	"------------------------------------------------ (Hit space to continue)--",
			w->w_ht);
		
	  }
	tbufls();		/* Ensure all output forced out */
	i = cmd_read();		/* then wait for user to input a char */
	if(i == SP)
	  {	if(moreflg)
		  {	yellat("", w->w_ht);
			d_screen(1);
			w->w_redp |= RD_WINRES;
			goto redo;
		  }
	  }
#if !(IMAGEN)		/* IMAGEN - always ignore what was typed */
	else unrchf = i;
#endif /*-IMAGEN*/
skipit:	chg_win(savwin);
	kill_win(w);
	redp(RD_WINDS);		/* Update all remaining windows */
}

/* Mode Line generation */

struct window *
make_mode(bw)
register struct window *bw;	/* Base window we are reporting status of */
{
	register struct buffer *b;	/* Buffer of this window */
	struct window *mw, *savew;	/* Save current window */
	struct buffer *saveb;	/* and current buffer (in case different) */
	char temp[20];

	saveb = cur_buf;	/* Save values prior to context switch */
	savew = cur_win;
	b = bw->w_buf;		/* Get buffer for that window */

#if FX_2MODEWINDS
	if((mw = sep_win) && (mw->w_flags&W_MODE) &&
	    (bw->w_pos == 0))		/* Base window is top window? */
	  {				/* Use sep_win as mode wind */
	  }
	else
#endif
		mw = mode_win;		/* Default is normal mode window */
	chg_win(mw);			/* Go to mode line window */
	e_gobob();			/* go to beginning */
	e_reset();			/* Flush buffer */
#if IMAGEN
	e_sputz(" ");
	e_sputz(b->b_name);
	if (b -> b_flags & B_MODIFIED)
		e_sputz("*");
	e_sputz(" (");
	if (b->b_flags & B_QUERYREP)
		e_sputz("[Query Replace] ");
	if (b->b_flags & B_CMODE)
		e_sputz("C");
	else if (b->b_flags & B_TEXTMODE)
		e_sputz("Text");
	else
		e_sputz("Fundamental");
	e_sputz(")  ");
	if (b->b_fn)
		e_sputz(b->b_fn);
	e_sputz("      ");
#else
	e_sputz(ev_verstr);		/* Editor name/version */
	e_sputz(" (");
	e_sputz(cur_mode->mjm_name);	/* insert major mode name */
#if FX_FILLMODE
	if(fill_mode) e_sputz(" Fill");
#endif /*FX_FILLMODE*/
#if FX_SKMAC
	if(kdef_mode) e_sputz(" MacroDef");
#endif /*FX_SKMAC*/
	e_sputz(") ");
	e_sputz(b->b_name);		/* buffer name */
	e_sputz(": ");
	if (b->b_fn)
		e_sputz(b->b_fn);       /* file name */
	if (b->b_flags & B_MODIFIED)
		e_sputz(" *");
	else	e_sputz("  ");
#endif /*-IMAGEN*/
	if(bw->w_pct < 200)		/* Not ALL? */
	  {	e_sputz(" --");
		switch(bw->w_pct)
		  {	case -1:
				e_sputz("TOP");
				break;
			case 150:
				e_sputz("BOT");
				break;
			default:
				dottoa(&temp[0],(chroff)bw->w_pct);
				e_sputz(&temp[0]);
				e_putc('%');
		  }
		e_sputz("--");
	  }
#if FX_SOWIND
	if(mw->w_flags&W_STANDOUT)
		e_insn(SP, (int)(scr_wd0 - e_blen()));	/* Stuff out with spaces */
#endif

	redp(RD_WINRES);
	chg_win(savew);		/* Restore context */
	chg_buf(saveb);
	return(mw);		/* Return mode window */
}


buf_mod()
{	register struct buffer *b;

	b = cur_buf;
	if((b->b_flags & B_MODIFIED) == 0)
	  {	b->b_flags |= B_MODIFIED;
		redp(RD_MODE);
	  }
}

/* BUF_TMOD - called when text modified in buffer, to set all
 *	the appropriate indicators so that redisplay works right.
 *	Changed text is everything from CUR_DOT to the given offset
 *	from same.  If stuff was deleted, offset should be 0.
 * BUF_TMAT - similar but argument is location of other end of range,
 *	when caller knows that and wants life easy.
 */

buf_tmat(dot)
chroff dot;
{	buf_tmod(dot - cur_dot);	/* Convert to offset */
}
buf_tmod(offset)
chroff offset;
{	register struct window *w;
	chroff a, b, tmp;

	w = cur_win;
	a = cur_dot;
	b = a + offset;
	if(a > b)	/* Get into right order */
	  {	tmp = a;
		a = b;
		b = tmp;
	  }
	b = e_blen() - b;	/* Make upper bound relative to EOB */
	if(w->w_bmod < 0)	/* Have range vars been set yet? */
	  {	w->w_bmod = a;	/* Nope, so can just set 'em now. */
		w->w_emod = b;
	  }
	else
	  {	if(a < w->w_bmod)
			w->w_bmod = a;
		if(b < w->w_emod)
			w->w_emod = b;
	  }
	buf_mod();		/* Maybe later just insert here? */
	redp(RD_TMOD);
}
