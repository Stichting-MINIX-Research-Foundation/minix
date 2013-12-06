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
#include <sys/cdefs.h>
__RCSID("$NetBSD: crtbegin.c,v 1.6 2013/11/29 23:00:48 joerg Exp $");

#include "crtbegin.h"

typedef void (*fptr_t)(void);

__dso_hidden const fptr_t __JCR_LIST__[0] __section(".jcr");

__weakref_visible void Jv_RegisterClasses(const fptr_t *)
	__weak_reference(_Jv_RegisterClasses);

#if !defined(HAVE_INITFINI_ARRAY)
__dso_hidden const fptr_t __CTOR_LIST__[] __section(".ctors") = {
	(fptr_t) -1,
};
__dso_hidden extern const fptr_t __CTOR_LIST_END__[];
#endif

#ifdef SHARED
__dso_hidden void *__dso_handle = &__dso_handle;

__weakref_visible void cxa_finalize(void *)
	__weak_reference(__cxa_finalize);
#else
__dso_hidden void *__dso_handle;
#endif

#if !defined(__ARM_EABI__)
__dso_hidden
#if !defined(__mips__)
	const
#endif
	long __EH_FRAME_LIST__[0] __section(".eh_frame");

__weakref_visible void register_frame_info(const void *, const void *)
	__weak_reference(__register_frame_info);
__weakref_visible void deregister_frame_info(const void *)
	__weak_reference(__deregister_frame_info);

static long dwarf_eh_object[8];
#endif

static void __do_global_ctors_aux(void) __used;

static void __section(".text.startup")
__do_global_ctors_aux(void)
{
	static unsigned char __initialized;

	if (__initialized)
		return;

	__initialized = 1;

#if !defined(__ARM_EABI__)
	if (register_frame_info)
		register_frame_info(__EH_FRAME_LIST__, &dwarf_eh_object);
#endif

	if (Jv_RegisterClasses && __JCR_LIST__[0] != 0)
		Jv_RegisterClasses(__JCR_LIST__);

#if !defined(HAVE_INITFINI_ARRAY)
	for (const fptr_t *p = __CTOR_LIST_END__; p > __CTOR_LIST__ + 1; ) {
		(*(*--p))();
	}
#endif
}

#if !defined(__ARM_EABI__) || defined(SHARED)
#if !defined(HAVE_INITFINI_ARRAY)
__dso_hidden const fptr_t __DTOR_LIST__[] __section(".dtors") = {
	(fptr_t) -1,
};
__dso_hidden extern const fptr_t __DTOR_LIST_END__[];
#endif

static void __do_global_dtors_aux(void) __used;

static void __section(".text.exit")
__do_global_dtors_aux(void)
{
	static unsigned char __finished;

	if (__finished)
		return;

	__finished = 1;

#ifdef SHARED
	if (cxa_finalize)
		(*cxa_finalize)(__dso_handle);
#endif

#if !defined(HAVE_INITFINI_ARRAY)
	for (const fptr_t *p = __DTOR_LIST__ + 1; p < __DTOR_LIST_END__; ) {
		(*(*p++))();
	}
#endif

#if !defined(__ARM_EABI__)
	if (deregister_frame_info)
		deregister_frame_info(__EH_FRAME_LIST__);
#endif
}
#endif /* !__ARM_EABI__ || SHARED */
