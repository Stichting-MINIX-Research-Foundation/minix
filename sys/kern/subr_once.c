/*	$NetBSD: subr_once.c,v 1.6 2009/03/15 17:14:40 cegger Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
 * Copyright (c)2008 Antti Kantee,
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
__KERNEL_RCSID(0, "$NetBSD: subr_once.c,v 1.6 2009/03/15 17:14:40 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/once.h>

static kmutex_t oncemtx;
static kcondvar_t oncecv;

void
once_init(void)
{

	mutex_init(&oncemtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&oncecv, "runonce");
}

int
_run_once(once_t *o, int (*fn)(void))
{

	/* Fastpath handled by RUN_ONCE() */

	mutex_enter(&oncemtx);
	if (o->o_status == ONCE_VIRGIN) {
		o->o_status = ONCE_RUNNING;
		mutex_exit(&oncemtx);
		o->o_error = fn();
		mutex_enter(&oncemtx);
		o->o_status = ONCE_DONE;
		cv_broadcast(&oncecv);
	}
	while (o->o_status != ONCE_DONE)
		cv_wait(&oncecv, &oncemtx);
	mutex_exit(&oncemtx);

	KASSERT(o->o_status == ONCE_DONE);
	return o->o_error;
}
