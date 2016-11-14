/*	$NetBSD: subr_spldebug.c,v 1.3 2010/04/25 11:49:22 ad Exp $	*/

/*-
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Young.
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

/*
 * Interrupt priority level debugging code.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_spldebug.c,v 1.3 2010/04/25 11:49:22 ad Exp $");

#include <sys/param.h>
#include <sys/spldebug.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <machine/return.h>

#define SPLRAISE_STACKLEN 32

void *splraise_retaddrs[MAXCPUS][SPLRAISE_STACKLEN][4];
int splraise_depth[MAXCPUS] = {0};
int spllowered_to[MAXCPUS] = {0};
void *spllowered_from[MAXCPUS][2] = {{0}};
bool spldebug = false;

void	spldebug_start(void);
void	spldebug_stop(void);

void
spldebug_start(void)
{
	spldebug = true;
}

void
spldebug_stop(void)
{
	spldebug = false;
}

void
spldebug_lower(int ipl)
{
	u_int cidx;

	if (!spldebug)
		return;

	if (ipl == IPL_HIGH)
		return;

	cidx = cpu_index(curcpu());

	KASSERT(cidx < maxcpus);

	splraise_depth[cidx] = 0;
	spllowered_to[cidx] = ipl;
#if 0
	spllowered_from[cidx][0] = return_address(0);
	spllowered_from[cidx][1] = return_address(1);
#endif
}

void
spldebug_raise(int ipl)
{
	int i;
	u_int cidx;
	void **retaddrs;

	if (!spldebug)
		return;

	if (ipl != IPL_HIGH)
		return;

	cidx = cpu_index(curcpu());

	KASSERT(cidx < maxcpus);

	if (splraise_depth[cidx] >= SPLRAISE_STACKLEN)
		return;

	retaddrs = &splraise_retaddrs[cidx][splraise_depth[cidx]++][0];

	retaddrs[0] = retaddrs[1] = retaddrs[2] = retaddrs[3] = NULL;

	for (i = 0; i < 4; i++) {
		retaddrs[i] = return_address(i);

		if (retaddrs[i] == NULL)
			break;
	}
}
