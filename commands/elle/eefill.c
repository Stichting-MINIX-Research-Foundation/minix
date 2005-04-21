/* ELLE - Copyright 1982, 1985, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EEFILL	Fill Mode functions
 */

#include "elle.h"

extern int ev_fcolumn;	/* Fill Column variable (defined in EEVINI) */
#if FX_SFPREF
char *fill_prefix;	/* Fill Prefix variable */
int fill_plen;		/* Length of Fill Prefix (0 = no prefix) */
#endif /*FX_SFPREF*/

#if FX_FILLMODE
int fill_mode = 0;	/* TRUE when Auto Fill Mode is on */
int *fill_trig;		/* Pointer to fill-trigger chbit array */
static char *fill_initrig = " \t.,;:)!";
#endif /*FX_FILLMODE*/

/* Following stuff for testing routines on */
/*

          1         2         3         4         5	    6         7
0123456789012345678901234567890123456789012345678901234567890123456789012345

Okay...  more stuff to hack.  Okay.  a b c d e f g h i j k l m
n o p q r s t u v w x y z dfsd stuff to hack 01234 Okay testing
more stuff to hack.  Okay...  more stuff to hack more stuff to
hack.  Okay...  more stuff to line long stuff to hack.  Okay...
even more gap and.  period.  okay, end of stuff.
	This is another fence.
*/


#if FX_SFCOL
/* EFUN: "Set Fill Column" */
f_sfcol()
{	register int linel;
	char temp[20];

	linel = exp_p ? exp : d_curind();
	if(linel < 0) linel = 0;
	say("Fill column = ");
	dottoa(temp,(chroff)linel);
	saytoo(temp);
	ev_fcolumn = linel;
}
#endif /*FX_SFCOL*/


#if FX_SFPREF
/* EFUN: "Set Fill Prefix" */
f_sfpref()
{	register int i;
	register char *cp;

	if((i = cur_dot - e_boldot()) > MAXLINE)
	  {	ding("Absurd Fill Prefix");
		return;
	  }
	if(fill_prefix)
	  {	chkfree(fill_prefix);
		fill_plen = 0;
	  }
	if(i <= 0)
	  {	fill_prefix = 0;
		cp = "";
	  }
	else
	  {	fill_prefix = cp = memalloc((SBMO)(i+1));
		fill_plen = i;
		e_gobol();
		do { *cp++ = e_getc(); }
		while(--i);
		*cp = 0;
		cp = fill_prefix;
	  }
	say("Fill Prefix = \"");
	saytoo(cp);
	saytoo("\"");
}


/* TSTFILLP(lim) - Check for existence of Fill Prefix at current dot.  If
 *	not there, returns 0 without changing dot.  If there, returns
 *	1 and leaves dot immediately after the Fill Prefix.
 *	Lim = # of chars allowed to scan from buffer.
 */
tstfillp(lim)
int lim;
{	register int i, c;
	register char *cp;
	chroff savdot;

	if(!(i = fill_plen) || (i > lim))
		return(0);
	savdot = e_dot();
	cp = fill_prefix;
	do {	if(*cp++ != e_getc())
		  {	e_go(savdot);
			return(0);
		  }
	  } while(--i);
	return(1);
}
#endif /*FX_SFPREF*/

#if FX_FILLREG || FX_FILLPARA

/* ED_FILL(start, end, flag) - Fill a region.
 *	Flag	0 for full filling; extra whitespace is flushed.  First
 *			word is always retained.
 *		1 for skimpy filling such as Auto-Fill likes.
 *			Extra whitespace is NOT flushed, except at
 *			beginning of a newly created line.
 *			This is not yet implemented however.
 * Note: updates cur_dot to compensate for changes in buffer, and returns
 *	there when done!
 * Note: Checks for Fill Prefix when it exists.
 */
ed_fill(begloc, endloc, flag)
chroff begloc, endloc;
int flag;
{	register int c;
	register int len, lastc;
	chroff savloc;
	int lastbrk;
	int parlen;

	parlen = endloc - begloc;
	if(parlen < 0)
	  {	begloc = endloc;
		parlen = -parlen;
	  }
	e_go(begloc);
	len = d_curind();		/* Set up current col */

#if FX_SFPREF
	/* If at beg of line, check for fill prefix and skip over it */
	if((len == 0) && tstfillp(parlen))
	  {	parlen -= fill_plen;
		len = d_curind();
	  }
#endif /*FX_SFPREF*/
	lastbrk = 0;			/* Put next word on no matter what. */
	c = 0;
	for(;;)
	  {
#if ICONOGRAPHICS
             if (c != ')' && c != '"')  /* allow for two sp after .) or ." */
#endif /*ICONOGRAPHICS*/
		lastc = c;
		if(--parlen < 0) break;
		c = e_getc();
		if(c == EOF)
			break;
#if FX_SFPREF
		/* If at beg of line, check for fill prefix and flush it */
		if((c == LF) && tstfillp(parlen))
		  {	e_igoff(-(fill_plen+1));
			e_ovwc(c = SP);
			e_deln((chroff)fill_plen);
			parlen -= fill_plen;
			if(cur_dot >= e_dot())
				cur_dot -= fill_plen;
		  }
#endif /*FX_SFPREF*/
		if(c == TAB || c == LF)		/* Replace tabs+eols by sps */
		  {	e_backc();		/* Back up 1 */
			e_ovwc(c = SP);
		  }
		if(c == SP)
		  {	if(lastc == SP)
			  {	e_rdelc();
				if(cur_dot > e_dot()) --cur_dot;
				continue;
			  }
			lastbrk = len;
			if(lastc == '.' || lastc == '!' || lastc == '?'
#if ICONOGRAPHICS
                                                        || lastc == ':'
#endif /*ICONOGRAPHICS*/
									)
			  {	if(--parlen < 0) goto done;
				if((c = e_getc()) == EOF)
					goto done;
				len++;
				if(c != SP)
				  {	e_backc();
					e_putc(c = SP);
					if(cur_dot >= e_dot()) ++cur_dot;
				  }
			  }
		  }
#if ICONOGRAPHICS
		if (c == BS)                    /* adjust for backspaces */
			if ((len -= 2) < 0) len = 0;
#endif /*ICONOGRAPHICS*/
		/* Normal char */
		if(++len > ev_fcolumn && lastbrk)	/* If went too far */
		  {	c = lastbrk - len;	/* Must put EOL at last SP */
			e_igoff(c);
			parlen -= c;	/* C is negative, actually adding */
			parlen--;
			e_ovwc(LF);
			lastbrk = 0;
			len = 0;
			c = SP;		/* Pretend this char was space */
#if FX_SFPREF
			if(fill_plen)
			  {	if(cur_dot >= e_dot())
					cur_dot += fill_plen;
				/* Better hope no nulls in prefix! */
				e_sputz(fill_prefix);
				len = d_curind();
			  }
#endif /*FX_SFPREF*/
		  }
	  }
done:	savloc = cur_dot;
	e_setcur();	/* Reached paragraph end, set cur_dot temporarily */
	buf_tmod(begloc-cur_dot);	/* So that proper range is marked */
	e_gosetcur(savloc);		/* Then restore original cur_dot */
}
#endif /*FX_FILLREG || FX_FILLPARA*/

#if FX_FILLMODE

/* EFUN: "Auto Fill Mode" */
/*	Toggles Auto Fill Mode (a minor mode). */
f_fillmode()
{	register char *cp;
	int *chballoc();

	fill_mode = fill_mode ? 0 : 1;
	if(!fill_trig)
	  {	fill_trig = chballoc(128);
		for(cp = fill_initrig; *cp; ++cp)
			chbis(fill_trig, *cp);
	  }
	redp(RD_MODE);
}

/* Called by F_INSSELF to handle char insertion in Auto Fill mode */
fx_insfill(c)
int c;
{
	ed_insn(c,exp);
	if(chbit(fill_trig, c))
	  {	fill_cur_line();

	  }
}


fill_cur_line()
{
	register int foundit, i;
	chroff lastbrkdot, boldot, eoldot;

	boldot = e_boldot();

	/* First back up to find place to make first break. */
	e_bwsp();
	lastbrkdot = e_dot();
	foundit = 0;
	for(foundit = 0; foundit >= 0;)
	  {	if((i = d_curind()) <= ev_fcolumn)
		  {	if(foundit)
				foundit = -1;
			else break;
		  }
		else ++foundit;
		while (!c_wsp (e_rgetc ())) ;
		e_bwsp();
		lastbrkdot = e_dot();
		if(lastbrkdot <= boldot)
		  {	lastbrkdot = boldot;
			break;
		  }
	  }

	if(foundit)
		ed_fill(lastbrkdot, e_eoldot(), 1);
}
#endif /*FX_FILLMODE*/

#if IMAGEN

#if FX_TEXTMODE
/* EFUN: "Text Mode Toggle" (not EMACS) */
f_textmode()
{
	cur_buf->b_flags ^= B_TEXTMODE;
	redp(RD_MODE);
}
#endif /*FX_TEXTMODE*/

int curr_indent = -1;		/* Current indent (for text mode autowrap) */
				/*  (misnomered: actually current column) */
chroff best_break;		/* Best break point so far */


/* Fill-mode version of "Insert Self" */

fim_insself(c)
int c;
{
	register int ind, flags = cur_buf->b_flags;

	/* In Text mode, auto-wrap happens at spaces after fill column */
	if (c == SP && flags & B_TEXTMODE && exp == 1 && magic_wrap(c))
		return;

	/* In C-mode, tab stops are every 4 columns */
	else if (c == TAB && flags & B_CMODE &&
			(ind = magic_backto_bol()) >= 0)
		ed_indto((ind + 4) & ~3);
	else
	  {	ed_insn(c, exp);

		/* Keep track of indent, once we have a grip on it */
		if (last_cmd == INSCMD && curr_indent != -1)
		  {	this_cmd = INSCMD;  /* Keep the ball rolling */
			if (c == TAB)
				curr_indent = ((curr_indent + 8) & ~7)
						 + 8 * (exp - 1);
			else if (c == '\n')
				curr_indent = 0;
			else if (c < SP || c > 0176)
				curr_indent += (2 * exp);
			else
				curr_indent += exp;
		  }
	  }
}

/* Fill-mode version of "Delete Character" */

fim_dchar()
{	/* In C mode, deleting at BOL should do fake TAB preservation */
	if (cur_buf->b_flags & B_CMODE)
	  {	chroff savdot;
		register int c, indent;

		if (e_rgetc() != LF)
		  {	/* Only hack this at BOL */
			e_getc();
			goto normal;
	    	  }
		e_getc();
		savdot = e_dot();
		indent = 0;
		while ((c = e_getc()) == SP || c == TAB)
			if (c == SP)
				++indent;
		else
			indent = (indent + 8) & ~7;
		e_rgetc();
		if (indent >= 4)
		  {	ed_delete(savdot, e_dot());
			ed_indto((indent - 4) & ~3);
			f_begline();		/* HACK!!!! */
		  }
		else
		  {	e_go(savdot);
			ef_deln(exp);
		  }
	  }
	else
 normal:	return (ef_deln(exp));
}

/* Fill-mode version of "Backward Delete Character" */

fim_bdchar()
{	register int ind;

	/* If in C mode, and deleting into white space at BOL, hack tabs */
	if (exp == 1 && cur_buf->b_flags & B_CMODE &&
			(ind = magic_backto_bol()) > 0)
		ed_indto(ind < 4 ? ind - 1 : ((ind - 4) & ~3));
	else
		return (ef_deln (-exp));
}

/* Fill-mode version of "CRLF" */
fim_crlf()
{	register int i;

	if(e_getc() == LF
	  && exp == 1
	  && e_lblankp() && e_lblankp())
	  {	e_gocur();
		e_gonl();
		e_setcur();
		ed_delete(e_dot(), e_eoldot());
	  }
	else
	  {	e_gocur();
#if IMAGEN
		if (cur_buf->b_flags & B_TEXTMODE && exp == 1 &&
		    magic_wrap('\n'))
			return;
		else
#endif /*IMAGEN*/
		if((i = exp) > 0)
			do ed_crins();
			while(--i);
	  }
}

/* Do all magic for auto-wrap in Text mode:
 * return as did wrap (i.e., everything is taken care of)
 */
magic_wrap(tc)
int tc;				/* "trigger char" */
{
	register int c, indent, i, nc;
	chroff savdot, modstart, breakdot;
    
	savdot = e_dot();
	nc = 0;
	if (last_cmd == INSCMD && curr_indent != -1)
	  {	indent = curr_indent;		/* Already know our indent */
		breakdot = best_break;
	  }
	else
	  {
#ifdef INDENTDEBUG
		barf2("Full indent calculation");
#endif
		for (nc = 0; (c = e_rgetc()) != EOF && c != '\n'; ++nc)
		    ;				/* nc: # chars to look at */
		if (c == '\n')			/* Go back over NL */
			e_getc();
		indent = 0;
    
		/* Search for last line break point, leaving it in breakdot */
		breakdot = (chroff)0;
		while (--nc >= 0)
		  {	c = e_getc();
			if (c == TAB)
				indent = (indent + 8) & ~7;
			else if (c < SP || c > 0176)
				indent += 2;
			else
				++indent;
			if ((c == SP || c == TAB) &&
			  (breakdot == (chroff)0 || (indent <= ev_fcolumn)))
				breakdot = e_dot();
		  }
	  }

    /* If there is nothing to do, get out */
	if (indent <= ev_fcolumn)
	  {	e_go(savdot);
		if (tc == SP)
		  {	curr_indent = indent;
			best_break = (chroff)(savdot + 1); /* Remember here, also */
			this_cmd = INSCMD;		/* We do know current indent */
		  }
		else if (tc == '\n')
		  {	curr_indent = 0;
			best_break = (chroff)0;
			this_cmd = INSCMD;
		  }
		else
			errbarf("bad trigger");
		return(0);
	  }

	if (breakdot == (chroff)0)
	  {
	/* No breakpoint found or none needed, just break line at end
	 */
		e_go(savdot);
		modstart = savdot;
		e_putc('\n');
	  }
	else
	  {
	/* Get to breakpoint and replace with newline
	 */
		e_go(breakdot);
		e_rdelc();
		modstart = e_dot();		/* Remember where changes start */
		e_putc('\n');			/* Insert line break */
		e_go(savdot);			/* Get back to trigger point */
	  }
	if (e_rgetc() != '\n')
	  {		/* If not at line start, */
		e_getc();
		e_putc(tc);			/*  insert trigger character */

	/* Once again, compute new indent by backing up to BOL */
		for (nc = 0; (c = e_rgetc()) != EOF && c != '\n'; ++nc)
			;
		if (c == '\n')			/* Go back over NL */
			e_getc();
		indent = 0;
		breakdot = (chroff)0;
		while (--nc >= 0)
		  {		/* Get back to current dot */
			c = e_getc();
			if (c == TAB)
				indent = (indent + 8) & ~7;
			else if (c < SP || c > 0176)
				indent += 2;
			else
				++indent;
			if ((c == SP || c == TAB) &&
			  (breakdot == (chroff)0 || (indent <= ev_fcolumn)))
				breakdot = e_dot();
		  }
		if (breakdot == (chroff)0)	/* If no good break found, use dot */
			breakdot = e_dot();
		curr_indent = indent;		/* Now we know where we are */
		if (tc == '\n')			/* If trigger was NL */
			best_break = (chroff)0;	/*  indent is 0, and no best break */
		else
			best_break = breakdot;	/* This is best break so far */
	  }
	else
	  {	e_getc();
		curr_indent = 0;		/* At line start, no indent */
		best_break = (chroff)0;		/* Do not have a best break so far */
	  }
	ed_setcur();
	buf_tmat(modstart);			/* Alert to potential changes */
	this_cmd = INSCMD;			/* Say we know where we are */
	return(1);
}

/* Do lots of magic things for C-mode indent:
 * erase back to BOL iff we are looking back at white space only,
 * returning the indent level of the original dot
 * (< 0 means no erasure done)
 */
/*#define MYDEBUG /* */
#ifdef MYDEBUG
reveal(msg, v1, v2, v3)
char *msg;
{
	char ahint[128];
	sprintf(ahint, msg, v1, v2, v3);
	barf2(ahint);
}
#endif

magic_backto_bol()
{
	chroff savdot;
	register int c, indent, nc, i;

	savdot = e_dot();
        nc = 0;
	while ((c = e_rgetc()) != EOF && c != LF)
	  {	++nc;			/* Count # chars */
		if (c != SP && c != TAB)
		  {	e_go(savdot);
#ifdef MYDEBUG
			reveal("fail: nc: %d", nc);
#endif
			return -1;
        	  }
    	  }
	if (c == LF)			/* Go back over the LF */
		e_getc();
    	indent = 0;			/* (zero-based indent) */
    	savdot = e_dot();		/* BOL is now origin for delete */
    	for (i = 1; i <= nc; ++i)
    		if ((c = e_getc()) == SP)
			++indent;
		else 			/* (tab) */
			indent = (indent + 8) & ~7;
    	if (nc > 0)			/* Don't bother deleting nothing */
		ed_delete(savdot, e_dot());
#ifdef MYDEBUG
	reveal("indent: %d, nc: %d, foo: %d", indent, nc, 234);
#endif
	return(indent);
}
#endif /*IMAGEN*/

#if ICONOGRAPHICS
/* Iconographics hack for Auto-Fill mode.  Too big and clumsy, but
 * retained for posterity in case it has some obscure feature.
 */

fill_current_line ()
{
        chroff startpos, endpos, savepos, limitpos;
        int i, foundit;
        SBSTR *savep;

        foundit = 0;
        while (d_curind() > ev_fcolumn)
           {
            foundit = 1;
            startpos = e_dot ();
            e_bwsp ();
            while (d_curind() > ev_fcolumn) /* back up to ends of wds*/
               {                                /* until <= fill column */
                while (!c_wsp (e_rgetc ())) ;
                e_bwsp ();
               }
            if (e_dot () == e_boldot ())
               { /*	ding ("Word does not fit in fill column"); */
	                return(0);
               }
            savep = e_copyn (startpos - e_dot ());
            e_setcur ();                /* ed_delete does gocur */
            ed_delete (savepos = e_dot (), startpos);

		f_crlf();		/* Now insert newline */
		e_sputz(fill_prefix);	/* With fill prefix */
            startpos += e_dot () - savepos;
            if (d_curind() > ev_fcolumn)
               {	ed_delete (savepos, e_dot ());
	                sb_sins (cur_buf, savep);
	                e_setcur ();
        	        ding ("Fill prefix > fill column???");
                	return(0);
               }
            savepos = e_dot ();         /* gun inherited initial whitespace */
            sb_sins (cur_buf, savep);
            e_go (savepos);
            e_fwsp ();
            if ((limitpos = e_dot ()) > startpos) limitpos = startpos;
                                        /* in case rest of line was white */
            ed_delete (savepos, limitpos);
            e_gosetcur (startpos + savepos - limitpos);
           }

        return foundit;
  }
#endif /*ICONOGRAPHICS*/
