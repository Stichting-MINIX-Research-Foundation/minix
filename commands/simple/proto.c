/* proto - Generate ANSI C prototypes.	Author:	Eric R. Smith */

/* Program to extract function declarations from C source code
 * Written by Eric R. Smith and placed in the public domain
 * Thanks are due to Jwahar R. Bammi for fixing several bugs
 * And providing the Unix makefiles.
 */
#define EXIT_SUCCESS  0
#define EXIT_FAILURE  1

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#define ISCSYM(x) ((x) > 0 && (isalnum(x) || (x) == '_' ))
#define ABORTED ( (Word *) -1 )
#define MAXPARAM 20		/* max. number of parameters to a function */

typedef struct word {
  struct word *next;
  char string[1];
} Word;

int inquote = 0;		/* in a quote? */
int newline_seen = 1;		/* are we at the start of a line */
long linenum = 1L;		/* line number in current file */
long endline = 0L;		/* the last line before the { of a f'n */
long symline = 0L;		/* Line that symbol was on, set by getsym() */
int dostatic = 0;		/* do static functions? */
int donum = 0;			/* print line numbers? */
int dohead = 1;			/* do file headers? */
int docond = 1;			/* conditionalize for non-ANSI compilers? */
int dodiff = 0;			/* Output a diff file to prototype original */
int doold = 0;			/* do old style: P() */
int glastc = ' ';		/* last char. seen by getsym() */
Word *endlist;			/* Parentheses after the parameters */
char *progname;			/* name of program (for error messages) */


_PROTOTYPE(Word * word_alloc, (char *s));
_PROTOTYPE(void word_free, (Word * w));
_PROTOTYPE(int List_len, (Word * w));
_PROTOTYPE(Word * word_append, (Word * w1, Word * w2));
_PROTOTYPE(int foundin, (Word * w1, Word * w2));
_PROTOTYPE(void addword, (Word * w, char *s));
_PROTOTYPE(void printlist, (Word * p));
_PROTOTYPE(Word * typelist, (Word * p));
_PROTOTYPE(void typefixhack, (Word * w));
_PROTOTYPE(int ngetc, (FILE * f));
_PROTOTYPE(int fnextch, (FILE * f));
_PROTOTYPE(int nextch, (FILE * f));
_PROTOTYPE(int getsym, (char *buf, FILE * f));
_PROTOTYPE(int skipit, (char *buf, FILE * f));
_PROTOTYPE(Word * getparamlist, (FILE * f));
_PROTOTYPE(void emit, (Word * wlist, Word * plist, long startline));
_PROTOTYPE(void getdecl, (FILE * f));
_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void Usage, (void));

/* Routines for manipulating lists of words. */

Word *word_alloc(s)
char *s;
{
  Word *w;

  w = (Word *) malloc(sizeof(Word) + strlen(s) + 1);
  if (w == NULL) {
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(1);
  }
  (void) strcpy(w->string, s);
  w->next = NULL;
  return w;
}

void word_free(w)
Word *w;
{
  Word *oldw;
  while (w) {
	oldw = w;
	w = w->next;
	free((char *) oldw);
  }
}

/* Return the length of a list; empty words are not counted */
int List_len(w)
Word *w;
{
  int count = 0;

  while (w) {
	if (*w->string) count++;
	w = w->next;
  }
  return count;
}

/* Append two lists, and return the result */
Word *word_append(w1, w2)
Word *w1, *w2;
{
  Word *r, *w;

  r = w = word_alloc("");

  while (w1) {
	w->next = word_alloc(w1->string);
	w = w->next;
	w1 = w1->next;
  }
  while (w2) {
	w->next = word_alloc(w2->string);
	w = w->next;
	w2 = w2->next;
  }

  return r;
}

/* See if the last entry in w2 is in w1 */
int foundin(w1, w2)
Word *w1, *w2;
{
  while (w2->next) w2 = w2->next;

  while (w1) {
	if (!strcmp(w1->string, w2->string)) return 1;
	w1 = w1->next;
  }
  return 0;
}

/* Add the string s to the given list of words */
void addword(w, s)
Word *w;
char *s;
{
  while (w->next) w = w->next;
  w->next = word_alloc(s);
}

/* Printlist: print out a list */
void printlist(p)
Word *p;
{
  Word *w;
  int i = 0;

  for (w = p; w; w = w->next) {
	printf("%s", w->string);
	if (ISCSYM(w->string[0]) && i > 0
			&& w->next && w->next->string[0] != ',') printf(" ");
	i++;
  }
}

/* Given a list representing a type and a variable name, extract just
 * the base type, e.g. "struct word *x" would yield "struct word".
 * Similarly, "unsigned char x[]" would yield "unsigned char".
 */
Word *typelist(p)
Word *p;
{
  Word *w, *r, *last;

  last = r = w = word_alloc("");
  while (p && p->next) {
	if (p->string[0] == '[') {
		word_free(w);
		last->next = NULL;
		break;
	}
	if (p->string[0] && !ISCSYM(p->string[0])) break;
	w->next = word_alloc(p->string);
	last = w;
	w = w->next;
	p = p->next;
  }
  return r;
}

/* Typefixhack: promote formal parameters of type "char", "unsigned char",
 * "short", or "unsigned short" to "int".
 */
void typefixhack(w)
Word *w;
{
  Word *oldw = 0;

  while (w) {
	if (*w->string) {
		if ((!strcmp(w->string, "char") ||
		     !strcmp(w->string, "short"))
		    && (List_len(w->next) < 2)) {
			if (oldw && !strcmp(oldw->string, "unsigned")) {
				oldw->next = w->next;
				free((char *) w);
				w = oldw;
			}
			(void) strcpy(w->string, "int");
		}
	}
	w = w->next;
  }
}

/* Read a character: if it's a newline, increment the line count */
int ngetc(f)
FILE *f;
{
  int c;

  c = getc(f);
  if (c == '\n') linenum++;

  return c;
}

/* Read the next character from the file. If the character is '\' then
 * read and skip the next character. Any comment sequence is converted
 * to a blank.
 */
int fnextch(f)
FILE *f;
{
  int c, lastc, incomment;

  c = ngetc(f);
  while (c == '\\') {
	c = ngetc(f);		/* skip a character */
	c = ngetc(f);
  }
  if (c == '/' && !inquote) {
	c = ngetc(f);
	if (c == '*') {
		incomment = 1;
		c = ' ';
		while (incomment) {
			lastc = c;
			c = ngetc(f);
			if (lastc == '*' && c == '/')
				incomment = 0;
			else if (c < 0)
				return c;
		}
		return fnextch(f);
	} else {
		if (c == '\n') linenum--;
		(void) ungetc(c, f);
		return '/';
	}
  }
  return c;
}


/* Get the next "interesting" character. Comments are skipped, and strings
 * are converted to "0". Also, if a line starts with "#" it is skipped.
 */
int nextch(f)
FILE *f;
{
  int c;

  c = fnextch(f);
  if (newline_seen && c == '#') {
	do {
		c = fnextch(f);
	} while (c >= 0 && c != '\n');
	if (c < 0) return c;
  }
  newline_seen = (c == '\n');

  if (c == '\'' || c == '\"') {
	inquote = c;
	while ((c = fnextch(f)) >= 0) {
		if (c == inquote) {
			inquote = 0;
			return '0';
		}
	}
  }
  return c;
}

/* Get the next symbol from the file, skipping blanks.
 * Return 0 if OK, -1 for EOF.
 * Also collapses everything between { and }
 */
int getsym(buf, f)
char *buf;
FILE *f;
{
  register int c;
  int inbrack = 0;

  c = glastc;
  while ((c > 0) && isspace(c)) c = nextch(f);
  if (c < 0) return -1;
  if (c == '{') {
	inbrack = 1;
	endline = linenum;
	while (inbrack) {
		c = nextch(f);
		if (c < 0) {
			glastc = c;
			return c;
		}
		if (c == '{')
			inbrack++;
		else if (c == '}')
			inbrack--;
	}
	(void) strcpy(buf, "{}");
	glastc = nextch(f);
	return 0;
  }
  if (!ISCSYM(c)) {
	*buf++ = c;
	glastc = nextch(f);
	if (c == '(' && glastc == '*') {	/* Look for a 'f'n pointer */
		*buf++ = glastc;
		glastc = nextch(f);
	}
	*buf = 0;
	return 0;
  }
  symline = linenum;
  while (ISCSYM(c)) {
	*buf++ = c;
	c = nextch(f);
  }
  *buf = 0;
  glastc = c;
  return 0;
}


/* Skipit: skip until a ";" or the end of a function declaration is seen */
int skipit(buf, f)
char *buf;
FILE *f;
{
  int i;

  do {
	i = getsym(buf, f);
	if (i < 0) return i;
  } while (*buf != ';' && *buf != '{');

  return 0;
}

/* Get a parameter list; when this is called the next symbol in line
 * should be the first thing in the list.
 */
Word *getparamlist(f)
FILE *f;
{
  static Word *pname[MAXPARAM];	/* parameter names */
  Word *tlist,			/* type name */
  *plist;			/* temporary */
  int np = 0;			/* number of parameters */
  int typed[MAXPARAM];		/* parameter has been given a type */
  int tlistdone;		/* finished finding the type name */
  int sawsomething;
  int i;
  int inparen = 0;
  char buf[80];

  for (i = 0; i < MAXPARAM; i++) typed[i] = 0;

  plist = word_alloc("");
  endlist = word_alloc("");

  /* First, get the stuff inside brackets (if anything) */

  sawsomething = 0;		/* gets set nonzero when we see an arg */
  for (;;) {
	if (getsym(buf, f) < 0) return(NULL);
	if (*buf == ')' && (--inparen < 0)) {
		if (sawsomething) {	/* if we've seen an arg */
			pname[np] = plist;
			plist = word_alloc("");
			np++;
		}
		break;
	}
	if (*buf == ';') {	/* something weird */
		return ABORTED;
	}
	sawsomething = 1;	/* there's something in the arg. list */
	if (*buf == ',' && inparen == 0) {
		pname[np] = plist;
		plist = word_alloc("");
		np++;
	} else {
		addword(plist, buf);
		if (*buf == '(') inparen++;
	}
  }

  /* Next, get the declarations after the function header */
  inparen = 0;
  tlist = word_alloc("");
  plist = word_alloc("");
  tlistdone = 0;
  sawsomething = 0;
  for (;;) {
	if (getsym(buf, f) < 0) return(NULL);

	/* Handle parentheses, which should indicate func pointer rtn values */
	if (*buf == '(') {
		addword(endlist, buf);
		addword(endlist, " void ");
		inparen++;
	} else if (*buf == ')') {
		if (symline == linenum) {
			addword(endlist, buf);
			addword(endlist, buf);
		}
		inparen--;
	} else if (*buf == ',' && !inparen) {
		/* Handle a list like "int x,y,z" */
		if (!sawsomething) return(NULL);
		for (i = 0; i < np; i++) {
			if (!typed[i] && foundin(plist, pname[i])) {
				typed[i] = 1;
				word_free(pname[i]);
				pname[i] = word_append(tlist, plist);
				/* Promote types */
				typefixhack(pname[i]);
				break;
			}
		}
		if (!tlistdone) {
			tlist = typelist(plist);
			tlistdone = 1;
		}
		word_free(plist);
		plist = word_alloc("");
	} else if (*buf == ';') {
		/* Handle the end of a list */
		if (!sawsomething) return ABORTED;
		for (i = 0; i < np; i++) {
			if (!typed[i] && foundin(plist, pname[i])) {
				typed[i] = 1;
				word_free(pname[i]);
				pname[i] = word_append(tlist, plist);
				typefixhack(pname[i]);
				break;
			}
		}
		tlistdone = 0;
		word_free(tlist);
		word_free(plist);
		tlist = word_alloc("");
		plist = word_alloc("");
	} else if (!strcmp(buf, "{}"))
		break;	/* Handle the  beginning of the function */
		/* Otherwise, throw word into list (except for "register") */
	else if (strcmp(buf, "register")) {
		sawsomething = 1;
		addword(plist, buf);
		if (*buf == '(') inparen++;
		if (*buf == ')') inparen--;
	}
  }

  /* Now take the info we have and build a prototype list */

  /* Empty parameter list means "void" */
  if (np == 0) return word_alloc("void");

  plist = tlist = word_alloc("");
  for (i = 0; i < np; i++) {

  /* If no type provided, make it an "int" */
	if (!(pname[i]->next) ||
	    (!(pname[i]->next->next)&&strcmp(pname[i]->next->string,"void"))) {
		addword(tlist, "int");
	}
	while (tlist->next) tlist = tlist->next;
	tlist->next = pname[i];
	if (i < np - 1) addword(tlist, ", ");
  }
  return plist;
}

/* Emit a function declaration. The attributes and name of the function
 * are in wlist; the parameters are in plist.
 */
void emit(wlist, plist, startline)
Word *wlist, *plist;
long startline;
{
  Word *w;
  int count = 0;

  if (doold == 0) printf("_PROTOTYPE( ");
  if (dodiff) {
	printf("%lda%ld,%ld\n", startline - 1, startline, startline +2);
	printf("> #ifdef __STDC__\n> ");
  }
  if (donum) printf("/*%8ld */ ", startline);
  for (w = wlist; w; w = w->next) {
	if (w->string[0]) count++;
  }
  if (count < 2) printf("int ");
  printlist(wlist);
  if (docond) {
	if (doold)
		printf(" P((");
	else
		printf(", (");
  } else {
	printf("(");
  }

  printlist(plist);
  printlist(endlist);

  if (docond) {
	if (doold)
		printf("))");
	else
		printf(") )");
  } else {
	printf(")");
  }

  if (!dodiff)
	printf(";\n");
  else
	printf("\n");

  if (dodiff) {
	printf("> #else\n");
	printf("%lda%ld\n", endline - 1, endline);
	printf("> #endif\n");
  }
}

/* Get all the function declarations */
void getdecl(f)
FILE *f;
{
  Word *plist, *wlist = NULL;
  char buf[80];
  int sawsomething;
  long startline = 0L;		/* line where declaration started */
  int oktoprint;

again:				/* SHAME SHAME */
  word_free(wlist);
  wlist = word_alloc("");
  sawsomething = 0;
  oktoprint = 1;

  for (;;) {
	if (getsym(buf, f) < 0) return;

	/* Guess when a declaration is not an external function definition */
	if (!strcmp(buf, ",") || !strcmp(buf, "{}") ||
	    !strcmp(buf, "=") || !strcmp(buf, "typedef") ||
	    !strcmp(buf, "extern")) {
		(void) skipit(buf, f);
		goto again;
	}
	if (!dostatic && !strcmp(buf, "static")) oktoprint = 0;

	/* For the benefit of compilers that allow "inline" declarations */
	if (!strcmp(buf, "inline") && !sawsomething) continue;
	if (!strcmp(buf, ";")) goto again;

	/* A left parenthesis *might* indicate a function definition */
	if (!strcmp(buf, "(")) {
		if (!sawsomething || !(plist = getparamlist(f))) {
			(void) skipit(buf, f);
			goto again;
		}
		if (plist == ABORTED) goto again;

		/* It seems to have been what we wanted */
		if (oktoprint) emit(wlist, plist, startline);
		word_free(plist);
		goto again;
	}
	addword(wlist, buf);
	if (!sawsomething) startline = symline;
	sawsomething = 1;
  }
}

int main(argc, argv)
int argc;
char **argv;
{
  FILE *f, *g;
  char *t;
  char newname[40];

  progname = argv[0];
  argv++;
  argc--;
  g = stdout;

  while (*argv && **argv == '-') {
	t = *argv++;
	--argc;
	t++;
	while (*t) {
		if (*t == 's')
			dostatic = 1;
		else if (*t == 'n')
			donum = 1;
		else if (*t == 'p')
			docond = 0;
		else if (*t == 'P')
			doold =1;
		else if (*t == 'd') {
			dodiff = 1;
			doold = 1;
			docond = 0;
			donum = 0;
			dostatic = 1;
		} else
			Usage();
		t++;
	}
  }

  if (docond && doold) {
	printf("#ifdef __STDC__\n");
	printf("# define P(args)\targs\n");
	printf("#else\n");
	printf("# define P(args)\t()\n");
	printf("#endif\n\n");
  }
  if (argc == 0)
	getdecl(stdin);
  else
	while (argc > 0 && *argv) {
		if (!(f = fopen(*argv, "r"))) {
			perror(*argv);
			exit(EXIT_FAILURE);
		}
#if 0
		if (dodiff) {
			(void) sprintf(newname, "%sdif", *argv);
			(void) fclose(g);
			if (!(g = fopen(newname, "w"))) {
				perror(newname);
				exit(EXIT_FAILURE);
			}
		}
#endif
		if (doold && dohead && !dodiff) printf("\n/* %s */\n", *argv);
		linenum = 1;
		newline_seen = 1;
		glastc = ' ';
		getdecl(f);
		argc--;
		argv++;
		(void) fclose(f);
	}
  if (docond && doold) printf("\n#undef P\n");	/* clean up namespace */
  (void) fclose(g);
  return(EXIT_SUCCESS);
}


void Usage()
{
  fputs("Usage: ", stderr);
  fputs(progname, stderr);
  fputs(" [-d][-n][-p][-s] [files ...]\n", stderr);
  fputs("   -P: use P() style instead of _PROTOTYPE\n", stderr);
  fputs("   -d: produce a diff file to prototype original source\n", stderr);
  fputs("   -n: put line numbers of declarations as comments\n", stderr);
  fputs("   -p: don't make header files readable by K&R compilers\n", stderr);
  fputs("   -s: include declarations for static functions\n", stderr);
  exit(EXIT_FAILURE);
}
