/*	$NetBSD: physmap.h,v 1.4 2013/04/08 01:33:53 uebayasi Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
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

#ifndef _SYS_PHYSMAP_H_
#define _SYS_PHYSMAP_H_

#if !defined(_KERNEL) && !defined(_KMEMUSER)
#error "not supposed to be exposed to userland"
#endif

#include <sys/types.h>
#include <sys/uio.h>
#include <uvm/uvm_extern.h>

typedef struct {
	paddr_t ps_addr;
	psize_t ps_len;
} physmap_segment_t;

/* typedef is in <sys/types.h> */
struct physmap {
	uint16_t pm_nsegs;
	uint16_t pm_maxsegs;
	physmap_segment_t pm_segs[0];
};

int	physmap_create_iov(physmap_t **, const struct vmspace *,
	    struct iovec *, size_t);
int	physmap_create_linear(physmap_t **, const struct vmspace *,
	    vaddr_t, vsize_t);
physmap_t *
	physmap_create_pagelist(struct vm_page **, size_t);

void	physmap_destroy(physmap_t *);

void *	physmap_map_init(physmap_t *, size_t, vm_prot_t);
size_t	physmap_map(void *, vaddr_t *);
void	physmap_map_fini(void *);

void	physmap_zero(physmap_t *, size_t, size_t);

#endif /* _SYS_PHYSMAP_H_ */
