/*	$NetBSD: rump_x86_cpu.c,v 1.3 2015/04/22 17:38:33 pooka Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rump_x86_cpu.c,v 1.3 2015/04/22 17:38:33 pooka Exp $");

#include <sys/param.h>
#include <sys/kmem.h>

#include <machine/cpu.h>

#include "rump_private.h"
#include "rump_curlwp.h"

struct cpu_info *cpu_info_list;

void
rump_cpu_attach(struct cpu_info *ci)
{

	if (cpu_info_list == NULL)
		ci->ci_flags |= CPUF_PRIMARY;

	/* XXX: wrong order, but ... */
	ci->ci_next = cpu_info_list;
	cpu_info_list = ci;

	kcpuset_set(kcpuset_attached, cpu_index(ci));
	kcpuset_set(kcpuset_running, cpu_index(ci));
}

struct cpu_info *
x86_curcpu()
{

	return curlwp->l_cpu;
}

struct lwp *
x86_curlwp()
{

	return rump_curlwp_fast();
}

void
wbinvd(void)
{

	/*
	 * Used by kobj_machdep().
	 *
	 * But, we Best not execute this since we're not Ring0 *.
	 * Honestly, I don't know why it's required even in the kernel.
	 */
}

struct clockframe *
rump_cpu_makeclockframe(void)
{

	return kmem_zalloc(sizeof(struct clockframe), KM_SLEEP);
}
