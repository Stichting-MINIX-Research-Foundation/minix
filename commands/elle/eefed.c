/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*	EEFED - ED-type functions
 */
#include "elle.h"

/*
 * ED_INSERT -- Insert character given as argument.
 */

ed_insert(c)
int c;
{	register SBBUF *sb;

	sb = (SBBUF *) cur_buf;		/* For a little speed */
	sb_putc(sb,c);		/* Insert the char */
	cur_dot++;		/* Advance dot */
	buf_tmod((chroff)-1);	/* Mark buffer modified, for redisplay etc. */
				/* Perhaps later use specialized routine? */
}

ed_insn(ch, cnt)
int ch, cnt;
{	register int i;
	if((i = cnt) > 0)
		do { ed_insert(ch);
		  } while(--i);
}

ed_crins()
{
#if FX_EOLMODE
	if (eolcrlf(cur_buf))	/* If EOL is made of CR-LF */
		ed_insert(CR);	/* then first insert CR, then drop down to */
#endif
	ed_insert(LF);		/* Insert LF */
}


ed_sins (s)			       /* insert this string */
register char *s;
{	register c;
	while (c = *s++)
		ed_insert (c);
}

ed_nsins (s, i)		/* Insert string of N chars */
register char *s;
register int i;
{	if(i > 0)
		do { ed_insert(*s++); } while(--i);
}

/* ED_INDTO(col) - Indent to specified column.
**	Finds current cursor position, and inserts tabs and spaces
** so cursor ends up at column goal.  Does nothing if already at or past
** specified column.
*/

ed_indto(goal)
register int goal;
{	register int ng;

	ng = goal & ~07;		/* Get distance to tab stop */
	ed_insn(TAB, ((ng - (d_curind() & ~07)) >> 3));
	ed_insn(SP, goal-ng);
}

/* Oddball routine - Set cur_dot to actual I/O location and
 * tell display that cursor probably moved.  This is not really a
 * function of itself; it provides support for real functions.
 */
ed_setcur()
{	e_setcur();	/* Set cur_dot */
	redp(RD_MOVE);	/* Alert redisplay to check cursor loc */
}

/* Go to given dot */
ed_go (dot)
chroff dot;
{	e_go(dot);
	ed_setcur();
}

/* Go to given offset from current location */
ed_goff(off)
chroff off;
{	e_goff(off);
	ed_setcur();
}

/* Go to given INTEGER offset from current location */
ed_igoff(ioff)
int ioff;
{	e_igoff(ioff);
	ed_setcur();
}

/* Reset (delete all of) Buffer
 * Should buffer be marked modified or not? Currently isn't.
 */
ed_reset()
{	if(e_blen() == 0)
		return;		/* Already empty */
	e_reset();
	cur_dot = 0;
	cur_win->w_topldot = 0;	/* Is this necessary? */
#if IMAGEN
	redp(RD_WINRES|RD_REDO);
#else
	redp(RD_WINRES);	/* This window needs complete update */
#endif /*-IMAGEN*/

/*	buf_mod(); */		/* Mark modified ?? */
/*	mark_p = 0; */		/* Say no mark set ?? */
}

ed_deln(off)
chroff off;
{	chroff dot;
	dot = e_dot();
	e_goff(off);	
	ed_delete(e_dot(), dot);
}

/* ED_DELETE(dot1,dot2) -  Delete all characters between the two
 *	positions indicated by dot1 and dot2.  Their order does not
 *	matter; cur_dot and mark_dot are updated as necessary.
 */
ed_delete(dot1,dot2)
chroff dot1,dot2;
{	chroff tmpdot, savdot;

	if(dot1 > dot2)
	  {	tmpdot = dot1;
		dot1 = dot2;
		dot2 = tmpdot;
	  }
	e_go(dot1);
	tmpdot = dot2-dot1;
	sb_deln((SBBUF *)cur_buf,tmpdot);

	savdot = cur_dot;		/* Save cur_dot value */
	cur_dot = dot1;			/* so can set up for */
	buf_tmod((chroff)0);		/* call to update disp-change vars */
	cur_dot = savdot;

	if(cur_dot >= dot2)
		cur_dot -= tmpdot;
	else if(cur_dot > dot1)
		cur_dot = dot1;
	if(mark_dot >= dot2)
		mark_dot -= tmpdot;
	else if(mark_dot > dot1)
		mark_dot = dot1;
	e_gocur();
}

/* ED_KILL(dot1,dot2) - Kill (save and delete) text between two places in
 *	the buffer.
 * We assume we are deleting from dot1 to dot2, thus if dot1 > dot2
 * then backwards deletion is implied, and the saved text is prefixed
 * (instead of appended) to any previously killed text.
 */
ed_kill(dot1,dot2)
chroff dot1,dot2;
{	register SBSTR *sd, *sdo;
	SBSTR *e_copyn();

	e_go(dot1);
	sd = e_copyn(dot2-dot1);
	if(sd == 0) return;
	if(last_cmd == KILLCMD && (sdo = kill_ring[kill_ptr]))
	  {	if(dot1 > dot2)	/* Prefix new killed stuff onto old stuff */
		  {	sbs_app(sd,sdo);
			kill_ring[kill_ptr] = sd;
		  }
		else		/* Append new stuff to old stuff */
			sbs_app(sdo,sd);
	  }
	else kill_push(sd);
	ed_delete(dot1,dot2);
}

kill_push(sdp)
SBSTR *sdp;
{	register SBSTR *sd;

	if(++kill_ptr >= KILL_LEN) kill_ptr = 0;
	if(sd = kill_ring[kill_ptr])
		sbs_del(sd);
	kill_ring[kill_ptr] = sdp;
}


#define isupper(c) (('A' <= c) && (c <= 'Z'))
#define islower(c) (('a' <= c) && (c <= 'z'))
#define toupper(c) (c + ('A' - 'a'))
#define tolower(c) (c + ('a' - 'A'))

#if FX_UCWORD||FX_LCWORD||FX_UCIWORD||FX_UCREG||FX_LCREG

/* ED_CASE(dot1,dot2,downp) - Change the case within a region.
 *	downp = 0 for uppercase, 1 for lowercase, 2 for capitalize.
 */
ed_case(dot1, dot2, downp)
chroff dot1, dot2;
int downp;
{	chroff dcnt;
	register int c, a, z;
	int modflg;

	modflg = 0;
	if((dcnt = dot2 - dot1) < 0)
	  {	dcnt = dot1;
		dot1 = dot2;
		dot2 = dcnt;
		dcnt -= dot1;
	  }
	e_go(dot1);

	if(downp==2)
	  {	a = 0;	/* 0 looking for wd, 1 in word */
		while(--dcnt >= 0)
		  {	if(delimp(c = e_getc()))	/* Char in wd? */
			  {	a = 0;			/* No */
				continue;
			  }
			 if(a)		/* If already inside word */
			  {	if(isupper(c))
					c = tolower(c);
				else continue;
			  }
			else	/* If encountered start of word */
			  {	a = 1;
				if(islower(c))
					c = toupper(c);
				else continue;
			  }
			e_backc();
			e_ovwc(c);
			modflg++;
		  }
		goto casdon;
	  }
	if(downp==0)
	  {	a = 'a';		/* Convert to lower case */
		z = 'z';
		downp = -040;
	  }
	else
	  {	a = 'A';		/* Convert to upper case */
		z = 'Z';
		downp = 040;
	  }
	while(--dcnt >= 0)
	  {	if(a <= (c = e_getc()) && c <= z)
		  {	e_backc();
			e_ovwc(c+downp);
			modflg++;
		  }
	  }

casdon:	dot2 = cur_dot;			/* Save dot */
	e_setcur();			/* Set up for modification range chk*/
	if(modflg)
		buf_tmat(dot1);		/* Stuff munged from there to here */
	ed_go(dot2);
}
#endif /* any ed_case caller */


/* UPCASE(c) - Return upper-case version of character */
upcase(ch)
int ch;
{	register int c;
	c = ch&0177;
	return(islower(c) ? toupper(c) : c);
}

