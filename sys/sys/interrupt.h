/*	$NetBSD: interrupt.h,v 1.2 2015/08/17 18:43:37 macallan Exp $	*/

/*
 * Copyright (c) 2015 Internet Initiative Japan Inc.
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

#ifndef _SYS_INTERRUPT_H_
#define _SYS_INTERRUPT_H_

#include <sys/types.h>
#include <sys/intr.h>
#include <sys/kcpuset.h>

typedef char intrid_t[INTRIDBUF];
struct intrids_handler {
	int iih_nids;
	intrid_t iih_intrids[1];
	/*
	 * The number of the "iih_intrids" array will be overwritten by
	 * "iih_nids" after intr_construct_intrids().
	 */
};

uint64_t	interrupt_get_count(const char *, u_int);
void		interrupt_get_assigned(const char *, kcpuset_t *);
void		interrupt_get_available(kcpuset_t *);
void		interrupt_get_devname(const char *, char *, size_t);
struct intrids_handler	*interrupt_construct_intrids(const kcpuset_t *);
void		interrupt_destruct_intrids(struct intrids_handler *);
int		interrupt_distribute(void *, const kcpuset_t *, kcpuset_t *);
int		interrupt_distribute_handler(const char *, const kcpuset_t *,
		    kcpuset_t *);

#endif /* !_SYS_INTERRUPT_H_ */
