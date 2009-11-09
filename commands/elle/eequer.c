/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EEQUER	Query-Replace and Replace-String functions
 */

#include "elle.h"		       /* include structure definitions */

/* EFUN: "Query Replace" */
/*	Crude approximation of EMACS function.
 */
f_querep()
{	static struct majmode iqrpmode = { "Query Replace" };
	ed_dorep(0, &iqrpmode);
}

/* EFUN: "Replace String" */
/*	Similar to Query Replace and uses same code.
 */
f_repstr()
{	static struct majmode irepmode = { "Replace String" };
	ed_dorep(1, &irepmode);
}

#if FX_REPLINE
/* EFUN: "Replace in Line" (not EMACS) */
/*	Acts like Replace String but only operates on current line.
**	Currently a big crock.
**	Feature of crockishness is that Unkill Pop (M-Y) will restore old
**	line.
*/
f_repline()
{
	extern struct buffer *make_buf();
	struct buffer *b, *oldb = cur_buf;
	static struct majmode rlmode = { "Replace in Line" };

	if(!(b = make_buf(" **LINE**")))
	  {	ring_bell();
		return;
	  }
	f_kline();		/* Kill line(s) from original buffer */
	chg_buf(b);		/* Switch to temp buffer */
	f_unkill();		/* Get killed stuff into temp buffer */
	e_gosetcur((chroff)0);	/* Starting at beginning, */
	ed_dorep(1, &rlmode);		/* Execute Replace String on it. */
	ed_kill((chroff)0, e_blen());	/* Now kill everything in it, */
	chg_buf(oldb);		/* switch back to original buffer, */
	f_unkill();		/* and restore new stuff! */
	kill_buf(b);		/* Now flush temporary buffer. */
}
#endif


/* Note that the major mode is set without changing the buffer's major
 * mode.  When the function is done, the current major mode is reset
 * from the buffer mode.
 */
ed_dorep(type, mode)		/* 0 = Query Replace, 1 = Replace String */
int type;
struct majmode *mode;
{	register int c;
	register int olen, allflg;
	char *srch_ask();
	char *opromp, *npromp;
	char *nstr, *ostr;	/* Note ostr is == to srch_str */
	int nlen;
	chroff last_loc;
#if IMAGEN
	int nrepled = 0;
	char replmsg[64];
#endif /*IMAGEN*/

	/* Set mode, then get search string and replace string */
#if IMAGEN
	cur_win->w_buf->b_flags |= B_QUERYREP;
#else
	cur_mode = mode;	/* Set major mode pointer */
#endif /*-IMAGEN*/

	redp(RD_MODE);
	nstr = 0;
#if IMAGEN
	opromp = "Old string: ";
	npromp = "New string: ";
#else
	opromp = "Replace string: ";
	npromp = "with string: ";
#endif /*-IMAGEN*/
	if((ostr = srch_ask(opromp)) == 0)
		goto done;
	olen = srch_len;	/* srch_ask sets this! */
	if((nstr = ask("%s%s %s", opromp, ostr, npromp)) == 0)
		goto done;
	nlen = ask_len;

	/* Now enter search and subcommand loop */
	allflg = type;		/* Unless 0 for Query Rep, replace all */ 
	for(;;)
	  {	last_loc = cur_dot;
		if(e_search(ostr,olen,0) == 0)
			break;
		ed_setcur();			/* Cursor moved */
	redisp:
		if(!allflg) redisplay();	/* Update screen */
	getcmd:
		if(!allflg) c = cmd_read();
		else c = SP;
		switch(c)
		  {
#if IMAGEN
			case 'n':
#endif /*IMAGEN*/
			case DEL:	/* Don't replace, go on */
				continue;
#if IMAGEN
			case ',':
#endif /*IMAGEN*/
			case '.':	/* Replace and exit */
			case SP:	/* Replace, go on */
				ed_delete(cur_dot,(chroff)(cur_dot-olen));
				ed_nsins(nstr,nlen);
#if IMAGEN
				++nrepled;
#endif /*IMAGEN*/
				if(c == '.') goto done;
				continue;
#if IMAGEN
			default:
#endif /*IMAGEN*/
			case '?':	/* Show options */
#if IMAGEN
				saynow("\
' '=>change, 'n'=>don't, '.'=>change, quit, '!'=>change rest, '^'=>back up");
#else
				saynow("\
SP=Replace, DEL=Don't, ESC=Stop, !=Replace all, ^=Back up, .=Replace & Stop");
#endif /*-IMAGEN*/
				goto getcmd;
			case '^':	/* Return to last place found */
				ed_go(last_loc);
				goto redisp;

			case CTRL('G'):
			case ESC:	/* Exit where we are */
				goto done;

			case CTRL('L'):	/* Redisplay */
				redp(RD_SCREEN);
				goto redisp;

			case '!':	/* Replace all the rest */
				allflg++;
				goto getcmd;

#if !(IMAGEN)
			case ',':	/* Replace and show */
			case CTRL('R'):	/* Enter edit mode recursively */
			case CTRL('W'):	/* Delete once and ^R */
				saynow("not implemented");
				goto getcmd;
			default:	/* Exit and re-read char */
				unrchf = c;
				goto done;
#endif /*-IMAGEN*/
		  }
	  }
done:
#if IMAGEN
	cur_win->w_buf->b_flags &= ~B_QUERYREP;
#else
	cur_mode = cur_buf->b_mode;
#endif /*-IMAGEN*/

	redp(RD_MODE);
	if(nstr) 	/* Free nstr (but not ostr, it's == srch_str!) */
		chkfree(nstr);
#if IMAGEN
	if (nrepled <= 0)
		saynow("No replacements done");
	else
	  {	sprintf(replmsg, "Replaced %d occurences", nrepled);
		saynow(replmsg);
	  }
#endif /*IMAGEN*/
}
