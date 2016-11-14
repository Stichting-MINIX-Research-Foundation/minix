/*	$NetBSD: rump_x86_cpu_counter.c,v 1.1 2014/02/12 22:28:43 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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

/*
 * __HAVE_CPU_COUNTER stubs.  It would be nice to have this
 * MI, but need MD for now because of inline-happy archs.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rump_x86_cpu_counter.c,v 1.1 2014/02/12 22:28:43 pooka Exp $");

#include <sys/param.h>

#include <x86/cpu_counter.h>

int
cpu_hascounter(void)
{

	return 0;
}

uint64_t
cpu_counter(void)
{

	return 0;
}

uint32_t
cpu_counter32(void)
{

	return 0;
}

uint64_t
cpu_frequency(struct cpu_info *ci)
{

	return 0;
}
