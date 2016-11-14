/*	$NetBSD: kern_sdt.c,v 1.2 2015/10/02 16:54:15 christos Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by CoyotePoint Systems, Inc. It was developed under contract to 
 * CoyotePoint by Darran Hunt.
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

/*-
 * Copyright 2006-2008 John Birrell <jb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/kern/kern_sdt.c,v 1.1.4.1 2009/08/03 08:13:06 kensmith Exp $
 *
 * Backend for the Statically Defined Tracing (SDT) kernel support. This is
 * required to allow a module to load even though DTrace kernel support may
 * not be present. A module may be built with SDT probes in it.
 *
 */
#ifdef _KERNEL_OPT
#include "opt_dtrace.h"
#endif

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sdt.h>

void sdt_probe_stub(u_int32_t, uintptr_t, uintptr_t,
	uintptr_t, uintptr_t, uintptr_t);

__link_set_decl(sdt_providers_set, struct sdt_provider);
__link_set_decl(sdt_probes_set, struct sdt_probe);
__link_set_decl(sdt_argtypes_set, struct sdt_argtype);

/*
 * Hook for the DTrace probe function. The 'sdt' provider will set this
 * to dtrace_probe when it loads.
 */
sdt_probe_func_t sdt_probe_func = sdt_probe_stub;

/*
 * This is a stub for probe calls in case kernel DTrace support isn't
 * compiled in. It should never get called because there is no DTrace
 * support to enable it.
 */
void
sdt_probe_stub(u_int32_t id, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
	struct sdt_provider * const * provider;
	struct sdt_probe * const * probe;
	struct sdt_argtype * const * argtype;
	printf("%s: XXX should not be called\n", __func__);
	printf("providers: ");
	__link_set_foreach(provider, sdt_providers_set)
		printf("%s ", (*provider)->name);
	printf("\nprobes: ");
	__link_set_foreach(probe, sdt_probes_set)
		printf("%s ", (*probe)->name);
	printf("\nargtypes: ");
	__link_set_foreach(argtype, sdt_argtypes_set)
		printf("%s ", (*argtype)->type);
	printf("\n");
}

/*
 * initialize the SDT dtrace probe function
 */
void
sdt_init(void *dtrace_probe)
{

	sdt_probe_func = dtrace_probe;
}

/*
 * Disable the SDT dtrace probe function
 */
void
sdt_exit(void)
{

	sdt_probe_func = sdt_probe_stub;
}
