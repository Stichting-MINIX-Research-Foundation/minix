/*
 * Echo command.
 *
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */

#define main echocmd

#include "bltin.h"

#undef eflag


main(argc, argv)  char **argv; {
      register char **ap;
      register char *p;
      register char c;
      int count;
      int nflag = 0;
#ifndef eflag
      int eflag = 0;
#endif

      ap = argv;
      if (argc)
	    ap++;
      if ((p = *ap) != NULL) {
	    if (equal(p, "--")) {
		  ap++;
	    }
	    if (equal(p, "-n")) {
		  nflag++;
		  ap++;
	    } else if (equal(p, "-e")) {
#ifndef eflag
		  eflag++;
#endif
		  ap++;
	    }
      }
      while ((p = *ap++) != NULL) {
	    while ((c = *p++) != '\0') {
		  if (c == '\\' && eflag) {
			switch (*p++) {
			case 'b':  c = '\b';  break;
			case 'c':  return 0;		/* exit */
			case 'f':  c = '\f';  break;
			case 'n':  c = '\n';  break;
			case 'r':  c = '\r';  break;
			case 't':  c = '\t';  break;
			case 'v':  c = '\v';  break;
			case '\\':  break;		/* c = '\\' */
			case '0':
			      c = 0;
			      count = 3;
			      while (--count >= 0 && (unsigned)(*p - '0') < 8)
				    c = (c << 3) + (*p++ - '0');
			      break;
			default:
			      p--;
			      break;
			}
		  }
		  putchar(c);
	    }
	    if (*ap)
		  putchar(' ');
      }
      if (! nflag)
	    putchar('\n');
      return 0;
}
