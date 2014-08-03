/*	$NetBSD: error.c,v 1.38 2012/03/15 02:02:20 joerg Exp $	*/

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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)error.c	8.2 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: error.c,v 1.38 2012/03/15 02:02:20 joerg Exp $");
#endif
#endif /* not lint */

/*
 * Errors and exceptions.
 */

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "shell.h"
#include "eval.h" /* for commandname */
#include "main.h"
#include "options.h"
#include "output.h"
#include "error.h"
#include "show.h"


/*
 * Code to handle exceptions in C.
 */

struct jmploc *handler;
int exception;
volatile int suppressint;
volatile int intpending;


static void exverror(int, const char *, va_list) __dead;

/*
 * Called to raise an exception.  Since C doesn't include exceptions, we
 * just do a longjmp to the exception handler.  The type of exception is
 * stored in the global variable "exception".
 */

void
exraise(int e)
{
	if (handler == NULL)
		abort();
	exception = e;
	longjmp(handler->loc, 1);
}


/*
 * Called from trap.c when a SIGINT is received.  (If the user specifies
 * that SIGINT is to be trapped or ignored using the trap builtin, then
 * this routine is not called.)  Suppressint is nonzero when interrupts
 * are held using the INTOFF macro.  The call to _exit is necessary because
 * there is a short period after a fork before the signal handlers are
 * set to the appropriate value for the child.  (The test for iflag is
 * just defensive programming.)
 */

void
onint(void)
{
	sigset_t nsigset;

	if (suppressint) {
		intpending = 1;
		return;
	}
	intpending = 0;
	sigemptyset(&nsigset);
	sigprocmask(SIG_SETMASK, &nsigset, NULL);
	if (rootshell && iflag)
		exraise(EXINT);
	else {
		signal(SIGINT, SIG_DFL);
		raise(SIGINT);
	}
	/* NOTREACHED */
}

static __printflike(2, 0) void
exvwarning(int sv_errno, const char *msg, va_list ap)
{
	/* Partially emulate line buffered output so that:
	 *	printf '%d\n' 1 a 2
	 * and
	 *	printf '%d %d %d\n' 1 a 2
	 * both generate sensible text when stdout and stderr are merged.
	 */
	if (output.nextc != output.buf && output.nextc[-1] == '\n')
		flushout(&output);
	if (commandname)
		outfmt(&errout, "%s: ", commandname);
	else
		outfmt(&errout, "%s: ", getprogname());
	if (msg != NULL) {
		doformat(&errout, msg, ap);
		if (sv_errno >= 0)
			outfmt(&errout, ": ");
	}
	if (sv_errno >= 0)
		outfmt(&errout, "%s", strerror(sv_errno));
	out2c('\n');
	flushout(&errout);
}

/*
 * Exverror is called to raise the error exception.  If the second argument
 * is not NULL then error prints an error message using printf style
 * formatting.  It then raises the error exception.
 */
static __printflike(2, 0) void
exverror(int cond, const char *msg, va_list ap)
{
	CLEAR_PENDING_INT;
	INTOFF;

#ifdef DEBUG
	if (msg) {
		TRACE(("exverror(%d, \"", cond));
		TRACEV((msg, ap));
		TRACE(("\") pid=%d\n", getpid()));
	} else
		TRACE(("exverror(%d, NULL) pid=%d\n", cond, getpid()));
#endif
	if (msg)
		exvwarning(-1, msg, ap);

	flushall();
	exraise(cond);
	/* NOTREACHED */
}


void
error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	exverror(EXERROR, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}


void
exerror(int cond, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	exverror(cond, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}

/*
 * error/warning routines for external builtins
 */

void
sh_exit(int rval)
{
	exerrno = rval & 255;
	exraise(EXEXEC);
}

void
sh_err(int status, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	exvwarning(errno, fmt, ap);
	va_end(ap);
	sh_exit(status);
}

void
sh_verr(int status, const char *fmt, va_list ap)
{
	exvwarning(errno, fmt, ap);
	sh_exit(status);
}

void
sh_errx(int status, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	exvwarning(-1, fmt, ap);
	va_end(ap);
	sh_exit(status);
}

void
sh_verrx(int status, const char *fmt, va_list ap)
{
	exvwarning(-1, fmt, ap);
	sh_exit(status);
}

void
sh_warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	exvwarning(errno, fmt, ap);
	va_end(ap);
}

void
sh_vwarn(const char *fmt, va_list ap)
{
	exvwarning(errno, fmt, ap);
}

void
sh_warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	exvwarning(-1, fmt, ap);
	va_end(ap);
}

void
sh_vwarnx(const char *fmt, va_list ap)
{
	exvwarning(-1, fmt, ap);
}


/*
 * Table of error messages.
 */

struct errname {
	short errcode;		/* error number */
	short action;		/* operation which encountered the error */
	const char *msg;	/* text describing the error */
};


#define ALL (E_OPEN|E_CREAT|E_EXEC)

STATIC const struct errname errormsg[] = {
	{ EINTR,	ALL,	"interrupted" },
	{ EACCES,	ALL,	"permission denied" },
	{ EIO,		ALL,	"I/O error" },
	{ EEXIST,	ALL,	"file exists" },
	{ ENOENT,	E_OPEN,	"no such file" },
	{ ENOENT,	E_CREAT,"directory nonexistent" },
	{ ENOENT,	E_EXEC,	"not found" },
	{ ENOTDIR,	E_OPEN,	"no such file" },
	{ ENOTDIR,	E_CREAT,"directory nonexistent" },
	{ ENOTDIR,	E_EXEC,	"not found" },
	{ EISDIR,	ALL,	"is a directory" },
#ifdef EMFILE
	{ EMFILE,	ALL,	"too many open files" },
#endif
	{ ENFILE,	ALL,	"file table overflow" },
	{ ENOSPC,	ALL,	"file system full" },
#ifdef EDQUOT
	{ EDQUOT,	ALL,	"disk quota exceeded" },
#endif
#ifdef ENOSR
	{ ENOSR,	ALL,	"no streams resources" },
#endif
	{ ENXIO,	ALL,	"no such device or address" },
	{ EROFS,	ALL,	"read-only file system" },
	{ ETXTBSY,	ALL,	"text busy" },
#ifdef EAGAIN
	{ EAGAIN,	E_EXEC,	"not enough memory" },
#endif
	{ ENOMEM,	ALL,	"not enough memory" },
#ifdef ENOLINK
	{ ENOLINK,	ALL,	"remote access failed" },
#endif
#ifdef EMULTIHOP
	{ EMULTIHOP,	ALL,	"remote access failed" },
#endif
#ifdef ECOMM
	{ ECOMM,	ALL,	"remote access failed" },
#endif
#ifdef ESTALE
	{ ESTALE,	ALL,	"remote access failed" },
#endif
#ifdef ETIMEDOUT
	{ ETIMEDOUT,	ALL,	"remote access failed" },
#endif
#ifdef ELOOP
	{ ELOOP,	ALL,	"symbolic link loop" },
#endif
#ifdef ENAMETOOLONG
	{ ENAMETOOLONG,	ALL,	"file name too long" },
#endif
	{ E2BIG,	E_EXEC,	"argument list too long" },
#ifdef ELIBACC
	{ ELIBACC,	E_EXEC,	"shared library missing" },
#endif
	{ 0,		0,	NULL },
};


/*
 * Return a string describing an error.  The returned string may be a
 * pointer to a static buffer that will be overwritten on the next call.
 * Action describes the operation that got the error.
 */

const char *
errmsg(int e, int action)
{
	struct errname const *ep;
	static char buf[12];

	for (ep = errormsg ; ep->errcode ; ep++) {
		if (ep->errcode == e && (ep->action & action) != 0)
			return ep->msg;
	}
	fmtstr(buf, sizeof buf, "error %d", e);
	return buf;
}
