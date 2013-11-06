/*	$NetBSD: cctr.h,v 1.3 2008/04/28 20:24:10 martin Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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

#ifndef _SYS_CCTR_H_
#define	_SYS_CCTR_H_

#include <sys/timetc.h>

/*
 * Variables used by cycle counter in kern_cctr.c.
 */
struct cctr_state {
	volatile u_int   cc_gen;   /* generation number for this data set */
	volatile int64_t cc_val;   /* reference CC value at calibration time */
	volatile int64_t cc_cc;	   /* local CC value at calibration time */
	volatile int64_t cc_delta; /* reference CC difference for
				      last calibration period */
	volatile int64_t cc_denom; /* local CC difference for
				      last calibration period */
};

struct cpu_info;

void cc_calibrate_cpu(struct cpu_info *);
struct timecounter *cc_init(timecounter_get_t, uint64_t, const char *, int);
u_int cc_get_timecount(struct timecounter *);

#endif /* _SYS_CCTR_H_ */
