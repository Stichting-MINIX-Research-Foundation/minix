/* sed - stream editor		Author: Eric S. Raymond */

/* This used to be three different files with the following makefile:
 * (Note the chmem).

CFLAGS=	-F -T.

OBJS=   sedcomp.s sedexec.s

sed: 	$(OBJS)
        cc -T. -o sed $(OBJS)
  @chmem =13312 sed

$(OBJS):	sed.h

 * If you want longer lines: increase MAXBUF.
 * If you want scripts with more text: increase POOLSIZE.
 * If you want more commands per script: increase MAXCMDS.
 */

#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/*+++++++++++++++*/

/* Sed.h -- types and constants for the stream editor */

/* Data area sizes used by both modules */
#define MAXBUF		4000	/* current line buffer size */
#define MAXAPPENDS	20	/* maximum number of appends */
#define MAXTAGS		9	/* tagged patterns are \1 to \9 */

/* Constants for compiled-command representation */
#define EQCMD	0x01		/* = -- print current line number	 */
#define ACMD	0x02		/* a -- append text after current line	 */
#define BCMD	0x03		/* b -- branch to label			 */
#define CCMD	0x04		/* c -- change current line		 */
#define DCMD	0x05		/* d -- delete all of pattern space */
#define CDCMD	0x06		/* D -- delete first line of pattern space */
#define GCMD	0x07		/* g -- copy hold space to pattern space */
#define CGCMD	0x08		/* G -- append hold space to pattern space */
#define HCMD	0x09		/* h -- copy pattern space to hold space */
#define CHCMD	0x0A		/* H -- append pattern space to hold space */
#define ICMD	0x0B		/* i -- insert text before current line	 */
#define LCMD	0x0C		/* l -- print pattern space in escaped form */
#define NCMD	0x0D		/* n -- get next line into pattern space */
#define CNCMD	0x0E		/* N -- append next line to pattern space */
#define PCMD	0x0F		/* p -- print pattern space to output	 */
#define CPCMD	0x10		/* P -- print first line of pattern space */
#define QCMD	0x11		/* q -- exit the stream editor		 */
#define RCMD	0x12		/* r -- read in a file after current line */
#define SCMD	0x13		/* s -- regular-expression substitute	 */
#define TCMD	0x14		/* t -- branch on any substitute successful */
#define CTCMD	0x15		/* T -- branch on any substitute failed	 */
#define WCMD	0x16		/* w -- write pattern space to file	 */
#define CWCMD	0x17		/* W -- write first line of pattern space */
#define XCMD	0x18		/* x -- exhange pattern and hold spaces	 */
#define YCMD	0x19		/* y -- transliterate text		 */

struct cmd_t {			/* compiled-command representation */
  char *addr1;			/* first address for command */
  char *addr2;			/* second address for command */
  union {
	char *lhs;		/* s command lhs */
	struct cmd_t *link;	/* label link */
  } u;
  char command;			/* command code */
  char *rhs;			/* s command replacement string */
  FILE *fout;			/* associated output file descriptor */
  struct {
	char allbut;		/* was negation specified? */
	char global;		/* was g postfix specified? */
	char print;		/* was p postfix specified? */
	char inrange;		/* in an address range? */
  } flags;
};
typedef struct cmd_t sedcmd;	/* use this name for declarations */

#define BAD	((char *) -1)	/* guaranteed not a string ptr */



/* Address and regular expression compiled-form markers */
#define STAR	1		/* marker for Kleene star */
#define CCHR	2		/* non-newline character to be matched
			 * follows */
#define CDOT	4		/* dot wild-card marker */
#define CCL	6		/* character class follows */
#define CNL	8		/* match line start */
#define CDOL	10		/* match line end */
#define CBRA	12		/* tagged pattern start marker */
#define CKET	14		/* tagged pattern end marker */
#define CBACK	16		/* backslash-digit pair marker */
#define CLNUM	18		/* numeric-address index follows */
#define CEND	20		/* symbol for end-of-source */
#define CEOF	22		/* end-of-field mark */

/* Sed.h ends here */

#ifndef CMASK
#define CMASK  0xFF		/* some char type should have been unsigned
			 * char? */
#endif

/*+++++++++++++++*/

/* Sed - stream editor		Author: Eric S. Raymond */

/*
   The stream editor compiles its command input	 (from files or -e options)
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

/* #include <stdio.h> */
/* #include "sed.h"   */

/* Imported functions */

/***** public stuff ******/

#define MAXCMDS		500	/* maximum number of compiled commands */
#define MAXLINES	256	/* max # numeric addresses to compile */

/* Main data areas */
char linebuf[MAXBUF + 1];	/* current-line buffer */
sedcmd cmds[MAXCMDS + 1];	/* hold compiled commands */
long linenum[MAXLINES];		/* numeric-addresses table */

/* Miscellaneous shared variables */
int nflag;			/* -n option flag */
int eargc;			/* scratch copy of argument count */
char **eargv;			/* scratch copy of argument list */
char bits[] = {1, 2, 4, 8, 16, 32, 64, 128};

/***** module common stuff *****/

#define POOLSIZE	20000	/* size of string-pool space */
#define WFILES		10	/* max # w output files that can be compiled */
#define RELIMIT		256	/* max chars in compiled RE */
#define MAXDEPTH	20	/* maximum {}-nesting level */
#define MAXLABS		50	/* max # of labels that can be handled */

#define SKIPWS(pc)	while ((*pc==' ') || (*pc=='\t')) pc++
#define ABORT(msg)	(fprintf(stderr, msg, linebuf), quit(2))
#define IFEQ(x, v)	if (*x == v) x++ ,	/* do expression */

/* Error messages */
static char AGMSG[] = "sed: garbled address %s\n";
static char CGMSG[] = "sed: garbled command %s\n";
static char TMTXT[] = "sed: too much text: %s\n";
static char AD1NG[] = "sed: no addresses allowed for %s\n";
static char AD2NG[] = "sed: only one address allowed for %s\n";
static char TMCDS[] = "sed: too many commands, last was %s\n";
static char COCFI[] = "sed: cannot open command-file %s\n";
static char UFLAG[] = "sed: unknown flag %c\n";
static char CCOFI[] = "sed: cannot create %s\n";
static char ULABL[] = "sed: undefined label %s\n";
static char TMLBR[] = "sed: too many {'s\n";
static char FRENL[] = "sed: first RE must be non-null\n";
static char NSCAX[] = "sed: no such command as %s\n";
static char TMRBR[] = "sed: too many }'s\n";
static char DLABL[] = "sed: duplicate label %s\n";
static char TMLAB[] = "sed: too many labels: %s\n";
static char TMWFI[] = "sed: too many w files\n";
static char REITL[] = "sed: RE too long: %s\n";
static char TMLNR[] = "sed: too many line numbers\n";
static char TRAIL[] = "sed: command \"%s\" has trailing garbage\n";

typedef struct {		/* represent a command label */
  char *name;			/* the label name */
  sedcmd *last;			/* it's on the label search list */
  sedcmd *address;		/* pointer to the cmd it labels */
}

 label;

/* Label handling */
static label labels[MAXLABS];	/* here's the label table */
static label *lab = labels + 1;	/* pointer to current label */
static label *lablst = labels;	/* header for search list */

/* String pool for regular expressions, append text, etc. etc. */
static char pool[POOLSIZE];	/* the pool */
static char *fp = pool;		/* current pool pointer */
static char *poolend = pool + POOLSIZE;	/* pointer past pool end */

/* Compilation state */
static FILE *cmdf = NULL;	/* current command source */
static char *cp = linebuf;	/* compile pointer */
static sedcmd *cmdp = cmds;	/* current compiled-cmd ptr */
static char *lastre = NULL;	/* old RE pointer */
static int bdepth = 0;		/* current {}-nesting level */
static int bcount = 0;		/* # tagged patterns in current RE */

/* Compilation flags */
static int eflag;		/* -e option flag */
static int gflag;		/* -g option flag */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(static void compile, (void));
_PROTOTYPE(static int cmdcomp, (int cchar));
_PROTOTYPE(static char *rhscomp, (char *rhsp, int delim));
_PROTOTYPE(static char *recomp, (char *expbuf, int redelim));
_PROTOTYPE(static int cmdline, (char *cbuf));
_PROTOTYPE(static char *address, (char *expbuf));
_PROTOTYPE(static char *gettext, (char *txp));
_PROTOTYPE(static label *search, (label *ptr));
_PROTOTYPE(static void resolve, (void));
_PROTOTYPE(static char *ycomp, (char *ep, int delim));
_PROTOTYPE(void quit, (int n));
_PROTOTYPE(void execute, (void));
_PROTOTYPE(static int selected, (sedcmd *ipc));
_PROTOTYPE(static int match, (char *expbuf, int gf));
_PROTOTYPE(static int advance, (char *lp, char *ep));
_PROTOTYPE(static int substitute, (sedcmd *ipc));
_PROTOTYPE(static void dosub, (char *rhsbuf));
_PROTOTYPE(static char *place, (char *asp, char *al1, char *al2));
_PROTOTYPE(static void listto, (char *p1, FILE *fp));
_PROTOTYPE(static void truncated, (int h));
_PROTOTYPE(static void command, (sedcmd *ipc));
_PROTOTYPE(static void openfile, (char *file));
_PROTOTYPE(static void get, (void));
_PROTOTYPE(static void initget, (void));
_PROTOTYPE(static char *getline, (char *buf));
_PROTOTYPE(static int Memcmp, (char *a, char *b, int count));
_PROTOTYPE(static void readout, (void));

int main(argc, argv)
/* Main sequence of the stream editor */
int argc;
char *argv[];
{
  eargc = argc;			/* set local copy of argument count */
  eargv = argv;			/* set local copy of argument list */
  cmdp->addr1 = pool;		/* 1st addr expand will be at pool start */
  if (eargc == 1) quit(0);	/* exit immediately if no arguments */
  /* Scan through the arguments, interpreting each one */
  while ((--eargc > 0) && (**++eargv == '-')) switch (eargv[0][1]) {
	    case 'e':
		eflag++;
		compile();	/* compile with e flag on */
		eflag = 0;
		continue;	/* get another argument */
	    case 'f':
		if (eargc-- <= 0)	/* barf if no -f file */
			quit(2);
		if ((cmdf = fopen(*++eargv, "r")) == NULL) {
			fprintf(stderr, COCFI, *eargv);
			quit(2);
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


  if (cmdp == cmds) {		/* no commands have been compiled */
	eargv--;
	eargc++;
	eflag++;
	compile();
	eflag = 0;
	eargv++;
	eargc--;
  }
  if (bdepth)			/* we have unbalanced squigglies */
	ABORT(TMLBR);

  lablst->address = cmdp;	/* set up header of label linked list */
  resolve();			/* resolve label table indirections */
  execute();			/* execute commands */
  quit(0);			/* everything was O.K. if we got here */
  return(0);
}


#define H	0x80		/* 128 bit, on if there's really code for
			 * command */
#define LOWCMD	56		/* = '8', lowest char indexed in cmdmask */

/* Indirect through this to get command internal code, if it exists */
static char cmdmask[] =
{
 0, 0, H, 0, 0, H + EQCMD, 0, 0,
 0, 0, 0, 0, H + CDCMD, 0, 0, CGCMD,
 CHCMD, 0, 0, 0, 0, 0, CNCMD, 0,
 CPCMD, 0, 0, 0, H + CTCMD, 0, 0, H + CWCMD,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, H + ACMD, H + BCMD, H + CCMD, DCMD, 0, 0, GCMD,
 HCMD, H + ICMD, 0, 0, H + LCMD, 0, NCMD, 0,
 PCMD, H + QCMD, H + RCMD, H + SCMD, H + TCMD, 0, 0, H + WCMD,
 XCMD, H + YCMD, 0, H + BCMD, 0, H, 0, 0,
};

static void compile()
/* Precompile sed commands out of a file */
{
  char ccode;


  for (;;) {			/* main compilation loop */
	if (*cp == '\0') {	/* get a new command line */
		*linebuf = '\0';	/* K.H */
		if (cmdline(cp = linebuf) < 0) break;
	}
	SKIPWS(cp);
	if (*cp == '\0')	/* empty */
		continue;
	if (*cp == '#') {	/* comment */
		while (*cp) ++cp;
		continue;
	}
	if (*cp == ';') {	/* ; separates cmds */
		cp++;
		continue;
	}

	/* Compile first address */
	if (fp > poolend)
		ABORT(TMTXT);
	else if ((fp = address(cmdp->addr1 = fp)) == BAD)
		ABORT(AGMSG);

	if (fp == cmdp->addr1) {/* if empty RE was found */
		if (lastre)	/* if there was previous RE */
			cmdp->addr1 = lastre;	/* use it */
		else
			ABORT(FRENL);
	} else if (fp == NULL) {/* if fp was NULL */
		fp = cmdp->addr1;	/* use current pool location */
		cmdp->addr1 = NULL;
	} else {
		lastre = cmdp->addr1;
		if (*cp == ',' || *cp == ';') {	/* there's 2nd addr */
			cp++;
			if (fp > poolend) ABORT(TMTXT);
			fp = address(cmdp->addr2 = fp);
			if (fp == BAD || fp == NULL) ABORT(AGMSG);
			if (fp == cmdp->addr2)
				cmdp->addr2 = lastre;
			else
				lastre = cmdp->addr2;
		} else
			cmdp->addr2 = NULL;	/* no 2nd address */
	}
	if (fp > poolend) ABORT(TMTXT);

	SKIPWS(cp);		/* discard whitespace after address */
	IFEQ(cp, '!') cmdp->flags.allbut = 1;

	SKIPWS(cp);		/* get cmd char, range-check it */
	if ((*cp < LOWCMD) || (*cp > '~')
	    || ((ccode = cmdmask[*cp - LOWCMD]) == 0))
		ABORT(NSCAX);

	cmdp->command = ccode & ~H;	/* fill in command value */
	if ((ccode & H) == 0)	/* if no compile-time code */
		cp++;		/* discard command char */
	else if (cmdcomp(*cp++))/* execute it; if ret = 1 */
		continue;	/* skip next line read */

	if (++cmdp >= cmds + MAXCMDS) ABORT(TMCDS);

	SKIPWS(cp);		/* look for trailing stuff */
	if (*cp != '\0' && *cp != ';' && *cp != '#') ABORT(TRAIL);
  }
}

static int cmdcomp(cchar)
/* Compile a single command */
register char cchar;		/* character name of command */
{
  static sedcmd **cmpstk[MAXDEPTH];	/* current cmd stack for {} */
  static char *fname[WFILES];	/* w file name pointers */
  static FILE *fout[WFILES];	/* w file file ptrs */
  static int nwfiles = 1;	/* count of open w files */
  int i;			/* indexing dummy used in w */
  sedcmd *sp1, *sp2;		/* temps for label searches */
  label *lpt;
  char redelim;			/* current RE delimiter */

  fout[0] = stdout;
  switch (cchar) {
      case '{':			/* start command group */
	cmdp->flags.allbut = !cmdp->flags.allbut;
	cmpstk[bdepth++] = &(cmdp->u.link);
	if (++cmdp >= cmds + MAXCMDS) ABORT(TMCDS);
	return(1);

      case '}':			/* end command group */
	if (cmdp->addr1) ABORT(AD1NG);	/* no addresses allowed */
	if (--bdepth < 0) ABORT(TMRBR);	/* too many right braces */
	*cmpstk[bdepth] = cmdp;	/* set the jump address */
	return(1);

      case '=':			/* print current source line number */
      case 'q':			/* exit the stream editor */
	if (cmdp->addr2) ABORT(AD2NG);
	break;

      case ':':			/* label declaration */
	if (cmdp->addr1) ABORT(AD1NG);	/* no addresses allowed */
	fp = gettext(lab->name = fp);	/* get the label name */
	if (lpt = search(lab)) {/* does it have a double? */
		if (lpt->address) ABORT(DLABL);	/* yes, abort */
	} else {		/* check that it doesn't overflow label table */
		lab->last = NULL;
		lpt = lab;
		if (++lab >= labels + MAXLABS) ABORT(TMLAB);
	}
	lpt->address = cmdp;
	return(1);

      case 'b':			/* branch command */
      case 't':			/* branch-on-succeed command */
      case 'T':			/* branch-on-fail command */
	SKIPWS(cp);
	if (*cp == '\0') {	/* if branch is to start of cmds... */
		/* Add current command to end of label last */
		if (sp1 = lablst->last) {
			while (sp2 = sp1->u.link) sp1 = sp2;
			sp1->u.link = cmdp;
		} else		/* lablst->last == NULL */
			lablst->last = cmdp;
		break;
	}
	fp = gettext(lab->name = fp);	/* else get label into pool */
	if (lpt = search(lab)) {/* enter branch to it */
		if (lpt->address)
			cmdp->u.link = lpt->address;
		else {
			sp1 = lpt->last;
			while (sp2 = sp1->u.link) sp1 = sp2;
			sp1->u.link = cmdp;
		}
	} else {		/* matching named label not found */
		lab->last = cmdp;	/* add the new label */
		lab->address = NULL;	/* it's forward of here */
		if (++lab >= labels + MAXLABS)	/* overflow if last */
			ABORT(TMLAB);
	}
	break;

      case 'a':			/* append text */
      case 'i':			/* insert text */
      case 'r':			/* read file into stream */
	if (cmdp->addr2) ABORT(AD2NG);
      case 'c':			/* change text */
	if ((*cp == '\\') && (*++cp == '\n')) cp++;
	fp = gettext(cmdp->u.lhs = fp);
	break;

      case 'D':			/* delete current line in hold space */
	cmdp->u.link = cmds;
	break;

      case 's':			/* substitute regular expression */
	redelim = *cp++;	/* get delimiter from 1st ch */
	if ((fp = recomp(cmdp->u.lhs = fp, redelim)) == BAD) ABORT(CGMSG);
	if (fp == cmdp->u.lhs)	/* if compiled RE zero len */
		cmdp->u.lhs = lastre;	/* use the previous one */
	else			/* otherwise */
		lastre = cmdp->u.lhs;	/* save the one just found */
	if ((cmdp->rhs = fp) > poolend) ABORT(TMTXT);
	if ((fp = rhscomp(cmdp->rhs, redelim)) == BAD) ABORT(CGMSG);
	if (gflag) cmdp->flags.global ++;
	while (*cp == 'g' || *cp == 'p' || *cp == 'P') {
		IFEQ(cp, 'g') cmdp->flags.global ++;
		IFEQ(cp, 'p') cmdp->flags.print = 1;
		IFEQ(cp, 'P') cmdp->flags.print = 2;
	}

      case 'l':			/* list pattern space */
	if (*cp == 'w')
		cp++;		/* and execute a w command! */
	else
		break;		/* s or l is done */

      case 'w':			/* write-pattern-space command */
      case 'W':			/* write-first-line command */
	if (nwfiles >= WFILES) ABORT(TMWFI);
	fp = gettext(fname[nwfiles] = fp);	/* filename will be in pool */
	for (i = nwfiles - 1; i >= 0; i--)	/* match it in table */
		if ((fname[i] != NULL) &&
		    (strcmp(fname[nwfiles], fname[i]) == 0)) {
			cmdp->fout = fout[i];
			return(0);
		}

	/* If didn't find one, open new out file */
	if ((cmdp->fout = fopen(fname[nwfiles], "w")) == NULL) {
		fprintf(stderr, CCOFI, fname[nwfiles]);
		quit(2);
	}
	fout[nwfiles++] = cmdp->fout;
	break;

      case 'y':			/* transliterate text */
	fp = ycomp(cmdp->u.lhs = fp, *cp++);	/* compile translit */
	if (fp == BAD) ABORT(CGMSG);	/* fail on bad form */
	if (fp > poolend) ABORT(TMTXT);	/* fail on overflow */
	break;
  }
  return(0);			/* succeeded in interpreting one command */
}

static char *rhscomp(rhsp, delim)	/* uses bcount */
 /* Generate replacement string for substitute command right hand side */
register char *rhsp;		/* place to compile expression to */
register char delim;		/* regular-expression end-mark to look for */
{
  register char *p = cp;	/* strictly for speed */

  for (;;)
	if ((*rhsp = *p++) == '\\') {	/* copy; if it's a \, */
		*rhsp = *p++;	/* copy escaped char */
		/* Check validity of pattern tag */
		if (*rhsp > bcount + '0' && *rhsp <= '9') return(BAD);
		*rhsp++ |= 0x80;/* mark the good ones */
		continue;
	} else if (*rhsp == delim) {	/* found RE end, hooray... */
		*rhsp++ = '\0';	/* cap the expression string */
		cp = p;
		return(rhsp);	/* pt at 1 past the RE */
	} else if (*rhsp++ == '\0')	/* last ch not RE end, help! */
		return(BAD);
}

static char *recomp(expbuf, redelim)	/* uses cp, bcount */
 /* Compile a regular expression to internal form */
char *expbuf;			/* place to compile it to */
char redelim;			/* RE end-marker to look for */
{
  register char *ep = expbuf;	/* current-compiled-char pointer */
  register char *sp = cp;	/* source-character ptr */
  register int c;		/* current-character pointer */
  char negclass;		/* all-but flag */
  char *lastep;			/* ptr to last expr compiled */
  char *svclass;		/* start of current char class */
  char brnest[MAXTAGS];		/* bracket-nesting array */
  char *brnestp;		/* ptr to current bracket-nest */
  int classct;			/* class element count */
  int tags;			/* # of closed tags */

  if (*cp == redelim)		/* if first char is RE endmarker */
	return(cp++, expbuf);	/* leave existing RE unchanged */

  lastep = NULL;		/* there's no previous RE */
  brnestp = brnest;		/* initialize ptr to brnest array */
  tags = bcount = 0;		/* initialize counters */

  if (*ep++ = (*sp == '^'))	/* check for start-of-line syntax */
	sp++;

  for (;;) {
	if (ep >= expbuf + RELIMIT)	/* match is too large */
		return(cp = sp, BAD);
	if ((c = *sp++) == redelim) {	/* found the end of the RE */
		cp = sp;
		if (brnestp != brnest)	/* \(, \) unbalanced */
			return(BAD);
		*ep++ = CEOF;	/* write end-of-pattern mark */
		return(ep);	/* return ptr to compiled RE */
	}
	if (c != '*')		/* if we're a postfix op */
		lastep = ep;	/* get ready to match last */

	switch (c) {
	    case '\\':
		if ((c = *sp++) == '(') {	/* start tagged section */
			if (bcount >= MAXTAGS) return(cp = sp, BAD);
			*brnestp++ = bcount;	/* update tag stack */
			*ep++ = CBRA;	/* enter tag-start */
			*ep++ = bcount++;	/* bump tag count */
			continue;
		} else if (c == ')') {	/* end tagged section */
			if (brnestp <= brnest)	/* extra \) */
				return(cp = sp, BAD);
			*ep++ = CKET;	/* enter end-of-tag */
			*ep++ = *--brnestp;	/* pop tag stack */
			tags++;	/* count closed tags */
			continue;
		} else if (c >= '1' && c <= '9') {	/* tag use */
			if ((c -= '1') >= tags)	/* too few */
				return(BAD);
			*ep++ = CBACK;	/* enter tag mark */
			*ep++ = c;	/* and the number */
			continue;
		} else if (c == '\n')	/* escaped newline no good */
			return(cp = sp, BAD);
		else if (c == 'n')	/* match a newline */
			c = '\n';
		else if (c == 't')	/* match a tab */
			c = '\t';
		else if (c == 'r')	/* match a return */
			c = '\r';
		goto defchar;

	    case '\0':		/* ignore nuls */
		continue;

	    case '\n':		/* trailing pattern delimiter is missing */
		return(cp = sp, BAD);

	    case '.':		/* match any char except newline */
		*ep++ = CDOT;
		continue;
	    case '*':		/* 0..n repeats of previous pattern */
		if (lastep == NULL)	/* if * isn't first on line */
			goto defchar;	/* match a literal * */
		if (*lastep == CKET)	/* can't iterate a tag */
			return(cp = sp, BAD);
		*lastep |= STAR;/* flag previous pattern */
		continue;

	    case '$':		/* match only end-of-line */
		if (*sp != redelim)	/* if we're not at end of RE */
			goto defchar;	/* match a literal $ */
		*ep++ = CDOL;	/* insert end-symbol mark */
		continue;

	    case '[':		/* begin character set pattern */
		if (ep + 17 >= expbuf + RELIMIT) ABORT(REITL);
		*ep++ = CCL;	/* insert class mark */
		if (negclass = ((c = *sp++) == '^')) c = *sp++;
		svclass = sp;	/* save ptr to class start */
		do {
			if (c == '\0') ABORT(CGMSG);

			/* Handle character ranges */
			if (c == '-' && sp > svclass && *sp != ']')
				for (c = sp[-2]; c < *sp; c++)
					ep[c >> 3] |= bits[c & 7];

			/* Handle escape sequences in sets */
			if (c == '\\')
				if ((c = *sp++) == 'n')
					c = '\n';
				else if (c == 't')
					c = '\t';
				else if (c == 'r')
					c = '\r';

			/* Enter (possibly translated) char in set */
			ep[c >> 3] |= bits[c & 7];
		} while
			((c = *sp++) != ']');

		/* Invert the bitmask if all-but was specified */
		if (negclass) for (classct = 0; classct < 16; classct++)
				ep[classct] ^= 0xFF;
		ep[0] &= 0xFE;	/* never match ASCII 0 */
		ep += 16;	/* advance ep past set mask */
		continue;

  defchar:			/* match literal character */
	    default:		/* which is what we'd do by default */
		*ep++ = CCHR;	/* insert character mark */
		*ep++ = c;
	}
  }
}

static int cmdline(cbuf)	/* uses eflag, eargc, cmdf */
 /* Read next command from -e argument or command file */
register char *cbuf;
{
  register int inc;		/* not char because must hold EOF */

  *cbuf-- = 0;			/* so pre-increment points us at cbuf */

  /* E command flag is on */
  if (eflag) {
	register char *p;	/* ptr to current -e argument */
	static char *savep;	/* saves previous value of p */

	if (eflag > 0) {	/* there are pending -e arguments */
		eflag = -1;
		if (eargc-- <= 0) quit(2);	/* if no arguments, barf */

		/* Else transcribe next e argument into cbuf */
		p = *++eargv;
		while (*++cbuf = *p++)
			if (*cbuf == '\\') {
				if ((*++cbuf = *p++) == '\0')
					return(savep = NULL, -1);
				else
					continue;
			} else if (*cbuf == '\n') {	/* end of 1 cmd line */
				*cbuf = '\0';
				return(savep = p, 1);
				/* We'll be back for the rest... */
			}

		/* Found end-of-string; can advance to next argument */
		return(savep = NULL, 1);
	}
	if ((p = savep) == NULL) return(-1);

	while (*++cbuf = *p++)
		if (*cbuf == '\\') {
			if ((*++cbuf = *p++) == '0')
				return(savep = NULL, -1);
			else
				continue;
		} else if (*cbuf == '\n') {
			*cbuf = '\0';
			return(savep = p, 1);
		}
	return(savep = NULL, 1);
  }

  /* If no -e flag read from command file descriptor */
  while ((inc = getc(cmdf)) != EOF)	/* get next char */
	if ((*++cbuf = inc) == '\\')	/* if it's escape */
		*++cbuf = inc = getc(cmdf);	/* get next char */
	else if (*cbuf == '\n')	/* end on newline */
		return(*cbuf = '\0', 1);	/* cap the string */

  return(*++cbuf = '\0', -1);	/* end-of-file, no more chars */
}

static char *address(expbuf)	/* uses cp, linenum */
 /* Expand an address at *cp... into expbuf, return ptr at following char */
register char *expbuf;
{
  static int numl = 0;		/* current ind in addr-number table */
  register char *rcp;		/* temp compile ptr for forwd look */
  long lno;			/* computed value of numeric address */

  if (*cp == '$') {		/* end-of-source address */
	*expbuf++ = CEND;	/* write symbolic end address */
	*expbuf++ = CEOF;	/* and the end-of-address mark (!) */
	cp++;			/* go to next source character */
	return(expbuf);	/* we're done */
  }
  if (*cp == '/' || *cp == '\\') { /* start of regular-expression match */
	if (*cp == '\\') cp++;
	return(recomp(expbuf, *cp++));	/* compile the RE */
  }

  rcp = cp;
  lno = 0;			/* now handle a numeric address */
  while (*rcp >= '0' && *rcp <= '9')	/* collect digits */
	lno = lno * 10 + *rcp++ - '0';	/* compute their value */

  if (rcp > cp) {		/* if we caught a number... */
	*expbuf++ = CLNUM;	/* put a numeric-address marker */
	*expbuf++ = numl;	/* and the address table index */
	linenum[numl++] = lno;	/* and set the table entry */
	if (numl >= MAXLINES)	/* oh-oh, address table overflow */
		ABORT(TMLNR);	/* abort with error message */
	*expbuf++ = CEOF;	/* write the end-of-address marker */
	cp = rcp;		/* point compile past the address */
	return(expbuf);	/* we're done */
  }
  return(NULL);			/* no legal address was found */
}

static char *gettext(txp)	/* uses global cp */
 /* Accept multiline input from *cp..., discarding leading whitespace */
register char *txp;		/* where to put the text */
{
  register char *p = cp;	/* this is for speed */

  SKIPWS(p);			/* discard whitespace */
  do {
	if ((*txp = *p++) == '\\')	/* handle escapes */
		*txp = *p++;
	if (*txp == '\0')	/* we're at end of input */
		return(cp = --p, ++txp);
	else if (*txp == '\n')	/* also SKIPWS after newline */
		SKIPWS(p);
  } while
	(txp++);		/* keep going till we find that nul */
  return(txp);
}

static label *search(ptr)	/* uses global lablst */
 /* Find the label matching *ptr, return NULL if none */
register label *ptr;
{
  register label *rp;
  for (rp = lablst; rp < ptr; rp++)
	if ((rp->name != NULL) && (strcmp(rp->name, ptr->name) == 0))
		return(rp);
  return(NULL);
}

static void resolve()
{				/* uses global lablst */
  /* Write label links into the compiled-command space */
  register label *lptr;
  register sedcmd *rptr, *trptr;

  /* Loop through the label table */
  for (lptr = lablst; lptr < lab; lptr++)
	if (lptr->address == NULL) {	/* barf if not defined */
		fprintf(stderr, ULABL, lptr->name);
		quit(2);
	} else if (lptr->last) {/* if last is non-null */
		rptr = lptr->last;	/* chase it */
		while (trptr = rptr->u.link) {	/* resolve refs */
			rptr->u.link = lptr->address;
			rptr = trptr;
		}
		rptr->u.link = lptr->address;
	}
}

static char *ycomp(ep, delim)
/* Compile a y (transliterate) command */
register char *ep;		/* where to compile to */
char delim;			/* end delimiter to look for */
{
  register char *tp, *sp;
  register int c;

  /* Scan the 'from' section for invalid chars */
  for (sp = tp = cp; *tp != delim; tp++) {
	if (*tp == '\\') tp++;
	if ((*tp == '\n') || (*tp == '\0')) return (BAD);
  }
  tp++;				/* tp now points at first char of 'to'
			 * section */

  /* Now rescan the 'from' section */
  while ((c = *sp++ & 0x7F) != delim) {
	if (c == '\\' && *sp == 'n') {
		sp++;
		c = '\n';
	}
	if ((ep[c] = *tp++) == '\\' && *tp == 'n') {
		ep[c] = '\n';
		tp++;
	}
	if ((ep[c] == delim) || (ep[c] == '\0')) return(BAD);
  }

  if (*tp != delim)		/* 'to', 'from' parts have unequal lengths */
	return(BAD);

  cp = ++tp;			/* point compile ptr past translit */

  for (c = 0; c < 128; c++)	/* fill in self-map entries in table */
	if (ep[c] == 0) ep[c] = c;

  return(ep + 0x80);		/* return first free location past table end */
}

void quit(n)
int n;
{
/* Flush buffers and exit.  Now a historical relic.  Rely on exit to flush
 * the buffers.
 */
  exit(n);
}

/*+++++++++++++++*/

/*
   sedexec.c -- execute compiled form of stream editor commands

   The single entry point of this module is the function execute(). It
   may take a string argument (the name of a file to be used as text)  or
   the argument NULL which tells it to filter standard input. It executes
   the compiled commands in cmds[] on each line in turn.

   The function command() does most of the work. Match() and advance()
   are used for matching text against precompiled regular expressions and
   dosub() does right-hand-side substitution.  Getline() does text input;
   readout() and Memcmp() are output and string-comparison utilities.
*/

/* #include <stdio.h>	*/
/* #include <ctype.h>	*/
/* #include "sed.h"	*/

/***** shared variables imported from the main ******/

/* Main data areas */
extern char linebuf[];		/* current-line buffer */
extern sedcmd cmds[];		/* hold compiled commands */
extern long linenum[];		/* numeric-addresses table */

/* Miscellaneous shared variables */
extern int nflag;		/* -n option flag */
extern int eargc;		/* scratch copy of argument count */
extern char **eargv;		/* scratch copy of argument list */
extern char bits[];		/* the bits table */

/***** end of imported stuff *****/

#define MAXHOLD	 MAXBUF		/* size of the hold space */
#define GENSIZ	 MAXBUF		/* maximum genbuf size */

#define TRUE	 1
#define FALSE	 0

static char LTLMSG[] = "sed: line too long\n";

static char *spend;		/* current end-of-line-buffer pointer */
static long lnum = 0L;		/* current source line number */

/* Append buffer maintenance */
static sedcmd *appends[MAXAPPENDS];	/* array of ptrs to a,i,c commands */
static sedcmd **aptr = appends;	/* ptr to current append */

/* Genbuf and its pointers */
static char genbuf[GENSIZ];
static char *loc1;
static char *loc2;
static char *locs;

/* Command-logic flags */
static int lastline;		/* do-line flag */
static int jump;		/* jump to cmd's link address if set */
static int delete;		/* delete command flag */

/* Tagged-pattern tracking */
static char *bracend[MAXTAGS];	/* tagged pattern start pointers */
static char *brastart[MAXTAGS];	/* tagged pattern end pointers */

static int anysub;		/* true if any s on current line succeeded */


void execute()
/* Execute the compiled commands in cmds[] */
{
  register char *p1;		/* dummy copy ptrs */
  register sedcmd *ipc;		/* ptr to current command */
  char *execp;			/* ptr to source */


  initget();

  /* Here's the main command-execution loop */
  for (;;) {

	/* Get next line to filter */
	if ((execp = getline(linebuf)) == BAD) return;
	spend = execp;
	anysub = FALSE;

	/* Loop through compiled commands, executing them */
	for (ipc = cmds; ipc->command;) {
		if (!selected(ipc)) {
			ipc++;
			continue;
		}
		command(ipc);	/* execute the command pointed at */

		if (delete)	/* if delete flag is set */
			break;	/* don't exec rest of compiled cmds */

		if (jump) {	/* if jump set, follow cmd's link */
			jump = FALSE;
			if ((ipc = ipc->u.link) == 0) {
				ipc = cmds;
				break;
			}
		} else		/* normal goto next command */
			ipc++;
	}

	/* We've now done all modification commands on the line */

	/* Here's where the transformed line is output */
	if (!nflag && !delete) {
		for (p1 = linebuf; p1 < spend; p1++) putc(*p1, stdout);
		putc('\n', stdout);
	}

	/* If we've been set up for append, emit the text from it */
	if (aptr > appends) readout();

	delete = FALSE;		/* clear delete flag; about to get next cmd */
  }
}

static int selected(ipc)
/* Is current command selected */
sedcmd *ipc;
{
  register char *p1 = ipc->addr1;	/* point p1 at first address */
  register char *p2 = ipc->addr2;	/* and p2 at second */
  int c;
  int sel = TRUE;		/* select by default */

  if (!p1)			/* No addresses: always selected */
	;
  else if (ipc->flags.inrange) {
	if (*p2 == CEND);
	else if (*p2 == CLNUM) {
		c = p2[1] & CMASK;
		if (lnum >= linenum[c]) {
			ipc->flags.inrange = FALSE;
			if (lnum > linenum[c]) sel = FALSE;
		}
	} else if (match(p2, 0))
		ipc->flags.inrange = FALSE;
  } else if (*p1 == CEND) {
	if (!lastline) sel = FALSE;
  } else if (*p1 == CLNUM) {
	c = p1[1] & CMASK;
	if (lnum != linenum[c])
		sel = FALSE;
	else if (p2)
		ipc->flags.inrange = TRUE;
  } else if (match(p1, 0)) {
	if (p2) ipc->flags.inrange = TRUE;
  } else
	sel = FALSE;

  return ipc->flags.allbut ? !sel : sel;
}

static int match(expbuf, gf)	/* uses genbuf */
 /* Match RE at expbuf against linebuf; if gf set, copy linebuf from genbuf */
char *expbuf;
int gf;
{
  register char *p1, *p2, c;

  if (gf) {
	if (*expbuf) return(FALSE);
	p1 = linebuf;
	p2 = genbuf;
	while (*p1++ = *p2++);
	locs = p1 = loc2;
  } else {
	p1 = linebuf;
	locs = FALSE;
  }

  p2 = expbuf;
  if (*p2++) {
	loc1 = p1;
	if (*p2 == CCHR && p2[1] != *p1)	/* 1st char is wrong */
		return(FALSE);	/* so fail */
	return(advance(p1, p2));/* else try to match rest */
  }

  /* Quick check for 1st character if it's literal */
  if (*p2 == CCHR) {
	c = p2[1];		/* pull out character to search for */
	do {
		if (*p1 != c) continue;	/* scan the source string */
		if (advance(p1, p2))	/* found it, match the rest */
			return(loc1 = p1, 1);
	} while
		(*p1++);
	return(FALSE);		/* didn't find that first char */
  }

  /* Else try for unanchored match of the pattern */
  do {
	if (advance(p1, p2)) return(loc1 = p1, 1);
  } while
	(*p1++);

  /* If got here, didn't match either way */
  return(FALSE);
}

static int advance(lp, ep)
/* Attempt to advance match pointer by one pattern element */
register char *lp;		/* source (linebuf) ptr */
register char *ep;		/* regular expression element ptr */
{
  register char *curlp;		/* save ptr for closures */
  char c;			/* scratch character holder */
  char *bbeg;
  int ct;

  for (;;) switch (*ep++) {
	    case CCHR:		/* literal character */
		if (*ep++ == *lp++)	/* if chars are equal */
			continue;	/* matched */
		return(FALSE);	/* else return false */

	    case CDOT:		/* anything but newline */
		if (*lp++)	/* first NUL is at EOL */
			continue;	/* keep going if didn't find */
		return(FALSE);	/* else return false */

	    case CNL:		/* start-of-line */
	    case CDOL:		/* end-of-line */
		if (*lp == 0)	/* found that first NUL? */
			continue;	/* yes, keep going */
		return(FALSE);	/* else return false */

	    case CEOF:		/* end-of-address mark */
		loc2 = lp;	/* set second loc */
		return(TRUE);	/* return true */

	    case CCL:		/* a closure */
		c = *lp++ & 0177;
		if (ep[c >> 3] & bits[c & 07]) {	/* is char in set? */
			ep += 16;	/* then skip rest of bitmask */
			continue;	/* and keep going */
		}
		return(FALSE);	/* else return false */

	    case CBRA:		/* start of tagged pattern */
		brastart[*ep++] = lp;	/* mark it */
		continue;	/* and go */

	    case CKET:		/* end of tagged pattern */
		bracend[*ep++] = lp;	/* mark it */
		continue;	/* and go */

	    case CBACK:
		bbeg = brastart[*ep];
		ct = bracend[*ep++] - bbeg;

		if (Memcmp(bbeg, lp, ct)) {
			lp += ct;
			continue;
		}
		return(FALSE);

	    case CBACK | STAR:
		bbeg = brastart[*ep];
		ct = bracend[*ep++] - bbeg;
		curlp = lp;
		while (Memcmp(bbeg, lp, ct)) lp += ct;

		while (lp >= curlp) {
			if (advance(lp, ep)) return(TRUE);
			lp -= ct;
		}
		return(FALSE);


	    case CDOT | STAR:	/* match .* */
		curlp = lp;	/* save closure start loc */
		while (*lp++);	/* match anything */
		goto star;	/* now look for followers */

	    case CCHR | STAR:	/* match <literal char>* */
		curlp = lp;	/* save closure start loc */
		while (*lp++ == *ep);	/* match many of that char */
		ep++;		/* to start of next element */
		goto star;	/* match it and followers */

	    case CCL | STAR:	/* match [...]* */
		curlp = lp;	/* save closure start loc */
		do {
			c = *lp++ & 0x7F;	/* match any in set */
		} while
			(ep[c >> 3] & bits[c & 07]);
		ep += 16;	/* skip past the set */
		goto star;	/* match followers */

  star:				/* the recursion part of a * or + match */
		if (--lp == curlp)	/* 0 matches */
			continue;

		if (*ep == CCHR) {
			c = ep[1];
			do {
				if (*lp != c) continue;
				if (advance(lp, ep)) return (TRUE);
			} while
				(lp-- > curlp);
			return(FALSE);
		}
		if (*ep == CBACK) {
			c = *(brastart[ep[1]]);
			do {
				if (*lp != c) continue;
				if (advance(lp, ep)) return (TRUE);
			} while
				(lp-- > curlp);
			return(FALSE);
		}
		do {
			if (lp == locs) break;
			if (advance(lp, ep)) return (TRUE);
		} while
			(lp-- > curlp);
		return(FALSE);

	    default:
		fprintf(stderr, "sed: RE error, %o\n", *--ep);
		quit(2);
	}
}

static int substitute(ipc)
/* Perform s command */
sedcmd *ipc;			/* ptr to s command struct */
{
  int nullmatch;

  if (match(ipc->u.lhs, 0)) {	/* if no match */
	nullmatch = (loc1 == loc2);
	dosub(ipc->rhs);	/* perform it once */
  } else
	return(FALSE);		/* command fails */

  if (ipc->flags.global)	/* if global flag enabled */
	while (*loc2) {		/* cycle through possibles */
		if (nullmatch) loc2++;
		if (match(ipc->u.lhs, 1)) {	/* found another */
			nullmatch = (loc1 == loc2);
			dosub(ipc->rhs);	/* so substitute */
		} else		/* otherwise, */
			break;	/* we're done */
	}
  return(TRUE);			/* we succeeded */
}

static void dosub(rhsbuf)	/* uses linebuf, genbuf, spend */
 /* Generate substituted right-hand side (of s command) */
char *rhsbuf;			/* where to put the result */
{
  register char *lp, *sp, *rp;
  int c;

  /* Copy linebuf to genbuf up to location  1 */
  lp = linebuf;
  sp = genbuf;
  while (lp < loc1) *sp++ = *lp++;

  for (rp = rhsbuf; c = *rp++;) {
	if (c == '&') {
		sp = place(sp, loc1, loc2);
		continue;
	} else if (c & 0200 && (c &= 0177) >= '1' && c < MAXTAGS + '1') {
		sp = place(sp, brastart[c - '1'], bracend[c - '1']);
		continue;
	}
	*sp++ = c & 0177;
	if (sp >= genbuf + MAXBUF) fprintf(stderr, LTLMSG);
  }
  lp = loc2;
  loc2 = sp - genbuf + linebuf;
  while (*sp++ = *lp++)
	if (sp >= genbuf + MAXBUF) fprintf(stderr, LTLMSG);
  lp = linebuf;
  sp = genbuf;
  while (*lp++ = *sp++);
  spend = lp - 1;
}

static char *place(asp, al1, al2)	/* uses genbuf */
 /* Place chars at *al1...*(al1 - 1) at asp... in genbuf[] */
register char *asp, *al1, *al2;
{
  while (al1 < al2) {
	*asp++ = *al1++;
	if (asp >= genbuf + MAXBUF) fprintf(stderr, LTLMSG);
  }
  return(asp);
}

static void listto(p1, fp)
/* Write a hex dump expansion of *p1... to fp */
register char *p1;		/* the source */
FILE *fp;			/* output stream to write to */
{
  p1--;
  while (*p1++)
	if (isprint(*p1))
		putc(*p1, fp);	/* pass it through */
	else {
		putc('\\', fp);	/* emit a backslash */
		switch (*p1) {
		    case '\b':
			putc('b', fp);
			break;	/* BS */
		    case '\t':
			putc('t', fp);
			break;	/* TAB */
		    case '\n':
			putc('n', fp);
			break;	/* NL */
		    case '\r':
			putc('r', fp);
			break;	/* CR */
		    case '\33':
			putc('e', fp);
			break;	/* ESC */
		    default:
			fprintf(fp, "%02x", *p1 & 0xFF);
		}
	}
  putc('\n', fp);
}

static void truncated(h)
int h;
{
  static long last = 0L;

  if (lnum == last) return;
  last = lnum;

  fprintf(stderr, "sed: ");
  fprintf(stderr, h ? "hold space" : "line %ld", lnum);
  fprintf(stderr, " truncated to %d characters\n", MAXBUF);
}

static void command(ipc)
/* Execute compiled command pointed at by ipc */
sedcmd *ipc;
{
  static char holdsp[MAXHOLD + 1];	/* the hold space */
  static char *hspend = holdsp;	/* hold space end pointer */
  register char *p1, *p2;
  char *execp;
  int didsub;			/* true if last s succeeded */

  switch (ipc->command) {
      case ACMD:		/* append */
	*aptr++ = ipc;
	if (aptr >= appends + MAXAPPENDS) fprintf(stderr,
			"sed: too many appends after line %ld\n",
			lnum);
	*aptr = 0;
	break;

      case CCMD:		/* change pattern space */
	delete = TRUE;
	if (!ipc->flags.inrange || lastline) printf("%s\n", ipc->u.lhs);
	break;

      case DCMD:		/* delete pattern space */
	delete++;
	break;

      case CDCMD:		/* delete a line in hold space */
	p1 = p2 = linebuf;
	while (*p1 != '\n')
		if (delete = (*p1++ == 0)) return;
	p1++;
	while (*p2++ = *p1++) continue;
	spend = p2 - 1;
	jump++;
	break;

      case EQCMD:		/* show current line number */
	fprintf(stdout, "%ld\n", lnum);
	break;

      case GCMD:		/* copy hold space to pattern space */
	p1 = linebuf;
	p2 = holdsp;
	while (*p1++ = *p2++);
	spend = p1 - 1;
	break;

      case CGCMD:		/* append hold space to pattern space */
	*spend++ = '\n';
	p1 = spend;
	p2 = holdsp;
	do
		if (p1 > linebuf + MAXBUF) {
			truncated(0);
			p1[-1] = 0;
			break;
		}
	while (*p1++ = *p2++);

	spend = p1 - 1;
	break;

      case HCMD:		/* copy pattern space to hold space */
	p1 = holdsp;
	p2 = linebuf;
	while (*p1++ = *p2++);
	hspend = p1 - 1;
	break;

      case CHCMD:		/* append pattern space to hold space */
	*hspend++ = '\n';
	p1 = hspend;
	p2 = linebuf;
	do
		if (p1 > holdsp + MAXBUF) {
			truncated(1);
			p1[-1] = 0;
			break;
		}
	while (*p1++ = *p2++);

	hspend = p1 - 1;
	break;

      case ICMD:		/* insert text */
	printf("%s\n", ipc->u.lhs);
	break;

      case BCMD:		/* branch to label */
	jump = TRUE;
	break;

      case LCMD:		/* list text */
	listto(linebuf, (ipc->fout != NULL) ? ipc->fout : stdout);
	break;

      case NCMD:		/* read next line into pattern space */
	if (!nflag) puts(linebuf);	/* flush out the current line */
	if (aptr > appends) readout();	/* do pending a, r commands */
	if ((execp = getline(linebuf)) == BAD) {
		delete = TRUE;
		break;
	}
	spend = execp;
	anysub = FALSE;
	break;

      case CNCMD:		/* append next line to pattern space */
	if (aptr > appends) readout();
	*spend++ = '\n';
	if ((execp = getline(spend)) == BAD) {
		*--spend = 0;
		break;
	}
	spend = execp;
	anysub = FALSE;
	break;

      case PCMD:		/* print pattern space */
	puts(linebuf);
	break;

      case CPCMD:		/* print one line from pattern space */
cpcom:				/* so s command can jump here */
	for (p1 = linebuf; *p1 != '\n' && *p1 != '\0';) putc(*p1++, stdout);
	putc('\n', stdout);
	break;

      case QCMD:		/* quit the stream editor */
	if (!nflag) puts(linebuf);	/* flush out the current line */
	if (aptr > appends)
		readout();	/* do any pending a and r commands */
	quit(0);

      case RCMD:		/* read a file into the stream */
	*aptr++ = ipc;
	if (aptr >= appends + MAXAPPENDS) fprintf(stderr,
			"sed: too many reads after line %ld\n",
			lnum);
	*aptr = 0;
	break;

      case SCMD:		/* substitute RE */
	didsub = substitute(ipc);
	if (didsub) anysub = TRUE;
	if (ipc->flags.print && didsub)
		if (ipc->flags.print == TRUE)
			puts(linebuf);
		else
			goto cpcom;
	if (didsub && ipc->fout) fprintf(ipc->fout, "%s\n", linebuf);
	break;

      case TCMD:		/* branch on any s successful */
      case CTCMD:		/* branch on any s failed */
	if (anysub == (ipc->command == CTCMD))
		break;		/* no branch if any s failed, else */
	anysub = FALSE;
	jump = TRUE;		/* set up to jump to assoc'd label */
	break;

      case CWCMD:		/* write one line from pattern space */
	for (p1 = linebuf; *p1 != '\n' && *p1 != '\0';)
		putc(*p1++, ipc->fout);
	putc('\n', ipc->fout);
	break;

      case WCMD:		/* write pattern space to file */
	fprintf(ipc->fout, "%s\n", linebuf);
	break;

      case XCMD:		/* exchange pattern and hold spaces */
	p1 = linebuf;
	p2 = genbuf;
	while (*p2++ = *p1++) continue;
	p1 = holdsp;
	p2 = linebuf;
	while (*p2++ = *p1++) continue;
	spend = p2 - 1;
	p1 = genbuf;
	p2 = holdsp;
	while (*p2++ = *p1++) continue;
	hspend = p2 - 1;
	break;

      case YCMD:
	p1 = linebuf;
	p2 = ipc->u.lhs;
	while (*p1 = p2[*p1]) p1++;
	break;
  }
}

static void openfile(file)
char *file;
/* Replace stdin by given file */
{
  if (freopen(file, "r", stdin) == NULL) {
	fprintf(stderr, "sed: can't open %s\n", file);
	quit(1);
  }
}

static int c;			/* Will be the next char to read, a kind of
			 * lookahead */

static void get()
/* Read next character into c treating all argument files as run through cat */
{
  while ((c = getchar()) == EOF && --eargc >= 0) openfile(*eargv++);
}

static void initget()
/* Initialise character input */
{
  if (--eargc >= 0) openfile(*eargv++);	/* else input == stdin */
  get();
}

static char *getline(buf)
/* Get next line of text to be edited, return pointer to end */
register char *buf;		/* where to send the input */
{
  if (c == EOF) return BAD;

  lnum++;			/* we can read a new line */

  do {
	if (c == '\n') {
		get();
		break;
	}
	if (buf <= linebuf + MAXBUF) *buf++ = c;
	get();
  } while (c != EOF);

  if (c == EOF) lastline = TRUE;

  if (buf > linebuf + MAXBUF) {
	truncated(0);
	--buf;
  }
  *buf = 0;
  return buf;
}

static int Memcmp(a, b, count)
/* Return TRUE if *a... == *b... for count chars, FALSE otherwise */
register char *a, *b;
int count;
{
  while (count--)		/* look at count characters */
	if (*a++ != *b++)	/* if any are nonequal	 */
		return(FALSE);	/* return FALSE for false */
  return(TRUE);			/* compare succeeded */
}

static void readout()
/* Write file indicated by r command to output */
{
  register int t;		/* hold input char or EOF */
  FILE *fi;			/* ptr to file to be read */

  aptr = appends - 1;		/* arrange for pre-increment to work right */
  while (*++aptr)
	if ((*aptr)->command == ACMD)	/* process "a" cmd */
		printf("%s\n", (*aptr)->u.lhs);
	else {			/* process "r" cmd */
		if ((fi = fopen((*aptr)->u.lhs, "r")) == NULL) {
			fprintf(stderr, "sed: can't open %s\n",
				(*aptr)->u.lhs);
			continue;
		}
		while ((t = getc(fi)) != EOF) putc((char) t, stdout);
		fclose(fi);
	}
  aptr = appends;		/* reset the append ptr */
  *aptr = 0;
}

/* Sedexec.c ends here */
