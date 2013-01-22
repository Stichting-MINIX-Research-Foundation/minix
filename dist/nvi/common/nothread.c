/*	$NetBSD: nothread.c,v 1.1.1.2 2008/05/18 14:29:48 aymeric Exp $ */

/*-
 * Copyright (c) 2000
 *	Sven Verdoolaege.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: nothread.c,v 1.4 2000/07/22 14:52:37 skimo Exp (Berkeley) Date: 2000/07/22 14:52:37";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

static int vi_nothread_run __P((WIN *wp, void *(*fun)(void*), void *data));
static int vi_nothread_lock __P((WIN *, void **));

/*
 * thread_init
 *
 * PUBLIC: void thread_init __P((GS *gp));
 */
void 
thread_init(GS *gp)
{
	gp->run = vi_nothread_run;
	gp->lock_init = vi_nothread_lock;
	gp->lock_end = vi_nothread_lock;
	gp->lock_try = vi_nothread_lock;
	gp->lock_unlock = vi_nothread_lock;
}

static int
vi_nothread_run(WIN *wp, void *(*fun)(void*), void *data)
{
	fun(data);
	return 0;
}

static int 
vi_nothread_lock (WIN * wp, void **lp)
{
	return 0;
}
