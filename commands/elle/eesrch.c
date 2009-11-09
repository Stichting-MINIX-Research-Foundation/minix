/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EESRCH	Searching functions
 */

#include "elle.h"
#if !(V6)
#include <signal.h>
#else
#include "eesigs.h"		/* Use this on V6 system */
#endif /*V6*/

/*
 *   Buffer String Search routines
 *
 *      If no search string is provided, a string that was previously
 *      used in the last search is once again used.
 */

/* EFUN: "String Search" */
f_srch()
{	return (lin_search (0));
}

/* EFUN: "Reverse String Search" */
f_rsrch()
{	return (lin_search (1));
}

/* LIN_SEARCH - Main routine for non-incremental String Search.  Asks for
 *	a search string and looks for it.
 */ 
lin_search (backwards)
int  backwards;
{	register char *mem;	/* item to be searched for */
	register int res;
	int srchint(), (*sav_srchalarm)();
	char *srch_ask();
	chroff savdot;

	savdot = cur_dot;		/* Save original loc */

#if ICONOGRAPHICS
        if((mem = srch_ask(backwards ? "Reverse Search%s%s%s"
                                     : "Search%s%s%s"))==0)
                return;
#else
        if((mem = srch_ask(backwards ? "Reverse Search: " : "Search: "))==0)
                return;
#endif /*-ICONOGRAPHICS*/
	sav_srchalarm = signal(SIGALRM,/*&*/srchint);	/* Handle timeout */
	alarm(1);					/* One sec from now */

	res = e_search(mem,srch_len,backwards);	/* Search for str! */

	alarm(0);				/* Turn off alarm */
	signal(SIGALRM,sav_srchalarm);		/* Restore old handler */

	if(res)				/* Search won? */
	  {	ed_setcur();
		return;
	  }

	/* Search failed */
	e_gosetcur(savdot);
	ding("Search Failed");
}

srchint()
{	yelltoo(" ...");
}

char *
srch_ask(prompt)
char *prompt;
{	register char *ans, *old;

#if ICONOGRAPHICS
        if (srch_str)
           ans = ask(prompt, " (", srch_str, "): ");
        else ans = ask (prompt, ": ", "", "");
        if (ans == 0) return (0);
#else
        if((ans = ask(prompt)) == 0)
                return(0);              /* user punted ... */
#endif /*-ICONOGRAPHICS*/
	old = srch_str;
	if (*ans == '\0')
	  {	chkfree(ans);
		if ((ans = old) == 0)		/* no string specified */
		  {	dingtoo("Nothing to search for");
			return(0);
		  }
#if !(ICONOGRAPHICS)
		saylntoo(old, srch_len);	/* Show what old string is */
#endif /*-ICONOGRAPHICS*/
	  }
	else
	  {	if (old)
			chkfree(old);	/* free up old srch string */
		srch_str = ans;
		srch_len = ask_len;
	  }
	return(ans);
}

#if 0
	Incremental Search stuff.
Description of EMACS behavior:
	^Q quotes next char.
	DEL cancels last char.  If this cancelled a match, point is moved
		to previous match.
	If not all of input can be found, it is not discarded.  Can rub out,
		discard unmatched stuff with ^G, exit, etc.
	^S repeats search forward; ^R repeats backward.
		If empty string, either
			changes direction (if not same)
			or brings back previous string
	ESC exits.  If empty string, changes to non-incremental string search.
	^G of a winning search aborts, exits, and moves point back to origin.
	^G of a failing search discards the input that wasn''t found.
	Other C- or M- chars exit and are executed.
ELLE also interprets ^H (BS) as DEL, because some keyboards make it hard to
	type DEL and there is no way the user can
	re-bind the incremental-search commands.
#endif /*COMMENT*/

#if FX_ISRCH
/* EFUN: "Incremental Search" */
f_isrch() { i_search(0); }
#endif /*FX_ISRCH*/

#if FX_RISRCH
/* EFUN: "Reverse Search" */
f_risrch() { i_search(1); }
#endif /*FX_RISRCH*/

#if FX_ISRCH || FX_RISRCH

i_search(back)
int back;		/* Current mode: 0 if forward, 1 if backward */
{	register int c;
	register int inpcnt;	/* # chars in current input srch str */
	int inpgood;		/* Length of last winning string */
	char inpstr[ISRCHLIM];	/* Holds current input search string */
	chroff inpdot[ISRCHLIM];	/* Holds winning addrs for each */
	struct window *savwin;
	int winning;	/* 1 = currently winning, 0 = currently failing */
	int pref, shown;
	int f_insself(), (*(cmd_fun()))();

	winning = 1;
	inpcnt = 0;
	inpgood = 0;
	inpdot[0] = cur_dot;
	savwin = cur_win;

	/* Set up prompt and read all TTY input thus far */
	shown = 0;
 sloop:	c = cmd_wait();		/* See if any command input waiting */
	if(shown || !c)
	  {	e_setcur();	/* Assume we moved around, so set cur_dot */
		chg_win(ask_win);
		ed_reset();	/* Flush contents & invoke redisplay */
		ed_sins(back ? "R-search: " : "I-search: ");
		ed_nsins(inpstr, inpcnt);
		if(!winning) ed_sins("\t(FAILING)");
		upd_wind((struct window *)0);	/* Force ask_win update */
		if(c)
		  {	upd_curs(cur_dot);
			tbufls();
		  }
		chg_win(savwin);
		shown = 1;	/* Say search prompt has been shown */
	  }
	if(!c)			/* If no user input waiting, show buffer */
	  {	redp(RD_MOVE);		/* Cursor moved in window */
		redisplay();
	  }
	c = cmd_read();		/* Get input char */
	switch(c)
	  {	case DEL:		/* Cancel last char */
		case BS:		/* Hard to type DEL on some kbds */
			if(inpcnt <= 0) goto sloop;
			if(--inpcnt > inpgood) goto sloop;
			winning = 1;
			if(inpcnt == inpgood) goto sloop;
			inpgood--;
			ed_go(inpdot[inpcnt]);
			goto sloop;

		case CTRL('Q'):
			c = cmd_read();	/* Quote next char */
			break;
		case CTRL('S'):
			pref = 0;
			goto ctlsr;
		case CTRL('R'):
			pref = 1;
			goto ctlsr;

		case CTRL('G'):
			if(winning)
			  {	ed_go(inpdot[0]);
				goto sdone;
			  }
			inpcnt = inpgood;
			winning = 1;
			goto sloop;
		case ESC:
		case CR:
			if(inpcnt)
				goto sdone;
			lin_search(back);
			return;
		default:
			if(f_insself != cmd_fun(c))
			  {	unrchf = c;
				goto sdone;
			  }
		case TAB:	/* Strange self-inserting char */
			break;
	  }
	if(inpcnt >= ISRCHLIM-1)
	  {	ding("I-search str too long");
		sleep(1);
		goto sdone;
	  }
	inpstr[inpcnt++] = c;
	if(!winning) goto sloop;

	/* Now search for string.  (Arm alarm interrupt?) */
	/* cur_dot has current location cursor is at; we want to back off
	 * from this so a repeated search will find the same location if
	 * appropriate. */
	e_igoff(back ? inpcnt : -(inpcnt-1));
dosrch:
	winning = e_search(inpstr,inpcnt,back);
	if (winning)
	  {	inpgood = inpcnt;	/* Remember last win length */
		inpdot[inpcnt] = e_dot();	/* and location */
	  }
	else e_gocur();			/* Back to start position */
	goto sloop;

 ctlsr:	if (pref != back)
	  {	back = pref;
		if(inpcnt <= 0) goto sloop;
	  }
	if(inpcnt <= 0)
	  {	if(!srch_str || (inpcnt = srch_len) <= 0)
			goto sloop;
		bcopy((SBMA)srch_str, (SBMA)inpstr, srch_len);
		inpcnt = srch_len;
		unrchf = c;		/* Repeat cmd after display */
		shown = 1;		/* Force search-string display */
		goto sloop;
	  }
	goto dosrch;

  sdone:
	if(srch_str) chkfree(srch_str);
	srch_str = memalloc((SBMO)(inpcnt+1));
	bcopy((SBMA)inpstr,(SBMA)srch_str,inpcnt);	/* Copy into srch_str */
	srch_len = inpcnt;
	e_setcur();
	chg_win(ask_win);
	ed_reset();
	chg_win(savwin);
	redp(RD_CHKALL);
}
#endif /*FX_ISRCH || FX_RISRCH*/
