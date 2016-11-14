/*	$NetBSD: netisr.c,v 1.9 2014/06/05 23:48:17 rmind Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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
__KERNEL_RCSID(0, "$NetBSD: netisr.c,v 1.9 2014/06/05 23:48:17 rmind Exp $");

#include <sys/param.h>
#include <sys/intr.h>

#include <net/netisr.h>

#include <rump/rumpuser.h>

#include "rump_net_private.h"

static void *netisrs[NETISR_MAX];

void
schednetisr(int isr)
{
	KASSERT(isr != NETISR_IP);
	KASSERT(isr != NETISR_IPV6);

	/*
	 * Do not schedule a softint that is not registered.
	 * This might cause the inq to fill, but the one calling us
	 * should start dropping packets once the inq is full,
	 * so no big harm done.
	 */
	if (__predict_true(netisrs[isr]))
		softint_schedule(netisrs[isr]);
}

void
rump_netisr_register(int level, void (*handler)(void))
{
	KASSERT(level != NETISR_IP);
	KASSERT(level != NETISR_IPV6);

	netisrs[level] = softint_establish(SOFTINT_NET | SOFTINT_MPSAFE,
	    (void (*)(void *))handler, NULL);
}
