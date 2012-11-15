/*	$NetBSD: cpuset.c,v 1.18 2012/03/09 15:41:16 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef _STANDALONE
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: cpuset.c,v 1.18 2012/03/09 15:41:16 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/sched.h>
#ifdef _KERNEL
#include <sys/kmem.h>
#include <lib/libkern/libkern.h>
#include <sys/atomic.h>
#else
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#endif

#define	CPUSET_SHIFT	5
#define	CPUSET_MASK	31
#define CPUSET_NENTRIES(nc)	((nc) > 32 ? ((nc) >> CPUSET_SHIFT) : 1)
#ifndef __lint__
#define CPUSET_SIZE(n)	(sizeof( \
	struct {  \
		uint32_t bits[0]; \
	}) + sizeof(uint32_t) * (n))
#else
#define CPUSET_SIZE(n)	0
#endif

struct _cpuset {
	uint32_t	bits[0];
};

#ifndef _KERNEL

static size_t cpuset_size = 0;
static size_t cpuset_nentries = 0;

size_t
/*ARGSUSED*/
_cpuset_size(const cpuset_t *c)
{

	return cpuset_size;
}

void
_cpuset_zero(cpuset_t *c)
{

	memset(c->bits, 0, cpuset_nentries * sizeof(c->bits[0]));
}

int
_cpuset_isset(cpuid_t i, const cpuset_t *c)
{
	const unsigned long j = i >> CPUSET_SHIFT;

	if (j >= cpuset_nentries) {
		errno = EINVAL;
		return -1;
	}
	return ((1 << (unsigned int)(i & CPUSET_MASK)) & c->bits[j]) != 0;
}

int
_cpuset_set(cpuid_t i, cpuset_t *c)
{
	const unsigned long j = i >> CPUSET_SHIFT;

	if (j >= cpuset_nentries) {
		errno = EINVAL;
		return -1;
	}
	c->bits[j] |= 1 << (unsigned int)(i & CPUSET_MASK);
	return 0;
}

int
_cpuset_clr(cpuid_t i, cpuset_t *c)
{
	const unsigned long j = i >> CPUSET_SHIFT;

	if (j >= cpuset_nentries) {
		errno = EINVAL;
		return -1;
	}
	c->bits[j] &= ~(1 << (unsigned int)(i & CPUSET_MASK));
	return 0;
}

cpuset_t *
_cpuset_create(void)
{

	if (cpuset_size == 0) {
		static int mib[2] = { CTL_HW, HW_NCPU };
		size_t len;
		u_int nc;

		len = sizeof(nc);
		if (sysctl(mib, (unsigned int)__arraycount(mib), &nc, &len,
		    NULL, 0) == -1)
			return NULL;

		cpuset_nentries = CPUSET_NENTRIES(nc);
		cpuset_size = CPUSET_SIZE(cpuset_nentries);
	}
	return calloc(1, cpuset_size);
}

void
_cpuset_destroy(cpuset_t *c)
{

	free(c);
}

#endif
#endif
