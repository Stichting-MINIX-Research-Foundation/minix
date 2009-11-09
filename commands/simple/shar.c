/* shar - make a shell archive		Author: Michiel Husijes */

#include <stdlib.h>		/* for the nonstd :-( _PROTOTYPE */
#include <stdio.h>

static _PROTOTYPE( void error, (char *progname, char *operation,
				char *filename) );
_PROTOTYPE( int main, (int argc, char **argv) );

int main(argc, argv)
int argc;
char *argv[];
{
  int argn;
  register int ch;
  register FILE *fp;
  int exitstatus;
  char *filename;

  exitstatus = 0;
  for (argn = 1; argn < argc; argn++) {
	filename = argv[argn];
	if ((fp = fopen(filename, "r")) == NULL) {
		error(argv[0], "opening ", filename);
		exitstatus = 1;
	} else {
		fputs("echo x - ", stdout);
		fputs(filename, stdout);
		fputs("\nsed '/^X/s///' > ", stdout);
		fputs(filename, stdout);
		fputs(" << '/'\n", stdout);
		while ((ch = getc(fp)) != EOF) {
			putchar('X');
			putchar(ch);
			while (ch != '\n') {
				ch = getc(fp);
				if (ch == EOF) break;
				putchar(ch);
			}
			if (ch == EOF) break;
		}
		fputs("/\n", stdout);
		if (ferror(fp)) {
			error(argv[0], "reading ", filename);
			exitstatus = 1;
		}
		if (fclose(fp) != 0) {
			error(argv[0], "closing ", filename);
			exitstatus = 1;
		}
		if (ferror(stdout)) break;	/* lost already */
	}
  }
  fflush(stdout);
  if (ferror(stdout)) {
	error(argv[0], "writing ", "stdout");
	exitstatus = 1;
  }
  return(exitstatus);
}

static void error(progname, operation, filename)
char *progname;
char *operation;
char *filename;
{
  fputs(progname, stderr);
  fputs(": error ", stderr);
  fputs(operation, stderr);
  perror(filename);
}
