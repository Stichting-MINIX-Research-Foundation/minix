/*	$NetBSD: kern_ssp.c,v 1.6 2011/11/19 22:51:25 tls Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: kern_ssp.c,v 1.6 2011/11/19 22:51:25 tls Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/intr.h>
#include <sys/cprng.h>

#if defined(__SSP__) || defined(__SSP_ALL__)
long __stack_chk_guard[8] = {0, 0, 0, 0, 0, 0, 0, 0};
void __stack_chk_fail(void);

void
__stack_chk_fail(void)
{
	panic("stack overflow detected; terminated");
}

void
ssp_init(void)
{
	int s;

	aprint_debug("Initializing SSP: ");
	/*
	 * We initialize ssp here carefully:
	 *	1. after we got some entropy
	 *	2. without calling a function
	 */
	size_t i;
	long guard[__arraycount(__stack_chk_guard)];

	cprng_fast(guard, sizeof(guard));
	s = splhigh();
	for (i = 0; i < __arraycount(guard); i++)
		__stack_chk_guard[i] = guard[i];
	splx(s);
	for (i = 0; i < __arraycount(guard); i++)
		aprint_debug("%lx ", guard[i]);
	aprint_debug("\n");
}
#else
void
ssp_init(void)
{
}
#endif
