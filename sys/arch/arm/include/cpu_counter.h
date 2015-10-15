/*	$NetBSD: cpu_counter.h,v 1.3 2014/02/26 01:54:10 matt Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#ifndef _ARM_CPU_COUNTER_H_
#define _ARM_CPU_COUNTER_H_

/*
 * ARM specific support for CPU counter (ARM11 and Cortex only).
 * If __HAVE_CPU_COUNTER is defined for any other CPU_*, it will crash.
 */

#ifdef _KERNEL

#include <sys/cpu.h>

#if defined(CPU_CORTEX) || defined(CPU_ARM11)
#define cpu_hascounter()	(curcpu()->ci_data.cpu_cc_freq != 0)
#else
#define cpu_hascounter()	false
#endif

#define cpu_counter()		cpu_counter32()

#if defined(CPU_CORTEX) || defined(CPU_ARM11)
static __inline uint32_t
cpu_counter32(void)
{
#if defined(CPU_CORTEX) && defined(CPU_ARM11)
	const bool cortex_p = CPU_ID_CORTEX_P(curcpu()->ci_arm_cpuid);
#elif defined(CPU_CORTEX)
	const bool cortex_p = true;
#elif defined(CPU_ARM11)
	const bool cortex_p = false;
#endif

	if (cortex_p)
		return armreg_pmccntr_read();
	else
		return armreg_pmccntrv6_read();
}
#endif

static __inline uint64_t
cpu_frequency(struct cpu_info *ci)
{
	return ci->ci_data.cpu_cc_freq;
}

#endif /* _KERNEL */

#endif /* _ARM_CPU_COUNTER_H_ */
