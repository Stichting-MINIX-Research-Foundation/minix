/*
 * a small awk clone
 *
 * (C) 1989 Saeko Hirabauashi & Kouichi Hirabayashi
 *
 * Absolutely no warranty. Use this software with your own risk.
 *
 * Permission to use, copy, modify and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright and disclaimer notice.
 *
 * This program was written to fit into 64K+64K memory of the Minix 1.2.
 */


#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include "awk.h"

extern char **FS, **FILENAME;
extern char record[];
extern FILE *ifp;

NODE *parse();
CELL *execute();
FILE *efopen(), *fopen();
char *strsave();

int xargc;
char **xargv;
char *srcprg;
FILE *pfp;
char *cmd;
#if 0
int iflg;	/* interactive mode */
#endif

main(argc, argv) char **argv;
{
  char *s, *strpbrk(), *strchr();
  void onint();

#ifdef DOS
  _sharg(&argc, &argv);
#endif
  signal(SIGINT, onint);
  signal(SIGFPE, onint);
  cmd = argv[0];
  init();
  while (--argc > 0 && (*++argv)[0] == '-')
	for (s = argv[0]+1; *s; s++)
		if (strcmp(argv[0], "-") == 0)
			break;
		else
		switch (*s) {
#if 0
		case 'i':
			iflg++;
			pfp = stdin;
			interactive();
			/* no return */
#endif
		case 'F':
			*FS = ++s;
			break;
		case 'f':
			if (*(s+1))
				s++;
			else {
				argc--; s = *++argv;
			}
			pfp = efopen(s, "r");
			s += strlen(s) - 1;
			break;
		}
  xargc = argc; xargv = argv;
  if (pfp == NULL && xargc > 0) {
	srcprg = *xargv++; xargc--;
  }
/*
  if (pfp == NULL && xargc > 0) {
	if (strpbrk(xargv[0], " !$^()={}[];<>,/~") != NULL) {
		sprintf(record, "%s\n", xargv[0]);
		srcprg = strsave(record);
	}
	else {
		sprintf(record, "%s.awk", xargv[0]);
		if ((pfp = fopen(record, "r")) == NULL)
			error("can't open %s", record);
	}
	xargc--; xargv++;
  }
*/

  while (*xargv != NULL && strchr(*xargv, '=') != NULL) {
	setvar(*xargv++);
	xargc--;
  }

  initarg(cmd, xargc, xargv);
  if (xargc == 0) {
	ifp = stdin; *FILENAME = "-";
  }
  parse();
  closeall();
  exit(0);
}

FILE *
efopen(file, mode) char *file, *mode;
{
  FILE *fp, *fopen();

  if ((fp = fopen(file, mode)) == NULL)
	error("cannot open %s", file);
  return fp;
}

error(s, t) char *s, *t;
{
  extern double *NR;

  fprintf(stderr, "awk: ");
  fprintf(stderr, s, t);
  fprintf(stderr, "\n");
  if (NR != NULL) {
	fprintf(stderr, "record number %g\n", *NR);
  }
#ifdef DOS
  closeall();
#endif
  exit(1);
}

void
onint(i)
{
  closeall();
  exit(0x80 | i);
}
