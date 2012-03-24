/* head - print the first few lines of a file	Author: Andy Tanenbaum */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT 10

int main(int argc, char **argv);
void do_file(int n, FILE *f);
void usage(void);

int main(argc, argv)
int argc;
char *argv[];
{
  FILE *f;
  int legacy, n, k, nfiles;
  char *ptr;

  /* Check for flags.  One can only specify how many lines to print. */
  k = 1;
  n = DEFAULT;
  legacy = 0;
  for (k = 1; k < argc && argv[k][0] == '-'; k++) {
	ptr = &argv[k][1];
	if (ptr[0] == 'n' && ptr[1] == 0) {
		k++;
		if (k >= argc) usage();
		ptr = argv[k];
	}
	else if (ptr[0] == '-' && ptr[1] == 0) {
		k++;
		break;	
	}
	else if (++legacy > 1) usage();
	n = atoi(ptr);
	if (n <= 0) usage();
  }
  nfiles = argc - k;

  if (nfiles == 0) {
	/* Print standard input only. */
	do_file(n, stdin);
	exit(0);
  }

  /* One or more files have been listed explicitly. */
  while (k < argc) {
	if (nfiles > 1) printf("==> %s <==\n", argv[k]);
	if ((f = fopen(argv[k], "r")) == NULL)
		fprintf(stderr, "%s: cannot open %s: %s\n",
			argv[0], argv[k], strerror(errno));
	else {
		do_file(n, f);
		fclose(f);
	}
	k++;
	if (k < argc) printf("\n");
  }
  return(0);
}



void do_file(n, f)
int n;
FILE *f;
{
  int c;

  /* Print the first 'n' lines of a file. */
  while (n) switch (c = getc(f)) {
	    case EOF:
		return;
	    case '\n':
		--n;
	    default:	putc((char) c, stdout);
	}
}


void usage()
{
  fprintf(stderr, "Usage: head [-lines | -n lines] [file ...]\n");
  exit(1);
}
