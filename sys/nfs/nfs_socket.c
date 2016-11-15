/*	$NetBSD: nfs_socket.c,v 1.197 2015/07/15 03:28:55 manu Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: nfs_socket.c,v 1.197 2015/07/15 03:28:55 manu Exp $");

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
#include <sys/time.h>

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

#ifdef MBUFTRACE
struct mowner nfs_mowner = MOWNER_INIT("nfs","");
#endif

/*
 * Estimate rto for an nfs rpc sent via. an unreliable datagram.
 * Use the mean and mean deviation of rtt for the appropriate type of rpc
 * for the frequent rpcs and a default for the others.
 * The justification for doing "other" this way is that these rpcs
 * happen so infrequently that timer est. would probably be stale.
 * Also, since many of these rpcs are
 * non-idempotent, a conservative timeout is desired.
 * getattr, lookup - A+2D
 * read, write     - A+4D
 * other           - nm_timeo
 */
#define	NFS_RTO(n, t) \
	((t) == 0 ? (n)->nm_timeo : \
	 ((t) < 3 ? \
	  (((((n)->nm_srtt[t-1] + 3) >> 2) + (n)->nm_sdrtt[t-1] + 1) >> 1) : \
	  ((((n)->nm_srtt[t-1] + 7) >> 3) + (n)->nm_sdrtt[t-1] + 1)))
#define	NFS_SRTT(r)	(r)->r_nmp->nm_srtt[nfs_proct[(r)->r_procnum] - 1]
#define	NFS_SDRTT(r)	(r)->r_nmp->nm_sdrtt[nfs_proct[(r)->r_procnum] - 1]

/*
 * Defines which timer to use for the procnum.
 * 0 - default
 * 1 - getattr
 * 2 - lookup
 * 3 - read
 * 4 - write
 */
const int nfs_proct[NFS_NPROCS] = {
	[NFSPROC_NULL] = 0,
	[NFSPROC_GETATTR] = 1,
	[NFSPROC_SETATTR] = 0,
	[NFSPROC_LOOKUP] = 2,
	[NFSPROC_ACCESS] = 1,
	[NFSPROC_READLINK] = 3,
	[NFSPROC_READ] = 3,
	[NFSPROC_WRITE] = 4,
	[NFSPROC_CREATE] = 0,
	[NFSPROC_MKDIR] = 0,
	[NFSPROC_SYMLINK] = 0,
	[NFSPROC_MKNOD] = 0,
	[NFSPROC_REMOVE] = 0,
	[NFSPROC_RMDIR] = 0,
	[NFSPROC_RENAME] = 0,
	[NFSPROC_LINK] = 0,
	[NFSPROC_READDIR] = 3,
	[NFSPROC_READDIRPLUS] = 3,
	[NFSPROC_FSSTAT] = 0,
	[NFSPROC_FSINFO] = 0,
	[NFSPROC_PATHCONF] = 0,
	[NFSPROC_COMMIT] = 0,
	[NFSPROC_NOOP] = 0,
};

#ifdef DEBUG
/*
 * Avoid spamming the console with debugging messages.  We only print
 * the nfs timer and reply error debugs every 10 seconds.
 */
const struct timeval nfs_err_interval = { 10, 0 };
struct timeval nfs_reply_last_err_time;
struct timeval nfs_timer_last_err_time;
#endif

/*
 * There is a congestion window for outstanding rpcs maintained per mount
 * point. The cwnd size is adjusted in roughly the way that:
 * Van Jacobson, Congestion avoidance and Control, In "Proceedings of
 * SIGCOMM '88". ACM, August 1988.
 * describes for TCP. The cwnd size is chopped in half on a retransmit timeout
 * and incremented by 1/cwnd when each rpc reply is received and a full cwnd
 * of rpcs is in progress.
 * (The sent count and cwnd are scaled for integer arith.)
 * Variants of "slow start" were tried and were found to be too much of a
 * performance hit (ave. rtt 3 times larger),
 * I suspect due to the large rtt that nfs rpcs have.
 */
int nfsrtton = 0;  
struct nfsrtt nfsrtt;
static const int nfs_backoff[8] = { 2, 4, 8, 16, 32, 64, 128, 256, };
struct nfsreqhead nfs_reqq;
static callout_t nfs_timer_ch;
static struct evcnt nfs_timer_ev;
static struct evcnt nfs_timer_start_ev;
static struct evcnt nfs_timer_stop_ev;
static kmutex_t nfs_timer_lock;
static bool (*nfs_timer_srvvec)(void);

/*
 * Initialize sockets and congestion for a new NFS connection.
 * We do not free the sockaddr if error.
 */
int
nfs_connect(struct nfsmount *nmp, struct nfsreq *rep, struct lwp *l)
{
	struct socket *so;
	int error, rcvreserve, sndreserve;
	struct sockaddr *saddr;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	int val;

	nmp->nm_so = NULL;
	saddr = mtod(nmp->nm_nam, struct sockaddr *);
	error = socreate(saddr->sa_family, &nmp->nm_so,
		nmp->nm_sotype, nmp->nm_soproto, l, NULL);
	if (error)
		goto bad;
	so = nmp->nm_so;
#ifdef MBUFTRACE
	so->so_mowner = &nfs_mowner;
	so->so_rcv.sb_mowner = &nfs_mowner;
	so->so_snd.sb_mowner = &nfs_mowner;
#endif
	nmp->nm_soflags = so->so_proto->pr_flags;

	/*
	 * Some servers require that the client port be a reserved port number.
	 */
	if (saddr->sa_family == AF_INET && (nmp->nm_flag & NFSMNT_RESVPORT)) {
		val = IP_PORTRANGE_LOW;

		if ((error = so_setsockopt(NULL, so, IPPROTO_IP, IP_PORTRANGE,
		    &val, sizeof(val))))
			goto bad;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = 0;
		error = sobind(so, (struct sockaddr *)&sin, &lwp0);
		if (error)
			goto bad;
	}
	if (saddr->sa_family == AF_INET6 && (nmp->nm_flag & NFSMNT_RESVPORT)) {
		val = IPV6_PORTRANGE_LOW;

		if ((error = so_setsockopt(NULL, so, IPPROTO_IPV6,
		    IPV6_PORTRANGE, &val, sizeof(val))))
			goto bad;
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_family = AF_INET6;
		error = sobind(so, (struct sockaddr *)&sin6, &lwp0);
		if (error)
			goto bad;
	}

	/*
	 * Protocols that do not require connections may be optionally left
	 * unconnected for servers that reply from a port other than NFS_PORT.
	 */
	solock(so);
	if (nmp->nm_flag & NFSMNT_NOCONN) {
		if (nmp->nm_soflags & PR_CONNREQUIRED) {
			sounlock(so);
			error = ENOTCONN;
			goto bad;
		}
	} else {
		error = soconnect(so, mtod(nmp->nm_nam, struct sockaddr *), l);
		if (error) {
			sounlock(so);
			goto bad;
		}

		/*
		 * Wait for the connection to complete. Cribbed from the
		 * connect system call but with the wait timing out so
		 * that interruptible mounts don't hang here for a long time.
		 */
		while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
			(void)sowait(so, false, 2 * hz);
			if ((so->so_state & SS_ISCONNECTING) &&
			    so->so_error == 0 && rep &&
			    (error = nfs_sigintr(nmp, rep, rep->r_lwp)) != 0){
				so->so_state &= ~SS_ISCONNECTING;
				sounlock(so);
				goto bad;
			}
		}
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			sounlock(so);
			goto bad;
		}
	}
	if (nmp->nm_flag & (NFSMNT_SOFT | NFSMNT_INT)) {
		so->so_rcv.sb_timeo = (5 * hz);
		so->so_snd.sb_timeo = (5 * hz);
	} else {
		/*
		 * enable receive timeout to detect server crash and reconnect.
		 * otherwise, we can be stuck in soreceive forever.
		 */
		so->so_rcv.sb_timeo = (5 * hz);
		so->so_snd.sb_timeo = 0;
	}
	if (nmp->nm_sotype == SOCK_DGRAM) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * 3;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * 2;
	} else if (nmp->nm_sotype == SOCK_SEQPACKET) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * 3;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * 3;
	} else {
		sounlock(so);
		if (nmp->nm_sotype != SOCK_STREAM)
			panic("nfscon sotype");
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
			val = 1;
			so_setsockopt(NULL, so, SOL_SOCKET, SO_KEEPALIVE, &val,
			    sizeof(val));
		}
		if (so->so_proto->pr_protocol == IPPROTO_TCP) {
			val = 1;
			so_setsockopt(NULL, so, IPPROTO_TCP, TCP_NODELAY, &val,
			    sizeof(val));
		}
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * 3;
		rcvreserve = (nmp->nm_rsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * 3;
		solock(so);
	}
	error = soreserve(so, sndreserve, rcvreserve);
	if (error) {
		sounlock(so);
		goto bad;
	}
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;
	sounlock(so);

	/* Initialize other non-zero congestion variables */
	nmp->nm_srtt[0] = nmp->nm_srtt[1] = nmp->nm_srtt[2] = nmp->nm_srtt[3] =
		NFS_TIMEO << 3;
	nmp->nm_sdrtt[0] = nmp->nm_sdrtt[1] = nmp->nm_sdrtt[2] =
		nmp->nm_sdrtt[3] = 0;
	nmp->nm_cwnd = NFS_MAXCWND / 2;	    /* Initial send window */
	nmp->nm_sent = 0;
	nmp->nm_timeouts = 0;
	return (0);

bad:
	nfs_disconnect(nmp);
	return (error);
}

/*
 * Reconnect routine:
 * Called when a connection is broken on a reliable protocol.
 * - clean up the old socket
 * - nfs_connect() again
 * - set R_MUSTRESEND for all outstanding requests on mount point
 * If this fails the mount point is DEAD!
 * nb: Must be called with the nfs_sndlock() set on the mount point.
 */
int
nfs_reconnect(struct nfsreq *rep)
{
	struct nfsreq *rp;
	struct nfsmount *nmp = rep->r_nmp;
	int error;
	time_t before_ts;

	nfs_disconnect(nmp);

	/*
	 * Force unmount: do not try to reconnect
	 */
	if (nmp->nm_iflag & NFSMNT_DISMNTFORCE)
		return EIO;

	before_ts = time_uptime;
	while ((error = nfs_connect(nmp, rep, &lwp0)) != 0) {
		if (error == EINTR || error == ERESTART)
			return (EINTR);

		if (rep->r_flags & R_SOFTTERM)
			return (EIO);

		/*
		 * Soft mount can fail here, but not too fast: 
		 * we want to make sure we at least honoured 
		 * NFS timeout.
		 */
		if ((nmp->nm_flag & NFSMNT_SOFT) &&
		    (time_uptime - before_ts > nmp->nm_timeo / NFS_HZ))
			return (EIO);

		kpause("nfscn2", false, hz, NULL);
	}

	/*
	 * Loop through outstanding request list and fix up all requests
	 * on old socket.
	 */
	TAILQ_FOREACH(rp, &nfs_reqq, r_chain) {
		if (rp->r_nmp == nmp) {
			if ((rp->r_flags & R_MUSTRESEND) == 0)
				rp->r_flags |= R_MUSTRESEND | R_REXMITTED;
			rp->r_rexmit = 0;
		}
	}
	return (0);
}

/*
 * NFS disconnect. Clean up and unlink.
 */
void
nfs_disconnect(struct nfsmount *nmp)
{
	struct socket *so;
	int drain = 0;

	if (nmp->nm_so) {
		so = nmp->nm_so;
		nmp->nm_so = NULL;
		solock(so);
		soshutdown(so, SHUT_RDWR);
		sounlock(so);
		drain = (nmp->nm_iflag & NFSMNT_DISMNT) != 0;
		if (drain) {
			/*
			 * soshutdown() above should wake up the current
			 * listener.
			 * Now wake up those waiting for the receive lock, and
			 * wait for them to go away unhappy, to prevent *nmp
			 * from evaporating while they're sleeping.
			 */
			mutex_enter(&nmp->nm_lock);
			while (nmp->nm_waiters > 0) {
				cv_broadcast(&nmp->nm_rcvcv);
				cv_broadcast(&nmp->nm_sndcv);
				cv_wait(&nmp->nm_disconcv, &nmp->nm_lock);
			}
			mutex_exit(&nmp->nm_lock);
		}
		soclose(so);
	}
#ifdef DIAGNOSTIC
	if (drain && (nmp->nm_waiters > 0))
		panic("nfs_disconnect: waiters left after drain?");
#endif
}

void
nfs_safedisconnect(struct nfsmount *nmp)
{
	struct nfsreq dummyreq;

	memset(&dummyreq, 0, sizeof(dummyreq));
	dummyreq.r_nmp = nmp;
	nfs_rcvlock(nmp, &dummyreq); /* XXX ignored error return */
	nfs_disconnect(nmp);
	nfs_rcvunlock(nmp);
}

/*
 * This is the nfs send routine. For connection based socket types, it
 * must be called with an nfs_sndlock() on the socket.
 * "rep == NULL" indicates that it has been called from a server.
 * For the client side:
 * - return EINTR if the RPC is terminated, 0 otherwise
 * - set R_MUSTRESEND if the send fails for any reason
 * - do any cleanup required by recoverable socket errors (? ? ?)
 * For the server side:
 * - return EINTR or ERESTART if interrupted by a signal
 * - return EPIPE if a connection is lost for connection based sockets (TCP...)
 * - do any cleanup required by recoverable socket errors (? ? ?)
 */
int
nfs_send(struct socket *so, struct mbuf *nam, struct mbuf *top, struct nfsreq *rep, struct lwp *l)
{
	struct sockaddr *sendnam;
	int error, soflags, flags;

	/* XXX nfs_doio()/nfs_request() calls with  rep->r_lwp == NULL */
	if (l == NULL && rep->r_lwp == NULL)
		l = curlwp;

	if (rep) {
		if (rep->r_flags & R_SOFTTERM) {
			m_freem(top);
			return (EINTR);
		}
		if ((so = rep->r_nmp->nm_so) == NULL) {
			rep->r_flags |= R_MUSTRESEND;
			m_freem(top);
			return (0);
		}
		rep->r_flags &= ~R_MUSTRESEND;
		soflags = rep->r_nmp->nm_soflags;
	} else
		soflags = so->so_proto->pr_flags;
	if ((soflags & PR_CONNREQUIRED) || (so->so_state & SS_ISCONNECTED))
		sendnam = NULL;
	else
		sendnam = mtod(nam, struct sockaddr *);
	if (so->so_type == SOCK_SEQPACKET)
		flags = MSG_EOR;
	else
		flags = 0;

	error = (*so->so_send)(so, sendnam, NULL, top, NULL, flags,  l);
	if (error) {
		if (rep) {
			if (error == ENOBUFS && so->so_type == SOCK_DGRAM) {
				/*
				 * We're too fast for the network/driver,
				 * and UDP isn't flowcontrolled.
				 * We need to resend. This is not fatal,
				 * just try again.
				 *
				 * Could be smarter here by doing some sort
				 * of a backoff, but this is rare.
				 */
				rep->r_flags |= R_MUSTRESEND;
			} else {
				if (error != EPIPE)
					log(LOG_INFO,
					    "nfs send error %d for %s\n",
					    error,
					    rep->r_nmp->nm_mountp->
						    mnt_stat.f_mntfromname);
				/*
				 * Deal with errors for the client side.
				 */
				if (rep->r_flags & R_SOFTTERM)
					error = EINTR;
				else if (error != EMSGSIZE)
					rep->r_flags |= R_MUSTRESEND;
			}
		} else {
			/*
			 * See above. This error can happen under normal
			 * circumstances and the log is too noisy.
			 * The error will still show up in nfsstat.
			 */
			if (error != ENOBUFS || so->so_type != SOCK_DGRAM)
				log(LOG_INFO, "nfsd send error %d\n", error);
		}

		/*
		 * Handle any recoverable (soft) socket errors here. (? ? ?)
		 */
		if (error != EINTR && error != ERESTART &&
		    error != EWOULDBLOCK && error != EPIPE &&
		    error != EMSGSIZE)
			error = 0;
	}
	return (error);
}

/*
 * Generate the rpc reply header
 * siz arg. is used to decide if adding a cluster is worthwhile
 */
int
nfs_rephead(int siz, struct nfsrv_descript *nd, struct nfssvc_sock *slp, int err, int cache, u_quad_t *frev, struct mbuf **mrq, struct mbuf **mbp, char **bposp)
{
	u_int32_t *tl;
	struct mbuf *mreq;
	char *bpos;
	struct mbuf *mb;

	mreq = m_gethdr(M_WAIT, MT_DATA);
	MCLAIM(mreq, &nfs_mowner);
	mb = mreq;
	/*
	 * If this is a big reply, use a cluster else
	 * try and leave leading space for the lower level headers.
	 */
	siz += RPC_REPLYSIZ;
	if (siz >= max_datalen) {
		m_clget(mreq, M_WAIT);
	} else
		mreq->m_data += max_hdr;
	tl = mtod(mreq, u_int32_t *);
	mreq->m_len = 6 * NFSX_UNSIGNED;
	bpos = ((char *)tl) + mreq->m_len;
	*tl++ = txdr_unsigned(nd->nd_retxid);
	*tl++ = rpc_reply;
	if (err == ERPCMISMATCH || (err & NFSERR_AUTHERR)) {
		*tl++ = rpc_msgdenied;
		if (err & NFSERR_AUTHERR) {
			*tl++ = rpc_autherr;
			*tl = txdr_unsigned(err & ~NFSERR_AUTHERR);
			mreq->m_len -= NFSX_UNSIGNED;
			bpos -= NFSX_UNSIGNED;
		} else {
			*tl++ = rpc_mismatch;
			*tl++ = txdr_unsigned(RPC_VER2);
			*tl = txdr_unsigned(RPC_VER2);
		}
	} else {
		*tl++ = rpc_msgaccepted;

		/*
		 * For Kerberos authentication, we must send the nickname
		 * verifier back, otherwise just RPCAUTH_NULL.
		 */
		if (nd->nd_flag & ND_KERBFULL) {
			struct nfsuid *nuidp;
			struct timeval ktvin, ktvout;

			memset(&ktvout, 0, sizeof ktvout);	/* XXX gcc */

			LIST_FOREACH(nuidp,
			    NUIDHASH(slp, kauth_cred_geteuid(nd->nd_cr)),
			    nu_hash) {
				if (kauth_cred_geteuid(nuidp->nu_cr) ==
				kauth_cred_geteuid(nd->nd_cr) &&
				    (!nd->nd_nam2 || netaddr_match(
				    NU_NETFAM(nuidp), &nuidp->nu_haddr,
				    nd->nd_nam2)))
					break;
			}
			if (nuidp) {
				ktvin.tv_sec =
				    txdr_unsigned(nuidp->nu_timestamp.tv_sec
					- 1);
				ktvin.tv_usec =
				    txdr_unsigned(nuidp->nu_timestamp.tv_usec);

				/*
				 * Encrypt the timestamp in ecb mode using the
				 * session key.
				 */
#ifdef NFSKERB
				XXX
#else
				(void)ktvin.tv_sec;
#endif

				*tl++ = rpc_auth_kerb;
				*tl++ = txdr_unsigned(3 * NFSX_UNSIGNED);
				*tl = ktvout.tv_sec;
				nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
				*tl++ = ktvout.tv_usec;
				*tl++ = txdr_unsigned(
				    kauth_cred_geteuid(nuidp->nu_cr));
			} else {
				*tl++ = 0;
				*tl++ = 0;
			}
		} else {
			*tl++ = 0;
			*tl++ = 0;
		}
		switch (err) {
		case EPROGUNAVAIL:
			*tl = txdr_unsigned(RPC_PROGUNAVAIL);
			break;
		case EPROGMISMATCH:
			*tl = txdr_unsigned(RPC_PROGMISMATCH);
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(2);
			*tl = txdr_unsigned(3);
			break;
		case EPROCUNAVAIL:
			*tl = txdr_unsigned(RPC_PROCUNAVAIL);
			break;
		case EBADRPC:
			*tl = txdr_unsigned(RPC_GARBAGE);
			break;
		default:
			*tl = 0;
			if (err != NFSERR_RETVOID) {
				nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
				if (err)
				    *tl = txdr_unsigned(nfsrv_errmap(nd, err));
				else
				    *tl = 0;
			}
			break;
		};
	}

	if (mrq != NULL)
		*mrq = mreq;
	*mbp = mb;
	*bposp = bpos;
	if (err != 0 && err != NFSERR_RETVOID)
		nfsstats.srvrpc_errs++;
	return (0);
}

static void
nfs_timer_schedule(void)
{

	callout_schedule(&nfs_timer_ch, nfs_ticks);
}

void
nfs_timer_start(void)
{

	if (callout_pending(&nfs_timer_ch))
		return;

	nfs_timer_start_ev.ev_count++;
	nfs_timer_schedule();
}

void
nfs_timer_init(void)
{

	mutex_init(&nfs_timer_lock, MUTEX_DEFAULT, IPL_NONE);
	callout_init(&nfs_timer_ch, 0);
	callout_setfunc(&nfs_timer_ch, nfs_timer, NULL);
	evcnt_attach_dynamic(&nfs_timer_ev, EVCNT_TYPE_MISC, NULL,
	    "nfs", "timer");
	evcnt_attach_dynamic(&nfs_timer_start_ev, EVCNT_TYPE_MISC, NULL,
	    "nfs", "timer start");
	evcnt_attach_dynamic(&nfs_timer_stop_ev, EVCNT_TYPE_MISC, NULL,
	    "nfs", "timer stop");
}

void
nfs_timer_fini(void)
{

	callout_halt(&nfs_timer_ch, NULL);
	callout_destroy(&nfs_timer_ch);
	mutex_destroy(&nfs_timer_lock);
	evcnt_detach(&nfs_timer_ev);
	evcnt_detach(&nfs_timer_start_ev);
	evcnt_detach(&nfs_timer_stop_ev);
}

void
nfs_timer_srvinit(bool (*func)(void))
{

	nfs_timer_srvvec = func;
}

void
nfs_timer_srvfini(void)
{

	mutex_enter(&nfs_timer_lock);
	nfs_timer_srvvec = NULL;
	mutex_exit(&nfs_timer_lock);
}


/*
 * Nfs timer routine
 * Scan the nfsreq list and retranmit any requests that have timed out
 * To avoid retransmission attempts on STREAM sockets (in the future) make
 * sure to set the r_retry field to 0 (implies nm_retry == 0).
 */
void
nfs_timer(void *arg)
{
	struct nfsreq *rep;
	struct mbuf *m;
	struct socket *so;
	struct nfsmount *nmp;
	int timeo;
	int error;
	bool more = false;

	nfs_timer_ev.ev_count++;

	mutex_enter(softnet_lock);	/* XXX PR 40491 */
	TAILQ_FOREACH(rep, &nfs_reqq, r_chain) {
		more = true;
		nmp = rep->r_nmp;
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM))
			continue;
		if (nfs_sigintr(nmp, rep, rep->r_lwp)) {
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (rep->r_rtt >= 0) {
			rep->r_rtt++;
			if (nmp->nm_flag & NFSMNT_DUMBTIMR)
				timeo = nmp->nm_timeo;
			else
				timeo = NFS_RTO(nmp, nfs_proct[rep->r_procnum]);
			if (nmp->nm_timeouts > 0)
				timeo *= nfs_backoff[nmp->nm_timeouts - 1];
			if (timeo > NFS_MAXTIMEO)
				timeo = NFS_MAXTIMEO;
			if (rep->r_rtt <= timeo)
				continue;
			if (nmp->nm_timeouts <
			    (sizeof(nfs_backoff) / sizeof(nfs_backoff[0])))
				nmp->nm_timeouts++;
		}
		/*
		 * Check for server not responding
		 */
		if ((rep->r_flags & R_TPRINTFMSG) == 0 &&
		     rep->r_rexmit > nmp->nm_deadthresh) {
			nfs_msg(rep->r_lwp,
			    nmp->nm_mountp->mnt_stat.f_mntfromname,
			    "not responding");
			rep->r_flags |= R_TPRINTFMSG;
		}
		if (rep->r_rexmit >= rep->r_retry) {	/* too many */
			nfsstats.rpctimeouts++;
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (nmp->nm_sotype != SOCK_DGRAM) {
			if (++rep->r_rexmit > NFS_MAXREXMIT)
				rep->r_rexmit = NFS_MAXREXMIT;
			continue;
		}
		if ((so = nmp->nm_so) == NULL)
			continue;

		/*
		 * If there is enough space and the window allows..
		 *	Resend it
		 * Set r_rtt to -1 in case we fail to send it now.
		 */
		/* solock(so);		XXX PR 40491 */
		rep->r_rtt = -1;
		if (sbspace(&so->so_snd) >= rep->r_mreq->m_pkthdr.len &&
		   ((nmp->nm_flag & NFSMNT_DUMBTIMR) ||
		    (rep->r_flags & R_SENT) ||
		    nmp->nm_sent < nmp->nm_cwnd) &&
		   (m = m_copym(rep->r_mreq, 0, M_COPYALL, M_DONTWAIT))){
		        if (so->so_state & SS_ISCONNECTED)
			    error = (*so->so_proto->pr_usrreqs->pr_send)(so,
			    m, NULL, NULL, NULL);
			else
			    error = (*so->so_proto->pr_usrreqs->pr_send)(so,
				m, mtod(nmp->nm_nam, struct sockaddr *),
				NULL, NULL);
			if (error) {
				if (NFSIGNORE_SOERROR(nmp->nm_soflags, error)) {
#ifdef DEBUG
					if (ratecheck(&nfs_timer_last_err_time,
					    &nfs_err_interval))
						printf("%s: ignoring error "
						       "%d\n", __func__, error);
#endif
					so->so_error = 0;
				}
			} else {
				/*
				 * Iff first send, start timing
				 * else turn timing off, backoff timer
				 * and divide congestion window by 2.
				 */
				if (rep->r_flags & R_SENT) {
					rep->r_flags &= ~R_TIMING;
					if (++rep->r_rexmit > NFS_MAXREXMIT)
						rep->r_rexmit = NFS_MAXREXMIT;
					nmp->nm_cwnd >>= 1;
					if (nmp->nm_cwnd < NFS_CWNDSCALE)
						nmp->nm_cwnd = NFS_CWNDSCALE;
					nfsstats.rpcretries++;
				} else {
					rep->r_flags |= R_SENT;
					nmp->nm_sent += NFS_CWNDSCALE;
				}
				rep->r_rtt = 0;
			}
		}
		/* sounlock(so);	XXX PR 40491 */
	}
	mutex_exit(softnet_lock);	/* XXX PR 40491 */

	mutex_enter(&nfs_timer_lock);
	if (nfs_timer_srvvec != NULL) {
		more |= (*nfs_timer_srvvec)();
	}
	mutex_exit(&nfs_timer_lock);

	if (more) {
		nfs_timer_schedule();
	} else {
		nfs_timer_stop_ev.ev_count++;
	}
}

/*
 * Test for a termination condition pending on the process.
 * This is used for NFSMNT_INT mounts.
 */
int
nfs_sigintr(struct nfsmount *nmp, struct nfsreq *rep, struct lwp *l)
{
	sigset_t ss;

	if (rep && (rep->r_flags & R_SOFTTERM))
		return (EINTR);
	if (!(nmp->nm_flag & NFSMNT_INT))
		return (0);
	if (l) {
		sigpending1(l, &ss);
#if 0
		sigminusset(&l->l_proc->p_sigctx.ps_sigignore, &ss);
#endif
		if (sigismember(&ss, SIGINT) || sigismember(&ss, SIGTERM) ||
		    sigismember(&ss, SIGKILL) || sigismember(&ss, SIGHUP) ||
		    sigismember(&ss, SIGQUIT))
			return (EINTR);
	}
	return (0);
}

int
nfs_rcvlock(struct nfsmount *nmp, struct nfsreq *rep)
{
	int *flagp = &nmp->nm_iflag;
	int slptimeo = 0;
	bool catch_p;
	int error = 0;

	KASSERT(nmp == rep->r_nmp);

	if (nmp->nm_flag & NFSMNT_SOFT)
		slptimeo = nmp->nm_retry * nmp->nm_timeo;

	if (nmp->nm_iflag & NFSMNT_DISMNTFORCE)
		slptimeo = hz;

	catch_p = (nmp->nm_flag & NFSMNT_INT) != 0;
	mutex_enter(&nmp->nm_lock);
	while (/* CONSTCOND */ true) {
		if (*flagp & NFSMNT_DISMNT) {
			cv_signal(&nmp->nm_disconcv);
			error = EIO;
			break;
		}
		/* If our reply was received while we were sleeping,
		 * then just return without taking the lock to avoid a
		 * situation where a single iod could 'capture' the
		 * receive lock.
		 */
		if (rep->r_mrep != NULL) {
			cv_signal(&nmp->nm_rcvcv);
			error = EALREADY;
			break;
		}
		if (nfs_sigintr(rep->r_nmp, rep, rep->r_lwp)) {
			cv_signal(&nmp->nm_rcvcv);
			error = EINTR;
			break;
		}
		if ((*flagp & NFSMNT_RCVLOCK) == 0) {
			*flagp |= NFSMNT_RCVLOCK;
			break;
		}
		if (catch_p) {
			error = cv_timedwait_sig(&nmp->nm_rcvcv, &nmp->nm_lock,
			    slptimeo);
		} else {
			error = cv_timedwait(&nmp->nm_rcvcv, &nmp->nm_lock,
			    slptimeo);
		}
		if (error) {
			if ((error == EWOULDBLOCK) &&
			    (nmp->nm_flag & NFSMNT_SOFT)) {
				error = EIO;
				break;
			}
			error = 0;
		}
		if (catch_p) {
			catch_p = false;
			slptimeo = 2 * hz;
		}
	}
	mutex_exit(&nmp->nm_lock);
	return error;
}

/*
 * Unlock the stream socket for others.
 */
void
nfs_rcvunlock(struct nfsmount *nmp)
{

	mutex_enter(&nmp->nm_lock);
	if ((nmp->nm_iflag & NFSMNT_RCVLOCK) == 0)
		panic("nfs rcvunlock");
	nmp->nm_iflag &= ~NFSMNT_RCVLOCK;
	cv_signal(&nmp->nm_rcvcv);
	mutex_exit(&nmp->nm_lock);
}

/*
 * Parse an RPC request
 * - verify it
 * - allocate and fill in the cred.
 */
int
nfs_getreq(struct nfsrv_descript *nd, struct nfsd *nfsd, int has_header)
{
	int len, i;
	u_int32_t *tl;
	int32_t t1;
	struct uio uio;
	struct iovec iov;
	char *dpos, *cp2, *cp;
	u_int32_t nfsvers, auth_type;
	uid_t nickuid;
	int error = 0, ticklen;
	struct mbuf *mrep, *md;
	struct nfsuid *nuidp;
	struct timeval tvin, tvout;

	memset(&tvout, 0, sizeof tvout);	/* XXX gcc */

	KASSERT(nd->nd_cr == NULL);
	mrep = nd->nd_mrep;
	md = nd->nd_md;
	dpos = nd->nd_dpos;
	if (has_header) {
		nfsm_dissect(tl, u_int32_t *, 10 * NFSX_UNSIGNED);
		nd->nd_retxid = fxdr_unsigned(u_int32_t, *tl++);
		if (*tl++ != rpc_call) {
			m_freem(mrep);
			return (EBADRPC);
		}
	} else
		nfsm_dissect(tl, u_int32_t *, 8 * NFSX_UNSIGNED);
	nd->nd_repstat = 0;
	nd->nd_flag = 0;
	if (*tl++ != rpc_vers) {
		nd->nd_repstat = ERPCMISMATCH;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	if (*tl != nfs_prog) {
		nd->nd_repstat = EPROGUNAVAIL;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	tl++;
	nfsvers = fxdr_unsigned(u_int32_t, *tl++);
	if (nfsvers < NFS_VER2 || nfsvers > NFS_VER3) {
		nd->nd_repstat = EPROGMISMATCH;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	if (nfsvers == NFS_VER3)
		nd->nd_flag = ND_NFSV3;
	nd->nd_procnum = fxdr_unsigned(u_int32_t, *tl++);
	if (nd->nd_procnum == NFSPROC_NULL)
		return (0);
	if (nd->nd_procnum > NFSPROC_COMMIT ||
	    (!nd->nd_flag && nd->nd_procnum > NFSV2PROC_STATFS)) {
		nd->nd_repstat = EPROCUNAVAIL;
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}
	if ((nd->nd_flag & ND_NFSV3) == 0)
		nd->nd_procnum = nfsv3_procid[nd->nd_procnum];
	auth_type = *tl++;
	len = fxdr_unsigned(int, *tl++);
	if (len < 0 || len > RPCAUTH_MAXSIZ) {
		m_freem(mrep);
		return (EBADRPC);
	}

	nd->nd_flag &= ~ND_KERBAUTH;
	/*
	 * Handle auth_unix or auth_kerb.
	 */
	if (auth_type == rpc_auth_unix) {
		uid_t uid;
		gid_t gid;

		nd->nd_cr = kauth_cred_alloc();
		len = fxdr_unsigned(int, *++tl);
		if (len < 0 || len > NFS_MAXNAMLEN) {
			m_freem(mrep);
			error = EBADRPC;
			goto errout;
		}
		nfsm_adv(nfsm_rndup(len));
		nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);

		uid = fxdr_unsigned(uid_t, *tl++);
		gid = fxdr_unsigned(gid_t, *tl++);
		kauth_cred_setuid(nd->nd_cr, uid);
		kauth_cred_seteuid(nd->nd_cr, uid);
		kauth_cred_setsvuid(nd->nd_cr, uid);
		kauth_cred_setgid(nd->nd_cr, gid);
		kauth_cred_setegid(nd->nd_cr, gid);
		kauth_cred_setsvgid(nd->nd_cr, gid);

		len = fxdr_unsigned(int, *tl);
		if (len < 0 || len > RPCAUTH_UNIXGIDS) {
			m_freem(mrep);
			error = EBADRPC;
			goto errout;
		}
		nfsm_dissect(tl, u_int32_t *, (len + 2) * NFSX_UNSIGNED);

		if (len > 0) {
			size_t grbuf_size = min(len, NGROUPS) * sizeof(gid_t);
			gid_t *grbuf = kmem_alloc(grbuf_size, KM_SLEEP);

			for (i = 0; i < len; i++) {
				if (i < NGROUPS) /* XXX elad */
					grbuf[i] = fxdr_unsigned(gid_t, *tl++);
				else
					tl++;
			}
			kauth_cred_setgroups(nd->nd_cr, grbuf,
			    min(len, NGROUPS), -1, UIO_SYSSPACE);
			kmem_free(grbuf, grbuf_size);
		}

		len = fxdr_unsigned(int, *++tl);
		if (len < 0 || len > RPCAUTH_MAXSIZ) {
			m_freem(mrep);
			error = EBADRPC;
			goto errout;
		}
		if (len > 0)
			nfsm_adv(nfsm_rndup(len));
	} else if (auth_type == rpc_auth_kerb) {
		switch (fxdr_unsigned(int, *tl++)) {
		case RPCAKN_FULLNAME:
			ticklen = fxdr_unsigned(int, *tl);
			*((u_int32_t *)nfsd->nfsd_authstr) = *tl;
			uio.uio_resid = nfsm_rndup(ticklen) + NFSX_UNSIGNED;
			nfsd->nfsd_authlen = uio.uio_resid + NFSX_UNSIGNED;
			if (uio.uio_resid > (len - 2 * NFSX_UNSIGNED)) {
				m_freem(mrep);
				error = EBADRPC;
				goto errout;
			}
			uio.uio_offset = 0;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			UIO_SETUP_SYSSPACE(&uio);
			iov.iov_base = (void *)&nfsd->nfsd_authstr[4];
			iov.iov_len = RPCAUTH_MAXSIZ - 4;
			nfsm_mtouio(&uio, uio.uio_resid);
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (*tl++ != rpc_auth_kerb ||
				fxdr_unsigned(int, *tl) != 4 * NFSX_UNSIGNED) {
				printf("Bad kerb verifier\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			nfsm_dissect(cp, void *, 4 * NFSX_UNSIGNED);
			tl = (u_int32_t *)cp;
			if (fxdr_unsigned(int, *tl) != RPCAKN_FULLNAME) {
				printf("Not fullname kerb verifier\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			cp += NFSX_UNSIGNED;
			memcpy(nfsd->nfsd_verfstr, cp, 3 * NFSX_UNSIGNED);
			nfsd->nfsd_verflen = 3 * NFSX_UNSIGNED;
			nd->nd_flag |= ND_KERBFULL;
			nfsd->nfsd_flag |= NFSD_NEEDAUTH;
			break;
		case RPCAKN_NICKNAME:
			if (len != 2 * NFSX_UNSIGNED) {
				printf("Kerb nickname short\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADCRED);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			nickuid = fxdr_unsigned(uid_t, *tl);
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			if (*tl++ != rpc_auth_kerb ||
				fxdr_unsigned(int, *tl) != 3 * NFSX_UNSIGNED) {
				printf("Kerb nick verifier bad\n");
				nd->nd_repstat = (NFSERR_AUTHERR|AUTH_BADVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			tvin.tv_sec = *tl++;
			tvin.tv_usec = *tl;

			LIST_FOREACH(nuidp, NUIDHASH(nfsd->nfsd_slp, nickuid),
			    nu_hash) {
				if (kauth_cred_geteuid(nuidp->nu_cr) == nickuid &&
				    (!nd->nd_nam2 ||
				     netaddr_match(NU_NETFAM(nuidp),
				      &nuidp->nu_haddr, nd->nd_nam2)))
					break;
			}
			if (!nuidp) {
				nd->nd_repstat =
					(NFSERR_AUTHERR|AUTH_REJECTCRED);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}

			/*
			 * Now, decrypt the timestamp using the session key
			 * and validate it.
			 */
#ifdef NFSKERB
			XXX
#else
			(void)tvin.tv_sec;
#endif

			tvout.tv_sec = fxdr_unsigned(long, tvout.tv_sec);
			tvout.tv_usec = fxdr_unsigned(long, tvout.tv_usec);
			if (nuidp->nu_expire < time_second ||
			    nuidp->nu_timestamp.tv_sec > tvout.tv_sec ||
			    (nuidp->nu_timestamp.tv_sec == tvout.tv_sec &&
			     nuidp->nu_timestamp.tv_usec > tvout.tv_usec)) {
				nuidp->nu_expire = 0;
				nd->nd_repstat =
				    (NFSERR_AUTHERR|AUTH_REJECTVERF);
				nd->nd_procnum = NFSPROC_NOOP;
				return (0);
			}
			kauth_cred_hold(nuidp->nu_cr);
			nd->nd_cr = nuidp->nu_cr;
			nd->nd_flag |= ND_KERBNICK;
		}
	} else {
		nd->nd_repstat = (NFSERR_AUTHERR | AUTH_REJECTCRED);
		nd->nd_procnum = NFSPROC_NOOP;
		return (0);
	}

	nd->nd_md = md;
	nd->nd_dpos = dpos;
	KASSERT((nd->nd_cr == NULL && (nfsd->nfsd_flag & NFSD_NEEDAUTH) != 0)
	     || (nd->nd_cr != NULL && (nfsd->nfsd_flag & NFSD_NEEDAUTH) == 0));
	return (0);
nfsmout:
errout:
	KASSERT(error != 0);
	if (nd->nd_cr != NULL) {
		kauth_cred_free(nd->nd_cr);
		nd->nd_cr = NULL;
	}
	return (error);
}

int
nfs_msg(struct lwp *l, const char *server, const char *msg)
{
	tpr_t tpr;

#if 0 /* XXX nfs_timer can't block on proc_lock */
	if (l)
		tpr = tprintf_open(l->l_proc);
	else
#endif
		tpr = NULL;
	tprintf(tpr, "nfs server %s: %s\n", server, msg);
	tprintf_close(tpr);
	return (0);
}

static struct pool nfs_srvdesc_pool;

void
nfsdreq_init(void)
{

	pool_init(&nfs_srvdesc_pool, sizeof(struct nfsrv_descript),
	    0, 0, 0, "nfsrvdescpl", &pool_allocator_nointr, IPL_NONE);
}

void
nfsdreq_fini(void)
{

	pool_destroy(&nfs_srvdesc_pool);
}

struct nfsrv_descript *
nfsdreq_alloc(void)
{
	struct nfsrv_descript *nd;

	nd = pool_get(&nfs_srvdesc_pool, PR_WAITOK);
	nd->nd_cr = NULL;
	return nd;
}

void
nfsdreq_free(struct nfsrv_descript *nd)
{
	kauth_cred_t cr;

	cr = nd->nd_cr;
	if (cr != NULL) {
		kauth_cred_free(cr);
	}
	pool_put(&nfs_srvdesc_pool, nd);
}
