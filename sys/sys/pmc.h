/*	$NetBSD: pmc.h,v 1.5 2005/12/11 12:25:20 christos Exp $	*/

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

#ifndef _SYS_PMC_H_
#define	_SYS_PMC_H_

struct pmc_counter_cfg {
	pmc_evid_t	event_id;		/* What event to configure */
	pmc_ctr_t	reset_value;		/* Value to set counter to */
	uint32_t	flags;			/* PMC_FLAG_* */
};

#if defined(_KERNEL)

/*
 *      The following functions are defined in machine/pmc.h as either
 * functions or macros.
 *
 * int	pmc_get_num_counters(void);
 * int	pmc_get_counter_type(int ctr);
 *
 * void	pmc_save_context(struct proc *p);
 * void	pmc_restore_context(struct proc *p);
 * void	pmc_process_exit(struct proc *p);
 *
 * int	pmc_enable_counter(struct proc *p, int ctr);
 * int	pmc_disable_counter(struct proc *p, int ctr);
 * int	pmc_counter_isrunning(struct proc *p, int ctr);
 * int	pmc_counter_isconfigured(struct proc *p, int ctr);
 *
 * int	pmc_configure_counter(struct proc *p, int ctr, pmc_counter_cfg *cfg);
 *
 * int	pmc_get_counter_value(struct proc *p, int ctr, int flags,
 *          uint64_t *pval);
 *
 * int	pmc_accumulate(struct proc *p_parent, struct proc *p_exiting);
 *
 * int	pmc_alloc_kernel_counter(int ctr, struct pmc_counter_cfg *cfg);
 * int	pmc_free_kernel_counter(int ctr);
 *
 * int	pmc_start_profiling(int ctr, struct pmc_counter_cfg *cfg);
 * int	pmc_stop_profiling(int ctr);
 *
 * int	PMC_ENABLED(struct proc *p);
 */

#define PMC_VALUE_FLAGS_CHILDREN	0x00000001

#endif	/* _KERNEL */

#include <machine/pmc.h>

#define PMC_OP_START		1
#define PMC_OP_STOP		2
#define PMC_OP_CONFIGURE	3
#define PMC_OP_PROFSTART	4
#define PMC_OP_PROFSTOP		5
int	pmc_control(int ctr, int operation, void *args);

#define PMC_INFO_NCOUNTERS			1
#define PMC_INFO_CPUCTR_TYPE			2
#define PMC_INFO_COUNTER_VALUE			3
#define PMC_INFO_ACCUMULATED_COUNTER_VALUE	4
int	pmc_get_info(int ctr, int request, void *data);

#endif	/* !_SYS_PMC_H_ */
