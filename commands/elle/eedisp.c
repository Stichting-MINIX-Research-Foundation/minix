/* ELLE - Copyright 1982, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/* EEDISP	Redisplay and screen image routines
 */

#if 0

Note that there are several different types of "efficiency" criteria
involved with respect to display updating:
	(1) Terminal speed: minimize # characters output.
	(2) Program speed: minimize CPU time used.
	(3) Program size: minimize code and memory usage.
	(4) Program modularity: minimize "hooks" between edit/display rtns.
The current algorithms necessarily represent a compromise among all of
these objectives.

	The cursor is always located at CUR_DOT in the buffer CUR_BUF
of the current window CUR_WIN.  This may not be true during function
execution, but is always true at the top-level loop of command
execution and redisplay.  In order to minimize update overhead, there
are various flags or variables that the edit functions can use to
communicate with "redisplay" and tell it how extensive the updates
really need to be.

	The entire known screen is always represented by a linked list
of "windows"; updating the entire screen consists of separately
updating every window on the list.  Windows can only be defined
horizontally (as a range of lines), and must not overlap.  Each window
has a buffer associated with it; the redisplay routines are responsible
for displaying the contents of this buffer.

	The lowest level data structure for the screen consists of an
array of SCR_LINE structures, one for each possible physical screen
line.  Each line structure has some flags, and pointers to three different
representations of what should be on the line:
	(1) SL_BOFF, SL_LEN - Defines the range of the buffer data which
		this screen line should represent.
		If the flag SL_EOL is set, this range ends with (and includes)
		an EOL character.
	(2) SL_LINE, SL_COL - Always keeps a copy of the current physical
		screen line image.  Each byte is a character which occupies
		only one column position on the screen.
		If the flag SL_CSO is set, the line is in standout mode.
	(3) SL_NLIN, SL_NCOL - The desired "new" screen line image.
		This is only valid if the SL_MOD flag is set for the line,
		indicating that these variables are set and point to the
		new image of what the screen line should be.
		If the flag SL_NSO is set, the new line should be in standout
		mode.

	Lastly there is a variable SL_CONT, which is needed for
continuation of too-long logical lines over several physical lines.  If
SL_CONT is:
	0 = logical line fits entirely on the screen.
		Either SL_EOL is set, or this line is ended by EOF
		(end of the buffer).
	1 = logical line is too long, but the last buffer char fits
		entirely on this physical line.  SL_EOL is never set.
	>1 = logical line is too long, and the last buffer char
		"overruns" the end of the physical image; that is, part of
		its representation is at the end of this line, but the
		rest of it is at the start of the next line.  This can
		only happen with "big" characters like TAB, ^A, ~^A, etc.
		that need more than one column of representation.
		There are SL_CONT-1 chars of overrun stored at the
		end of SL_LINE (SL_NLIN if SL_MOD is set).
		SL_EOL is never set.

Note that if a line contains any overrun, and the next line is also
part of the same window, the next line''s screen image will start with
the SL_CONT-1 chars of overrun, rather than with the representation of
that line''s first buffer char.

	The "EOL" character on Unix systems is normally the new-line
character '\n' (ASCII LF).  However, on other systems EOL may be
indicated by a two-character CR-LF sequence, with either CR or LF alone
considered to be "stray".  For this reason, the buffer flag B_EOLCRLF
exists to control handling and display of EOLs.  If the flag is off,
the EOL mode is LF, and there are no problems of splitting up characters.
If the flag is on, however, the EOL mode is CRLF and the following rules
hold:
	EOL is the sequence CR-LF only.
	LF without preceding CR is a "stray" LF, displayed as ^J.
	CR without following LF is a "stray" CR, displayed as ^M.
	Stray LFs and CRs do not terminate a logical line.
	"End of Line" as a position is the dot just before the CR of a CR-LF.
	"Beg of Line" as a position is the dot just after the LF of a CR-LF.
	If the current dot is between a CR and LF, it is positioned at
		the beginning of the physical screen line.


SL_LINE and SL_COL are always accurate at every stage of processing.
The other variables are accurate only after fix_wind has been called
to "fix up" the line structures within a window.  If either
RD_WINRES or RD_TMOD is set, none of these "other variables" should
be depended on.  Any functions which are screen-relative (d_ type)
must be sure that fix_wind is called if necessary, and must give
preference to the "new" representation in SL_NLINE and SL_NCOL if
SL_MOD is set.

The flag RD_UPDWIN will be set by fix_wind if any lines have been
modified.  Because fix_wind does not perform any actual display update,
it is possible for functions to continue operating on the buffer and
screen image without requiring that changes be displayed until there is
nothing else left to do.  The routine upd_wind performs the actual
terminal I/O necessary to update all the screen lines which have SL_MOD
set.  Although the process of updating each line is currently
non-interruptible, it is possible for upd_wind to interrupt itself
between line updates if it detects that user input has happened, and it will
return with the window only partially updated.  The screen image state
will be completely consistent, however, and the RD_UPDWIN flag will
remain set.

Communication between the editing functions and the redisplay routines
is limited as much as possible to the flags in the global RD_TYPE.
Each window has its own copy of these flags in W_REDP, so that if
windows are changed, the update hints for that window will be
preserved.  The flags that can be set are listed below.  Those marked
with "*" are global in nature; all others apply only within a single
window (normally the current window).

* RD_SCREEN - Total refresh.  Clears entire screen and redisplays all
	windows.
* RD_MODE - Mode line has changed, update it.
* RD_CHKALL - Check ALL windows for any redisplay flags, and perform
	any updates necessary.  Otherwise only the current (or specified)
	window flags are checked.
* RD_WINDS - Updates all windows.  Like RD_WINRES applied to all windows.
  RD_WINRES - Update window (assume completely changed).
  RD_TMOD - Text changed in this window.  The range of changes is
	specified by W_BMOD and W_EMOD in combination with W_OLDZ.
	Redisplay checking will limit itself to this range.
	These vars are set by buf_tmod in the main command loop, and
	reset by fix_wind when the window is fixed up.
  RD_MOVE - Cursor has moved within current window; may have moved outside
	the window.  W_DOT or CUR_DOT specifies where it should be.
  RD_ILIN - Hint: Line insert done.  Currently no function sets this.
  RD_DLIN - Hint: Line delete done.  Currently no function sets this.

Internal flags:
  RD_UPDWIN - Window needs updating. Used by fix_wind and upd_wind only.
	Set when window has been "fixed up" and at least one screen
	line was modified.
  RD_FIXWIN - Supposed to mean window needs fixing (via call to fix_wind).
	Not really used.

Not implemented, may never be, but comments retained:
  RD_WINCLR - Clear window (not entire screen)
  RD_NEWWIN - Window has moved.  (not needed? Random stuff here)
	a. to follow cursor; redisplay selects a new TOPLDOT.
	b. randomly; new TOPLDOT furnished, use unless cursor out (then a).
	c. find new TOPLDOT as directed (move up/down N screen lines)
	For now, assume that (c) doesn''t apply (ie C-V uses (b) and sets
	TOPLDOT itself).  So fix_wind selects new one only if cursor
	won''t fit.  topldot takes precedence over sl_boff.

#endif /*COMMENT*/

/* Declarations and stuff */

#include "elle.h"

static int sctr();

int trm_mode;	/* 0 = TTY in normal, non-edit mode.
		 * 1 = TTY in edit mode.
		 * -1 = TTY detached (hung up).
		 * This flag is only used by the 3 routines below,
		 * plus hup_exit.
		 */

/* REDP_INIT() - Called once-only at startup to initialize redisplay
 *	and terminal
 */
redp_init ()
{
	trm_mode = 0;		/* Ensure flag says not in edit mode */
	ts_init();		/* Get sys term info, set up stuff */
	if (trm_ospeed == 0)	/* Default speed to 9600 if unknown */
		trm_ospeed = 13;
	t_init();		/* Identify term type, set term-dep stuff */
	set_scr();		/* Set up software screen image */
	set_tty();		/* Enter editing mode! */
	redp(RD_SCREEN|RD_MODE); /* Force full re-display, new mode line */
}

/* SET_TTY() - Set up terminal modes for editing */

set_tty()
{	if(trm_mode) return;	/* Ignore if detached or in edit mode */
	trm_mode++;
	ts_enter();		/* Set up system's ideas about terminal */
	t_enter();		/* Set terminal up for editing */
}

/* CLEAN_EXIT() - Restore original terminal modes.
 *	Returns previous state.
 */
clean_exit ()
{	register int prevstate = trm_mode;

	if(prevstate > 0)	/* Ignore unless in editing mode */
	  {	trm_mode = 0;
		t_curpos(scr_ht-1, 0);	/* Go to screen bottom */
		t_exit();		/* Clean up the terminal */
		tbufls();		/* Force out all buffered output */
		ts_exit();		/* Restore system's old term state */
#if ! IMAGEN
		writez(1,"\n");		/* Get fresh line using OS output */
#endif /*-IMAGEN*/
	  }
	return prevstate;
}

/* SET_SCR() - Allocate screen image, set up screenline pointer table */

set_scr()
{	register struct scr_line **scrp, *stp;
	register scrsiz;
	char *sbuf;

	scr_wd0 = scr_wid - 1;
	scrsiz = scr_ht*(scr_wid+MAXCHAR);
	if(  scr_ht  > MAXHT || scr_wid > MAXLINE)
	  {	clean_exit();
		printf("ELLE: %dx%d screen too big\n",scr_ht,scr_wid);
		exit(1);
	  }
	if((stp = (struct scr_line *) calloc(scr_ht*sizeof(struct scr_line)
							 + scrsiz*2,1)) == 0)
	  {	clean_exit();
		printf("ELLE: not enough memory\n");
		exit(1);
	  }
	sbuf = (char *)stp + scr_ht*sizeof(struct scr_line);
	for(scrp = &scr[0]; scrp < &scr[scr_ht]; sbuf += scr_wid+MAXCHAR)
	  {	stp->sl_line = sbuf;
		stp->sl_nlin = sbuf + scrsiz;
		*scrp++ = stp++;
	  }
}

/* REDISPLAY()
 *	Main function of redisplay routines.  Called every time ELLE
 * forces update of the terminal screen.  "rd_type" contains hints
 * as to what has changed or needs updating, to avoid wasting time
 * on things which don't need attention.
 */
redisplay ()
{	register struct window *w;
	register i;
	struct window *make_mode();

	w = cur_win;
	w->w_redp |= rd_type&RDS_WINFLGS;	/* Set cur_win's flags */
	rd_type &= ~RDS_WINFLGS;		/* Leave only globals */

	if (rd_type & RD_SCREEN)		/* Clear and refresh? */
	  {
		t_clear ();			/* Clear the screen */
		for(i = scr_ht; --i >= 0;)	/* Clear screen image */
			scr[i]->sl_col = 0;
		if(w != ask_win)		/* If not in ask-window */
		  {	chg_win(ask_win);
			e_reset();		/* Then flush its contents */
			chg_win(w);
		  }
		redp(RD_WINDS);		/* Update all windows */
		rd_type &= ~RD_SCREEN;	/* If redisplay is interrupted, */
					/* don't do it all over again */
	  }
	if (rd_type & RD_WINDS)		/* Update all windows? */
	  {	redp(RD_CHKALL);
		for (w = win_head; w; w = w -> w_next)	/* For each win */
			w->w_redp |= RD_WINRES;
		rd_type &= ~RD_WINDS;
	  }
	if (rd_type & RD_CHKALL)	/* Check all windows for changes? */
	  {	for (w = win_head; w; w = w->w_next)	/* For each win */
		    if(!(w->w_flags&W_MODE))		/* skip mode wins */
			if(w->w_redp && upd_wind(w))
				return;		/* May be interrupted */

	  }

	/* See if ask-window needs updating (to avoid RD_CHKALL in SAY) */
	if((w = ask_win)->w_redp && upd_wind(w))
		return;				/* May be interrupted */

	/* Check current window for changes */
	if((w = cur_win)->w_redp && upd_wind(w))
		return;				/* May be interrupted */

	/* Now update mode line(s) if necessary */
	if(rd_type&RD_MODE)
	  {
		fupd_wind(w = make_mode(user_win));
#if FX_2MODEWINDS
		if (sep_win			/* If 2 windows */
		  && (sep_win->w_flags&W_MODE)	/* and 2 mode windows */
		  && (sep_win->w_redp || mode_win->w_redp))	/* Check */
			fupd_wind(make_mode(oth_win));	/* Must update both */
#endif
	  }

	/* Finally, leave cursor in right place. */
	if(upd_curs(cur_dot)==0)		/* If something screwed up, */
		errbarf("Cursor out of window");	/* Complain, */
						/* and leave cursor at bot */
	rd_type = 0;
	tbufls();		/* Force out all terminal output */
}

fupd_wind(w)		/* Force window update */
register struct window *w;
{
	w->w_redp |= RD_WINRES;
	if(fix_wind(w))
		upd_wind(w);
}

/*
 * UPD_CURS
 *	Move screen cursor to position of specified dot within current window.
 *	Returns 0 if dot was not within window (and cursor was not moved),
 *	otherwise returns 1 for success.
 */
upd_curs(adot)
chroff adot;
{	register struct scr_line *s;
	register int y, x;
	chroff savdot;

	if((y = d_line(adot)) < 0)
		return(0);	/* Fail, not within window */
	s = scr[y];		/* Now have line that dot is on */

	/* Get proper offset for any continuation chars from prev line */
	if(y > cur_win->w_pos)
	  {	if((x = scr[y-1]->sl_cont) > 0)
			x--;
	  }
	else x = 0;

	savdot = e_dot();
	e_go(s->sl_boff);
	if((x = d_ncols((int)(adot - s->sl_boff),x)) < 0)
	  {	/* If lost, assume it's because we are just after a char
		** which has its representation continued onto next line.
		** Move cursor to end of that continuation.
		** d_line should have ensured that this is safe, but
		** we double-check just to make sure.
		*/
		if((x = s->sl_cont) > 0)	/* Set X to end of cont */
			--x;
						/* and on next line down */
		if(++y >= (cur_win->w_pos + cur_win->w_ht))
		  {	e_go(savdot);		/* Failed, below window */
			return(0);
		  }
	  }
	e_go(savdot);
	t_move(y, x);		/* Move cursor cleverly */
	return(1);		/* Return success! */
}

/* Return line # for given dot, -1 if out of current window */
d_line(cdot)
chroff cdot;
{	register struct scr_line *s;
	register struct window *w;
	register int i;
	chroff savdot;
	int bot;

	w = cur_win;
	i = w->w_pos;
	bot = i + w->w_ht;
	for(; i < bot; i++)
	  {	s = scr[i];
		if(cdot <= s->sl_boff)
			goto gotl;
	  }
	/* End of window, repeat test specially for last line */
	savdot = s->sl_boff + (chroff)s->sl_len;
	if(cdot > savdot)	/* If past last char of last line */
		return(-1);	/* then clearly outside */
	--i;			/* Make i match s (bottom line) */
	if(savdot != cdot)	/* If not exactly at end */
		return(i);	/* Then we're inside for sure */
	goto linbet;

gotl:	if(s->sl_boff != cdot)	/* Are we on line boundary? */
	  {	if(i <= w->w_pos)	/* No, off top of window? */
			return(-1);	/* Above top, out for sure */
		return(--i);
	  }

	/* Here, dot is exactly on line boundary, have to decide which
	 * line it really belongs to.
	 * Get S = pointer to line which cursor is at the end of.
	 */
	if(i <= w->w_pos)	/* Quick chk of trivial case, empty buffer */
		return(i);
	s = scr[--i];
linbet:
	if((s->sl_flg&SL_EOL)	/* If line has LF */
	  || (s->sl_cont > 1))	/* or a continued char */
		if(++i >= bot)		/* Then cursor is on next line */
			return(-1);
	return(i);
}

/* D_NCOLS - auxiliary for UPD_CURS.  (also called by indtion() in EEFD)
**	 We are positioned at a place in the current buffer corresponding to
** the beginning of the screen line, and given:
**	lcnt - # of chars in buffer to move forward over
**	ccol - current column position
** Returns the new column position.  There are some special cases:
**	Hits EOF: returns normally (new column position)
**	Hits EOL: returns -1
**	Position is past end of screen: returns -1
** The buffer position has changed, but this is irrelevant as upd_curs
** restores it just after the call.
*/
d_ncols(lcnt, ccol)
int lcnt;
int ccol;
{	register int col, i;
	register SBBUF *sb;
	int c;
	char tmp[MAXCHAR*2];	/* MAXCHAR is enough, but *2 just in case */

	col = ccol;
	sb = (SBBUF *) cur_buf;
	if((i = lcnt) > 0)
		do {	if((c = sb_getc(sb)) == EOF)
				break;
			/* Check to see if we've run into an EOL */
#if FX_EOLMODE
			if(c == CR)
			  {	if(eolcrlf(sb))
				  {	if((c = sb_getc(sb)) == LF) /* EOL? */
					/* Real EOL.  Fail unless point
					** is between CR and LF, in which case
					** we return 0 (left margin).
					*/
					    return (i==1 ? 0 : -1);
					/* Stray CR, back up & fall thru */
					if(c != EOF)
						sb_backc(sb);
					c = CR;
				  }
			  } else if (c == LF)
			  {	if(!eolcrlf(sb))	/* Real EOL? */
					return -1;	/* Yes, fail */
				/* If EOL mode is CRLF then hitting a LF
				** can only happen for stray LFs (the
				** previous check for CR takes care of
				** CRLFs, and we never start scanning
				** from the middle of a CRLF.
				** Drop thru to show stray LF.
				*/
			  }
#else
			if(c == LF)
				return(-1);
#endif /*-FX_EOLMODE*/
			col += sctr(c, tmp, col);
		  } while(--i);
	if(col > scr_wd0)
		return(-1);
	return(col);
}

/* D_LUPD - called from command level to completely redisplay a
 *	specific line on the screen.
 */
d_lupd(w, idx)
struct window *w;		/* Window this line belongs to, if known */
int idx;
{	t_curpos(idx, 0);
	t_docleol();		/* Zap physical screen line */
	scr[idx]->sl_col = 0;	/* Reflect it on phys screen image */
	if(w)			/* Mark window for updating */
		w->w_redp |= RD_WINRES;
	else redp(RD_WINDS);	/* No window given, assume global */
	redp(RD_MOVE);		/* Cursor has moved */
}

/* Clear a window completely the "quickest possible way" */
clear_wind(w)
register struct window *w;
{
	register int i = w->w_pos;	/* Top line of window */
	register int bot = i + w->w_ht;	/* Bottom line (plus 1) of window */

	for ( ; i < bot; ++i)
		d_lupd(w, i);		/* Zap that line */
}

/* FIX_WIND - Sets up window screen image.  Does not generate any
 *	terminal output, but completely specifies what the new screen
 *	image should look like.
 *	Only the following 4 flags (lumped together in RDS_DOFIX)
 *	provoke fix_wind to do something:
 *		RD_MOVE - cursor has moved, must make sure still within
 *			window, and select new one if not.
 *		RD_TMOD - Text has been changed somewhere.
 *		RD_FIXWIN - Something requested that fix_wind fix things.
 *			Normally this is set when a new w_topldot is set.
 *		RD_WINRES - Window needs to be completely regenerated.
 * Results:
 *	Verifies that the current dot for the window (w_dot) exists.
 * If it is past the end of buffer, it is reset to EOB, and if this is
 * the current window, also updates cur_dot.  Otherwise, w_dot is never
 * adjusted; it is fix_wind's responsibility to make sure that the window
 * displays w_dot.
 *	Verifies that current w_topldot setting will result in cursor
 * (specified by w_dot) appearing within window.  If not, resets w_topldot
 * to an appropriate value (1/3 of way down from top, unless
 * moving up in which case 1/3 of way up from bottom).
 *	Makes sure that sl_boff, sl_len, sl_flg, and sl_cont
 * are set properly for all lines in window.  SL_MOD is set
 * for any lines requiring screen updates; these lines
 * also have sl_nlin and sl_ncol properly set.
 *	Note that sl_line and sl_col are NOT updated or changed, because
 * the physical screen has not been altered!
 *
 *	Returns 0 if no physical screen updates are needed (other than
 *		cursor moving and mode line updating).
 *	Returns 1 if screen updates are needed; RD_UPDWIN is set in w_redp,
 *		indicating that UPD_WIND should be called.
 */

fix_wind (win)
struct window *win;
{
	register struct window *w;
	register int i;
	register struct scr_line *s;
	chroff cdot, bdelta, updot, sdot, newz;
	chroff savdot;
	struct buffer *savbuf;
	int bot, nlmod, savi, contf, ocontf, randomflg;
	int newpct;

	if(!(w = win))
		return(0);
	if(!(w->w_redp&RDS_DOFIX))	/* Anything we need to do? */
		return(0);		/* Nope, just ignore */

	/* Find current dot for this window, and set up other stuff */
	cdot = (w == cur_win) ? cur_dot : w->w_dot;
	bot = w->w_pos + w->w_ht;
	savbuf = cur_buf;
	cur_buf = w->w_buf;
	savdot = e_dot();
	nlmod = 0;			/* No screen image changes so far */

	/* Dot (ie cursor) is before current top?  If so, must move
	 * backwards to find a new topldot.  Note also that buffer may have
	 * changed so that either cdot or topldot points past EOF.
	 */
	if(w->w_topldot > cdot)
	  {	/* Yes, must search backwards scrht/3 screen lines */
		/* from cdot in order to find topldot. */
		/* Don't bother updating scr stuff beforehand since we'll
		 * have to revise everything anyway and can do it on the fly.
		 */
		i = (ev_mvpct * w->w_ht) / 100;
		goto skipdn;

	finddn:	i = ((100 - ev_mvpct) * w->w_ht) / 100;
	skipdn:	if(i <= 0) i = 1;	/* Ensure # is reasonable */
		else if(i >= w->w_ht) i = w->w_ht-1;
		e_go(cdot);		/* Start here (may normalize to EOF)*/
		d_backup(i ? i : 1);	/* Try to back up cleverly */
		w->w_topldot = e_dot();
		randomflg = 0;		/* We have some idea where we are */
	fixall:		/* Entry point for later recheck, with randomflg==1 */
		newz = e_blen();
		if(newz < cdot)		/* Part of buf may have gone away */
		  {			/* So normalize dot to EOF */
			w->w_dot = cdot = newz;
			if(w == cur_win)	/* Special check for fixing */
				cur_dot = newz;	/* up cur_dot too! */
			goto finddn;	/* and get a new top-of-window loc */
		  }
	retry:	i = w->w_pos;
		contf = 0;
		s = 0;
		for(; i < bot; i++)
		  {	nlmod++;
			fix_line(scr[i], s);	/* s = 0 the first time */
			s = scr[i];
#if FX_SOWIND
			if(w->w_flags & W_STANDOUT)
				s->sl_flg |= SL_NSO;
			else s->sl_flg &= ~SL_NSO;
#endif
		  }
		if(inwinp(w,cdot))	/* Ensure in window */
			goto mdone;
		if(randomflg)		/* If jumped randomly, */
		  {	i = (ev_nwpct * w->w_ht) / 100;
			goto skipdn;	/* Try to select new window */
		  }

		/* We tried to back up and went too far. */
		if(cdot < w->w_topldot)	/* Verify place is ahead */
		  {	errbarf("fix_wind failed");	/* Didn't back up?? */
			goto finddn;
		  }
		/* Move down one line and try again */
		if(w->w_ht > 1)
			w->w_topldot = scr[w->w_pos+1]->sl_boff;
		else
		  {	s = scr[w->w_pos];
			w->w_topldot = s->sl_boff + s->sl_len;
		  }
		e_go(w->w_topldot);
		goto retry;
	  }

	/* At some future point, could separate out processing for
	 * RD_WINRES and RD_FIXWIN.  Latter flag implies only w_topldot
	 * has changed (new window selected).  Former implies whole
	 * buffer has been munged, and everything is completely redone.
	 */
	if(w->w_redp&(RD_WINRES|RD_FIXWIN))	/* If re-figuring whole window */
	  {	e_go(w->w_topldot);	/* Start here, and */
		randomflg = 1;		/* set up flag saying random jump */
		goto fixall;		/* and go crunch all lines. */
	  }
	if((w->w_redp&RD_TMOD)==0)	/* If claims no text mods, */
	  {	if(inwinp(w,cdot)==0)	/* Just verify cursor loc. */
			goto finddn;	/* Sigh.... */
		newz = w->w_oldz;	/* Win, set up for exit. */
		goto done;
	  }
	/* Here only when RD_TMOD is set, indicating changes are
	 * between range variables.
	 */
	/* Find upper bound of any mods.  This is a little gross in the
	 * speed dept and some faster way should perhaps be devised.
	 * In particular the main loop should incrementally keep track of
	 * buffer size, and should set a flag RD_TEXT if anything has
	 * actually been changed.  Edit routines should have lots of
	 * flags available to tell main loop more precisely what they did,
	 * so main loop can take care of updating b/emod and stuff.
	 */
	if((newz = e_blen()) == 0)
		goto finddn;		/* Ensure blank window is cleared */
	bdelta = newz - w->w_oldz;
	if((updot = newz) > w->w_emod)
		updot -= w->w_emod;
	if(bdelta == 0 && (updot == w->w_bmod))
		goto inwinq;

	/* Could also check for updot < w_topldot (changes above win)
	 * or sl_boff+sl_len < w_bmod  (changes below win) but those
	 * cases are probably pretty rare.
	 */
	/* First find line where changes start */
	for(i = w->w_pos; i < bot; i++)
	  {	s = scr[i];
		if(w->w_bmod <= s->sl_boff)	/* Changes prior to this? */
			break;
	  }
	if(i >= bot)			/* Test last line specially */
	  {	if(w->w_bmod > (s->sl_boff + (chroff)s->sl_len))
			goto inwinq;	/* Outside window */
					/* Last line changed, hack it */
	  }
	if(i > w->w_pos			/* If we have a prev line */
	  && (s->sl_len == 0		/* and we're at EOF, */
	    || w->w_bmod != s->sl_boff	/* or not at start of line */
	    || scr[i-1]->sl_cont))	/* or prev line is continuation */
		s = scr[--i];		/* then it's prev line we want */

	/* I has index for screen line changes begin on; S has ptr.
	 * This piece of code handles case where buffer has been modified
	 * starting at BMOD, and BDELTA chars have been inserted/deleted;
	 * range of changes ends at UPDOT.
	 */
	savi = i;
	while(++i < bot)
		scr[i]->sl_boff += bdelta;
	i = savi;

	/* Now start with 1st changed line and start figuring new line
	 * lengths.  Stop when hit end, or past updot and boff is correct
	 * for start of line.
	 */
	/* can improve this by jumping out when past emod, and testing for
	 * an EOL - then know stuff has to match someplace, so look for that.
	 * could then simply update lengths or something?
	 */
	if(i > w->w_pos)	/* Find # cols already there from prev line*/
		contf = scr[i-1]->sl_cont;
	else contf = 0;
	ocontf = 1;			/* Fake it so always update 1st line*/
	e_go(sdot = s->sl_boff);
	for(; i < bot; i++)
	  {	s = scr[i];
		if(updot <= sdot	/* If past changed stuff */
		  && sdot == s->sl_boff	/* and locs are lined up */
		  && contf == 0		/* and previous line clean */
		  && ocontf == 0)	/* (both old and new images) */
			break;		/* Then done. */
		nlmod++;
		ocontf = s->sl_cont;	/* Save old-image contf value */
		fix_line(s, (i > w->w_pos) ? scr[i-1] : 0);
#if FX_SOWIND
		if(w->w_flags & W_STANDOUT)
			s->sl_flg |= SL_NSO;
		else s->sl_flg &= ~SL_NSO;
#endif
		sdot = e_dot();
		contf = s->sl_cont;	/* Get new-image contf value */
	  }
	if(inwinp(w,cdot))	/* OK, screen fixed, see if cursor inside */
		goto mdone;
	goto finddn;

	/* Test if still in window and dispatch appropriately */
inwinq:	if(inwinp(w,cdot))
		goto done;
	else goto finddn;

	/* Come here when done, after mods made to window.
	 * Calculate new %-of-buffer position for window's view, and
	 * see if it's changed from current %.
	 */
mdone:	if(w != cur_win) goto done;	/* If not current window, ignore */
	s = scr[bot-1];
	if((s->sl_boff + (chroff)s->sl_len) >= newz)
		if(w->w_topldot) newpct = 150;	/* BOT */
		else newpct = 200;		/* ALL */
	else if(w->w_topldot == 0)
		newpct = -1;			/* TOP */
	else		/* NOTE: This won't work if topldot is huge */
		newpct = (w->w_topldot*100)/newz;	/* nn% */
	if(newpct != w->w_pct)		/* OK, now compare with old % */
	  {	w->w_pct = newpct;	/* Different, must set and */
		redp(RD_MODE);		/* invoke redisplay of mode line! */
	  }

done:	w->w_bmod = -1;		/* To indicate vars not set */
	w->w_oldz = newz;
	w->w_redp &= ~RDS_DOFIX;	/* Clear flags that invoked us */
	if(nlmod)
		w->w_redp |= RD_UPDWIN;	/* Say stuff to be updated */
	e_go(savdot);
	cur_buf = savbuf;
	return(nlmod);
}

/* INWINP - Returns true if given dot is inside given window.
 */
inwinp(win,cdot)
struct window *win;
chroff cdot;
{	register struct scr_line *s;
	register struct window *w;
	chroff sdot;

	w = win;
	if(cdot < w->w_topldot)
		return(0);
	s = scr[(w->w_pos + w->w_ht) - 1];
	sdot = s->sl_boff + (chroff)s->sl_len;
	if(cdot < sdot)
		return(1);		/* Yup, inside window. */
	if(cdot > sdot)
		return(0);

	/* Dot is exactly at end of window, must check further. */
	if(s->sl_len		/* If line exists, */
	 && ((s->sl_flg&SL_EOL)	/* and ends in LF, */
	    || s->sl_cont > 1))	/* or sl_cont > 1, lose. */
		return(0);
	return(1);		/* Else inside, win. */
}

/*
 * UPD_WIND
 *	If argument 0, assumes cur_win and DOESN'T interrupt if input
 *	detected.
 */

upd_wind(win)
struct window *win;
{	register int i, n;
	register struct scr_line *s;
	struct window *w;
	int top, bot, dspf, num, isave, noicost, nodcost, iline, dline;
#if FX_SOWIND
	int oldso;
#endif
#if IMAGEN
	int origdspf;
	char redpmsg[128];
#endif /*IMAGEN*/

	if((w=win)==0)
		w = cur_win;
	dspf = w->w_redp;		/* Get update flags for window */
#if IMAGEN
	origdspf = dspf;
#endif /*IMAGEN*/
	if(w == cur_win)		/* If updating current window, */
		dspf |= rd_type;	/* merge in global flags */
	if((dspf &= RDS_WINFLGS) == 0)	/* Well, it might happen sometimes */
		goto zdone;
	w->w_redp = dspf;
	if(dspf&(RD_WINRES|RD_TMOD|RD_MOVE|RD_FIXWIN))
	  {	fix_wind(w);		/* May set some flags, so */
		dspf = w->w_redp;	/* get them back... */
	  }
	if((dspf&RD_UPDWIN)==0)		/* Must ask for update! */
		goto zdone;
#if IMAGEN
	if (dbg_redp)
	  {	sprintf(redpmsg,
			"buffer: %14s, rd_type: %06o, w_redp: %06o, dspf: %06o",
			w->w_buf->b_name, rd_type, origdspf, dspf);
		barf2(redpmsg);
	  }
#endif /*IMAGEN*/

	/* Assume screen structure set up by FIX_WIND, just go
	 * effect change for every line modified.
	 */
#if FX_SOWIND
	oldso = t_dostandout((w->w_flags&W_STANDOUT)? 1:0);
#endif
	top = w->w_pos;
	bot = top + w->w_ht;
	for(i = top; i < bot; ++i)
	  if((s = scr[i])->sl_flg&SL_MOD)
	  {	if(win && tinwait())	/* If OK, stop if any chars typed */
		  {	tbufls();
			w->w_redp = dspf;
#if FX_SOWIND
			t_dostandout(oldso);
#endif
			return(1);	/* Return immediately, say int'd */
		  }
		if(slineq(s,s))		/* Compare old with new */
			goto ldone;	/* Lines equal, no update needed */
	
#if IMAGEN
		/* If hint says redo entirely */
		if (dspf & RD_REDO)
		  {	s->sl_flg |= SL_REDO;	 /* Do "fast update" */
			goto nodel;		/* Just go update line */
		  }
#endif /*IMAGEN*/
		if((trm_flags&TF_IDLIN)==0)
			goto nodel;		/* Just go update line */


		/* Check for I/D line.  If no hints exist, check for both
		 * insert and delete.
		 */
		if((dspf&(RD_ILIN|RD_DLIN))==0)
			dspf |= RD_ILIN|RD_DLIN;
		noicost = 0;
		nodcost = 0;

		/* Check for insert line.  See if the current old screen
		 * line is duplicated among any of the new lines which
		 * follow it.  If a match is found, keep looking and add
		 * up the number of characters in the matching lines.
		 */
		if(dspf&RD_ILIN)
		  {
			/* See if this old screen line is needed elsewhere */
			if(s->sl_col == 0)	/* Ignore if blank */
				goto noins;
			
			for(n = i+1; n < bot; n++)
			  {	if((scr[n]->sl_flg&SL_MOD)==0)
					break;
				if(slineq(s, scr[n]))	/* Old, new */
				  {	if(!noicost) iline = n;	/* 1st time */
					noicost += s->sl_col;
					s++;
				  }
				else if(noicost) break;
			  }
			if(!noicost)		/* If no match, forget it */
				goto noins;	/* S will not have changed. */
			s = scr[i];		/* Restore S */
			n = iline;		/* Have matches, get index
						 * of first matching line */

			/* Heuristic to decide whether to perform
			 * insert-line operation.  Kind of stupid, but
			 * good enough for now.
			 */
			num = (n-i)*(tvc_ldn+tvc_lin) + (tvc_li + tvc_ld);
			if((n-i) >= (scr_ht-(ECHOLINES+3))
						/* Don't move lines all the
						 * way down full screen! */
			  || num >= noicost)	/* Compare cost with estimated
						 * cost of not doing insert.*/
				goto noins;

			/* Insert lines! */
			dspf &= ~RD_ILIN;
			inslin(i, n - i, w);
			for(; i < n; i++)	/* Update intervening lines */
				upd_line (i);
			goto ldone;
		  }
noins:

		/* Check for delete line.  See if the new screen line
		 * is duplicated among any of the old lines already on
		 * the screen.  If a match is found, keep looking and add
		 * up the number of characters in the matching lines.
		 */
		if(dspf&RD_DLIN)
		  {
			/* See if the new line already exists elsewhere */
			if(s->sl_ncol == 0)	/* Ignore blank lines */
				goto nodel;
			for (n = i + 1; n < bot; n++)
			  {	if((scr[n]->sl_flg&SL_MOD)==0)
					break;
				if(slineq(scr[n],s))	/* Old, new */
				  {	if(!nodcost) dline = n;	/* 1st time */
 					nodcost += s->sl_ncol;
					s++;
				  }
				else if(nodcost) break;
			  }
			if(!nodcost)		/* If no match, forget it */
				goto nodel;	/* S will not have changed. */
			s = scr[i];		/* Restore S */
			n = dline;		/* Index of 1st match */

			/* Heuristic to decide whether to perform
			 * delete-line operation.  Same hack as for
			 * insert-line.
			 */
			num = (n-i)*(tvc_ldn+tvc_lin) + (tvc_li + tvc_ld);
			if((n-i) >= (scr_ht-(ECHOLINES+3))
			  || num >= nodcost)
				goto nodel;

			/* Delete lines! */
			dspf &= ~RD_DLIN;
			dellin(i, n - i, w);
			goto ldone;
		  }
nodel:
		/* All failed, so just update line */
		upd_line(i);
ldone:		s->sl_flg &= ~SL_MOD;	/* Clear mod flag */
	  }
done:
#if FX_SOWIND
	t_dostandout(oldso);	/* Back to previous mode */
#endif
zdone:	w->w_redp = 0;
	return(0);		/* Say completed */
}


/*
 * SLINEQ - Compare old, new screen image lines.  If new line doesn't
 *	have the modified flag set, use its old image.
 *	If the standout mode differs, always fails.
 */

slineq(olds, news)
struct scr_line *olds;
struct scr_line *news;
{	register char *cpo, *cpn;
	register int cnt;

	cpo = (char *)news;
	if(((struct scr_line *)cpo)->sl_flg&SL_MOD)
	  {	cnt = ((struct scr_line *)cpo)->sl_ncol;
		cpn = ((struct scr_line *)cpo)->sl_nlin;
#if FX_SOWIND		/* Mode of old must match mode of new */
		if(((olds->sl_flg & SL_CSO)==0) !=
			((((struct scr_line *)cpo)->sl_flg & SL_NSO)==0))
			return 0;
#endif
	  }
	else
	  {	cnt = ((struct scr_line *)cpo)->sl_col;
		cpn = ((struct scr_line *)cpo)->sl_line;
#if FX_SOWIND		/* Modes of current lines must match */
		if((olds->sl_flg & SL_CSO) !=
			(((struct scr_line *)cpo)->sl_flg & SL_CSO))
			return 0;
#endif
	  }

	/* Crufty match stuff */
	if(cnt != olds->sl_col)
		return(0);
	if(cnt)
	  {	cpo = olds->sl_line;
		do { if(*cpo++ != *cpn++)
			return(0);
		  } while(--cnt);
	  }
	return(1);
}

/* UPD_LINE(lineno) - Effects the update of a physical screen line,
 *	assuming that the screen line structure for that line has been
 *	properly set up by fix_wind.  It cannot be interrupted by typein.
 *	Does a lot of work to check out optimization for char I/D.
 *	Someday it could also check out the possibility of doing a CLEOL at
 *	some point to reduce the number of spaces that need to be output.
 */

upd_line(y)
int y;
{	register i;
	register char *sci, *cp;
	struct scr_line *s;

	int xpos;			/* actual screen position */
	int c, c2, p2, cmpcost, delcost;
	int savc, ocol, ncol;
	char *savcp, *savsci;
#if FX_SOWIND
	int oldso, newso;
	int writall = 0;
#endif

	s = scr[y];
	savsci = sci = s->sl_line;	/* What is currently on the screen */
#if IMAGEN
	if (s->sl_flg & SL_REDO)
	  {	/* Check for line-redo flag */
		s->sl_flg &= ~SL_REDO;	/* Clear it: we are handling it */
		writall = 1;	/* Re-do this line completely */
		t_move(y, 0);
		t_docleol();
		s->sl_col = 0;
	  }
#endif /*IMAGEN*/

#if FX_SOWIND
	/* See whether modes of the lines are the same or not. */
	newso = (s->sl_flg & SL_NSO)!=0;	/* Get new mode (true if SO)*/
	if(((s->sl_flg & SL_CSO)!=0) !=	newso)
	  {	t_move(y, 0);		/* Not same, must zap existing line */
		t_docleol();
		s->sl_col = 0;
		writall = newso;	/* Output all if SO is new mode */
	  }
	oldso = t_dostandout(newso);	/* Get in right mode */
#endif

	ocol = s->sl_col;
	savcp = cp = s->sl_nlin;
	ncol = s->sl_ncol;

	/* Find leading equalness */
	i = ocol;
	if(i > ncol) i = ncol;		/* Use minimum count */
	if(i)
	  {	do { if(*cp++ != *sci++)
			  {	--cp;
				break;
			  }
		  } while(--i);
		i = cp - savcp;
		sci = savsci;		/* Restore ptr to beg of cur line */
	  }

	/* From here on, "i" is now the x-coordinate (column addr)
	 * of the first position that doesn't match.  "cp" points to
	 * the first nonmatching char in the new line image.
	 */
#if COHERENT		/* Has direct video interface capability */
	if(trm_flags&TF_DIRVID)
	  {	if(ncol < ocol)
		  {	/* Flesh out new line to completely replace old */
			fillsp(&s->sl_nlin[ncol], ocol-ncol);
			ncol = ocol;
		  }
		/* Spit out changed stuff.  t_direct will handle the
		 * case where i == ncol (ie no changes needed).
		 */
		t_direct(y,i,cp,ncol-i);
		goto done;
	  }
#endif /*COHERENT*/

	if(i == ncol)			/* Matched up to end of new line? */
		goto idone;		/* Yes, can skip big loop! */

#if FX_SOWIND
	if(writall)			/* If simply writing everything...*/
	  {	t_move(y, 0);
		tputn(cp, ncol);	/* Output them all */
		curs_col = ncol;	/* Update cursor position */
		goto idone;		/* then wrap up! */
	  }
#endif

	/* Now must fill out remainder of old line with blanks. */
	if(ocol < scr_wid)
	  {
#if FX_SOWIND
		if(newso) fillset(&sci[ocol], scr_wid-ocol, 0);
		else
#endif
		fillsp(&sci[ocol],scr_wid-ocol);	/* Fill out */
	  }

	/******  Main update loop. ******/
	for (; i < ncol; i++)
	  {	c = *cp++;		/* Note *CP will point to next */
		if(c == sci[i])
			continue;
		if(i >= ocol)		/* Past EOL of old line? */
		  {
putin:			sci[i] = c;
			if(y != curs_lin || i != curs_col)
				t_move(y, i);
			tput(c);
			curs_col++;
			continue;
		  }

		if((trm_flags&TF_IDCHR)==0)	/* Replace */
			goto putin;

		/* Do checking to see whether char I/D operations should
		 * be invoked.  This code is quite CPU intensive and
		 * can cause noticeable pauses if run on a slow CPU with
		 * a fast (9600) terminal line.  The optimization tradeoff
		 * seems worthwhile most of the time, however.
		 */
		cmpcost = 0;		/* Default is don't compare */
		if(ncol == ocol)	/* If line lengths same, must chk */
		  {
/*			if(ncol >= scr_wid) */	/* If line overrun, compare */
				cmpcost++;
		  }
#if 0
If ncol == ocol, have problem with tabs:
	If don''t use I/D char, but tabs exist, lots of wasteful update.
	If DO use I/D char, and no tabs exist, potential for mistakenly
		using I/D when didn''t have to.  Not too bad, though?
	If DO use I/D char, then mild screw when inserting/deleting
		just before a tab, since could have just overwritten,
		but I/D insists on jerking things around.
	Insert test:
		If old char was space, replace? Problem: will cause cursor
		jump if really should have shifted a long run of spaces.
		But that is probably okay.
	Delete test:
		If new char is space, replace? again, will cause cursor jump
		with long run of spaces.
#endif /*COMMENT*/

		if(ncol < ocol || cmpcost)	/* Try delete-char */
		  {
			/* Search old for match of c and nextc */
dodel:			savc = c;
			if(i >= ncol-1)
				goto putin;
			c2 = *cp;
			if(c == SP && ncol == ocol)
				goto tryins;
			p2 = i;
			for(;;)
			  {	if(c == sci[i] && c2 == sci[i+1])
					break;
				if(++i < ocol)
					continue;
				i = p2;
				if(cmpcost) {cmpcost = 0; goto tryins;}
				goto putin;
			  }
			/* Find # chars that match (i.e. will be saved) */
			for(c=1; (i+c < ncol) && (sci[i+c] == cp[c-1]); c++);
			delcost = tvc_cd + tvc_cdn*(i - p2);
			if(delcost >= c)
			  {	c = savc;
				i = p2;
				if(cmpcost) { cmpcost = 0; goto tryins;}
				goto putin;	/* Punt */
			  }
			if(cmpcost)
			  {	c = savc; i = p2;
				goto tryins;
			  }
			t_move(y, p2);
			c = i - p2;	/* Find # chars to flush */
			strncpy(&sci[p2],&sci[i], ocol-i);
			ocol -= c;
			fillsp(&sci[ocol], c);
			i = p2;			/* Restore i */
			t_delchr(c);		/* Flush this many cols */
			continue;
		  }

		/* Try ins-char */
		/* Search new for match of i and i+1 */
		/* Note this cannot be used while in standout mode, since
		** the new spaces created will probably be in the wrong mode.
		*/
tryins:
#if FX_SOWIND
		if(newso) goto putin;
#endif
		if(i+1 >= ocol)
			goto putin;

		savc = c;
		savcp = cp;
		c2 = sci[i+1];
		if(sci[i] == SP && ncol == ocol)
			goto putin;
		xpos = i;		/* save current col */
		i++;
		for(;;)
		  {	if(i >= ncol) goto puntx;
			c = *cp++;
inlp2:			if(c != sci[xpos])
			  {	if(i > scr_wid) goto puntx;
				i++;
				continue;
			  }
			if(i >= ncol) goto puntx;
			c = *cp++;
			if(c != c2)
			  {	i++;		/* Allow for previous c */
				goto inlp2;	/* which is always 1 */
			  }
			break;
		  }
		if(i >= scr_wid) goto puntx;

		/* Find how many chars match (i.e. will be saved) */
		for(c = 2; xpos+c < ncol && sci[xpos+c] == *cp++; c++);
		if((p2 = tvc_ci + tvc_cin*(i - xpos)) >= c)
			goto puntx;	/* Not worth it... */
		if(cmpcost && p2 >= delcost)
			goto puntx;	/* Do delchr instead */

		/* We've decided to insert some chars! */
		i -= xpos;		/* Get # char positions to insert */
		cp = savcp;		/* Get ptr to newline string */
		--cp;			/* Point at 1st char to insert */
					/* Make room in scr array */
		inspc(&sci[xpos],
			&sci[(ocol+i >= scr_wid) ? scr_wid-i : ocol], i);
		ocol += i;		/* Update size of old line */
		strncpy(&sci[xpos], cp, i);	/* Copy all inserted chars */

		t_move(y, xpos);	/* Now ensure in right place */
		t_inschr(i, cp);	/* and insert string onto screen! */

		cp += i;		/* Update source ptr */
		cp++;			/* Point to next char */
		i += xpos;
		continue;		/* Now continue loop! */

	puntx:	i = xpos;
		c = savc;
		cp = savcp;
		if(cmpcost) { cmpcost = 0; goto dodel;}
		goto putin;
	  }

	/* All done putting up new stuff.  Now see if any remaining old
	** stuff needs to be cleared from end of line.
	*/
idone:	if(i < ocol)		/* if still have text to right, */
	  {	t_move(y,i);	/* move there */
		t_docleol();	/* and clear old stuff. */
	  }

done:	s->sl_line = s->sl_nlin;	/* Replace old image by new */
	s->sl_col = s->sl_ncol;
	s->sl_nlin = sci;
	s->sl_flg &= ~SL_MOD;
#if FX_SOWIND			/* Copy standout mode to current */
	if(newso) s->sl_flg |= SL_CSO;
	else s->sl_flg &= ~SL_CSO;
#endif
}

#if FX_SOWIND
fillset(str,cnt,c)
char *str;
int cnt;
int c;
{	register int n;
	register char *cp;
	if((n = cnt) <= 0) return;
	cp = str;
	do{ *cp++ = c;
	  } while(--n);
}
#endif

fillsp(str,cnt)
char *str;
int cnt;
{	register int n;
	register char *cp;
	if((n = cnt) <= 0) return;
	cp = str;
	do{ *cp++ = SP;
	  } while(--n);
}
inspc(cp0, cpl, cnt)
char *cp0, *cpl;
int cnt;
{	register char *cp, *cp2;
	register n;
	if((n = cnt) <= 0) return;
	cp = cpl;		/* ptr to last+1 char in string */
	cp2 = cp+n;		/* ptr to loc+1 to move to */
	n = cp - cp0;		/* # chars to move */
	do *--cp2 = *--cp;
	while(--n);
	n = cnt;		/* Now fill gap with spaces */
	do *cp++ = SP;
	while(--n);
}

/* FIX_LINE - Fixes up new screen image for a single line.  Does not
 *	do any actual terminal I/O, and does not change the old screen
 *	image.  Assumes that previous line (if any is furnished) has
 *	already been properly set up.
 */

int sctreol = 0;	/* Ugly crock for talking to sctrin() */
			/* 0 = no EOL seen, 1 = EOL seen, -1 = EOF seen */
fix_line(slp, olds)
struct scr_line *slp;
struct scr_line *olds;
{	register struct scr_line *s;
	register int col, scrw;
	char *cp;
	int ch;

	col = 0;
	scrw = scr_wid;
	cp = slp->sl_nlin;
	if((s = olds) && (col = s->sl_cont))
	  {	if(--col)
			strncpy(cp, (s->sl_flg&SL_MOD) ?
					&s->sl_nlin[scrw]
					 : &s->sl_line[scrw], col);
		cp += col;
	  }
	scrw--;			/* Note now using scr_wd0 !! */
	s = slp;
	s->sl_boff = e_dot();
	col = sctrin(cp, scrw, col);
	if (col < scrw || sctreol)	/* Does line need continuation mark? */
		s->sl_cont = 0;		/* No, say no cont chars */
	else {
		/* Yes, find # cols of overflow.  If not 0, must be > 0 */
		/* and char is a biggie.  Make room for continuation chars */
		if(col -= scrw)
			inspc(&s->sl_nlin[scrw],&s->sl_nlin[scrw+col], 1);
		s->sl_cont = col+1;		/* # cont chars, plus 1 */
		s->sl_nlin[scrw] = CI_CLINE;	/* Display "contin" mark */
		col = scrw+1;
	  }

	s->sl_ncol = col;
	s->sl_len = e_dot() - s->sl_boff;
	s->sl_flg |= (SL_MOD|SL_EOL);	/* Say new, and assume line has EOL */
	if(sctreol <= 0)		/* unless it doesn't really */
		s->sl_flg &= ~SL_EOL;	/* in which case turn off flag */
	return;
}

/* SCTRIN - auxiliary for FIX_LINE.
 *	lim - # cols chars are allowed to use
 *	ccol - current column (0 = bol)
 * Returns when see EOL or EOF, or
 *	when all columns have been filled up.  Retval-ccol = # overflow.
 *	Note that any overflow is indivisible (i.e. a char with a
 *	multi-col representation is responsible for the overflow).
 *	So, overflow = 0 means next char would be in 1st non-ex column
 *	and overflow > 0 means last char read has extra columns, but
 *	it did start within bounds.
 */
sctrin(to, lim, ccol)
char *to;
int lim;
int ccol;
{	register SBBUF *sb;
	register col, cnt;

	sb = (SBBUF *) cur_buf;
	col = ccol;
	sctreol = 0;		/* No EOL or EOF seen */
	do
	  {	cnt = sb_getc(sb);
		if(cnt == EOF)
		  {	--sctreol;	/* Say EOF seen! */
			return(col);
		  }
#if FX_EOLMODE
		if(cnt == CR)		/* Possible EOL? */
		  {	if(eolcrlf(sb))
			  {	if((cnt = sb_getc(sb)) == LF)	/* Real EOL? */
				  {	sctreol++;
					return col;	/* Yes, return */
				  }
				/* Stray CR, back up & fall thru */
				if(cnt != EOF)
					sb_backc(sb);
				cnt = CR;	/* Show stray CR */
			  }
		  } else if (cnt == LF)
		  {	if(!eolcrlf(sb))	/* Real EOL? */
			  {	sctreol++;
				return col;	/* Yes, return */
			  }
			/* If EOL mode is CRLF then hitting a LF
			** can only happen for stray LFs (the
			** previous check for CR takes care of
			** CRLFs, and we never start scanning
			** from the middle of a CRLF.
			** Drop thru to show stray LF.
			*/
		  }
#else
		if(cnt == LF)
		  {	sctreol++;	/* Say EOL seen */
			return col;
		  }
#endif /*_FX_EOLMODE*/
		cnt = sctr(cnt, to, col);
		to += cnt;
		col += cnt;
	  } while(col < lim);

	/* If we're stopping because last char put us precisely at the
	** end of the line, make a further check to see whether an EOL
	** is next.  If so, we can include that in the line since it
	** doesn't need any more columns for representation!
	*/
	if (col == lim)		/* If stopping exactly at edge of screen */
	    switch (sb_getc(sb))	/* Check out next char */
	      {	case EOF:
			--sctreol;		/* Yes, note EOF seen */
			break;			/* and can return immed */
#if FX_EOLMODE
		case CR:		/* Possible EOL? */
			if(eolcrlf(sb))
			  {	if((cnt = sb_getc(sb)) == LF)	/* Real EOL? */
				  {	sctreol++;	/* Yes, set flag */
					break;		/* and return */
				  }
				/* Stray CR, back up & fall thru */
				if(cnt != EOF)		/* Back up char that */
					sb_backc(sb);	/* came after the CR */
				sb_rgetc(sb);		/* Then back over CR */
				break;
			  }
			sb_backc(sb);
			break;
		case LF:
			if(!eolcrlf(sb))	/* Real EOL? */
			  {	sctreol++;	/* Yes, set flag */
				break;		/* and return */
			  }
			/* If EOL mode is CRLF then hitting a LF
			** can only happen for stray LFs (the
			** previous check for CR takes care of
			** CRLFs, and we never start scanning
			** from the middle of a CRLF.
			** Drop thru into default to back up over LF.
			*/
#else
		case LF:
			sctreol++;	/* Say EOL seen */
			break;		/* and return */
#endif /*-FX_EOLMODE*/
		default:
			sb_backc(sb);		/* Back up over random char */
			break;
	    }
	return(col);
}

/* SCTR - Screen Char TRanslation routine.
**	This routine is completely responsible for the way a buffer char is
** displayed on the screen.  Given a char and the current column position,
** it stores the representation using the given pointer and returns
** the number of chars (columns) used by the representation.
**	Normal printing chars (plus space) are simply themselves.
**	TAB is a variable number of spaces depending on the column pos.
**		(we use standard tabstops of 8)
**	All control chars are uparrow followed by a printing char.
**		e.g. ctrl-A = ^A
**		This includes ESC which is ^[.
**		DEL is shown as ^?.
**	Chars with the 8th bit set have the prefix CI_META (currently ~) and
**		the rest of the representation is as above (except for TAB).
**	Chars with the 9th bit set have the prefix CI_TOP (currently |) and
**		the rest of the representation is as above (except for TAB).
**		This only exists for systems with 9-bit chars such as TOPS-20.
*/

static int
sctr(ch, to, ccol)
int ch;			/* Buffer char to translate */
char *to;		/* Place to deposit translation in */
int ccol;		/* Current column position */
{	register char *cp;
	register c, n;

	c = ch;
	if(037 < c && c < 0177)	/* Most common case */
	  {	*to = c;
		return(1);
	  }
	cp = to;
	if(c == TAB)			/* Next most common case */
	  {	n = 010 - (ccol&07);	/* Tab stops are every 8 cols */
		ccol = n;		/* Save value */
		do *cp++ = SP;
		while (--n);
		return(ccol);
	  }
	ccol = 1;			/* Re-use var */
#if TOPS20
	if(c&0400)			/* 9th bit set? */
	  {	*cp++ = CI_TOP;
		ccol++;
	  }
#endif /*TOPS20*/
	if(c&0200)
	  {	*cp++ = CI_META;
		ccol++;
	  }
	if((c &= 0177) <= 037 || c == 0177)
	  {	*cp++ = CI_CNTRL;
		c ^= 0100;		/* Transform cntrl char */
		ccol++;
	  }
	*cp = c;
	return(ccol);
}

/* INSLIN(line, N, wind) - Insert lines
 * DELLIN(line, N, wind) - Delete lines
 *	Both routines insert/delete N lines at "line" in window "wind"
 *	and update the screen image accordingly.
 */

inslin (line, n, win)
int   line;			       /* line number to insert BEFORE */
int   n;			       /* number of lines to insert */
struct window *win;		       /* window we are in */
{	register int  i;
	register int bot;
	register char **savp;
	char *savscr[MAXHT];

	bot = win -> w_ht + win -> w_pos;
	t_curpos (line, 0);
	t_inslin (n, bot);		/* do the insertion on the screen */
	savp = &savscr[0];
	for (i = 1; i <= n; i++)	/* free lines that fall off-screen */
		*savp++ = scr[bot - i]->sl_line;

	for (i = bot - 1; i >= line + n; i--)		/* move down lines */
	  {	scr[i]->sl_line = scr[i - n]->sl_line;	/* below the insertion */
		scr[i]->sl_col = scr[i - n]->sl_col;
	  }
	savp = &savscr[0];
	for (i = line + n - 1; i >= line; i--)
				       /* blank lines where inserted */
	  {	scr[i]->sl_line = *savp++;
		scr[i]->sl_col = 0;
	  }
	for(i = line; i < bot; ++i)
		scr[i]->sl_flg |= SL_MOD;
}

dellin (line, n, win)
int   line;			       /* first line to be deleted */
int   n;			       /* number of lines to be deleted */
struct window *win;		       /* window we are in */
{	register int  i;
	register int bot;
	register char **savp;
	char *savscr[MAXHT];

	bot = win -> w_ht + win -> w_pos;

	t_curpos (line, 0);
	t_dellin (n, bot);	       /* do the deletion on the screen */
	savp = &savscr[0];
	for (i = line; i < line + n; i++)    /* free the deleted lines */
		*savp++ = scr[i]->sl_line;
	for (i = line; i < bot - n; i++)	/* move lines up to fill */
	  {	scr[i]->sl_line = scr[i + n]->sl_line;	/* deleted spaces */
		scr[i]->sl_col = scr[i + n]->sl_col;
	  }

	savp = &savscr[0];
	for (i = bot - n; i < bot; i++)      /* blank lines at bottom */
	  {	scr[i]->sl_line = *savp++;
		scr[i]->sl_col = 0;
	  }
	for(i = line; i < bot; ++i)
		scr[i]->sl_flg |= SL_MOD;
}

/* T_ Terminal functions - these are similar to the terminal-dependent
 *	routines in EETERM (which they call) but rely on some knowledge of
 *	the screen image in order to do their job cleverly.
 */

#if FX_SOWIND

/* T_DOSTANDOUT(on) - Turn standout mode on or off, cleverly.
**	Returns previous state.
*/
static int curso = 0;		/* Current state (initially off) */
int
t_dostandout(on)
int on;
{
	int oldso;

	if ((oldso = curso) != on)	/* If desired state doesn't match, */
	  {	t_standout(on);		/* invoke new state. */
		curso = on;
	  }
	return oldso;
}
#endif


t_move(y,x)
register int y,x;
{	register int d;

	if(y != curs_lin)		/* No vertical smarts yet */
	  {	t_curpos(y, x);
		return;
	  }
	if((d = (x - curs_col)) >= 0)	/* Find diff in column position */
	  {	if(d == 0) return;	/* If none, nothing to do! */

		/* Moving right.  If distance is less than abs-move cost,
		 * do clever right-move by copying screen image */
		if(d < tvc_pos)
#if FX_SOWIND	/* Ensure not in standout mode */
			if((scr[y]->sl_flg&(SL_CSO|SL_NSO))==0)
#endif
		  {
			tputn(&scr[y]->sl_line[curs_col], d);
			curs_col = x;
			return;
		  }
	  }
	/* Moving to left, try to do clever left-move by backspacing
	 * instead of using abs move.
	 */
	else if((d = -d)*tvc_bs < tvc_pos)
	  {	do { t_backspace();
		  } while(--d);
		return;
	  }
	/* No luck with cleverness, just move. */
	t_curpos(y, x);
}

t_docleol()
{	register struct scr_line *s;
	register int cnt, ocol;

	if(trm_flags&TF_CLEOL) t_cleol();	/* Winning */
	else		/* Losing */
	  {	s = scr[curs_lin];
		if((cnt = s->sl_col - curs_col) > 0)
		  {
#if FX_SOWIND
			int oldso = t_dostandout(0);
#endif
			ocol = curs_col;
			do { tput(SP); curs_col++;
			  } while(--cnt);
#if FX_SOWIND
			t_dostandout(oldso);
#endif
			t_move(curs_lin, ocol);
		  }
	  }
}

