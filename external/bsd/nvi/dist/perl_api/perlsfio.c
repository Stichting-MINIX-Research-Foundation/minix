/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1996
 *	Sven Verdoolaege. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: perlsfio.c,v 8.3 2000/04/30 17:00:15 skimo Exp  (Berkeley) Date: 2000/04/30 17:00:15 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: perlsfio.c,v 1.3 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

/* perl redefines them
 * avoid warnings
 */
#undef USE_DYNAMIC_LOADING
#undef DEBUG
#undef PACKAGE
#undef ARGS
#define ARGS ARGS

#include "config.h"

#include "../common/common.h"
#include "perl_api_extern.h"

/*
 * PUBLIC: #ifdef USE_SFIO
 */
#ifdef USE_SFIO

#define NIL(type)       ((type)0)

static int
sfnviwrite(f, buf, n, disc)
Sfio_t* f;      /* stream involved */
char*           buf;    /* buffer to read into */
int             n;      /* number of bytes to read */
Sfdisc_t*       disc;   /* discipline */        
{
	SCR *scrp;

	scrp = (SCR *)SvIV((SV*)SvRV(perl_get_sv("curscr", FALSE)));
	msgq(scrp, M_INFO, "%.*s", n, buf);
	return n;
}

/*
 * sfdcnewnvi --
 *	Create nvi discipline
 *
 * PUBLIC: Sfdisc_t* sfdcnewnvi __P((SCR*));
 */

Sfdisc_t *
sfdcnewnvi(scrp)
	SCR *scrp;
{
	Sfdisc_t*   disc;

	MALLOC(scrp, disc, Sfdisc_t*, sizeof(Sfdisc_t));
	if (!disc) return disc;

	disc->readf = (Sfread_f)NULL;
	disc->writef = sfnviwrite;
	disc->seekf = (Sfseek_f)NULL;
	disc->exceptf = (Sfexcept_f)NULL;
	return disc;
}

/*
 * PUBLIC: #endif
 */
#endif /* USE_SFIO */
