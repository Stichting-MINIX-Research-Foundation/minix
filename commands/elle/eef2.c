/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EEF2		Various functions
 */

#include "elle.h"

/* Line Handling functions */

/* EFUN: "Beginning of Line" */
f_begline()
{	e_gobol();
	ed_setcur();
}

/* EFUN: "End of Line" */
f_endline()
{	e_goeol();
	ed_setcur();
}

/* EFUN: "Next Line" */
/*	Goes to beginning of next line */
f_nxtline()
{	return(down_bline(exp));
}

/* EFUN: "Previous Line" */
/*	Goes to beginning of previous line */
f_prvline()
{	return(down_bline(-exp));
}

/* EFUN: "Down Real Line" */
f_dnrline ()
{	down_line(exp);
}

/* EFUN: "Up Real Line" */
f_uprline ()
{	down_line(-exp);
}

#if FX_OLINE
/* EFUN: "Open Line" */
f_oline()
{	register int i;
	chroff savdot;

	savdot = cur_dot;
	if((i = exp) > 0)
		do { ed_crins(); }
		while(--i);
	e_gosetcur(savdot);
}
#endif /*FX_OLINE*/

#if FX_DELBLINES
/* EFUN: "Delete Blank Lines" */
/*	Delete blank lines around point.
 */
f_delblines()
{	register int c;
	chroff dot1, dot2, oldcur;

	oldcur = cur_dot;
	do { e_gobwsp(); }
	while ((c = e_rgetc()) == LF);
	if (c != EOF)
		e_gonl();
	dot1 = e_dot();
	if(dot1 > oldcur) return;
	do { e_gofwsp(); }
	while ((c = e_getc()) == LF);
	if(c != EOF)
		e_gobol();
	dot2 = e_dot();
	if(dot2 < oldcur) return;
	ed_delete(dot1,dot2);
}
#endif /*FX_DELBLINES*/

#if FX_KLINE
/* EFUN: "Kill Line" */
f_kline()
{
	if(exp_p)
		e_goline(exp);		/* Move that many lines */
					/* (if 0, goes to BOL) */
	else				/* No arg, handle specially */
	  {	if(e_lblankp())		/* Is rest of line blank? */
			;		/* Yes, now at next line! */
		else e_goeol();		/* No, go to EOL rather than NL */
	  }
	ed_kill(cur_dot,e_dot());
	e_setcur();
	this_cmd = KILLCMD;
}
#endif /*FX_KLINE*/

#if FX_BKLINE
/* EFUN: "Backward Kill Line" (not EMACS) */
/*	Originally an Iconographics function.
*/
f_bkline()
{
	if(exp_p) exp = -exp;		/* If arg, invert it */
	else
	  {	exp = 0;		/* No arg, furnish 0 */
		exp_p = 1;
	  }
	f_kline();			/* Invoke "Kill Line" */
}
#endif /*FX_BKLINE*/

#if FX_GOLINE
/* EFUN: "Goto Line" (not EMACS) (GNU goto-line) */
f_goline()
{
        e_gobob();
        down_bline(exp-1);    /* already at line 1 */
}
#endif /*FX_GOLINE*/

down_bline(arg)
int arg;
{	
	if(arg)
		e_goline(arg);
	ed_setcur();
}

#if FX_DNRLINE || FX_UPRLINE
down_line (x)
int x;
{	register int i, res;

	res = x ? e_goline(x) : 1;	/* Move that many lines */
	goal = 0;
	if(res == 0)			/* Hit buffer limits (EOF)? */
	  {	if(x > 0)		/* Moving downwards? */
		  {
#if !(IMAGEN)		/* If IMAGEN, do not extend!! */
			if(x == 1) ed_crins();	/* Yeah, maybe extend */
			else
#endif
				goal = indtion(cur_dot);
			goto done;
		  }
	  }

	if(last_cmd == LINECMD		/* If previous cmd also a line move */
	  && pgoal != -1)		/* and we have a previous goal col */
		goal = pgoal;		/* then make it the current goal */
	else goal = indtion(cur_dot);	/* Else invent goal from current pos */

	i = inindex(e_dot(), goal);	/* See # chars needed to reach goal */
	if(i == -1)			/* If off edge of line, */
		e_goeol();		/* just move to end of this line */
	else e_igoff(i);		/* else move to goal. */

done:	pgoal = goal;
	this_cmd = LINECMD;
	ed_setcur();
}
#endif /*FX_DNRLINE || FX_UPRLINE*/



/* Region Handling functions */

/* EFUN: "Set/Pop Mark" */
f_setmark()
{
	mark_dot = e_dot();
	mark_p = 1;
	if(ev_markshow)			/* If have one, show indicator */
		saytoo(ev_markshow);	/* that mark was set. */
}

/* EFUN: "Exchange Point and Mark" */
f_exchmark()
{	chroff tmpdot;

	if(chkmark())
	  {	tmpdot = mark_dot;
		mark_dot = cur_dot;
		ed_go(tmpdot);		/* Set cur_dot and go there */
	  }
}

/* EFUN: "Kill Region" */
f_kregion()
{
	if(chkmark())
	  {	ed_kill(cur_dot,mark_dot); /* Will adj cur_dot, mark_dot */
		e_gocur();
		this_cmd = KILLCMD;
	  }
}

/* EFUN: "Copy Region" */
f_copreg()
{
	if(chkmark())
	  {	e_gocur();
		kill_push(e_copyn(mark_dot - cur_dot));
		e_gocur();
	  }
}


/* EFUN: "Uppercase Region" */
f_ucreg()
{	ef_creg(0);
}

/* EFUN: "Lowercase Region" */
f_lcreg()
{	ef_creg(1);
}

ef_creg(downp)
int downp;
{
	if(chkmark())
		ed_case(cur_dot,mark_dot,downp);
}

#if FX_FILLREG
/* EFUN: "Fill Region" */
f_fillreg()
{	if(chkmark())
		ed_fill(mark_dot,cur_dot,0);
}
#endif /*FX_FILLREG*/

/* CHKMARK() - minor utility for region-hacking functions.
 *	Returns TRUE if mark exists.
 *	Otherwise complains to user and returns 0.
 */
chkmark()
{	if(mark_p == 0)
		ding("No mark!");
	return(mark_p);
}

/* Paragraph functions */

#if FX_FPARA
/* EFUN: "Forward Paragraph" */
f_fpara()
{	int e_gobpa(), e_goepa();

	exp_do(e_goepa, e_gobpa);
	ed_setcur();
}
#endif /*FX_FPARA*/

#if FX_BPARA
/* EFUN: "Backward Paragraph" */
/*	Go to beginning of paragraph.
 *	Skip all whitespace until text seen, then stop at beginning of
 *	1st line starting with whitespace.
 */
f_bpara()
{	int e_gobpa(), e_goepa();

	exp_do(e_gobpa, e_goepa);
	ed_setcur();
}
#endif /*FX_BPARA*/

#if FX_MRKPARA
/* EFUN: "Mark Paragraph" */
f_mrkpara()
{
	f_fpara();		/* Go to end of paragraph */
	f_setmark();		/* Put mark there */
	f_bpara();		/* Then back to start of paragraph */
}
#endif /*FX_MRKPARA*/

#if FX_FILLPARA
/* EFUN: "Fill Paragraph" */
f_fillpara()
{
	chroff savloc, endloc;

	savloc = cur_dot;
#if ICONOGRAPHICS
        e_getc();			/* DON'T go to next para if at end */
        e_gobpa();			/* of this one!! */
#endif /*ICONOGRAPHICS*/
	e_goepa();			/* Go to end of parag */
	if(e_rgetc() != LF)		/* If last char EOL, back over it. */
		e_getc();
	endloc = e_dot();		/* Remember where end is */
	e_gobpa();			/* Go to beg of parag */
#if ICONOGRAPHICS
        kill_push(e_copyn(endloc - e_dot ()));
                                        /* put old version on kill ring */
#endif /*ICONOGRAPHICS*/
	e_fwsp();			/* Move over initial whitespace */
	ed_fill(e_dot(), endloc, 0);	/* Fill this region, return to dot */
}
#endif /*FX_FILLPARA*/
