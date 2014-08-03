/*	$NetBSD: bltin.h,v 1.13 2008/10/12 01:40:37 dholland Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bltin.h	8.1 (Berkeley) 5/31/93
 */

/*
 * This file is included by programs which are optionally built into the
 * shell.
 *
 * We always define SHELL_BUILTIN, to allow other included headers to
 * hide some of their symbols if appropriate.
 *
 * If SHELL is defined, we try to map the standard UNIX library routines
 * to ash routines using defines.
 */

#define SHELL_BUILTIN
#include "../shell.h"
#include "../mystring.h"
#ifdef SHELL
#include "../output.h"
#include "../error.h"
#undef stdout
#undef stderr
#undef putc
#undef putchar
#undef fileno
#undef ferror
#define FILE struct output
#define stdout out1
#define stderr out2
#define _RETURN_INT(x)	((x), 0) /* map from void foo() to int bar() */
#define fprintf(...)	_RETURN_INT(outfmt(__VA_ARGS__))
#define printf(...)	_RETURN_INT(out1fmt(__VA_ARGS__))
#define putc(c, file)	_RETURN_INT(outc(c, file))
#define putchar(c)	_RETURN_INT(out1c(c))
#define fputs(...)	_RETURN_INT(outstr(__VA_ARGS__))
#define fflush(f)	_RETURN_INT(flushout(f))
#define fileno(f) ((f)->fd)
#define ferror(f) ((f)->flags & OUTPUT_ERR)
#define INITARGS(argv)
#define	err sh_err
#define	verr sh_verr
#define	errx sh_errx
#define	verrx sh_verrx
#define	warn sh_warn
#define	vwarn sh_vwarn
#define	warnx sh_warnx
#define	vwarnx sh_vwarnx
#define exit sh_exit
#define setprogname(s)
#define getprogname() commandname
#define setlocate(l,s) 0

#define getenv(p) bltinlookup((p),0)

#else /* ! SHELL */
#undef NULL
#include <stdio.h>
#undef main
#define INITARGS(argv)	if ((commandname = argv[0]) == NULL) {fputs("Argc is zero\n", stderr); exit(2);} else
#endif /* ! SHELL */

pointer stalloc(int);

int echocmd(int, char **);


extern const char *commandname;
