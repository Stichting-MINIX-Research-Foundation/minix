/* ELLE - Copyright 1982, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*	EEEDIT - E-type routines */

#include "elle.h"

/* E_	- Operate on cur_buf.  Do not change value of cur_dot unless
 *		unavoidable side effect (also e_setcur).
 * EX_	- Like E_ but take SB ptr value.  Never touch cur_dot.
 * ED_	- Like E_, operate on cur_buf, update cur_dot and display stuff.
 * D_	- Perform necessary display update for given operations.
 *
 * Note that "dot" refers to the current read/write pointer for a sbbuffer.
 * The name comes from EMACS/TECO where "." represents this value.
 */

#define CURSBB (SBBUF *)cur_buf		/* Shorthand for current SB buffer */

e_reset()	/* Reset current buffer */
{	ex_reset(CURSBB);
	cur_dot = 0;
}

/* Basic functions - apply SB routines to current buffer.
 * There is some optimization here which knows that certain SB functions
 * are macros.
 */
e_rgetc()	/* Read/move 1 char backward */
{	return(sb_rgetc((CURSBB)));
}
e_rdelc()	/* Delete 1 char backward */
{	return(sb_rdelc((CURSBB)));
}
e_delc()	/* Delete 1 char forward */
{	return(sb_deln(CURSBB,(chroff)1));
}
e_getc()	/* Read/move 1 char forward */
{	register SBBUF *sb;
	sb = CURSBB;		/* Macro: use reg */
	return(sb_getc(sb));
}
e_backc()	/* Move 1 char backward */
{	register SBBUF *sb;
	sb = CURSBB;		/* Macro: use reg */
	sb_backc(sb);			/* No value returned */
}
e_putc(c)	/* Insert/write 1 char forward */
char c;
{	register SBBUF *sb;
	sb = CURSBB;		/* Macro: use reg */
	return(sb_putc(sb, c));
}
e_peekc()	/* Read 1 char forward (no move) */
{	register SBBUF *sb;
	sb = CURSBB;		/* Macro: use reg */
	return(sb_peekc(sb));
}
e_ovwc(ch)	/* Overwrite 1 char forward */
char ch;
{
	sb_setovw(CURSBB);	/* Turn on overwrite mode */
	e_putc(ch);
	sb_clrovw(CURSBB);	/* Turn off overwrite mode */
}

SBSTR *
e_copyn(off)	/* Copy N chars forward/backward, return SD to sbstring */
chroff off;
{	return(sb_cpyn(CURSBB,off));
}
e_deln(off)	/* Delete N chars forward/backward */
chroff off;
{	return(sb_deln(CURSBB, off));
}

/* E_SETCUR() - set cur_dot to current position (dot).  This is the only
 *	E_ routine that mungs cur_dot except for e_reset.
 */
e_setcur()
{	cur_dot = e_dot();
}
e_gosetcur(dot)		/* Go to specified dot and set cur_dot as well */
chroff dot;
{	sb_seek(CURSBB,dot,0);
	e_setcur();	/* Not cur_dot = dot since want canonicalization */
}

/* E_GO(dot) - Move to specified location. */
/* These "GO" routines all move to the location specified, returning
 *	0 if successful and -1 on error.  "cur_dot" is never changed,
 *	with the exception of e_gosetcur.
 * Note that other "GO" routines (eg E_GONL) will return 1 if successful
 *	and 0 if stopped by EOF.
 */

e_gocur() { return(e_go(cur_dot)); }		/* Move to cur_dot */
e_gobob() { return(e_go((chroff) 0)); }		/* Move to Beg Of Buffer */
e_goeob() { return(sb_seek(CURSBB,(chroff)0,2)); } /* Move to End Of Buffer */
e_go(dot)	/* Move to specified location. */
chroff dot;
{	return(sb_seek(CURSBB,dot,0));
}
e_igoff(ioff)	/* Move (int) N chars forward/backward */
int ioff;
{	return(sb_seek(CURSBB,(chroff)ioff,1));
}

e_goff(off)	/* Move (full) N chars forward/backward */
chroff off;
{	return(sb_seek(CURSBB,off,1));
}

int ex_gonl(), ex_gopl(), ex_gobol(), ex_goeol();

e_gobol() { return(ex_gobol(CURSBB)); }	/* Move to beg of this line */
e_goeol() { return(ex_goeol(CURSBB)); }	/* Move to end of this line */
e_gonl()  { return(ex_gonl(CURSBB)); }	/* Move to beg of next line */
e_gopl()  { return(ex_gopl(CURSBB)); }	/* Move to beg of prev line */


/* E_DOT() - Return current value of dot. */
chroff e_dot()   { return(sb_tell(CURSBB)); }		/* Current pos */
chroff e_nldot() { return(e_alldot(CURSBB,ex_gonl)); }	/* Beg of next line */
chroff e_pldot() { return(e_alldot(CURSBB,ex_gopl)); }	/* Beg of prev line */
chroff e_boldot(){ return(e_alldot(CURSBB,ex_gobol));}	/* Beg of this line */
chroff e_eoldot(){ return(e_alldot(CURSBB,ex_goeol));}	/* End of this line */

chroff
e_alldot(sbp,rtn)	/* Auxiliary for above stuff */
SBBUF *sbp;
int (*rtn)();
{	return(ex_alldot(sbp,rtn,e_dot()));
}

/* E_BLEN - Return length of current buffer */
chroff
e_blen() { return(ex_blen(CURSBB)); }

/* EX_ routines - similar to E_ but take a buffer/sbbuf argument
 *	instead of assuming current buffer.
 */

/* EX_RESET - Reset a given buffer */
ex_reset(b)
struct buffer *b;
{	sbs_del(sb_close((SBBUF *)b));
	sb_open((SBBUF *)b,(SBSTR *)0);
}

ex_go(sbp,loc)		/* Move to given dot in specified sbbuf */
chroff loc;
SBBUF *sbp;
{	return(sb_seek(sbp,loc,0));
}

chroff
ex_dot(sbp)	/* Return current position in specified sbbuf */
SBBUF *sbp;
{
	return(sb_tell(sbp));
}


chroff
ex_boldot(sbp,dot)	/* Return dot for BOL of specified sbbuf */
SBBUF *sbp;
chroff dot;
{	return(ex_alldot(sbp,ex_gobol,dot));
}

chroff
ex_alldot(sbp,rtn,dot)	/* Auxiliary for some e_ stuff */
SBBUF *sbp;
int (*rtn)();
chroff dot;
{	register SBBUF *sb;
	chroff savloc, retloc;

	savloc = sb_tell(sb = sbp);
	sb_seek(sb,dot,0);
	(*rtn)(sb);
	retloc = sb_tell(sb);
	sb_seek(sb,savloc,0);
	return(retloc);
}

/* GO (forward) to Next Line of specified sbbuf - returns 0 if stopped at EOF
 * before an EOL is seen. */
ex_gonl(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register int c;
	sb = sbp;
#if FX_EOLMODE
	if(eolcrlf(sb))
		while((c = sb_getc(sb)) != EOF)
		  {	if(c == LF)		/* Possible EOL? */
			  {	sb_backc(sb);	/* See if prev char was CR */
				if((c = sb_rgetc(sb)) != EOF)
					sb_getc(sb);
				sb_getc(sb);	/* Must restore position */
				if(c == CR)	/* Now test for CR */
					return(1);	/* Won, CR-LF! */
			  }
		  }
	else
#endif
	while((c = sb_getc(sb)) != EOF)
		if(c == LF)
			return(1);
	return(0);
}

/* GO (forward) to End Of Line of specified sbbuf - returns 0 if stopped at
 * EOF before an EOL is seen. */
ex_goeol(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register int c;
	sb = sbp;
#if FX_EOLMODE
	if(eolcrlf(sb))
		while((c = sb_getc(sb)) != EOF)
		  {	if(c == LF)		/* Possible EOL? */
			  {	sb_backc(sb);	/* See if prev char was CR */
				if((c = sb_rgetc(sb)) == CR)
					return(1);	/* Won, CR-LF! */
				if(c != EOF)	/* No, must restore position */
					sb_getc(sb);	/* Skip over */
				sb_getc(sb);		/* Skip over LF */
			  }
		  }
	else
#endif
	while((c = sb_getc(sb)) != EOF)
		if(c == LF)
		  {	sb_backc(sb);
			return(1);
		  }
	return(0);
}

/* GO (backward) to Beg Of Line of specified sbbuf - returns 0 if stopped
 * at EOF
 */
ex_gobol(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register int c;
	sb = sbp;
#if FX_EOLMODE
	if(eolcrlf(sb))
		while((c = sb_rgetc(sb)) != EOF)
		  {	if(c == LF)		/* Possible EOL? */
			  {	if((c = sb_rgetc(sb)) == CR)
				  {	sb_getc(sb);	/* Won, CR-LF! */
					sb_getc(sb);	/* Move back */
					return(1);
				  }
				if(c != EOF)	/* No, must restore position */
					sb_getc(sb);	/* Undo the rgetc */
			  }
		  }
	else
#endif
	while((c = sb_rgetc(sb)) != EOF)
		if(c == LF)
		  {	sb_getc(sb);
			return(1);
		  }
	return(0);
}

/* GO (backward) to Previous Line of specified sbbuf - returns 0 if stopped
 * at EOF before an EOL is seen (i.e. if already on 1st line of buffer)
 */
ex_gopl(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register int c;
	sb = sbp;
#if FX_EOLMODE
	if(eolcrlf(sb))
		while((c = sb_rgetc(sb)) != EOF)
		  {	if(c == LF)		/* Possible EOL? */
			  {	if((c = sb_rgetc(sb)) == CR)
				  {	ex_gobol(sb);
					return(1);		/* Won! */
				  }
				if(c != EOF)	/* No, must restore position */
					sb_getc(sb);	/* Undo the rgetc */
			  }
		  }
	else
#endif
	while((c = sb_rgetc(sb)) != EOF)
		if(c == LF)
		  {	ex_gobol(sb);
			return(1);	/* Won! */
		  }
	return(0);
}


chroff
ex_blen(sbp)		/* Return length of specified sbbuf */
SBBUF *sbp;
{
	return(sb_tell(sbp)+sb_ztell(sbp));
}

/* Miscellaneous stuff */

/* E_GOFWSP() - Forward over whitespace */
e_gofwsp()
{	register int c;
	while((c = e_getc()) == SP || c == TAB);
	if(c != EOF) e_backc();
}

/* E_GOBWSP() - Backward over whitespace */
e_gobwsp()
{	register int c;
	while((c = e_rgetc()) == SP || c == TAB);
	if(c != EOF) e_getc();
}


/* E_GOLINE(n) - Goes N lines forward (or backward).
**	If N == 0, goes to beginning of current line.
**	Returns 0 if hit EOF.
*/
e_goline(i)
register int i;
{
	if(i > 0)
	  {	do { if(!e_gonl()) return(0); }
		while(--i);
	  }
	else if(i < 0)
	  {	i = -i;
		do { if(!e_gopl()) return(0); }
		while(--i);
	  }
	else e_gobol();		/* arg of 0 */
	return 1;
}

/* E_LBLANKP() - Returns true if all characters in rest of line are blank.
 *	Moves to beginning of next line as side effect, unless fails.
 */
e_lblankp()
{	register int c;
	for(;;) switch(e_getc())
	  {
		case SP:
		case TAB:
			continue;
		case LF:	/* Normally drop thru to return 1 as for EOF */
#if FX_EOLMODE
			if(eolcrlf(cur_buf))
			  {	e_rgetc();
				if((c = e_rgetc()) != EOF) /* Get prev char */
					e_getc();
				e_getc();
				if(c != CR)		/* Now examine */
					continue;	/* Not CR-LF, go on */
			  }	/* Else drop thru to return win */
#endif
		case EOF:
			return(1);
		default:
			return(0);
	  }
	/* Never drops out */
}


e_insn(ch, cnt)
int ch;
int cnt;
{	register int i;
	if((i = cnt) > 0)
		do { e_putc(ch);
		  } while(--i);
}

e_sputz(acp)
char *acp;
{	register SBBUF *sb;
	register char *cp;
	register int c;
	if(cp = acp)
	  {	sb = CURSBB;
		while(c = *cp++)
			sb_putc(sb,c);
	  }
}

/* BOLEQ - Returns TRUE if 2 dots are on the same line
 *	(i.e. have the same Beg-Of-Line)
 */
boleq(dot1,dot2)
chroff dot1,dot2;
{	return( (ex_boldot(CURSBB,dot1) == ex_boldot(CURSBB,dot2)));
}


char *
dottoa(str,val)
char *str;
chroff val;
{	register char *s;

	s = str;
	if(val < 0)
	  {	*s++ = '-';
		val = -val;
	  }
	if(val >= 10)
		s = dottoa(s, val/10);
	*s++ = '0' + (int)(val%10);
	*s = 0;
	return(s);
}


/* Paragraph utilities */

#if FX_FPARA || FX_BPARA || FX_MRKPARA || FX_FILLPARA

#if FX_SFPREF
extern char *fill_prefix;	/* Defined in eefill.c for now */
extern int fill_plen;		/* Ditto */
#endif /*FX_SFPREF*/

#if ICONOGRAPHICS
int para_mode = PARABLOCK;	/* eexcmd.c only other file that refs this */
#endif /*ICONOGRAPHICS*/


/* Go to beginning of paragraph */
e_gobpa()
{	register int c;
	chroff savdot;

	savdot = e_dot();
	e_bwsp();
	while((c = e_rgetc()) != EOF)
		if(c == LF)		/* Went past line? */
		  {	e_getc();		/* Back up and check */
#if FX_SFPREF
			if(fill_plen)
				if(tstfillp(fill_plen))
				  {	e_igoff(-(fill_plen+1));
					continue;
				  }
				else break;
#endif /*FX_SFPREF*/
#if ICONOGRAPHICS
                        c = e_peekc ();

                        if (para_mode == PARABLOCK)
                            if (c == LF)
                                break;

                        if (para_mode == PARALINE)
                            if (c_wsp (c))
                                break;
#else
			if(c_pwsp(e_peekc()))	/* Check 1st chr for wsp */
				break;		/* If wsp, done */
#endif /*ICONOGRAPHICS*/
			e_rgetc();		/* Nope, continue */
		  }
	if((c = e_peekc()) == '.' || c == '-')
	  {	e_gonl();
		if(e_dot() >= savdot)
			e_gopl();
	  }
}

/* Go to end of paragraph */
e_goepa()
{	register int c;

	e_gobol();			/* First go to beg of cur line */
	e_fwsp();
	while((c = e_getc()) != EOF)
        	if (c == LF)
		  {
#if FX_SFPREF
		if(fill_plen)		/* If Fill Prefix is defined */
			if(tstfillp(fill_plen))	/* then must start with it */
				continue;
			else break;		/* or else we're done */
#endif /*FX_SFPREF*/
#if ICONOGRAPHICS
                if (para_mode == PARABLOCK)
                    if (e_peekc () == LF)
                        break;

                if (para_mode == PARALINE)
                    if (c_wsp (e_peekc ()))
                        break;
#else
                if(c_pwsp(e_peekc()))
                        break;
#endif /*-ICONOGRAPHICS*/
		  }
}

exp_do(rpos, rneg)
int (*rpos)(), (*rneg)();
{	register int e;
	register int (*rtn)();

	if((e = exp) == 0)
		return;
	rtn = rpos;
	if(e < 0)
	  {	rtn = rneg;
		e = -e;
	  }
	do { (*rtn)();
	  } while(--e);
}


e_fwsp()
{	register int c;
	while(c_wsp(c = e_getc()));
	if(c != EOF) e_backc();
}
e_bwsp()
{	register int c;
	while(c_wsp(c = e_rgetc()));
	if(c != EOF) e_getc();
}


c_wsp(ch)
int ch;
{	register int c;
	c = ch;
	if(c == SP || c == TAB || c == LF || c == FF)
		return(1);
	return(0);
}
c_pwsp(ch)
int ch;
{	register int c;
	c = ch;
	if(c == '.' || c == '-')
		return(1);
	return(c_wsp(c));
}

#endif /* FX_FPARA || FX_BPARA || FX_MRKPARA || FX_FILLPARA */

/* Word function auxiliaries */

/* Returns true if this character is a delimiter. */
delimp(c)
int  c;
{	static int  delim_tab[] =
	  {
	    0177777, 0177777,		/* All controls */
	    0177777, 0176000,		/* All punct but 0-9 */
	    0000001, 0074000,		/* All punct but A-Z and _ */
	    0000001, 0174000		/* All punct but a-z */
	  };
	return (delim_tab[c >> 4] & (1 << (c & 017)));
}

e_wding(adot,n)
register chroff *adot;
int n;
{	chroff savdot;
	savdot = e_dot();
	e_gowd(n);
	*adot = e_dot();
	e_go(savdot);
	if(*adot == savdot)
	  {	ring_bell();
		return(0);
	  }
	return(1);
}
chroff
e_wdot(dot,n)
chroff dot;
int n;
{	chroff savdot, retdot;
	savdot = e_dot();
	e_go(dot);
	e_gowd(n);
	retdot = e_dot();
	e_go(savdot);
	return(retdot);
}
e_gowd(n)
int n;
{	register int (*gch)(), c, cnt;
	int e_getc(), e_rgetc();
	chroff ret_dot;

	if((cnt = n) == 0)
		return;
	if(cnt > 0)
		gch = e_getc;		/* Forward routine */
	else
	  {	gch = e_rgetc;		/* Backward routine */
		cnt = -cnt;
	  }
	do
	  {	ret_dot = e_dot();	/* Remember dot for last word found */
		while((c = (*gch)()) != EOF && delimp(c));
		if(c == EOF)
		  {	e_go(ret_dot);		/* Use last word found */
			break;
		  }
		while((c = (*gch)()) != EOF && !delimp(c));
		if(c == EOF)
			break;
		if(n < 0) e_getc(); else e_backc();
	  } while(--cnt);
}

/* Searching */

e_search(mstr,mlen,backwards)
char *mstr;
int mlen;
int backwards;
{	register SBBUF *sb;
	register char *cp;
	register int c;
	char *savcp;
	int cnt, scnt;
#if IMAGEN
	register int c1;
	register int caseless = (cur_buf->b_flags & B_TEXTMODE);
#endif /*IMAGEN*/

	sb = (SBBUF *) cur_buf;
	if (!backwards)
	  {		/* Search forwards */
	sfwd:	cp = mstr;
		while((c = sb_getc(sb)) != EOF)
		  {
#if IMAGEN
			if((!caseless && c != *cp) || 
			    (caseless && upcase(c) != upcase(*cp))) continue;
#else
			if(c != *cp) continue;
#endif /*-IMAGEN*/
			cp++;
			cnt = mlen;
			while(--cnt > 0)
			  {
#if IMAGEN
				c1 = *cp++;
				c = e_getc();
				if ((!caseless && c1 != c) ||
				     (caseless && upcase(c1) != upcase(c)))
#else
				if(*cp++ != (c = e_getc()))
#endif /*-IMAGEN*/
				  {	if(c == EOF) return(0);
					sb_seek(sb,(chroff)(cnt-mlen),1);
					goto sfwd;
				  }
			  }
			return(1);
		  }
	  }
	else
	  {		/* Search backwards */
		scnt = mlen - 1;
		savcp = mstr + scnt;		/* Point to end of string */

	sbck:	cp = savcp;
		while((c = sb_rgetc(sb)) != EOF)
		  {
#if IMAGEN
			if((!caseless && c != *cp) || 
			    (caseless && upcase(c) != upcase(*cp))) continue;
#else
			if(c != *cp) continue;
#endif /*-IMAGEN*/
			cp--;
			if((cnt = scnt) == 0) return(1);
			do
			  {
#if IMAGEN
				c1 = *cp--;
				c = e_rgetc();
				if ((!caseless && c1 != c) ||
				     (caseless && upcase(c1) != upcase(c)))
#else
				if(*cp-- != (c = e_rgetc()))
#endif /*-IMAGEN*/
				  {	if(c == EOF) return(0);
					sb_seek(sb,(chroff)(mlen-cnt),1);
					goto sbck;
				  }
			  }
			while(--cnt);
			return(1);
		  }
	  }
	return(0);		/* Failed */
}
