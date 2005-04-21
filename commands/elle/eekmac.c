/* ELLE - Copyright 1982, 1985, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*	EEKMAC - Keyboard Macro routines
 *		Modelled after the "e_macro.c" for ICONOGRAPHICS
 *		by C. D. Tavares, 9/11/82
 */

#include "elle.h"

#if FX_SKMAC		/* Entire file is under this conditional! */

int kdef_mode;		/* Set when collecting (a "minor mode") */
static int km_flag = 0;	/* 1 = executing, -1 collecting, 0 neither */
static int km_exp;	/* Arg to "Execute Kbd Macro" - # times more to xct */
static struct buffer *km_buf;

/* EFUN: "Start Kbd Macro" */

f_skmac()
{	register struct buffer *b;
	struct buffer *make_buf();

	if(km_flag)
	  {	ding("Kbd macro active, ignoring \"Start Kbd Macro\"");
		return;
	  }
	if((b = km_buf) == 0)
		b = km_buf = make_buf(" *KBDMAC*");
	ex_reset(b);
	km_flag = -1;		/* Say starting macro collection */
	kdef_mode = 1;
	redp(RD_MODE);
}

/* EFUN: "End Kbd Macro" */

f_ekmac()
{
	if(km_flag > 0 && (--km_exp >= 0))
	  {	ex_go((SBBUF *)km_buf, (chroff)0);
	  }
	else if(km_flag)
	  {	km_flag = 0;
		kdef_mode = 0;	/* Flush minor mode */
		redp(RD_MODE);
	  }
}

/* EFUN: "Execute Kbd Macro" */

f_xkmac()
{
	if(km_flag)
		ding("Already in kbd macro!");
	else if(km_buf == 0)
		ding("No kbd macro defined");
	else if((km_exp = exp-1) >= 0)
	  {
		ex_go((SBBUF *)km_buf, (chroff) 0);
		km_flag = 1;		/* Start macro execution */
	  }
}

/* EFUN: "View Kbd Macro" */

f_vkmac()
{	register struct buffer *b, *savbuf;
	chroff prmplen;

	if(!(b = km_buf))
	  {	ding("No kbd macro defined");
		return;
	  }
	savbuf = cur_buf;
	chg_buf(b);
	e_gobob();
	e_sputz("Current Kbd macro:\n\n");
	prmplen = e_dot();
	mk_showin(b);		/* Show the macro buffer temporarily */
	e_gobob();
        chg_buf(savbuf);
	sb_deln((SBBUF *)b, prmplen);	/* Flush the prompt */
}

/* KM_GETC - return next command char from kbd macro being executed.
**	This is < 0 if not executing kbd macro.  Also responsible for
**	gathering input for kbd macro.
*/
km_getc()
{	register int c;

	while (km_flag > 0)		/* Executing macro? */
	  {	c = sb_getc(((SBBUF *)km_buf));	/* Yes, get char */
		if(c != EOF)
			return(c);		/* and return as cmd */

		if(--km_exp >= 0)		/* Macro done.  Repeat? */
			ex_go((SBBUF *)km_buf, (chroff)0);	/* Yes */
		else km_flag = 0;		/* No, stop execution */
	  }
	c = tgetc();			/* Get char from user (TTY) */
	if(km_flag < 0)			/* Save it if collecting macro */
	  {	sb_putc(((SBBUF *)km_buf), c);
	  }
	return(c);
}

/* KM_INWAIT() - Return TRUE if any keyboard-macro input waiting.
 */
km_inwait()
{	register int c;
	if(km_flag > 0)
		if((c = sb_getc(((SBBUF *)km_buf))) != EOF || (km_exp > 0))
		  {	sb_backc(((SBBUF *)km_buf));
			return(1);
		  }
	return(0);
}

km_abort ()
{
	if(km_flag > 0)		/* Executing? */
		km_flag = 0;	/* Stop */
	else if(km_flag < 0)	/* Collecting? */
		f_ekmac();	/* Close it out */
}

#endif /*FX_SKMAC*/

#if 0	/* Old unused stuff */
static char mode_buf [60];

add_mode (mode)
  char *mode;
  {
        register char *cur, *c, *m;

        if (cur_mode != mode_buf)
           {
            strcpy (mode_buf, cur_mode);
            cur_mode = mode_buf;
           }

        if (cur_mode [0]) strcat (cur_mode, ", ");
        strcat (cur_mode, mode);
        make_mode ();
  }

remove_mode (mode)
  char *mode;
  {
        register char *cur, *c, *m;

        if (*cur_mode == 0) return;

        if (cur_mode != mode_buf)
           {
            strcpy (mode_buf, cur_mode);
            cur_mode = mode_buf;
           }

        for (cur = cur_mode ; *cur ; cur++)
            if (*cur == *mode)          /* 1st char matches */
               {
                for (c = cur, m = mode ; *m && (*m == *c) ; m++, c++) ;
                if (!(*m))              /* ok, mode matched */
                   {                    /* kill leading ", " */
                    if (*(cur - 1) == ' ') --cur;
                    if (*(cur - 1) == ',') --cur;
                    for ( ; *cur = *c ; cur++, c++) ;   /* recopy to end */
                    make_mode ();
                    return;
                   }
               }
  }
#endif /*COMMENT*/
