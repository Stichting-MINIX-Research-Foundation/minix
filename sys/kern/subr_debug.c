/*	$NetBSD: subr_debug.c,v 1.7 2008/04/30 20:20:53 ad Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*
 * Shared support code for kernels built with the DEBUG option.
 */
 
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_debug.c,v 1.7 2008/04/30 20:20:53 ad Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/atomic.h>
#include <sys/cpu.h>

#include <uvm/uvm_extern.h>

#include <machine/lock.h>

/*
 * Allocation/free validation by pointer address.  Introduces
 * significant overhead and is not enabled by default.  Patch
 * `debug_freecheck' to 1 at boot time to enable.
 */
#define	FREECHECK_BYTES		(8*1024*1024)

typedef struct fcitem {
	void		*i_addr;
	struct fcitem	*i_next;
} fcitem_t;

fcitem_t		*freecheck_free;
__cpu_simple_lock_t	freecheck_lock;
u_int			debug_freecheck;

void
debug_init(void)
{
	size_t cnt;
	fcitem_t *i;

	__cpu_simple_lock_init(&freecheck_lock);

	if (debug_freecheck) {
		i = (fcitem_t *)uvm_km_alloc(kernel_map, FREECHECK_BYTES, 0,
		    UVM_KMF_WIRED);
		if (i == NULL) {
			printf("freecheck_init: unable to allocate memory");
			return;
		}

		for (cnt = FREECHECK_BYTES / sizeof(*i); cnt != 0; cnt--) {
			i->i_next = freecheck_free;
			freecheck_free = i++;
		}
	}
}

void
freecheck_out(void **head, void *addr)
{
	fcitem_t *i;
	int s;

	if (!debug_freecheck)
		return;

	s = splvm();
	__cpu_simple_lock(&freecheck_lock);
	for (i = *head; i != NULL; i = i->i_next) {
		if (i->i_addr != addr)
			continue;
		__cpu_simple_unlock(&freecheck_lock);
		splx(s);
		panic("freecheck_out: %p already out", addr);
	}
	if ((i = freecheck_free) != NULL) {
		freecheck_free = i->i_next;
		i->i_addr = addr;
		i->i_next = *head;
		*head = i;
	}
	__cpu_simple_unlock(&freecheck_lock);
	splx(s);

	if (i == NULL) {
		if (atomic_swap_uint(&debug_freecheck, 1) == 0)
			printf("freecheck_out: no more slots\n");
	}
}

void
freecheck_in(void **head, void *addr)
{
	fcitem_t *i;
	void *pp;
	int s;

	if (!debug_freecheck)
		return;

	s = splvm();
	__cpu_simple_lock(&freecheck_lock);
	for (i = *head, pp = head; i != NULL; pp = &i->i_next, i = i->i_next) {
		if (i->i_addr == addr) {
			*(fcitem_t **)pp = i->i_next;
			i->i_next = freecheck_free;
			freecheck_free = i;
			break;
		}
	}
	__cpu_simple_unlock(&freecheck_lock);
	splx(s);

	if (i != NULL)
		return;

#ifdef DDB
	printf("freecheck_in: %p not out\n", addr);
	Debugger();
#else
	panic("freecheck_in: %p not out", addr);
#endif
}
