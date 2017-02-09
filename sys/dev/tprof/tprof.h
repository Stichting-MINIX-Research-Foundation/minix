/*	$NetBSD: tprof.h,v 1.5 2011/02/05 14:04:40 yamt Exp $	*/

/*-
 * Copyright (c)2008,2009,2010 YAMAMOTO Takashi,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DEV_TPROF_TPROF_H_
#define _DEV_TPROF_TPROF_H_

/*
 * definitions used by backend drivers
 */

#include <sys/types.h>

#include <dev/tprof/tprof_types.h>

typedef struct tprof_backend_cookie tprof_backend_cookie_t;

typedef struct tprof_backend_ops {
	uint64_t (*tbo_estimate_freq)(void);	/* samples per second */
	int (*tbo_start)(tprof_backend_cookie_t *);
	void (*tbo_stop)(tprof_backend_cookie_t *);
} tprof_backend_ops_t;

#define	TPROF_BACKEND_VERSION	3
int tprof_backend_register(const char *, const tprof_backend_ops_t *, int);
int tprof_backend_unregister(const char *);

typedef struct {
	uintptr_t tfi_pc;	/* program counter */
	bool tfi_inkernel;	/* if tfi_pc is in the kernel address space */
} tprof_frame_info_t;

void tprof_sample(tprof_backend_cookie_t *, const tprof_frame_info_t *);

#endif /* _DEV_TPROF_TPROF_H_ */
