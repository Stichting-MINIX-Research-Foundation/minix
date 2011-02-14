/*	$NetBSD: cpuset.c,v 1.16 2010/09/21 02:03:29 rmind Exp $	*/

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
__RCSID("$NetBSD: cpuset.c,v 1.16 2010/09/21 02:03:29 rmind Exp $");
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

#ifdef _KERNEL
struct _kcpuset {
	unsigned int	nused;
	struct _kcpuset *next;
	uint32_t	bits[0];
};
#define KCPUSET_SIZE(n)	(sizeof( \
	struct {  \
		unsigned int nused; \
		struct _kcpuset *next; \
		uint32_t bits[0]; \
	}) + sizeof(uint32_t) * (n))
#endif

static size_t cpuset_size = 0;
static size_t cpuset_nentries = 0;

#ifndef _KERNEL
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
	return ((1 << (i & CPUSET_MASK)) & c->bits[j]) != 0;
}

int
_cpuset_set(cpuid_t i, cpuset_t *c)
{
	const unsigned long j = i >> CPUSET_SHIFT;

	if (j >= cpuset_nentries) {
		errno = EINVAL;
		return -1;
	}
	c->bits[j] |= 1 << (i & CPUSET_MASK);
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
	c->bits[j] &= ~(1 << (i & CPUSET_MASK));
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
		if (sysctl(mib, __arraycount(mib), &nc, &len, NULL, 0) == -1)
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

#else

kcpuset_t *
kcpuset_create(void)
{
	kcpuset_t *c;

	if (cpuset_size == 0) {
		cpuset_nentries = CPUSET_NENTRIES(MAXCPUS);
		cpuset_size = KCPUSET_SIZE(cpuset_nentries);
	}
	c = kmem_zalloc(cpuset_size, KM_SLEEP);
	c->next = NULL;
	c->nused = 1;
	return c;
}

void
kcpuset_destroy(kcpuset_t *c)
{
	kcpuset_t *nc;

	while (c) {
		KASSERT(c->nused == 0);
		nc = c->next;
		kmem_free(c, cpuset_size);
		c = nc;
	}
}

void
kcpuset_copy(kcpuset_t *d, const kcpuset_t *s)
{

	KASSERT(d->nused == 1);
	memcpy(d->bits, s->bits, cpuset_nentries * sizeof(s->bits[0]));
}

void
kcpuset_use(kcpuset_t *c)
{

	atomic_inc_uint(&c->nused);
}

void
kcpuset_unuse(kcpuset_t *c, kcpuset_t **lst)
{

	if (atomic_dec_uint_nv(&c->nused) != 0)
		return;
	KASSERT(c->nused == 0);
	KASSERT(c->next == NULL);
	if (lst == NULL) {
		kcpuset_destroy(c);
		return;
	}
	c->next = *lst;
	*lst = c;
}

int
kcpuset_copyin(const cpuset_t *u, kcpuset_t *k, size_t len)
{

	KASSERT(k->nused > 0);
	KASSERT(k->next == NULL);
	if (len != CPUSET_SIZE(cpuset_nentries))
		return EINVAL;
	return copyin(u->bits, k->bits, cpuset_nentries * sizeof(k->bits[0]));
}

int
kcpuset_copyout(const kcpuset_t *k, cpuset_t *u, size_t len)
{

	KASSERT(k->nused > 0);
	KASSERT(k->next == NULL);
	if (len != CPUSET_SIZE(cpuset_nentries))
		return EINVAL;
	return copyout(k->bits, u->bits, cpuset_nentries * sizeof(u->bits[0]));
}

void
kcpuset_zero(kcpuset_t *c)
{

	KASSERT(c->nused > 0);
	KASSERT(c->next == NULL);
	memset(c->bits, 0, cpuset_nentries * sizeof(c->bits[0]));
}

void
kcpuset_fill(kcpuset_t *c)
{

	KASSERT(c->nused > 0);
	KASSERT(c->next == NULL);
	memset(c->bits, ~0, cpuset_nentries * sizeof(c->bits[0]));
}

void
kcpuset_set(cpuid_t i, kcpuset_t *c)
{
	const unsigned long j = i >> CPUSET_SHIFT;

	KASSERT(c->next == NULL);
	KASSERT(j < cpuset_nentries);
	c->bits[j] |= 1 << (i & CPUSET_MASK);
}

int
kcpuset_isset(cpuid_t i, const kcpuset_t *c)
{
	const unsigned long j = i >> CPUSET_SHIFT;

	KASSERT(c != NULL);
	KASSERT(c->nused > 0);
	KASSERT(c->next == NULL);
	KASSERT(j < cpuset_nentries);
	return ((1 << (i & CPUSET_MASK)) & c->bits[j]) != 0;
}

bool
kcpuset_iszero(const kcpuset_t *c)
{
	unsigned long j;

	for (j = 0; j < cpuset_nentries; j++)
		if (c->bits[j] != 0)
			return false;
	return true;
}

bool
kcpuset_match(const kcpuset_t *c1, const kcpuset_t *c2)
{
	unsigned long j;

	for (j = 0; j < cpuset_nentries; j++)
		if ((c1->bits[j] & c2->bits[j]) != c2->bits[j])
			return false;
	return true;
}

#endif
#endif
