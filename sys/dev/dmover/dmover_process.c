/*	$NetBSD: dmover_process.c,v 1.5 2011/05/14 14:49:19 jakllsch Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * dmover_process.c: Processing engine for dmover-api.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dmover_process.c,v 1.5 2011/05/14 14:49:19 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/intr.h>
#include <sys/mutex.h>

#include <dev/dmover/dmovervar.h>

TAILQ_HEAD(, dmover_request) dmover_completed_q;
kmutex_t dmover_completed_q_lock;

void	*dmover_completed_si;

void	dmover_complete(void *);

/*
 * dmover_process_init:
 *
 *	Initialize the processing engine.
 */
void
dmover_process_initialize(void)
{

	TAILQ_INIT(&dmover_completed_q);
	mutex_init(&dmover_completed_q_lock, MUTEX_DEFAULT, IPL_BIO);

	dmover_completed_si = softint_establish(SOFTINT_CLOCK,
	    dmover_complete, NULL);
}

/*
 * dmover_process:	[client interface function]
 *
 *	Submit a tranform request for processing.
 */
void
dmover_process(struct dmover_request *dreq)
{
	struct dmover_session *dses = dreq->dreq_session;
	struct dmover_assignment *das;
	struct dmover_backend *dmb;
	int s;

#ifdef DIAGNOSTIC
	if ((dreq->dreq_flags & DMOVER_REQ_WAIT) != 0 &&
	    dreq->dreq_callback != NULL)
		panic("dmover_process: WAIT used with callback");
#endif

	/* Clear unwanted flag bits. */
	dreq->dreq_flags &= __DMOVER_REQ_FLAGS_PRESERVE;

	s = splbio();

	/* XXXLOCK */

	/* XXX Right now, the back-end is statically assigned. */
	das = &dses->__dses_assignment;

	dmb = das->das_backend;
	dreq->dreq_assignment = das;

	dmover_session_insque(dses, dreq);
	dmover_backend_insque(dmb, dreq);

	/* XXXUNLOCK */

	splx(s);

	/* Kick the back-end into action. */
	(*dmb->dmb_process)(das->das_backend);

	if (dreq->dreq_flags & DMOVER_REQ_WAIT) {
		s = splbio();
		/* XXXLOCK */
		while ((dreq->dreq_flags & DMOVER_REQ_DONE) == 0)
			(void) tsleep(dreq, PRIBIO, "dmover", 0);
		/* XXXUNLOCK */
		splx(s);
	}
}

/*
 * dmover_done:		[back-end interface function]
 *
 *	Back-end notification that the dmover is done.
 */
void
dmover_done(struct dmover_request *dreq)
{
	struct dmover_session *dses = dreq->dreq_session;
	int s;

	s = splbio();

	/* XXXLOCK */

	dmover_session_remque(dses, dreq);
	/* backend has removed it from its queue */

	/* XXXUNLOCK */

	dreq->dreq_flags |= DMOVER_REQ_DONE;
	dreq->dreq_flags &= ~DMOVER_REQ_RUNNING;
	dreq->dreq_assignment = NULL;

	if (dreq->dreq_callback != NULL) {
		mutex_enter(&dmover_completed_q_lock);
		TAILQ_INSERT_TAIL(&dmover_completed_q, dreq, dreq_dmbq);
		mutex_exit(&dmover_completed_q_lock);
		softint_schedule(dmover_completed_si);
	} else if (dreq->dreq_flags & DMOVER_REQ_WAIT)
		wakeup(dreq);

	splx(s);
}

/*
 * dmover_complete:
 *
 *	Complete a request by invoking the callback.
 */
void
dmover_complete(void *arg)
{
	struct dmover_request *dreq;

	for (;;) {
		mutex_enter(&dmover_completed_q_lock);
		if ((dreq = TAILQ_FIRST(&dmover_completed_q)) != NULL)
			TAILQ_REMOVE(&dmover_completed_q, dreq, dreq_dmbq);
		mutex_exit(&dmover_completed_q_lock);

		if (dreq == NULL)
			return;

		(*dreq->dreq_callback)(dreq);
	}
}
