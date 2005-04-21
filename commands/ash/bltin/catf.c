/*
 * Copy the files given as arguments to the standard output.  The file
 * name "-" refers to the standard input.
 *
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */

#define main catfcmd

#include "bltin.h"
#include "../error.h"
#include <sys/param.h>
#include <fcntl.h>


#ifdef SBUFSIZE
#define BUFSIZE() SBUFSIZE
#else
#ifdef MAXBSIZE
#define BUFSIZE() MAXBSIZE
#else
#define BUFSIZE() BSIZE
#endif
#endif


main(argc, argv)  char **argv; {
      char *filename;
      char *buf = stalloc(BUFSIZE());
      int fd;
      int i;
#ifdef SHELL
      volatile int input;
      struct jmploc jmploc;
      struct jmploc *volatile savehandler;
#endif

      INITARGS(argv);
#ifdef SHELL
      input = -1;
      if (setjmp(jmploc.loc)) {
	    close(input);
	    handler = savehandler;
	    longjmp(handler, 1);
      }
      savehandler = handler;
      handler = &jmploc;
#endif
      while ((filename = *++argv) != NULL) {
	    if (filename[0] == '-' && filename[1] == '\0') {
		  fd = 0;
	    } else {
#ifdef SHELL
		  INTOFF;
		  if ((fd = open(filename, O_RDONLY)) < 0)
			error("Can't open %s", filename);
		  input = fd;
		  INTON;
#else
		  if ((fd = open(filename, O_RDONLY)) < 0) {
			fprintf(stderr, "catf: Can't open %s\n", filename);
			exit(2);
		  }
#endif
	    }
	    while ((i = read(fd, buf, BUFSIZE())) > 0) {
#ifdef SHELL
		  if (out1 == &memout) {
			register char *p;
			for (p = buf ; --i >= 0 ; p++) {
			      outc(*p, &memout);
			}
		  } else {
			write(1, buf, i);
		  }
#else
		  write(1, buf, i);
#endif
	    }
	    if (fd != 0)
		  close(fd);
      }
#ifdef SHELL
      handler = savehandler;
#endif
}
