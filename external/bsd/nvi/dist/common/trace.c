/*	$NetBSD: trace.c,v 1.4 2014/01/26 21:43:45 christos Exp $	*/
/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: trace.c,v 8.4 1997/08/03 15:04:23 bostic Exp  (Berkeley) Date: 1997/08/03 15:04:23 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: trace.c,v 1.4 2014/01/26 21:43:45 christos Exp $");
#endif

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
vtrace_end(void)
{
	if (tfp != NULL && tfp != stderr)
		(void)fclose(tfp);
}

/*
 * vtrace_init --
 *	Initialize tracing.
 *
 * PUBLIC: void vtrace_init __P((const char *));
 */
void
vtrace_init(const char *name)
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
vtrace(const char *fmt, ...)
{
	va_list ap;

	if (tfp == NULL)
		vtrace_init(NULL);

	va_start(ap, fmt);
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}
#endif
