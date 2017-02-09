/*	$NetBSD: net_component.c,v 1.4 2015/08/31 08:02:45 ozaki-r Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by The Nokia Foundation
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
__KERNEL_RCSID(0, "$NetBSD: net_component.c,v 1.4 2015/08/31 08:02:45 ozaki-r Exp $");

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/if_llatbl.h>
#include <net/route.h>

#include "rump_private.h"
#include "rump_net_private.h"

RUMP_COMPONENT(RUMP_COMPONENT_NET)
{

	ifinit1();
	ifinit();
	lltableinit();
}

RUMP_COMPONENT(RUMP_COMPONENT_NET_ROUTE)
{
	extern struct domain routedomain, linkdomain;
#ifdef COMPAT_50
	extern struct domain compat_50_routedomain;
#endif

	domain_attach(&linkdomain);
	domain_attach(&routedomain);
#ifdef COMPAT_50
	domain_attach(&compat_50_routedomain);
#endif
}

RUMP_COMPONENT(RUMP_COMPONENT_NET_IF)
{

	loopattach(1);
}
