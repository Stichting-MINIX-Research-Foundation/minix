/* sedcomp.c -- stream editor main and compilation phase
   Copyright (C) 1995-2003 Eric S. Raymond
   Copyright (C) 2004-2006 Rene Rebe

   The stream editor compiles its command input  (from files or -e options)
into an internal form using compile() then executes the compiled form using
execute(). Main() just initializes data structures, interprets command line
options, and calls compile() and execute() in appropriate sequence.
   The data structure produced by compile() is an array of compiled-command
structures (type sedcmd).  These contain several pointers into pool[], the
regular-expression and text-data pool, plus a command code and g & p flags.
In the special case that the command is a label the struct  will hold a ptr
into the labels array labels[] during most of the compile,  until resolve()
resolves references at the end.
   The operation of execute() is described in its source module. 
*/

#include <stdlib.h>		/* exit */
#include <stdio.h>		/* uses getc, fprintf, fopen, fclose */
#include <ctype.h>		/* isdigit */
#include <string.h>		/* strcmp */
#include "sed.h"		/* command type struct and name defines */

/***** public stuff ******/

#define MAXCMDS		200	/* maximum number of compiled commands */
#define MAXLINES	256	/* max # numeric addresses to compile */ 

/* main data areas */
char	linebuf[MAXBUF+1];	/* current-line buffer */
sedcmd	cmds[MAXCMDS+1];	/* hold compiled commands */
long	linenum[MAXLINES];	/* numeric-addresses table */

/* miscellaneous shared variables */ 
int	nflag;			/* -n option flag */
int	eargc;			/* scratch copy of argument count */
sedcmd	*pending	= NULL;	/* next command to be executed */

int	last_line_used = 0;	/* last line address ($) was used */

void die (const char* msg) {
	fprintf(stderr, "sed: ");
	fprintf(stderr, msg, linebuf);
	fprintf(stderr, "\n");
	exit(2);
}

/***** module common stuff *****/

#define POOLSIZE	10000	/* size of string-pool space */
#define WFILES		10	/* max # w output files that can be compiled */
#define	RELIMIT		256	/* max chars in compiled RE */
#define	MAXDEPTH	20	/* maximum {}-nesting level */
#define	MAXLABS		50	/* max # of labels that can be handled */

#define SKIPWS(pc)	while ((*pc==' ') || (*pc=='\t')) pc++
#define IFEQ(x, v)	if (*x == v) x++ , /* do expression */

/* error messages */
static char	AGMSG[]	= "garbled address %s";
static char	CGMSG[]	= "garbled command %s";
static char	TMTXT[]	= "too much text: %s";
static char	AD1NG[]	= "no addresses allowed for %s";
static char	AD2NG[]	= "only one address allowed for %s";
static char	TMCDS[]	= "too many commands, last was %s";
static char	COCFI[]	= "cannot open command-file %s";
static char	UFLAG[]	= "unknown flag %c";
/*static char	COOFI[]	= "cannot open %s";*/
static char	CCOFI[]	= "cannot create %s";
static char	ULABL[]	= "undefined label %s";
static char	TMLBR[]	= "too many {'s";
static char	FRENL[]	= "first RE must be non-null";
static char	NSCAX[]	= "no such command as %s";
static char	TMRBR[]	= "too many }'s";
static char	DLABL[]	= "duplicate label %s";
static char	TMLAB[]	= "too many labels: %s";
static char	TMWFI[]	= "too many w files";
static char	REITL[]	= "RE too long: %s";
static char	TMLNR[]	= "too many line numbers";
static char	TRAIL[]	= "command \"%s\" has trailing garbage";
static char	RETER[] = "RE not terminated: %s";
static char	CCERR[] = "unknown character class: %s";

/* cclass to c function mapping ,-) */
const char* cclasses[] = {
	"alnum", "a-zA-Z0-9",
	"lower", "a-z",
	"space", " \f\n\r\t\v",
	"alpha", "a-zA-Z",
	"digit", "0-9",
	"upper", "A-Z",
	"blank", " \t",
	"xdigit", "0-9A-Fa-f",
	"cntrl", "\x01-\x1f\x7e",
	"print", " -\x7e",
	"graph", "!-\x7e",
	"punct", "!-/:-@[-`{-\x7e",
	NULL, NULL};
 
typedef struct			/* represent a command label */
{
	char		*name;		/* the label name */
	sedcmd		*last;		/* it's on the label search list */  
	sedcmd		*address;	/* pointer to the cmd it labels */
} label;

/* label handling */
static label	labels[MAXLABS];	/* here's the label table */
static label	*lab	= labels + 1;	/* pointer to current label */
static label	*lablst = labels;	/* header for search list */

/* string pool for regular expressions, append text, etc. etc. */
static char	pool[POOLSIZE];			/* the pool */
static char	*fp	= pool;			/* current pool pointer */
static char	*poolend = pool + POOLSIZE;	/* pointer past pool end */

/* compilation state */
static FILE	*cmdf	= NULL;		/* current command source */
static char	*cp	= linebuf;	/* compile pointer */
static sedcmd	*cmdp	= cmds;		/* current compiled-cmd ptr */
static char	*lastre	= NULL;		/* old RE pointer */
static int	bdepth	= 0;		/* current {}-nesting level */
static int	bcount	= 0;		/* # tagged patterns in current RE */
static char	**eargv;		/* scratch copy of argument list */

/* compilation flags */
static int	eflag;			/* -e option flag */
static int	gflag;			/* -g option flag */

/* prototypes */
static char *address(char *expbuf);
static char *gettext(char* txp);
static char *recomp(char *expbuf, char redelim);
static char *rhscomp(char* rhsp, char delim);
static char *ycomp(char *ep, char delim);
static int cmdcomp(char cchar);
static int cmdline(char	*cbuf);
static label *search(label *ptr);
static void compile(void);
static void resolve(void);

/* sedexec.c protypes */
void execute(char* file);

/* main sequence of the stream editor */
int main(int argc, char *argv[])
{
	eargc	= argc;		/* set local copy of argument count */
	eargv	= argv;		/* set local copy of argument list */
	cmdp->addr1 = pool;	/* 1st addr expand will be at pool start */
	if (eargc == 1)
		exit(0);	/* exit immediately if no arguments */

	/* scan through the arguments, interpreting each one */
	while ((--eargc > 0) && (**++eargv == '-'))
		switch (eargv[0][1])
		{
		case 'e':
			eflag++; compile();	/* compile with e flag on */
			eflag = 0;
			continue;		/* get another argument */
		case 'f':
			if (eargc-- <= 0)	/* barf if no -f file */
				exit(2);
			if ((cmdf = fopen(*++eargv, "r")) == NULL)
			{
				fprintf(stderr, COCFI, *eargv);
				exit(2);
			}
			compile();	/* file is O.K., compile it */
			fclose(cmdf);
			continue;	/* go back for another argument */
		case 'g':
			gflag++;	/* set global flag on all s cmds */
			continue;
		case 'n':
			nflag++;	/* no print except on p flag or w */
			continue;
		default:
			fprintf(stdout, UFLAG, eargv[0][1]);
			continue;
		}

	if (cmdp == cmds)	/* no commands have been compiled */
	{
		eargv--; eargc++;
		eflag++; compile(); eflag = 0;
		eargv++; eargc--;
	}

	if (bdepth)	/* we have unbalanced squigglies */
		die(TMLBR);

	lablst->address = cmdp;	/* set up header of label linked list */
	resolve();		/* resolve label table indirections */
	if (eargc <= 0)		/* if there were no -e commands */
		execute(NULL);	/*   execute commands from stdin only */
	else while(--eargc>=0)	/* else execute only -e commands */
		execute(*eargv++);
	exit(0);		/* everything was O.K. if we got here */
}

#define	H	0x80	/* 128 bit, on if there's really code for command */
#define LOWCMD	56	/* = '8', lowest char indexed in cmdmask */ 

/* indirect through this to get command internal code, if it exists */
static char	cmdmask[] =
{
	0,	0,	H,	0,	0,	H+EQCMD,0,	0,
	0,	0,	0,	0,	H+CDCMD,0,	0,	CGCMD,
	CHCMD,	0,	0,	0,	H+CLCMD,0,	CNCMD,	0,
	CPCMD,	0,	0,	0,	H+CTCMD,0,	0,	H+CWCMD,
	0,	0,	0,	0,	0,	0,	0,	0,
	0,	H+ACMD,	H+BCMD,	H+CCMD,	DCMD,	0,	0,	GCMD,
	HCMD,	H+ICMD,	0,	0,	H+LCMD,	0,	NCMD,	0,
	PCMD,	H+QCMD,	H+RCMD,	H+SCMD,	H+TCMD,	0,	0,	H+WCMD,
	XCMD,	H+YCMD,	0,	H+BCMD,	0,	H,	0,	0,
};

/* precompile sed commands out of a file */
static void compile(void)
{
	char	ccode;

	for(;;)					/* main compilation loop */
	{
		SKIPWS(cp);
		if (*cp == ';') {
			cp++;
			SKIPWS(cp);
		}

		if (*cp == '\0' || *cp == '#')	/* get a new command line */
			if (cmdline(cp = linebuf) < 0)
				break;
		SKIPWS(cp);

		if (*cp == '\0' || *cp == '#')	/* a comment */
			continue;

		/* compile first address */
		if (fp > poolend)
			die(TMTXT);
		else if ((fp = address(cmdp->addr1 = fp)) == BAD)
			die(AGMSG);

		if (fp == cmdp->addr1)		/* if empty RE was found */
		{
			if (lastre)		/* if there was previous RE */
				cmdp->addr1 = lastre;	/* use it */
			else
				die(FRENL);
		}
		else if (fp == NULL)		/* if fp was NULL */
		{
			fp = cmdp->addr1;	/* use current pool location */
			cmdp->addr1 = NULL;
		}
		else
		{
			lastre = cmdp->addr1;
			if (*cp == ',' || *cp == ';')	/* there's 2nd addr */
			{
				cp++;
				if (fp > poolend) die(TMTXT);
				fp = address(cmdp->addr2 = fp);
				if (fp == BAD || fp == NULL) die(AGMSG);
				if (fp == cmdp->addr2)
					cmdp->addr2 = lastre;
				else
					lastre = cmdp->addr2;
			}
			else
				cmdp->addr2 = NULL;	/* no 2nd address */
		}
		if (fp > poolend) die(TMTXT);

		SKIPWS(cp);		/* discard whitespace after address */

		if (*cp == '!') {
			cmdp->flags.allbut = 1;
			cp++; SKIPWS(cp);
		}

		/* get cmd char, range-check it */
		if ((*cp < LOWCMD) || (*cp > '~')
			|| ((ccode = cmdmask[*cp - LOWCMD]) == 0))
				die(NSCAX);

		cmdp->command = ccode & ~H;	/* fill in command value */
		if ((ccode & H) == 0)		/* if no compile-time code */
			cp++;			/* discard command char */
		else if (cmdcomp(*cp++))	/* execute it; if ret = 1 */
			continue;		/* skip next line read */

		if (++cmdp >= cmds + MAXCMDS) die(TMCDS);

		SKIPWS(cp);			/* look for trailing stuff */
		if (*cp != '\0')
		{
			if (*cp == ';')
			{
				continue;
			}
			else if (*cp != '#' && *cp != '}')
				die(TRAIL);
		}
	}
}

/* compile a single command */
static int cmdcomp(char cchar)
{
	static sedcmd	**cmpstk[MAXDEPTH];	/* current cmd stack for {} */
	static const char *fname[WFILES];	/* w file name pointers */
	static FILE	*fout[WFILES];		/* w file file ptrs */
	static int	nwfiles	= 2;		/* count of open w files */
	int		i;			/* indexing dummy used in w */
	sedcmd		*sp1, *sp2;		/* temps for label searches */
	label		*lpt;			/* ditto, and the searcher */
	char		redelim;		/* current RE delimiter */

	fout[0] = stdout;
	fout[1] = stderr;
	
	fname[0] = "/dev/stdout";
	fname[1] = "/dev/stderr";

	switch(cchar)
	{
	case '{':	/* start command group */
		cmdp->flags.allbut = !cmdp->flags.allbut;
		cmpstk[bdepth++] = &(cmdp->u.link);
		if (++cmdp >= cmds + MAXCMDS) die(TMCDS);
		if (*cp == '\0') *cp++ = ';', *cp = '\0';	/* get next cmd w/o lineread */
		return(1);

	case '}':	/* end command group */
		if (cmdp->addr1) die(AD1NG);	/* no addresses allowed */
		if (--bdepth < 0) die(TMRBR);	/* too many right braces */
		*cmpstk[bdepth] = cmdp;		/* set the jump address */
		return(1);

	case '=':			/* print current source line number */
	case 'q':			/* exit the stream editor */
		if (cmdp->addr2) die(AD2NG);
		break;

	case ':':	/* label declaration */
		if (cmdp->addr1) die(AD1NG);	/* no addresses allowed */
		fp = gettext(lab->name = fp);	/* get the label name */
		if ((lpt = search(lab)))	/* does it have a double? */
		{
			if (lpt->address) die(DLABL);	/* yes, abort */
		}
		else	/* check that it doesn't overflow label table */
		{
			lab->last = NULL;
			lpt = lab;
			if (++lab >= labels + MAXLABS) die(TMLAB);
		}
		lpt->address = cmdp;
		return(1);

	case 'b':	/* branch command */
	case 't':	/* branch-on-succeed command */
	case 'T':	/* branch-on-fail command */
		SKIPWS(cp);
		if (*cp == '\0')	/* if branch is to start of cmds... */
		{
			/* add current command to end of label last */
			if ((sp1 = lablst->last)) 
			{
				while((sp2 = sp1->u.link))
					sp1 = sp2;
				sp1->u.link = cmdp;
			}
			else	/* lablst->last == NULL */
				lablst->last = cmdp;
			break;
		}
		fp = gettext(lab->name = fp);	/* else get label into pool */
		if ((lpt = search(lab)))	/* enter branch to it */
		{
			if (lpt->address)
				cmdp->u.link = lpt->address;
			else
			{
				sp1 = lpt->last;
				while((sp2 = sp1->u.link))
					sp1 = sp2;
				sp1->u.link = cmdp;
			}
		}
		else		/* matching named label not found */
		{
			lab->last = cmdp;	/* add the new label */
			lab->address = NULL;	/* it's forward of here */
			if (++lab >= labels + MAXLABS)	/* overflow if last */
				die(TMLAB);
		}
		break;

	case 'a':	/* append text */
	case 'i':	/* insert text */
	case 'r':	/* read file into stream */
		if (cmdp->addr2) die(AD2NG);
	case 'c':	/* change text */
		if ((*cp == '\\') && (*++cp == '\n')) cp++;
		fp = gettext(cmdp->u.lhs = fp);
		break;

	case 'D':	/* delete current line in hold space */
		cmdp->u.link = cmds;
		break;

	case 's':	/* substitute regular expression */
		if (*cp == 0) /* get delimiter from 1st ch */
			die(RETER);
		else
			redelim = *cp++;
		
		if ((fp = recomp(cmdp->u.lhs = fp, redelim)) == BAD)
			die(CGMSG);
		if (fp == cmdp->u.lhs) {	/* if compiled RE zero len */ 
			if (lastre) {
				cmdp->u.lhs = lastre;	/* use the previous one */
				cp++;                   /*   skip delim */
			}
			else
				die(FRENL);
		}
		else				/* otherwise */
			lastre = cmdp->u.lhs;	/*   save the one just found */
		
		if ((cmdp->rhs = fp) > poolend) die(TMTXT);
		if ((fp = rhscomp(cmdp->rhs, redelim)) == BAD) die(CGMSG);
		if (gflag) cmdp->flags.global++;
		while (*cp == 'g' || *cp == 'p' || *cp == 'P' || isdigit(*cp))
		{
			IFEQ(cp, 'g') cmdp->flags.global++;
			IFEQ(cp, 'p') cmdp->flags.print = 1;
			IFEQ(cp, 'P') cmdp->flags.print = 2;
			if(isdigit(*cp))
			{
				if (cmdp->nth)
					break; /* no multiple n args */
				
				cmdp->nth = atoi(cp); /* check 0? */
				while (isdigit(*cp)) cp++;
			}
		}

	case 'l':	/* list pattern space */
	case 'L':	/* dump pattern space */
		if (*cp == 'w')
			cp++;		/* and execute a w command! */
		else
			break;		/* s or L or l is done */

	case 'w':	/* write-pattern-space command */
	case 'W':	/* write-first-line command */
		if (nwfiles >= WFILES) die(TMWFI);
		fname[nwfiles] = fp;
		fp = gettext((fname[nwfiles] = fp, fp));	/* filename will be in pool */
		for(i = nwfiles-1; i >= 0; i--)	/* match it in table */
			if (strcmp(fname[nwfiles], fname[i]) == 0)
			{
				cmdp->fout = fout[i];
				return(0);
			}
		/* if didn't find one, open new out file */
		if ((cmdp->fout = fopen(fname[nwfiles], "w")) == NULL)
		{
			fprintf(stderr, CCOFI, fname[nwfiles]);
			exit(2);
		}
		fout[nwfiles++] = cmdp->fout;
		break;

	case 'y':	/* transliterate text */
		fp = ycomp(cmdp->u.lhs = fp, *cp++);	/* compile translit */
		if (fp == BAD) die(CGMSG);		/* fail on bad form */
		if (fp > poolend) die(TMTXT);		/* fail on overflow */
		break;
	}
	return(0);	/* succeeded in interpreting one command */
}

/* generate replacement string for substitute command right hand side
   rhsp:	place to compile expression to
   delim:	regular-expression end-mark to look for */
static char *rhscomp(char* rhsp, char delim)	/* uses bcount */
{
	register char	*p = cp;

	for(;;)
		/* copy for the likely case it is not s.th. special */
		if ((*rhsp = *p++) == '\\') /* back reference or escape  */
		{
			if (*p >= '0' && *p <= '9') /* back reference */
			{
			dobackref:
				*rhsp = *p++;
				/* check validity of pattern tag */
				if (*rhsp > bcount + '0')
					return(BAD);
				*rhsp++ |= 0x80; /* mark the good ones */
			}
			else /* escape */
			{
				switch (*p) {
					case 'n': *rhsp = '\n'; break;
					case 'r': *rhsp = '\r'; break;
					case 't': *rhsp = '\t'; break;
					default: *rhsp = *p;
				}
				rhsp++; p++;
			}
		}
		else if (*rhsp == delim)	/* found RE end, hooray... */
		{
			*rhsp++ = '\0';		/* cap the expression string */
			cp = p;
			return(rhsp);		/* pt at 1 past the RE */
		}
		else if (*rhsp == '&')		/* special case, convert to backref \0 */
		{
			*--p = '0';
			goto dobackref;
		}
		else if (*rhsp++ == '\0')	/* last ch not RE end, help! */
			return(BAD);
}

/* compile a regular expression to internal form
   expbuf:	place to compile it to
   redelim:	RE end-marker to look for */
static char *recomp(char *expbuf, char redelim)	/* uses cp, bcount */
{
	register char	*ep = expbuf;	/* current-compiled-char pointer */
	register char	*sp = cp;	/* source-character ptr */
	register int	c;		/* current-character pointer */
	char		negclass;	/* all-but flag */
	char		*lastep;	/* ptr to last expr compiled */
	char		*lastep2;	/* dito, but from the last loop */
	char		*svclass;	/* start of current char class */
	char		brnest[MAXTAGS];	/* bracket-nesting array */
	char		*brnestp;	/* ptr to current bracket-nest */
	char		*pp;		/* scratch pointer */
	int 		classct;	/* class element count */
	int		tags;		/* # of closed tags */

	if (*cp == redelim) {		/* if first char is RE endmarker */
	    return(ep);
	}

	lastep = lastep2 = NULL;	/* there's no previous RE */
	brnestp = brnest;		/* initialize ptr to brnest array */
	tags = bcount = 0;		/* initialize counters */

	if ((*ep++ = (*sp == '^')))	/* check for start-of-line syntax */
		sp++;

	for (;;)
	{
		if (*sp == 0) /* no termination */
			die (RETER);
		if (ep >= expbuf + RELIMIT)	/* match is too large */
			return(cp = sp, BAD);
		if ((c = *sp++) == redelim)	/* found the end of the RE */
		{
			cp = sp;
			if (brnestp != brnest)	/* \(, \) unbalanced */
				return(BAD);
			*ep++ = CEOF;		/* write end-of-pattern mark */
			return(ep);		/* return ptr to compiled RE */
		}

		lastep = lastep2;
		lastep2 = ep;

		switch (c)
		{
		case '\\':
			if ((c = *sp++) == '(')	/* start tagged section */
			{
				if (bcount >= MAXTAGS)
					return(cp = sp, BAD);
				*brnestp++ = bcount;	/* update tag stack */
				*ep++ = CBRA;		/* enter tag-start */
				*ep++ = bcount++;	/* bump tag count */
				lastep2 = NULL;
				continue;
			}
			else if (c == ')')	/* end tagged section */
			{
				if (brnestp <= brnest)	/* extra \) */
					return(cp = sp, BAD);
				*ep++ = CKET;		/* enter end-of-tag */
				*ep++ = *--brnestp;	/* pop tag stack */
				tags++;			/* count closed tags */
				for (lastep2 = ep-1; *lastep2 != CBRA; )
					--lastep2; /* FIXME: lastep becomes start */
				continue;
			}
			else if (c >= '1' && c <= '9' && c != redelim)	/* tag use, if !delim */
			{
				if ((c -= '1') >= tags)	/* too few */
					return(BAD);
				*ep++ = CBACK;		/* enter tag mark */
				*ep++ = c;		/* and the number */
				continue;
			}
			else if (c == '\n')	/* escaped newline no good */
				return(cp = sp, BAD);
			else if (c == 'n')		/* match a newline */
				c = '\n';
			else if (c == 't')		/* match a tab */
				c = '\t';
			else if (c == 'r')		/* match a return */
				c = '\r';
			else if (c == '+') /* 1..n repeat of previous pattern */
			{
			  if (lastep == NULL)	/* if + not first on line */
				goto defchar;	/*   match a literal + */
			  pp = ep;		/* else save old ep */
			  *ep++ = *lastep++ | STAR;	/* flag the copy */
			  while (lastep < pp)	/* so we can blt the pattern */
				*ep++ = *lastep++;
			  lastep2 = lastep;       /* no new expression */
			  continue;
			}
			goto defchar;		/* else match \c */

		case '\0':	/* ignore nuls */
			continue;

		case '\n':	/* trailing pattern delimiter is missing */
			return(cp = sp, BAD);

		case '.':	/* match any char except newline */
			*ep++ = CDOT;
			continue;

		case '*':	/* 0..n repeat of previous pattern */
			if (lastep == NULL)	/* if * isn't first on line */
				goto defchar;	/*   match a literal * */
			*lastep |= STAR;	/* flag previous pattern */
			lastep2 = lastep;	/* no new expression */
			continue;

		case '$':	/* match only end-of-line */
			if (*sp != redelim)	/* if we're not at end of RE */
				goto defchar;	/*   match a literal $ */
			*ep++ = CDOL;		/* insert end-symbol mark */
			continue;

		case '[':	/* begin character set pattern */
			if (ep + 17 >= expbuf + RELIMIT)
				die(REITL);
			*ep++ = CCL;		/* insert class mark */
			if ((negclass = ((c = *sp++) == '^')))
				c = *sp++;
			svclass = sp;		/* save ptr to class start */
			do {
				if (c == '\0') die(CGMSG);
				/* handle predefined character classes */
				if (c == '[' && *sp == ':')
				{
				  /* look for the matching ":]]" */
				  char *p;
				  const char *p2;
				  for (p = sp+3; *p; p++)
				    if  (*p == ']' &&
				         *(p-1) == ']' &&
					 *(p-2) == ':')
					{
					  char cc[8];
					  const char **it;
					  p2 = sp+1;
					  for (p2 = sp+1;
					       p2 < p-2 && p2-sp-1 < sizeof(cc);
					       p2++)
					    cc[p2-sp-1] = *p2;
					  cc[p2-sp-1] = 0; /* termination */

					  it = cclasses;
					  while (*it && strcmp(*it, cc))
						it +=2;
					  if (!*it++)
					    die(CCERR);

					  /* generate mask */
					  p2 = *it;
					  while (*p2) {
					    if (p2[1] == '-' && p2[2]) {
						for (c = *p2; c <= p2[2]; c++)
                                		  ep[c >> 3] |= bits(c & 7);
						p2 += 3;
					    }
					    else {
						c = *p2++;
					  	ep[c >> 3] |= bits(c & 7);
					    }
					  }
					  sp = p; c = 0; break;
					}
				}

				/* handle character ranges */
				if (c == '-' && sp > svclass && *sp != ']')
					for (c = sp[-2]; c < *sp; c++)
						ep[c >> 3] |= bits(c & 7);

				/* handle escape sequences in sets */
				if (c == '\\')
				{
					if ((c = *sp++) == 'n')
						c = '\n';
					else if (c == 't')
						c = '\t';
					else if (c == 'r')
						c = '\r';
				}

				/* enter (possibly translated) char in set */
				if (c)
					ep[c >> 3] |= bits(c & 7);
			} while
				((c = *sp++) != ']');

			/* invert the bitmask if all-but was specified */
			if (negclass)
				for(classct = 0; classct < 16; classct++)
					ep[classct] ^= 0xFF;
			ep[0] &= 0xFE;		/* never match ASCII 0 */ 
			ep += 16;		/* advance ep past set mask */
			continue;

		defchar:	/* match literal character */
		default:	/* which is what we'd do by default */
			*ep++ = CCHR;		/* insert character mark */
			*ep++ = c;
		}
	}
}

/* read next command from -e argument or command file */
static int cmdline(char	*cbuf)		/* uses eflag, eargc, cmdf */
{
	register int	inc;	/* not char because must hold EOF */

	cbuf--;			/* so pre-increment points us at cbuf */

	/* e command flag is on */
	if (eflag)
	{
		register char	*p;	/* ptr to current -e argument */
		static char	*savep;	/* saves previous value of p */

		if (eflag > 0)	/* there are pending -e arguments */
		{
			eflag = -1;
			if (eargc-- <= 0)
				exit(2);	/* if no arguments, barf */

			/* else transcribe next e argument into cbuf */
			p = *++eargv;
			while((*++cbuf = *p++))
				if (*cbuf == '\\')
				{
					if ((*++cbuf = *p++) == '\0')
						return(savep = NULL, -1);
					else
						continue;
				}
				else if (*cbuf == '\n')	/* end of 1 cmd line */
				{ 
					*cbuf = '\0';
					return(savep = p, 1);
					/* we'll be back for the rest... */
				}

			/* found end-of-string; can advance to next argument */
			return(savep = NULL, 1);
		}

		if ((p = savep) == NULL)
			return(-1);

		while((*++cbuf = *p++))
			if (*cbuf == '\\')
			{
				if ((*++cbuf = *p++) == '0')
					return(savep = NULL, -1);
				else
					continue;
			}
			else if (*cbuf == '\n')
			{
				*cbuf = '\0';
				return(savep = p, 1);
			}

		return(savep = NULL, 1);
	}

	/* if no -e flag read from command file descriptor */
	while((inc = getc(cmdf)) != EOF)		/* get next char */
		if ((*++cbuf = inc) == '\\')		/* if it's escape */ 
			*++cbuf = inc = getc(cmdf);	/* get next char */
		else if (*cbuf == '\n')			/* end on newline */
			return(*cbuf = '\0', 1);	/* cap the string */

	return(*++cbuf = '\0', -1);	/* end-of-file, no more chars */
}

/* expand an address at *cp... into expbuf, return ptr at following char */
static char *address(char *expbuf)		/* uses cp, linenum */
{
	static int	numl = 0;	/* current ind in addr-number table */
	register char	*rcp;		/* temp compile ptr for forwd look */
	long		lno;		/* computed value of numeric address */

	if (*cp == '$')			/* end-of-source address */
	{
		*expbuf++ = CEND;	/* write symbolic end address */
		*expbuf++ = CEOF;	/* and the end-of-address mark (!) */
		cp++;			/* go to next source character */
		last_line_used = TRUE;
		return(expbuf);		/* we're done */
	}
	if (*cp == '/')			/* start of regular-expression match */
		return(recomp(expbuf, *cp++));	/* compile the RE */

	rcp = cp; lno = 0;		/* now handle a numeric address */
	while(*rcp >= '0' && *rcp <= '9')	/* collect digits */
		lno = lno*10 + *rcp++ - '0';	/*  compute their value */

	if (rcp > cp)			/* if we caught a number... */
	{
		*expbuf++ = CLNUM;	/* put a numeric-address marker */
		*expbuf++ = numl;	/* and the address table index */
		linenum[numl++] = lno;	/* and set the table entry */
		if (numl >= MAXLINES)	/* oh-oh, address table overflow */
			die(TMLNR);	/*   abort with error message */
		*expbuf++ = CEOF;	/* write the end-of-address marker */
		cp = rcp;		/* point compile past the address */ 
		return(expbuf);		/* we're done */
	}

	return(NULL);		/* no legal address was found */
}

/* accept multiline input from *cp..., discarding leading whitespace
   txp: where to put the text */
static char *gettext(char* txp)		/* uses global cp */
{
	register char	*p = cp;

	SKIPWS(p);			/* discard whitespace */
	do {
		if ((*txp = *p++) == '\\')	/* handle escapes */
			*txp = *p++;
		if (*txp == '\0')		/* we're at end of input */
			return(cp = --p, ++txp);
		else if (*txp == '\n')		/* also SKIPWS after newline */
			SKIPWS(p);
	} while (txp++);		/* keep going till we find that nul */
	return(txp);
}

/* find the label matching *ptr, return NULL if none */
static label *search(label *ptr)		/* uses global lablst */
{
	register label	*rp;
	for(rp = lablst; rp < ptr; rp++)
		if ((rp->name != NULL) && (strcmp(rp->name, ptr->name) == 0))
			return(rp);
	return(NULL);
}

/* write label links into the compiled-command space */
static void resolve(void)			/* uses global lablst */
{
	register label		*lptr;
	register sedcmd		*rptr, *trptr;

	/* loop through the label table */
	for(lptr = lablst; lptr < lab; lptr++)
		if (lptr->address == NULL)	/* barf if not defined */
		{
			fprintf(stderr, ULABL, lptr->name);
			exit(2);
		}
		else if (lptr->last)		/* if last is non-null */
		{
			rptr = lptr->last;		/* chase it */
			while((trptr = rptr->u.link))	/* resolve refs */
			{
				rptr->u.link = lptr->address;
				rptr = trptr;
			}
			rptr->u.link = lptr->address;
		}
}

/* compile a y (transliterate) command
   ep:		where to compile to
   delim:	end delimiter to look for */
static char *ycomp(char *ep, char delim)
{
	char *tp, *sp;
	int c;

	/* scan the 'from' section for invalid chars */
	for(sp = tp = cp; *tp != delim; tp++)
	{
		if (*tp == '\\')
			tp++;
		if ((*tp == '\n') || (*tp == '\0'))
			return(BAD);
	}
	tp++;		/* tp now points at first char of 'to' section */

	/* now rescan the 'from' section */
	while((c = *sp++ & 0x7F) != delim)
	{
		if (c == '\\' && *sp == 'n')
		{
			sp++;
			c = '\n';
		}
		if ((ep[c] = *tp++) == '\\' && *tp == 'n')
		{
			ep[c] = '\n';
			tp++;
		}
		if ((ep[c] == delim) || (ep[c] == '\0'))
			return(BAD);
	}

	if (*tp != delim)	/* 'to', 'from' parts have unequal lengths */
		return(BAD);

	cp = ++tp;			/* point compile ptr past translit */

	for(c = 0; c < 128; c++)	/* fill in self-map entries in table */
		if (ep[c] == 0)
			ep[c] = c;

	return(ep + 0x80);	/* return first free location past table end */
}

/* sedcomp.c ends here */
