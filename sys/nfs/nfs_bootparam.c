/*	$NetBSD: nfs_bootparam.c,v 1.38 2013/09/12 18:00:18 drochner Exp $	*/

/*-
 * Copyright (c) 1995, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 * Support for NFS diskless booting, Sun-style (RPC/bootparams)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_bootparam.c,v 1.38 2013/09/12 18:00:18 drochner Exp $");

#ifdef _KERNEL_OPT
#include "opt_nfs_boot.h"
#include "arp.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/vnode.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/if_inarp.h>

#include <nfs/rpcv2.h>
#include <nfs/krpc.h>
#include <nfs/xdr_subs.h>

#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsdiskless.h>
#include <nfs/nfs_var.h>

/*
 * There are two implementations of NFS diskless boot.
 * This implementation uses Sun RPC/bootparams, and the
 * the other uses BOOTP (RFC951 - see nfs_bootdhcp.c).
 *
 * The Sun-style boot sequence goes as follows:
 * (1) Use RARP to get our interface address
 * (2) Use RPC/bootparam/whoami to get our hostname,
 *     our IP address, and the server's IP address.
 * (3) Use RPC/bootparam/getfile to get the root path
 * (4) Use RPC/mountd to get the root file handle
 * (5) Use RPC/bootparam/getfile to get the swap path
 * (6) Use RPC/mountd to get the swap file handle
 */

/* bootparam RPC */
static int bp_whoami (struct sockaddr_in *bpsin,
	struct in_addr *my_ip, struct in_addr *gw_ip, struct lwp *l);
static int bp_getfile (struct sockaddr_in *bpsin, const char *key,
	struct nfs_dlmount *ndm, struct lwp *l);


/*
 * Get client name, gateway address, then
 * get root and swap server:pathname info.
 * RPCs: bootparam/whoami, bootparam/getfile
 *
 * Use the old broadcast address for the WHOAMI
 * call because we do not yet know our netmask.
 * The server address returned by the WHOAMI call
 * is used for all subsequent bootparam RPCs.
 */
int
nfs_bootparam(struct nfs_diskless *nd, struct lwp *lwp, int *flags)
{
	struct ifnet *ifp = nd->nd_ifp;
	struct in_addr my_ip, arps_ip, gw_ip;
	struct sockaddr_in bp_sin;
	struct sockaddr_in *sin;
#ifndef NFS_BOOTPARAM_NOGATEWAY
	struct nfs_dlmount *gw_ndm = 0;
	char *p;
	u_int32_t mask;
#endif
	int error;

	/*
	 * Bring up the interface. (just set the "up" flag)
	 */
	error = nfs_boot_ifupdown(ifp, lwp, 1);
	if (error) {
		printf("nfs_boot: SIFFLAGS, error=%d\n", error);
		return (error);
	}

	error = EADDRNOTAVAIL;
#if NARP > 0
	if (ifp->if_type == IFT_ETHER || ifp->if_type == IFT_FDDI) {
		/*
		 * Do RARP for the interface address.
		 */
		error = revarpwhoarewe(ifp, &arps_ip, &my_ip);
	}
#endif
	if (error) {
		printf("revarp failed, error=%d\n", error);
		goto out;
	}

	if (!(*flags & NFS_BOOT_HAS_MYIP)) {
		nd->nd_myip.s_addr = my_ip.s_addr;
		printf("nfs_boot: client_addr=%s", inet_ntoa(my_ip));
		printf(" (RARP from %s)\n", inet_ntoa(arps_ip));
		*flags |= NFS_BOOT_HAS_MYIP;
	}

	/*
	 * Do enough of ifconfig(8) so that the chosen interface
	 * can talk to the servers.  (just set the address)
	 */
	error = nfs_boot_setaddress(ifp, lwp, my_ip.s_addr,
				    INADDR_ANY, INADDR_ANY);
	if (error) {
		printf("nfs_boot: set ifaddr, error=%d\n", error);
		goto out;
	}

	/*
	 * Get client name and gateway address.
	 * RPC: bootparam/whoami
	 * Use the old broadcast address for the WHOAMI
	 * call because we do not yet know our netmask.
	 * The server address returned by the WHOAMI call
	 * is used for all subsequent booptaram RPCs.
	 */
	sin = &bp_sin;
	memset((void *)sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_BROADCAST;

	/* Do the RPC/bootparam/whoami. */
	error = bp_whoami(sin, &my_ip, &gw_ip, lwp);
	if (error) {
		printf("nfs_boot: bootparam whoami, error=%d\n", error);
		goto delout;
	}
	*flags |= NFS_BOOT_HAS_SERVADDR | NFS_BOOT_HAS_SERVER;
	printf("nfs_boot: server_addr=%s\n", inet_ntoa(sin->sin_addr));
	printf("nfs_boot: hostname=%s\n", hostname);

	/*
	 * Now fetch the server:pathname strings and server IP
	 * for root and swap.  Missing swap is not fatal.
	 */
	error = bp_getfile(sin, "root", &nd->nd_root, lwp);
	if (error) {
		printf("nfs_boot: bootparam get root: %d\n", error);
		goto delout;
	}
	*flags |= NFS_BOOT_HAS_ROOTPATH;

#ifndef NFS_BOOTPARAM_NOGATEWAY
	gw_ndm = kmem_alloc(sizeof(*gw_ndm), KM_SLEEP);
	memset((void *)gw_ndm, 0, sizeof(*gw_ndm));
	error = bp_getfile(sin, "gateway", gw_ndm, lwp);
	if (error) {
		/* No gateway supplied. No error, but try fallback. */
		error = 0;
		goto nogwrepl;
	}
	sin = (struct sockaddr_in *) &gw_ndm->ndm_saddr;
	if (sin->sin_addr.s_addr == 0)
		goto out;	/* no gateway */

	/* OK, we have a gateway! */
	printf("nfs_boot: gateway=%s\n", inet_ntoa(sin->sin_addr));
	/* Just save it.  Caller adds the route. */
	nd->nd_gwip = sin->sin_addr;
	*flags |= NFS_BOOT_HAS_GWIP;

	/* Look for a mask string after the colon. */
	p = strchr(gw_ndm->ndm_host, ':');
	if (p == 0)
		goto out;	/* no netmask */
	/* have pathname */
	p++;	/* skip ':' */
	mask = inet_addr(p);	/* libkern */
	if (mask == 0)
		goto out;	/* no netmask */

	/* Have a netmask too!  Save it; update the I/F. */
	nd->nd_mask.s_addr = mask;
	*flags |= NFS_BOOT_HAS_MASK;
	printf("nfs_boot: my_mask=%s\n", inet_ntoa(nd->nd_mask));
	(void)  nfs_boot_deladdress(ifp, lwp, my_ip.s_addr);
	error = nfs_boot_setaddress(ifp, lwp, my_ip.s_addr,
				    mask, INADDR_ANY);
	if (error) {
		printf("nfs_boot: set ifmask, error=%d\n", error);
		goto out;
	}
	goto gwok;
nogwrepl:
#endif
#ifdef NFS_BOOT_GATEWAY
	/*
	 * Note: we normally ignore the gateway address returned
	 * by the "bootparam/whoami" RPC above, because many old
	 * bootparam servers supply a bogus gateway value.
	 *
	 * These deficiencies in the bootparam RPC interface are
	 * circumvented by using the bootparam/getfile RPC.  The
	 * parameter "gateway" is requested, and if its returned,
	 * we use the "server" part of the reply as the gateway,
	 * and use the "pathname" part of the reply as the mask.
	 * (The mask comes to us as a string.)
	 */
	if (gw_ip.s_addr) {
		/* Our caller will add the route. */
		nd->nd_gwip = gw_ip;
		*flags |= NFS_BOOT_HAS_GWIP;
	}
#endif

delout:
	if (error)
		(void) nfs_boot_deladdress(ifp, lwp, my_ip.s_addr);
out:
	if (error) {
		(void) nfs_boot_ifupdown(ifp, lwp, 0);
		nfs_boot_flushrt(ifp);
	}
#ifndef NFS_BOOTPARAM_NOGATEWAY
gwok:
	if (gw_ndm)
		kmem_free(gw_ndm, sizeof(*gw_ndm));
#endif
	if ((*flags & NFS_BOOT_ALLINFO) != NFS_BOOT_ALLINFO)
		return error ? error : EADDRNOTAVAIL;

	return (error);
}


/*
 * RPC: bootparam/whoami
 * Given client IP address, get:
 *	client name	(hostname)
 *	domain name (domainname)
 *	gateway address
 *
 * The hostname and domainname are set here for convenience.
 *
 * Note - bpsin is initialized to the broadcast address,
 * and will be replaced with the bootparam server address
 * after this call is complete.  Have to use PMAP_PROC_CALL
 * to make sure we get responses only from a servers that
 * know about us (don't want to broadcast a getport call).
 */
static int
bp_whoami(struct sockaddr_in *bpsin, struct in_addr *my_ip,
	struct in_addr *gw_ip, struct lwp *l)
{
	/* RPC structures for PMAPPROC_CALLIT */
	struct whoami_call {
		u_int32_t call_prog;
		u_int32_t call_vers;
		u_int32_t call_proc;
		u_int32_t call_arglen;
	} *call;
	struct callit_reply {
		u_int32_t port;
		u_int32_t encap_len;
		/* encapsulated data here */
	} *reply;

	struct mbuf *m, *from;
	struct sockaddr_in *sin;
	int error;
	int16_t port;

	/*
	 * Build request message for PMAPPROC_CALLIT.
	 */
	m = m_get(M_WAIT, MT_DATA);
	call = mtod(m, struct whoami_call *);
	m->m_len = sizeof(*call);
	call->call_prog = txdr_unsigned(BOOTPARAM_PROG);
	call->call_vers = txdr_unsigned(BOOTPARAM_VERS);
	call->call_proc = txdr_unsigned(BOOTPARAM_WHOAMI);

	/*
	 * append encapsulated data (client IP address)
	 */
	m->m_next = xdr_inaddr_encode(my_ip);
	call->call_arglen = txdr_unsigned(m->m_next->m_len);

	/* RPC: portmap/callit */
	bpsin->sin_port = htons(PMAPPORT);
	error = krpc_call(bpsin, PMAPPROG, PMAPVERS,
			PMAPPROC_CALLIT, &m, &from, l);
	if (error) {
		m_freem(m);
		return error;
	}

	/*
	 * Parse result message.
	 */
	if (m->m_len < sizeof(*reply)) {
		m = m_pullup(m, sizeof(*reply));
		if (m == NULL)
			goto bad;
	}
	reply = mtod(m, struct callit_reply *);
	port = fxdr_unsigned(u_int32_t, reply->port);
	m_adj(m, sizeof(*reply));

	/*
	 * Save bootparam server address
	 */
	sin = mtod(from, struct sockaddr_in *);
	bpsin->sin_port = htons(port);
	bpsin->sin_addr.s_addr = sin->sin_addr.s_addr;

	/* client name */
	hostnamelen = MAXHOSTNAMELEN-1;
	m = xdr_string_decode(m, hostname, &hostnamelen);
	if (m == NULL)
		goto bad;

	/* domain name */
	domainnamelen = MAXHOSTNAMELEN-1;
	m = xdr_string_decode(m, domainname, &domainnamelen);
	if (m == NULL)
		goto bad;

	/* gateway address */
	m = xdr_inaddr_decode(m, gw_ip);
	if (m == NULL)
		goto bad;

	/* success */
	goto out;

bad:
	printf("nfs_boot: bootparam_whoami: bad reply\n");
	error = EBADRPC;

out:
	m_freem(from);
	if (m)
		m_freem(m);
	return(error);
}


/*
 * RPC: bootparam/getfile
 * Given client name and file "key", get:
 *	server name
 *	server IP address
 *	server pathname
 */
static int
bp_getfile(struct sockaddr_in *bpsin, const char *key,
	struct nfs_dlmount *ndm, struct lwp *l)
{
	char pathname[MNAMELEN];
	struct in_addr inaddr;
	struct sockaddr_in *sin;
	struct mbuf *m;
	char *serv_name;
	int error, sn_len, path_len;

	/*
	 * Build request message.
	 */

	/* client name (hostname) */
	m  = xdr_string_encode(hostname, hostnamelen);
	if (m == NULL)
		return (ENOMEM);

	/* key name (root or swap) */
	/*XXXUNCONST*/
	m->m_next = xdr_string_encode(__UNCONST(key), strlen(key));
	if (m->m_next == NULL)
		return (ENOMEM);

	/* RPC: bootparam/getfile */
	error = krpc_call(bpsin, BOOTPARAM_PROG, BOOTPARAM_VERS,
	                  BOOTPARAM_GETFILE, &m, NULL, l);
	if (error)
		return error;

	/*
	 * Parse result message.
	 */

	/* server name */
	serv_name = &ndm->ndm_host[0];
	sn_len = sizeof(ndm->ndm_host) - 1;
	m = xdr_string_decode(m, serv_name, &sn_len);
	if (m == NULL)
		goto bad;

	/* server IP address (mountd/NFS) */
	m = xdr_inaddr_decode(m, &inaddr);
	if (m == NULL)
		goto bad;

	/* server pathname */
	path_len = sizeof(pathname) - 1;
	m = xdr_string_decode(m, pathname, &path_len);
	if (m == NULL)
		goto bad;

	/*
	 * Store the results in the nfs_dlmount.
	 * The strings become "server:pathname"
	 */
	sin = (struct sockaddr_in *) &ndm->ndm_saddr;
	memset((void *)sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = inaddr;
	if ((sn_len + 1 + path_len + 1) > sizeof(ndm->ndm_host)) {
		printf("nfs_boot: getfile name too long\n");
		error = EIO;
		goto out;
	}
	ndm->ndm_host[sn_len] = ':';
	memcpy(ndm->ndm_host + sn_len + 1, pathname, path_len + 1);

	/* success */
	goto out;

bad:
	printf("nfs_boot: bootparam_getfile: bad reply\n");
	error = EBADRPC;

out:
	m_freem(m);
	return(0);
}
