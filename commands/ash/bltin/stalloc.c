/*
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */

#include "../shell.h"


void error();
pointer malloc();


pointer
stalloc(nbytes) {
      register pointer p;

      if ((p = malloc(nbytes)) == NULL)
	    error("Out of space");
      return p;
}
