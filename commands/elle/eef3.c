/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EEF3		Various Functions (Yanking, Indentation, miscellaneous)
 */

#include "elle.h"

#if FX_APPNKILL
/* EFUN: "Append Next Kill" */
f_appnkill()
{	this_cmd = KILLCMD;	/* Fake out next call to ed_kill */
}
#endif /*FX_APPNKILL*/

#if FX_UNKILL
/* EFUN: "Un-kill" */
f_unkill()
{	register SBSTR *sd;

	if((sd = kill_ring[kill_ptr]) == 0)
	  {	ring_bell();
		return;
	  }
	mark_dot = cur_dot;		/* Set mark at old location */
	mark_p = 1;			/* Mark's been set */
	sb_sins((SBBUF *)cur_buf,sbs_cpy(sd));	/* Insert copy of stuff */
	cur_dot = e_dot();		/* We're now after the new stuff */
	buf_tmat(mark_dot);		/* Say modified from here to cur_dot*/
	this_cmd = YANKCMD;
}
#endif /*FX_UNKILL*/

#if FX_UNKPOP
/* EFUN: "Un-kill Pop" */
f_unkpop()
{	register SBSTR *sd;
	register int i;

	if (last_cmd != YANKCMD)
	  {	ring_bell ();
		return;
	  }
	ed_delete(cur_dot,mark_dot);
	if(cur_dot > mark_dot)
		cur_dot = mark_dot;
	i = KILL_LEN;
	do {
		if(--kill_ptr < 0)
			kill_ptr = KILL_LEN-1;
		if(sd = kill_ring[kill_ptr])
			break;
	  } while(--i);

	/* kill_ptr now pointing to right place; effect the yank. */
	e_gocur();		/* Make sure point at right place too! */
	return(f_unkill());
}
#endif /*FX_UNKPOP*/

/* Indentation routines - still not polished */

#if FX_INDATM
/* EFUN: "Indent According to Mode" */
/*	In Fundamental mode, just inserts a tab.
*/
f_indatm()
{	f_insself(TAB);		/* This takes care of mode checking */
}
#endif /*FX_INDATM*/

#if FX_INDNL
/* EFUN: "Indent New Line" */
f_indnl()			/* execute CR followed by tab */
{
#if IMAGEN
	/* Not dispatch-based, but rather hard-wired to do Gosmacs thing */
	ed_crins();
	f_indund();
#else
	cmd_xct(CR);
	cmd_xct(TAB);
#endif /*-IMAGEN*/
}
#endif /*FX_INDNL*/


#if FX_BACKIND
/* EFUN: "Back to Indentation"
**	Moves to end of current line's indentation.
*/
f_backind()
{	e_gobol();	/* First move to beg of line */
	e_gofwsp();	/* Then forward over whitespace */
	ed_setcur();
}
#endif /*FX_BACKIND*/


#if FX_INDCOMM

static char *comm_beg = "/* ";
static char *comm_end = " */";

/* EFUN: "Indent for Comment" */
f_indcomm()
{
	f_endline();
	if(indtion(cur_dot) < ev_ccolumn)
		ed_indto(ev_ccolumn);
	else ed_sins("  ");
	ed_sins (comm_beg);
	ed_sins (comm_end);
	e_igoff(-strlen (comm_end));       /* back over end string */
	e_setcur();
}
#endif /*FX_INDCOMM*/

#if FX_INDREL
/* EFUN: "Indent Relative" */
/* This used to mistakenly be called Indent Under.
**	Still not fully implemented.
**	If at beginning of line, looks back at previous indented line,
** and indents this line that much.  If there is no preceding indented
** line or not at beginning of line, insert a tab.
*/
f_indrel()
{	register int c;
	register  n;
#if IMAGEN
	chroff savdot;
#endif /*IMAGEN*/
#if ICONOGRAPHICS
        chroff savdot;
        int curind, newind, morebuf;
#endif /*ICONOGRAPHICS*/

	if((c = e_rgetc()) == EOF)
#if IMAGEN
		return(f_insself(TAB));	/* Do mode-based tabbing */
#else
		return(ed_insert(TAB));
#endif /*-IMAGEN*/

	if(c == LF)
	  {	e_gobol();
		e_gofwsp();
		n = d_curind();
		e_gonl();		/* Return to orig pos */
		if(n)
		  {	ed_indto(n);
#if IMAGEN
			savdot = e_dot();
			e_gofwsp();
			ed_delete(savdot, e_dot());
#endif /*IMAGEN*/
			return;
		  }
	  }
#if ICONOGRAPHICS
        else
          {     e_igoff (1);
                curind = indtion (savdot = e_dot ());
                                /* get current dot and indentation */
                while (1)       /* find a prev line that extends rightward */
                   {    morebuf = e_gopl ();
                        e_goeol ();
                        if ((newind = d_curind()) > curind) break;
                        if (morebuf == 0)  /* hit beginning of buffer */
                        {       e_go (savdot);
                                f_delspc();
                                return (1);
                        }
                   }

                e_gobol ();
                e_igoff (inindex (e_dot (), curind));
                if (d_curind() > curind)
                        e_rgetc ();              /* pushed ahead by tab */

                while (c_wsp (e_getc ()) == 0) ;
                e_backc ();
                e_fwsp ();
                newind = d_curind();
                e_go (savdot);
                f_delspc();
                ed_indto (newind);
           }
#else
        else e_getc();
#if IMAGEN
	f_insself(TAB);			/* Do mode-based tabbing */
#else
	ed_insert(TAB);
#endif /*-IMAGEN*/
#endif /*-ICONOGRAPHICS*/
}
#endif /*FX_INDREL*/


/* Paren matching stuff.  Note that this stuff will be very painful unless
** tinwait() works properly.
*/
#if 0
/* EFUN: "Self-Insert and Match" (intended to be bound to brackets) */
/* (KLH: Evidently this was never finished)
*/
insertmatch(c)
register int c;
{
	
}
#endif

/* Should change this to Matching Paren */
#if FX_MATCHBRACK
/* EFUN: "Match Bracket" (not EMACS) - from IMAGEN config
 * Show the matching bracket for the character right before dot
 */
f_matchbrack()
{
	chroff savdot;
	register int i, mc, secs;

	if (exp_p)
		secs = exp;
	else
		secs = 1;
	savdot = cur_dot;		/* Save our location */
	mc = e_rgetc();			/* Pick up character before dot */
	if (mc != ')' && mc != ']' && mc != '}')
	  {	e_getc();		/* Nothing, try at dot instead */
		e_getc();
		mc = e_rgetc();
		if (mc != ')' && mc != ']' && mc != '}')
		  {	ding("What bracket?");
			e_go(savdot);
			return;
		  }
	  }
	if (! matchonelevel(mc))
		ring_bell();
	else
	  {	ed_setcur();
	        if (d_line(cur_dot) < 0)
			secs = 10;	/* Wait longer if off-screen */
		redisplay();		/* Wish it were simple upd_wind() */
	        for (i = 1; i <= secs; ++i)
		  {	if (tinwait())
				break;
			sleep(1);
	          }
	  }
	e_gosetcur(savdot);		/* Back to origin */
	redp(RD_MOVE);			/* Cursor has moved */
}


/* Try to match 'mc', return true iff found it */
matchonelevel(mc)
register int mc;
{
	register int c;

	while ((c = e_rgetc()) != EOF)
	  {	if (c == /*[*/ ']' || c == /*(*/ ')' || c == /*{*/ '}')
		  {	if (! matchonelevel(c))
				break;
		  }
		else if (c == '(' /*)*/)
			return(mc == /*(*/ ')');
		else if (c == '[' /*]*/)
			return(mc == /*[*/ ']');
		else if (c == '{' /*}*/)
			return(mc == /*{*/ '}');
	  }
	return(0);
}
#endif /*FX_MATCHBRACK*/
