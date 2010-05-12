/* wc - count lines, words and characters	Author: David Messer */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

/*
 *
 *	Usage:  wc [-lwc] [names]
 *
 *		Flags:
 *			l - count lines.
 *			w - count words.
 *			c - count characters.
 *
 *		Flags l, w, and c are default.
 *		Words are delimited by any non-alphabetic character.
 *
 *  Released into the PUBLIC-DOMAIN 02/10/86
 *
 *	If you find this program to be of use to you, a donation of
 *	whatever you think it is worth will be cheerfully accepted.
 *
 *	Written by: David L. Messer
 *				P.O. Box 19130, Mpls, MN,  55119
 *      Program (heavily) modified by Andy Tanenbaum
 */


int lflag;			/* Count lines */
int wflag;			/* Count words */
int cflag;			/* Count characters */

long lcount;			/* Count of lines */
long wcount;			/* Count of words */
long ccount;			/* Count of characters */

long ltotal;			/* Total count of lines */
long wtotal;			/* Total count of words */
long ctotal;			/* Total count of characters */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void count, (FILE *f));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
  int k;
  char *cp;
  int tflag, files;

  /* Get flags. */
  files = argc - 1;
  k = 1;
  cp = argv[1];
  if (argc > 1 && *cp++ == '-') {
	files--;
	k++;			/* points to first file */
	while (*cp != 0) {
		switch (*cp) {
		    case 'l':	lflag++;	break;
		    case 'w':	wflag++;	break;
		    case 'c':	cflag++;	break;
		    default:	usage();
		}
		cp++;
	}
  }

  /* If no flags are set, treat as wc -lwc. */
  if (!lflag && !wflag && !cflag) {
	lflag = 1;
	wflag = 1;
	cflag = 1;
  }

  /* Process files. */
  tflag = files >= 2;		/* set if # files > 1 */

  /* Check to see if input comes from std input. */
  if (k >= argc) {
	count(stdin);
	if (lflag) printf(" %6ld", lcount);
	if (wflag) printf(" %6ld", wcount);
	if (cflag) printf(" %6ld", ccount);
	printf(" \n");
	fflush(stdout);
	exit(0);
  }

  /* There is an explicit list of files.  Loop on files. */
  while (k < argc) {
	FILE *f;

	if ((f = fopen(argv[k], "r")) == NULL) {
		fprintf(stderr, "wc: cannot open %s\n", argv[k]);
	} else {
		count(f);
		if (lflag) printf(" %6ld", lcount);
		if (wflag) printf(" %6ld", wcount);
		if (cflag) printf(" %6ld", ccount);
		printf(" %s\n", argv[k]);
		fclose(f);
	}
	k++;
  }

  if (tflag) {
	if (lflag) printf(" %6ld", ltotal);
	if (wflag) printf(" %6ld", wtotal);
	if (cflag) printf(" %6ld", ctotal);
	printf(" total\n");
  }
  fflush(stdout);
  return(0);
}

void count(f)
FILE *f;
{
  register int c;
  register int word = 0;

  lcount = 0;
  wcount = 0;
  ccount = 0L;

  while ((c = getc(f)) != EOF) {
	ccount++;

	if (isspace(c)) {
		if (word) wcount++;
		word = 0;
	} else {
		word = 1;
	}

	if (c == '\n' || c == '\f') lcount++;
  }
  ltotal += lcount;
  wtotal += wcount;
  ctotal += ccount;
}

void usage()
{
  fprintf(stderr, "Usage: wc [-lwc] [name ...]\n");
  exit(1);
}
