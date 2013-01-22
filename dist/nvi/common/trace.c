/*	$NetBSD: trace.c,v 1.1.1.2 2008/05/18 14:29:52 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: trace.c,v 8.4 1997/08/03 15:04:23 bostic Exp (Berkeley) Date: 1997/08/03 15:04:23";
#endif /* not lint */

#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "common.h"

#ifdef TRACE

static FILE *tfp;

/*
 * vtrace_end --
 *	End tracing.
 *
 * PUBLIC: void vtrace_end __P((void));
 */
void
vtrace_end()
{
	if (tfp != NULL && tfp != stderr)
		(void)fclose(tfp);
}

/*
 * vtrace_init --
 *	Initialize tracing.
 *
 * PUBLIC: void vtrace_init __P((char *));
 */
void
vtrace_init(name)
	char *name;
{
	if (name == NULL || (tfp = fopen(name, "w")) == NULL)
		tfp = stderr;
	vtrace("\n=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\nTRACE\n");
}

/*
 * vtrace --
 *	Debugging trace routine.
 *
 * PUBLIC: void vtrace __P((const char *, ...));
 */
void
#ifdef __STDC__
vtrace(const char *fmt, ...)
#else
vtrace(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	if (tfp == NULL)
		vtrace_init(NULL);

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}
#endif
