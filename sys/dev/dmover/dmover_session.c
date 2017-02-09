/*	$NetBSD: dmover_session.c,v 1.6 2011/05/14 18:24:47 jakllsch Exp $	*/

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
 * dmover_session.c: Session management functions for dmover-api.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dmover_session.c,v 1.6 2011/05/14 18:24:47 jakllsch Exp $");

#include <sys/param.h>
#include <sys/pool.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/dmover/dmovervar.h>

struct pool dmover_session_pool;

static bool initialized;

void
dmover_session_initialize(void)
{

	KASSERT(initialized == false);

	pool_init(&dmover_session_pool, sizeof(struct dmover_session),
	    0, 0, 0, "dmses", &pool_allocator_nointr, IPL_NONE);
	initialized = true;
}

/*
 * dmover_session_create:	[client interface function]
 *
 *	Create a dmover session.
 */
int
dmover_session_create(const char *type, struct dmover_session **dsesp)
{
	struct dmover_session *dses;
	int error;

	if (__predict_false(initialized == false)) {
		error = initialized ? false : ENXIO;

		if (error)
			return (error);
	}

	dses = pool_get(&dmover_session_pool, PR_NOWAIT);
	if (__predict_false(dses == NULL))
		return (ENOMEM);

	/* Allocate a back-end to the session. */
	error = dmover_backend_alloc(dses, type);
	if (__predict_false(error)) {
		pool_put(&dmover_session_pool, dses);
		return (error);
	}

	TAILQ_INIT(&dses->__dses_pendreqs);
	dses->__dses_npendreqs = 0;

	*dsesp = dses;
	return (0);
}

/*
 * dmover_session_destroy:	[client interface function]
 *
 *	Tear down a dmover session.
 */
void
dmover_session_destroy(struct dmover_session *dses)
{

	KASSERT(initialized == true);

	/* XXX */
	if (dses->__dses_npendreqs)
		panic("dmover_session_destroy");

	dmover_backend_release(dses);
	pool_put(&dmover_session_pool, dses);
}
