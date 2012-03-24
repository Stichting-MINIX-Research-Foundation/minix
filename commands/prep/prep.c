/* prep - prepare file for statistics 	Author: Andy Tanenbaum */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#define TROFF_CHAR	'.'	/* troff commands begin with this char */
#define EOL		'\n'	/* end of line char */
#define APOSTROPHE	047	/* single quote */
#define BACKSLASH       '\\'	/* troff code */

int lfread;			/* set when last char read was lf */
int lfwritten = 1;		/* set when last char written was lf */

int main(int argc, char **argv);
void skipline(void);
int backslash(void);
void usage(void);

int main(argc, argv)
int argc;
char *argv[];
{

  int c;

  if (argc > 2) usage();
  if (argc == 2) {
	if (freopen(argv[1], "r", stdin) == NULL) {
		printf("prep: cannot open %s\n", argv[1]);
		exit(1);
	}
  }
  while ((c = getchar()) != EOF) {
	/* Lines beginning with "." are troff commands -- skip them. */
	if (lfread && c == TROFF_CHAR) {
		skipline();
		continue;
	}
	while (c == BACKSLASH) c = backslash();	/* eat troff stuff */

	if (isupper(c)) {
		putchar(tolower(c));
		lfwritten = 0;
		lfread = 0;
		continue;
	}
	if (islower(c)) {
		putchar(c);
		lfwritten = 0;
		lfread = 0;
		continue;
	}
	if (c == APOSTROPHE) {
		putchar(c);
		lfwritten = 0;
		lfread = 0;
		continue;
	}
	lfread = (c == EOL ? 1 : 0);
	if (lfwritten) continue;
	putchar(EOL);
	lfwritten = 1;
  }
  return(0);
}


void skipline()
{
  char c;

  while ((c = getchar()) != EOL);
}


int backslash()
{
/* A backslash has been seen.  Eat troff stuff. */

  int c, c1, c2;

  c = getchar();
  switch (c) {
      case 'f':
	c = getchar();
	c = getchar();
	return(c);

      case 's':			/* \s7  or \s14 */
	c = getchar();
	c = getchar();
	if (isdigit(c)) c = getchar();
	return(c);

      case 'n':			/* \na or \n(xx  */
	c = getchar();
	if (c == '(') {
		c = getchar();
		c = getchar();
	}
	c = getchar();
	return(c);

      case '*':			/* / * (XX */
	c = getchar();
	if (c == '(') {
		c = getchar();
		c = getchar();
		c = getchar();
		return(c);
	}

      case '(':			/* troff 4-character escape sequence */
	c1 = getchar();
	c2 = getchar();
	if (c1 == 'e' && c2 == 'm') return(' ');
	if (c1 == 'e' && c2 == 'n') return(' ');
	c = getchar();
	return(c);

      case '%':			/* soft hyphen: \% */
	c = getchar();
	return(c);

      default:
	return(c);

  }
}

void usage()
{
  printf("Usage: prep [file]\n");
  exit(1);
}
