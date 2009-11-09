/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */

/* EECMDS	Command table lookup and profile code
 */

#include "elle.h"

/* Function table, see the included file for explanation. */

	/* First must pre-declare function addrs */
#define EFUN(rtn,rtnstr,name) int rtn();
#define EFUNHOLE
#include "eefdef.h"

	/* Now re-insert to define function table */
int (*funtab[])() =
{
#undef EFUN		/* Avoid redefinition error message */
#undef EFUNHOLE
#define EFUN(rtn,rtnstr,name) rtn,
#define EFUNHOLE 0,
#include "eefdef.h"
};
int funmax = sizeof(funtab)/sizeof(funtab[0]);	/* 1st illegal function # */

/* Insert default command char map tables and profile structure */

#include "defprf.c"

/* EFUN: "Prefix Meta" */
/*	Meta-prefix command.
 *	For now, very simple.  Perhaps later try to hair up with
 *	time-out "M-" prompt?
 */
f_pfxmeta()
{	return(cmd_xct(cmd_read()|CB_META));
}

/* EFUN: "Prefix Extend" */
/*	Extended-prefix command.
 *	Likewise trivial; perhaps later hair up with timeout "^X-" prompt?
 */
f_pfxext()
{	return(cmd_xct(cmd_read()|CB_EXT));
}

/* EFUN: "Universal Arg" */
/*	This routine is also called by "Argument Digit" with a special arg
 * of -1 in order to share code.  Since that invocation always sets unrchf,
 * it should always complete at least one digit read loop.
 * Note that exp and exp_p are set to 1 and 0 at the top-level command
 * loop.
 */
f_uarg(ch)
int ch;
{	register int c, oc, i;

	/* Set distinguishing exp_p value depending on whether invoked
	 * by CTRL-U or another function (Argument Digit, Negative Argument)
	 */
	exp_p = (ch < 0) ? 1 : 4;
	i = 0;			/* Read numerical arg if any follows */
	for(;;)
	  {	oc = cmd_read();	/* Get next input char */
		c = oc & 0177;
		if(c == '-' && !i)
		  {	exp_p = -1;
			exp = 1;	/* Set in case no digits follow */
		  }
		else if('0' <= c && c <= '9')	/* If it's a digit too, */
		  {	i = (i * 10) + c - '0';	/* add digit in. */
			if(exp_p >= 0) exp_p = 1;
			exp = i;
		  }
		else break;
	  }
	exp *= exp_p;		/* Multiply arg appropriately */
	unrchf = oc;		/* Not a digit, re-read it next. */

	this_cmd = ARGCMD;
}

/* EFUN: "Negative Argument" */
f_negarg(ch)
int ch;
{	f_uarg(-1);		/* Invoke code from Universal Arg */
	exp = -exp;
}

/* EFUN: "Argument Digit" */
f_argdig(ch)
int ch;
{	unrchf = ch;		/* Re-read the digit */
	f_uarg(-1);		/* Invoke code from Universal Arg */
}

/* EFUN: "Set Profile" */
/*	Asks for a profile file and sets profile from it.
 */
f_setprof()
{	int set_profile();
	hack_file("Set Profile: ", set_profile);
}

#if FX_VTBUTTONS
/* EFUN: "VT100 Button Hack" */
/*	This must be bound to Meta-O if anything, because the VT100 sends
 *	an ESC O prefix when the function buttons are used.
 */
f_vtbuttons ()			/* vt100 function buttons */
{
	switch(cmd_read())
	  {	case ('A'): 
			return (f_uprline ());
		case ('B'): 
			return (f_dnrline ());
		case ('C'): 
			return (f_fword ());
		case ('D'): 
			return (f_bword ());
		case ('Q'): 		/* PF1 */
			return (f_kregion());
		default: 
			ring_bell ();
		break;
	  }
}
#endif /*FX_VTBUTTONS*/

/* CMD_WAIT() - Return TRUE if any command input waiting.
*/
cmd_wait()
{	return(unrchf >= 0
#if FX_SKMAC
		|| km_inwait()		/* Check for kbdmac input waiting */
#endif /*FX_SKMAC*/
		|| tinwait());
}

/* CMD_READ() - Read a command (single char) from user, and return it.
*/
cmd_read()
{	register int c;

	if((c = unrchf) >= 0)	/* Re-reading last char? */
	  {	unrchf = -1;
		return(c);
	  }
#if FX_SKMAC			/* Hacking keyboard macros? */
	return(km_getc());	/* Yes.  This calls tgetc if no kbd macro */
#else
	return(tgetc());
#endif /*-FX_SKMAC*/
}

/* CMD_XCT(ch) - Command Execution dispatch routine.
**	Takes char and executes the function (efun) bound to that command key.
*/
cmd_xct(ch)
int ch;
{	register int (*funct) ();
	register int c;
	int (*(cmd_fun())) ();

	if(funct = cmd_fun(c = ch))		/* Get function to run */
		return((*funct) (c&0177));	/* Invoke with char arg */
	ring_bell();		/* Undefined command char, error. */
}

/* CMD_FUN(ch) - Return function for char, 0 if none
*/
int (*cmd_fun(c))()
int c;
{
	return(funtab[cmd_idx(c)]);
}

/* CMD_IDX(ch) - Given command char, return function index for it
*/
cmd_idx(c)
register int c;
{	register char *cp;
	register int i;

	if(c&CB_EXT)
	  {	cp = def_prof.extvec;
		i = def_prof.extvcnt;
		goto inlup;
	  }
	if(c&CB_META)
	  {	cp = def_prof.metavec;
		i = def_prof.metavcnt;
	inlup:	c = upcase(c);
		do {	if(*cp++ != c) cp++;
			else
			  {	i = *cp&0377;
				break;
			  }
		  } while(--i);		/* If counts out, will return 0! */
	  }
	else i = def_prof.chrvec[c&0177]&0377;
	if(i >= funmax)
		return(0);
	return(i);
}

/* Profile hacking */

#if TOPS20
#include <sys/file.h>		/* for O_BINARY */
#endif

set_profile(filename)
char *filename;
{	char pfile[200];
	char psfile[200];
	register int pfd, len;
	chroff sbx_fdlen();
	register char *profptr;
	struct stored_profile st_prof;

	if(filename) strcpy(pfile,filename);
	else		/* Check for user's profile */
	  {
		strcat(strcat(strcpy(pfile,homedir),"/"),ev_profile);
	  }
	if((pfd = open(pfile,
#if TOPS20
				O_BINARY
#else
				0
#endif
					)) < 0)
	  {	if(filename)
		  {	ding("Cannot open file");
		  }
		return;
	  }
	if((len = (int)sbx_fdlen(pfd)) < sizeof(struct stored_profile))
		goto badfil;
	profptr = memalloc((SBMO)len);
	if(read(pfd,profptr,len) != len)
		goto badfmt;

	/* Have read profile into memory, now set up ptrs etc */
	bcopy((SBMA)profptr,(SBMA)&st_prof,sizeof(struct stored_profile));
	def_prof.version = prof_upack(st_prof.version);
	if(def_prof.version != 1)
		goto badfmt;
	def_prof.chrvcnt = prof_upack(st_prof.chrvcnt);
	def_prof.chrvec  = profptr + prof_upack(st_prof.chrvec);
	def_prof.metavcnt = prof_upack(st_prof.metavcnt);
	def_prof.metavec = profptr + prof_upack(st_prof.metavec);
	def_prof.extvcnt = prof_upack(st_prof.extvcnt);
	def_prof.extvec  = profptr + prof_upack(st_prof.extvec);
#if SUN
	def_prof.menuvcnt = prof_upack(st_prof.menuvcnt);
	def_prof.menuvec = profptr + prof_upack(st_prof.menuvec);
#endif /*SUN*/
	goto done;

badfmt:	chkfree(profptr);
badfil:	ding("Bad profile format");
done:	close(pfd);
}

#if SUN
/* SUN Menu profile hacking.
 *	This is here, instead of e_sun.c, because
 * the profile format is still evolving and for the time being I want to
 * keep all profile-hacking code in one place. --KLH
 */
#include "suntool/tool_hs.h"
#include "suntool/menu.h"

#define MENUMAX	16

/* Defined in eesun.c */
extern struct menu *menuptr;
extern struct menu menu;

char *funamtab[] = {
#undef EFUN
#undef EFUNHOLE
#define EFUN(rtn,rtnstr,name) name,
#define EFUNHOLE 0,
#include "eefdef.h"
};

init_menu() /* initialize the menu for elle from user profile */
{
	register struct menuitem *mi;
	register int n, i, fni;

	if((n = def_prof.menuvcnt) <= 0)
		return;
	if(n > MENUMAX) n = MENUMAX;
	mi = menu.m_items = (struct menuitem *) calloc(n, sizeof *mi);

	menu.m_itemcount = 0;
	for(i = 0; i < n; i++)
	  {	fni = def_prof.menuvec[i]&0377;
		if(fni >= funmax) continue;
		if(funtab[fni] && funamtab[fni])
		  {	mi->mi_data = (caddr_t) funtab[fni];
			mi->mi_imagedata = (caddr_t) strdup(funamtab[fni]);
			mi->mi_imagetype = MENU_IMAGESTRING;
			mi++;
			menu.m_itemcount++;
		  }
	  }
	if(menu.m_itemcount)
		menuptr = &menu;
}
#endif /*SUN*/
