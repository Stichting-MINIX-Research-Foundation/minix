/*	$NetBSD: uipc_syscalls_50.c,v 1.3 2011/01/19 10:21:16 tsutsui Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: uipc_syscalls_50.c,v 1.3 2011/01/19 10:21:16 tsutsui Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/msg.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/time.h>

#include <net/if.h>

#include <compat/sys/time.h>
#include <compat/sys/socket.h>
#include <compat/sys/sockio.h>

#ifdef COMPAT_OIFDATA
/*ARGSUSED*/
int
compat_ifdatareq(struct lwp *l, u_long cmd, void *data)
{
	struct oifdatareq *ifdr = data;
	struct ifnet *ifp;
	int error;

	ifp = ifunit(ifdr->ifdr_name);
	if (ifp == NULL)
		return ENXIO;

	switch (cmd) {
	case SIOCGIFDATA:
		ifdatan2o(&ifdr->ifdr_data, &ifp->if_data);
		return 0;

	case SIOCZIFDATA:
		if (l != NULL) {
			error = kauth_authorize_network(l->l_cred,
			    KAUTH_NETWORK_INTERFACE,
			    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp,
			    (void *)cmd, NULL);
			if (error != 0)
				return error;
		}
		ifdatan2o(&ifdr->ifdr_data, &ifp->if_data);
		/*
		 * Assumes that the volatile counters that can be
		 * zero'ed are at the end of if_data.
		 */
		memset(&ifp->if_data.ifi_ipackets, 0, sizeof(ifp->if_data) -
		    offsetof(struct if_data, ifi_ipackets));
		return 0;

	default:
		return EINVAL;
	}
}
#endif
