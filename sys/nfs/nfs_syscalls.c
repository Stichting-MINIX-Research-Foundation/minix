/*	$NetBSD: nfs_syscalls.c,v 1.156 2015/06/22 10:35:00 mrg Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: nfs_syscalls.c,v 1.156 2015/06/22 10:35:00 mrg Exp $");

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

extern int32_t (*nfsrv3_procs[NFS_NPROCS])(struct nfsrv_descript *,
						struct nfssvc_sock *,
						struct lwp *, struct mbuf **);
extern int nfsrvw_procrastinate;
extern int nuidhash_max;

static int nfs_numnfsd = 0;
static struct nfsdrt nfsdrt;
kmutex_t nfsd_lock;
struct nfssvc_sockhead nfssvc_sockhead;
kcondvar_t nfsd_initcv;
struct nfssvc_sockhead nfssvc_sockpending;
struct nfsdhead nfsd_head;
struct nfsdidlehead nfsd_idle_head;

int nfssvc_sockhead_flag;
int nfsd_head_flag;

struct nfssvc_sock *nfs_udpsock;
struct nfssvc_sock *nfs_udp6sock;

static struct nfssvc_sock *nfsrv_sockalloc(void);
static void nfsrv_sockfree(struct nfssvc_sock *);
static void nfsd_rt(int, struct nfsrv_descript *, int);
static int nfssvc_nfsd(struct nfssvc_copy_ops *, struct nfsd_srvargs *, void *,
		struct lwp *);

static int nfssvc_addsock_in(struct nfsd_args *, const void *);
static int nfssvc_setexports_in(struct mountd_exports_list *, const void *);
static int nfssvc_nsd_in(struct nfsd_srvargs *, const void *);
static int nfssvc_nsd_out(void *, const struct nfsd_srvargs *);
static int nfssvc_exp_in(struct export_args *, const void *, size_t);

static int
nfssvc_addsock_in(struct nfsd_args *nfsdarg, const void *argp)
{

	return copyin(argp, nfsdarg, sizeof *nfsdarg);
}

static int
nfssvc_setexports_in(struct mountd_exports_list *mel, const void *argp)
{

	return copyin(argp, mel, sizeof *mel);
}

static int
nfssvc_nsd_in(struct nfsd_srvargs *nsd, const void *argp)
{

	return copyin(argp, nsd, sizeof *nsd);
}

static int
nfssvc_nsd_out(void *argp, const struct nfsd_srvargs *nsd)
{

	return copyout(nsd, argp, sizeof *nsd);
}

static int
nfssvc_exp_in(struct export_args *exp, const void *argp, size_t nexports)
{

	return copyin(argp, exp, sizeof(*exp) * nexports);
}

/*
 * NFS server system calls
 */

static struct nfssvc_copy_ops native_ops = {
	.addsock_in = nfssvc_addsock_in,
	.setexports_in = nfssvc_setexports_in,
	.nsd_in = nfssvc_nsd_in,
	.nsd_out = nfssvc_nsd_out,
	.exp_in = nfssvc_exp_in,
};

/*
 * Nfs server pseudo system call for the nfsd's
 * Based on the flag value it either:
 * - adds a socket to the selection list
 * - remains in the kernel as an nfsd
 * - remains in the kernel as an nfsiod
 */

int
sys_nfssvc(struct lwp *l, const struct sys_nfssvc_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) flag;
		syscallarg(void *) argp;
	} */
	int	flag = SCARG(uap, flag);
	void	*argp = SCARG(uap, argp);

	return do_nfssvc(&native_ops, l, flag, argp, retval);
}

int
do_nfssvc(struct nfssvc_copy_ops *ops, struct lwp *l, int flag, void *argp, register_t *retval)
{
	int error;
	file_t *fp;
	struct mbuf *nam;
	struct nfsd_args nfsdarg;
	struct nfsd_srvargs nfsd_srvargs, *nsd = &nfsd_srvargs;
	struct nfsd *nfsd;
	struct nfssvc_sock *slp;
	struct nfsuid *nuidp;

	error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_NFS,
	    KAUTH_REQ_NETWORK_NFS_SVC, NULL, NULL, NULL);
	if (error)
		return (error);

	mutex_enter(&nfsd_lock);
	while (nfssvc_sockhead_flag & SLP_INIT) {
		cv_wait(&nfsd_initcv, &nfsd_lock);
	}
	mutex_exit(&nfsd_lock);

	if (flag & NFSSVC_BIOD) {
		/* Dummy implementation of nfsios for 1.4 and earlier. */
		error = kpause("nfsbiod", true, 0, NULL);
	} else if (flag & NFSSVC_MNTD) {
		error = ENOSYS;
	} else if (flag & NFSSVC_ADDSOCK) {
		error = ops->addsock_in(&nfsdarg, argp);
		if (error)
			return (error);
		/* getsock() will use the descriptor for us */
		if ((fp = fd_getfile(nfsdarg.sock)) == NULL)
			return (EBADF);
		if (fp->f_type != DTYPE_SOCKET) {
			fd_putfile(nfsdarg.sock);
			return (ENOTSOCK);
		}
		/*
		 * Get the client address for connected sockets.
		 */
		if (nfsdarg.name == NULL || nfsdarg.namelen == 0)
			nam = (struct mbuf *)0;
		else {
			error = sockargs(&nam, nfsdarg.name, nfsdarg.namelen,
				MT_SONAME);
			if (error) {
				fd_putfile(nfsdarg.sock);
				return (error);
			}
		}
		error = nfssvc_addsock(fp, nam);
		fd_putfile(nfsdarg.sock);
	} else if (flag & NFSSVC_SETEXPORTSLIST) {
		struct export_args *args;
		struct mountd_exports_list mel;

		error = ops->setexports_in(&mel, argp);
		if (error != 0)
			return error;

		args = (struct export_args *)malloc(mel.mel_nexports *
		    sizeof(struct export_args), M_TEMP, M_WAITOK);
		error = ops->exp_in(args, mel.mel_exports, mel.mel_nexports);
		if (error != 0) {
			free(args, M_TEMP);
			return error;
		}
		mel.mel_exports = args;

		error = mountd_set_exports_list(&mel, l, NULL);

		free(args, M_TEMP);
	} else {
		error = ops->nsd_in(nsd, argp);
		if (error)
			return (error);
		if ((flag & NFSSVC_AUTHIN) &&
		    ((nfsd = nsd->nsd_nfsd)) != NULL &&
		    (nfsd->nfsd_slp->ns_flags & SLP_VALID)) {
			slp = nfsd->nfsd_slp;

			/*
			 * First check to see if another nfsd has already
			 * added this credential.
			 */
			LIST_FOREACH(nuidp, NUIDHASH(slp, nsd->nsd_cr.cr_uid),
			    nu_hash) {
				if (kauth_cred_geteuid(nuidp->nu_cr) ==
				    nsd->nsd_cr.cr_uid &&
				    (!nfsd->nfsd_nd->nd_nam2 ||
				     netaddr_match(NU_NETFAM(nuidp),
				     &nuidp->nu_haddr, nfsd->nfsd_nd->nd_nam2)))
					break;
			}
			if (nuidp) {
			    kauth_cred_hold(nuidp->nu_cr);
			    nfsd->nfsd_nd->nd_cr = nuidp->nu_cr;
			    nfsd->nfsd_nd->nd_flag |= ND_KERBFULL;
			} else {
			    /*
			     * Nope, so we will.
			     */
			    if (slp->ns_numuids < nuidhash_max) {
				slp->ns_numuids++;
				nuidp = kmem_alloc(sizeof(*nuidp), KM_SLEEP);
			    } else
				nuidp = (struct nfsuid *)0;
			    if ((slp->ns_flags & SLP_VALID) == 0) {
				if (nuidp)
				    kmem_free(nuidp, sizeof(*nuidp));
			    } else {
				if (nuidp == (struct nfsuid *)0) {
				    nuidp = TAILQ_FIRST(&slp->ns_uidlruhead);
				    LIST_REMOVE(nuidp, nu_hash);
				    TAILQ_REMOVE(&slp->ns_uidlruhead, nuidp,
					nu_lru);
				    if (nuidp->nu_flag & NU_NAM)
					m_freem(nuidp->nu_nam);
			        }
				nuidp->nu_flag = 0;
				kauth_uucred_to_cred(nuidp->nu_cr,
				    &nsd->nsd_cr);
				nuidp->nu_timestamp = nsd->nsd_timestamp;
				nuidp->nu_expire = time_second + nsd->nsd_ttl;
				/*
				 * and save the session key in nu_key.
				 */
				memcpy(nuidp->nu_key, nsd->nsd_key,
				    sizeof(nsd->nsd_key));
				if (nfsd->nfsd_nd->nd_nam2) {
				    struct sockaddr_in *saddr;

				    saddr = mtod(nfsd->nfsd_nd->nd_nam2,
					 struct sockaddr_in *);
				    switch (saddr->sin_family) {
				    case AF_INET:
					nuidp->nu_flag |= NU_INETADDR;
					nuidp->nu_inetaddr =
					     saddr->sin_addr.s_addr;
					break;
				    case AF_INET6:
					nuidp->nu_flag |= NU_NAM;
					nuidp->nu_nam = m_copym(
					    nfsd->nfsd_nd->nd_nam2, 0,
					     M_COPYALL, M_WAIT);
					break;
				    default:
					return EAFNOSUPPORT;
				    };
				}
				TAILQ_INSERT_TAIL(&slp->ns_uidlruhead, nuidp,
					nu_lru);
				LIST_INSERT_HEAD(NUIDHASH(slp, nsd->nsd_uid),
					nuidp, nu_hash);
				kauth_cred_hold(nuidp->nu_cr);
				nfsd->nfsd_nd->nd_cr = nuidp->nu_cr;
				nfsd->nfsd_nd->nd_flag |= ND_KERBFULL;
			    }
			}
		}
		if ((flag & NFSSVC_AUTHINFAIL) &&
		    (nfsd = nsd->nsd_nfsd))
			nfsd->nfsd_flag |= NFSD_AUTHFAIL;
		error = nfssvc_nfsd(ops, nsd, argp, l);
	}
	if (error == EINTR || error == ERESTART)
		error = 0;
	return (error);
}

static struct nfssvc_sock *
nfsrv_sockalloc(void)
{
	struct nfssvc_sock *slp;

	slp = kmem_alloc(sizeof(*slp), KM_SLEEP);
	memset(slp, 0, sizeof (struct nfssvc_sock));
	mutex_init(&slp->ns_lock, MUTEX_DRIVER, IPL_SOFTNET);
	mutex_init(&slp->ns_alock, MUTEX_DRIVER, IPL_SOFTNET);
	cv_init(&slp->ns_cv, "nfsdsock");
	TAILQ_INIT(&slp->ns_uidlruhead);
	LIST_INIT(&slp->ns_tq);
	SIMPLEQ_INIT(&slp->ns_sendq);
	mutex_enter(&nfsd_lock);
	TAILQ_INSERT_TAIL(&nfssvc_sockhead, slp, ns_chain);
	mutex_exit(&nfsd_lock);

	return slp;
}

static void
nfsrv_sockfree(struct nfssvc_sock *slp)
{

	KASSERT(slp->ns_so == NULL);
	KASSERT(slp->ns_fp == NULL);
	KASSERT((slp->ns_flags & SLP_VALID) == 0);
	mutex_destroy(&slp->ns_lock);
	mutex_destroy(&slp->ns_alock);
	cv_destroy(&slp->ns_cv);
	kmem_free(slp, sizeof(*slp));
}

/*
 * Adds a socket to the list for servicing by nfsds.
 */
int
nfssvc_addsock(file_t *fp, struct mbuf *mynam)
{
	int siz;
	struct nfssvc_sock *slp;
	struct socket *so;
	struct nfssvc_sock *tslp;
	int error;
	int val;

	so = fp->f_socket;
	tslp = (struct nfssvc_sock *)0;
	/*
	 * Add it to the list, as required.
	 */
	if (so->so_proto->pr_protocol == IPPROTO_UDP) {
		if (so->so_proto->pr_domain->dom_family == AF_INET6)
			tslp = nfs_udp6sock;
		else {
			tslp = nfs_udpsock;
			if (tslp->ns_flags & SLP_VALID) {
				m_freem(mynam);
				return (EPERM);
			}
		}
	}
	if (so->so_type == SOCK_STREAM)
		siz = NFS_MAXPACKET + sizeof (u_long);
	else
		siz = NFS_MAXPACKET;
	solock(so);
	error = soreserve(so, siz, siz);
	sounlock(so);
	if (error) {
		m_freem(mynam);
		return (error);
	}

	/*
	 * Set protocol specific options { for now TCP only } and
	 * reserve some space. For datagram sockets, this can get called
	 * repeatedly for the same socket, but that isn't harmful.
	 */
	if (so->so_type == SOCK_STREAM) {
		val = 1;
		so_setsockopt(NULL, so, SOL_SOCKET, SO_KEEPALIVE, &val,
		    sizeof(val));
	}
	if ((so->so_proto->pr_domain->dom_family == AF_INET ||
	    so->so_proto->pr_domain->dom_family == AF_INET6) &&
	    so->so_proto->pr_protocol == IPPROTO_TCP) {
		val = 1;
		so_setsockopt(NULL, so, IPPROTO_TCP, TCP_NODELAY, &val,
		    sizeof(val));
	}
	solock(so);
	so->so_rcv.sb_flags &= ~SB_NOINTR;
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_flags &= ~SB_NOINTR;
	so->so_snd.sb_timeo = 0;
	sounlock(so);
	if (tslp) {
		slp = tslp;
	} else {
		slp = nfsrv_sockalloc();
	}
	slp->ns_so = so;
	slp->ns_nam = mynam;
	mutex_enter(&fp->f_lock);
	fp->f_count++;
	mutex_exit(&fp->f_lock);
	slp->ns_fp = fp;
	slp->ns_flags = SLP_VALID;
	slp->ns_aflags = SLP_A_NEEDQ;
	slp->ns_gflags = 0;
	slp->ns_sflags = 0;
	solock(so);
	so->so_upcallarg = (void *)slp;
	so->so_upcall = nfsrv_soupcall;
	so->so_rcv.sb_flags |= SB_UPCALL;
	sounlock(so);
	nfsrv_wakenfsd(slp);
	return (0);
}

/*
 * Called by nfssvc() for nfsds. Just loops around servicing rpc requests
 * until it is killed by a signal.
 */
static int
nfssvc_nfsd(struct nfssvc_copy_ops *ops, struct nfsd_srvargs *nsd,
	    void *argp, struct lwp *l)
{
	struct timeval tv;
	struct mbuf *m;
	struct nfssvc_sock *slp;
	struct nfsd *nfsd = nsd->nsd_nfsd;
	struct nfsrv_descript *nd = NULL;
	struct mbuf *mreq;
	u_quad_t cur_usec;
	int error = 0, cacherep, siz, sotype, writes_todo;
	struct proc *p = l->l_proc;
	bool doreinit;

#ifndef nolint
	cacherep = RC_DOIT;
	writes_todo = 0;
#endif
	if (nfsd == NULL) {
		nsd->nsd_nfsd = nfsd = kmem_alloc(sizeof(*nfsd), KM_SLEEP);
		memset(nfsd, 0, sizeof (struct nfsd));
		cv_init(&nfsd->nfsd_cv, "nfsd");
		nfsd->nfsd_procp = p;
		mutex_enter(&nfsd_lock);
		while ((nfssvc_sockhead_flag & SLP_INIT) != 0) {
			KASSERT(nfs_numnfsd == 0);
			cv_wait(&nfsd_initcv, &nfsd_lock);
		}
		TAILQ_INSERT_TAIL(&nfsd_head, nfsd, nfsd_chain);
		nfs_numnfsd++;
		mutex_exit(&nfsd_lock);
	}
	/*
	 * Loop getting rpc requests until SIGKILL.
	 */
	for (;;) {
		bool dummy;

		if ((curcpu()->ci_schedstate.spc_flags & SPCF_SHOULDYIELD)
		    != 0) {
			preempt();
		}
		if (nfsd->nfsd_slp == NULL) {
			mutex_enter(&nfsd_lock);
			while (nfsd->nfsd_slp == NULL &&
			    (nfsd_head_flag & NFSD_CHECKSLP) == 0) {
				SLIST_INSERT_HEAD(&nfsd_idle_head, nfsd,
				    nfsd_idle);
				error = cv_wait_sig(&nfsd->nfsd_cv, &nfsd_lock);
				if (error) {
					slp = nfsd->nfsd_slp;
					nfsd->nfsd_slp = NULL;
					if (!slp)
						SLIST_REMOVE(&nfsd_idle_head,
						    nfsd, nfsd, nfsd_idle);
					mutex_exit(&nfsd_lock);
					if (slp) {
						nfsrv_wakenfsd(slp);
						nfsrv_slpderef(slp);
					}
					goto done;
				}
			}
			if (nfsd->nfsd_slp == NULL &&
			    (nfsd_head_flag & NFSD_CHECKSLP) != 0) {
				slp = TAILQ_FIRST(&nfssvc_sockpending);
				if (slp) {
					KASSERT((slp->ns_gflags & SLP_G_DOREC)
					    != 0);
					TAILQ_REMOVE(&nfssvc_sockpending, slp,
					    ns_pending);
					slp->ns_gflags &= ~SLP_G_DOREC;
					slp->ns_sref++;
					nfsd->nfsd_slp = slp;
				} else
					nfsd_head_flag &= ~NFSD_CHECKSLP;
			}
			KASSERT(nfsd->nfsd_slp == NULL ||
			    nfsd->nfsd_slp->ns_sref > 0);
			mutex_exit(&nfsd_lock);
			if ((slp = nfsd->nfsd_slp) == NULL)
				continue;
			if (slp->ns_flags & SLP_VALID) {
				bool more;

				if (nfsdsock_testbits(slp, SLP_A_NEEDQ)) {
					nfsrv_rcv(slp);
				}
				if (nfsdsock_testbits(slp, SLP_A_DISCONN)) {
					nfsrv_zapsock(slp);
				}
				error = nfsrv_dorec(slp, nfsd, &nd, &more);
				getmicrotime(&tv);
				cur_usec = (u_quad_t)tv.tv_sec * 1000000 +
					(u_quad_t)tv.tv_usec;
				writes_todo = 0;
				if (error) {
					struct nfsrv_descript *nd2;

					mutex_enter(&nfsd_lock);
					nd2 = LIST_FIRST(&slp->ns_tq);
					if (nd2 != NULL &&
					    nd2->nd_time <= cur_usec) {
						error = 0;
						cacherep = RC_DOIT;
						writes_todo = 1;
					}
					mutex_exit(&nfsd_lock);
				}
				if (error == 0 && more) {
					nfsrv_wakenfsd(slp);
				}
			}
		} else {
			error = 0;
			slp = nfsd->nfsd_slp;
		}
		KASSERT(slp != NULL);
		KASSERT(nfsd->nfsd_slp == slp);
		if (error || (slp->ns_flags & SLP_VALID) == 0) {
			if (nd) {
				nfsdreq_free(nd);
				nd = NULL;
			}
			nfsd->nfsd_slp = NULL;
			nfsrv_slpderef(slp);
			continue;
		}
		sotype = slp->ns_so->so_type;
		if (nd) {
			getmicrotime(&nd->nd_starttime);
			if (nd->nd_nam2)
				nd->nd_nam = nd->nd_nam2;
			else
				nd->nd_nam = slp->ns_nam;

			/*
			 * Check to see if authorization is needed.
			 */
			if (nfsd->nfsd_flag & NFSD_NEEDAUTH) {
				nfsd->nfsd_flag &= ~NFSD_NEEDAUTH;
				nsd->nsd_haddr = mtod(nd->nd_nam,
				    struct sockaddr_in *)->sin_addr.s_addr;
				nsd->nsd_authlen = nfsd->nfsd_authlen;
				nsd->nsd_verflen = nfsd->nfsd_verflen;
				if (!copyout(nfsd->nfsd_authstr,
				    nsd->nsd_authstr, nfsd->nfsd_authlen) &&
				    !copyout(nfsd->nfsd_verfstr,
				    nsd->nsd_verfstr, nfsd->nfsd_verflen) &&
				    !ops->nsd_out(argp, nsd)) {
					return (ENEEDAUTH);
				}
				cacherep = RC_DROPIT;
			} else
				cacherep = nfsrv_getcache(nd, slp, &mreq);

			if (nfsd->nfsd_flag & NFSD_AUTHFAIL) {
				nfsd->nfsd_flag &= ~NFSD_AUTHFAIL;
				nd->nd_procnum = NFSPROC_NOOP;
				nd->nd_repstat =
				    (NFSERR_AUTHERR | AUTH_TOOWEAK);
				cacherep = RC_DOIT;
			}
		}

		/*
		 * Loop to get all the write rpc relies that have been
		 * gathered together.
		 */
		do {
			switch (cacherep) {
			case RC_DOIT:
				mreq = NULL;
				netexport_rdlock();
				if (writes_todo || nd == NULL ||
				     (!(nd->nd_flag & ND_NFSV3) &&
				     nd->nd_procnum == NFSPROC_WRITE &&
				     nfsrvw_procrastinate > 0))
					error = nfsrv_writegather(&nd, slp,
					    l, &mreq);
				else
					error =
					    (*(nfsrv3_procs[nd->nd_procnum]))
					    (nd, slp, l, &mreq);
				netexport_rdunlock();
				if (mreq == NULL) {
					if (nd != NULL) {
						if (nd->nd_nam2)
							m_free(nd->nd_nam2);
					}
					break;
				}
				if (error) {
					nfsstats.srv_errs++;
					if (nd) {
						nfsrv_updatecache(nd, false,
						    mreq);
						if (nd->nd_nam2)
							m_freem(nd->nd_nam2);
					}
					break;
				}
				if (nd) {
					nfsstats.srvrpccnt[nd->nd_procnum]++;
					nfsrv_updatecache(nd, true, mreq);
					nd->nd_mrep = NULL;
				}
			case RC_REPLY:
				m = mreq;
				siz = 0;
				while (m) {
					siz += m->m_len;
					m = m->m_next;
				}
				if (siz <= 0 || siz > NFS_MAXPACKET) {
					printf("mbuf siz=%d\n",siz);
					panic("Bad nfs svc reply");
				}
				m = mreq;
				m->m_pkthdr.len = siz;
				m->m_pkthdr.rcvif = (struct ifnet *)0;
				/*
				 * For stream protocols, prepend a Sun RPC
				 * Record Mark.
				 */
				if (sotype == SOCK_STREAM) {
					M_PREPEND(m, NFSX_UNSIGNED, M_WAIT);
					*mtod(m, u_int32_t *) =
					    htonl(0x80000000 | siz);
				}
				if (nd) {
					nd->nd_mreq = m;
					if (nfsrtton) {
						nfsd_rt(slp->ns_so->so_type, nd,
						    cacherep);
					}
					error = nfsdsock_sendreply(slp, nd);
					nd = NULL;
				}
				if (error == EPIPE)
					nfsrv_zapsock(slp);
				if (error == EINTR || error == ERESTART) {
					nfsd->nfsd_slp = NULL;
					nfsrv_slpderef(slp);
					goto done;
				}
				break;
			case RC_DROPIT:
				if (nd) {
					if (nfsrtton)
						nfsd_rt(sotype, nd, cacherep);
					m_freem(nd->nd_mrep);
					m_freem(nd->nd_nam2);
				}
				break;
			}
			if (nd) {
				nfsdreq_free(nd);
				nd = NULL;
			}

			/*
			 * Check to see if there are outstanding writes that
			 * need to be serviced.
			 */
			getmicrotime(&tv);
			cur_usec = (u_quad_t)tv.tv_sec * 1000000 +
			    (u_quad_t)tv.tv_usec;
			mutex_enter(&nfsd_lock);
			if (LIST_FIRST(&slp->ns_tq) &&
			    LIST_FIRST(&slp->ns_tq)->nd_time <= cur_usec) {
				cacherep = RC_DOIT;
				writes_todo = 1;
			} else
				writes_todo = 0;
			mutex_exit(&nfsd_lock);
		} while (writes_todo);
		if (nfsrv_dorec(slp, nfsd, &nd, &dummy)) {
			nfsd->nfsd_slp = NULL;
			nfsrv_slpderef(slp);
		}
	}
done:
	mutex_enter(&nfsd_lock);
	TAILQ_REMOVE(&nfsd_head, nfsd, nfsd_chain);
	doreinit = --nfs_numnfsd == 0;
	if (doreinit)
		nfssvc_sockhead_flag |= SLP_INIT;
	mutex_exit(&nfsd_lock);
	cv_destroy(&nfsd->nfsd_cv);
	kmem_free(nfsd, sizeof(*nfsd));
	nsd->nsd_nfsd = NULL;
	if (doreinit)
		nfsrv_init(true);	/* Reinitialize everything */
	return (error);
}

/*
 * Shut down a socket associated with an nfssvc_sock structure.
 * Should be called with the send lock set, if required.
 * The trick here is to increment the sref at the start, so that the nfsds
 * will stop using it and clear ns_flag at the end so that it will not be
 * reassigned during cleanup.
 *
 * called at splsoftnet.
 */
void
nfsrv_zapsock(struct nfssvc_sock *slp)
{
	struct nfsuid *nuidp, *nnuidp;
	struct nfsrv_descript *nwp;
	struct socket *so;
	struct mbuf *m;

	if (nfsdsock_drain(slp)) {
		return;
	}
	mutex_enter(&nfsd_lock);
	if (slp->ns_gflags & SLP_G_DOREC) {
		TAILQ_REMOVE(&nfssvc_sockpending, slp, ns_pending);
		slp->ns_gflags &= ~SLP_G_DOREC;
	}
	mutex_exit(&nfsd_lock);

	so = slp->ns_so;
	KASSERT(so != NULL);
	solock(so);
	so->so_upcall = NULL;
	so->so_upcallarg = NULL;
	so->so_rcv.sb_flags &= ~SB_UPCALL;
	soshutdown(so, SHUT_RDWR);
	sounlock(so);

	m_freem(slp->ns_raw);
	m = slp->ns_rec;
	while (m != NULL) {
		struct mbuf *n;

		n = m->m_nextpkt;
		m_freem(m);
		m = n;
	}
	/* XXX what about freeing ns_frag ? */
	for (nuidp = TAILQ_FIRST(&slp->ns_uidlruhead); nuidp != 0;
	    nuidp = nnuidp) {
		nnuidp = TAILQ_NEXT(nuidp, nu_lru);
		LIST_REMOVE(nuidp, nu_hash);
		TAILQ_REMOVE(&slp->ns_uidlruhead, nuidp, nu_lru);
		if (nuidp->nu_flag & NU_NAM)
			m_freem(nuidp->nu_nam);
		kmem_free(nuidp, sizeof(*nuidp));
	}
	mutex_enter(&nfsd_lock);
	while ((nwp = LIST_FIRST(&slp->ns_tq)) != NULL) {
		LIST_REMOVE(nwp, nd_tq);
		mutex_exit(&nfsd_lock);
		nfsdreq_free(nwp);
		mutex_enter(&nfsd_lock);
	}
	mutex_exit(&nfsd_lock);
}

/*
 * Derefence a server socket structure. If it has no more references and
 * is no longer valid, you can throw it away.
 */
void
nfsrv_slpderef(struct nfssvc_sock *slp)
{
	uint32_t ref;

	mutex_enter(&nfsd_lock);
	KASSERT(slp->ns_sref > 0);
	ref = --slp->ns_sref;
	if (ref == 0 && (slp->ns_flags & SLP_VALID) == 0) {
		file_t *fp;

		KASSERT((slp->ns_gflags & SLP_G_DOREC) == 0);
		TAILQ_REMOVE(&nfssvc_sockhead, slp, ns_chain);
		mutex_exit(&nfsd_lock);

		fp = slp->ns_fp;
		if (fp != NULL) {
			slp->ns_fp = NULL;
			KASSERT(fp != NULL);
			KASSERT(fp->f_socket == slp->ns_so);
			KASSERT(fp->f_count > 0);
			closef(fp);
			slp->ns_so = NULL;
		}
		
		if (slp->ns_nam)
			m_free(slp->ns_nam);
		nfsrv_sockfree(slp);
	} else
		mutex_exit(&nfsd_lock);
}

/*
 * Initialize the data structures for the server.
 * Handshake with any new nfsds starting up to avoid any chance of
 * corruption.
 */
void
nfsrv_init(int terminating)
{
	struct nfssvc_sock *slp;

	if (!terminating) {
		mutex_init(&nfsd_lock, MUTEX_DRIVER, IPL_SOFTNET);
		cv_init(&nfsd_initcv, "nfsdinit");
	}

	mutex_enter(&nfsd_lock);
	if (!terminating && (nfssvc_sockhead_flag & SLP_INIT) != 0)
		panic("nfsd init");
	nfssvc_sockhead_flag |= SLP_INIT;

	if (terminating) {
		KASSERT(SLIST_EMPTY(&nfsd_idle_head));
		KASSERT(TAILQ_EMPTY(&nfsd_head));
		while ((slp = TAILQ_FIRST(&nfssvc_sockhead)) != NULL) {
			mutex_exit(&nfsd_lock);
			KASSERT(slp->ns_sref == 0);
			slp->ns_sref++;
			nfsrv_zapsock(slp);
			nfsrv_slpderef(slp);
			mutex_enter(&nfsd_lock);
		}
		KASSERT(TAILQ_EMPTY(&nfssvc_sockpending));
		mutex_exit(&nfsd_lock);
		nfsrv_cleancache();	/* And clear out server cache */
	} else {
		mutex_exit(&nfsd_lock);
		nfs_pub.np_valid = 0;
	}

	TAILQ_INIT(&nfssvc_sockhead);
	TAILQ_INIT(&nfssvc_sockpending);

	TAILQ_INIT(&nfsd_head);
	SLIST_INIT(&nfsd_idle_head);
	nfsd_head_flag &= ~NFSD_CHECKSLP;

	nfs_udpsock = nfsrv_sockalloc();
	nfs_udp6sock = nfsrv_sockalloc();

	mutex_enter(&nfsd_lock);
	nfssvc_sockhead_flag &= ~SLP_INIT;
	cv_broadcast(&nfsd_initcv);
	mutex_exit(&nfsd_lock);
}

void
nfsrv_fini(void)
{

	nfsrv_init(true);
	cv_destroy(&nfsd_initcv);
	mutex_destroy(&nfsd_lock);
}	

/*
 * Add entries to the server monitor log.
 */
static void
nfsd_rt(int sotype, struct nfsrv_descript *nd, int cacherep)
{
	struct timeval tv;
	struct drt *rt;

	rt = &nfsdrt.drt[nfsdrt.pos];
	if (cacherep == RC_DOIT)
		rt->flag = 0;
	else if (cacherep == RC_REPLY)
		rt->flag = DRT_CACHEREPLY;
	else
		rt->flag = DRT_CACHEDROP;
	if (sotype == SOCK_STREAM)
		rt->flag |= DRT_TCP;
	if (nd->nd_flag & ND_NFSV3)
		rt->flag |= DRT_NFSV3;
	rt->proc = nd->nd_procnum;
	if (mtod(nd->nd_nam, struct sockaddr *)->sa_family == AF_INET)
	    rt->ipadr = mtod(nd->nd_nam, struct sockaddr_in *)->sin_addr.s_addr;
	else
	    rt->ipadr = INADDR_ANY;
	getmicrotime(&tv);
	rt->resptime = ((tv.tv_sec - nd->nd_starttime.tv_sec) * 1000000) +
		(tv.tv_usec - nd->nd_starttime.tv_usec);
	rt->tstamp = tv;
	nfsdrt.pos = (nfsdrt.pos + 1) % NFSRTTLOGSIZ;
}
