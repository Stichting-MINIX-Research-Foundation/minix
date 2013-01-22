/*	$NetBSD: pthread.c,v 1.1.1.2 2008/05/18 14:29:50 aymeric Exp $ */

/*-
 * Copyright (c) 2000
 *	Sven Verdoolaege.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: pthread.c,v 1.4 2000/07/22 14:52:37 skimo Exp (Berkeley) Date: 2000/07/22 14:52:37";
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

#include <pthread.h>

#include "../common/common.h"

static int vi_pthread_run __P((WIN *wp, void *(*fun)(void*), void *data));
static int vi_pthread_lock_init __P((WIN *, void **));
static int vi_pthread_lock_end __P((WIN *, void **));
static int vi_pthread_lock_try __P((WIN *, void **));
static int vi_pthread_lock_unlock __P((WIN *, void **));

/*
 * thread_init
 *
 * PUBLIC: void thread_init __P((GS *gp));
 */
void 
thread_init(GS *gp)
{
	gp->run = vi_pthread_run;
	gp->lock_init = vi_pthread_lock_init;
	gp->lock_end = vi_pthread_lock_end;
	gp->lock_try = vi_pthread_lock_try;
	gp->lock_unlock = vi_pthread_lock_unlock;
}

static int
vi_pthread_run(WIN *wp, void *(*fun)(void*), void *data)
{
	pthread_t *t = malloc(sizeof(pthread_t));
	pthread_create(t, NULL, fun, data);
	return 0;
}

static int 
vi_pthread_lock_init (WIN * wp, void **p)
{
	pthread_mutex_t *mutex;
	int rc;

	MALLOC_RET(NULL, mutex, pthread_mutex_t *, sizeof(*mutex));

	if (rc = pthread_mutex_init(mutex, NULL)) {
		free(mutex);
		*p = NULL;
		return rc;
	}
	*p = mutex;
	return 0;
}

static int 
vi_pthread_lock_end (WIN * wp, void **p)
{
	int rc;
	pthread_mutex_t *mutex = (pthread_mutex_t *)*p;

	if (rc = pthread_mutex_destroy(mutex))
		return rc;

	free(mutex);
	*p = NULL;
	return 0;
}

static int 
vi_pthread_lock_try (WIN * wp, void **p)
{
	printf("try %p\n", *p);
	fflush(stdout);
	return 0;
	return pthread_mutex_trylock((pthread_mutex_t *)*p);
}

static int 
vi_pthread_lock_unlock (WIN * wp, void **p)
{
	printf("unlock %p\n", *p);
	return 0;
	return pthread_mutex_unlock((pthread_mutex_t *)*p);
}

