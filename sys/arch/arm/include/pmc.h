/*	$NetBSD: pmc.h,v 1.3 2002/08/09 05:27:10 thorpej Exp $	*/

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

#ifndef _ARM_PMC_H_
#define	_ARM_PMC_H_

#define	PMC_CLASS_I80200	0x10000		/* i80200-compatible */
#define	PMC_TYPE_I80200_CCNT	0x10001		/* cycle counter */
#define	PMC_TYPE_I80200_PMCx	0x10002		/* performance counter */

#if defined(_KERNEL)

#include <arm/cpuconf.h>

struct arm_pmc_funcs {
	void	(*fork)(struct proc *p1, struct proc *p2);
	int	(*num_counters)(void);
	int	(*counter_type)(int ctr);
	void	(*save_context)(struct proc *p);
	void	(*restore_context)(struct proc *p);
	void	(*enable_counter)(struct proc *p, int ctr);
	void	(*disable_counter)(struct proc *p, int ctr);
	void	(*accumulate)(struct proc *parent, struct proc *child);
	void	(*process_exit)(struct proc *p);
	int	(*configure_counter)(struct proc *p, int ctr, struct pmc_counter_cfg *cfg);
	int	(*get_counter_val)(struct proc *p, int ctr, int flags, uint64_t *pval);
	int	(*counter_isconfigured)(struct proc *p, int ctr);
	int	(*counter_isrunning)(struct proc *p, int ctr);
	int	(*start_profiling)(int ctr, struct pmc_counter_cfg *cfg);
	int	(*stop_profiling)(int ctr);
	int	(*alloc_kernel_ctr)(int ctr, struct pmc_counter_cfg *cfg);
	int	(*free_kernel_ctr)(int ctr);
};
extern struct arm_pmc_funcs *arm_pmc;

#define pmc_md_fork(p1,p2)		(arm_pmc->fork((p1),(p2)))
#define pmc_get_num_counters()		(arm_pmc->num_counters())
#define pmc_get_counter_type(c)		(arm_pmc->counter_type(c))
#define pmc_save_context(p)		(arm_pmc->save_context(p))
#define pmc_restore_context(p)		(arm_pmc->restore_context(p))
#define pmc_enable_counter(p,c)		(arm_pmc->enable_counter((p),(c)))
#define pmc_disable_counter(p,c)	(arm_pmc->disable_counter((p),(c)))
#define pmc_accumulate(p1,p2)		(arm_pmc->accumulate((p1),(p2)))
#define pmc_process_exit(p1)		(arm_pmc->process_exit(p))
#define pmc_counter_isconfigured(p,c)	(arm_pmc->counter_isconfigured((p),(c)))
#define pmc_counter_isrunning(p,c)	(arm_pmc->counter_isrunning((p),(c)))
#define pmc_start_profiling(c,f)	(arm_pmc->start_profiling((c),(f)))
#define pmc_stop_profiling(c)		(arm_pmc->stop_profiling((c)))
#define pmc_alloc_kernel_counter(c,f)	(arm_pmc->alloc_kernel_ctr((c),(f)))
#define pmc_free_kernel_counter(c)	(arm_pmc->free_kernel_ctr((c)))
#define pmc_configure_counter(p,c,f)	\
				(arm_pmc->configure_counter((p),(c),(f)))
#define pmc_get_counter_value(p,c,f,pv)	\
				(arm_pmc->get_counter_val((p),(c),(f),(pv)))

#define PMC_ENABLED(p)			(p)->p_md.pmc_enabled

#endif /* defined(_KERNEL) */

#endif /* _ARM_PMC_H_ */
