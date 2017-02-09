/*	$NetBSD: acpi_func.h,v 1.4 2010/07/24 21:53:54 jruoho Exp $	*/

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2007-2009 Jung-uk Kim <jkim@FreeBSD.org>
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

#ifndef _SYS_DEV_ACPI_ACPICA_ACPI_FUNC_H
#define _SYS_DEV_ACPI_ACPICA_ACPI_FUNC_H

#include <machine/cpufunc.h>

#include <sys/atomic.h>

#define GL_ACQUIRED	(-1)
#define GL_BUSY		0
#define GL_BIT_PENDING	1
#define GL_BIT_OWNED	2
#define GL_BIT_MASK	(GL_BIT_PENDING | GL_BIT_OWNED)

#define ACPI_ACQUIRE_GLOBAL_LOCK(GLptr, Acq) 				\
do { 									\
	(Acq) = acpi_acquire_global_lock(&((GLptr)->GlobalLock));	\
} while (0)

#define ACPI_RELEASE_GLOBAL_LOCK(GLptr, Acq) 				\
do {									\
	(Acq) = acpi_release_global_lock(&((GLptr)->GlobalLock));	\
} while (0)

static inline int
acpi_acquire_global_lock(uint32_t *lock)
{
	uint32_t new, old, val;

	do {
		old = *lock;
		new = ((old & ~GL_BIT_MASK) | GL_BIT_OWNED) |
		    ((old >> 1) & GL_BIT_PENDING);
		val = atomic_cas_32(lock, old, new);
	} while (__predict_false(val != old));

	return ((new < GL_BIT_MASK) ? GL_ACQUIRED : GL_BUSY);
}

static inline int
acpi_release_global_lock(uint32_t *lock)
{
	uint32_t new, old, val;

	do {
		old = *lock;
		new = old & ~GL_BIT_MASK;
		val = atomic_cas_32(lock, old, new);
	} while (__predict_false(val != old));

	return old & GL_BIT_PENDING;
}

/*
 * XXX: Should be in a MD header.
 */
#ifndef __ia64__
#define	ACPI_FLUSH_CPU_CACHE()	wbinvd()
#else
#define ACPI_FLUSH_CPU_CACHE()	/* XXX: ia64_fc()? */
#endif

#endif /* !_SYS_DEV_ACPI_ACPICA_ACPI_FUNC_H */
