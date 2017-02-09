/*	$NetBSD: net_stats.c,v 1.4 2008/05/04 07:22:14 thorpej Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: net_stats.c,v 1.4 2008/05/04 07:22:14 thorpej Exp $");

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/kmem.h>

#include <net/net_stats.h>

typedef struct {
	uint64_t	*ctx_counters;	/* pointer to collated counter array */
	u_int		 ctx_ncounters;	/* number of counters in array */
} netstat_sysctl_context;

/*
 * netstat_convert_to_user_cb --
 *	Internal routine used as a call-back for percpu data enumeration.
 */
static void
netstat_convert_to_user_cb(void *v1, void *v2, struct cpu_info *ci)
{
	const uint64_t *local_counters = v1;
	netstat_sysctl_context *ctx = v2;
	u_int i;

	for (i = 0; i < ctx->ctx_ncounters; i++)
		ctx->ctx_counters[i] += local_counters[i];
}

/*
 * netstat_sysctl --
 *	Common routine for collating and reporting network statistics
 *	that are gathered per-CPU.  Statistics counters are assumed
 *	to be arrays of uint64_t's.
 */
int
netstat_sysctl(percpu_t *stat, u_int ncounters, SYSCTLFN_ARGS)
{
	netstat_sysctl_context ctx;
	struct sysctlnode node;
	uint64_t *counters;
	size_t countersize;
	int rv;

	countersize = sizeof(uint64_t) * ncounters;

	counters = kmem_zalloc(countersize, KM_SLEEP);
	if (counters == NULL)
		return (ENOMEM);

	ctx.ctx_counters = counters;
	ctx.ctx_ncounters = ncounters;

	percpu_foreach(stat, netstat_convert_to_user_cb, &ctx);

	node = *rnode;
	node.sysctl_data = counters;
	node.sysctl_size = countersize;
	rv = sysctl_lookup(SYSCTLFN_CALL(&node));

	kmem_free(counters, countersize);

	return (rv);
}
