/*	$NetBSD: cpu.h,v 1.22 2015/04/22 17:38:33 pooka Exp $	*/

/*
 * Copyright (c) 2008-2011 Antti Kantee.  All Rights Reserved.
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

/*
 * CPU defitions for a generic arch.  Unfortunately there are some
 * MD #ifdefs here.  They are required because of MD inlines and macros.
 */

#ifndef _SYS_RUMP_CPU_H_
#define _SYS_RUMP_CPU_H_

#ifndef _LOCORE

#include <sys/cpu_data.h>
#include <machine/pcb.h>

#include "rump_curlwp.h"

struct cpu_info {
	struct cpu_data ci_data;
	cpuid_t ci_cpuid;
	struct lwp *ci_curlwp;

	struct cpu_info *ci_next;

#ifdef __alpha__
	uint64_t ci_pcc_freq;
#endif

#ifdef __vax__
	int ci_ipimsgs;
#define IPI_SEND_CNCHAR 0
#define IPI_DDB 0
#endif /* __vax__ */

#ifdef __powerpc__
	struct cache_info {
		int dcache_size;
		int dcache_line_size;
		int icache_size;
		int icache_line_size;
	} ci_ci;
#endif /* __powerpc */
};

#ifdef __vax__
static __inline void cpu_handle_ipi(void) {}
#endif /* __vax__ */

#ifdef __powerpc__
void __syncicache(void *, size_t);
#endif /* __powerpc__ */

#define curlwp rump_curlwp_fast()

#define curcpu() (curlwp->l_cpu)
#define cpu_number() (cpu_index(curcpu))

extern struct cpu_info *rumpcpu_info_list;
#define CPU_INFO_ITERATOR		int __unused
#define CPU_INFO_FOREACH(_cii_, _ci_)	_cii_ = 0, _ci_ = rumpcpu_info_list; \
					_ci_ != NULL; _ci_ = _ci_->ci_next
#define CPU_IS_PRIMARY(_ci_)		(_ci_->ci_index == 0)

#define CLKF_USERMODE(framep)	0
#define CLKF_PC(framep)		0
#define CLKF_INTR(framep)	0

#endif /* !_LOCORE */

#endif /* _SYS_RUMP_CPU_H_ */
