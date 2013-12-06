/* 	$NetBSD: initfini.c,v 1.11 2013/08/19 22:14:37 matt Exp $	 */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: initfini.c,v 1.11 2013/08/19 22:14:37 matt Exp $");

#ifdef _LIBC
#include "namespace.h"
#endif

#include <sys/param.h>
#include <sys/exec.h>
#if !defined(__minix)
#include <sys/tls.h>
#endif /* !defined(__minix) */
#include <stdbool.h>

void	_libc_init(void) __attribute__((__constructor__, __used__));

void	__guard_setup(void);
void	__libc_thr_init(void);
void	__libc_atomic_init(void);
void	__libc_atexit_init(void);
void	__libc_env_init(void);

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
__dso_hidden void	__libc_static_tls_setup(void);
#endif

#ifdef __weak_alias
__weak_alias(_dlauxinfo,___dlauxinfo)
static void *__libc_dlauxinfo;

void *___dlauxinfo(void) __pure;

void *
___dlauxinfo(void)
{
	return __libc_dlauxinfo;
}
#endif

static bool libc_initialised;

void _libc_init(void);

/*
 * Declare as common symbol to allow new libc with older binaries to
 * not trigger an undefined reference.
 */
struct ps_strings *__ps_strings;

/*
 * _libc_init is called twice.  The first time explicitly by crt0.o
 * (for newer versions) and the second time as indirectly via _init().
 */
void __section(".text.startup")
_libc_init(void)
{

	if (libc_initialised)
		return;

	libc_initialised = 1;

	if (__ps_strings != NULL)
		__libc_dlauxinfo = __ps_strings->ps_argvstr +
		    __ps_strings->ps_nargvstr + __ps_strings->ps_nenvstr + 2;

	/* For -fstack-protector */
	__guard_setup();

#ifdef _REENTRANT
	/* Atomic operations */
	__libc_atomic_init();
#endif

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	/* Initialize TLS for statically linked programs. */
	__libc_static_tls_setup();
#endif

#ifdef _REENTRANT
	/* Threads */
	__libc_thr_init();
#endif

	/* Initialize the atexit mutexes */
	__libc_atexit_init();

	/* Initialize environment memory RB tree. */
	__libc_env_init();
}
