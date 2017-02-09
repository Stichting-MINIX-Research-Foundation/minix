/*	$NetBSD: sys_pmc.c,v 1.11 2014/01/25 21:11:49 christos Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_pmc.c,v 1.11 2014/01/25 21:11:49 christos Exp $");

#include "opt_perfctrs.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/syscallargs.h>
#include <sys/types.h>

#if defined(PERFCTRS)
#include <sys/pmc.h>
#endif

/*
 * XXX We need a multiprocessor locking protocol!
 */

int
sys_pmc_control(struct lwp *l, const struct sys_pmc_control_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) ctr;
		syscallarg(int) op;
		syscallarg(void *) args;
	} */
#ifndef PERFCTRS
	return ENXIO;
#else
	struct pmc_counter_cfg cfg;
	void *args;
	int ctr, operation, error=0;

	ctr = SCARG(uap, ctr);
	__USE(ctr);
	operation = SCARG(uap, op);

	KERNEL_LOCK(1, NULL);
	switch (operation) {
	case PMC_OP_START:
		if (!pmc_counter_isconfigured(l->l_proc, ctr)) {
			error = ENXIO;
		} else if (pmc_counter_isrunning(l->l_proc, ctr)) {
			error = EINPROGRESS;
		} else {
			pmc_enable_counter(l->l_proc, ctr);
		}
		break;
	case PMC_OP_STOP:
		if (!pmc_counter_isconfigured(l->l_proc, ctr)) {
			error = ENXIO;
		} else if (pmc_counter_isrunning(l->l_proc, ctr)) {
			pmc_disable_counter(l->l_proc, ctr);
		}
		break;
	case PMC_OP_CONFIGURE:
		args = SCARG(uap, args);

		if (pmc_counter_isrunning(l->l_proc, ctr)) {
			pmc_disable_counter(l->l_proc, ctr);
		}
		error = copyin(args, &cfg, sizeof(struct pmc_counter_cfg));
		if (error == 0) {
			error = pmc_configure_counter(l->l_proc, ctr, &cfg);
		}
		break;
	case PMC_OP_PROFSTART:
		args = SCARG(uap, args);

		error = copyin(args, &cfg, sizeof(struct pmc_counter_cfg));
		if (error == 0) {
			error = pmc_start_profiling(ctr, &cfg);
		}
		break;
	case PMC_OP_PROFSTOP:
		error = pmc_stop_profiling(ctr);
		break;
	default:
		error = EINVAL;
		break;
	}
	KERNEL_UNLOCK_ONE(NULL);
	return error;
#endif
}

int
sys_pmc_get_info(struct lwp *l, const struct sys_pmc_get_info_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) ctr;
		syscallarg(int) op;
		syscallarg(void *) args;
	} */
#ifndef PERFCTRS
	return ENXIO;
#else
	uint64_t val;
	void *args;
	int nctrs, ctr, ctrt, request, error=0, flags=0;

	ctr = SCARG(uap, ctr);
	request = SCARG(uap, op);
	args = SCARG(uap, args);
	__USE(flags);

	KERNEL_LOCK(1, NULL);
	nctrs = pmc_get_num_counters();
	switch (request) {
	case PMC_INFO_NCOUNTERS:	/* args should be (int *) */
		error = copyout(&nctrs, args, sizeof(int));
		break;

	case PMC_INFO_CPUCTR_TYPE:	/* args should be (int *) */
		ctrt = pmc_get_counter_type(ctr);
		error = copyout(&ctrt, args, sizeof(int));
		break;
					/* args should be (pmc_ctr_t *) */
	case PMC_INFO_ACCUMULATED_COUNTER_VALUE:
		flags = PMC_VALUE_FLAGS_CHILDREN;
		/*FALLTHROUGH*/
	case PMC_INFO_COUNTER_VALUE:
		if (ctr < 0 || ctr >= nctrs) {
			error = EINVAL;
			break;
		}
		error = pmc_get_counter_value(l->l_proc, ctr, flags, &val);
		if (error == 0) {
			error = copyout(&val, args, sizeof(uint64_t));
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	KERNEL_UNLOCK_ONE(NULL);
	return error;
#endif
}
