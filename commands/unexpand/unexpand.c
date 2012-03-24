/*  unexpand - convert spaces to tabs	Author: Terrence W. Holm */

/*  Usage:  unexpand  [ -a ]  [ file ... ]  */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define  TAB   8


int column = 0;			/* Current column, retained between files  */
int spaces = 0;			/* Spaces since last tab stop  	    */
int leading_blank = 1;		/* Only unexpand leading blanks,	    */
/* Overruled by -a option	            */

int main(int argc, char **argv);
void Unexpand(FILE *f, int all);

int main(argc, argv)
int argc;
char *argv[];

{
  int all = 0;			/* -a flag means unexpand all spaces  */
  int i;
  FILE *f;

  if (argc > 1 && argv[1][0] == '-') {
	if (strcmp(argv[1], "-a") == 0)
		all = 1;
	else {
		fprintf(stderr, "Usage:  unexpand  [ -a ]  [ file ... ]\n");
		exit(1);
	}

	--argc;
	++argv;
  }
  if (argc == 1)
	Unexpand(stdin, all);
  else
	for (i = 1; i < argc; ++i) {
		if ((f = fopen(argv[i], "r")) == NULL) {
			perror(argv[i]);
			exit(1);
		}
		Unexpand(f, all);
		fclose(f);
	}


  /* If there are pending spaces print them.  */

  while (spaces > 0) {
	putchar(' ');
	--spaces;
  }

  return(0);
}

void Unexpand(f, all)
FILE *f;
int all;

{
  int c;

  while ((c = getc(f)) != EOF) {
	if (c == ' ' && (all || leading_blank)) {
		++column;
		++spaces;

		/* If we have white space up to a tab stop, then output	 */
		/* A tab. If only one space is required, use a ' '.	 */

		if (column % TAB == 0) {
			if (spaces == 1)
				putchar(' ');
			else
				putchar('\t');

			spaces = 0;
		}
		continue;
	}

	/* If a tab character is encountered in the input then		*/
	/* Simply echo it. Any accumulated spaces can only be 		*/
	/* Since the last tab stop, so ignore them.			*/
	if (c == '\t') {
		column = (column / TAB + 1) * TAB;
		spaces = 0;
		putchar('\t');
		continue;
	}

	/* A non-space character is to be printed. If there   */
	/* Are pending spaces, then print them. There will be */
	/* At most TAB-1 spaces to print.		      */
	while (spaces > 0) {
		putchar(' ');
		--spaces;
	}

	if (c == '\n' || c == '\r') {
		column = 0;
		leading_blank = 1;
		putchar(c);
		continue;
	}
	if (c == '\b')
		column = column > 0 ? column - 1 : 0;
	else
		++column;

	leading_blank = 0;
	putchar(c);
  }
}
