/*	$NetBSD: t_atomic.c,v 1.5 2014/12/10 04:37:53 christos Exp $	*/

/*
 * Copyright (C) 2011, 2013  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* Id: t_atomic.c,v 1.2 2011/01/11 23:47:12 tbox Exp  */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/atomic.h>
#include <isc/mem.h>
#include <isc/util.h>
#include <isc/string.h>
#include <isc/print.h>
#include <isc/event.h>
#include <isc/task.h>

#include <tests/t_api.h>

char *progname;

#define CHECK(x) RUNTIME_CHECK(ISC_R_SUCCESS == (x))

isc_mem_t *mctx = NULL;
isc_taskmgr_t *task_manager = NULL;

#if defined(ISC_PLATFORM_HAVEXADD) || defined(ISC_PLATFORM_HAVEXADDQ)
static void
setup(void) {
	/* 1 */ CHECK(isc_mem_create(0, 0, &mctx));
	/* 2 */ CHECK(isc_taskmgr_create(mctx, 32, 0, &task_manager));
}

static void
teardown(void) {
	/* 2 */ isc_taskmgr_destroy(&task_manager);
	/* 1 */ isc_mem_destroy(&mctx);
}
#endif

#define TASKS 32
#define ITERATIONS 10000
#define COUNTS_PER_ITERATION 1000
#define INCREMENT_64 (isc_int64_t)0x0000000010000000
#define EXPECTED_COUNT_32 (TASKS * ITERATIONS * COUNTS_PER_ITERATION)
#define EXPECTED_COUNT_64 (TASKS * ITERATIONS * COUNTS_PER_ITERATION * INCREMENT_64)

typedef struct {
	isc_uint32_t iteration;
} counter_t;

counter_t counters[TASKS];

void do_xaddq(isc_task_t *task, isc_event_t *ev);

#if defined(ISC_PLATFORM_HAVEXADD)
isc_int32_t counter_32;

void do_xadd(isc_task_t *task, isc_event_t *ev);

void
do_xadd(isc_task_t *task, isc_event_t *ev) {
	counter_t *state = (counter_t *)ev->ev_arg;
	int i;

	for (i = 0 ; i < COUNTS_PER_ITERATION ; i++) {
		isc_atomic_xadd(&counter_32, 1);
	}

	state->iteration++;
	if (state->iteration < ITERATIONS) {
		isc_task_send(task, &ev);
	} else {
		isc_event_free(&ev);
	}
}

static void
test_atomic_xadd() {
	int test_result;
	isc_task_t *tasks[TASKS];
	isc_event_t *event;
	int i;

	t_assert("test_atomic_xadd", 1, T_REQUIRED, "%s",
		 "ensure that isc_atomic_xadd() works.");

	setup();

	memset(counters, 0, sizeof(counters));
	counter_32 = 0;

	/*
	 * Create our tasks, and allocate an event to get the counters going.
	 */
	for (i = 0 ; i < TASKS ; i++) {
		tasks[i] = NULL;
		CHECK(isc_task_create(task_manager, 0, &tasks[i]));
		event = isc_event_allocate(mctx, NULL, 1000, do_xadd,
					   &counters[i], sizeof(struct isc_event));
		isc_task_sendanddetach(&tasks[i], &event);
	}

	teardown();

	test_result = T_PASS;
	t_info("32-bit counter %d, expected %d\n", counter_32, EXPECTED_COUNT_32);
	if (counter_32 != EXPECTED_COUNT_32)
		test_result = T_FAIL;
	t_result(test_result);

	counter_32 = 0;
}
#endif

#if defined(ISC_PLATFORM_HAVEXADDQ)
isc_int64_t counter_64;

void do_xaddq(isc_task_t *task, isc_event_t *ev);

void
do_xaddq(isc_task_t *task, isc_event_t *ev) {
	counter_t *state = (counter_t *)ev->ev_arg;
	int i;

	for (i = 0 ; i < COUNTS_PER_ITERATION ; i++) {
		isc_atomic_xaddq(&counter_64, INCREMENT_64);
	}

	state->iteration++;
	if (state->iteration < ITERATIONS) {
		isc_task_send(task, &ev);
	} else {
		isc_event_free(&ev);
	}
}

static void
test_atomic_xaddq() {
	int test_result;
	isc_task_t *tasks[TASKS];
	isc_event_t *event;
	int i;

	t_assert("test_atomic_xaddq", 1, T_REQUIRED, "%s",
		 "ensure that isc_atomic_xaddq() works.");

	setup();

	memset(counters, 0, sizeof(counters));
	counter_64 = 0;

	/*
	 * Create our tasks, and allocate an event to get the counters going.
	 */
	for (i = 0 ; i < TASKS ; i++) {
		tasks[i] = NULL;
		CHECK(isc_task_create(task_manager, 0, &tasks[i]));
		event = isc_event_allocate(mctx, NULL, 1000, do_xaddq,
					   &counters[i], sizeof(struct isc_event));
		isc_task_sendanddetach(&tasks[i], &event);
	}

	teardown();

	test_result = T_PASS;
	t_info("64-bit counter %"ISC_PRINT_QUADFORMAT"d, expected %"ISC_PRINT_QUADFORMAT"d\n",
	       counter_64, EXPECTED_COUNT_64);
	if (counter_64 != EXPECTED_COUNT_64)
		test_result = T_FAIL;
	t_result(test_result);

	counter_64 = 0;
}
#endif


testspec_t T_testlist[] = {
#if defined(ISC_PLATFORM_HAVEXADD)
	{ (PFV) test_atomic_xadd,	"test_atomic_xadd"		},
#endif
#if defined(ISC_PLATFORM_HAVEXADDQ)
	{ (PFV) test_atomic_xaddq,	"test_atomic_xaddq"		},
#endif
	{ (PFV) 0,			NULL }
};

#ifdef WIN32
int
main(int argc, char **argv) {
	t_settests(T_testlist);
	return (t_main(argc, argv));
}
#endif
