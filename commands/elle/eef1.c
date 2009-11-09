/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EEF1	Various functions
 *		Char move/ins/del
 *		Case change
 *		Char/word transpose
 */

#include "elle.h"

/* EFUN: "Insert Self" */
f_insself (c)
int c;
{
#if IMAGEN
	fim_insself(c);
#else
#if FX_FILLMODE
	extern int fill_mode;

	if(fill_mode) fx_insfill(c);
	else
#endif /*FX_FILLMODE*/
	ed_insn(c, exp);	/* Normal stuff */
#endif /*-IMAGEN*/
}

/* EFUN: "Quoted Insert"
**	Inserts next char directly, <exp> number of times.
** Does not check anything about the char and does not do anything
** depending on the mode.  In particular, CR is not turned into EOL.
*/
f_quotins()
{
	ed_insn(cmd_read(), exp);	/* Insert next char directly */
}

#if FX_CRLF
/* EFUN: "CRLF" */
f_crlf()
{
#if IMAGEN
	fim_crlf();
#else
	register int i;

	if(e_goeol() == cur_dot		/* If at end of current line */
	  && exp == 1			/* and inserting only 1 new line */
	  && e_lblankp() && e_lblankp())	/* and next 2 lines blank */
	  {	e_gocur();		/* Then just re-use next line. */
		e_gonl();		/* Go to its start */
		e_setcur();		/* and establish cur_dot there. */
		ed_delete(e_dot(), e_eoldot());	/* Ensure any blanks flushed */
	  }
	else
	  {	e_gocur();		/* Otherwise back to original place */
		if((i = exp) > 0)	/* and simply insert newlines */
			do ed_crins();
			while(--i);
	  }
#endif /*-IMAGEN*/
}
#endif /*FX_CRLF*/

/* EFUN: "Forward Character" */
f_fchar()
{	ed_igoff(exp);
}

/* EFUN: "Backward Character" */
f_bchar()
{	ed_igoff(-exp);
}

/* EFUN: "Delete Character" */
f_dchar ()
{
#if IMAGEN
	fim_dchar();
#else
	ef_deln(exp);
#endif /*-IMAGEN*/
}

/* EFUN: "Backward Delete Character" */
f_bdchar ()
{
#if IMAGEN
	fim_bdchar();
#else
	ef_deln(-exp);
#endif /*-IMAGEN*/
}

/* Delete forward or backward N characters.
 * If arg, kills instead of deleting.
 */
ef_deln(x)
int x;
{
	e_igoff(x);
	if(exp_p) ed_kill(cur_dot, e_dot());
	else ed_delete(cur_dot, e_dot());
}

#if FX_DELSPC
/* EFUN: "Delete Horizontal Space" */
/*	Delete spaces/tabs around point.
 */
f_delspc()
{	chroff dot1;

	e_gobwsp();			/* Move backward over whitespace */
	dot1 = e_dot();			/* Save point */
	e_gofwsp();			/* Move forward over whitespace */
	ed_delete(dot1,e_dot());	/* Delete twixt start and here */
}
#endif /*FX_DELSPC*/

#if FX_TCHARS
/* EFUN: "Transpose Characters"
 *	Transpose chars before and after cursor.  Doesn't hack args yet.
 * EMACS: With positive arg, exchs chars before & after cursor, moves right,
 *	and repeats the specified # of times, dragging the char to the
 *	left of the cursor right.
 *	With negative arg, transposes 2 chars to left of cursor, moves
 *	between them, and repeats the specified # of times, exactly undoing
 *	the positive arg form.  With zero arg, transposes chars at point
 *	and mark.
 *	HOWEVER: at the end of a line, with no arg, the preceding 2 chars
 *	are transposed.
 */
f_tchars()
{	register int c, c2;
#if IMAGEN
	c = e_rgetc();			/* Gosmacs style: twiddle prev 2 */
	if (c == EOF)
		return(e_gocur());	/* Do nothing at beginning of bfr */
#else

	if((c = e_getc()) == EOF	/* If at EOF */
	  || e_rgetc() == LF)		/* or at end of line, */
		c = e_rgetc();		/* use preceding 2 chars */
#endif /*-IMAGEN*/

	if((c2 = e_rgetc()) == EOF)	/* At beginning of buffer? */
		return(e_gocur());	/* Yes, do nothing */
	e_ovwc(c);
	e_ovwc(c2);
	e_setcur();
	buf_tmod((chroff)-2);		/* Munged these 2 chars */
}
#endif /*FX_TCHARS*/

#if FX_FWORD
/* EFUN: "Forward Word" */
f_fword()
{	chroff retdot;
	if(e_wding(&retdot, exp))
		ed_go(retdot);
}
#endif

#if FX_BWORD
/* EFUN: "Backward Word" */
f_bword()
{	exp = -exp;
	f_fword();
}
#endif

#if FX_KWORD
/* EFUN: "Kill Word" */
f_kword()
{	chroff retdot;

	if(e_wding(&retdot,exp))
	  {	ed_kill(cur_dot,retdot);
		this_cmd = KILLCMD;
	  }
}
#endif

#if FX_BKWORD
/* EFUN: "Backward Kill Word" */
f_bkword()
{	exp = -exp;
	f_kword();
}
#endif

#if FX_TWORDS
/* EFUN: "Transpose Words" */
/*	Transpose word.  Urk!
 */
f_twords()
{	register SBSTR *sd1, *sd2;
	register SBBUF *sb;
	chroff begwd1, endwd1, begwd2, endwd2;

	endwd2 = e_wdot(cur_dot, 1);	/* Go to end of 2nd word */
	begwd2 = e_wdot(endwd2, -1);	/* Go to beg of 2nd word */
	if(begwd2 >= endwd2)		/* If no 2nd word, punt. */
		return;
	begwd1 = e_wdot(begwd2, -1);	/* Go to beg of 1st word */
	endwd1 = e_wdot(begwd1, 1);	/* Go to end of 1st word */
	if(begwd1 >= endwd1)		/* If no 1st word, punt. */
		return;
	if(endwd1 > begwd2)		/* Avoid possible overlap */
		return;

	e_go(begwd2);
	sb = (SBBUF *)cur_buf;
	sd2 = sb_killn(sb, endwd2 - begwd2);	/* Excise wd 2 first */
	e_go(begwd1);
	sd1 = sb_killn(sb, endwd1 - begwd1);	/* Excise wd 1 */
	sb_sins(sb, sd2);			/* Replace with wd 2 */
	e_goff(begwd2 - endwd1);		/* Move past between stuff */
	sb_sins(sb, sd1);			/* Insert wd 1 */
	e_setcur();
	buf_tmat(begwd1);			/* Modified this range */
}
#endif /*FX_TWORDS*/

/* Case hacking functions and support */

#if FX_UCWORD
/* EFUN: "Uppercase Word" */
f_ucword()
{	case_word(0);
}
#endif /*FX_UCWORD*/

#if FX_LCWORD
/* EFUN: "Lowercase Word" */
f_lcword()
{	case_word(1);
}
#endif /*FX_LCWORD*/

#if FX_UCIWORD
/* EFUN: "Uppercase Initial" */
f_uciword()
{	case_word(2);
}
#endif /*FX_UCIWORD*/

#if FX_UCWORD||FX_LCWORD||FX_UCIWORD
case_word (downp)
{	chroff retdot;
#if IMAGEN
	chroff startdot;

	/* Normalize our position to beginning of "current" word,
	 * where "current" is defined to be the current word we are in,
	 * or else the previous word if we are not in any word.
	 * All this silly nonsense just to perpetuate Gosmacs's
	 * wrong behaviour!
	 */
	startdot = cur_dot;	/* Save current position */
	e_getc();	/* If at beg of word, ensure we get inside it */
	e_gowd(-1);	/* Go to start of this or prev word */
	e_setcur();	/* Set cur_dot */
#endif /*IMAGEN*/

	if(e_wding(&retdot, exp))
	  {	ed_case(cur_dot,retdot,downp);
		e_gosetcur(retdot);
	  }
#if IMAGEN
	e_gosetcur(startdot);
#endif /*IMAGEN*/
}
#endif /* any case_word caller */
