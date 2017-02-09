/*	$NetBSD: nfs_clntsocket.c,v 1.3 2015/07/15 03:28:55 manu Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: nfs_clntsocket.c,v 1.3 2015/07/15 03:28:55 manu Exp $");

#ifdef _KERNEL_OPT
#include "opt_nfs.h"
#include "opt_mbuftrace.h"
#endif

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

static int nfs_sndlock(struct nfsmount *, struct nfsreq *);
static void nfs_sndunlock(struct nfsmount *);

/*
 * Receive a Sun RPC Request/Reply. For SOCK_DGRAM, the work is all
 * done by soreceive(), but for SOCK_STREAM we must deal with the Record
 * Mark and consolidate the data into a new mbuf list.
 * nb: Sometimes TCP passes the data up to soreceive() in long lists of
 *     small mbufs.
 * For SOCK_STREAM we must be very careful to read an entire record once
 * we have read any of it, even if the system call has been interrupted.
 */
static int
nfs_receive(struct nfsreq *rep, struct mbuf **aname, struct mbuf **mp,
    struct lwp *l)
{
	struct socket *so;
	struct uio auio;
	struct iovec aio;
	struct mbuf *m;
	struct mbuf *control;
	u_int32_t len;
	struct mbuf **getnam;
	int error, sotype, rcvflg;

	/*
	 * Set up arguments for soreceive()
	 */
	*mp = NULL;
	*aname = NULL;
	sotype = rep->r_nmp->nm_sotype;

	/*
	 * For reliable protocols, lock against other senders/receivers
	 * in case a reconnect is necessary.
	 * For SOCK_STREAM, first get the Record Mark to find out how much
	 * more there is to get.
	 * We must lock the socket against other receivers
	 * until we have an entire rpc request/reply.
	 */
	if (sotype != SOCK_DGRAM) {
		error = nfs_sndlock(rep->r_nmp, rep);
		if (error)
			return (error);
tryagain:
		/*
		 * Check for fatal errors and resending request.
		 */
		/*
		 * Ugh: If a reconnect attempt just happened, nm_so
		 * would have changed. NULL indicates a failed
		 * attempt that has essentially shut down this
		 * mount point.
		 */
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM)) {
			nfs_sndunlock(rep->r_nmp);
			return (EINTR);
		}
		so = rep->r_nmp->nm_so;
		if (!so) {
			error = nfs_reconnect(rep);
			if (error) {
				nfs_sndunlock(rep->r_nmp);
				return (error);
			}
			goto tryagain;
		}
		while (rep->r_flags & R_MUSTRESEND) {
			m = m_copym(rep->r_mreq, 0, M_COPYALL, M_WAIT);
			nfsstats.rpcretries++;
			rep->r_rtt = 0;
			rep->r_flags &= ~R_TIMING;
			error = nfs_send(so, rep->r_nmp->nm_nam, m, rep, l);
			if (error) {
				if (error == EINTR || error == ERESTART ||
				    (error = nfs_reconnect(rep)) != 0) {
					nfs_sndunlock(rep->r_nmp);
					return (error);
				}
				goto tryagain;
			}
		}
		nfs_sndunlock(rep->r_nmp);
		if (sotype == SOCK_STREAM) {
			aio.iov_base = (void *) &len;
			aio.iov_len = sizeof(u_int32_t);
			auio.uio_iov = &aio;
			auio.uio_iovcnt = 1;
			auio.uio_rw = UIO_READ;
			auio.uio_offset = 0;
			auio.uio_resid = sizeof(u_int32_t);
			UIO_SETUP_SYSSPACE(&auio);
			do {
			   rcvflg = MSG_WAITALL;
			   error = (*so->so_receive)(so, NULL, &auio,
				NULL, NULL, &rcvflg);
			   if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
				/*
				 * if it seems that the server died after it
				 * received our request, set EPIPE so that
				 * we'll reconnect and retransmit requests.
				 */
				if (rep->r_rexmit >= rep->r_nmp->nm_retry) {
					nfsstats.rpctimeouts++;
					error = EPIPE;
				}
			   }
			} while (error == EWOULDBLOCK);
			if (!error && auio.uio_resid > 0) {
			    /*
			     * Don't log a 0 byte receive; it means
			     * that the socket has been closed, and
			     * can happen during normal operation
			     * (forcible unmount or Solaris server).
			     */
			    if (auio.uio_resid != sizeof (u_int32_t))
			      log(LOG_INFO,
				 "short receive (%lu/%lu) from nfs server %s\n",
				 (u_long)sizeof(u_int32_t) - auio.uio_resid,
				 (u_long)sizeof(u_int32_t),
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EPIPE;
			}
			if (error)
				goto errout;
			len = ntohl(len) & ~0x80000000;
			/*
			 * This is SERIOUS! We are out of sync with the sender
			 * and forcing a disconnect/reconnect is all I can do.
			 */
			if (len > NFS_MAXPACKET) {
			    log(LOG_ERR, "%s (%d) from nfs server %s\n",
				"impossible packet length",
				len,
				rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EFBIG;
			    goto errout;
			}
			auio.uio_resid = len;
			do {
			    rcvflg = MSG_WAITALL;
			    error =  (*so->so_receive)(so, NULL,
				&auio, mp, NULL, &rcvflg);
			} while (error == EWOULDBLOCK || error == EINTR ||
				 error == ERESTART);
			if (!error && auio.uio_resid > 0) {
			    if (len != auio.uio_resid)
			      log(LOG_INFO,
				"short receive (%lu/%d) from nfs server %s\n",
				(u_long)len - auio.uio_resid, len,
				rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EPIPE;
			}
		} else {
			/*
			 * NB: Since uio_resid is big, MSG_WAITALL is ignored
			 * and soreceive() will return when it has either a
			 * control msg or a data msg.
			 * We have no use for control msg., but must grab them
			 * and then throw them away so we know what is going
			 * on.
			 */
			auio.uio_resid = len = 100000000; /* Anything Big */
			/* not need to setup uio_vmspace */
			do {
			    rcvflg = 0;
			    error =  (*so->so_receive)(so, NULL,
				&auio, mp, &control, &rcvflg);
			    if (control)
				m_freem(control);
			    if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
			    }
			} while (error == EWOULDBLOCK ||
				 (!error && *mp == NULL && control));
			if ((rcvflg & MSG_EOR) == 0)
				printf("Egad!!\n");
			if (!error && *mp == NULL)
				error = EPIPE;
			len -= auio.uio_resid;
		}
errout:
		if (error && error != EINTR && error != ERESTART) {
			m_freem(*mp);
			*mp = NULL;
			if (error != EPIPE)
				log(LOG_INFO,
				    "receive error %d from nfs server %s\n",
				    error,
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			error = nfs_sndlock(rep->r_nmp, rep);
			if (!error)
				error = nfs_reconnect(rep);
			if (!error)
				goto tryagain;
			else
				nfs_sndunlock(rep->r_nmp);
		}
	} else {
		if ((so = rep->r_nmp->nm_so) == NULL)
			return (EACCES);
		if (so->so_state & SS_ISCONNECTED)
			getnam = NULL;
		else
			getnam = aname;
		auio.uio_resid = len = 1000000;
		/* not need to setup uio_vmspace */
		do {
			rcvflg = 0;
			error =  (*so->so_receive)(so, getnam, &auio, mp,
				NULL, &rcvflg);
			if (error == EWOULDBLOCK &&
			    (rep->r_flags & R_SOFTTERM))
				return (EINTR);
		} while (error == EWOULDBLOCK);
		len -= auio.uio_resid;
		if (!error && *mp == NULL)
			error = EPIPE;
	}
	if (error) {
		m_freem(*mp);
		*mp = NULL;
	}
	return (error);
}

/*
 * Implement receipt of reply on a socket.
 * We must search through the list of received datagrams matching them
 * with outstanding requests using the xid, until ours is found.
 */
/* ARGSUSED */
static int
nfs_reply(struct nfsreq *myrep, struct lwp *lwp)
{
	struct nfsreq *rep;
	struct nfsmount *nmp = myrep->r_nmp;
	int32_t t1;
	struct mbuf *mrep, *nam, *md;
	u_int32_t rxid, *tl;
	char *dpos, *cp2;
	int error;

	/*
	 * Loop around until we get our own reply
	 */
	for (;;) {
		/*
		 * Lock against other receivers so that I don't get stuck in
		 * sbwait() after someone else has received my reply for me.
		 * Also necessary for connection based protocols to avoid
		 * race conditions during a reconnect.
		 */
		error = nfs_rcvlock(nmp, myrep);
		if (error == EALREADY)
			return (0);
		if (error)
			return (error);
		/*
		 * Get the next Rpc reply off the socket
		 */

		mutex_enter(&nmp->nm_lock);
		nmp->nm_waiters++;
		mutex_exit(&nmp->nm_lock);

		error = nfs_receive(myrep, &nam, &mrep, lwp);

		mutex_enter(&nmp->nm_lock);
		nmp->nm_waiters--;
		cv_signal(&nmp->nm_disconcv);
		mutex_exit(&nmp->nm_lock);

		if (error) {
			nfs_rcvunlock(nmp);

			if (nmp->nm_iflag & NFSMNT_DISMNT) {
				/*
				 * Oops, we're going away now..
				 */
				return error;
			}
			/*
			 * Ignore routing errors on connectionless protocols? ?
			 */
			if (NFSIGNORE_SOERROR(nmp->nm_soflags, error)) {
				nmp->nm_so->so_error = 0;
#ifdef DEBUG
				if (ratecheck(&nfs_reply_last_err_time,
				    &nfs_err_interval))
					printf("%s: ignoring error %d\n",
					       __func__, error);
#endif
				continue;
			}
			return (error);
		}
		if (nam)
			m_freem(nam);

		/*
		 * Get the xid and check that it is an rpc reply
		 */
		md = mrep;
		dpos = mtod(md, void *);
		nfsm_dissect(tl, u_int32_t *, 2*NFSX_UNSIGNED);
		rxid = *tl++;
		if (*tl != rpc_reply) {
			nfsstats.rpcinvalid++;
			m_freem(mrep);
nfsmout:
			nfs_rcvunlock(nmp);
			continue;
		}

		/*
		 * Loop through the request list to match up the reply
		 * Iff no match, just drop the datagram
		 */
		TAILQ_FOREACH(rep, &nfs_reqq, r_chain) {
			if (rep->r_mrep == NULL && rxid == rep->r_xid) {
				/* Found it.. */
				rep->r_mrep = mrep;
				rep->r_md = md;
				rep->r_dpos = dpos;
				if (nfsrtton) {
					struct rttl *rt;

					rt = &nfsrtt.rttl[nfsrtt.pos];
					rt->proc = rep->r_procnum;
					rt->rto = NFS_RTO(nmp, nfs_proct[rep->r_procnum]);
					rt->sent = nmp->nm_sent;
					rt->cwnd = nmp->nm_cwnd;
					rt->srtt = nmp->nm_srtt[nfs_proct[rep->r_procnum] - 1];
					rt->sdrtt = nmp->nm_sdrtt[nfs_proct[rep->r_procnum] - 1];
					rt->fsid = nmp->nm_mountp->mnt_stat.f_fsidx;
					getmicrotime(&rt->tstamp);
					if (rep->r_flags & R_TIMING)
						rt->rtt = rep->r_rtt;
					else
						rt->rtt = 1000000;
					nfsrtt.pos = (nfsrtt.pos + 1) % NFSRTTLOGSIZ;
				}
				/*
				 * Update congestion window.
				 * Do the additive increase of
				 * one rpc/rtt.
				 */
				if (nmp->nm_cwnd <= nmp->nm_sent) {
					nmp->nm_cwnd +=
					   (NFS_CWNDSCALE * NFS_CWNDSCALE +
					   (nmp->nm_cwnd >> 1)) / nmp->nm_cwnd;
					if (nmp->nm_cwnd > NFS_MAXCWND)
						nmp->nm_cwnd = NFS_MAXCWND;
				}
				rep->r_flags &= ~R_SENT;
				nmp->nm_sent -= NFS_CWNDSCALE;
				/*
				 * Update rtt using a gain of 0.125 on the mean
				 * and a gain of 0.25 on the deviation.
				 */
				if (rep->r_flags & R_TIMING) {
					/*
					 * Since the timer resolution of
					 * NFS_HZ is so course, it can often
					 * result in r_rtt == 0. Since
					 * r_rtt == N means that the actual
					 * rtt is between N+dt and N+2-dt ticks,
					 * add 1.
					 */
					t1 = rep->r_rtt + 1;
					t1 -= (NFS_SRTT(rep) >> 3);
					NFS_SRTT(rep) += t1;
					if (t1 < 0)
						t1 = -t1;
					t1 -= (NFS_SDRTT(rep) >> 2);
					NFS_SDRTT(rep) += t1;
				}
				nmp->nm_timeouts = 0;
				break;
			}
		}
		nfs_rcvunlock(nmp);
		/*
		 * If not matched to a request, drop it.
		 * If it's mine, get out.
		 */
		if (rep == 0) {
			nfsstats.rpcunexpected++;
			m_freem(mrep);
		} else if (rep == myrep) {
			if (rep->r_mrep == NULL)
				panic("nfsreply nil");
			return (0);
		}
	}
}

/*
 * nfs_request - goes something like this
 *	- fill in request struct
 *	- links it into list
 *	- calls nfs_send() for first transmit
 *	- calls nfs_receive() to get reply
 *	- break down rpc header and return with nfs reply pointed to
 *	  by mrep or error
 * nb: always frees up mreq mbuf list
 */
int
nfs_request(struct nfsnode *np, struct mbuf *mrest, int procnum, struct lwp *lwp, kauth_cred_t cred, struct mbuf **mrp, struct mbuf **mdp, char **dposp, int *rexmitp)
{
	struct mbuf *m, *mrep;
	struct nfsreq *rep;
	u_int32_t *tl;
	int i;
	struct nfsmount *nmp = VFSTONFS(np->n_vnode->v_mount);
	struct mbuf *md, *mheadend;
	char nickv[RPCX_NICKVERF];
	time_t waituntil;
	char *dpos, *cp2;
	int t1, s, error = 0, mrest_len, auth_len, auth_type;
	int trylater_delay = NFS_TRYLATERDEL, failed_auth = 0;
	int verf_len, verf_type;
	u_int32_t xid;
	char *auth_str, *verf_str;
	NFSKERBKEY_T key;		/* save session key */
	kauth_cred_t acred;
	struct mbuf *mrest_backup = NULL;
	kauth_cred_t origcred = NULL; /* XXX: gcc */
	bool retry_cred = true;
	bool use_opencred = (np->n_flag & NUSEOPENCRED) != 0;

	if (rexmitp != NULL)
		*rexmitp = 0;

	acred = kauth_cred_alloc();

tryagain_cred:
	KASSERT(cred != NULL);
	rep = kmem_alloc(sizeof(*rep), KM_SLEEP);
	rep->r_nmp = nmp;
	KASSERT(lwp == NULL || lwp == curlwp);
	rep->r_lwp = lwp;
	rep->r_procnum = procnum;
	i = 0;
	m = mrest;
	while (m) {
		i += m->m_len;
		m = m->m_next;
	}
	mrest_len = i;

	/*
	 * Get the RPC header with authorization.
	 */
kerbauth:
	verf_str = auth_str = NULL;
	if (nmp->nm_flag & NFSMNT_KERB) {
		verf_str = nickv;
		verf_len = sizeof (nickv);
		auth_type = RPCAUTH_KERB4;
		memset((void *)key, 0, sizeof (key));
		if (failed_auth || nfs_getnickauth(nmp, cred, &auth_str,
			&auth_len, verf_str, verf_len)) {
			error = nfs_getauth(nmp, rep, cred, &auth_str,
				&auth_len, verf_str, &verf_len, key);
			if (error) {
				kmem_free(rep, sizeof(*rep));
				m_freem(mrest);
				KASSERT(kauth_cred_getrefcnt(acred) == 1);
				kauth_cred_free(acred);
				return (error);
			}
		}
		retry_cred = false;
	} else {
		/* AUTH_UNIX */
		uid_t uid;
		gid_t gid;

		/*
		 * on the most unix filesystems, permission checks are
		 * done when the file is open(2)'ed.
		 * ie. once a file is successfully open'ed,
		 * following i/o operations never fail with EACCES.
		 * we try to follow the semantics as far as possible.
		 *
		 * note that we expect that the nfs server always grant
		 * accesses by the file's owner.
		 */
		origcred = cred;
		switch (procnum) {
		case NFSPROC_READ:
		case NFSPROC_WRITE:
		case NFSPROC_COMMIT:
			uid = np->n_vattr->va_uid;
			gid = np->n_vattr->va_gid;
			if (kauth_cred_geteuid(cred) == uid &&
			    kauth_cred_getegid(cred) == gid) {
				retry_cred = false;
				break;
			}
			if (use_opencred)
				break;
			kauth_cred_setuid(acred, uid);
			kauth_cred_seteuid(acred, uid);
			kauth_cred_setsvuid(acred, uid);
			kauth_cred_setgid(acred, gid);
			kauth_cred_setegid(acred, gid);
			kauth_cred_setsvgid(acred, gid);
			cred = acred;
			break;
		default:
			retry_cred = false;
			break;
		}
		/*
		 * backup mbuf chain if we can need it later to retry.
		 *
		 * XXX maybe we can keep a direct reference to
		 * mrest without doing m_copym, but it's ...ugly.
		 */
		if (retry_cred)
			mrest_backup = m_copym(mrest, 0, M_COPYALL, M_WAIT);
		auth_type = RPCAUTH_UNIX;
		/* XXX elad - ngroups */
		auth_len = (((kauth_cred_ngroups(cred) > nmp->nm_numgrps) ?
			nmp->nm_numgrps : kauth_cred_ngroups(cred)) << 2) +
			5 * NFSX_UNSIGNED;
	}
	m = nfsm_rpchead(cred, nmp->nm_flag, procnum, auth_type, auth_len,
	     auth_str, verf_len, verf_str, mrest, mrest_len, &mheadend, &xid);
	if (auth_str)
		free(auth_str, M_TEMP);

	/*
	 * For stream protocols, insert a Sun RPC Record Mark.
	 */
	if (nmp->nm_sotype == SOCK_STREAM) {
		M_PREPEND(m, NFSX_UNSIGNED, M_WAIT);
		*mtod(m, u_int32_t *) = htonl(0x80000000 |
			 (m->m_pkthdr.len - NFSX_UNSIGNED));
	}
	rep->r_mreq = m;
	rep->r_xid = xid;
tryagain:
	if (nmp->nm_flag & NFSMNT_SOFT)
		rep->r_retry = nmp->nm_retry;
	else
		rep->r_retry = NFS_MAXREXMIT + 1;	/* past clip limit */
	rep->r_rtt = rep->r_rexmit = 0;
	if (nfs_proct[procnum] > 0)
		rep->r_flags = R_TIMING;
	else
		rep->r_flags = 0;
	rep->r_mrep = NULL;

	/*
	 * Do the client side RPC.
	 */
	nfsstats.rpcrequests++;
	/*
	 * Chain request into list of outstanding requests. Be sure
	 * to put it LAST so timer finds oldest requests first.
	 */
	s = splsoftnet();
	TAILQ_INSERT_TAIL(&nfs_reqq, rep, r_chain);
	nfs_timer_start();

	/*
	 * If backing off another request or avoiding congestion, don't
	 * send this one now but let timer do it. If not timing a request,
	 * do it now.
	 */
	if (nmp->nm_so && (nmp->nm_sotype != SOCK_DGRAM ||
	    (nmp->nm_flag & NFSMNT_DUMBTIMR) || nmp->nm_sent < nmp->nm_cwnd)) {
		splx(s);
		if (nmp->nm_soflags & PR_CONNREQUIRED)
			error = nfs_sndlock(nmp, rep);
		if (!error) {
			m = m_copym(rep->r_mreq, 0, M_COPYALL, M_WAIT);
			error = nfs_send(nmp->nm_so, nmp->nm_nam, m, rep, lwp);
			if (nmp->nm_soflags & PR_CONNREQUIRED)
				nfs_sndunlock(nmp);
		}
		s = splsoftnet();
		if (!error && (rep->r_flags & R_MUSTRESEND) == 0) {
			if ((rep->r_flags & R_SENT) == 0) {
				nmp->nm_sent += NFS_CWNDSCALE;
				rep->r_flags |= R_SENT;
			}
		}
		splx(s);
	} else {
		splx(s);
		rep->r_rtt = -1;
	}

	/*
	 * Wait for the reply from our send or the timer's.
	 */
	if (!error || error == EPIPE || error == EWOULDBLOCK)
		error = nfs_reply(rep, lwp);

	/*
	 * RPC done, unlink the request.
	 */
	s = splsoftnet();
	TAILQ_REMOVE(&nfs_reqq, rep, r_chain);

	/*
	 * Decrement the outstanding request count.
	 */
	if (rep->r_flags & R_SENT) {
		rep->r_flags &= ~R_SENT;	/* paranoia */
		nmp->nm_sent -= NFS_CWNDSCALE;
	}
	splx(s);

	if (rexmitp != NULL) {
		int rexmit;

		if (nmp->nm_sotype != SOCK_DGRAM)
			rexmit = (rep->r_flags & R_REXMITTED) != 0;
		else
			rexmit = rep->r_rexmit;
		*rexmitp = rexmit;
	}

	/*
	 * If there was a successful reply and a tprintf msg.
	 * tprintf a response.
	 */
	if (!error && (rep->r_flags & R_TPRINTFMSG))
		nfs_msg(rep->r_lwp, nmp->nm_mountp->mnt_stat.f_mntfromname,
		    "is alive again");
	mrep = rep->r_mrep;
	md = rep->r_md;
	dpos = rep->r_dpos;
	if (error)
		goto nfsmout;

	/*
	 * break down the rpc header and check if ok
	 */
	nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	if (*tl++ == rpc_msgdenied) {
		if (*tl == rpc_mismatch)
			error = EOPNOTSUPP;
		else if ((nmp->nm_flag & NFSMNT_KERB) && *tl++ == rpc_autherr) {
			if (!failed_auth) {
				failed_auth++;
				mheadend->m_next = NULL;
				m_freem(mrep);
				m_freem(rep->r_mreq);
				goto kerbauth;
			} else
				error = EAUTH;
		} else
			error = EACCES;
		m_freem(mrep);
		goto nfsmout;
	}

	/*
	 * Grab any Kerberos verifier, otherwise just throw it away.
	 */
	verf_type = fxdr_unsigned(int, *tl++);
	i = fxdr_unsigned(int32_t, *tl);
	if ((nmp->nm_flag & NFSMNT_KERB) && verf_type == RPCAUTH_KERB4) {
		error = nfs_savenickauth(nmp, cred, i, key, &md, &dpos, mrep);
		if (error)
			goto nfsmout;
	} else if (i > 0)
		nfsm_adv(nfsm_rndup(i));
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	/* 0 == ok */
	if (*tl == 0) {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		if (*tl != 0) {
			error = fxdr_unsigned(int, *tl);
			switch (error) {
			case NFSERR_PERM:
				error = EPERM;
				break;

			case NFSERR_NOENT:
				error = ENOENT;
				break;

			case NFSERR_IO:
				error = EIO;
				break;

			case NFSERR_NXIO:
				error = ENXIO;
				break;

			case NFSERR_ACCES:
				error = EACCES;
				if (!retry_cred)
					break;
				m_freem(mrep);
				m_freem(rep->r_mreq);
				kmem_free(rep, sizeof(*rep));
				use_opencred = !use_opencred;
				if (mrest_backup == NULL) {
					/* m_copym failure */
					KASSERT(
					    kauth_cred_getrefcnt(acred) == 1);
					kauth_cred_free(acred);
					return ENOMEM;
				}
				mrest = mrest_backup;
				mrest_backup = NULL;
				cred = origcred;
				error = 0;
				retry_cred = false;
				goto tryagain_cred;

			case NFSERR_EXIST:
				error = EEXIST;
				break;

			case NFSERR_XDEV:
				error = EXDEV;
				break;

			case NFSERR_NODEV:
				error = ENODEV;
				break;

			case NFSERR_NOTDIR:
				error = ENOTDIR;
				break;

			case NFSERR_ISDIR:
				error = EISDIR;
				break;

			case NFSERR_INVAL:
				error = EINVAL;
				break;

			case NFSERR_FBIG:
				error = EFBIG;
				break;

			case NFSERR_NOSPC:
				error = ENOSPC;
				break;

			case NFSERR_ROFS:
				error = EROFS;
				break;

			case NFSERR_MLINK:
				error = EMLINK;
				break;

			case NFSERR_TIMEDOUT:
				error = ETIMEDOUT;
				break;

			case NFSERR_NAMETOL:
				error = ENAMETOOLONG;
				break;

			case NFSERR_NOTEMPTY:
				error = ENOTEMPTY;
				break;

			case NFSERR_DQUOT:
				error = EDQUOT;
				break;

			case NFSERR_STALE:
				/*
				 * If the File Handle was stale, invalidate the
				 * lookup cache, just in case.
				 */
				error = ESTALE;
				cache_purge(NFSTOV(np));
				break;

			case NFSERR_REMOTE:
				error = EREMOTE;
				break;

			case NFSERR_WFLUSH:
			case NFSERR_BADHANDLE:
			case NFSERR_NOT_SYNC:
			case NFSERR_BAD_COOKIE:
				error = EINVAL;
				break;

			case NFSERR_NOTSUPP:
				error = ENOTSUP;
				break;

			case NFSERR_TOOSMALL:
			case NFSERR_SERVERFAULT:
			case NFSERR_BADTYPE:
				error = EINVAL;
				break;

			case NFSERR_TRYLATER:
				if ((nmp->nm_flag & NFSMNT_NFSV3) == 0)
					break;
				m_freem(mrep);
				error = 0;
				waituntil = time_second + trylater_delay;
				while (time_second < waituntil) {
					kpause("nfstrylater", false, hz, NULL);
				}
				trylater_delay *= NFS_TRYLATERDELMUL;
				if (trylater_delay > NFS_TRYLATERDELMAX)
					trylater_delay = NFS_TRYLATERDELMAX;
				/*
				 * RFC1813:
				 * The client should wait and then try
				 * the request with a new RPC transaction ID.
				 */
				nfs_renewxid(rep);
				goto tryagain;

			default:
#ifdef DIAGNOSTIC
				printf("Invalid rpc error code %d\n", error);
#endif
				error = EINVAL;
				break;
			}

			if (nmp->nm_flag & NFSMNT_NFSV3) {
				*mrp = mrep;
				*mdp = md;
				*dposp = dpos;
				error |= NFSERR_RETERR;
			} else
				m_freem(mrep);
			goto nfsmout;
		}

		/*
		 * note which credential worked to minimize number of retries.
		 */
		if (use_opencred)
			np->n_flag |= NUSEOPENCRED;
		else
			np->n_flag &= ~NUSEOPENCRED;

		*mrp = mrep;
		*mdp = md;
		*dposp = dpos;

		KASSERT(error == 0);
		goto nfsmout;
	}
	m_freem(mrep);
	error = EPROTONOSUPPORT;
nfsmout:
	KASSERT(kauth_cred_getrefcnt(acred) == 1);
	kauth_cred_free(acred);
	m_freem(rep->r_mreq);
	kmem_free(rep, sizeof(*rep));
	m_freem(mrest_backup);
	return (error);
}

/*
 * Lock a socket against others.
 * Necessary for STREAM sockets to ensure you get an entire rpc request/reply
 * and also to avoid race conditions between the processes with nfs requests
 * in progress when a reconnect is necessary.
 */
static int
nfs_sndlock(struct nfsmount *nmp, struct nfsreq *rep)
{
	struct lwp *l;
	int timeo = 0;
	bool catch_p = false;
	int error = 0;

	if (nmp->nm_flag & NFSMNT_SOFT)
		timeo = nmp->nm_retry * nmp->nm_timeo;

	if (nmp->nm_iflag & NFSMNT_DISMNTFORCE)
		timeo = hz;

	if (rep) {
		l = rep->r_lwp;
		if (rep->r_nmp->nm_flag & NFSMNT_INT)
			catch_p = true;
	} else
		l = NULL;
	mutex_enter(&nmp->nm_lock);
	while ((nmp->nm_iflag & NFSMNT_SNDLOCK) != 0) {
		if (rep && nfs_sigintr(rep->r_nmp, rep, l)) {
			error = EINTR;
			goto quit;
		}
		if (catch_p) {
			error = cv_timedwait_sig(&nmp->nm_sndcv,
						 &nmp->nm_lock, timeo);
		} else {
			error = cv_timedwait(&nmp->nm_sndcv,
					     &nmp->nm_lock, timeo);
		}

		if (error) {
			if ((error == EWOULDBLOCK) &&
			    (nmp->nm_flag & NFSMNT_SOFT)) {
				error = EIO;
				goto quit;
			}
			error = 0;
		}
		if (catch_p) {
			catch_p = false;
			timeo = 2 * hz;
		}
	}
	nmp->nm_iflag |= NFSMNT_SNDLOCK;
quit:
	mutex_exit(&nmp->nm_lock);
	return error;
}

/*
 * Unlock the stream socket for others.
 */
static void
nfs_sndunlock(struct nfsmount *nmp)
{

	mutex_enter(&nmp->nm_lock);
	if ((nmp->nm_iflag & NFSMNT_SNDLOCK) == 0)
		panic("nfs sndunlock");
	nmp->nm_iflag &= ~NFSMNT_SNDLOCK;
	cv_signal(&nmp->nm_sndcv);
	mutex_exit(&nmp->nm_lock);
}
