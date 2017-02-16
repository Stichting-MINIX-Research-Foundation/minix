/*	$NetBSD: dispatch.c,v 1.4 2014/07/12 12:09:37 spz Exp $	*/
/* dispatch.c

   Network input dispatcher... */

/*
 * Copyright (c) 2004-2011,2013 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: dispatch.c,v 1.4 2014/07/12 12:09:37 spz Exp $");

#include "dhcpd.h"

#include <sys/time.h>

struct timeout *timeouts;
static struct timeout *free_timeouts;

void set_time(TIME t)
{
	/* Do any outstanding timeouts. */
	if (cur_tv . tv_sec != t) {
		cur_tv . tv_sec = t;
		cur_tv . tv_usec = 0;
		process_outstanding_timeouts ((struct timeval *)0);
	}
}

struct timeval *process_outstanding_timeouts (struct timeval *tvp)
{
	/* Call any expired timeouts, and then if there's
	   still a timeout registered, time out the select
	   call then. */
      another:
	if (timeouts) {
		struct timeout *t;
		if ((timeouts -> when . tv_sec < cur_tv . tv_sec) ||
		    ((timeouts -> when . tv_sec == cur_tv . tv_sec) &&
		     (timeouts -> when . tv_usec <= cur_tv . tv_usec))) {
			t = timeouts;
			timeouts = timeouts -> next;
			(*(t -> func)) (t -> what);
			if (t -> unref)
				(*t -> unref) (&t -> what, MDL);
			t -> next = free_timeouts;
			free_timeouts = t;
			goto another;
		}
		if (tvp) {
			tvp -> tv_sec = timeouts -> when . tv_sec;
			tvp -> tv_usec = timeouts -> when . tv_usec;
		}
		return tvp;
	} else
		return (struct timeval *)0;
}

/* Wait for packets to come in using select().   When one does, call
   receive_packet to receive the packet and possibly strip hardware
   addressing information from it, and then call through the
   bootp_packet_handler hook to try to do something with it. */

/*
 * Use the DHCP timeout list as a place to store DHCP specific
 * information, but use the ISC timer system to actually dispatch
 * the events.
 *
 * There are several things that the DHCP timer code does that the
 * ISC code doesn't:
 * 1) It allows for negative times
 * 2) The cancel arguments are different.  The DHCP code uses the
 * function and data to find the proper timer to cancel while the
 * ISC code uses a pointer to the timer.
 * 3) The DHCP code includes provision for incrementing and decrementing
 * a reference counter associated with the data.
 * The first one is fairly easy to fix but will take some time to go throuh
 * the callers and update them.  The second is also not all that difficult
 * in concept - add a pointer to the appropriate structures to hold a pointer
 * to the timer and use that.  The complications arise in trying to ensure
 * that all of the corner cases are covered.  The last one is potentially
 * more painful and requires more investigation.
 * 
 * The plan is continue with the older DHCP calls and timer list.  The
 * calls will continue to manipulate the list but will also pass a
 * timer to the ISC timer code for the actual dispatch.  Later, if desired,
 * we can go back and modify the underlying calls to use the ISC
 * timer functions directly without requiring all of the code to change
 * at the same time.
 */

void
dispatch(void)
{
	isc_result_t status;

	do {
		status = isc_app_ctxrun(dhcp_gbl_ctx.actx);

		/*
		 * isc_app_ctxrun can be stopped by receiving a
		 * signal. It will return ISC_R_RELOAD in that
		 * case. That is a normal behavior.
		 */

		if (status == ISC_R_RELOAD) {
			/*
			 * dhcp_set_control_state() will do the job.
			 * Note its first argument is ignored.
			 */
			status = dhcp_set_control_state(server_shutdown,
							server_shutdown);
			if (status == ISC_R_SUCCESS)
				status = ISC_R_RELOAD;
		}
	} while (status == ISC_R_RELOAD);

	log_fatal ("Dispatch routine failed: %s -- exiting",
		   isc_result_totext (status));
}

static void
isclib_timer_callback(isc_task_t  *taskp,
		      isc_event_t *eventp)
{
	struct timeout *t = (struct timeout *)eventp->ev_arg;
	struct timeout *q, *r;

	/* Get the current time... */
	gettimeofday (&cur_tv, (struct timezone *)0);

	/*
	 * Find the timeout on the dhcp list and remove it.
	 * As the list isn't ordered we search the entire list
	 */

	r = NULL;
	for (q = timeouts; q; q = q->next) {
		if (q == t) {
			if (r)
				r->next = q->next;
			else
				timeouts = q->next;
			break;
		}
		r = q;
	}

	/*
	 * The timer should always be on the list.  If it is we do
	 * the work and detach the timer block, if not we log an error.
	 * In both cases we attempt free the ISC event and continue
	 * processing.
	 */

	if (q != NULL) {
		/* call the callback function */
		(*(q->func)) (q->what);
		if (q->unref) {
			(*q->unref) (&q->what, MDL);
		}
		q->next = free_timeouts;
		isc_timer_detach(&q->isc_timeout);
		free_timeouts = q;
	} else {
		/*
		 * Hmm, we should clean up the timer structure but aren't
		 * sure about the pointer to the timer block we got so
		 * don't try to - may change this to a log_fatal
		 */
		log_error("Error finding timer structure");
	}

	isc_event_free(&eventp);
	return;
}

/* maximum value for usec */
#define USEC_MAX 1000000
#define DHCP_SEC_MAX  0xFFFFFFFF

void add_timeout (when, where, what, ref, unref)
	struct timeval *when;
	void (*where) (void *);
	void *what;
	tvref_t ref;
	tvunref_t unref;
{
	struct timeout *t, *q;
	int usereset = 0;
	isc_result_t status;
	int64_t sec;
	int usec;
	isc_interval_t interval;
	isc_time_t expires;

	/* See if this timeout supersedes an existing timeout. */
	t = (struct timeout *)0;
	for (q = timeouts; q; q = q->next) {
		if ((where == NULL || q->func == where) &&
		    q->what == what) {
			if (t)
				t->next = q->next;
			else
				timeouts = q->next;
			usereset = 1;
			break;
		}
		t = q;
	}

	/* If we didn't supersede a timeout, allocate a timeout
	   structure now. */
	if (!q) {
		if (free_timeouts) {
			q = free_timeouts;
			free_timeouts = q->next;
		} else {
			q = ((struct timeout *)
			     dmalloc(sizeof(struct timeout), MDL));
			if (!q) {
				log_fatal("add_timeout: no memory!");
			}
		}
		memset(q, 0, sizeof *q);
		q->func = where;
		q->ref = ref;
		q->unref = unref;
		if (q->ref)
			(*q->ref)(&q->what, what, MDL);
		else
			q->what = what;
	}

	/*
	 * The value passed in is a time from an epoch but we need a relative
	 * time so we need to do some math to try and recover the period.
	 * This is complicated by the fact that not all of the calls cared
	 * about the usec value, if it's zero we assume the caller didn't care.
	 *
	 * The ISC timer library doesn't seem to like negative values
	 * and can't accept any values above 4G-1 seconds so we limit
	 * the values to 0 <= value < 4G-1.  We do it before
	 * checking the trace option so that both the trace code and
	 * the working code use the same values.
	 */

	sec  = when->tv_sec - cur_tv.tv_sec;
	usec = when->tv_usec - cur_tv.tv_usec;
	
	if ((when->tv_usec != 0) && (usec < 0)) {
		sec--;
		usec += USEC_MAX;
	}

	if (sec < 0) {
		sec  = 0;
		usec = 0;
	} else if (sec > DHCP_SEC_MAX) {
		log_error("Timeout requested too large "
			  "reducing to 2^^32-1");
		sec = DHCP_SEC_MAX;
		usec = 0;
	} else if (usec < 0) {
		usec = 0;
	} else if (usec >= USEC_MAX) {
		usec = USEC_MAX - 1;
	}

	/* 
	 * This is necessary for the tracing code but we put it
	 * here in case we want to compare timing information
	 * for some reason, like debugging.
	 */
	q->when.tv_sec  = cur_tv.tv_sec + (sec & DHCP_SEC_MAX);
	q->when.tv_usec = usec;

#if defined (TRACING)
	if (trace_playback()) {
		/*
		 * If we are doing playback we need to handle the timers
		 * within this code rather than having the isclib handle
		 * them for us.  We need to keep the timer list in order
		 * to allow us to find the ones to timeout.
		 *
		 * By using a different timer setup in the playback we may
		 * have variations between the orginal and the playback but
		 * it's the best we can do for now.
		 */

		/* Beginning of list? */
		if (!timeouts || (timeouts->when.tv_sec > q-> when.tv_sec) ||
		    ((timeouts->when.tv_sec == q->when.tv_sec) &&
		     (timeouts->when.tv_usec > q->when.tv_usec))) {
			q->next = timeouts;
			timeouts = q;
			return;
		}

		/* Middle of list? */
		for (t = timeouts; t->next; t = t->next) {
			if ((t->next->when.tv_sec > q->when.tv_sec) ||
			    ((t->next->when.tv_sec == q->when.tv_sec) &&
			     (t->next->when.tv_usec > q->when.tv_usec))) {
				q->next = t->next;
				t->next = q;
				return;
			}
		}

		/* End of list. */
		t->next = q;
		q->next = (struct timeout *)0;
		return;
	}
#endif
	/*
	 * Don't bother sorting the DHCP list, just add it to the front.
	 * Eventually the list should be removed as we migrate the callers
	 * to the native ISC timer functions, if it becomes a performance
	 * problem before then we may need to order the list.
	 */
	q->next  = timeouts;
	timeouts = q;

	isc_interval_set(&interval, sec & DHCP_SEC_MAX, usec * 1000);
	status = isc_time_nowplusinterval(&expires, &interval);
	if (status != ISC_R_SUCCESS) {
		/*
		 * The system time function isn't happy or returned
		 * a value larger than isc_time_t can hold.
		 */
		log_fatal("Unable to set up timer: %s",
			  isc_result_totext(status));
	}

	if (usereset == 0) {
		status = isc_timer_create(dhcp_gbl_ctx.timermgr,
					  isc_timertype_once, &expires,
					  NULL, dhcp_gbl_ctx.task,
					  isclib_timer_callback,
					  (void *)q, &q->isc_timeout);
	} else {
		status = isc_timer_reset(q->isc_timeout,
					 isc_timertype_once, &expires,
					 NULL, 0);
	}

	/* If it fails log an error and die */
	if (status != ISC_R_SUCCESS) {
		log_fatal("Unable to add timeout to isclib\n");
	}

	return;
}

void cancel_timeout (where, what)
	void (*where) (void *);
	void *what;
{
	struct timeout *t, *q;

	/* Look for this timeout on the list, and unlink it if we find it. */
	t = (struct timeout *)0;
	for (q = timeouts; q; q = q -> next) {
		if (q->func == where && q->what == what) {
			if (t)
				t->next = q->next;
			else
				timeouts = q->next;
			break;
		}
		t = q;
	}

	/*
	 * If we found the timeout, cancel it and put it on the free list.
	 * The TRACING stuff is ugly but we don't add a timer when doing
	 * playback so we don't want to remove them then either.
	 */
	if (q) {
#if defined (TRACING)
		if (!trace_playback()) {
#endif
			isc_timer_detach(&q->isc_timeout);
#if defined (TRACING)
		}
#endif

		if (q->unref)
			(*q->unref) (&q->what, MDL);
		q->next = free_timeouts;
		free_timeouts = q;
	}
}

#if defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void cancel_all_timeouts ()
{
	struct timeout *t, *n;
	for (t = timeouts; t; t = n) {
		n = t->next;
		isc_timer_detach(&t->isc_timeout);
		if (t->unref && t->what)
			(*t->unref) (&t->what, MDL);
		t->next = free_timeouts;
		free_timeouts = t;
	}
}

void relinquish_timeouts ()
{
	struct timeout *t, *n;
	for (t = free_timeouts; t; t = n) {
		n = t->next;
		dfree(t, MDL);
	}
}
#endif
