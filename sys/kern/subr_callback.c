/*	$NetBSD: subr_callback.c,v 1.7 2009/11/24 20:11:50 rmind Exp $	*/

/*-
 * Copyright (c)2006 YAMAMOTO Takashi,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_callback.c,v 1.7 2009/11/24 20:11:50 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/callback.h>

void
callback_head_init(struct callback_head *ch, int ipl)
{

	memset(ch, 0, sizeof(struct callback_head));
	mutex_init(&ch->ch_lock, MUTEX_DEFAULT, ipl);
	cv_init(&ch->ch_cv, "callback");
	TAILQ_INIT(&ch->ch_q);
#if 0 /* already zero-initialized */
	ch->ch_next = NULL;
	ch->ch_nentries = 0;
	ch->ch_running = 0;
	ch->ch_flags = 0;
#endif
}

void
callback_head_destroy(struct callback_head *ch)
{

	mutex_destroy(&ch->ch_lock);
	cv_destroy(&ch->ch_cv);
}

void
callback_register(struct callback_head *ch, struct callback_entry *ce,
    void *obj, int (*fn)(struct callback_entry *, void *, void *))
{

	KASSERT(fn != NULL);
	ce->ce_func = fn;
	ce->ce_obj = obj;
	mutex_enter(&ch->ch_lock);
	TAILQ_INSERT_TAIL(&ch->ch_q, ce, ce_q);
	ch->ch_nentries++;
	mutex_exit(&ch->ch_lock);
}

void
callback_unregister(struct callback_head *ch, struct callback_entry *ce)
{

	mutex_enter(&ch->ch_lock);
	while (ch->ch_running > 0)
		cv_wait(&ch->ch_cv, &ch->ch_lock);
	if (__predict_false(ch->ch_next == ce)) {
		ch->ch_next = TAILQ_NEXT(ce, ce_q);
	}
	TAILQ_REMOVE(&ch->ch_q, ce, ce_q);
	ch->ch_nentries--;
	mutex_exit(&ch->ch_lock);
}

static int
callback_runone(struct callback_head *ch, void *arg)
{
	struct callback_entry *ce;
	int result;

	KASSERT(ch->ch_nentries > 0);
	KASSERT(ch->ch_running > 0);

	ce = ch->ch_next;
	if (ce == NULL) {
		ce = TAILQ_FIRST(&ch->ch_q);
	}
	KASSERT(ce != NULL);
	KASSERT(ce->ce_func != NULL);
	result = (*ce->ce_func)(ce, ce->ce_obj, arg);
	ch->ch_next = TAILQ_NEXT(ce, ce_q);
	return result;
}

static void
callback_run_enter(struct callback_head *ch)
{

	mutex_enter(&ch->ch_lock);
	ch->ch_running++;
	mutex_exit(&ch->ch_lock);
}

static void
callback_run_leave(struct callback_head *ch)
{

	mutex_enter(&ch->ch_lock);
	KASSERT(ch->ch_running > 0);
	ch->ch_running--;
	if (ch->ch_running == 0)
		cv_broadcast(&ch->ch_cv);
	mutex_exit(&ch->ch_lock);
}

int
callback_run_roundrobin(struct callback_head *ch, void *arg)
{
	int i;
	int n;
	int result = 0;

	callback_run_enter(ch);
	n = ch->ch_nentries;
	for (i = 0; i < n; i++) {
		result = callback_runone(ch, arg);
		if (result != CALLBACK_CHAIN_CONTINUE) {
			break;
		}
	}
	callback_run_leave(ch);

	return result;
}
