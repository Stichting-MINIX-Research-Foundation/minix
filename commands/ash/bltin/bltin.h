/*
 * This file is included by programs which are optionally built into the
 * shell.  If SHELL is defined, we try to map the standard UNIX library
 * routines to ash routines using defines.
 *
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */

#include "../shell.h"
#include "../mystring.h"
#ifdef SHELL
#include "../output.h"
#define stdout out1
#define stderr out2
#define printf out1fmt
#define putc(c, file)	outc(c, file)
#define putchar(c)	out1c(c)
#define fprintf outfmt
#define fputs outstr
#define fflush flushout
#define INITARGS(argv)
#else
#undef NULL
#include <stdio.h>
#undef main
#define INITARGS(argv)	if ((commandname = argv[0]) == NULL) {fputs("Argc is zero\n", stderr); exit(2);} else
#endif

#ifdef __STDC__
pointer stalloc(int);
void error(char *, ...);
#else
pointer stalloc();
void error();
#endif


extern char *commandname;
