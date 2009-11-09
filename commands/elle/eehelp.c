/* ELLE - Copyright 1982, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*	EEHELP - Help function
 */

#include "elle.h"		       /* include structure definitions */


#if FX_DESCRIBE
/* EFUN: "Describe" */
/*	DESCRIBE - Help-command hack.
**	Crude approximation of EMACS function.
*/
static struct buffer *help_buf;

f_describe()
{	register char *cp;
	register int i, c;
	char str[10];
	struct buffer *savbuf, *b, *make_buf();
	chroff bdot;

	saynow("Help for command: ");
	i = cmd_idx(c = cmd_read());		/* Get function idx for cmd */
	if(c&CB_META) sayntoo("M-");
	if(i == FN_PFXMETA)
	  {	sayntoo("M-");
		i = cmd_idx(c = (cmd_read() | CB_META));
	  }
	else if(i == FN_PFXEXT)
	  {	sayntoo("^X-");
		i = cmd_idx(c = (cmd_read() | CB_EXT));
	  }
	str[0] = c&0177;
	str[1] = 0;
	sayntoo(str);

	/* Now read in the help file, if necessary */
	savbuf = cur_buf;
	if(help_buf)
		chg_buf(help_buf);
	else
	  {
		saynow("Loading ");
		sayntoo(ev_helpfile);
		sayntoo("...");
		chg_buf(help_buf = make_buf(" **HELP**"));
		if(read_file(ev_helpfile) == 0)
		  {	chg_buf(savbuf);
			kill_buf(help_buf);
			help_buf = 0;
			return;
		  }
	  }


	/* Find function index in current buffer */
	cp = str;
	*cp++ = '<';
	*cp++ = 'F';
	cp = dottoa(cp, (chroff)i);
	*cp++ = '>';
	e_gobob();
	if(e_search(str, cp-str, 0) == 0)
		sayntoo(" No help found");
	else
	  {
		bdot = e_dot();
		while(!e_lblankp()) e_gonl();	/* Move past 1st blank line */
		b = make_buf(" *SHOW*");
		sb_sins((SBBUF *)b, e_copyn(bdot - e_dot()));
		mk_showin(b);			/* Show the stuff */
		kill_buf(b);
		sayclr();
	  }
	chg_buf(savbuf);
}
#endif /*FX_DESCRIBE*/
