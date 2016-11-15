/*	$NetBSD: dmover_backend.c,v 1.9 2011/05/14 18:24:47 jakllsch Exp $	*/

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
 * dmover_backend.c: Backend management functions for dmover-api.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dmover_backend.c,v 1.9 2011/05/14 18:24:47 jakllsch Exp $");

#include <sys/param.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/once.h>

#include <dev/dmover/dmovervar.h>

TAILQ_HEAD(, dmover_backend) dmover_backend_list;
kmutex_t dmover_backend_list_lock;
static bool initialized;

static int
initialize(void)
{

	KASSERT(initialized == false);

	TAILQ_INIT(&dmover_backend_list);
	mutex_init(&dmover_backend_list_lock, MUTEX_DEFAULT, IPL_VM);

	/* Initialize the other bits of dmover. */
	dmover_session_initialize();
	dmover_request_initialize();
	dmover_process_initialize();

	initialized = true;

	return 0;
}

/*
 * dmover_backend_register:	[back-end interface function]
 *
 *	Register a back-end with dmover-api.
 */
void
dmover_backend_register(struct dmover_backend *dmb)
{
	static ONCE_DECL(control);

	RUN_ONCE(&control, initialize);

	KASSERT(initialized == true);

	LIST_INIT(&dmb->dmb_sessions);
	dmb->dmb_nsessions = 0;

	TAILQ_INIT(&dmb->dmb_pendreqs);
	dmb->dmb_npendreqs = 0;

	mutex_enter(&dmover_backend_list_lock);
	TAILQ_INSERT_TAIL(&dmover_backend_list, dmb, dmb_list);
	mutex_exit(&dmover_backend_list_lock);
}

/*
 * dmover_backend_unregister:	[back-end interface function]
 *
 *	Un-register a back-end from dmover-api.
 */
void
dmover_backend_unregister(struct dmover_backend *dmb)
{

	KASSERT(initialized == true);

	/* XXX */
	if (dmb->dmb_nsessions)
		panic("dmover_backend_unregister");

	mutex_enter(&dmover_backend_list_lock);
	TAILQ_REMOVE(&dmover_backend_list, dmb, dmb_list);
	mutex_exit(&dmover_backend_list_lock);
}

/*
 * dmover_backend_alloc:
 *
 *	Allocate and return a back-end on behalf of a session.
 */
int
dmover_backend_alloc(struct dmover_session *dses, const char *type)
{
	struct dmover_backend *dmb, *best_dmb = NULL;
	const struct dmover_algdesc *algdesc, *best_algdesc = NULL;

	if (__predict_false(initialized == false)) {
		return (ESRCH);
	}

	mutex_enter(&dmover_backend_list_lock);

	/* First, find a back-end that can handle the session parts. */
	for (dmb = TAILQ_FIRST(&dmover_backend_list); dmb != NULL;
	     dmb = TAILQ_NEXT(dmb, dmb_list)) {
		/*
		 * First, check to see if the back-end supports the
		 * function we wish to perform.
		 */
		algdesc = dmover_algdesc_lookup(dmb->dmb_algdescs,
		    dmb->dmb_nalgdescs, type);
		if (algdesc == NULL)
			continue;

		if (best_dmb == NULL) {
			best_dmb = dmb;
			best_algdesc = algdesc;
			continue;
		}

		/*
		 * XXX All the stuff from here on should be shot in
		 * XXX the head.  Instead, we should build a list
		 * XXX of candidates, and select the best back-end
		 * XXX when a request is scheduled for processing.
		 */

		if (dmb->dmb_speed >= best_dmb->dmb_speed) {
			/*
			 * If the current best match is slower than
			 * this back-end, then this one is the new
			 * best match.
			 */
			if (dmb->dmb_speed > best_dmb->dmb_speed) {
				best_dmb = dmb;
				best_algdesc = algdesc;
				continue;
			}

			/*
			 * If this back-end has fewer sessions allocated
			 * to it than the current best match, then this
			 * one is now the best match.
			 */
			if (best_dmb->dmb_nsessions > dmb->dmb_nsessions) {
				best_dmb = dmb;
				best_algdesc = algdesc;
				continue;
			}
		}
	}
	if (best_dmb == NULL) {
		mutex_exit(&dmover_backend_list_lock);
		return (ESRCH);
	}

	KASSERT(best_algdesc != NULL);

	/* Plug the back-end into the static (XXX) assignment. */
	dses->__dses_assignment.das_backend = best_dmb;
	dses->__dses_assignment.das_algdesc = best_algdesc;

	dses->dses_ninputs = best_algdesc->dad_ninputs;

	LIST_INSERT_HEAD(&best_dmb->dmb_sessions, dses, __dses_list);
	best_dmb->dmb_nsessions++;

	mutex_exit(&dmover_backend_list_lock);

	return (0);
}

/*
 * dmover_backend_release:
 *
 *	Release the back-end from the specified session.
 */
void
dmover_backend_release(struct dmover_session *dses)
{
	struct dmover_backend *dmb;

	mutex_enter(&dmover_backend_list_lock);

	/* XXX Clear out the static assignment. */
	dmb = dses->__dses_assignment.das_backend;
	dses->__dses_assignment.das_backend = NULL;
	dses->__dses_assignment.das_algdesc = NULL;

	LIST_REMOVE(dses, __dses_list);
	dmb->dmb_nsessions--;

	mutex_exit(&dmover_backend_list_lock);
}
