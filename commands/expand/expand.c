/*  expand - expand tabs to spaces	Author: Terrence W. Holm */

/*  Usage:  expand  [ -tab1,tab2,tab3,... ]  [ file ... ]  */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_TABS 32

int column = 0;			/* Current column, retained between files  */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void Expand, (FILE *f, int tab_index, int tabs []));

int main(argc, argv)
int argc;
char *argv[];
{
  int tabs[MAX_TABS];
  int tab_index = 0;		/* Default one tab   */
  int i;
  FILE *f;

  tabs[0] = 8;			/* Default tab stop  */

  if (argc > 1 && argv[1][0] == '-') {
	char *p = argv[1];
	int last_tab_stop = 0;

	for (tab_index = 0; tab_index < MAX_TABS; ++tab_index) {
		if ((tabs[tab_index] = atoi(p + 1)) <= last_tab_stop) {
			fprintf(stderr, "Bad tab stop spec\n");
			exit(1);
		}
		last_tab_stop = tabs[tab_index];

		if ((p = strchr(p + 1, ',')) == NULL) break;
	}

	--argc;
	++argv;
  }
  if (argc == 1)
	Expand(stdin, tab_index, tabs);
  else
	for (i = 1; i < argc; ++i) {
		if ((f = fopen(argv[i], "r")) == NULL) {
			perror(argv[i]);
			exit(1);
		}
		Expand(f, tab_index, tabs);
		fclose(f);
	}

  return(0);
}


void Expand(f, tab_index, tabs)
FILE *f;
int tab_index;
int tabs[];
{
  int next;
  int c;
  int i;

  while ((c = getc(f)) != EOF) {
	if (c == '\t') {
		if (tab_index == 0)
			next = (column / tabs[0] + 1) * tabs[0];
		else {
			for (i = 0; i <= tab_index && tabs[i] <= column; ++i);

			if (i > tab_index)
				next = column + 1;
			else
				next = tabs[i];
		}

		do {
			++column;
			putchar(' ');
		} while (column < next);

		continue;
	}
	if (c == '\b')
		column = column > 0 ? column - 1 : 0;
	else if (c == '\n' || c == '\r')
		column = 0;
	else
		++column;

	putchar(c);
  }
}
