/*	$NetBSD: tls.c,v 1.7 2013/08/19 22:14:37 matt Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: tls.c,v 1.7 2013/08/19 22:14:37 matt Exp $");

#include "namespace.h"

#define	_rtld_tls_allocate	__libc_rtld_tls_allocate
#define	_rtld_tls_free		__libc_rtld_tls_free

#include <sys/tls.h>

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)

#include <sys/param.h>
#include <sys/mman.h>
#include <link_elf.h>
#include <lwp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__dso_hidden void	__libc_static_tls_setup(void);

static const void *tls_initaddr;
static size_t tls_initsize;
static size_t tls_size;
static size_t tls_allocation;
static void *initial_thread_tcb;

void * __libc_tls_get_addr(void);

__weak_alias(__tls_get_addr, __libc_tls_get_addr)
#ifdef __i386__
__weak_alias(___tls_get_addr, __libc_tls_get_addr)
#endif

void *
__libc_tls_get_addr(void)
{

	abort();
	/* NOTREACHED */
}

__weak_alias(_rtld_tls_allocate, __libc_rtld_tls_allocate)

struct tls_tcb *
_rtld_tls_allocate(void)
{
	struct tls_tcb *tcb;
	uint8_t *p;

	if (initial_thread_tcb == NULL) {
#ifdef __HAVE_TLS_VARIANT_II
		tls_size = roundup2(tls_size, sizeof(void *));
#endif
		tls_allocation = tls_size + sizeof(*tcb);

		initial_thread_tcb = p = mmap(NULL, tls_allocation,
		    PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	} else {
		p = calloc(1, tls_allocation);
	}
	if (p == NULL) {
		static const char msg[] =  "TLS allocation failed, terminating\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(127);
	}
#ifdef __HAVE_TLS_VARIANT_I
	/* LINTED */
	tcb = (struct tls_tcb *)p;
	p += sizeof(struct tls_tcb);
#else
	/* LINTED tls_size is rounded above */
	tcb = (struct tls_tcb *)(p + tls_size);
	tcb->tcb_self = tcb;
#endif
	memcpy(p, tls_initaddr, tls_initsize);

	return tcb;
}

__weak_alias(_rtld_tls_free, __libc_rtld_tls_free)

void
_rtld_tls_free(struct tls_tcb *tcb)
{
	uint8_t *p;

#ifdef __HAVE_TLS_VARIANT_I
	/* LINTED */
	p = (uint8_t *)tcb;
#else
	/* LINTED */
	p = (uint8_t *)tcb - tls_size;
#endif
	if (p == initial_thread_tcb)
		munmap(p, tls_allocation);
	else
		free(p);
}

__weakref_visible int rtld_DYNAMIC __weak_reference(_DYNAMIC);

static int __section(".text.startup")
__libc_static_tls_setup_cb(struct dl_phdr_info *data, size_t len, void *cookie)
{
	const Elf_Phdr *phdr = data->dlpi_phdr;
	const Elf_Phdr *phlimit = data->dlpi_phdr + data->dlpi_phnum;

	for (; phdr < phlimit; ++phdr) {
		if (phdr->p_type != PT_TLS)
			continue;
		tls_initaddr = (void *)(phdr->p_vaddr + data->dlpi_addr);
		tls_initsize = phdr->p_filesz;
		tls_size = phdr->p_memsz;
	}
	return 0;
}

void
__libc_static_tls_setup(void)
{
	struct tls_tcb *tcb;

	if (&rtld_DYNAMIC != NULL) {
#ifdef __powerpc__
		/*
		 * Old powerpc crt0's are going to overwrite r2 so we need to
		 * restore it but only do so if the saved value isn't NULL (if
		 * it is NULL, ld.elf_so doesn't have the matching change).
		 */
		if ((tcb = _lwp_getprivate()) != NULL)
			__lwp_settcb(tcb);
#endif
		return;
	}

	dl_iterate_phdr(__libc_static_tls_setup_cb, NULL);

	tcb = _rtld_tls_allocate();
#ifdef __HAVE___LWP_SETTCB
	__lwp_settcb(tcb);
#else
	_lwp_setprivate(tcb);
#endif
}

#endif /* __HAVE_TLS_VARIANT_I || __HAVE_TLS_VARIANT_II */
