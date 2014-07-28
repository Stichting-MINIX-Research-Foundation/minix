/*
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */

#include <stdio.h>


main(argc, argv)  char **argv; {
      int mask;

      if (argc > 1) {
	    fprintf(stderr, "umask: only builtin version of umask can set value\n");
	    exit(2);
      }
      printf("%.4o\n", umask(0));
      return 0;
}
