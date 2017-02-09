/*	$NetBSD: nfs_iod.c,v 1.7 2015/07/15 03:28:55 manu Exp $	*/

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
 *	@(#)nfs_syscalls.c	8.5 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_iod.c,v 1.7 2015/07/15 03:28:55 manu Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/namei.h>
#include <sys/syslog.h>
#include <sys/filedesc.h>
#include <sys/kthread.h>
#include <sys/kauth.h>
#include <sys/syscallargs.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nfs/xdr_subs.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsrvcache.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsrtt.h>
#include <nfs/nfs_var.h>

extern int nuidhash_max;

/*
 * locking order:
 *	nfs_iodlist_lock -> nid_lock -> nm_lock
 */
kmutex_t nfs_iodlist_lock;
struct nfs_iodlist nfs_iodlist_idle;
struct nfs_iodlist nfs_iodlist_all;
int nfs_niothreads = -1; /* == "0, and has never been set" */
int nfs_defect = 0;

/*
 * Asynchronous I/O threads for client nfs.
 * They do read-ahead and write-behind operations on the block I/O cache.
 * Never returns unless it fails or gets killed.
 */

static void
nfssvc_iod(void *arg)
{
	struct buf *bp;
	struct nfs_iod *myiod;
	struct nfsmount *nmp;

	myiod = kmem_alloc(sizeof(*myiod), KM_SLEEP);
	mutex_init(&myiod->nid_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&myiod->nid_cv, "nfsiod");
	myiod->nid_exiting = false;
	myiod->nid_mount = NULL;
	mutex_enter(&nfs_iodlist_lock);
	LIST_INSERT_HEAD(&nfs_iodlist_all, myiod, nid_all);
	mutex_exit(&nfs_iodlist_lock);

	for (;;) {
		mutex_enter(&nfs_iodlist_lock);
		LIST_INSERT_HEAD(&nfs_iodlist_idle, myiod, nid_idle);
		mutex_exit(&nfs_iodlist_lock);

		mutex_enter(&myiod->nid_lock);
		while (/*CONSTCOND*/ true) {
			nmp = myiod->nid_mount;
			if (nmp) {
				myiod->nid_mount = NULL;
				break;
			}
			if (__predict_false(myiod->nid_exiting)) {
				/*
				 * drop nid_lock to preserve locking order.
				 */
				mutex_exit(&myiod->nid_lock);
				mutex_enter(&nfs_iodlist_lock);
				mutex_enter(&myiod->nid_lock);
				/*
				 * recheck nid_mount because nfs_asyncio can
				 * pick us in the meantime as we are still on
				 * nfs_iodlist_lock.
				 */
				if (myiod->nid_mount != NULL) {
					mutex_exit(&nfs_iodlist_lock);
					continue;
				}
				LIST_REMOVE(myiod, nid_idle);
				mutex_exit(&nfs_iodlist_lock);
				goto quit;
			}
			cv_wait(&myiod->nid_cv, &myiod->nid_lock);
		}
		mutex_exit(&myiod->nid_lock);

		mutex_enter(&nmp->nm_lock);
		while ((bp = TAILQ_FIRST(&nmp->nm_bufq)) != NULL) {
			/* Take one off the front of the list */
			TAILQ_REMOVE(&nmp->nm_bufq, bp, b_freelist);
			nmp->nm_bufqlen--;
			if (nmp->nm_bufqlen < 2 * nmp->nm_bufqiods) {
				cv_broadcast(&nmp->nm_aiocv);
			}
			mutex_exit(&nmp->nm_lock);
			KERNEL_LOCK(1, curlwp);
			(void)nfs_doio(bp);
			KERNEL_UNLOCK_LAST(curlwp);
			mutex_enter(&nmp->nm_lock);
			/*
			 * If there are more than one iod on this mount, 
			 * then defect so that the iods can be shared out
			 * fairly between the mounts
			 */
			if (nfs_defect && nmp->nm_bufqiods > 1) {
				break;
			}
		}
		KASSERT(nmp->nm_bufqiods > 0);
		nmp->nm_bufqiods--;
		mutex_exit(&nmp->nm_lock);
	}
quit:
	KASSERT(myiod->nid_mount == NULL);
	mutex_exit(&myiod->nid_lock);

	cv_destroy(&myiod->nid_cv);
	mutex_destroy(&myiod->nid_lock);
	kmem_free(myiod, sizeof(*myiod));

	kthread_exit(0);
}

void
nfs_iodinit(void)
{

	mutex_init(&nfs_iodlist_lock, MUTEX_DEFAULT, IPL_NONE);
	LIST_INIT(&nfs_iodlist_all);
	LIST_INIT(&nfs_iodlist_idle);
}

void
nfs_iodfini(void)
{
	int error __diagused;

	error = nfs_set_niothreads(0);
	KASSERT(error == 0);
	mutex_destroy(&nfs_iodlist_lock);
}

int
nfs_iodbusy(struct nfsmount *nmp)
{
	struct nfs_iod *iod;
	int ret = 0;

	mutex_enter(&nfs_iodlist_lock);
	LIST_FOREACH(iod, &nfs_iodlist_all, nid_all) {
		if (iod->nid_mount == nmp)
			ret++;
	}
	mutex_exit(&nfs_iodlist_lock);

	return ret;
}

int
nfs_set_niothreads(int newval)
{
	struct nfs_iod *nid;
	int error = 0;
        int hold_count;

	KERNEL_UNLOCK_ALL(curlwp, &hold_count);

	mutex_enter(&nfs_iodlist_lock);
	/* clamp to sane range */
	nfs_niothreads = max(0, min(newval, NFS_MAXASYNCDAEMON));

	while (nfs_numasync != nfs_niothreads && error == 0) {
		while (nfs_numasync < nfs_niothreads) {

			/*
			 * kthread_create can wait for pagedaemon and
			 * pagedaemon can wait for nfsiod which needs to acquire
			 * nfs_iodlist_lock.
			 */

			mutex_exit(&nfs_iodlist_lock);
			error = kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL,
			    nfssvc_iod, NULL, NULL, "nfsio");
			mutex_enter(&nfs_iodlist_lock);
			if (error) {
				/* give up */
				nfs_niothreads = nfs_numasync;
				break;
			}
			nfs_numasync++;
		}
		while (nfs_numasync > nfs_niothreads) {
			nid = LIST_FIRST(&nfs_iodlist_all);
			if (nid == NULL) {
				/* iod has not started yet. */
				kpause("nfsiorm", false, hz, &nfs_iodlist_lock);
				continue;
			}
			LIST_REMOVE(nid, nid_all);
			mutex_enter(&nid->nid_lock);
			KASSERT(!nid->nid_exiting);
			nid->nid_exiting = true;
			cv_signal(&nid->nid_cv);
			mutex_exit(&nid->nid_lock);
			nfs_numasync--;
		}
	}
	mutex_exit(&nfs_iodlist_lock);

	KERNEL_LOCK(hold_count, curlwp);
	return error;
}

/*
 * Get an authorization string for the uid by having the mount_nfs sitting
 * on this mount point porpous out of the kernel and do it.
 */
int
nfs_getauth(struct nfsmount *nmp, struct nfsreq *rep, kauth_cred_t cred, char **auth_str, int *auth_len, char *verf_str, int *verf_len, NFSKERBKEY_T key)
	/* key:		 return session key */
{
	int error = 0;

	while ((nmp->nm_iflag & NFSMNT_WAITAUTH) == 0) {
		nmp->nm_iflag |= NFSMNT_WANTAUTH;
		(void) tsleep((void *)&nmp->nm_authtype, PSOCK,
			"nfsauth1", 2 * hz);
		error = nfs_sigintr(nmp, rep, rep->r_lwp);
		if (error) {
			nmp->nm_iflag &= ~NFSMNT_WANTAUTH;
			return (error);
		}
	}
	nmp->nm_iflag &= ~(NFSMNT_WAITAUTH | NFSMNT_WANTAUTH);
	nmp->nm_authstr = *auth_str = (char *)malloc(RPCAUTH_MAXSIZ, M_TEMP, M_WAITOK);
	nmp->nm_authlen = RPCAUTH_MAXSIZ;
	nmp->nm_verfstr = verf_str;
	nmp->nm_verflen = *verf_len;
	nmp->nm_authuid = kauth_cred_geteuid(cred);
	wakeup((void *)&nmp->nm_authstr);

	/*
	 * And wait for mount_nfs to do its stuff.
	 */
	while ((nmp->nm_iflag & NFSMNT_HASAUTH) == 0 && error == 0) {
		(void) tsleep((void *)&nmp->nm_authlen, PSOCK,
			"nfsauth2", 2 * hz);
		error = nfs_sigintr(nmp, rep, rep->r_lwp);
	}
	if (nmp->nm_iflag & NFSMNT_AUTHERR) {
		nmp->nm_iflag &= ~NFSMNT_AUTHERR;
		error = EAUTH;
	}
	if (error)
		free((void *)*auth_str, M_TEMP);
	else {
		*auth_len = nmp->nm_authlen;
		*verf_len = nmp->nm_verflen;
		memcpy(key, nmp->nm_key, sizeof (NFSKERBKEY_T));
	}
	nmp->nm_iflag &= ~NFSMNT_HASAUTH;
	nmp->nm_iflag |= NFSMNT_WAITAUTH;
	if (nmp->nm_iflag & NFSMNT_WANTAUTH) {
		nmp->nm_iflag &= ~NFSMNT_WANTAUTH;
		wakeup((void *)&nmp->nm_authtype);
	}
	return (error);
}

/*
 * Get a nickname authenticator and verifier.
 */
int
nfs_getnickauth(struct nfsmount *nmp, kauth_cred_t cred, char **auth_str,
    int *auth_len, char *verf_str, int verf_len)
{
#ifdef NFSKERB
	struct timeval ktvin;
#endif
	struct timeval ktvout, tv;
	struct nfsuid *nuidp;
	u_int32_t *nickp, *verfp;

	memset(&ktvout, 0, sizeof ktvout);	/* XXX gcc */

#ifdef DIAGNOSTIC
	if (verf_len < (4 * NFSX_UNSIGNED))
		panic("nfs_getnickauth verf too small");
#endif
	LIST_FOREACH(nuidp, NMUIDHASH(nmp, kauth_cred_geteuid(cred)), nu_hash) {
		if (kauth_cred_geteuid(nuidp->nu_cr) == kauth_cred_geteuid(cred))
			break;
	}
	if (!nuidp || nuidp->nu_expire < time_second)
		return (EACCES);

	/*
	 * Move to the end of the lru list (end of lru == most recently used).
	 */
	TAILQ_REMOVE(&nmp->nm_uidlruhead, nuidp, nu_lru);
	TAILQ_INSERT_TAIL(&nmp->nm_uidlruhead, nuidp, nu_lru);

	nickp = (u_int32_t *)malloc(2 * NFSX_UNSIGNED, M_TEMP, M_WAITOK);
	*nickp++ = txdr_unsigned(RPCAKN_NICKNAME);
	*nickp = txdr_unsigned(nuidp->nu_nickname);
	*auth_str = (char *)nickp;
	*auth_len = 2 * NFSX_UNSIGNED;

	/*
	 * Now we must encrypt the verifier and package it up.
	 */
	verfp = (u_int32_t *)verf_str;
	*verfp++ = txdr_unsigned(RPCAKN_NICKNAME);
	getmicrotime(&tv);
	if (tv.tv_sec > nuidp->nu_timestamp.tv_sec ||
	    (tv.tv_sec == nuidp->nu_timestamp.tv_sec &&
	     tv.tv_usec > nuidp->nu_timestamp.tv_usec))
		nuidp->nu_timestamp = tv;
	else
		nuidp->nu_timestamp.tv_usec++;
#ifdef NFSKERB
	ktvin.tv_sec = txdr_unsigned(nuidp->nu_timestamp.tv_sec);
	ktvin.tv_usec = txdr_unsigned(nuidp->nu_timestamp.tv_usec);

	/*
	 * Now encrypt the timestamp verifier in ecb mode using the session
	 * key.
	 */
	XXX
#endif

	*verfp++ = ktvout.tv_sec;
	*verfp++ = ktvout.tv_usec;
	*verfp = 0;
	return (0);
}

/*
 * Save the current nickname in a hash list entry on the mount point.
 */
int
nfs_savenickauth(struct nfsmount *nmp, kauth_cred_t cred, int len, NFSKERBKEY_T key, struct mbuf **mdp, char **dposp, struct mbuf *mrep)
{
	struct nfsuid *nuidp;
	u_int32_t *tl;
	int32_t t1;
	struct mbuf *md = *mdp;
	struct timeval ktvin, ktvout;
	u_int32_t nick;
	char *dpos = *dposp, *cp2;
	int deltasec, error = 0;

	memset(&ktvout, 0, sizeof ktvout);	 /* XXX gcc */

	if (len == (3 * NFSX_UNSIGNED)) {
		nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		ktvin.tv_sec = *tl++;
		ktvin.tv_usec = *tl++;
		nick = fxdr_unsigned(u_int32_t, *tl);

		/*
		 * Decrypt the timestamp in ecb mode.
		 */
#ifdef NFSKERB
		XXX
#else
		(void)ktvin.tv_sec;
#endif
		ktvout.tv_sec = fxdr_unsigned(long, ktvout.tv_sec);
		ktvout.tv_usec = fxdr_unsigned(long, ktvout.tv_usec);
		deltasec = time_second - ktvout.tv_sec;
		if (deltasec < 0)
			deltasec = -deltasec;
		/*
		 * If ok, add it to the hash list for the mount point.
		 */
		if (deltasec <= NFS_KERBCLOCKSKEW) {
			if (nmp->nm_numuids < nuidhash_max) {
				nmp->nm_numuids++;
				nuidp = kmem_alloc(sizeof(*nuidp), KM_SLEEP);
			} else {
				nuidp = TAILQ_FIRST(&nmp->nm_uidlruhead);
				LIST_REMOVE(nuidp, nu_hash);
				TAILQ_REMOVE(&nmp->nm_uidlruhead, nuidp,
					nu_lru);
			}
			nuidp->nu_flag = 0;
			kauth_cred_seteuid(nuidp->nu_cr, kauth_cred_geteuid(cred));
			nuidp->nu_expire = time_second + NFS_KERBTTL;
			nuidp->nu_timestamp = ktvout;
			nuidp->nu_nickname = nick;
			memcpy(nuidp->nu_key, key, sizeof (NFSKERBKEY_T));
			TAILQ_INSERT_TAIL(&nmp->nm_uidlruhead, nuidp,
				nu_lru);
			LIST_INSERT_HEAD(NMUIDHASH(nmp, kauth_cred_geteuid(cred)),
				nuidp, nu_hash);
		}
	} else
		nfsm_adv(nfsm_rndup(len));
nfsmout:
	*mdp = md;
	*dposp = dpos;
	return (error);
}
