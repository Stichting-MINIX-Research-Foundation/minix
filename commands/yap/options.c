/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _OPTIONS_

# include "in_all.h"
# include "options.h"
# include "output.h"
# include "display.h"
# include <ctype.h>

STATIC int parsopt();
char *getenv();

/*
 * Read the options. Return the argv pointer following them if there were
 * no errors, otherwise return 0.
 */

char **
readoptions(argv) char ** argv; {

	register char ** av = argv+1;
	register char *p;

	if (p = getenv("YAP")) {
		(VOID) parsopt(p);
	}
	while (*av && **av == '-') {
		if (parsopt(*av)) {
			/*
			 * Error in option
			 */
			putline(*av);
			putline(": illegal option\n");
			return (char **) 0;
		}
		av++;
	}
	if (*av && **av == '+') {
		/*
		 * Command in command line
		 */
		startcomm = *av + 1;
		av++;
	}
	return av;
}

STATIC int
parsopt(s) register char *s; {
	register i;

	if (*s == '-') s++;
	if (isdigit(*s)) {
		/*
		 * pagesize option
		 */
		i = 0;
		do {
			i = i * 10 + *s++ - '0';
		} while (isdigit(*s));
		if (i < MINPAGESIZE) i = MINPAGESIZE;
		pagesize = i;
	}
	while (*s) {
		switch(*s++) {
		  case 'c' :
			cflag++;
			break;
		  case 'n' :
			nflag++;
			break;
		  case 'u' :
			uflag++;
			break;
		  case 'q' :
			qflag++;
			break;
		  default :
			return 1;
		}
	}
	return 0;
}
