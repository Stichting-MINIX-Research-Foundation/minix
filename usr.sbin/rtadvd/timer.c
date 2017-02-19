/*	$NetBSD: timer.c,v 1.12 2015/06/05 14:09:20 roy Exp $	*/
/*	$KAME: timer.c,v 1.11 2005/04/14 06:22:35 suz Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/time.h>

#include <limits.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include "timer.h"

struct rtadvd_timer_head_t ra_timer = TAILQ_HEAD_INITIALIZER(ra_timer);
static struct timespec tm_limit = { LONG_MAX, 1000000000L - 1 };
static struct timespec tm_max;

void
rtadvd_timer_init(void)
{

	TAILQ_INIT(&ra_timer);
	tm_max = tm_limit;
}

struct rtadvd_timer *
rtadvd_add_timer(struct rtadvd_timer *(*timeout) (void *),
    void (*update) (void *, struct timespec *),
    void *timeodata, void *updatedata)
{
	struct rtadvd_timer *newtimer;

	if ((newtimer = malloc(sizeof(*newtimer))) == NULL) {
		syslog(LOG_ERR,
		       "<%s> can't allocate memory", __func__);
		exit(1);
	}

	memset(newtimer, 0, sizeof(*newtimer));

	if (timeout == NULL) {
		syslog(LOG_ERR,
		       "<%s> timeout function unspecified", __func__);
		exit(1);
	}
	newtimer->expire = timeout;
	newtimer->update = update;
	newtimer->expire_data = timeodata;
	newtimer->update_data = updatedata;
	newtimer->tm = tm_max;

	/* link into chain */
	TAILQ_INSERT_TAIL(&ra_timer, newtimer, next);

	return(newtimer);
}

void
rtadvd_remove_timer(struct rtadvd_timer **timer)
{

	if (*timer) {
		TAILQ_REMOVE(&ra_timer, *timer, next);
		free(*timer);
		*timer = NULL;
	}
}

void
rtadvd_set_timer(struct timespec *tm, struct rtadvd_timer *timer)
{
	struct timespec now;

	/* reset the timer */
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecadd(&now, tm, &timer->tm);

	/* upate the next expiration time */
	if (timespeccmp(&timer->tm, &tm_max, <))
		tm_max = timer->tm;
}

/*
 * Check expiration for each timer. If a timer expires,
 * call the expire function for the timer and update the timer.
 * Return the next interval for select() call.
 */
struct timespec *
rtadvd_check_timer(void)
{
	static struct timespec returnval;
	struct timespec now;
	struct rtadvd_timer *tm, *tmn;

	clock_gettime(CLOCK_MONOTONIC, &now);
	tm_max = tm_limit;

	TAILQ_FOREACH_SAFE(tm, &ra_timer, next, tmn) {
		if (timespeccmp(&tm->tm, &now, <=)) {
			if ((*tm->expire)(tm->expire_data) == NULL)
				continue; /* the timer was removed */
			if (tm->update)
				(*tm->update)(tm->update_data, &tm->tm);
			timespecadd(&tm->tm, &now, &tm->tm);
		}
		if (timespeccmp(&tm->tm, &tm_max, <))
			tm_max = tm->tm;
	}

	if (timespeccmp(&tm_max, &tm_limit, ==))
		return(NULL);
	if (timespeccmp(&tm_max, &now, <)) {
		/* this may occur when the interval is too small */
		timespecclear(&returnval);
	} else
		timespecsub(&tm_max, &now, &returnval);
	return(&returnval);
}

struct timespec *
rtadvd_timer_rest(struct rtadvd_timer *timer)
{
	static struct timespec returnval;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	if (timespeccmp(&timer->tm, &now, <=)) {
		syslog(LOG_DEBUG,
		       "<%s> a timer must be expired, but not yet",
		       __func__);
		returnval.tv_sec = 0;
		returnval.tv_nsec = 0;
	}
	else
		timespecsub(&timer->tm, &now, &returnval);

	return(&returnval);
}
