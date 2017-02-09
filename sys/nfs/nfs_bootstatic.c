/*	$NetBSD: nfs_bootstatic.c,v 1.8 2009/10/23 02:32:34 snj Exp $	*/

/*
 *
 * Copyright (c) 2004 Christian Limpach.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_bootstatic.c,v 1.8 2009/10/23 02:32:34 snj Exp $");

#ifdef _KERNEL_OPT
#include "opt_nfs_boot.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/if_inarp.h>

#include <nfs/rpcv2.h>

#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsdiskless.h>

int (*nfs_bootstatic_callback)(struct nfs_diskless *) = NULL;

int
nfs_bootstatic(struct nfs_diskless *nd, struct lwp *lwp, int *_flags)
{
	struct ifnet *ifp = nd->nd_ifp;
	struct sockaddr_in *sin;
	int error, flags;

	if (nfs_bootstatic_callback)
		flags = (*nfs_bootstatic_callback)(nd);
	else
		flags = 0;

	if (flags & NFS_BOOT_NOSTATIC)
		return EOPNOTSUPP;

	if (flags == 0) {
#ifdef NFS_BOOTSTATIC_MYIP
		if (!(*_flags & NFS_BOOT_HAS_MYIP)) {
			nd->nd_myip.s_addr = inet_addr(NFS_BOOTSTATIC_MYIP);
			flags |= NFS_BOOT_HAS_MYIP;
		}
#endif
#ifdef NFS_BOOTSTATIC_GWIP
		if (!(*_flags & NFS_BOOT_HAS_GWIP)) {
			nd->nd_gwip.s_addr = inet_addr(NFS_BOOTSTATIC_GWIP);
			flags |= NFS_BOOT_HAS_GWIP;
		}
#endif
#ifdef NFS_BOOTSTATIC_MASK
		if (!(*_flags & NFS_BOOT_HAS_MASK)) {
			nd->nd_mask.s_addr = inet_addr(NFS_BOOTSTATIC_MASK);
			flags |= NFS_BOOT_HAS_MASK;
		}
#endif
#ifdef NFS_BOOTSTATIC_SERVADDR
		if (!(*_flags & NFS_BOOT_HAS_SERVADDR)) {
			sin = (struct sockaddr_in *) &nd->nd_root.ndm_saddr;
			memset((void *)sin, 0, sizeof(*sin));
			sin->sin_len = sizeof(*sin);
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = inet_addr(NFS_BOOTSTATIC_SERVADDR);
			flags |= NFS_BOOT_HAS_SERVADDR;
		}
#endif
#ifdef NFS_BOOTSTATIC_SERVER
		if (!(*_flags & NFS_BOOT_HAS_SERVER)) {
			strncpy(nd->nd_root.ndm_host, NFS_BOOTSTATIC_SERVER,
				MNAMELEN);
			flags |= NFS_BOOT_HAS_SERVER;
			if (strchr(nd->nd_root.ndm_host, ':') != NULL)
				flags |= NFS_BOOT_HAS_ROOTPATH;
		}
#endif
	}

	*_flags |= flags;

	if (!(*_flags & NFS_BOOT_HAS_SERVADDR) && (*_flags & NFS_BOOT_HAS_SERVER)) {
		char rootserver[MNAMELEN];
		char *sep;

		sep = strchr(nd->nd_root.ndm_host, ':');
		strlcpy(rootserver, nd->nd_root.ndm_host,
			sep - nd->nd_root.ndm_host+1);

		sin = (struct sockaddr_in *) &nd->nd_root.ndm_saddr;
		memset((void *)sin, 0, sizeof(*sin));
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = inet_addr(rootserver);
		flags |= NFS_BOOT_HAS_SERVADDR;
		*_flags |= NFS_BOOT_HAS_SERVADDR;
	}

	if (flags & NFS_BOOT_HAS_MYIP)
		aprint_normal("nfs_boot: client_addr=%s\n",
		    inet_ntoa(nd->nd_myip));

	if (flags & NFS_BOOT_HAS_GWIP)
		aprint_normal("nfs_boot: gateway=%s\n",
		    inet_ntoa(nd->nd_gwip));

	if (flags & NFS_BOOT_HAS_MASK)
		aprint_normal("nfs_boot: netmask=%s\n",
		    inet_ntoa(nd->nd_mask));

	if (flags & NFS_BOOT_HAS_SERVADDR) {
		sin = (struct sockaddr_in *) &nd->nd_root.ndm_saddr;
		aprint_normal("nfs_boot: server=%s\n",
		    inet_ntoa(sin->sin_addr));
	}

	if (flags & NFS_BOOT_HAS_SERVER)
		aprint_normal("nfs_boot: root=%.*s\n", MNAMELEN,
		    nd->nd_root.ndm_host);

	/*
	 * Do enough of ifconfig(8) so that the chosen interface
	 * can talk to the servers.
	 */
	error = nfs_boot_setaddress(ifp, lwp,
	    *_flags & NFS_BOOT_HAS_MYIP ? nd->nd_myip.s_addr : INADDR_ANY,
	    *_flags & NFS_BOOT_HAS_MASK ? nd->nd_mask.s_addr : INADDR_ANY,
	    INADDR_ANY);
	if (error) {
		aprint_error("nfs_boot: set ifaddr, error=%d\n", error);
		goto out;
	}

	if ((*_flags & NFS_BOOT_ALLINFO) != NFS_BOOT_ALLINFO)
		return EADDRNOTAVAIL;

	error = 0;

 out:

	return error;
}
