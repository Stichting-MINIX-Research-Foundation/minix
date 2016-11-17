/*	$NetBSD: rumpuser_component.c,v 1.6 2013/05/07 15:18:35 pooka Exp $	*/

/*
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpuser_component.c,v 1.6 2013/05/07 15:18:35 pooka Exp $");
#endif /* !lint */

#include <stdint.h>

/*
 * These interfaces affect the shlib major/minor; they can be called from
 * any program when applicable.  The rest of the interfaces provided
 * by rumpuser are part of the rump kernel/hypervisor contract and
 * are versioned by RUMPUSER_VERSION.
 */

#include <rump/rumpuser_component.h>

#include "rumpuser_int.h"

void *
rumpuser_component_unschedule(void)
{
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);
	return (void *)(intptr_t)nlocks;
}

void
rumpuser_component_schedule(void *cookie)
{
	int nlocks = (int)(intptr_t)cookie;

	rumpkern_sched(nlocks, NULL);
}

void
rumpuser_component_kthread(void)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_newlwp(0);
	rumpuser__hyp.hyp_unschedule();
}

struct lwp *
rumpuser_component_curlwp(void)
{
	struct lwp *l;

	rumpuser__hyp.hyp_schedule();
	l = rumpuser__hyp.hyp_lwproc_curlwp();
	rumpuser__hyp.hyp_unschedule();

	return l;
}

void
rumpuser_component_switchlwp(struct lwp *l)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_switch(l);
	rumpuser__hyp.hyp_unschedule();
}

void
rumpuser_component_kthread_release(void)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_release();
	rumpuser__hyp.hyp_unschedule();
}

int
rumpuser_component_errtrans(int hosterr)
{

	return rumpuser__errtrans(hosterr);
}
