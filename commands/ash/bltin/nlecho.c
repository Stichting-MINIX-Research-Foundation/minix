/*
 * Echo the command argument to the standard output, one line at a time.
 * This command is useful for debugging th shell and whenever you what
 * to output strings literally.
 *
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */


#define main nlechocmd

#include "bltin.h"


main(argc, argv)  char **argv; {
      register char **ap;

      for (ap = argv + 1 ; *ap ; ap++) {
	    fputs(*ap, stdout);
	    putchar('\n');
      }
      return 0;
}
