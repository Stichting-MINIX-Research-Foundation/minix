/*	$NetBSD: apple_smcvar.h,v 1.4 2014/04/01 17:49:05 riastradh Exp $	*/

/*
 * Apple System Management Controller State
 */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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

#ifndef _DEV_IC_APPLE_SMCVAR_H_
#define _DEV_IC_APPLE_SMCVAR_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/device_if.h>
#include <sys/mutex.h>

struct apple_smc_tag {
	device_t		smc_dev;
	bus_space_tag_t		smc_bst;
	bus_space_handle_t	smc_bsh;
	bus_size_t		smc_size;
	uint32_t		smc_nkeys;	/* number of keys in the SMC */
	kmutex_t		smc_io_lock;	/* excludes I/O with the SMC */

#if 0				/* XXX sysctl */
	struct sysctllog	*smc_sysctllog;
	const struct sysctlnode	*smc_sysctlnode;
#endif
};

void	apple_smc_attach(struct apple_smc_tag *);
int	apple_smc_detach(struct apple_smc_tag *, int);
int	apple_smc_rescan(struct apple_smc_tag *, const char *, const int *);
void	apple_smc_child_detached(struct apple_smc_tag *, device_t);

#endif  /* _DEV_IC_APPLE_SMCVAR_H_ */
