/*
 * The line command.  Reads one line from the standard input and writes it
 * to the standard output.
 *
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */

#define main linecmd

#include "bltin.h"


main(argc, argv)  char **argv; {
      char c;

      for (;;) {
	    if (read(0, &c, 1) != 1) {
		  putchar('\n');
		  return 1;
	    }
	    putchar(c);
	    if (c == '\n')
		  return 0;
      }
}
