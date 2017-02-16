/*	$NetBSD: app.c,v 1.6 2014/12/10 04:38:01 christos Exp $	*/

/*
 * Copyright (C) 2004, 2007, 2009, 2013, 2014  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* Id: app.c,v 1.9 2009/09/02 23:48:03 tbox Exp  */

#include <config.h>

#include <sys/types.h>

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <process.h>

#include <isc/app.h>
#include <isc/boolean.h>
#include <isc/condition.h>
#include <isc/mem.h>
#include <isc/msgs.h>
#include <isc/mutex.h>
#include <isc/event.h>
#include <isc/platform.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/time.h>
#include <isc/util.h>
#include <isc/thread.h>

/*%
 * For BIND9 internal applications built with threads, we use a single app
 * context and let multiple worker, I/O, timer threads do actual jobs.
 */

static isc_thread_t	blockedthread;

/*%
 * The following are intended for internal use (indicated by "isc__"
 * prefix) but are not declared as static, allowing direct access from
 * unit tests etc.
 */
isc_result_t isc__app_start(void);
isc_result_t isc__app_ctxstart(isc_appctx_t *ctx);
isc_result_t isc__app_onrun(isc_mem_t *mctx, isc_task_t *task,
			    isc_taskaction_t action, void *arg);
isc_result_t isc__app_ctxrun(isc_appctx_t *ctx);
isc_result_t isc__app_run(void);
isc_result_t isc__app_ctxshutdown(isc_appctx_t *ctx);
isc_result_t isc__app_shutdown(void);
isc_result_t isc__app_reload(void);
isc_result_t isc__app_ctxsuspend(isc_appctx_t *ctx);
void isc__app_ctxfinish(isc_appctx_t *ctx);
void isc__app_finish(void);
void isc__app_block(void);
void isc__app_unblock(void);
isc_result_t isc__appctx_create(isc_mem_t *mctx, isc_appctx_t **ctxp);
void isc__appctx_destroy(isc_appctx_t **ctxp);
void isc__appctx_settaskmgr(isc_appctx_t *ctx, isc_taskmgr_t *taskmgr);
void isc__appctx_setsocketmgr(isc_appctx_t *ctx, isc_socketmgr_t *socketmgr);
void isc__appctx_settimermgr(isc_appctx_t *ctx, isc_timermgr_t *timermgr);
isc_result_t isc__app_ctxonrun(isc_appctx_t *ctx, isc_mem_t *mctx,
			       isc_task_t *task, isc_taskaction_t action,
			       void *arg);

/*
 * The application context of this module.  This implementation actually
 * doesn't use it. (This may change in the future).
 */
#define APPCTX_MAGIC		ISC_MAGIC('A', 'p', 'c', 'x')
#define VALID_APPCTX(c)		ISC_MAGIC_VALID(c, APPCTX_MAGIC)

/* Events to wait for */

#define NUM_EVENTS 2

enum {
	RELOAD_EVENT,
	SHUTDOWN_EVENT
};

typedef struct isc__appctx {
	isc_appctx_t		common;
	isc_mem_t		*mctx;
	isc_eventlist_t		on_run;
	isc_mutex_t		lock;
	isc_boolean_t		shutdown_requested;
	isc_boolean_t		running;
	/*
	 * We assume that 'want_shutdown' can be read and written atomically.
	 */
	isc_boolean_t		want_shutdown;
	/*
	 * We assume that 'want_reload' can be read and written atomically.
	 */
	isc_boolean_t		want_reload;

	isc_boolean_t		blocked;

	HANDLE			hEvents[NUM_EVENTS];

	isc_taskmgr_t		*taskmgr;
	isc_socketmgr_t		*socketmgr;
	isc_timermgr_t		*timermgr;
} isc__appctx_t;

static isc__appctx_t isc_g_appctx;

static struct {
	isc_appmethods_t methods;

	/*%
	 * The following are defined just for avoiding unused static functions.
	 */
	void *run, *shutdown, *start, *reload, *finish, *block, *unblock;
} appmethods = {
	{
		isc__appctx_destroy,
		isc__app_ctxstart,
		isc__app_ctxrun,
		isc__app_ctxsuspend,
		isc__app_ctxshutdown,
		isc__app_ctxfinish,
		isc__appctx_settaskmgr,
		isc__appctx_setsocketmgr,
		isc__appctx_settimermgr,
		isc__app_ctxonrun
	},
	(void *)isc__app_run,
	(void *)isc__app_shutdown,
	(void *)isc__app_start,
	(void *)isc__app_reload,
	(void *)isc__app_finish,
	(void *)isc__app_block,
	(void *)isc__app_unblock
};

/*
 * We need to remember which thread is the main thread...
 */
static isc_thread_t	main_thread;

isc_result_t
isc__app_ctxstart(isc_appctx_t *ctx0) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;
	isc_result_t result;

	REQUIRE(VALID_APPCTX(ctx));

	/*
	 * Start an ISC library application.
	 */

	main_thread = GetCurrentThread();

	result = isc_mutex_init(&ctx->lock);
	if (result != ISC_R_SUCCESS)
		return (result);

	ctx->shutdown_requested = ISC_FALSE;
	ctx->running = ISC_FALSE;
	ctx->want_shutdown = ISC_FALSE;
	ctx->want_reload = ISC_FALSE;
	ctx->blocked  = ISC_FALSE;

	/* Create the reload event in a non-signaled state */
	ctx->hEvents[RELOAD_EVENT] = CreateEvent(NULL, FALSE, FALSE, NULL);

	/* Create the shutdown event in a non-signaled state */
	ctx->hEvents[SHUTDOWN_EVENT] = CreateEvent(NULL, FALSE, FALSE, NULL);

	ISC_LIST_INIT(ctx->on_run);
	return (ISC_R_SUCCESS);
}

isc_result_t
isc__app_start(void) {
	isc_g_appctx.common.impmagic = APPCTX_MAGIC;
	isc_g_appctx.common.magic = ISCAPI_APPCTX_MAGIC;
	isc_g_appctx.common.methods = &appmethods.methods;
	isc_g_appctx.mctx = NULL;
	/* The remaining members will be initialized in ctxstart() */

	return (isc__app_ctxstart((isc_appctx_t *)&isc_g_appctx));
}

isc_result_t
isc__app_onrun(isc_mem_t *mctx, isc_task_t *task, isc_taskaction_t action,
	       void *arg)
{
	return (isc__app_ctxonrun((isc_appctx_t *)&isc_g_appctx, mctx,
				  task, action, arg));
}

isc_result_t
isc__app_ctxonrun(isc_appctx_t *ctx0, isc_mem_t *mctx, isc_task_t *task,
		  isc_taskaction_t action, void *arg)
{
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;
	isc_event_t *event;
	isc_task_t *cloned_task = NULL;
	isc_result_t result;

	LOCK(&ctx->lock);

	if (ctx->running) {
		result = ISC_R_ALREADYRUNNING;
		goto unlock;
	}

	/*
	 * Note that we store the task to which we're going to send the event
	 * in the event's "sender" field.
	 */
	isc_task_attach(task, &cloned_task);
	event = isc_event_allocate(mctx, cloned_task, ISC_APPEVENT_SHUTDOWN,
				   action, arg, sizeof(*event));
	if (event == NULL) {
		result = ISC_R_NOMEMORY;
		goto unlock;
	}

	ISC_LIST_APPEND(ctx->on_run, event, ev_link);

	result = ISC_R_SUCCESS;

 unlock:
	UNLOCK(&ctx->lock);

	return (result);
}

isc_result_t
isc__app_ctxrun(isc_appctx_t *ctx0) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;
	isc_event_t *event, *next_event;
	isc_task_t *task;
	HANDLE *pHandles = NULL;
	DWORD  dwWaitResult;

	REQUIRE(VALID_APPCTX(ctx));

	REQUIRE(main_thread == GetCurrentThread());

	LOCK(&ctx->lock);

	if (!ctx->running) {
		ctx->running = ISC_TRUE;

		/*
		 * Post any on-run events (in FIFO order).
		 */
		for (event = ISC_LIST_HEAD(ctx->on_run);
		     event != NULL;
		     event = next_event) {
			next_event = ISC_LIST_NEXT(event, ev_link);
			ISC_LIST_UNLINK(ctx->on_run, event, ev_link);
			task = event->ev_sender;
			event->ev_sender = NULL;
			isc_task_sendanddetach(&task, &event);
		}

	}

	UNLOCK(&ctx->lock);

	/*
	 * There is no danger if isc_app_shutdown() is called before we wait
	 * for events.
	 */

	while (!ctx->want_shutdown) {
		dwWaitResult = WaitForMultipleObjects(NUM_EVENTS, ctx->hEvents,
						      FALSE, INFINITE);

		/* See why we returned */

		if (WaitSucceeded(dwWaitResult, NUM_EVENTS)) {
			/*
			 * The return was due to one of the events
			 * being signaled
			 */
			switch (WaitSucceededIndex(dwWaitResult)) {
			case RELOAD_EVENT:
				ctx->want_reload = ISC_TRUE;
				break;

			case SHUTDOWN_EVENT:
				ctx->want_shutdown = ISC_TRUE;
				break;
			}
		}

		if (ctx->want_reload) {
			ctx->want_reload = ISC_FALSE;
			return (ISC_R_RELOAD);
		}

		if (ctx->want_shutdown && ctx->blocked)
			exit(-1);
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
isc__app_run(void) {
	return (isc__app_ctxrun((isc_appctx_t *)&isc_g_appctx));
}

isc_result_t
isc__app_ctxshutdown(isc_appctx_t *ctx0) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;
	isc_boolean_t want_kill = ISC_TRUE;

	REQUIRE(VALID_APPCTX(ctx));

	LOCK(&ctx->lock);

	REQUIRE(ctx->running);

	if (ctx->shutdown_requested)
		want_kill = ISC_FALSE;		/* We're only signaling once */
	else
		ctx->shutdown_requested = ISC_TRUE;

	UNLOCK(&ctx->lock);

	if (want_kill)
		SetEvent(ctx->hEvents[SHUTDOWN_EVENT]);

	return (ISC_R_SUCCESS);
}

isc_result_t
isc__app_shutdown(void) {
	return (isc__app_ctxshutdown((isc_appctx_t *)&isc_g_appctx));
}

isc_result_t
isc__app_ctxsuspend(isc_appctx_t *ctx0) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;
	isc_boolean_t want_kill = ISC_TRUE;

	REQUIRE(VALID_APPCTX(ctx));

	LOCK(&ctx->lock);

	REQUIRE(ctx->running);

	/*
	 * Don't send the reload signal if we're shutting down.
	 */
	if (ctx->shutdown_requested)
		want_kill = ISC_FALSE;

	UNLOCK(&ctx->lock);

	if (want_kill)
		SetEvent(ctx->hEvents[RELOAD_EVENT]);

	return (ISC_R_SUCCESS);
}

isc_result_t
isc__app_reload(void) {
	return (isc__app_ctxsuspend((isc_appctx_t *)&isc_g_appctx));
}

void
isc__app_ctxfinish(isc_appctx_t *ctx0) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;

	REQUIRE(VALID_APPCTX(ctx));

	DESTROYLOCK(&ctx->lock);
}

void
isc__app_finish(void) {
	isc__app_ctxfinish((isc_appctx_t *)&isc_g_appctx);
}

void
isc__app_block(void) {
	REQUIRE(isc_g_appctx.running);
	REQUIRE(!isc_g_appctx.blocked);

	isc_g_appctx.blocked = ISC_TRUE;
	blockedthread = GetCurrentThread();
}

void
isc__app_unblock(void) {
	REQUIRE(isc_g_appctx.running);
	REQUIRE(isc_g_appctx.blocked);

	isc_g_appctx.blocked = ISC_FALSE;
	REQUIRE(blockedthread == GetCurrentThread());
}

isc_result_t
isc__appctx_create(isc_mem_t *mctx, isc_appctx_t **ctxp) {
	isc__appctx_t *ctx;

	REQUIRE(mctx != NULL);
	REQUIRE(ctxp != NULL && *ctxp == NULL);

	ctx = isc_mem_get(mctx, sizeof(*ctx));
	if (ctx == NULL)
		return (ISC_R_NOMEMORY);

	ctx->common.impmagic = APPCTX_MAGIC;
	ctx->common.magic = ISCAPI_APPCTX_MAGIC;
	ctx->common.methods = &appmethods.methods;

	ctx->mctx = NULL;
	isc_mem_attach(mctx, &ctx->mctx);

	ctx->taskmgr = NULL;
	ctx->socketmgr = NULL;
	ctx->timermgr = NULL;

	*ctxp = (isc_appctx_t *)ctx;

	return (ISC_R_SUCCESS);
}

void
isc__appctx_destroy(isc_appctx_t **ctxp) {
	isc__appctx_t *ctx;

	REQUIRE(ctxp != NULL);
	ctx = (isc__appctx_t *)*ctxp;
	REQUIRE(VALID_APPCTX(ctx));

	isc_mem_putanddetach(&ctx->mctx, ctx, sizeof(*ctx));

	*ctxp = NULL;
}

void
isc__appctx_settaskmgr(isc_appctx_t *ctx0, isc_taskmgr_t *taskmgr) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;

	REQUIRE(VALID_APPCTX(ctx));

	ctx->taskmgr = taskmgr;
}

void
isc__appctx_setsocketmgr(isc_appctx_t *ctx0, isc_socketmgr_t *socketmgr) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;

	REQUIRE(VALID_APPCTX(ctx));

	ctx->socketmgr = socketmgr;
}

void
isc__appctx_settimermgr(isc_appctx_t *ctx0, isc_timermgr_t *timermgr) {
	isc__appctx_t *ctx = (isc__appctx_t *)ctx0;

	REQUIRE(VALID_APPCTX(ctx));

	ctx->timermgr = timermgr;
}

isc_result_t
isc__app_register(void) {
	return (isc_app_register(isc__appctx_create));
}

#include "../app_api.c"
