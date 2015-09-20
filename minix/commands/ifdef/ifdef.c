/* ifdef - remove #ifdefs		Author: Warren Toomey */

/* Copyright 1989 by Warren Toomey	wkt@cs.adfa.oz.au[@uunet.uu.net]
 *
 * You may freely copy or distribute this code as long as this notice
 * remains intact.
 *
 * You may modify this code, as long as this notice remains intact, and
 * you add another notice indicating that the code has been modified.
 *
 * You may NOT sell this code or in any way profit from this code without
 * prior agreement from the author.
 */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* Definition of structures and constants used in ifdef.c  */

/* Types of symbols */
#define DEF	  1		/* Symbol is defined    */
#define UNDEF	  2		/* Symbol isn't defined */
#define IGN	  3		/* Ignore this symbol unless defined */

/* Redef mode values */
#define MUTABLE   1		/* Symbol can change defined <-> undefined */
#define IMMUTABLE 2		/* Symbol can't change as above            */

/* Processing modes */
#define NO	0		/* Don't process */
#define YES	1		/* Process */

/* Ignore (IGN), ignore but process */
struct DEFINE {
  char *symbol;			/* SLL of defined symbols. The redef  */
  char type;			/* field indicates if this symbol can */
  char redef;			/* change from defined <-> undefined. */
  struct DEFINE *next;		/* Type is DEF or UNDEF.	      */
};

/* Global variables & structures */
FILE *zin;			/* Input file for processing  */
struct DEFINE *defptr;		/* Defined symbols SLL        */
struct DEFINE *defend;		/* Ptr to last node in defptr */
struct DEFINE *deftemp;		/* Ptr to last found node     */
int line = 1;			/* Current line number        */
int table = 0;			/* Don't normally want a table */

extern int optind;
extern char *optarg;

/* Prototypes. */
int main(int argc, char **argv);
char fgetarg(FILE *stream, char *cbuf);
int find(char *symd);
void defit(char *sym, int redef, int typed);
void stop(void);
void gotoeoln(void);
void prteoln(void);
void printtable(void);
char getendif(void);
void gettable(void);
void parse(void);
void usage(void);

#ifdef __STDC__
char fgetarg ( FILE *stream , char *cbuf )
#else
char fgetarg(stream, cbuf)	/* Get next arg from file into cbuf, */
FILE *stream;			/* returning the character that      */
char *cbuf;			/* terminated it. Cbuf returns 0     */
#endif
{				/* if no arg. EOF is returned if no  */
  int ch;			/* args left in file.                */
  int i;

  i = 0;
  cbuf[i] = 0;

  while (((ch = fgetc(stream)) == ' ') || (ch == '\t') || (ch == '\n'))
	if (ch == '\n') return(ch);	/* Bypass leading */
					/* Whitespace     */
  if (feof(stream)) return(EOF);

  cbuf[i++] = ch;

  while (((ch = fgetc(stream)) != ' ') && (ch != '\t') && (ch != '\n'))
	cbuf[i++] = ch;			/* Get the argument */

  cbuf[i] = 0;
  return(ch);
}


#ifdef __STDC__
int find ( char *sym )
#else
int find(sym)
char *sym;
#endif
{				/* Return DEF if defined else UNDEF */

  deftemp = defptr;
  while (deftemp) {			/* Search for the symbol */
	if (!strcmp(deftemp->symbol, sym))
		return(deftemp->type);	/* Setting up the type */
	deftemp = deftemp->next;
  }
  return(0);
}



#define Define(x,y)	defit(x,y,DEF)
#define Undefine(x,y)	defit(x,y,UNDEF)
#define Ignore(x,y)	defit(x,y,IGN)

#ifdef __STDC__
void defit ( char *sym , int redef , int type )
#else
void defit(sym, redef, type)	/* Add symbol to the define list */
char *sym;
char redef;			/* Mode: MUTABLE etc      */
char type;			/* Type: DEF, UNDEF, IGN  */
#endif
{
  struct DEFINE *temp;
  char c;

  c = find(sym);		/* First try finding the symbol */
  if (type == c) return;	/* Return if already declared */
  if (c) {			/* We have to move if from DEF <-> UNDEF */
	if (deftemp->redef == IMMUTABLE)
		return;
	else {
		deftemp->type = type;
		deftemp->redef = redef;
	}
  } else {			/* We must create a struct & add it */
				/* Malloc room for the struct */
	if ((temp = (struct DEFINE *)malloc(sizeof(struct DEFINE))) == NULL) {
		(void)fprintf(stderr, "ifdef: could not malloc\n");
		exit(1);
	}

					/* Malloc room for symbol */
	if ((temp->symbol = (char *)malloc(strlen(sym) + 1)) == NULL) {
		(void)fprintf(stderr, "ifdef: could not malloc\n");
		exit(1);
	}
	(void)strcpy(temp->symbol, sym); /* Copy symbol into struct      */
	temp->redef = redef;		/* and set its redef mode too   */
	temp->type = type;		/* as well as making it defined */


					/* Now add to the SLL */
	if (defptr == NULL)		/* If first node set  */
		defptr = temp;		/* the pointers to it */
	else
		defend->next = temp;	/* else add it to the */
	defend = temp;			/* end of the list.   */
  }
}



#ifdef __STDC__
void stop ( void )
#else
void stop()
#endif
{				/* Stop: Tidy up at EOF */
  if (table) printtable();
  (void)fclose(zin);
  exit(0);
}

#define Goto	{ line++; if (ch!='\n') gotoeoln(); }
#define Print	{ line++; if (ch!='\n') prteoln();  }

#ifdef __STDC__
void gotoeoln ( void )
#else
void gotoeoln()			/* Go to the end of the line */
#endif
{
  int ch;
  while ((ch = fgetc(zin)) != '\n')
	if (ch == EOF) stop();
}


#ifdef __STDC__
void prteoln ( void )
#else
void prteoln()			/* Print to the end of the line */
#endif
{
  int ch;
  while ((ch = fgetc(zin)) != '\n')
	if (ch == EOF)
		stop();
	else
		(void)putchar(ch);
  (void)putchar('\n');
}


#ifdef __STDC__
void printtable ( void )
#else
void printtable()		/* Print the defines in the SLL */
#endif
{
  struct DEFINE *temp;

  (void)printf("Defined\n\n");

  temp = defptr;
  while (temp) {
	if (temp->type == DEF) (void)printf("%s\n", temp->symbol);
	temp = temp->next;
  }

  (void)printf("\n\nUndefined\n\n");

  temp = defptr;
  while (temp) {
	if (temp->type == UNDEF) (void)printf("%s\n", temp->symbol);
	temp = temp->next;
  }
}

#ifdef __STDC__
char getendif ( void )
#else
char getendif()
#endif
{				/* Find matching endif when ignoring */
  char word[80];		/* Buffer for symbols */
  int ch;
  int skip;			/* Number of skipped #ifdefs */

  skip = 1;

  while (1) {
			/* Scan through the file looking for starting lines */
	if ((ch = fgetc(zin)) == EOF)
		stop();		/* Get first char on the line */
	if (ch != '#') {	/* If not a # ignore line     */
		(void)putchar(ch);
		Print;
		continue;
	}
	ch = fgetarg(zin, word);	/* Get the word after the # */

	if (!strcmp(word, "ifdef") || !strcmp(word, "ifndef")) skip++;
						/* Keep track of ifdefs & */
	if (!strcmp(word, "endif")) skip--;	/* endifs		  */

	(void)printf("#%s%c", word, ch);	/* Print the line out 	  */
	Print;
	if (!skip) return('\n');	/* If matching endif, return */
  }
}


#ifdef __STDC__
void gettable ( void )
#else
void gettable()			/* Get & print a table of defines etc.  */
#endif
{

  char word[80];		/* Buffer for symbols */
  int ch;

  while (1) {
			/* Scan through the file looking for starting lines */
	if ((ch = fgetc(zin)) == EOF)
		stop();		/* Get first char on the line */
	if (ch != '#') {	/* If not a # ignore line     */
		Goto;
		continue;
	}
	ch = fgetarg(zin, word);	/* Get the word after the # */

	if (!strcmp(word, "define")) {		/* Define: Define the */
		ch = fgetarg(zin, word);	/* symbol, and goto   */
		Define(word, MUTABLE);		/* the end of line    */
		Goto;
		continue;
	}
	if (!strcmp(word, "undef")) {		/* Undef: Undefine the */
		ch = fgetarg(zin, word);	/* symbol, and goto    */
		Undefine(word, MUTABLE);	/* the end of line     */
		Goto;
		continue;
	}					/* Ifdef:            */
	if (!strcmp(word, "ifdef") || !strcmp(word, "ifndef")) {
		ch = fgetarg(zin, word);	/* Get the symbol */
		if (find(word) != DEF)
			Undefine(word, MUTABLE);	/* undefine it */
		Goto;
		continue;
	}
	Goto;				/* else ignore the line */
  }
}



#ifdef __STDC__
void parse ( void )
#else
void parse()
#endif
{				/* Parse & remove ifdefs from C source */
  char word[80];		/* Buffer for symbols */
  int ch;
  int proc;			/* Should we be processing this bit?    */
  int skip;			/* Number of skipped #ifdefs		 */

  proc = 1;
  skip = 0;

  while (1) {
			/* Scan through the file looking for starting lines */
	if ((ch = fgetc(zin)) == EOF)
		stop();		/* Get first char on the line */
	if (ch != '#') {
		if (proc) {	/* If not # and  we're processing */
			(void)putchar(ch); /* then print the line */
			Print;
			continue;
		} else {
			Goto;	/* else just skip the line  */
			continue;
		}
	}

	ch = fgetarg(zin, word);	/* Get the word after the # */

	if (!strcmp(word, "define") && proc) {	/* Define: Define the */
		ch = fgetarg(zin, word);	/* symbol, and goto   */
		Define(word, MUTABLE);		/* the end of line    */
		(void)printf("#define %s%c", word, ch);
		Print;
		continue;
	}
	if (!strcmp(word, "undef") && proc) {	/* Undef: Undefine the */
		ch = fgetarg(zin, word);	/* symbol, and goto    */
		Undefine(word, MUTABLE);	/* the end of line     */
		(void)printf("#undef %s%c", word, ch);
		Print;
		continue;
	}
	if (!strcmp(word, "if")) {	/* If: we cannot handle these */
		if (!proc)		/* at the moment, so just */
			skip++;		/* treat them as an ignored */
		else {			/* definition */
			(void)printf("#%s%c",word,ch);
			Print;
			ch = getendif();	/* Get matching endif */
			continue;
		     	}
	}
	if (!strcmp(word, "ifdef")) {	/* Ifdef:	     */
		if (!proc)		/* If not processing */
			skip++;		/* skip it           */
		else {
			ch = fgetarg(zin, word); /* Get the symbol */
			switch (find(word)) {
			    case DEF:
				break;
			    case IGN:
				(void)printf("#ifdef %s%c", word, ch);
				Print;
				ch = getendif(); /* Get matching endif */
				break;
						/* If symbol undefined */
			    default:
				Undefine(word, MUTABLE); /* undefine it */
				proc = 0;	/* & stop processing */
			}
		}
		Goto;
		continue;
	}
	if (!strcmp(word, "ifndef")) {
		/* Ifndef: */
		if (!proc)	/* If not processing */
			skip++;	/* skip the line     */
		else {
			ch = fgetarg(zin, word); /* Get the symbol */
			switch (find(word)) {	/* If defined, stop */
			    case DEF:
				proc = 0;	/* processing       */
				break;
			    case IGN:
				(void)printf("#ifdef %s%c", word, ch);
				Print;
				ch = getendif(); /* Get matching endif */
				break;
			}
		}
		Goto;
		continue;
	}
	if (!strcmp(word, "else") && !skip) {	/* Else: Flip processing */
		proc = !proc;
		Goto;
		continue;
	}
	if (!strcmp(word, "endif")) {	/* Endif: If no skipped   */
					/* ifdefs turn processing */
		if (!skip)		/* on, else decrement the */
			proc = 1;	/* number of skips        */
		else
			skip--;
		Goto;
		continue;
	}
		/* The word fails all of the above tests, so if we're */
		/* processing, print the line. */
	if (proc) {
		(void)printf("#%s%c", word, ch);
		Print;
	} else
		Goto;
  }
}


#ifdef __STDC__
void usage ( void )
#else
void usage()
#endif
{
  (void)fprintf(stderr, "Usage: ifdef [-t] [-Dsymbol] [-dsymbol] [-Usymbol] [-Isymbol] <file>\n");
  exit(0);
}


#ifdef __STDC__
int main(int argc , char *argv [])
#else
int main(argc, argv)
int argc;
char *argv[];
#endif
{
  char sym[80];			/* Temp symbol storage */
  int c;

  if (argc == 1) usage();	/* Catch the curious user	 */
  while ((c = getopt(argc, argv, "tD:d:U:I:")) != EOF) {
	switch (c) {
	    case 't':
		table = 1;	/* Get the various options */
		break;

	    case 'd':
		(void)strcpy(sym, optarg);
		Define(sym, MUTABLE);
		break;

	    case 'D':
		(void)strcpy(sym, optarg);
		Define(sym, IMMUTABLE);
		break;

	    case 'U':
		(void)strcpy(sym, optarg);
		Undefine(sym, IMMUTABLE);
		break;

	    case 'I':
		(void)strcpy(sym, optarg);
		Ignore(sym, IMMUTABLE);
		break;

	    default:	usage();
	}
  }

  zin = stdin;		/* If a C file is named */
			/* Open stdin with it */
  if (*argv[argc - 1] != '-') {
	(void)fclose(zin);
	if ((zin = fopen(argv[argc - 1], "r")) == NULL) {
		perror("ifdef");
		exit(1);
	}
  }
  if (table)
	gettable();		/* Either generate a table or    */
  else
	parse();		/* parse & replace with the file */
  return(0);
}
