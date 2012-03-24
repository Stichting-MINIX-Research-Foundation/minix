/* fold - folds long lines		Author: Terrence W. Holm */

/*  Usage:  fold  [ -width ]  [ file ... ]  */

#include <stdlib.h>
#include <stdio.h>

#define  TAB		8
#define  DEFAULT_WIDTH  80

int column = 0;			/* Current column, retained between files  */

int main(int argc, char **argv);
void Fold(FILE *f, int width);

int main(argc, argv)
int argc;
char *argv[];
{
  int width = DEFAULT_WIDTH;
  int i;
  FILE *f;

  if (argc > 1 && argv[1][0] == '-') {
	if ((width = atoi(&argv[1][1])) <= 0) {
		fprintf(stderr, "Bad number for fold\n");
		exit(1);
	}
	--argc;
	++argv;
  }
  if (argc == 1)
	Fold(stdin, width);
  else {
	for (i = 1; i < argc; ++i) {
		if ((f = fopen(argv[i], "r")) == NULL) {
			perror(argv[i]);
			exit(1);
		}
		Fold(f, width);
		fclose(f);
	}
  }
  return(0);
}


void Fold(f, width)
FILE *f;
int width;
{
  int c;

  while ((c = getc(f)) != EOF) {
	if (c == '\t')
		column = (column / TAB + 1) * TAB;
	else if (c == '\b')
		column = column > 0 ? column - 1 : 0;
	else if (c == '\n' || c == '\r')
		column = 0;
	else
		++column;

	if (column > width) {
		putchar('\n');
		column = c == '\t' ? TAB : 1;
	}
	putchar(c);
  }
}
