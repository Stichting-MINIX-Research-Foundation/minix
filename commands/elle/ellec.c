/* ELLEC - Copyright 1983 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/* ELLEC - ELLE Compiler
 *    Invoked with no arguments, acts as general ELLE compiler filter;
 *	reads ASCII profile input, and outputs a binary profile.
 *    Otherwise:
 *	-Profile	Compiles user's profile.
 *		HOME/.ellepro.e -> HOME/.ellepro.bN (N = fmt version #)
 *		NOTE: on V6, "HOME/" is left out.
 *	-Pconf		Outputs defprf.c for ELLE (accepts std in) (old -E)
 *	-Fconf		Outputs eefdef.h for ELLE (accepts std in)
 *	-FXconf		Outputs eefidx.h for ELLE (accepts std in)
 *	-CMconf		Outputs config makefile for ELLE ( " )
 *	-CSconf	arg	Outputs config file using "arg" - for V6 config.v6 file.
 *	-Pdump		Outputs defprf.e user profile (old -P)
 *	-Fdump		Outputs deffun.e
 */

#if 0
The ASCII profile file associates command chars with functions.
It is simply a series of lisp-like expressions of the form
	(keybind <char spec> <fun spec>)
		
	e.g. (keybind "C-Y" "Yank Pop")

Command char specification:
	Allowed prefixes:
		<ch>	The char itself
		C-	Controlify (zap bits 140)
		^<ch>	Ditto
		M-	Meta (add bit 200) - case ignored
		X-	Extended (add bit) - case ignored
	To quote a char or char spec, use quoted-string syntax.

Function name specification:
	Function names are specified by quoted strings containing the entire
	long-form ASCII function name.  Matching is done case-independently.

#endif /*COMMENT*/


#include "eesite.h"		/* Site specific stuff if any */
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "eeprof.h"		/* Profile structure definition */


#define EFUNMAX 400	/* Maximum # (+1) of functions that can be defined */
#define KBTSIZ (128*2)	/* Initial size of key binding tables */


/* EFUN Function definition table.
**	Functions that were copied from the pre-defined table will
** have a value of NULL in ef_mod.
*/
struct fun {
	int ef_idx;	/* Function index (same as index of entry) */
	char *ef_name;	/* Function name */
	char *ef_adr;	/* C routine name in ELLE */
	char *ef_mod;	/* Source module that C routine lives in */
};
struct fun efuntab[EFUNMAX];
int efxmax = 0;		/* Largest function idx used */

int format_ver = PROF_VER;	/* Current version # of binary profile fmt */

/* Keybind mapping tables.  There are four separate tables:
**	Simple character.  Always 128 single-byte items, indexed by the simple
**		command char.  Each item is the corresponding function index.
**	Meta char.  Variable number of 2-byte items; first is a command char
**		and second is its function index.
**	Extended char.  Same format as Meta char.
**	Menu item (SUN only).  Variable number of single-byte items, each
**		a function index.
**
*/
char *chrptr;		/* Pointer to simple-char table */
int chrsiz = 128;	/* Current size of table */
int chrcnt = 0;		/* # bytes actually used */

char *mtaptr;		/* Pointer to meta-char table */
int mtasiz = KBTSIZ;	/* Current size (twice the # of items) */
int mtacnt = 0;		/* # bytes actually used */

char *extptr;		/* Pointer to ext-char table */
int extsiz = KBTSIZ;	/* Current size (twice the # of items) */
int extcnt = 0;		/* # bytes actually used */

char *mnuptr;		/* Pointer to menu-item table (SUN only) */
int mnusiz = KBTSIZ;	/* Current size */
int mnucnt = 0;		/* # bytes actually used */


#define CB_EXT 0400		/* "X-" prefix on command char */
#define CB_META 0200		/* "M-" prefix on command char */


/* Set up the pre-defined data areas.  This includes the
** predefined function table plus the default profile (keyboard mappings).
** Note this only contains entries for ef_name and ef_adr.
*/

struct fun pdfuntab[] = {	/* Pre-Defined Function table */
#define EFUN(rtn,rtnstr,name) 0, name, rtnstr, 0,
#define EFUNHOLE 0, 0, 0, 0,
#include "eefdef.h"
};
int npdfuns = sizeof(pdfuntab)/sizeof(struct fun);	/* # of entries */

#include "defprf.c"	/* Insert default profile mapping */
			/* This defines charmap, metamap, and extmap. */

/* Stuff for feeble-minded list processor */
#define NIL ((struct lnode *)0)
#define LTRUE (&ltruenode)

#define LT_VAL 0
#define LT_STR 1
#define LT_LIST 2
#define LT_ATOM 3	/* Should use this later instead of LT_STR */

struct lnode {
	struct lnode *lnxt;
	int ltyp;
	union {
		int lvi;
		char *lvs;
		struct lnode *lvl;
	} lval;
};

struct lnode ltruenode;		/* Constant TRUE */

_PROTOTYPE(int main , (int argc , char **argv ));
_PROTOTYPE(int doargs , (int argc , char **argv ));
_PROTOTYPE(char **findkey , (char *cp , char ***aretp , char **tabp , int tabsiz , int elsize ));
_PROTOTYPE(int nstrcmp , (char *s1 , char *s2 ));
_PROTOTYPE(int ustrcmp , (char *s1 , char *s2 ));
_PROTOTYPE(int strueq , (char *s1 , char *s2 ));
_PROTOTYPE(int do_opcod , (void));
_PROTOTYPE(int do_opasc , (void));
_PROTOTYPE(int outkbind , (int c , int fx ));
_PROTOTYPE(int do_obprof , (void));
_PROTOTYPE(int mupcase , (int ch ));
_PROTOTYPE(int upcase , (int ch ));
_PROTOTYPE(char *qstr , (char *str ));
_PROTOTYPE(char *charep , (int c ));
_PROTOTYPE(int do_ocnf , (char *str ));
_PROTOTYPE(int do_ofcod , (void));
_PROTOTYPE(int do_ofasc , (void));
_PROTOTYPE(int do_ofxcod , (void));
_PROTOTYPE(int compile_stdin , (void));
_PROTOTYPE(int lrch , (void));
_PROTOTYPE(struct lnode *lread , (void));
_PROTOTYPE(int wspfls , (void));
_PROTOTYPE(struct lnode *lrstr , (int flg ));
_PROTOTYPE(int islword , (int c ));
_PROTOTYPE(struct lnode *eval , (struct lnode *lp ));
_PROTOTYPE(struct lnode *undefall , (struct lnode *lp ));
_PROTOTYPE(struct lnode *efun , (struct lnode *lp ));
_PROTOTYPE(struct lnode *keybind , (struct lnode *lp ));
_PROTOTYPE(struct lnode *keyallun , (void));
_PROTOTYPE(struct lnode *menuitem , (struct lnode *lp ));
_PROTOTYPE(int repchar , (char *str ));
_PROTOTYPE(struct lnode *getln , (void));
_PROTOTYPE(int numcvt , (char *str , int *anum ));
_PROTOTYPE(int listcnt , (struct lnode *lp ));
_PROTOTYPE(char *funname , (int i ));
_PROTOTYPE(int findfun , (char *name ));
_PROTOTYPE(int funcnt , (int *arr ));
_PROTOTYPE(int scpy , (char *from , char *to , int cnt ));
_PROTOTYPE(char *stripsp , (char *cp ));

int warn();
int lerr();
int fatal();


/* ELLEC argument stuff */
char *argfile;
char *outfile;
int swfilter;	/* If no args */
int swprof;	/* -P */
int swelle;	/* -E */

int uproflg;	/* Do compilation of user's profile */
int swpcnf;	/* Output defprf.c (C initialization of profile) */
int swfcnf;	/* Output eefdef.h */
int swfxcnf;	/* Output eefidx.h */
int swcmcnf;	/* Output config makefile (makecf.fun) */
char *swcscnf;	/* Output config specially (for V6) */
int swallc;	/* Do all of config stuff */
int swfdmp;	/* Output deffun.e */
int nfiles;	/* # file specs seen */

main(argc,argv)
int argc;
char **argv;
{	register int i;
	register char *cp;
	char temp[300];

	/* Initialize LTRUE
	** (cannot portably initialize a union at compile time)
	*/
	ltruenode.ltyp = LT_VAL;	/* Set type (other fields zero) */

	/* Process switches */
	if(argc <= 1) swfilter++;	/* If no args, assume filter */
	else doargs(argc,argv);

	/* Initialize function definitions and key bindings from predefs */
	chrptr = malloc(chrsiz);
	mtaptr = malloc(mtasiz);
	extptr = malloc(extsiz);
	mnuptr = malloc(mnusiz);
	if (!chrptr || !mtaptr || !extptr || !mnuptr)
		fatal("cannot init, no memory");

	scpy(charmap, chrptr, (chrcnt = sizeof(charmap)));
	scpy(metamap, mtaptr, (mtacnt = sizeof(metamap)));
	scpy( extmap, extptr, (extcnt = sizeof(extmap)));
	if(def_prof.menuvec)
		scpy(def_prof.menuvec, mnuptr, (mnucnt = def_prof.menuvcnt));

	for(i = 0; i < npdfuns; ++i)		/* Initialize function defs */
		if(pdfuntab[i].ef_name)
		  {	efuntab[i].ef_idx = i;
			efuntab[i].ef_name = pdfuntab[i].ef_name;
			efuntab[i].ef_adr = stripsp(pdfuntab[i].ef_adr);
			if(efxmax < i) efxmax = i;
		  }


	/* Routines expect input from stdin and output their results
	 * to stdout.
	 */
	if(argfile)
		if(freopen(argfile,"r",stdin) == NULL)
			fatal("cannot open input file \"%s\"",argfile);
	if(outfile)
		if(freopen(outfile,"w",stdout) == NULL)
			fatal("cannot open output file \"%s\"",outfile);


	/* Check for general compilation */
	if(swfilter)
	  {	/* Not really implemented yet */
		fatal("bad usage, see doc");
	  }

	/* Do profile hacking of some kind */
	if(swprof || swelle)
	  {	if (compile_stdin())	/* Compile input profile */
		  {	if(swprof)
				do_opasc();	/* Output ASCII profile (.e) */
			else if(swelle)
				do_opcod();	/* Output bin profile (.b1) */
		  }
		exit(0);
	  }

	/* Check for variousness */
	if(swpcnf)
	  {	if(compile_stdin())		/* Compile input */
			do_opcod();	/* Output the C initialization code */
		exit(0);
	  }
	if(swfxcnf)
	  {	if(compile_stdin())		/* Compile input */
			do_ofxcod();	/* Output the eefidx.h code */
		exit(0);
	  }
	if(swfcnf)
	  {	if(compile_stdin())		/* Compile input */
			do_ofcod();	/* Output the eefdef.h code */
		exit(0);
	  }
	if(swcmcnf || swcscnf)
	  {	if(compile_stdin())		/* Compile input */
			do_ocnf(swcscnf);	/* Output the makecf.fun code */
		exit(0);
	  }
	if(swfdmp)
	  {	if(compile_stdin())		/* Compile input */
			do_ofasc();	/* Output the deffun.e code */
		exit(0);
	  }


	/* Hack user's profile */
	if(!uproflg) exit(0);
	if(!argfile)
	  {
		temp[0] = 0;
#if !V6
		if (cp = getenv("HOME"))
			strcat(temp, cp);
#if !TOPS20
		strcat(temp,"/");
#endif /*-TOPS20*/
#endif /*-V6*/
		strcat(temp, EVPROFTEXTFILE);
		if(freopen(temp,"r",stdin) == NULL)
			fatal("cannot open profile \"%s\"",temp);
	  }
	if(!outfile)
	  {
		temp[0] = 0;
#if !V6
		if (cp = getenv("HOME"))
			strcat(temp, cp);
#if !TOPS20
		strcat(temp,"/");
#endif /*-TOPS20*/
#endif /*-V6*/
		strcat(temp, EVPROFBINFILE);
		if(freopen(temp,"wb",stdout) == NULL	/* Try binary 1st */
		  && freopen(temp,"w",stdout) == NULL)
			fatal("cannot open output profile \"%s\"",temp);

	  }
	/* Hack user's profile */
	if(compile_stdin())		/* Compile input profile */
		do_obprof();	/* Output the binary */

}

#define SW_FLG 0
#define SW_VAR 1
#define SW_STR 2
struct swarg {
	char *sw_name;
	long sw_type;
	int *sw_avar;
	char **sw_astr;
} swtab[] = {
	"P",		SW_FLG, &swprof,	0,	/* Old stuff */
	"E",		SW_FLG,	&swelle,	0,
	"Profile",	SW_FLG, &uproflg,	0,
	"Pconf",	SW_FLG, &swpcnf,	0,
	"Fconf",	SW_FLG, &swfcnf,	0,
	"FXconf",	SW_FLG, &swfxcnf,	0,
	"CMconf",	SW_FLG, &swcmcnf,	0,
	"CSconf",	SW_STR, 0,		&swcscnf,
	"Allconf",	SW_FLG, &swallc,	0,
	"Pdump",	SW_FLG,	&swprof,	0,
	"Fdump",	SW_FLG, &swfdmp,	0
};

doargs(argc,argv)
int argc;
char **argv;
{	register int cnt, c;
	register char **av;
	register int i;
	register struct swarg *swp;
	struct swarg *swp2;
	int swerrs = 0;

	av = argv;
	cnt = argc;
	nfiles = 0;

	while(--cnt > 0)
	  {	++av;
		if(*av[0] != '-')	/* If not switch, */
		  {			/* assume it's an input filename */
			nfiles++;
			continue;
		  }
		av[0]++;

		/* Try to look up switch in table */
		swp = (struct swarg *)findkey(av[0], &swp2, swtab,
			(sizeof(swtab))/(sizeof(struct swarg)),
			(sizeof(struct swarg))/(sizeof(char *)));
		if(swp2)
		  {	fprintf(stderr,"ellec: ambiguous switch: -%s = %s or %s\n",
				av[0], swp->sw_name, swp2->sw_name);
			goto swdone;
		  }
		if(swp)	switch(swp->sw_type)
		  {	case SW_FLG:
				*(swp->sw_avar) = 1;
				goto swdone;
			case SW_VAR:
				*(swp->sw_avar) = 1;
				if(cnt <= 1) goto swdone;
				if(isdigit(*av[1]))
				  {	*(swp->sw_avar) = atoi(av[1]);
					--cnt;
					goto swargdone;
				  }
				goto swdone;
				
			case SW_STR:
				if(cnt <= 1) goto swdone;
				*(swp->sw_astr) = av[1];
				goto swargdone;

			default:
				fprintf(stderr,"ellec: bad switch type: %s\n",
					av[0]);
				swerrs++;
		  }

	stop:	fprintf(stderr,"ellec: bad switch: %s\n",*av);
		swerrs++;
		goto swdone;
	swargdone:
		av[0] = 0;
		av++;
	swdone:	av[0] = 0;
	  }
	if(swerrs) exit(1);		/* Stop if any problems */
}

char **
findkey(cp, aretp, tabp, tabsiz, elsize)
register char *cp;
char ***aretp;
register char **tabp;
int tabsiz, elsize;
{	register char **mp1, **mp2;
	register int i, res;
	
	*aretp = mp1 = mp2 = 0;
	for(i = 0; i < tabsiz; ++i, tabp += elsize)
	  { if(res = ustrcmp(cp,*tabp))
		  {	if(res > 0) return(tabp);
			if(!mp1) mp1 = tabp;
			else mp2 = tabp;
		  }
	  }
	if(mp2)
		*aretp = mp2;		/* Ambiguous */
	return(mp1);
}

/* NSTRCMP - New string compare.
 *	Returns:
 *		2 if str2 > str1 (mismatch)
 *		1 if str2 counted out (str1 > str2)
 *		0 if full match
 *		-1 if str1 counted out (str1 < str2)
 *		-2 if str1 < str2 (mismatch)
 */

nstrcmp(s1,s2)
register char *s1, *s2;
{	register int c, d;

	while(c = *s1++)
	  {	if(c != *s2)
		  {	if((d = upcase(c) - upcase(*s2)) != 0)
				return(*s2==0 ? 1 : (d > 0 ? 2 : -2));
		  }
		++s2;
	  }
	return(*s2 ? -1 : 0);
}

/* USTRCMP - uppercase string compare.
 *	Returns 0 if mismatch,
 *		1 if full match,
 *		-1 if str1 runs out first (partial match)
 */
ustrcmp(s1,s2)
register char *s1, *s2;
{	register int c;

	if ( ! s1 || ! s2 ) return ( 0 );	/* Check for null ptr */
	while(c = *s1++)
	  { if(c != *s2)
		  {	if(((c ^ *s2) != 040)
			 || (upcase(c) != upcase(*s2)))
				return(0);
		  }
		s2++;
	  }
	return(c == *s2 ? 1 : -1);
}

strueq(s1,s2)
char *s1;
char *s2;
{	return (ustrcmp(s1, s2) > 0 ? 1 : 0);
}

/* Output C initialization code for default profile (defprf.c) */

do_opcod()
{	register int i, c, f;

	printf("\
/* This file defines the initial data for ELLE's default user profile.\n\
** It is automatically generated by ELLEC, and should not be edited.\n\
*/\n\
char charmap[] = {\n");
	for(i=0; i < chrcnt; i++)
	  {	printf("\t%2d,",(f = chrptr[i]&0377));
		printf("\t/* (%3o) %3s",i,charep(i));
		printf("  %s",funname(f));
		printf(" */\n");
	  }

	printf("};\n char metamap[] = {\n");
	for(i = 0; i < mtacnt; i += 2)
	  {	printf("\t0%-3o,%3d,",(c = mtaptr[i]&0377),(f = mtaptr[i+1]&0377));
		printf("\t/* %4s",charep(c|CB_META));
		printf("  %s",funname(f));
		printf(" */\n");
	  }

	printf("};\n char extmap[] = {\n");
	for(i = 0; i < extcnt; i += 2)
	  {	printf("\t0%-3o,%3d,",(c = extptr[i]&0377),(f = extptr[i+1]&0377));
		printf("\t/* %4s",charep(c|CB_EXT));
		printf("  %s",funname(f));
		printf(" */\n");
	  }
	printf("};\n");
	printf("struct profile def_prof = {\n");
	printf("  %d, /* Ver */\n", format_ver);
	printf("  sizeof(charmap),   charmap,\n");
	printf("  sizeof(metamap)/2, metamap,\n");
	printf("  sizeof(extmap)/2,  extmap, \n");
	printf("  0, 0\n");
	printf("};\n");

}

/* Output ASCII version of default profile */

int oerrs;

do_opasc()
{	register int i, c, f;

	oerrs = 0;
	printf("; ELLE default ASCII profile\n\n");
	printf("(keyallunbind)  ; To flush all existing bindings\n\n");
	for(i=0; i < chrcnt; i++)
		outkbind(i, chrptr[i]&0377);

	printf("\n; Meta chars\n\n");
	for(i = 0; i < mtacnt; i += 2)
		outkbind(CB_META | (mtaptr[i]&0377), mtaptr[i+1]&0377);

	printf("\n ; Extended commands\n\n");
	for(i = 0; i < extcnt; i += 2)
		outkbind(CB_EXT | (extptr[i]&0377), extptr[i+1]&0377);

	printf("\n");
	if(oerrs)
		fatal("%d errors encountered, check output file.", oerrs);
}

outkbind(c, fx)
{
	if(fx == 0)		/* Allow key to be mapped to nothing. */
		return;
	if(fx <= 0 || fx > efxmax)
		printf(";INTERNAL ERROR: Bad function index %d for key %s\n",
			fx, charep(c));
	else if(efuntab[fx].ef_name == NULL)
		printf(";INTERNAL ERROR: No name for function %d while mapping key %s\n",
			fx, charep(c));
	else {
	  	printf("(keybind %s \"%s\")\n",
			qstr(charep(c)),efuntab[fx].ef_name);
		return;
	  }
	oerrs++;
}

/* Output binary user profile */

do_obprof()
{	register unsigned int rp;	/* Relative "pointer" */
	struct stored_profile st_prof;

	rp = sizeof(st_prof);		/* Initialize */

	/* Fixed by Kochin Chang, July 1995 */
	/* format version should be the first field in compiled profile */
	prof_pack(st_prof.version, format_ver);

	prof_pack(st_prof.chrvec, rp);
	prof_pack(st_prof.chrvcnt, chrcnt);
	rp += chrcnt;

	prof_pack(st_prof.metavec, rp);
	prof_pack(st_prof.metavcnt, mtacnt/2);
	rp += mtacnt;

	prof_pack(st_prof.extvec, rp);
	prof_pack(st_prof.extvcnt, extcnt/2);
	rp += extcnt;

	prof_pack(st_prof.menuvec, rp);
	prof_pack(st_prof.menuvcnt, mnucnt);
	rp += mnucnt;

	fwrite((char *)&st_prof,sizeof(st_prof),1,stdout);
	fwrite(chrptr,sizeof(char),chrcnt,stdout);
	if(mtacnt)  fwrite(mtaptr, sizeof(*mtaptr), mtacnt, stdout);
	if(extcnt)  fwrite(extptr, sizeof(*extptr), extcnt, stdout);
	if(mnucnt) fwrite(mnuptr,sizeof(*mnuptr),mnucnt,stdout);
}

/* Return upper-case version of character */
mupcase(ch)
register int ch;
{	return((ch&(~0177)) | upcase(ch));
}
upcase (ch)
{	register int c;
	c = ch&0177;
	return((('a' <= c) && (c <= 'z')) ? (c - ('a'-'A')) : c);
}

char *
qstr(str)
register char *str;
{	register int c;
	static char qstrbuf[100];
	register char *cp;
	cp = str;
	while((c = *cp++) && islword(c));
	if(c == 0) return(str);	/* No quoting needed */

	cp = qstrbuf;
	*cp++ = '"';
	while(*cp++ = c = *str++)
		if(c == '"') *cp++ = c;	/* Double the quotes */
	cp[-1] = '"';
	*cp = 0;
	return(qstrbuf);
}

char *
charep(c)
register int c;
{	static char chrbuf[10];
	register char *cp;
	cp = chrbuf;
	if(c&CB_EXT)
	  {	*cp++ = 'X';
		*cp++ = '-';
		c &= ~CB_EXT;
	  }
	if(c&CB_META)
	  {	*cp++ = 'M';
		*cp++ = '-';
		c &= ~CB_META;
	  }
	if(c <= 037)
	  {	*cp++ = '^';
		c += 0100;
	  }
	if(c == 0177)
	  {	*cp++ = 'D';
		*cp++ = 'E';
		*cp++ = 'L';
	  }
	else *cp++ = c;
	*cp = 0;
	return(chrbuf);
}

/* Output config Makefile (makecf.fun)
 *	If argument is 0 (NULL), does Makefile type output.
 *	Otherwise uses string for special-purpose output.
 */
do_ocnf(str)
char *str;
{	register struct fun *fnp;
	register int i, mi;
	register char *cp;
	int fmtab[EFUNMAX];
	int nfmods;
	char *modtab[EFUNMAX];
	char *unknown = "unknown-module";

	if(str == NULL)			/* If not V6 version */
	  {	printf("# Function module definition file, generated by ELLEC\n");
		printf("FUN_OFILES = ");
	  }

	nfmods = 0;

	funcnt(fmtab);		/* Count function occs */

	for(i = 1; i <= efxmax; ++i)
	  {	if(fmtab[i] == 0) continue;
		fnp = &efuntab[i];

		if(fnp->ef_name == 0)
			fatal("internal error - no name for function %d", i);
		
		/* Got a function, store its module name if not a dup */
		if ((cp = fnp->ef_mod) == NULL)	/* Substitute if undef */
			cp = unknown;	
		for(mi=0; mi < nfmods; ++mi)
			if(ustrcmp(cp, modtab[mi]) > 0)
				break;
		if(mi < nfmods) continue;
		modtab[nfmods++] = cp;
	  }

	/* Now have table of all modules used.  Crunch them into output. */
	for(mi=0; mi < nfmods; ++mi)
	    if (modtab[mi] != unknown)
	      {	if(str != NULL)		/* V6 version? */
			printf("%s %s\n", str, modtab[mi]);
		else printf("\\\n\t%s.o", modtab[mi]);
	      }
	printf("\n");
}

/* Output eefdef.h */

do_ofcod()
{	register struct fun *fnp;
	register int i;
	char temp[40];
	int fmtab[EFUNMAX];

	printf("/* .H Function Definition file, generated by ELLEC */\n");
	printf("/*   0 */ EFUNHOLE /* Always undefined */\n");

	funcnt(fmtab);		/* Count function occs */

	for(i = 1; i <= efxmax ; ++i)
	  {
		fnp = &efuntab[i];
		printf("/* %3d */ ", i);
		if(fmtab[i] == 0 || fnp->ef_name == 0)
			printf("EFUNHOLE\n");
		else
		  {	sprintf(temp, "\"%s\"", fnp->ef_adr);
			printf("EFUN( %-12s, %-14s, \"%s\")\n", fnp->ef_adr,
				temp, fnp->ef_name);
		  }
	  }

}

/* Output ascii version of function def file
*/
do_ofasc()
{	register struct fun *fnp;
	register int i;
	register char *fa, *fm;

	printf("; Master Function Definition file\n");

	for(i = 1; i <= efxmax ; ++i)
	  {
		fnp = &efuntab[i];
		if(fnp->ef_idx == 0)	/* No definition for this index? */
			continue;
		if(fnp->ef_name == 0)
		  {	warn("internal error - no name for function %d", i);
			continue;
		  }

		if ((fa = fnp->ef_adr) == NULL)
			fa = "unknown-addr";
		if ((fm = fnp->ef_mod) == NULL)
			fm = "unknown-module";
		printf("(efun %d \"%s\" %s %s)\n",
			fnp->ef_idx, fnp->ef_name, fa, fm);
	  }
}

/* Output eefidx.h */

do_ofxcod()
{	register struct fun *fnp;
	register int i;
	register char *cp, *cp2;
	int fmtab[EFUNMAX];
	char tmpstr[100];

	printf("\
/* .H Function Index Definition file, generated by ELLEC */\n");
	printf("\
/* FN_ defines Function Numbers (indices) for all known functions */\n");
	printf("\
/* FX_ defines Function eXistence in this ELLE configuration */\n");

	funcnt(fmtab);		/* Count function occs */

	for(i = 1; i <= efxmax ; ++i)
	  {
		fnp = &efuntab[i];
		if(fnp->ef_idx == 0)	/* No definition for this index? */
			continue;
		if(fnp->ef_adr == 0 || fnp->ef_name == 0)
		  {	warn("internal error - no addr/name for function %d", i);
			continue;
		  }

		cp2 = fnp->ef_adr;
		cp = tmpstr;
		while(*cp++ = upcase(*cp2++));
		cp = tmpstr;
		if((*cp++ != 'F') || (*cp++ != '_'))
			cp = tmpstr;

		/* Always define FN_ as index */
		printf("#define FN_%-14s %3d /* %s */\n",
				cp, i, fnp->ef_name);
		/* Define FX_ as 0 if unused, else same as FN_ */
		printf("#define FX_%-14s %3d\n", cp,
			(fmtab[i] == 0) ? 0 : i);	/* 0 if unused */
	  }

}

/* Compile input! */

compile_stdin()
{	register struct lnode *lp;

	while((lp = lread()) != NIL)
		eval(lp);

	return(1);
}

#define MAXLINE 300
int llstch = -1;
int leofflg;
#define unlrch(c) llstch = c

int lineno = 0;
char linebuf[MAXLINE];
char *linecp = linebuf;

lrch()
{	register int c;
	if((c = llstch) >= 0)
	  {	if(c == 0 && leofflg)
			return(EOF);
		llstch = -1;
		return(c);
	  }
	if((c = getc(stdin)) == EOF)
	  {	leofflg = 1;
		llstch = 0;
		*linecp = 0;
		linecp = linebuf;
		return(c);
	  }
	if(c == '\n')
	  {	lineno++;
		linecp = linebuf;
	  }
	else *linecp++ = c;
	return(c);
}

struct lnode *
lread()
{	register int c;
	register struct lnode *lp, *lp2;
	struct lnode *head;

	wspfls();
	if((c = lrch())== EOF)
		return(NIL);
	if(c == ')')	/* End of a list? */
		return(NIL);
	if(c == '(')	/* Start of a list? */
	  {	head = lp = getln();
		lp->ltyp = LT_LIST;
		if((head->lval.lvl = lp = lread()) == NIL)
			return(head);	/* Return empty list */
		while(lp2 = lread())
		  {	lp->lnxt = lp2;
			lp = lp2;
		  }
		return(head);
	  }

	/* Atom of some kind */
	if(c=='"')
	  {	return(lrstr(1));
	  }
	unlrch(c);
	return(lrstr(0));
}

wspfls()
{	register int c;
	for(;;)
	  {	c = lrch();
		if(isspace(c)) continue;
		if(c == ';')
			while((c = lrch()) != '\n')
				if(c == EOF) return;		
		break;
	  }		
	if(c != EOF) unlrch(c);
}

#define LSMAX 300	/* Max # chars in atom string */
struct lnode *
lrstr(flg)
{	char cbuf[LSMAX];
	register char *cp;
	register int c, i;
	struct lnode *lp;

	cp = cbuf;
	i = 0;

	while((c = lrch()) != EOF)
	  if (flg) switch(c)
	  {	case '"':
			if((c = lrch()) == EOF)
				return(NIL);
			if(c != '"')
			  {	unlrch(c);
				goto out;
			  }
		default:
		ok:
			if(++i > LSMAX)
				break;
			*cp++ = c;
			continue;
	  }
	else
	  {	if(islword(c)) goto ok;
		unlrch(c);
		break;
	  }
  out:
	lp = getln();
	lp->ltyp = LT_STR;
	lp->lval.lvs = malloc(i+1);
	*cp = 0;
	strcpy(lp->lval.lvs, cbuf);
	return(lp);
}
islword(c)
{	return((040 < c && c < 0177
		&& c != '(' && c !=')' && c != ';'
		&& c != '"' && c != '\\') ? 1 : 0);
}


struct lnode *keybind(), *keyallun(), *menuitem(), *efun(),
	*undefall();

struct lfun {
	char *lfname;			/* Name of list function */
	struct lnode * (*lfrtn)();	/* Function address */
} lfntab[] = {
	"keybind",	keybind,
	"efun",		efun,
	"menuitem",	menuitem,
	"keyallunbind",	keyallun,
/*	"keyunbind",	keyunbind,	*/	/* Not yet */
	"undefall",	undefall,
/*	"undef",	undef,		*/	/* Not yet */
	0, 0
};

struct lnode *
eval(lp)
register struct lnode *lp;
{	register struct lnode *flp;
	register struct lfun *lfent;

	if(lp->ltyp != LT_LIST)
		return(lp);
	if((flp = lp->lval.lvl) == NIL)
		return(NIL);
	if(flp->ltyp != LT_STR)
		return(NIL);

	/* Look up list function and invoke it */
	for(lfent = lfntab; lfent->lfname; lfent++)
		if(strueq(flp->lval.lvs, lfent->lfname))
			return((*(lfent->lfrtn))(flp->lnxt));

	lerr("unknown op: (%s)", flp->lval.lvs);
	return(NIL);
}


/* UNDEFALL - (undefall)
**	Undefines all functions.  Typically used to clear out
** predefined functions prior to compiling a set of new efuns.
*/
struct lnode *
undefall(lp)
register struct lnode *lp;
{
	register int i;
	efxmax = 0;		/* Say nothing in function def table! */
	for(i = 0; i < EFUNMAX; ++i)
	  {	efuntab[i].ef_idx = 0;
		efuntab[i].ef_name = 0;
		efuntab[i].ef_adr = 0;
		efuntab[i].ef_mod = 0;
	  }
	return(LTRUE);
}

/* EFUN - (efun <index> <functionname> <address> <module>)
**	Checks out the args and if no problems, stores the function
** definition in efuntab.
*/
struct lnode *
efun(lp)
register struct lnode *lp;
{	struct lnode *nlp;
	register int c, i;
	register struct fun *fnp;
	char *fname, *faddr, *fmod;
	int fni, num;

	if(listcnt(lp) < 4)
	  {	lerr("efun - not enough args");
		return(NIL);
	  }

	/* First thing should be function index */
	switch(lp->ltyp)
	  {	case LT_VAL:
			fni = lp->lval.lvi;
			break;
		case LT_STR:
			if(numcvt(lp->lval.lvs, &num))
			  {	fni = num;
				break;
			  }
		default:
			lerr("efun - non-value function index");
			return(NIL);
	  }

	/* Next thing should be function name */
	lp = lp->lnxt;
	if(lp->ltyp != LT_STR)	/* Function name not a string */
	  {	lerr("efun - non-string function name");
		return(NIL);
	  }
	fname = lp->lval.lvs;

	/* Next thing should be function addr */
	lp = lp->lnxt;
	if(lp->ltyp != LT_STR)	/* Function addr not a string */
	  {	lerr("efun - non-string function addr");
		return(NIL);
	  }
	faddr = lp->lval.lvs;

	/* Next thing should be function module */
	lp = lp->lnxt;
	if(lp->ltyp != LT_STR)	/* Function module not a string */
	  {	lerr("efun - non-string function module");
		return(NIL);
	  }
	fmod = lp->lval.lvs;

	/* Now see if already exists or anything */
	if(fni <= 0 || fni > EFUNMAX)
	  {	lerr("efun - bad function index %d", fni);
		return(NIL);
	  }
	fnp = &efuntab[fni];
	if(fnp->ef_idx != 0)
	  {
		if (fnp->ef_idx == fni
		 && strueq(fnp->ef_name, fname)
		 && strueq(fnp->ef_adr, faddr)
		 && (fnp->ef_mod == NULL || strueq(fnp->ef_mod, fmod)))
			goto win;		/* Benign redefinition */

lerr("efun - redefining function (%d \"%s\" %s %s)",
		fnp->ef_idx, fnp->ef_name, fnp->ef_adr,
		(fnp->ef_mod ? fnp->ef_mod : "unknown-module"));
	  }
	for(i = 0; i < EFUNMAX; ++i)
	  {	if(efuntab[i].ef_idx == 0) continue;
		if(ustrcmp(efuntab[i].ef_adr,faddr) > 0
		  || ustrcmp(efuntab[i].ef_name, fname) > 0)
		  {	if(i == fni) continue;
			lerr("efun - name or address dup!  \"%s\"", fname);
			return(NIL);
		  }
	  }

	/* No problems, store the function def in efuntab! */
win:	fnp->ef_idx = fni;
	fnp->ef_mod = fmod;
	fnp->ef_adr = faddr;
	fnp->ef_name = fname;

	if(efxmax < fni) efxmax = fni;
	return(LTRUE);
}


/* KEYBIND - (keybind <charspec> <functionname>) */

struct lnode *
keybind(lp)
register struct lnode *lp;
{	struct lnode *nlp;
	register int c, i;
	int fni;

	if(lp == NIL || (nlp = lp->lnxt)== NIL)
		return(NIL);
	switch(lp->ltyp)
	  {	case LT_VAL:
			c = lp->lval.lvi;
			break;
		case LT_LIST:
			return(NIL);
		case LT_STR:
			c = repchar(lp->lval.lvs);
			break;
	  }
	if(c == -1)
		return(NIL);	/* No such command char name */

	lp = nlp;
	if(lp->ltyp != LT_STR)	/* Function name not a string */
	  {	lerr("(keybind) non-string function name");
		return(NIL);
	  }
	fni = findfun(lp->lval.lvs);
	if(fni == 0)		/* No such function name */
	  {	lerr("(keybind) no such function - \"%s\"", lp->lval.lvs);
		return(NIL);
	  }
	if(c & CB_EXT)
	  {	c &= ~CB_EXT;

		/* Check for redefinition */
		for(i = 0; i < extcnt; i += 2)
			if(c == (extptr[i]&0377))	/* Already there? */
			  {	if((extptr[i+1]&0377) != fni)	/* Yes, check fn */
				    lerr("(keybind) redefining X-%s as %d=\"%s\"",
					charep(c), fni, lp->lval.lvs);
				break;
			  }
		if(i >= extcnt)		/* Didn't find? */
		  {	if(extcnt >= extsiz)
			  {	lerr("(keybind) too many X- commands");
				return(NIL);	/* Too many EXT cmds */
			  }
			i = extcnt;	/* Increase size of table */
			extcnt += 2;
		  }
		/* Now store new binding */
		extptr[i] = c;
		extptr[i+1] = fni;
	  }
	else if(c&CB_META)
	  {	c &= ~CB_META;

		/* Check for redefinition */
		for(i = 0; i < mtacnt; i += 2)
			if(c == (mtaptr[i]&0377))	/* Already there? */
			  {	if((mtaptr[i+1]&0377) != fni)	/* Yes, check fn */
				    lerr("(keybind) redefining M-%s as %d=\"%s\"",
					charep(c), fni, lp->lval.lvs);
				break;
			  }
		if(i >= mtacnt)		/* Didn't find? */
		  {	if(mtacnt >= mtasiz)
			  {	lerr("(keybind) too many M- commands");
				return(NIL);	/* Too many META cmds */
			  }
			i = mtacnt;	/* Increase size of table */
			mtacnt += 2;
		  }
		/* Now store new binding */
		mtaptr[i] = c;
		mtaptr[i+1] = fni;
	  }
	else {
		i = c & 0177;
		if (chrptr[i] && (chrptr[i]&0377) != fni)
		    lerr("(keybind) redefining %s as %d=\"%s\"",
			charep(c), fni, lp->lval.lvs);
		chrptr[i] = fni;
	  }
	return(LTRUE);
}

/* KEYALLUNBIND - (keyallunbind) */
struct lnode *
keyallun()
{	register int i;
	register char *cp;

/*	fprintf(stderr, "ellec: clearing all key definitions\n"); */
	for(i = 0, cp = chrptr; i < chrcnt; i++)
		*cp++ = 0;
	mtacnt = extcnt = mnucnt = 0;
	return(LTRUE);
}

/* MENUITEM - (menuitem <functionname>) */

struct lnode *
menuitem(lp)
register struct lnode *lp;
{	register int i, fni;

	if(lp == NIL)
		return(NIL);
	switch(lp->ltyp)
	  {	case LT_VAL:
			fni = lp->lval.lvi;
			break;
		case LT_LIST:
			return(NIL);
		case LT_STR:
			fni = findfun(lp->lval.lvs);
			break;
	  }
	if(fni == 0) return(NIL);	/* Bad val or no such function name */
	for(i = 0; i < mnusiz; i++)
		if(fni == (mnuptr[i]&0377) || mnuptr[i] == 0)
		  {	mnuptr[i] = fni;
			mnucnt++;
			return(LTRUE);
		  }
	return(NIL);		/* Too many menu items */
}

repchar(str)
register char *str;
{	register int c;
	register int i, l;

	if (str == 0) return (-1);
	i = 0;
	l = strlen(str);
	c = (*str++)&0377;
	if(l == 0) return(-1);
	if(l == 1) return(c);	/* One-char representation */
	if(c == '^')
		if(l == 2) return((~0140) & mupcase(*str));
		else return(-1);
	c = mupcase(c);
	if (*str == '-')
	  {	if(*++str == 0) return(-1);
		switch(c)
		  {	case 'X': return(CB_EXT | mupcase(repchar(str)));
			case 'M': return(CB_META | mupcase(repchar(str)));
			case 'C': return((~0140) & repchar(str));
		  }
	  }
	if(c == 'S' && upcase(*str) == 'P' && l == 2)
		return(' ');
	if(c == 'D' && upcase(*str++) == 'E' && upcase(*str++) == 'L'
		&& *str == 0)
		return(0177);
	return(-1);		
}

struct lnode *
getln()
{	return((struct lnode *)calloc(1,sizeof(struct lnode)));
}

numcvt(str, anum)
char *str;
int *anum;
{	register char *cp;
	register int i, c, sign;
	if((cp = str) == 0)
		return 0;
	i = sign = 0;
	if(*cp == '-')
		cp++, sign++;
	while(c = *cp++)
		if(!isdigit(c)) return(0);
		else i = 10*i + (c - '0');
	*anum = sign ? -i : i;
	return(1);
}



listcnt(lp)
register struct lnode *lp;
{	register int i;
	i = 0;
	while(lp)
		++i, lp = lp->lnxt;
	return(i);
}

/* FUNNAME - Given function index, return function name.
**	Always wins; returns "unknown" for bad indices.
*/
char *
funname(i)
register int i;
{
	register char *cp = NULL;
	if(0 < i && i <= efxmax && (cp = efuntab[i].ef_name))
		return cp;
	return("unknown function");
}

findfun(name)
register char *name;
{	register int i;
	if((i = efxmax) > 0)
	  {	do { if(strueq(name, efuntab[i].ef_name))
			return(i);
		  } while(--i);
		return(0);
	  }
	return(0);
}


/* FUNCNT - Scan all key bindings, counting each occurrence of every
**	function index.
**	This is used to determine which functions are actually used.
*/
funcnt(arr)
register int *arr;		/* Pointer to array of EFUNMAX ints */
{
	register int i;

	for(i = 0; i < EFUNMAX; ++i)	/* Clear the array */
		arr[i] = 0;
	
	for(i = 0; i < chrcnt; ++i)	/* Scan bindings */
		arr[chrptr[i]&0377]++;
	for(i = 0; i < mtacnt; i += 2)
		arr[mtaptr[i+1]&0377]++;
	for(i = 0; i < extcnt; i += 2)
		arr[extptr[i+1]&0377]++;
}

scpy(from,to,cnt)
register char *from,*to;
register int cnt;
{	if(cnt > 0)
		do { *to++ = *from++; }
		while(--cnt);
}

/* STRIPSP - strip spaces from string.  Returns ptr to start. */
char *
stripsp(cp)
register char *cp;
{
	register char *ep, *lastp;
	while(*cp == ' ') ++cp;
	if (*cp)
	  {	ep = cp + strlen(cp);	/* Point to null ending the str */
		while (*--ep == ' ');
		*++ep = 0;		/* Tie it off */
	  }
	return cp;
}

warn(str,a,b,c,d,e,f,g,h,i)
char *str;
{
	fprintf(stderr, "ellec: ");
	fprintf(stderr, str, a,b,c,d,e,f,g,h,i);
	fprintf(stderr, "\n");
}

lerr(str,a,b,c,d,e,f,g,h,i)
char *str;
{
	warn(str, a,b,c,d,e,f,g,h,i);
	*linecp = 0;			/* Tie off current line buffer */
	fprintf(stderr, "    Line %d: %s\n", lineno, linebuf);
}

fatal(str,a,b,c,d,e,f,g,h,i)
char *str;
{
	warn(str, a,b,c,d,e,f,g,h,i);
	exit(1);
}

