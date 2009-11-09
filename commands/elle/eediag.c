/* ELLE - Copyright 1982, 1985, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*	EEDIAG - Error diagnostics and testing routines
 */

#include "elle.h"

#if FX_DEBUG

/* EFUN: "Debug Mode" */
/*	With no arg, toggles self-checking on and off.
 *	With arg of 4 (^U), enters special debug/diagnostics mode.
 */

f_debug(ch)
int ch;
{	extern int (*vfy_vec)();	/* In E_MAIN.C */
	char *vfy_data();

	if(ch < 0)		/* Internal call? */
	  {	dbg_diag();
		return;
	  }
	if(exp == 4)
	  {	askerr();
		return;
	  }
	if(vfy_vec) vfy_vec = 0;		/* Toggle current value */
	else vfy_vec = (int (*)())vfy_data;
	say(vfy_vec ? "Self-checking on" : "Self-checking off");
}

char *
vfy_data(flag)		/* Flag = 0 for quiet check */
int flag;
{
	register char *res, *mess;
	char *sbe_mvfy(), *sbe_sbvfy(), *sbe_svfy();

	if(res = sbe_mvfy(0)) mess = "Mem mgt";
	else if(res = sbe_sbvfy(cur_buf,0)) mess = "SBBUF";
	else if(res = sbe_svfy(0)) mess = "SD list";
	else return(0);		/* Success */

	if(flag)
	  {	int ostate = clean_exit();
		printf("\n%s error: %s !!!\n",mess,res);
		askerr();
		if(ostate > 0) set_tty();
	  }
	return(res);	/* Error seen */
}

extern char *asklin();
extern int sbx_nfl,sbm_nfl;

char diaghelp[] = "\
Q - Quit diag mode\n\
! - Goto subshell\n\
V - Verify Mem & SD lists\n\
MF - Mem Freelist\n\
M  - Mem list\n\
B  - Current buffer SB\n\
DF - SD Freelist\n\
D  - SDs in use\n\
DL - SD Logical lists\n\
DP - SD Physical lists\n\
C n - Compact; 0-7=sbx_comp(n), 8=SM freelist, 9=SD freelist.\n\
W  - Window printout\n\
X n - Xercise randomly (GC every n)\n\
Z n - like X but with notes\n";

dbg_diag()
{	register char *cp;
	register int c;
	char linbuf[100];
	char *sbe_mfl();
	char *sbe_sfl();
	char *sbe_sbs();
	char *sbe_sdlist();

    for(;;)
    {	printf("D>");
	asklin(cp = linbuf);			/* Read a line of input */
	switch(upcase(*cp++))
	  {
	case '?':
		writez(1,diaghelp);	/* Too long for printf */
		continue;
	case '!':
		f_pshinf();		/* Invoke inferior subshell */
		clean_exit();		/* Restore normal modes */
		continue;
	case 'Q':		/* Quit */
		return;

	case 'B':		/* Print current SBBUF */
		sbe_sbs(cur_buf,1);
		continue;

	case 'C':		/* C n - Compact */
		c = atoi(&linbuf[1]);
		if(c == 8)
			sbm_ngc();	/* GC the SM nodes */
#if 0 /* This doesn't work, dangerous to invoke. */
		else if(c == 9)
			sbm_xngc(&sbx_nfl,sizeof(struct sdblk),
				SM_DNODS);
#endif
		else
			sbx_comp(512,c);
		continue;

	case 'D':		/* Print all SD blocks in mem order */
		switch(upcase(*cp))
		  {
		case 0:		/* D - all SDs in mem order */
			sbe_sds();
			continue;
		case 'F':	/* DF - SD freelist */
			sbe_sfl(1);
			continue;
		case 'L':	/* DL - SD logical list */
			sbe_sdlist(1,0);
			continue;
		case 'P':	/* DP - SD physical list */
			sbe_sdlist(1,1);
			continue;
		  }
		break;		/* failure */

	case 'M':	
		switch(upcase(*cp))
		  {
		case 0:		/* M - all mem alloc info */
			sbe_mem();
			continue;
		case 'F':	/* MF - mem freelist */
			sbe_mfl(1);
			continue;
		  }
		break;		/* failure */

	case 'V':		/* Verify */
		if(cp = vfy_data(0))
			printf("  Failed: %s\n",cp);
		else printf("  OK\n");
		continue;
	case 'W':		/* Print out current window */
		db_prwind(cur_win);
		continue;
	case 'X':		/* Xercise */
		c = atoi(&linbuf[1]);
		vfy_exer(0, c ? c : 100);
		continue;
	case 'Z':		/* Zercise */
		c = atoi(&linbuf[1]);
		vfy_exer(1, c ? c : 100);
		continue;

	  }	/* End of switch */

	printf("?? Type ? for help\n");
    }	/* Loop forever */
}


/* VFY_EXER - a "random" editor exerciser.  It creates a buffer,
 *	fills it with some patterned stuff, and then edits it
 *	pseudo-randomly in ways which retain the basic pattern.
 *	Frequent GC's and self-checks are done, and execution
 *	halted either when an error is seen or when typein is detected.
 */
char *xer_strs [] = {
	"throne", "too", "sky", "fore", "fingers", "sex", "stone",
	"010", "nazgul", "base"
};


vfy_exer(pf, gcfrq)
int pf;			/* Nonzero to print notes as we go */
int gcfrq;		/* Frequency of GC invocation (# passes per GC) */
{	register int i, k, c;
	long npass;
	char *res, linbuf[100];
	chroff lbeg, lend;
	struct buffer *bfp, *make_buf();

	/* Clean out kill buffer first */
	for(i = 0; i < KILL_LEN; ++i)
		kill_push((SBSTR *)0);

	bfp = make_buf("**EXORCISE**");
	chg_buf(bfp);
	i = 2000;
	e_gobol();
	do {
		ed_sins("Line ");
		ed_sins(xer_strs[i%10]);
		e_putc(LF);
	  } while(--i);
	if(pf) printf("Bufflen: %ld\n", e_blen());

	/* Buffer now has stuff in it, start hacking. */
	npass = 0;
	srand(1);	/* Start random seed */
	for(;;)
	  {	if(tinwait() && (*asklin(linbuf)))
		  {	printf("Typein stop.\n");
			break;
		  }
		++npass;
		printf(" Pass %ld",npass);
		if(npass%gcfrq == 0)		/* Time to do a GC? */
		  {
			i = rand();		/* Level between 0-4 */
			i = (i < 0 ? -i : i) % 5;
			printf(" - GC lev %d\n", i);
			sbx_comp(512,i);
			goto xerchk;
		  }

		k = (i = rand())%1024;
		if (i&020000) k = -k;
		e_igoff(k);		/* Move randomly */
		e_gobol();		/* Get stuff to flush */
		lbeg = e_dot();
		k = (i = rand())%64;
		if(i&010000) k = -k;
		e_igoff(k);
		lend = e_nldot();
		if(pf) printf(" Kill %ld/ %d;", lbeg, k);
		ed_kill(lbeg, lend);
		if(res = vfy_data(0))
		  {	printf("XERR after kill: %s\n",res);
			break;
		  }
		k = (i = rand())%2048;
		if(i&04000) k = -k;
		e_igoff(k);
		e_gobol();
		e_setcur();
		if(pf) printf(" Yank %ld;", e_dot());
		f_unkill();		/* Yank back */
		if(res = vfy_data(0))
		  {	printf("XERR after yank: %s\n",res);
			break;
		  }
		last_cmd = YANKCMD;
		for(i = rand()%4; i >= 0; --i)
		  {	if(pf) printf(" Pop;");
			f_unkpop();	/* Do meta-Y */
			if(res = vfy_data(0))
			  {	printf("XERR after pop: %s\n",res);
				goto out;
			  }
		  }
		if(rand()&07)	/* Slowly add stuff */
		  {	if(pf) printf(" Add");
			ed_sins("Line ");
			ed_sins(xer_strs[rand()%10]);
			e_putc(LF);
			if(res = vfy_data(0))
			  {	printf("XERR after ins: %s\n",res);
				break;
			  }
		  }
		printf("\n");

		/* Okay, done with this pass edits, run through the
		 * file to ensure pattern is still there
		 */
	xerchk:	e_gobob();
		while((c = e_getc()) != EOF)
			if(c == LF && (c = e_getc()) != EOF)
			  {	if(         c != 'L'
				  || e_getc() != 'i'
				  || e_getc() != 'n'
				  || e_getc() != 'e'
				  || e_getc() != ' ')
				  {	printf("XERR in pattern!\n");
					goto out;
				  }
			  }
	  }
	/* User typein or error, stop. */
out:	e_setcur();
	redp(RD_SCREEN);
	printf("Loop count = %ld\n",npass);
}

/* DB_PRWIND(win) - Print out stuff about given window
 */
db_prwind(w)
register struct window *w;
{	register struct scr_line *s;
	register int i;
	char tstr[MAXLINE+MAXCHAR];
	char *db_scflgs();

	printf("cur_dot/ %ld  cur_buf/ %o cur_win/ %o\n",
		cur_dot, cur_buf, cur_win);

	printf("Window %o:\n", w);
	printf("  next/ %o\n", w->w_next);
	printf("  buf / %o\n", w->w_buf);
	printf("  redp/ %o\n", w->w_redp);

	printf("  topldot/ %ld\n", w->w_topldot);
	printf("  dot / %ld\n", w->w_dot);
	printf("  bmod/ %ld\n", w->w_bmod);
	printf("  emod/ %ld\n", w->w_emod);
	printf("  oldz/ %ld\n", w->w_oldz);

	printf("  pos / %d\n", w->w_pos);
	printf("  ht  / %d\n", w->w_ht);
	printf("\
#  Flags   Boff Len ! Cols Line\n");
	for(i = w->w_pos; i < w->w_pos + w->w_ht; ++i)
	  {	s = scr[i];
		printf("%2d %-5.5s %6ld %3d %1d %4d ",
			i, db_scflgs(s->sl_flg), s->sl_boff, s->sl_len,
			s->sl_cont, s->sl_col);
		strncpy(tstr, s->sl_line, MAXLINE);
		tstr[s->sl_col] = 0;
		printf("%-40.40s\n", tstr);
		if(s->sl_flg&SL_MOD)
		  {	printf("%26d ", s->sl_ncol);
			strncpy(tstr, s->sl_nlin, MAXLINE);
			tstr[s->sl_ncol] = 0;
			printf("%-40.40s\n", tstr);
		  }
	  }
}

char *
db_scflgs(flags)
int flags;
{	static char retstr[10];
	register char *cp;
	
	cp = retstr;
	if(flags&SL_MOD) *cp++ = 'M';
	if(flags&SL_EOL) *cp++ = 'E';
	*cp = 0;
	return(retstr);
}

#endif /*FX_DEBUG*/
