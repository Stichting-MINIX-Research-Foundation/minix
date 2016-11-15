/*	$NetBSD: nfs_srvsocket.c,v 1.4 2009/09/03 20:59:12 tls Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993, 1995
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
 *	@(#)nfs_socket.c	8.5 (Berkeley) 3/30/95
 */

/*
 * Socket operations for use by nfs
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_srvsocket.c,v 1.4 2009/09/03 20:59:12 tls Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/evcnt.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/vnode.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/tprintf.h>
#include <sys/namei.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsrtt.h>
#include <nfs/nfs_var.h>

static void nfsrv_wakenfsd_locked(struct nfssvc_sock *);

int (*nfsrv3_procs[NFS_NPROCS])(struct nfsrv_descript *,
				    struct nfssvc_sock *, struct lwp *,
				    struct mbuf **) = {
	nfsrv_null,
	nfsrv_getattr,
	nfsrv_setattr,
	nfsrv_lookup,
	nfsrv3_access,
	nfsrv_readlink,
	nfsrv_read,
	nfsrv_write,
	nfsrv_create,
	nfsrv_mkdir,
	nfsrv_symlink,
	nfsrv_mknod,
	nfsrv_remove,
	nfsrv_rmdir,
	nfsrv_rename,
	nfsrv_link,
	nfsrv_readdir,
	nfsrv_readdirplus,
	nfsrv_statfs,
	nfsrv_fsinfo,
	nfsrv_pathconf,
	nfsrv_commit,
	nfsrv_noop
};

/*
 * Socket upcall routine for the nfsd sockets.
 * The void *arg is a pointer to the "struct nfssvc_sock".
 */
void
nfsrv_soupcall(struct socket *so, void *arg, int events, int waitflag)
{
	struct nfssvc_sock *slp = (struct nfssvc_sock *)arg;

	nfsdsock_setbits(slp, SLP_A_NEEDQ);
	nfsrv_wakenfsd(slp);
}

void
nfsrv_rcv(struct nfssvc_sock *slp)
{
	struct socket *so;
	struct mbuf *m;
	struct mbuf *mp, *nam;
	struct uio auio;
	int flags;
	int error;
	int setflags = 0;

	error = nfsdsock_lock(slp, true);
	if (error) {
		setflags |= SLP_A_NEEDQ;
		goto dorecs_unlocked;
	}

	nfsdsock_clearbits(slp, SLP_A_NEEDQ);

	so = slp->ns_so;
	if (so->so_type == SOCK_STREAM) {
		/*
		 * Do soreceive().
		 */
		auio.uio_resid = 1000000000;
		/* not need to setup uio_vmspace */
		flags = MSG_DONTWAIT;
		error = (*so->so_receive)(so, &nam, &auio, &mp, NULL, &flags);
		if (error || mp == NULL) {
			if (error == EWOULDBLOCK)
				setflags |= SLP_A_NEEDQ;
			else
				setflags |= SLP_A_DISCONN;
			goto dorecs;
		}
		m = mp;
		m_claimm(m, &nfs_mowner);
		if (slp->ns_rawend) {
			slp->ns_rawend->m_next = m;
			slp->ns_cc += 1000000000 - auio.uio_resid;
		} else {
			slp->ns_raw = m;
			slp->ns_cc = 1000000000 - auio.uio_resid;
		}
		while (m->m_next)
			m = m->m_next;
		slp->ns_rawend = m;

		/*
		 * Now try and parse record(s) out of the raw stream data.
		 */
		error = nfsrv_getstream(slp, M_WAIT);
		if (error) {
			if (error == EPERM)
				setflags |= SLP_A_DISCONN;
			else
				setflags |= SLP_A_NEEDQ;
		}
	} else {
		do {
			auio.uio_resid = 1000000000;
			/* not need to setup uio_vmspace */
			flags = MSG_DONTWAIT;
			error = (*so->so_receive)(so, &nam, &auio, &mp, NULL,
			    &flags);
			if (mp) {
				if (nam) {
					m = nam;
					m->m_next = mp;
				} else
					m = mp;
				m_claimm(m, &nfs_mowner);
				if (slp->ns_recend)
					slp->ns_recend->m_nextpkt = m;
				else
					slp->ns_rec = m;
				slp->ns_recend = m;
				m->m_nextpkt = (struct mbuf *)0;
			}
			if (error) {
				if ((so->so_proto->pr_flags & PR_CONNREQUIRED)
				    && error != EWOULDBLOCK) {
					setflags |= SLP_A_DISCONN;
					goto dorecs;
				}
			}
		} while (mp);
	}
dorecs:
	nfsdsock_unlock(slp);

dorecs_unlocked:
	if (setflags) {
		nfsdsock_setbits(slp, setflags);
	}
}

int
nfsdsock_lock(struct nfssvc_sock *slp, bool waitok)
{

	mutex_enter(&slp->ns_lock);
	while ((~slp->ns_flags & (SLP_BUSY|SLP_VALID)) == 0) {
		if (!waitok) {
			mutex_exit(&slp->ns_lock);
			return EWOULDBLOCK;
		}
		cv_wait(&slp->ns_cv, &slp->ns_lock);
	}
	if ((slp->ns_flags & SLP_VALID) == 0) {
		mutex_exit(&slp->ns_lock);
		return EINVAL;
	}
	KASSERT((slp->ns_flags & SLP_BUSY) == 0);
	slp->ns_flags |= SLP_BUSY;
	mutex_exit(&slp->ns_lock);

	return 0;
}

void
nfsdsock_unlock(struct nfssvc_sock *slp)
{

	mutex_enter(&slp->ns_lock);
	KASSERT((slp->ns_flags & SLP_BUSY) != 0);
	cv_broadcast(&slp->ns_cv);
	slp->ns_flags &= ~SLP_BUSY;
	mutex_exit(&slp->ns_lock);
}

int
nfsdsock_drain(struct nfssvc_sock *slp)
{
	int error = 0;

	mutex_enter(&slp->ns_lock);
	if ((slp->ns_flags & SLP_VALID) == 0) {
		error = EINVAL;
		goto done;
	}
	slp->ns_flags &= ~SLP_VALID;
	while ((slp->ns_flags & SLP_BUSY) != 0) {
		cv_wait(&slp->ns_cv, &slp->ns_lock);
	}
done:
	mutex_exit(&slp->ns_lock);

	return error;
}

/*
 * Try and extract an RPC request from the mbuf data list received on a
 * stream socket. The "waitflag" argument indicates whether or not it
 * can sleep.
 */
int
nfsrv_getstream(struct nfssvc_sock *slp, int waitflag)
{
	struct mbuf *m, **mpp;
	struct mbuf *recm;
	u_int32_t recmark;
	int error = 0;

	KASSERT((slp->ns_flags & SLP_BUSY) != 0);
	for (;;) {
		if (slp->ns_reclen == 0) {
			if (slp->ns_cc < NFSX_UNSIGNED) {
				break;
			}
			m = slp->ns_raw;
			m_copydata(m, 0, NFSX_UNSIGNED, (void *)&recmark);
			m_adj(m, NFSX_UNSIGNED);
			slp->ns_cc -= NFSX_UNSIGNED;
			recmark = ntohl(recmark);
			slp->ns_reclen = recmark & ~0x80000000;
			if (recmark & 0x80000000)
				slp->ns_sflags |= SLP_S_LASTFRAG;
			else
				slp->ns_sflags &= ~SLP_S_LASTFRAG;
			if (slp->ns_reclen > NFS_MAXPACKET) {
				error = EPERM;
				break;
			}
		}

		/*
		 * Now get the record part.
		 *
		 * Note that slp->ns_reclen may be 0.  Linux sometimes
		 * generates 0-length records.
		 */
		if (slp->ns_cc == slp->ns_reclen) {
			recm = slp->ns_raw;
			slp->ns_raw = slp->ns_rawend = (struct mbuf *)0;
			slp->ns_cc = slp->ns_reclen = 0;
		} else if (slp->ns_cc > slp->ns_reclen) {
			recm = slp->ns_raw;
			m = m_split(recm, slp->ns_reclen, waitflag);
			if (m == NULL) {
				error = EWOULDBLOCK;
				break;
			}
			m_claimm(recm, &nfs_mowner);
			slp->ns_raw = m;
			if (m->m_next == NULL)
				slp->ns_rawend = m;
			slp->ns_cc -= slp->ns_reclen;
			slp->ns_reclen = 0;
		} else {
			break;
		}

		/*
		 * Accumulate the fragments into a record.
		 */
		mpp = &slp->ns_frag;
		while (*mpp)
			mpp = &((*mpp)->m_next);
		*mpp = recm;
		if (slp->ns_sflags & SLP_S_LASTFRAG) {
			if (slp->ns_recend)
				slp->ns_recend->m_nextpkt = slp->ns_frag;
			else
				slp->ns_rec = slp->ns_frag;
			slp->ns_recend = slp->ns_frag;
			slp->ns_frag = NULL;
		}
	}

	return error;
}

/*
 * Parse an RPC header.
 */
int
nfsrv_dorec(struct nfssvc_sock *slp, struct nfsd *nfsd,
    struct nfsrv_descript **ndp, bool *more)
{
	struct mbuf *m, *nam;
	struct nfsrv_descript *nd;
	int error;

	*ndp = NULL;
	*more = false;

	if (nfsdsock_lock(slp, true)) {
		return ENOBUFS;
	}
	m = slp->ns_rec;
	if (m == NULL) {
		nfsdsock_unlock(slp);
		return ENOBUFS;
	}
	slp->ns_rec = m->m_nextpkt;
	if (slp->ns_rec) {
		m->m_nextpkt = NULL;
		*more = true;
	} else {
		slp->ns_recend = NULL;
	}
	nfsdsock_unlock(slp);

	if (m->m_type == MT_SONAME) {
		nam = m;
		m = m->m_next;
		nam->m_next = NULL;
	} else
		nam = NULL;
	nd = nfsdreq_alloc();
	nd->nd_md = nd->nd_mrep = m;
	nd->nd_nam2 = nam;
	nd->nd_dpos = mtod(m, void *);
	error = nfs_getreq(nd, nfsd, true);
	if (error) {
		m_freem(nam);
		nfsdreq_free(nd);
		return (error);
	}
	*ndp = nd;
	nfsd->nfsd_nd = nd;
	return (0);
}

bool
nfsrv_timer(void)
{
	struct timeval tv;
	struct nfssvc_sock *slp;
	u_quad_t cur_usec;
	struct nfsrv_descript *nd;
	bool more;

	/*
	 * Scan the write gathering queues for writes that need to be
	 * completed now.
	 */
	getmicrotime(&tv);
	cur_usec = (u_quad_t)tv.tv_sec * 1000000 + (u_quad_t)tv.tv_usec;
	more = false;
	mutex_enter(&nfsd_lock);
	TAILQ_FOREACH(slp, &nfssvc_sockhead, ns_chain) {
		nd = LIST_FIRST(&slp->ns_tq);
		if (nd != NULL) {
			if (nd->nd_time <= cur_usec) {
				nfsrv_wakenfsd_locked(slp);
			}
			more = true;
		}
	}
	mutex_exit(&nfsd_lock);
	return more;
}

/*
 * Search for a sleeping nfsd and wake it up.
 * SIDE EFFECT: If none found, set NFSD_CHECKSLP flag, so that one of the
 * running nfsds will go look for the work in the nfssvc_sock list.
 */
static void
nfsrv_wakenfsd_locked(struct nfssvc_sock *slp)
{
	struct nfsd *nd;

	KASSERT(mutex_owned(&nfsd_lock));

	if ((slp->ns_flags & SLP_VALID) == 0)
		return;
	if (slp->ns_gflags & SLP_G_DOREC)
		return;
	nd = SLIST_FIRST(&nfsd_idle_head);
	if (nd) {
		SLIST_REMOVE_HEAD(&nfsd_idle_head, nfsd_idle);
		if (nd->nfsd_slp)
			panic("nfsd wakeup");
		slp->ns_sref++;
		KASSERT(slp->ns_sref > 0);
		nd->nfsd_slp = slp;
		cv_signal(&nd->nfsd_cv);
	} else {
		slp->ns_gflags |= SLP_G_DOREC;
		nfsd_head_flag |= NFSD_CHECKSLP;
		TAILQ_INSERT_TAIL(&nfssvc_sockpending, slp, ns_pending);
	}
}

void
nfsrv_wakenfsd(struct nfssvc_sock *slp)
{

	mutex_enter(&nfsd_lock);
	nfsrv_wakenfsd_locked(slp);
	mutex_exit(&nfsd_lock);
}

int
nfsdsock_sendreply(struct nfssvc_sock *slp, struct nfsrv_descript *nd)
{
	int error;

	if (nd->nd_mrep != NULL) {
		m_freem(nd->nd_mrep);
		nd->nd_mrep = NULL;
	}

	mutex_enter(&slp->ns_lock);
	if ((slp->ns_flags & SLP_SENDING) != 0) {
		SIMPLEQ_INSERT_TAIL(&slp->ns_sendq, nd, nd_sendq);
		mutex_exit(&slp->ns_lock);
		return 0;
	}
	KASSERT(SIMPLEQ_EMPTY(&slp->ns_sendq));
	slp->ns_flags |= SLP_SENDING;
	mutex_exit(&slp->ns_lock);

again:
	error = nfs_send(slp->ns_so, nd->nd_nam2, nd->nd_mreq, NULL, curlwp);
	if (nd->nd_nam2) {
		m_free(nd->nd_nam2);
	}
	nfsdreq_free(nd);

	mutex_enter(&slp->ns_lock);
	KASSERT((slp->ns_flags & SLP_SENDING) != 0);
	nd = SIMPLEQ_FIRST(&slp->ns_sendq);
	if (nd != NULL) {
		SIMPLEQ_REMOVE_HEAD(&slp->ns_sendq, nd_sendq);
		mutex_exit(&slp->ns_lock);
		goto again;
	}
	slp->ns_flags &= ~SLP_SENDING;
	mutex_exit(&slp->ns_lock);

	return error;
}

void
nfsdsock_setbits(struct nfssvc_sock *slp, int bits)
{

	mutex_enter(&slp->ns_alock);
	slp->ns_aflags |= bits;
	mutex_exit(&slp->ns_alock);
}

void
nfsdsock_clearbits(struct nfssvc_sock *slp, int bits)
{

	mutex_enter(&slp->ns_alock);
	slp->ns_aflags &= ~bits;
	mutex_exit(&slp->ns_alock);
}

bool
nfsdsock_testbits(struct nfssvc_sock *slp, int bits)
{

	return (slp->ns_aflags & bits);
}
