/*	$NetBSD: nfs_srvcache.c,v 1.45 2009/03/15 17:20:10 cegger Exp $	*/

/*
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)nfs_srvcache.c	8.3 (Berkeley) 3/30/95
 */

/*
 * Reference: Chet Juszczak, "Improving the Performance and Correctness
 *		of an NFS Server", in Proc. Winter 1989 USENIX Conference,
 *		pages 53-63. San Diego, February 1989.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_srvcache.c,v 1.45 2009/03/15 17:20:10 cegger Exp $");

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/condvar.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netinet/in.h>
#include <nfs/nfsm_subs.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsrvcache.h>
#include <nfs/nfs_var.h>

extern struct nfsstats nfsstats;
extern const int nfsv2_procid[NFS_NPROCS];
long numnfsrvcache, desirednfsrvcache = NFSRVCACHESIZ;
struct pool nfs_reqcache_pool;

#define	NFSRCHASH(xid) \
	(&nfsrvhashtbl[((xid) + ((xid) >> 24)) & nfsrvhash])
LIST_HEAD(nfsrvhash, nfsrvcache) *nfsrvhashtbl;
TAILQ_HEAD(nfsrvlru, nfsrvcache) nfsrvlruhead;
kmutex_t nfsrv_reqcache_lock;
u_long nfsrvhash;

#if defined(MBUFTRACE)
static struct mowner nfsd_cache_mowner = MOWNER_INIT("nfsd", "cache");
#endif /* defined(MBUFTRACE) */

#define	NETFAMILY(rp) \
		(((rp)->rc_flags & RC_INETADDR) ? AF_INET : -1)

static struct nfsrvcache *nfsrv_lookupcache(struct nfsrv_descript *nd);
static void nfsrv_unlockcache(struct nfsrvcache *rp);

/*
 * Static array that defines which nfs rpc's are nonidempotent
 */
const int nonidempotent[NFS_NPROCS] = {
	false,	/* NULL */
	false,	/* GETATTR */
	true,	/* SETATTR */
	false,	/* LOOKUP */
	false,	/* ACCESS */
	false,	/* READLINK */
	false,	/* READ */
	true,	/* WRITE */
	true,	/* CREATE */
	true,	/* MKDIR */
	true,	/* SYMLINK */
	true,	/* MKNOD */
	true,	/* REMOVE */
	true,	/* RMDIR */
	true,	/* RENAME */
	true,	/* LINK */
	false,	/* READDIR */
	false,	/* READDIRPLUS */
	false,	/* FSSTAT */
	false,	/* FSINFO */
	false,	/* PATHCONF */
	false,	/* COMMIT */
	false,	/* NOOP */
};

/* True iff the rpc reply is an nfs status ONLY! */
static const int nfsv2_repstat[NFS_NPROCS] = {
	false,	/* NULL */
	false,	/* GETATTR */
	false,	/* SETATTR */
	false,	/* NOOP */
	false,	/* LOOKUP */
	false,	/* READLINK */
	false,	/* READ */
	false,	/* Obsolete WRITECACHE */
	false,	/* WRITE */
	false,	/* CREATE */
	true,	/* REMOVE */
	true,	/* RENAME */
	true,	/* LINK */
	true,	/* SYMLINK */
	false,	/* MKDIR */
	true,	/* RMDIR */
	false,	/* READDIR */
	false,	/* STATFS */
};

static void
cleanentry(struct nfsrvcache *rp)
{

	if ((rp->rc_flags & RC_REPMBUF) != 0) {
		m_freem(rp->rc_reply);
	}
	if ((rp->rc_flags & RC_NAM) != 0) {
		m_free(rp->rc_nam);
	}
	rp->rc_flags &= ~(RC_REPSTATUS|RC_REPMBUF);
}

/*
 * Initialize the server request cache list
 */
void
nfsrv_initcache(void)
{

	mutex_init(&nfsrv_reqcache_lock, MUTEX_DEFAULT, IPL_NONE);
	nfsrvhashtbl = hashinit(desirednfsrvcache, HASH_LIST, true,
	    &nfsrvhash);
	TAILQ_INIT(&nfsrvlruhead);
	pool_init(&nfs_reqcache_pool, sizeof(struct nfsrvcache), 0, 0, 0,
	    "nfsreqcachepl", &pool_allocator_nointr, IPL_NONE);
	MOWNER_ATTACH(&nfsd_cache_mowner);
}

void
nfsrv_finicache(void)
{

	nfsrv_cleancache();
	KASSERT(TAILQ_EMPTY(&nfsrvlruhead));
	pool_destroy(&nfs_reqcache_pool);
	hashdone(nfsrvhashtbl, HASH_LIST, nfsrvhash);
	MOWNER_DETACH(&nfsd_cache_mowner);
	mutex_destroy(&nfsrv_reqcache_lock);
}

/*
 * Lookup a cache and lock it
 */
static struct nfsrvcache *
nfsrv_lookupcache(struct nfsrv_descript *nd)
{
	struct nfsrvcache *rp;

	KASSERT(mutex_owned(&nfsrv_reqcache_lock));

loop:
	LIST_FOREACH(rp, NFSRCHASH(nd->nd_retxid), rc_hash) {
		if (nd->nd_retxid == rp->rc_xid &&
		    nd->nd_procnum == rp->rc_proc &&
		    netaddr_match(NETFAMILY(rp), &rp->rc_haddr, nd->nd_nam)) {
			if ((rp->rc_gflags & RC_G_LOCKED) != 0) {
				cv_wait(&rp->rc_cv, &nfsrv_reqcache_lock);
				goto loop;
			}
			rp->rc_gflags |= RC_G_LOCKED;
			break;
		}
	}

	return rp;
}

/*
 * Unlock a cache
 */
static void
nfsrv_unlockcache(struct nfsrvcache *rp)
{

	KASSERT(mutex_owned(&nfsrv_reqcache_lock));

	KASSERT((rp->rc_gflags & RC_G_LOCKED) != 0);
	rp->rc_gflags &= ~RC_G_LOCKED;
	cv_broadcast(&rp->rc_cv);
}

/*
 * Look for the request in the cache
 * If found then
 *    return action and optionally reply
 * else
 *    insert it in the cache
 *
 * The rules are as follows:
 * - if in progress, return DROP request
 * - if completed within DELAY of the current time, return DROP it
 * - if completed a longer time ago return REPLY if the reply was cached or
 *   return DOIT
 * Update/add new request at end of lru list
 */
int
nfsrv_getcache(struct nfsrv_descript *nd, struct nfssvc_sock *slp, struct mbuf **repp)
{
	struct nfsrvcache *rp, *rpdup;
	struct mbuf *mb;
	struct sockaddr_in *saddr;
	char *bpos;
	int ret;

	mutex_enter(&nfsrv_reqcache_lock);
	rp = nfsrv_lookupcache(nd);
	if (rp) {
		mutex_exit(&nfsrv_reqcache_lock);
found:
		/* If not at end of LRU chain, move it there */
		if (TAILQ_NEXT(rp, rc_lru)) { /* racy but ok */
			mutex_enter(&nfsrv_reqcache_lock);
			TAILQ_REMOVE(&nfsrvlruhead, rp, rc_lru);
			TAILQ_INSERT_TAIL(&nfsrvlruhead, rp, rc_lru);
			mutex_exit(&nfsrv_reqcache_lock);
		}
		if (rp->rc_state == RC_UNUSED)
			panic("nfsrv cache");
		if (rp->rc_state == RC_INPROG) {
			nfsstats.srvcache_inproghits++;
			ret = RC_DROPIT;
		} else if (rp->rc_flags & RC_REPSTATUS) {
			nfsstats.srvcache_nonidemdonehits++;
			nfs_rephead(0, nd, slp, rp->rc_status,
			   0, (u_quad_t *)0, repp, &mb, &bpos);
			ret = RC_REPLY;
		} else if (rp->rc_flags & RC_REPMBUF) {
			nfsstats.srvcache_nonidemdonehits++;
			*repp = m_copym(rp->rc_reply, 0, M_COPYALL,
					M_WAIT);
			ret = RC_REPLY;
		} else {
			nfsstats.srvcache_idemdonehits++;
			rp->rc_state = RC_INPROG;
			ret = RC_DOIT;
		}
		mutex_enter(&nfsrv_reqcache_lock);
		nfsrv_unlockcache(rp);
		mutex_exit(&nfsrv_reqcache_lock);
		return ret;
	}
	nfsstats.srvcache_misses++;
	if (numnfsrvcache < desirednfsrvcache) {
		numnfsrvcache++;
		mutex_exit(&nfsrv_reqcache_lock);
		rp = pool_get(&nfs_reqcache_pool, PR_WAITOK);
		memset(rp, 0, sizeof *rp);
		cv_init(&rp->rc_cv, "nfsdrc");
		rp->rc_gflags = RC_G_LOCKED;
	} else {
		rp = TAILQ_FIRST(&nfsrvlruhead);
		while ((rp->rc_gflags & RC_G_LOCKED) != 0) {
			cv_wait(&rp->rc_cv, &nfsrv_reqcache_lock);
			rp = TAILQ_FIRST(&nfsrvlruhead);
		}
		rp->rc_gflags |= RC_G_LOCKED;
		LIST_REMOVE(rp, rc_hash);
		TAILQ_REMOVE(&nfsrvlruhead, rp, rc_lru);
		mutex_exit(&nfsrv_reqcache_lock);
		cleanentry(rp);
		rp->rc_flags = 0;
	}
	rp->rc_state = RC_INPROG;
	rp->rc_xid = nd->nd_retxid;
	saddr = mtod(nd->nd_nam, struct sockaddr_in *);
	switch (saddr->sin_family) {
	case AF_INET:
		rp->rc_flags |= RC_INETADDR;
		rp->rc_inetaddr = saddr->sin_addr.s_addr;
		break;
	default:
		rp->rc_flags |= RC_NAM;
		rp->rc_nam = m_copym(nd->nd_nam, 0, M_COPYALL, M_WAIT);
		m_claimm(rp->rc_nam, &nfsd_cache_mowner);
		break;
	};
	rp->rc_proc = nd->nd_procnum;
	mutex_enter(&nfsrv_reqcache_lock);
	rpdup = nfsrv_lookupcache(nd);
	if (rpdup != NULL) {
		/*
		 * other thread made duplicate cache entry.
		 */
		KASSERT(numnfsrvcache > 0);
		numnfsrvcache--;
		mutex_exit(&nfsrv_reqcache_lock);
		cleanentry(rp);
		cv_destroy(&rp->rc_cv);
		pool_put(&nfs_reqcache_pool, rp);
		rp = rpdup;
		goto found;
	}
	TAILQ_INSERT_TAIL(&nfsrvlruhead, rp, rc_lru);
	LIST_INSERT_HEAD(NFSRCHASH(nd->nd_retxid), rp, rc_hash);
	nfsrv_unlockcache(rp);
	mutex_exit(&nfsrv_reqcache_lock);
	return RC_DOIT;
}

/*
 * Update a request cache entry after the rpc has been done
 */
void
nfsrv_updatecache(struct nfsrv_descript *nd, int repvalid, struct mbuf *repmbuf)
{
	struct nfsrvcache *rp;

	mutex_enter(&nfsrv_reqcache_lock);
	rp = nfsrv_lookupcache(nd);
	mutex_exit(&nfsrv_reqcache_lock);
	if (rp) {
		cleanentry(rp);
		rp->rc_state = RC_DONE;
		/*
		 * If we have a valid reply update status and save
		 * the reply for non-idempotent rpc's.
		 */
		if (repvalid && nonidempotent[nd->nd_procnum]) {
			if ((nd->nd_flag & ND_NFSV3) == 0 &&
			  nfsv2_repstat[nfsv2_procid[nd->nd_procnum]]) {
				rp->rc_status = nd->nd_repstat;
				rp->rc_flags |= RC_REPSTATUS;
			} else {
				rp->rc_reply = m_copym(repmbuf,
					0, M_COPYALL, M_WAIT);
				m_claimm(rp->rc_reply, &nfsd_cache_mowner);
				rp->rc_flags |= RC_REPMBUF;
			}
		}
		mutex_enter(&nfsrv_reqcache_lock);
		nfsrv_unlockcache(rp);
		mutex_exit(&nfsrv_reqcache_lock);
	}
}

/*
 * Clean out the cache. Called when the last nfsd terminates.
 */
void
nfsrv_cleancache(void)
{
	struct nfsrvcache *rp;

	mutex_enter(&nfsrv_reqcache_lock);
	while ((rp = TAILQ_FIRST(&nfsrvlruhead)) != NULL) {
		KASSERT((rp->rc_gflags & RC_G_LOCKED) == 0);
		LIST_REMOVE(rp, rc_hash);
		TAILQ_REMOVE(&nfsrvlruhead, rp, rc_lru);
		KASSERT(numnfsrvcache > 0);
		numnfsrvcache--;
		mutex_exit(&nfsrv_reqcache_lock);
		cleanentry(rp);
		cv_destroy(&rp->rc_cv);
		pool_put(&nfs_reqcache_pool, rp);
		mutex_enter(&nfsrv_reqcache_lock);
	}
	KASSERT(numnfsrvcache == 0);
	mutex_exit(&nfsrv_reqcache_lock);
}
