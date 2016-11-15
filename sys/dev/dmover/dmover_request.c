/*	$NetBSD: dmover_request.c,v 1.8 2011/05/14 18:24:47 jakllsch Exp $	*/

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
 * dmover_request.c: Request management functions for dmover-api.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dmover_request.c,v 1.8 2011/05/14 18:24:47 jakllsch Exp $");

#include <sys/param.h>
#include <sys/pool.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/dmover/dmovervar.h>

pool_cache_t dmover_request_cache;

static bool initialized;

void
dmover_request_initialize(void)
{

	KASSERT(initialized == false);

	dmover_request_cache = pool_cache_init(sizeof(struct dmover_request),
		0, 0, 0, "dmreq", NULL, IPL_BIO, NULL, NULL, NULL);

	initialized = true;
}

/*
 * dmover_request_alloc:	[client interface function]
 *
 *	Allocate a transform request for the specified session.
 */
struct dmover_request *
dmover_request_alloc(struct dmover_session *dses, dmover_buffer *inbuf)
{
	struct dmover_request *dreq;
	int inputs = dses->dses_ninputs;

	if (__predict_false(initialized == false))
		return (NULL);

	dreq = pool_cache_get(dmover_request_cache, PR_NOWAIT);
	if (dreq == NULL)
		return (NULL);

	memset(dreq, 0, sizeof(*dreq));

	if (inputs != 0) {
		if (inbuf == NULL) {
			inbuf = malloc(sizeof(dmover_buffer) * inputs,
			    M_DEVBUF, M_NOWAIT);
			if (inbuf == NULL) {
				pool_cache_put(dmover_request_cache, dreq);
				return (NULL);
			}
		}
		dreq->dreq_flags |= __DMOVER_REQ_INBUF_FREE;
		dreq->dreq_inbuf = inbuf;
	}

	dreq->dreq_session = dses;

	return (dreq);
}

/*
 * dmover_request_free:		[client interface function]
 *
 *	Free a dmover request.
 */
void
dmover_request_free(struct dmover_request *dreq)
{

	KASSERT(initialized == true);

	if (dreq->dreq_flags & __DMOVER_REQ_INBUF_FREE)
		free(dreq->dreq_inbuf, M_DEVBUF);

	pool_cache_put(dmover_request_cache, dreq);
}
