/* $NetBSD: cpu_msr.h,v 1.7 2009/10/05 23:59:31 rmind Exp $ */

/*-
 * Copyright (c) 2007 Juan Romero Pardines.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _X86_CPU_MSR_H
#define _X86_CPU_MSR_H

#include <sys/param.h>
#include <sys/types.h>

#ifdef _KERNEL

struct msr_rw_info {
	int		msr_read;
	int		msr_type;
	uint64_t	msr_value;
	uint64_t	msr_mask;
};

static inline void
x86_msr_xcall(void *arg1, void *arg2)
{
	struct msr_rw_info *msrdat = arg1;
	uint64_t msr = 0;

	KASSERT(msrdat->msr_type != 0);

	/* Read the MSR requested and apply the mask if defined. */
	if (msrdat->msr_read) {
		msr = rdmsr(msrdat->msr_type);
		if (msrdat->msr_mask) {
			msr &= ~msrdat->msr_mask;
		}
	}
	/* Assign (or extract, on read) the value and perform the write. */
	msr |= msrdat->msr_value;
	wrmsr(msrdat->msr_type, msr);
}

#endif /* ! _KERNEL */
#endif /* ! _X86_CPU_MSR_H */
