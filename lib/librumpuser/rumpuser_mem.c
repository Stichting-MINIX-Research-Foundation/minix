/*	$NetBSD: rumpuser_mem.c,v 1.2 2014/08/24 14:37:31 pooka Exp $	*/

/*
 * Copyright (c) 2007-2010 Antti Kantee.  All Rights Reserved.
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

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpuser_mem.c,v 1.2 2014/08/24 14:37:31 pooka Exp $");
#endif /* !lint */

#include <sys/mman.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <rump/rumpuser.h>

#include "rumpuser_int.h"

int
rumpuser_malloc(size_t howmuch, int alignment, void **memp)
{
	void *mem = NULL;
	int rv;

	if (alignment == 0)
		alignment = sizeof(void *);

	rv = posix_memalign(&mem, (size_t)alignment, howmuch);
	if (__predict_false(rv != 0)) {
		if (rv == EINVAL) {
			printf("rumpuser_malloc: invalid alignment %d\n",
			    alignment);
			abort();
		}
	}

	*memp = mem;
	ET(rv);
}

/*ARGSUSED1*/
void
rumpuser_free(void *ptr, size_t size)
{

	free(ptr);
}

int
rumpuser_anonmmap(void *prefaddr, size_t size, int alignbit,
	int exec, void **memp)
{
	void *mem;
	int prot, rv;

#ifndef MAP_ALIGNED
#define MAP_ALIGNED(a) 0
	if (alignbit)
		fprintf(stderr, "rumpuser_anonmmap: warning, requested "
		    "alignment not supported by hypervisor\n");
#endif

	prot = PROT_READ|PROT_WRITE;
	if (exec)
		prot |= PROT_EXEC;
	mem = mmap(prefaddr, size, prot,
	    MAP_PRIVATE | MAP_ANON | MAP_ALIGNED(alignbit), -1, 0);
	if (mem == MAP_FAILED) {
		rv = errno;
	} else {
		*memp = mem;
		rv = 0;
	}

	ET(rv);
}

void
rumpuser_unmap(void *addr, size_t len)
{

	munmap(addr, len);
}

