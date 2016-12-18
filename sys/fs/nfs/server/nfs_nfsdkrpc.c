/*	$NetBSD: nfs_nfsdkrpc.c,v 1.2 2014/03/25 16:30:28 christos Exp $	*/
/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
/* __FBSDID("FreeBSD: head/sys/fs/nfsserver/nfs_nfsdkrpc.c 249596 2013-04-17 22:42:43Z ken "); */
__RCSID("$NetBSD: nfs_nfsdkrpc.c,v 1.2 2014/03/25 16:30:28 christos Exp $");

#include "opt_inet6.h"
#include "opt_kgssapi.h"

#include <fs/nfs/nfsport.h>

#include <rpc/rpc.h>
#include <rpc/rpcsec_gss.h>

#include <nfs/nfs_fha.h>
#include <fs/nfsserver/nfs_fha_new.h>

#include <security/mac/mac_framework.h>

NFSDLOCKMUTEX;
NFSV4ROOTLOCKMUTEX;
struct nfsv4lock nfsd_suspend_lock;

/*
 * Mapping of old NFS Version 2 RPC numbers to generic numbers.
 */
int newnfs_nfsv3_procid[NFS_V3NPROCS] = {
	NFSPROC_NULL,
	NFSPROC_GETATTR,
	NFSPROC_SETATTR,
	NFSPROC_NOOP,
	NFSPROC_LOOKUP,
	NFSPROC_READLINK,
	NFSPROC_READ,
	NFSPROC_NOOP,
	NFSPROC_WRITE,
	NFSPROC_CREATE,
	NFSPROC_REMOVE,
	NFSPROC_RENAME,
	NFSPROC_LINK,
	NFSPROC_SYMLINK,
	NFSPROC_MKDIR,
	NFSPROC_RMDIR,
	NFSPROC_READDIR,
	NFSPROC_FSSTAT,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
};


SYSCTL_DECL(_vfs_nfsd);

SVCPOOL		*nfsrvd_pool;

static int	nfs_privport = 0;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, nfs_privport, CTLFLAG_RW,
    &nfs_privport, 0,
    "Only allow clients using a privileged port for NFSv2 and 3");

static int	nfs_minvers = NFS_VER2;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, server_min_nfsvers, CTLFLAG_RW,
    &nfs_minvers, 0, "The lowest version of NFS handled by the server");

static int	nfs_maxvers = NFS_VER4;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, server_max_nfsvers, CTLFLAG_RW,
    &nfs_maxvers, 0, "The highest version of NFS handled by the server");

static int nfs_proc(struct nfsrv_descript *, u_int32_t, struct socket *,
    u_int64_t, struct nfsrvcache **);

extern u_long sb_max_adj;
extern int newnfs_numnfsd;
extern struct proc *nfsd_master_proc;

/*
 * NFS server system calls
 */

static void
nfssvc_program(struct svc_req *rqst, SVCXPRT *xprt)
{
	struct nfsrv_descript nd;
	struct nfsrvcache *rp = NULL;
	int cacherep, credflavor;

	memset(&nd, 0, sizeof(nd));
	if (rqst->rq_vers == NFS_VER2) {
		if (rqst->rq_proc > NFSV2PROC_STATFS) {
			svcerr_noproc(rqst);
			svc_freereq(rqst);
			goto out;
		}
		nd.nd_procnum = newnfs_nfsv3_procid[rqst->rq_proc];
		nd.nd_flag = ND_NFSV2;
	} else if (rqst->rq_vers == NFS_VER3) {
		if (rqst->rq_proc >= NFS_V3NPROCS) {
			svcerr_noproc(rqst);
			svc_freereq(rqst);
			goto out;
		}
		nd.nd_procnum = rqst->rq_proc;
		nd.nd_flag = ND_NFSV3;
	} else {
		if (rqst->rq_proc != NFSPROC_NULL &&
		    rqst->rq_proc != NFSV4PROC_COMPOUND) {
			svcerr_noproc(rqst);
			svc_freereq(rqst);
			goto out;
		}
		nd.nd_procnum = rqst->rq_proc;
		nd.nd_flag = ND_NFSV4;
	}

	/*
	 * Note: we want rq_addr, not svc_getrpccaller for nd_nam2 -
	 * NFS_SRVMAXDATA uses a NULL value for nd_nam2 to detect TCP
	 * mounts.
	 */
	nd.nd_mrep = rqst->rq_args;
	rqst->rq_args = NULL;
	newnfs_realign(&nd.nd_mrep, M_WAITOK);
	nd.nd_md = nd.nd_mrep;
	nd.nd_dpos = mtod(nd.nd_md, caddr_t);
	nd.nd_nam = svc_getrpccaller(rqst);
	nd.nd_nam2 = rqst->rq_addr;
	nd.nd_mreq = NULL;
	nd.nd_cred = NULL;

	if (nfs_privport && (nd.nd_flag & ND_NFSV4) == 0) {
		/* Check if source port is privileged */
		u_short port;
		struct sockaddr *nam = nd.nd_nam;
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)nam;
		/*
		 * INET/INET6 - same code:
		 *    sin_port and sin6_port are at same offset
		 */
		port = ntohs(sin->sin_port);
		if (port >= IPPORT_RESERVED &&
		    nd.nd_procnum != NFSPROC_NULL) {
#ifdef INET6
			char b6[INET6_ADDRSTRLEN];
#if defined(KLD_MODULE)
			/* Do not use ip6_sprintf: the nfs module should work without INET6. */
#define	ip6_sprintf(buf, a)						\
			(snprintf((buf), sizeof(buf), "%x:%x:%x:%x:%x:%x:%x:%x",	\
			    (a)->s6_addr16[0], (a)->s6_addr16[1],	\
			    (a)->s6_addr16[2], (a)->s6_addr16[3],	\
			    (a)->s6_addr16[4], (a)->s6_addr16[5],	\
			    (a)->s6_addr16[6], (a)->s6_addr16[7]),	\
			    (buf))
#endif
#endif
			printf("NFS request from unprivileged port (%s:%d)\n",
#ifdef INET6
			    sin->sin_family == AF_INET6 ?
			    ip6_sprintf(b6, &satosin6(sin)->sin6_addr) :
#if defined(KLD_MODULE)
#undef ip6_sprintf
#endif
#endif
			    inet_ntoa(sin->sin_addr), port);
			svcerr_weakauth(rqst);
			svc_freereq(rqst);
			m_freem(nd.nd_mrep);
			goto out;
		}
	}

	if (nd.nd_procnum != NFSPROC_NULL) {
		if (!svc_getcred(rqst, &nd.nd_cred, &credflavor)) {
			svcerr_weakauth(rqst);
			svc_freereq(rqst);
			m_freem(nd.nd_mrep);
			goto out;
		}

		/* Set the flag based on credflavor */
		if (credflavor == RPCSEC_GSS_KRB5) {
			nd.nd_flag |= ND_GSS;
		} else if (credflavor == RPCSEC_GSS_KRB5I) {
			nd.nd_flag |= (ND_GSS | ND_GSSINTEGRITY);
		} else if (credflavor == RPCSEC_GSS_KRB5P) {
			nd.nd_flag |= (ND_GSS | ND_GSSPRIVACY);
		} else if (credflavor != AUTH_SYS) {
			svcerr_weakauth(rqst);
			svc_freereq(rqst);
			m_freem(nd.nd_mrep);
			goto out;
		}

#ifdef MAC
		mac_cred_associate_nfsd(nd.nd_cred);
#endif
		/*
		 * Get a refcnt (shared lock) on nfsd_suspend_lock.
		 * NFSSVC_SUSPENDNFSD will take an exclusive lock on
		 * nfsd_suspend_lock to suspend these threads.
		 * This must be done here, before the check of
		 * nfsv4root exports by nfsvno_v4rootexport().
		 */
		NFSLOCKV4ROOTMUTEX();
		nfsv4_getref(&nfsd_suspend_lock, NULL, NFSV4ROOTLOCKMUTEXPTR,
		    NULL);
		NFSUNLOCKV4ROOTMUTEX();

		if ((nd.nd_flag & ND_NFSV4) != 0) {
			nd.nd_repstat = nfsvno_v4rootexport(&nd);
			if (nd.nd_repstat != 0) {
				NFSLOCKV4ROOTMUTEX();
				nfsv4_relref(&nfsd_suspend_lock);
				NFSUNLOCKV4ROOTMUTEX();
				svcerr_weakauth(rqst);
				svc_freereq(rqst);
				m_freem(nd.nd_mrep);
				goto out;
			}
		}

		cacherep = nfs_proc(&nd, rqst->rq_xid, xprt->xp_socket,
		    xprt->xp_sockref, &rp);
		NFSLOCKV4ROOTMUTEX();
		nfsv4_relref(&nfsd_suspend_lock);
		NFSUNLOCKV4ROOTMUTEX();
	} else {
		NFSMGET(nd.nd_mreq);
		nd.nd_mreq->m_len = 0;
		cacherep = RC_REPLY;
	}
	if (nd.nd_mrep != NULL)
		m_freem(nd.nd_mrep);

	if (nd.nd_cred != NULL)
		crfree(nd.nd_cred);

	if (cacherep == RC_DROPIT) {
		if (nd.nd_mreq != NULL)
			m_freem(nd.nd_mreq);
		svc_freereq(rqst);
		goto out;
	}

	if (nd.nd_mreq == NULL) {
		svcerr_decode(rqst);
		svc_freereq(rqst);
		goto out;
	}

	if (nd.nd_repstat & NFSERR_AUTHERR) {
		svcerr_auth(rqst, nd.nd_repstat & ~NFSERR_AUTHERR);
		if (nd.nd_mreq != NULL)
			m_freem(nd.nd_mreq);
	} else if (!svc_sendreply_mbuf(rqst, nd.nd_mreq)) {
		svcerr_systemerr(rqst);
	}
	if (rp != NULL)
		nfsrvd_sentcache(rp, xprt->xp_socket, 0);
	svc_freereq(rqst);

out:
	NFSEXITCODE(0);
}

/*
 * Check the cache and, optionally, do the RPC.
 * Return the appropriate cache response.
 */
static int
nfs_proc(struct nfsrv_descript *nd, u_int32_t xid, struct socket *so,
    u_int64_t sockref, struct nfsrvcache **rpp)
{
	struct thread *td = curthread;
	int cacherep = RC_DOIT, isdgram;

	*rpp = NULL;
	if (nd->nd_nam2 == NULL) {
		nd->nd_flag |= ND_STREAMSOCK;
		isdgram = 0;
	} else {
		isdgram = 1;
	}

	/*
	 * Two cases:
	 * 1 - For NFSv2 over UDP, if we are near our malloc/mget
	 *     limit, just drop the request. There is no
	 *     NFSERR_RESOURCE or NFSERR_DELAY for NFSv2 and the
	 *     client will timeout/retry over UDP in a little while.
	 * 2 - nd_repstat == 0 && nd_mreq == NULL, which
	 *     means a normal nfs rpc, so check the cache
	 */
	if ((nd->nd_flag & ND_NFSV2) && nd->nd_nam2 != NULL &&
	    nfsrv_mallocmget_limit()) {
		cacherep = RC_DROPIT;
	} else {
		/*
		 * For NFSv3, play it safe and assume that the client is
		 * doing retries on the same TCP connection.
		 */
		if ((nd->nd_flag & (ND_NFSV4 | ND_STREAMSOCK)) ==
		    ND_STREAMSOCK)
			nd->nd_flag |= ND_SAMETCPCONN;
		nd->nd_retxid = xid;
		nd->nd_tcpconntime = NFSD_MONOSEC;
		nd->nd_sockref = sockref;
		cacherep = nfsrvd_getcache(nd, so);
	}

	/*
	 * Handle the request. There are three cases.
	 * RC_DOIT - do the RPC
	 * RC_REPLY - return the reply already created
	 * RC_DROPIT - just throw the request away
	 */
	if (cacherep == RC_DOIT) {
		nfsrvd_dorpc(nd, isdgram, td);
		if (nd->nd_repstat == NFSERR_DONTREPLY)
			cacherep = RC_DROPIT;
		else
			cacherep = RC_REPLY;
		*rpp = nfsrvd_updatecache(nd, so);
	}

	NFSEXITCODE2(0, nd);
	return (cacherep);
}

/*
 * Adds a socket to the list for servicing by nfsds.
 */
int
nfsrvd_addsock(struct file *fp)
{
	int siz;
	struct socket *so;
	int error = 0;
	SVCXPRT *xprt;
	static u_int64_t sockref = 0;

	so = fp->f_data;

	siz = sb_max_adj;
	error = soreserve(so, siz, siz);
	if (error)
		goto out;

	/*
	 * Steal the socket from userland so that it doesn't close
	 * unexpectedly.
	 */
	if (so->so_type == SOCK_DGRAM)
		xprt = svc_dg_create(nfsrvd_pool, so, 0, 0);
	else
		xprt = svc_vc_create(nfsrvd_pool, so, 0, 0);
	if (xprt) {
		fp->f_ops = &badfileops;
		fp->f_data = NULL;
		xprt->xp_sockref = ++sockref;
		if (nfs_minvers == NFS_VER2)
			svc_reg(xprt, NFS_PROG, NFS_VER2, nfssvc_program,
			    NULL);
		if (nfs_minvers <= NFS_VER3 && nfs_maxvers >= NFS_VER3)
			svc_reg(xprt, NFS_PROG, NFS_VER3, nfssvc_program,
			    NULL);
		if (nfs_maxvers >= NFS_VER4)
			svc_reg(xprt, NFS_PROG, NFS_VER4, nfssvc_program,
			    NULL);
		SVC_RELEASE(xprt);
	}

out:
	NFSEXITCODE(error);
	return (error);
}

/*
 * Called by nfssvc() for nfsds. Just loops around servicing rpc requests
 * until it is killed by a signal.
 */
int
nfsrvd_nfsd(struct thread *td, struct nfsd_nfsd_args *args)
{
	char principal[MAXHOSTNAMELEN + 5];
	int error = 0;
	bool_t ret2, ret3, ret4;

	error = copyinstr(args->principal, principal, sizeof (principal),
	    NULL);
	if (error)
		goto out;

	/*
	 * Only the first nfsd actually does any work. The RPC code
	 * adds threads to it as needed. Any extra processes offered
	 * by nfsd just exit. If nfsd is new enough, it will call us
	 * once with a structure that specifies how many threads to
	 * use.
	 */
	NFSD_LOCK();
	if (newnfs_numnfsd == 0) {
		newnfs_numnfsd++;

		NFSD_UNLOCK();

		/* An empty string implies AUTH_SYS only. */
		if (principal[0] != '\0') {
			ret2 = rpc_gss_set_svc_name_call(principal,
			    "kerberosv5", GSS_C_INDEFINITE, NFS_PROG, NFS_VER2);
			ret3 = rpc_gss_set_svc_name_call(principal,
			    "kerberosv5", GSS_C_INDEFINITE, NFS_PROG, NFS_VER3);
			ret4 = rpc_gss_set_svc_name_call(principal,
			    "kerberosv5", GSS_C_INDEFINITE, NFS_PROG, NFS_VER4);

			if (!ret2 || !ret3 || !ret4)
				printf("nfsd: can't register svc name\n");
		}

		nfsrvd_pool->sp_minthreads = args->minthreads;
		nfsrvd_pool->sp_maxthreads = args->maxthreads;
			
		svc_run(nfsrvd_pool);

		if (principal[0] != '\0') {
			rpc_gss_clear_svc_name_call(NFS_PROG, NFS_VER2);
			rpc_gss_clear_svc_name_call(NFS_PROG, NFS_VER3);
			rpc_gss_clear_svc_name_call(NFS_PROG, NFS_VER4);
		}

		NFSD_LOCK();
		newnfs_numnfsd--;
		nfsrvd_init(1);
	}
	NFSD_UNLOCK();

out:
	NFSEXITCODE(error);
	return (error);
}

/*
 * Initialize the data structures for the server.
 * Handshake with any new nfsds starting up to avoid any chance of
 * corruption.
 */
void
nfsrvd_init(int terminating)
{

	NFSD_LOCK_ASSERT();

	if (terminating) {
		nfsd_master_proc = NULL;
		NFSD_UNLOCK();
		svcpool_destroy(nfsrvd_pool);
		nfsrvd_pool = NULL;
		NFSD_LOCK();
	}

	NFSD_UNLOCK();

	nfsrvd_pool = svcpool_create("nfsd", SYSCTL_STATIC_CHILDREN(_vfs_nfsd));
	nfsrvd_pool->sp_rcache = NULL;
	nfsrvd_pool->sp_assign = fhanew_assign;
	nfsrvd_pool->sp_done = fha_nd_complete;

	NFSD_LOCK();
}

