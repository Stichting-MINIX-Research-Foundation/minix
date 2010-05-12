/* Copyright 1987 Brian Beattie Rights Reserved.
 *
 * Permission to copy and/or distribute granted under the
 * following conditions:
 *
 * 1). No charge may be made other than resonable charges
 *	for reproduction.
 *
 * 2). This notice must remain intact.
 *
 * 3). No further restrictions may be added.
 *
 */

/*	This program used to be in many little pieces, with this makefile:
.SUFFIXES:	.c .s

CFLAGS = -F

OBJS =	append.s catsub.s ckglob.s deflt.s del.s docmd.s doglob.s\
  doprnt.s doread.s dowrite.s ed.s egets.s find.s getfn.s getlst.s\
  getnum.s getone.s getptr.s getrhs.s gettxt.s ins.s join.s maksub.s\
  move.s optpat.s set.s setbuf.s subst.s getpat.s matchs.s amatch.s\
  unmkpat.s omatch.s makepat.s bitmap.s dodash.s esc.s System.s

ed:	$(OBJS)
  cc -T. -i -o ed $(OBJS)
*/

#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>

/****************************/

/*	tools.h	*/
/*
 *	#defines for non-printing ASCII characters
 */

#define NUL	0x00		/* ^@ */
#define EOS	0x00		/* end of string */
#define SOH	0x01		/* ^A */
#define STX	0x02		/* ^B */
#define ETX	0x03		/* ^C */
#define EOT	0x04		/* ^D */
#define ENQ	0x05		/* ^E */
#define ACK	0x06		/* ^F */
#define BEL	0x07		/* ^G */
#define BS	0x08		/* ^H */
#define HT	0x09		/* ^I */
#define LF	0x0a		/* ^J */
#define NL	'\n'
#define VT	0x0b		/* ^K */
#define FF	0x0c		/* ^L */
#define CR	0x0d		/* ^M */
#define SO	0x0e		/* ^N */
#define SI	0x0f		/* ^O */
#define DLE	0x10		/* ^P */
#define DC1	0x11		/* ^Q */
#define DC2	0x12		/* ^R */
#define DC3	0x13		/* ^S */
#define DC4	0x14		/* ^T */
#define NAK	0x15		/* ^U */
#define SYN	0x16		/* ^V */
#define ETB	0x17		/* ^W */
#define CAN	0x18		/* ^X */
#define EM	0x19		/* ^Y */
#define SUB	0x1a		/* ^Z */
#define ESC	0x1b		/* ^[ */
#define FS	0x1c		/* ^\ */
#define GS	0x1d		/* ^] */
#define RS	0x1e		/* ^^ */
#define US	0x1f		/* ^_ */
#define SP	0x20		/* space */
#define DEL	0x7f		/* DEL */


#define TRUE	1
#define FALSE	0
#define ERR	-2


/*	Definitions of meta-characters used in pattern matching
 *	routines.  LITCHAR & NCCL are only used as token identifiers;
 *	all the others are also both token identifier and actual symbol
 *	used in the regular expression.
 */


#define BOL	'^'
#define EOL	'$'
#define ANY	'.'
#define LITCHAR	'L'
#define	ESCAPE	'\\'
#define CCL	'['		/* Character class: [...] */
#define CCLEND	']'
#define NEGATE	'^'
#define NCCL	'!'		/* Negative character class [^...] */
#define CLOSURE	'*'
#define OR_SYM	'|'
#define DITTO	'&'
#define OPEN	'('
#define CLOSE	')'

/* Largest permitted size for an expanded character class.  (i.e. the class
 * [a-z] will expand into 26 symbols; [a-z0-9] will expand into 36.)
 */
#define CLS_SIZE	128

/*
 *	Tokens are used to hold pattern templates. (see makepat())
 */
typedef char BITMAP;

typedef struct token {
  char tok;
  char lchar;
  BITMAP *bitmap;
  struct token *next;
} TOKEN;

#define TOKSIZE sizeof (TOKEN)

/*
 *	An absolute maximun for strings.
 */

#define MAXSTR	132		/* Maximum numbers of characters in a line */


/* Macros */
#define max(a,b)	((a>b)?a:b)
#define min(a,b)	((a<b)?a:b)
#define toupper(c)	(c>='a'&&c<='z'?c-32:c)

/*	ed.h	*/
#define FATAL	(ERR-1)
struct line {
  int l_stat;			/* empty, mark */
  struct line *l_prev;
  struct line *l_next;
  char l_buff[1];
};

typedef struct line LINE;

#define LINFREE	1		/* entry not in use */
#define LGLOB	2		/* line marked global */

				/* max number of chars per line */
#define MAXLINE	(sizeof(int) == 2 ? 256 : 8192)
#define MAXPAT	256		/* max number of chars per replacement
				 * pattern */
				/* max file name size */
#define MAXFNAME (sizeof(int) == 2 ? 256 : 1024)

extern LINE line0;
extern int curln, lastln, line1, line2, nlines;
extern int nflg;		/* print line number flag */
extern int lflg;		/* print line in verbose mode */
extern char *inptr;		/* tty input buffer */
extern char linbuf[], *linptr;	/* current line */
extern int truncflg;		/* truncate long line flag */
extern int eightbit;		/* save eighth bit */
extern int nonascii;		/* count of non-ascii chars read */
extern int nullchar;		/* count of null chars read */
extern int truncated;		/* count of lines truncated */
extern int fchanged;		/* file changed */

#define nextln(l)	((l)+1 > lastln ? 0 : (l)+1)
#define prevln(l)	((l)-1 < 0 ? lastln : (l)-1)

/*	amatch.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(static char *match, (char *lin, TOKEN *pat, char *boln));
_PROTOTYPE(char *amatch, (char *lin, TOKEN *pat, char *boln));
_PROTOTYPE(int append, (int line, int glob));
_PROTOTYPE(BITMAP *makebitmap, (unsigned size));
_PROTOTYPE(int setbit, (unsigned c, char *map, unsigned val));
_PROTOTYPE(int testbit, (unsigned c, char *map));
_PROTOTYPE(char *catsub, (char *from, char *to, char *sub, char *new, char *newend));
_PROTOTYPE(int ckglob, (void));
_PROTOTYPE(int deflt, (int def1, int def2));
_PROTOTYPE(int del, (int from, int to));
_PROTOTYPE(int docmd, (int glob));
_PROTOTYPE(int dolst, (int line1, int line2));
_PROTOTYPE(char *dodash, (int delim, char *src, char *map));
_PROTOTYPE(int doglob, (void));
_PROTOTYPE(int doprnt, (int from, int to));
_PROTOTYPE(void prntln, (char *str, int vflg, int lin));
_PROTOTYPE(void putcntl, (int c, FILE *stream));
_PROTOTYPE(int doread, (int lin, char *fname));
_PROTOTYPE(int dowrite, (int from, int to, char *fname, int apflg));
_PROTOTYPE(void intr, (int sig));
_PROTOTYPE(int egets, (char *str, int size, FILE *stream));
_PROTOTYPE(int esc, (char **s));
_PROTOTYPE(int find, (TOKEN *pat, int dir));
_PROTOTYPE(char *getfn, (void));
_PROTOTYPE(int getlst, (void));
_PROTOTYPE(int getnum, (int first));
_PROTOTYPE(int getone, (void));
_PROTOTYPE(TOKEN *getpat, (char *arg));
_PROTOTYPE(LINE *getptr, (int num));
_PROTOTYPE(int getrhs, (char *sub));
_PROTOTYPE(char *gettxt, (int num));
_PROTOTYPE(int ins, (char *str));
_PROTOTYPE(int System, (char *c));
_PROTOTYPE(int join, (int first, int last));
_PROTOTYPE(TOKEN *makepat, (char *arg, int delim));
_PROTOTYPE(char *maksub, (char *sub, int subsz));
_PROTOTYPE(char *matchs, (char *line, TOKEN *pat, int ret_endp));
_PROTOTYPE(int move, (int num));
_PROTOTYPE(int transfer, (int num));
_PROTOTYPE(int omatch, (char **linp, TOKEN *pat, char *boln));
_PROTOTYPE(TOKEN *optpat, (void));
_PROTOTYPE(int set, (void));
_PROTOTYPE(int show, (void));
_PROTOTYPE(void relink, (LINE *a, LINE *x, LINE *y, LINE *b));
_PROTOTYPE(void clrbuf, (void));
_PROTOTYPE(void set_buf, (void));
_PROTOTYPE(int subst, (TOKEN *pat, char *sub, int gflg, int pflag));
_PROTOTYPE(void unmakepat, (TOKEN *head));

/*     Scans throught the pattern template looking for a match
 * with lin.  Each element of lin is compared with the template
 * until either a mis-match is found or the end of the template
 * is reached.  In the former case a 0 is returned; in the latter,
 * a pointer into lin (pointing to the character following the
 * matched pattern) is returned.
 *
 *	"lin"	is a pointer to the line being searched.
 *	"pat"	is a pointer to a template made by makepat().
 *	"boln"	is a pointer into "lin" which points at the
 *			character at the beginning of the line.
 */

char *paropen[9], *parclose[9];
int between, parnum;

char *amatch(lin, pat, boln)
char *lin;
TOKEN *pat;
char *boln;
{
  between = 0;
  parnum = 0;

  lin = match(lin, pat, boln);

  if (between) return 0;

  while (parnum < 9) {
	paropen[parnum] = parclose[parnum] = "";
	parnum++;
  }
  return lin;
}

static char *match(lin, pat, boln)
char *lin;
TOKEN *pat;
char *boln;
{
  register char *bocl, *rval, *strstart;

  if (pat == 0) return 0;

  strstart = lin;

  while (pat) {
	if (pat->tok == CLOSURE && pat->next) {
		/* Process a closure: first skip over the closure
		 * token to the object to be repeated.  This object
		 * can be a character class. */

		pat = pat->next;

		/* Now match as many occurrences of the closure
		 * pattern as possible. */
		bocl = lin;

		while (*lin && omatch(&lin, pat, boln));

		/* 'Lin' now points to the character that made made
		 * us fail.  Now go on to process the rest of the
		 * string.  A problem here is a character following
		 * the closure which could have been in the closure.
		 * For example, in the pattern "[a-z]*t" (which
		 * matches any lower-case word ending in a t), the
		 * final 't' will be sucked up in the while loop.
		 * So, if the match fails, we back up a notch and try
		 * to match the rest of the string again, repeating
		 * this process recursively until we get back to the
		 * beginning of the closure.  The recursion goes, at
		 * most two levels deep. */

		if (pat = pat->next) {
			int savbtwn = between;
			int savprnm = parnum;

			while (bocl <= lin) {
				if (rval = match(lin, pat, boln)) {
					/* Success */
					return(rval);
				} else {
					--lin;
					between = savbtwn;
					parnum = savprnm;
				}
			}
			return(0);	/* match failed */
		}
	} else if (pat->tok == OPEN) {
		if (between || parnum >= 9) return 0;
		paropen[parnum] = lin;
		between = 1;
		pat = pat->next;
	} else if (pat->tok == CLOSE) {
		if (!between) return 0;
		parclose[parnum++] = lin;
		between = 0;
		pat = pat->next;
	} else if (omatch(&lin, pat, boln)) {
		pat = pat->next;
	} else {
		return(0);
	}
  }

  /* Note that omatch() advances lin to point at the next character to
   * be matched.  Consequently, when we reach the end of the template,
   * lin will be pointing at the character following the last character
   * matched.  The exceptions are templates containing only a BOLN or
   * EOLN token.  In these cases omatch doesn't advance.
   * 
   * A philosophical point should be mentioned here.  Is $ a position or a
   * character? (i.e. does $ mean the EOL character itself or does it
   * mean the character at the end of the line.)  I decided here to
   * make it mean the former, in order to make the behavior of match()
   * consistent.  If you give match the pattern ^$ (match all lines
   * consisting only of an end of line) then, since something has to be
   * returned, a pointer to the end of line character itself is
   * returned. */

  return((char *) max(strstart, lin));
}

/*	append.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int append(line, glob)
int line, glob;
{
  int stat;
  char lin[MAXLINE];

  if (glob) return(ERR);
  curln = line;
  while (1) {
	if (nflg) printf("%6d. ", curln + 1);

	if (fgets(lin, MAXLINE, stdin) == NULL) return(EOF);
	if (lin[0] == '.' && lin[1] == '\n') return (0);
	stat = ins(lin);
	if (stat < 0) return(ERR);

  }
}

/*	bitmap.c	*/
/*
 *	BITMAP.C -	makebitmap, setbit, testbit
 *			bit-map manipulation routines.
 *
 *	Copyright (c) Allen I. Holub, all rights reserved.  This program may
 *		for copied for personal, non-profit use only.
 *
 */

#ifdef DEBUG
/* #include <stdio.h> */
#endif

/* #include "tools.h" */


BITMAP *makebitmap(size)
unsigned size;
{
  /* Make a bit map with "size" bits.  The first entry in the map is an
   * "unsigned int" representing the maximum bit.  The map itself is
   * concatenated to this integer. Return a pointer to a map on
   * success, 0 if there's not enough memory. */

  unsigned *map, numbytes;

  numbytes = (size >> 3) + ((size & 0x07) ? 1 : 0);

#ifdef DEBUG
  printf("Making a %d bit map (%d bytes required)\n", size, numbytes);
#endif

  if (map = (unsigned *) malloc(numbytes + sizeof(unsigned))) {
	*map = size;
	memset(map + 1, 0, numbytes);
  }

  return((BITMAP *) map);
}

int setbit(c, map, val)
unsigned c, val;
char *map;
{
  /* Set bit c in the map to val. If c > map-size, 0 is returned, else
   * 1 is returned. */

  if (c >= *(unsigned *) map)	/* if c >= map size */
	return 0;

  map += sizeof(unsigned);	/* skip past size */

  if (val)
	map[c >> 3] |= 1 << (c & 0x07);
  else
	map[c >> 3] &= ~(1 << (c & 0x07));

  return 1;
}

int testbit(c, map)
unsigned c;
char *map;
{
  /* Return 1 if the bit corresponding to c in map is set. 0 if it is not. */

  if (c >= *(unsigned *) map) return 0;

  map += sizeof(unsigned);

  return(map[c >> 3] & (1 << (c & 0x07)));
}

/*	catsub.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

extern char *paropen[9], *parclose[9];

char *catsub(from, to, sub, new, newend)
char *from, *to, *sub, *new, *newend;
{
  char *cp, *cp2;

  for (cp = new; *sub != EOS && cp < newend;) {
	if (*sub == DITTO) for (cp2 = from; cp2 < to;) {
			*cp++ = *cp2++;
			if (cp >= newend) break;
		}
	else if (*sub == ESCAPE) {
		sub++;
		if ('1' <= *sub && *sub <= '9') {
			char *parcl = parclose[*sub - '1'];

			for (cp2 = paropen[*sub - '1']; cp2 < parcl;) {
				*cp++ = *cp2++;
				if (cp >= newend) break;
			}
		} else
			*cp++ = *sub;
	} else
		*cp++ = *sub;

	sub++;
  }

  return(cp);
}

/*	ckglob.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int ckglob()
{
  TOKEN *glbpat;
  char c, delim;
  char lin[MAXLINE];
  int num;
  LINE *ptr;

  c = *inptr;

  if (c != 'g' && c != 'v') return(0);

  if (deflt(1, lastln) < 0) return(ERR);

  delim = *++inptr;
  if (delim <= ' ') return(ERR);

  glbpat = optpat();

  if (*inptr == delim) inptr++;

  ptr = getptr(1);
  for (num = 1; num <= lastln; num++) {
	ptr->l_stat &= ~LGLOB;
	if (line1 <= num && num <= line2) {
		strcpy(lin, ptr->l_buff);
		strcat(lin, "\n");
		if (matchs(lin, glbpat, 0)) {
			if (c == 'g') ptr->l_stat |= LGLOB;
		} else {
			if (c == 'v') ptr->l_stat |= LGLOB;
		}
	}
	ptr = ptr->l_next;
  }
  return(1);
}

/*	deflt.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int deflt(def1, def2)
int def1, def2;
{
  if (nlines == 0) {
	line1 = def1;
	line2 = def2;
  }
  if (line1 > line2 || line1 <= 0) return(ERR);
  return(0);
}

/*	del.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int del(from, to)
int from, to;
{
  LINE *first, *last, *next, *tmp;

  if (from < 1) from = 1;
  first = getptr(prevln(from));
  last = getptr(nextln(to));
  next = first->l_next;
  while (next != last && next != &line0) {
	tmp = next->l_next;
	free((char *) next);
	next = tmp;
  }
  relink(first, last, first, last);
  lastln -= (to - from) + 1;
  curln = prevln(from);
  return(0);
}

/*	docmd.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

char fname[MAXFNAME];
int fchanged;
extern int nofname;

extern int mark[];

int docmd(glob)
int glob;
{
  static char rhs[MAXPAT];
  TOKEN *subpat;
  int c, err, line3;
  int apflg, pflag, gflag;
  int nchng;
  char *fptr;

  pflag = FALSE;
  while (*inptr == SP && *inptr == HT) inptr++;

  c = *inptr++;

  switch (c) {
      case NL:
	if (nlines == 0) {
		if ((line2 = nextln(curln)) == 0) return(ERR);
	}
	curln = line2;
	return(1);
	break;

      case '=':	printf("%d\n", line2);	break;

      case 'a':
	if (*inptr != NL || nlines > 1) return(ERR);

	if (append(line1, glob) < 0) return(ERR);;
	fchanged = TRUE;
	break;

      case 'c':
	if (*inptr != NL) return(ERR);

	if (deflt(curln, curln) < 0) return(ERR);

	if (del(line1, line2) < 0) return(ERR);
	if (append(curln, glob) < 0) return (ERR);
	fchanged = TRUE;
	break;

      case 'd':
	if (*inptr != NL) return(ERR);

	if (deflt(curln, curln) < 0) return(ERR);

	if (del(line1, line2) < 0) return(ERR);
	if (nextln(curln) != 0) curln = nextln(curln);
	fchanged = TRUE;
	break;

      case 'e':
	if (nlines > 0) return(ERR);
	if (fchanged) {
		fchanged = FALSE;
		return(ERR);
	}

	/* FALL THROUGH */
      case 'E':
	if (nlines > 0) return(ERR);

	if (*inptr != ' ' && *inptr != HT && *inptr != NL) return(ERR);

	if ((fptr = getfn()) == NULL) return(ERR);

	clrbuf();
	if ((err = doread(0, fptr)) < 0) return(err);

	strcpy(fname, fptr);
	fchanged = FALSE;
	break;

      case 'f':
	if (nlines > 0) return(ERR);

	if (*inptr != ' ' && *inptr != HT && *inptr != NL) return(ERR);

	if ((fptr = getfn()) == NULL) return(ERR);

	if (nofname)
		printf("%s\n", fname);
	else
		strcpy(fname, fptr);
	break;

      case 'i':
	if (*inptr != NL || nlines > 1) return(ERR);

	if (append(prevln(line1), glob) < 0) return(ERR);
	fchanged = TRUE;
	break;

      case 'j':
	if (*inptr != NL || deflt(curln, curln + 1) < 0) return(ERR);

	if (join(line1, line2) < 0) return(ERR);
	break;

      case 'k':
	while (*inptr == ' ' || *inptr == HT) inptr++;

	if (*inptr < 'a' || *inptr > 'z') return ERR;
	c = *inptr++;

	if (*inptr != ' ' && *inptr != HT && *inptr != NL) return(ERR);

	mark[c - 'a'] = line1;
	break;

      case 'l':
	if (*inptr != NL) return(ERR);
	if (deflt(curln, curln) < 0) return (ERR);
	if (dolst(line1, line2) < 0) return (ERR);
	break;

      case 'm':
	if ((line3 = getone()) < 0) return(ERR);
	if (deflt(curln, curln) < 0) return (ERR);
	if (move(line3) < 0) return (ERR);
	fchanged = TRUE;
	break;

      case 'P':
      case 'p':
	if (*inptr != NL) return(ERR);
	if (deflt(curln, curln) < 0) return (ERR);
	if (doprnt(line1, line2) < 0) return (ERR);
	break;

      case 'q':
	if (fchanged) {
		fchanged = FALSE;
		return(ERR);
	}

	/* FALL THROUGH */
      case 'Q':
	if (*inptr == NL && nlines == 0 && !glob)
		return(EOF);
	else
		return(ERR);

      case 'r':
	if (nlines > 1) return(ERR);

	if (nlines == 0) line2 = lastln;

	if (*inptr != ' ' && *inptr != HT && *inptr != NL) return(ERR);

	if ((fptr = getfn()) == NULL) return(ERR);

	if ((err = doread(line2, fptr)) < 0) return(err);
	fchanged = TRUE;
	break;

      case 's':
	if (*inptr == 'e') return(set());
	while (*inptr == SP || *inptr == HT) inptr++;
	if ((subpat = optpat()) == NULL) return (ERR);
	if ((gflag = getrhs(rhs)) < 0) return (ERR);
	if (*inptr == 'p') pflag++;
	if (deflt(curln, curln) < 0) return (ERR);
	if ((nchng = subst(subpat, rhs, gflag, pflag)) < 0) return (ERR);
	if (nchng) fchanged = TRUE;
	break;

      case 't':
	if ((line3 = getone()) < 0) return(ERR);
	if (deflt(curln, curln) < 0) return (ERR);
	if (transfer(line3) < 0) return (ERR);
	fchanged = TRUE;
	break;

      case 'W':
      case 'w':
	apflg = (c == 'W');

	if (*inptr != ' ' && *inptr != HT && *inptr != NL) return(ERR);

	if ((fptr = getfn()) == NULL) return(ERR);

	if (deflt(1, lastln) < 0) return(ERR);
	if (dowrite(line1, line2, fptr, apflg) < 0) return (ERR);
	fchanged = FALSE;
	break;

      case 'x':
	if (*inptr == NL && nlines == 0 && !glob) {
		if ((fptr = getfn()) == NULL) return(ERR);
		if (dowrite(1, lastln, fptr, 0) >= 0) return (EOF);
	}
	return(ERR);

      case 'z':
	if (deflt(curln, curln) < 0) return(ERR);

	switch (*inptr) {
	    case '-':
		if (doprnt(line1 - 21, line1) < 0) return(ERR);
		break;

	    case '.':
		if (doprnt(line1 - 11, line1 + 10) < 0) return(ERR);
		break;

	    case '+':
	    case '\n':
		if (doprnt(line1, line1 + 21) < 0) return(ERR);
		break;
	}
	break;

      default:	return(ERR);
}
  return(0);
}

int dolst(line1, line2)
int line1, line2;
{
  int oldlflg = lflg, p;

  lflg = 1;
  p = doprnt(line1, line2);
  lflg = oldlflg;

  return p;
}

/*	dodash.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

/*	Expand the set pointed to by *src into dest.
 *	Stop at delim.  Return 0 on error or size of
 *	character class on success.  Update *src to
 *	point at delim.  A set can have one element
 *	{x} or several elements ( {abcdefghijklmnopqrstuvwxyz}
 *	and {a-z} are equivalent ).  Note that the dash
 *	notation is expanded as sequential numbers.
 *	This means (since we are using the ASCII character
 *	set) that a-Z will contain the entire alphabet
 *	plus the symbols: [\]^_`.  The maximum number of
 *	characters in a character class is defined by maxccl.
 */
char *dodash(delim, src, map)
int delim;
char *src, *map;
{

  register int first, last;
  char *start;

  start = src;

  while (*src && *src != delim) {
	if (*src != '-') setbit(esc(&src), map, 1);

	else if (src == start || *(src + 1) == delim)
		setbit('-', map, 1);
	else {
		src++;

		if (*src < *(src - 2)) {
			first = *src;
			last = *(src - 2);
		} else {
			first = *(src - 2);
			last = *src;
		}

		while (++first <= last) setbit(first, map, 1);

	}
	src++;
  }
  return(src);
}

/*	doglob.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int doglob()
{
  int lin, stat;
  char *cmd;
  LINE *ptr;

  cmd = inptr;

  while (1) {
	ptr = getptr(1);
	for (lin = 1; lin <= lastln; lin++) {
		if (ptr->l_stat & LGLOB) break;
		ptr = ptr->l_next;
	}
	if (lin > lastln) break;

	ptr->l_stat &= ~LGLOB;
	curln = lin;
	inptr = cmd;
	if ((stat = getlst()) < 0) return(stat);
	if ((stat = docmd(1)) < 0) return (stat);
  }
  return(curln);
}

/*	doprnt.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int doprnt(from, to)
int from, to;
{
  int i;
  LINE *lptr;

  from = from < 1 ? 1 : from;
  to = to > lastln ? lastln : to;

  if (to != 0) {
	lptr = getptr(from);
	for (i = from; i <= to; i++) {
		prntln(lptr->l_buff, lflg, (nflg ? i : 0));
		lptr = lptr->l_next;
	}
	curln = to;
  }
  return(0);
}

void prntln(str, vflg, lin)
char *str;
int vflg, lin;
{
  if (lin) printf("%7d ", lin);
  while (*str && *str != NL) {
	if (*str < ' ' || *str >= 0x7f) {
		switch (*str) {
		    case '\t':
			if (vflg)
				putcntl(*str, stdout);
			else
				putc(*str, stdout);
			break;

		    case DEL:
			putc('^', stdout);
			putc('?', stdout);
			break;

		    default:
			putcntl(*str, stdout);
			break;
		}
	} else
		putc(*str, stdout);
	str++;
  }
  if (vflg) putc('$', stdout);
  putc('\n', stdout);
}

void putcntl(c, stream)
char c;
FILE *stream;
{
  putc('^', stream);
  putc((c & 31) | '@', stream);
}

/*	doread.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

extern int diag;

int doread(lin, fname)
int lin;
char *fname;
{
  FILE *fp;
  int err;
  long bytes;
  int lines;
  static char str[MAXLINE];

  err = 0;
  nonascii = nullchar = truncated = 0;

  if (diag) printf("\"%s\" ", fname);
  if ((fp = fopen(fname, "r")) == NULL) {
	printf("file open err\n");
	return(ERR);
  }
  curln = lin;
  for (lines = 0, bytes = 0; (err = egets(str, MAXLINE, fp)) > 0;) {
	bytes += strlen(str);
	if (ins(str) < 0) {
		printf("file insert error\n");
		err++;
		break;
	}
	lines++;
  }
  fclose(fp);
  if (err < 0) return(err);
  if (diag) {
	printf("%d lines %ld bytes", lines, bytes);
	if (nonascii) printf(" [%d non-ascii]", nonascii);
	if (nullchar) printf(" [%d nul]", nullchar);
	if (truncated) printf(" [%d lines truncated]", truncated);
	printf("\n");
  }
  return(err);
}

/*	dowrite.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int dowrite(from, to, fname, apflg)
int from, to;
char *fname;
int apflg;
{
  FILE *fp;
  int lin, err;
  int lines;
  long bytes;
  char *str;
  LINE *lptr;

  err = 0;

  lines = bytes = 0;
  if (diag) printf("\"%s\" ", fname);
  if ((fp = fopen(fname, (apflg ? "a" : "w"))) == NULL) {
	printf("file open error\n");
	return(ERR);
  }
  lptr = getptr(from);
  for (lin = from; lin <= to; lin++) {
	str = lptr->l_buff;
	lines++;
	bytes += strlen(str) + 1;
	if (fputs(str, fp) == EOF) {
		printf("file write error\n");
		err++;
		break;
	}
	fputc('\n', fp);
	lptr = lptr->l_next;
  }
  if (diag) printf("%d lines %ld bytes\n", lines, bytes);
  fclose(fp);
  return(err);
}

/*	ed.c	*/
/* Copyright 1987 Brian Beattie Rights Reserved.
 *
 * Permission to copy and/or distribute granted under the
 * following conditions:
 *
 * 1). No charge may be made other than resonable charges
 *	for reproduction.
 *
 * 2). This notice must remain intact.
 *
 * 3). No further restrictions may be added.
 *
 */
/* #include <stdio.h> */
/* #include <signal.h> */
/* #include "tools.h" */
/* #include "ed.h" */
#include <setjmp.h>
jmp_buf env;

LINE line0;
int curln = 0;
int lastln = 0;
char *inptr;
static char inlin[MAXLINE];
int nflg, lflg;
int line1, line2, nlines;
extern char fname[];
int version = 1;
int diag = 1;

void intr(sig)
int sig;
{
  printf("?\n");
  longjmp(env, 1);
}

int main(argc, argv)
int argc;
char **argv;
{
  int stat, i, doflush;

  set_buf();
  doflush = isatty(1);

  if (argc > 1 && (strcmp(argv[1], "-") == 0 || strcmp(argv[1], "-s") == 0)) {
	diag = 0;
	argc--;
	argv++;
  }
  if (argc > 1) {
	for (i = 1; i < argc; i++) {
		if (doread(0, argv[i]) == 0) {
			curln = 1;
			strcpy(fname, argv[i]);
			break;
		}
	}
  }
  while (1) {
	setjmp(env);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN) signal(SIGINT, intr);

	if (doflush) fflush(stdout);

	if (fgets(inlin, sizeof(inlin), stdin) == NULL) {
		break;
	}
	for (;;) {
		inptr = strchr(inlin, EOS);
		if (inptr >= inlin+2 && inptr[-2] == '\\' && inptr[-1] == NL) {
			inptr[-1] = 'n';
			if (fgets(inptr, sizeof(inlin) - (inptr - inlin),
						stdin) == NULL) break;
		} else {
			break;
		}
	}
	if (*inlin == '!') {
		if ((inptr = strchr(inlin, NL)) != NULL) *inptr = EOS;
		System(inlin + 1);
		continue;
	}
	inptr = inlin;
	if (getlst() >= 0)
		if ((stat = ckglob()) != 0) {
			if (stat >= 0 && (stat = doglob()) >= 0) {
				curln = stat;
				continue;
			}
		} else {
			if ((stat = docmd(0)) >= 0) {
				if (stat == 1) doprnt(curln, curln);
				continue;
			}
		}
	if (stat == EOF) {
		exit(0);
	}
	if (stat == FATAL) {
		fputs("FATAL ERROR\n", stderr);
		exit(1);
	}
	printf("?\n");
  }
  return(0);
}

/*	egets.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int eightbit = 1;		/* save eight bit */
int nonascii, nullchar, truncated;
int egets(str, size, stream)
char *str;
int size;
FILE *stream;
{
  int c, count;
  char *cp;

  for (count = 0, cp = str; size > count;) {
	c = getc(stream);
	if (c == EOF) {
		*cp++ = '\n';
		*cp = EOS;
		if (count) {
			printf("[Incomplete last line]\n");
		}
		return(count);
	}
	if (c == NL) {
		*cp++ = c;
		*cp = EOS;
		return(++count);
	}
	if (c > 127) {
		if (!eightbit)	/* if not saving eighth bit */
			c = c & 127;	/* strip eigth bit */
		nonascii++;	/* count it */
	}
	if (c) {
		*cp++ = c;	/* not null, keep it */
		count++;
	} else
		nullchar++;	/* count nulls */
  }
  str[count - 1] = EOS;
  if (c != NL) {
	printf("truncating line\n");
	truncated++;
	while ((c = getc(stream)) != EOF)
		if (c == NL) break;
  }
  return(count);
}

/*	esc.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

/* Map escape sequences into their equivalent symbols.  Returns the
 * correct ASCII character.  If no escape prefix is present then s
 * is untouched and *s is returned, otherwise **s is advanced to point
 * at the escaped character and the translated character is returned.
 */
int esc(s)
char **s;
{
  register int rval;


  if (**s != ESCAPE) {
	rval = **s;
  } else {
	(*s)++;

	switch (toupper(**s)) {
	    case '\000':	rval = ESCAPE;	break;
	    case 'S':	rval = ' ';	break;
	    case 'N':	rval = '\n';	break;
	    case 'T':	rval = '\t';	break;
	    case 'B':	rval = '\b';	break;
	    case 'R':	rval = '\r';	break;
	    default:	rval = **s;	break;
	}
  }

  return(rval);
}

/*	find.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int find(pat, dir)
TOKEN *pat;
int dir;
{
  int i, num;
  char lin[MAXLINE];
  LINE *ptr;

  num = curln;
  ptr = getptr(curln);
  num = (dir ? nextln(num) : prevln(num));
  ptr = (dir ? ptr->l_next : ptr->l_prev);
  for (i = 0; i < lastln; i++) {
	if (num == 0) {
		num = (dir ? nextln(num) : prevln(num));
		ptr = (dir ? ptr->l_next : ptr->l_prev);
	}
	strcpy(lin, ptr->l_buff);
	strcat(lin, "\n");
	if (matchs(lin, pat, 0)) {
		return(num);
	}
	num = (dir ? nextln(num) : prevln(num));
	ptr = (dir ? ptr->l_next : ptr->l_prev);
  }
  return(ERR);
}

/*	getfn.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

extern char fname[MAXFNAME];
int nofname;

char *getfn()
{
  static char file[256];
  char *cp;

  if (*inptr == NL) {
	nofname = TRUE;
	strcpy(file, fname);
  } else {
	nofname = FALSE;
	while (*inptr == SP || *inptr == HT) inptr++;

	cp = file;
	while (*inptr && *inptr != NL && *inptr != SP && *inptr != HT) {
		*cp++ = *inptr++;
	}
	*cp = '\0';

	if (strlen(file) == 0) {
		printf("bad file name\n");
		return(NULL);
	}
  }

  if (strlen(file) == 0) {
	printf("no file name\n");
	return(NULL);
  }
  return(file);
}

/*	getlst.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int getlst()
{
  int num;

  line2 = 0;
  for (nlines = 0; (num = getone()) >= 0;) {
	line1 = line2;
	line2 = num;
	nlines++;
	if (*inptr != ',' && *inptr != ';') break;
	if (*inptr == ';') curln = num;
	inptr++;
  }
  nlines = min(nlines, 2);
  if (nlines == 0) line2 = curln;
  if (nlines <= 1) line1 = line2;

  if (num == ERR)
	return(num);
  else
	return(nlines);
}

/*	getnum.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int mark['z' - 'a' + 1];

int getnum(first)
int first;
{
  TOKEN *srchpat;
  int num;
  char c;

  while (*inptr == SP || *inptr == HT) inptr++;

  if (*inptr >= '0' && *inptr <= '9') {	/* line number */
	for (num = 0; *inptr >= '0' && *inptr <= '9';) {
		num = (num * 10) + *inptr - '0';
		inptr++;
	}
	return num;
  }
  switch (c = *inptr) {
      case '.':
	inptr++;
	return(curln);

      case '$':
	inptr++;
	return(lastln);

      case '/':
      case '?':
	srchpat = optpat();
	if (*inptr == c) inptr++;
	return(find(srchpat, c == '/' ? 1 : 0));

      case '-':
      case '+':
	return(first ? curln : 1);

      case '\'':
	inptr++;
	if (*inptr < 'a' || *inptr > 'z') return(EOF);

	return mark[*inptr++ - 'a'];

      default:
	return(first ? EOF : 1);/* unknown address */
  }
}

/*	getone.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

#define FIRST 1
#define NOTFIRST 0

int getone()
{
  int c, i, num;

  if ((num = getnum(FIRST)) >= 0) {
	while (1) {
		while (*inptr == SP || *inptr == HT) inptr++;

		if (*inptr != '+' && *inptr != '-') break;
		c = *inptr++;

		if ((i = getnum(NOTFIRST)) < 0) return(i);

		if (c == '+') {
			num += i;
		} else {
			num -= i;
		}
	}
  }
  return(num > lastln ? ERR : num);
}

/*	getpat.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

/* Translate arg into a TOKEN string */
TOKEN *
 getpat(arg)
char *arg;
{

  return(makepat(arg, '\000'));
}

/*	getptr.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

LINE *
 getptr(num)
int num;
{
  LINE *ptr;
  int j;

  if (2 * num > lastln && num <= lastln) {	/* high line numbers */
	ptr = line0.l_prev;
	for (j = lastln; j > num; j--) ptr = ptr->l_prev;
  } else {			/* low line numbers */
	ptr = &line0;
	for (j = 0; j < num; j++) ptr = ptr->l_next;
  }
  return(ptr);
}

/*	getrhs.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int getrhs(sub)
char *sub;
{
  if (inptr[0] == NL || inptr[1] == NL)	/* check for eol */
	return(ERR);

  if (maksub(sub, MAXPAT) == NULL) return(ERR);

  inptr++;			/* skip over delimter */
  while (*inptr == SP || *inptr == HT) inptr++;
  if (*inptr == 'g') {
	inptr++;
	return(1);
  }
  return(0);
}

/*	gettxt.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

char *
 gettxt(num)
int num;
{
  LINE *lin;
  static char txtbuf[MAXLINE];

  lin = getptr(num);
  strcpy(txtbuf, lin->l_buff);
  strcat(txtbuf, "\n");
  return(txtbuf);
}

/*	ins.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int ins(str)
char *str;
{
  char buf[MAXLINE], *cp;
  LINE *new, *cur, *nxt;

  cp = buf;
  while (1) {
	if ((*cp = *str++) == NL) *cp = EOS;
	if (*cp) {
		cp++;
		continue;
	}
	if ((new = (LINE *) malloc(sizeof(LINE) + strlen(buf))) == NULL)
		return(ERR);	/* no memory */

	new->l_stat = 0;
	strcpy(new->l_buff, buf);	/* build new line */
	cur = getptr(curln);	/* get current line */
	nxt = cur->l_next;	/* get next line */
	relink(cur, new, new, nxt);	/* add to linked list */
	relink(new, nxt, cur, new);
	lastln++;
	curln++;

	if (*str == EOS)	/* end of line ? */
		return(1);

	cp = buf;
  }
}

/*	join.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

extern int fchanged;

int join(first, last)
int first, last;
{
  char buf[MAXLINE];
  char *cp = buf, *str;
  int num;

  if (first <= 0 || first > last || last > lastln) return(ERR);
  if (first == last) {
	curln = first;
	return 0;
  }
  for (num = first; num <= last; num++) {
	str = gettxt(num);

	while (*str != NL && cp < buf + MAXLINE - 1) *cp++ = *str++;

	if (cp == buf + MAXLINE - 1) {
		printf("line too long\n");
		return(ERR);
	}
  }
  *cp++ = NL;
  *cp = EOS;
  del(first, last);
  curln = first - 1;
  ins(buf);
  fchanged = TRUE;
  return 0;
}

/*	makepat.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

/* Make a pattern template from the strinng pointed to by arg.  Stop
 * when delim or '\000' or '\n' is found in arg.  Return a pointer to
 * the pattern template.
 *
 * The pattern template used here are somewhat different than those
 * used in the "Software Tools" book; each token is a structure of
 * the form TOKEN (see tools.h).  A token consists of an identifier,
 * a pointer to a string, a literal character and a pointer to another
 * token.  This last is 0 if there is no subsequent token.
 *
 * The one strangeness here is caused (again) by CLOSURE which has
 * to be put in front of the previous token.  To make this insertion a
 * little easier, the 'next' field of the last to point at the chain
 * (the one pointed to by 'tail) is made to point at the previous node.
 * When we are finished, tail->next is set to 0.
 */
TOKEN *
 makepat(arg, delim)
char *arg;
int delim;
{
  TOKEN *head, *tail, *ntok;
  int error;

  /* Check for characters that aren't legal at the beginning of a template. */

  if (*arg == '\0' || *arg == delim || *arg == '\n' || *arg == CLOSURE)
	return(0);

  error = 0;
  tail = head = NULL;

  while (*arg && *arg != delim && *arg != '\n' && !error) {
	ntok = (TOKEN *) malloc(TOKSIZE);
	ntok->lchar = '\000';
	ntok->next = 0;

	switch (*arg) {
	    case ANY:	ntok->tok = ANY;	break;

	    case BOL:
		if (head == 0)	/* then this is the first symbol */
			ntok->tok = BOL;
		else
			ntok->tok = LITCHAR;
		ntok->lchar = BOL;
		break;

	    case EOL:
		if (*(arg + 1) == delim || *(arg + 1) == '\000' ||
		    *(arg + 1) == '\n') {
			ntok->tok = EOL;
		} else {
			ntok->tok = LITCHAR;
			ntok->lchar = EOL;
		}
		break;

	    case CLOSURE:
		if (head != 0) {
			switch (tail->tok) {
			    case BOL:
			    case EOL:
			    case CLOSURE:
				return(0);

			    default:
				ntok->tok = CLOSURE;
			}
		}
		break;

	    case CCL:

		if (*(arg + 1) == NEGATE) {
			ntok->tok = NCCL;
			arg += 2;
		} else {
			ntok->tok = CCL;
			arg++;
		}

		if (ntok->bitmap = makebitmap(CLS_SIZE))
			arg = dodash(CCLEND, arg, ntok->bitmap);
		else {
			fprintf(stderr, "Not enough memory for pat\n");
			error = 1;
		}
		break;

	    default:
		if (*arg == ESCAPE && *(arg + 1) == OPEN) {
			ntok->tok = OPEN;
			arg++;
		} else if (*arg == ESCAPE && *(arg + 1) == CLOSE) {
			ntok->tok = CLOSE;
			arg++;
		} else {
			ntok->tok = LITCHAR;
			ntok->lchar = esc(&arg);
		}
	}

	if (error || ntok == 0) {
		unmakepat(head);
		return(0);
	} else if (head == 0) {
		/* This is the first node in the chain. */

		ntok->next = 0;
		head = tail = ntok;
	} else if (ntok->tok != CLOSURE) {
		/* Insert at end of list (after tail) */

		tail->next = ntok;
		ntok->next = tail;
		tail = ntok;
	} else if (head != tail) {
		/* More than one node in the chain.  Insert the
		 * CLOSURE node immediately in front of tail. */

		(tail->next)->next = ntok;
		ntok->next = tail;
	} else {
		/* Only one node in the chain,  Insert the CLOSURE
		 * node at the head of the linked list. */

		ntok->next = head;
		tail->next = ntok;
		head = ntok;
	}
	arg++;
  }

  tail->next = 0;
  return(head);
}

/*	maksub.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

char *
 maksub(sub, subsz)
char *sub;
int subsz;
{
  int size;
  char delim, *cp;

  size = 0;
  cp = sub;

  delim = *inptr++;
  for (size = 0; *inptr != delim && *inptr != NL && size < subsz; size++) {
	if (*inptr == '&') {
		*cp++ = DITTO;
		inptr++;
	} else if ((*cp++ = *inptr++) == ESCAPE) {
		if (size >= subsz) return(NULL);

		switch (toupper(*inptr)) {
		    case NL:	*cp++ = ESCAPE;		break;
			break;
		    case 'S':
			*cp++ = SP;
			inptr++;
			break;
		    case 'N':
			*cp++ = NL;
			inptr++;
			break;
		    case 'T':
			*cp++ = HT;
			inptr++;
			break;
		    case 'B':
			*cp++ = BS;
			inptr++;
			break;
		    case 'R':
			*cp++ = CR;
			inptr++;
			break;
		    case '0':{
				int i = 3;
				*cp = 0;
				do {
					if (*++inptr < '0' || *inptr > '7')
						break;

					*cp = (*cp << 3) | (*inptr - '0');
				} while (--i != 0);
				cp++;
			} break;
		    default:	*cp++ = *inptr++;	break;
		}
	}
  }
  if (size >= subsz) return(NULL);

  *cp = EOS;
  return(sub);
}

/*	matchs.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

/* Compares line and pattern.  Line is a character string while pat
 * is a pattern template made by getpat().
 * Returns:
 *	1. A zero if no match was found.
 *
 *	2. A pointer to the last character satisfing the match
 *	   if ret_endp is non-zero.
 *
 *	3. A pointer to the beginning of the matched string if
 *	   ret_endp is zero.
 *
 * e.g.:
 *
 *	matchs ("1234567890", getpat("4[0-9]*7), 0);
 * will return a pointer to the '4', while:
 *
 *	matchs ("1234567890", getpat("4[0-9]*7), 1);
 * will return a pointer to the '7'.
 */
char *
 matchs(line, pat, ret_endp)
char *line;
TOKEN *pat;
int ret_endp;
{

  char *rval, *bptr;
  char *line2;
  TOKEN *pat2;
  char c;
  short ok;

  bptr = line;

  while (*line) {

	if (pat && pat->tok == LITCHAR) {
		while (*line) {
			pat2 = pat;
			line2 = line;
			if (*line2 != pat2->lchar) {
				c = pat2->lchar;
				while (*line2 && *line2 != c) ++line2;
				line = line2;
				if (*line2 == '\0') break;
			}
			ok = 1;
			++line2;
			pat2 = pat2->next;
			while (pat2 && pat2->tok == LITCHAR) {
				if (*line2 != pat2->lchar) {
					ok = 0;
					break;
				}
				++line2;
				pat2 = pat2->next;
			}
			if (!pat2) {
				if (ret_endp)
					return(--line2);
				else
					return(line);
			} else if (ok)
				break;
			++line;
		}
		if (*line == '\0') return(0);
	} else {
		line2 = line;
		pat2 = pat;
	}
	if ((rval = amatch(line2, pat2, bptr)) == 0) {
		if (pat && pat->tok == BOL) break;
		line++;
	} else {
		if (rval > bptr && rval > line)
			rval--;	/* point to last char matched */
		rval = ret_endp ? rval : line;
		break;
	}
  }
  return(rval);
}

/*	move.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int move(num)
int num;
{
  LINE *k0, *k1, *k2, *k3;

  if (line1 <= 0 || line2 < line1 || (line1 <= num && num <= line2))
	return(ERR);
  k0 = getptr(prevln(line1));
  k1 = getptr(line1);
  k2 = getptr(line2);
  k3 = getptr(nextln(line2));

  relink(k0, k3, k0, k3);
  lastln -= line2 - line1 + 1;

  if (num > line1) num -= line2 - line1 + 1;

  curln = num + (line2 - line1 + 1);

  k0 = getptr(num);
  k3 = getptr(nextln(num));

  relink(k0, k1, k2, k3);
  relink(k2, k3, k0, k1);
  lastln += line2 - line1 + 1;

  return(1);
}

int transfer(num)
int num;
{
  int mid, lin, ntrans;

  if (line1 <= 0 || line1 > line2) return(ERR);

  mid = num < line2 ? num : line2;

  curln = num;
  ntrans = 0;

  for (lin = line1; lin <= mid; lin++) {
	ins(gettxt(lin));
	ntrans++;
  }
  lin += ntrans;
  line2 += ntrans;

  for (; lin <= line2; lin += 2) {
	ins(gettxt(lin));
	line2++;
  }
  return(1);
}

/*	omatch.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

/* Match one pattern element, pointed at by pat, with the character at
 * **linp.  Return non-zero on match.  Otherwise, return 0.  *Linp is
 * advanced to skip over the matched character; it is not advanced on
 * failure.  The amount of advance is 0 for patterns that match null
 * strings, 1 otherwise.  "boln" should point at the position that will
 * match a BOL token.
 */
int omatch(linp, pat, boln)
char **linp;
TOKEN *pat;
char *boln;
{

  register int advance;

  advance = -1;

  if (**linp) {
	switch (pat->tok) {
	    case LITCHAR:
		if (**linp == pat->lchar) advance = 1;
		break;

	    case BOL:
		if (*linp == boln) advance = 0;
		break;

	    case ANY:
		if (**linp != '\n') advance = 1;
		break;

	    case EOL:
		if (**linp == '\n') advance = 0;
		break;

	    case CCL:
		if (testbit(**linp, pat->bitmap)) advance = 1;
		break;

	    case NCCL:
		if (!testbit(**linp, pat->bitmap)) advance = 1;
		break;
	}
  }
  if (advance >= 0) *linp += advance;

  return(++advance);
}

/*	optpat.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

TOKEN *oldpat;

TOKEN *
 optpat()
{
  char delim, str[MAXPAT], *cp;

  delim = *inptr++;
  cp = str;
  while (*inptr != delim && *inptr != NL) {
	if (*inptr == ESCAPE && inptr[1] != NL) *cp++ = *inptr++;
	*cp++ = *inptr++;
  }

  *cp = EOS;
  if (*str == EOS) return(oldpat);
  if (oldpat) unmakepat(oldpat);
  oldpat = getpat(str);
  return(oldpat);
}

/*	set.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

struct tbl {
  char *t_str;
  int *t_ptr;
  int t_val;
} *t, tbl[] = {

  "number", &nflg, TRUE,
  "nonumber", &nflg, FALSE,
  "list", &lflg, TRUE,
  "nolist", &lflg, FALSE,
  "eightbit", &eightbit, TRUE,
  "noeightbit", &eightbit, FALSE,
  0
};

int set()
{
  char word[16];
  int i;

  inptr++;
  if (*inptr != 't') {
	if (*inptr != SP && *inptr != HT && *inptr != NL) return(ERR);
  } else
	inptr++;

  if (*inptr == NL) return(show());
  /* Skip white space */
  while (*inptr == SP || *inptr == HT) inptr++;

  for (i = 0; *inptr != SP && *inptr != HT && *inptr != NL;)
	word[i++] = *inptr++;
  word[i] = EOS;
  for (t = tbl; t->t_str; t++) {
	if (strcmp(word, t->t_str) == 0) {
		*t->t_ptr = t->t_val;
		return(0);
	}
  }
  return(0);
}

int show()
{
  extern int version;

  printf("ed version %d.%d\n", version / 100, version % 100);
  printf("number %s, list %s\n", nflg ? "ON" : "OFF", lflg ? "ON" : "OFF");
  return(0);
}

/*	setbuf.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

void relink(a, x, y, b)
LINE *a, *x, *y, *b;
{
  x->l_prev = a;
  y->l_next = b;
}

void clrbuf()
{
  del(1, lastln);
}

void set_buf()
{
  relink(&line0, &line0, &line0, &line0);
  curln = lastln = 0;
}

/*	subst.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */
/* #include "ed.h" */

int subst(pat, sub, gflg, pflag)
TOKEN *pat;
char *sub;
int gflg, pflag;
{
  int lin, chngd, nchngd;
  char *txtptr, *txt;
  char *lastm, *m, *new, buf[MAXLINE];

  if (line1 <= 0) return(ERR);
  nchngd = 0;			/* reset count of lines changed */
  for (lin = line1; lin <= line2; lin++) {
	txt = txtptr = gettxt(lin);
	new = buf;
	chngd = 0;
	lastm = NULL;
	while (*txtptr) {
		if (gflg || !chngd)
			m = amatch(txtptr, pat, txt);
		else
			m = NULL;
		if (m != NULL && lastm != m) {
			chngd++;
			new = catsub(txtptr, m, sub, new,
				     buf + MAXLINE);
			lastm = m;
		}
		if (m == NULL || m == txtptr) {
			*new++ = *txtptr++;
		} else {
			txtptr = m;
		}
	}
	if (chngd) {
		if (new >= buf + MAXLINE) return(ERR);
		*new++ = EOS;
		del(lin, lin);
		ins(buf);
		nchngd++;
		if (pflag) doprnt(curln, curln);
	}
  }
  if (nchngd == 0 && !gflg) {
	return(ERR);
  }
  return(nchngd);
}

/*	System.c	*/
#define SHELL	"/bin/sh"
#define SHELL2	"/usr/bin/sh"

int System(c)
char *c;
{
  int pid, status;

  switch (pid = fork()) {
      case -1:
	return -1;
      case 0:
	execl(SHELL, "sh", "-c", c, (char *) 0);
	execl(SHELL2, "sh", "-c", c, (char *) 0);
	exit(-1);
      default:	while (wait(&status) != pid);
}
  return status;
}

/*	unmkpat.c	*/
/* #include <stdio.h> */
/* #include "tools.h" */

/* Free up the memory usde for token string */
void unmakepat(head)
TOKEN *head;
{

  register TOKEN *old_head;

  while (head) {
	switch (head->tok) {
	    case CCL:
	    case NCCL:
		free(head->bitmap);
		/* Fall through to default */

	    default:
		old_head = head;
		head = head->next;
		free((char *) old_head);
		break;
	}
  }
}
