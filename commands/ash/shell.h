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
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)shell.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: src/bin/sh/shell.h,v 1.17 2004/04/06 20:06:51 markm Exp $
 */

#include <sys/types.h>	/* for mode_t */

/*
 * The follow should be set to reflect the type of system you have:
 *	JOBS -> 1 if you have Berkeley job control, 0 otherwise.
 *	TILDE -> 1 if you want the shell to expand ~logname.
 *	USEGETPW -> 1 if getpwnam() must be used to look up a name.
 *	READLINE -> 1 if line editing by readline() should be enabled.
 *	define BSD if you are running 4.2 BSD or later.
 *	define SYSV if you are running under System V.
 *	define DEBUG=1 to compile in debugging (set global "debug" to turn on)
 *	define DEBUG=2 to compile in and turn on debugging.
 *
 * When debugging is on, debugging info will be written to $HOME/trace and
 * a quit signal will generate a core dump.
 */

#ifndef JOBS
#define	JOBS 1
#endif
#ifndef BSD
#define BSD 1
#endif
#ifndef DEBUG
#define DEBUG 0
#endif
#define POSIX 1

/*
 * Type of used arithmetics. SUSv3 requires us to have at least signed long.
 */
typedef long arith_t;
#define	ARITH_FORMAT_STR  "%ld"
#define	atoarith_t(arg)  strtol(arg, NULL, 0)
#define	strtoarith_t(nptr, endptr, base)  strtol(nptr, endptr, base)

typedef void *pointer;
#define STATIC  static
#define MKINIT  /* empty */

extern char nullstr[1];		/* null string */

#if DEBUG
#define TRACE(param)  sh_trace param
#else
#define TRACE(param)
#endif

/*
 * $PchId: shell.h,v 1.7 2006/05/22 12:47:00 philip Exp $
 */
