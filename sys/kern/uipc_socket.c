/*	$NetBSD: uipc_socket.c,v 1.246 2015/08/24 22:21:26 pooka Exp $	*/

/*-
 * Copyright (c) 2002, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of Wasabi Systems, Inc, and by Andrew Doran.
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
 * Copyright (c) 2004 The FreeBSD Foundation
 * Copyright (c) 2004 Robert Watson
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)uipc_socket.c	8.6 (Berkeley) 5/2/95
 */

/*
 * Socket operation routines.
 *
 * These routines are called by the routines in sys_socket.c or from a
 * system process, and implement the semantics of socket operations by
 * switching out to the protocol specific routines.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_socket.c,v 1.246 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_sock_counters.h"
#include "opt_sosend_loan.h"
#include "opt_mbuftrace.h"
#include "opt_somaxkva.h"
#include "opt_multiprocessor.h"	/* XXX */
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/uidinfo.h>
#include <sys/event.h>
#include <sys/poll.h>
#include <sys/kauth.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/kthread.h>

#ifdef COMPAT_50
#include <compat/sys/time.h>
#include <compat/sys/socket.h>
#endif

#include <uvm/uvm_extern.h>
#include <uvm/uvm_loan.h>
#include <uvm/uvm_page.h>

MALLOC_DEFINE(M_SONAME, "soname", "socket name");

extern const struct fileops socketops;

extern int	somaxconn;			/* patchable (XXX sysctl) */
int		somaxconn = SOMAXCONN;
kmutex_t	*softnet_lock;

#ifdef SOSEND_COUNTERS
#include <sys/device.h>

static struct evcnt sosend_loan_big = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "loan big");
static struct evcnt sosend_copy_big = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "copy big");
static struct evcnt sosend_copy_small = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "copy small");
static struct evcnt sosend_kvalimit = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "sosend", "kva limit");

#define	SOSEND_COUNTER_INCR(ev)		(ev)->ev_count++

EVCNT_ATTACH_STATIC(sosend_loan_big);
EVCNT_ATTACH_STATIC(sosend_copy_big);
EVCNT_ATTACH_STATIC(sosend_copy_small);
EVCNT_ATTACH_STATIC(sosend_kvalimit);
#else

#define	SOSEND_COUNTER_INCR(ev)		/* nothing */

#endif /* SOSEND_COUNTERS */

#if defined(SOSEND_NO_LOAN) || defined(MULTIPROCESSOR)
int sock_loan_thresh = -1;
#else
int sock_loan_thresh = 4096;
#endif

static kmutex_t so_pendfree_lock;
static struct mbuf *so_pendfree = NULL;

#ifndef SOMAXKVA
#define	SOMAXKVA (16 * 1024 * 1024)
#endif
int somaxkva = SOMAXKVA;
static int socurkva;
static kcondvar_t socurkva_cv;

static kauth_listener_t socket_listener;

#define	SOCK_LOAN_CHUNK		65536

static void sopendfree_thread(void *);
static kcondvar_t pendfree_thread_cv;
static lwp_t *sopendfree_lwp;

static void sysctl_kern_socket_setup(void);
static struct sysctllog *socket_sysctllog;

static vsize_t
sokvareserve(struct socket *so, vsize_t len)
{
	int error;

	mutex_enter(&so_pendfree_lock);
	while (socurkva + len > somaxkva) {
		SOSEND_COUNTER_INCR(&sosend_kvalimit);
		error = cv_wait_sig(&socurkva_cv, &so_pendfree_lock);
		if (error) {
			len = 0;
			break;
		}
	}
	socurkva += len;
	mutex_exit(&so_pendfree_lock);
	return len;
}

static void
sokvaunreserve(vsize_t len)
{

	mutex_enter(&so_pendfree_lock);
	socurkva -= len;
	cv_broadcast(&socurkva_cv);
	mutex_exit(&so_pendfree_lock);
}

/*
 * sokvaalloc: allocate kva for loan.
 */

vaddr_t
sokvaalloc(vaddr_t sva, vsize_t len, struct socket *so)
{
	vaddr_t lva;

	/*
	 * reserve kva.
	 */

	if (sokvareserve(so, len) == 0)
		return 0;

	/*
	 * allocate kva.
	 */

	lva = uvm_km_alloc(kernel_map, len, atop(sva) & uvmexp.colormask,
	    UVM_KMF_COLORMATCH | UVM_KMF_VAONLY | UVM_KMF_WAITVA);
	if (lva == 0) {
		sokvaunreserve(len);
		return (0);
	}

	return lva;
}

/*
 * sokvafree: free kva for loan.
 */

void
sokvafree(vaddr_t sva, vsize_t len)
{

	/*
	 * free kva.
	 */

	uvm_km_free(kernel_map, sva, len, UVM_KMF_VAONLY);

	/*
	 * unreserve kva.
	 */

	sokvaunreserve(len);
}

static void
sodoloanfree(struct vm_page **pgs, void *buf, size_t size)
{
	vaddr_t sva, eva;
	vsize_t len;
	int npgs;

	KASSERT(pgs != NULL);

	eva = round_page((vaddr_t) buf + size);
	sva = trunc_page((vaddr_t) buf);
	len = eva - sva;
	npgs = len >> PAGE_SHIFT;

	pmap_kremove(sva, len);
	pmap_update(pmap_kernel());
	uvm_unloan(pgs, npgs, UVM_LOAN_TOPAGE);
	sokvafree(sva, len);
}

/*
 * sopendfree_thread: free mbufs on "pendfree" list.
 * unlock and relock so_pendfree_lock when freeing mbufs.
 */

static void
sopendfree_thread(void *v)
{
	struct mbuf *m, *next;
	size_t rv;

	mutex_enter(&so_pendfree_lock);

	for (;;) {
		rv = 0;
		while (so_pendfree != NULL) {
			m = so_pendfree;
			so_pendfree = NULL;
			mutex_exit(&so_pendfree_lock);

			for (; m != NULL; m = next) {
				next = m->m_next;
				KASSERT((~m->m_flags & (M_EXT|M_EXT_PAGES)) == 0);
				KASSERT(m->m_ext.ext_refcnt == 0);

				rv += m->m_ext.ext_size;
				sodoloanfree(m->m_ext.ext_pgs, m->m_ext.ext_buf,
				    m->m_ext.ext_size);
				pool_cache_put(mb_cache, m);
			}

			mutex_enter(&so_pendfree_lock);
		}
		if (rv)
			cv_broadcast(&socurkva_cv);
		cv_wait(&pendfree_thread_cv, &so_pendfree_lock);
	}
	panic("sopendfree_thread");
	/* NOTREACHED */
}

void
soloanfree(struct mbuf *m, void *buf, size_t size, void *arg)
{

	KASSERT(m != NULL);

	/*
	 * postpone freeing mbuf.
	 *
	 * we can't do it in interrupt context
	 * because we need to put kva back to kernel_map.
	 */

	mutex_enter(&so_pendfree_lock);
	m->m_next = so_pendfree;
	so_pendfree = m;
	cv_signal(&pendfree_thread_cv);
	mutex_exit(&so_pendfree_lock);
}

static long
sosend_loan(struct socket *so, struct uio *uio, struct mbuf *m, long space)
{
	struct iovec *iov = uio->uio_iov;
	vaddr_t sva, eva;
	vsize_t len;
	vaddr_t lva;
	int npgs, error;
	vaddr_t va;
	int i;

	if (VMSPACE_IS_KERNEL_P(uio->uio_vmspace))
		return (0);

	if (iov->iov_len < (size_t) space)
		space = iov->iov_len;
	if (space > SOCK_LOAN_CHUNK)
		space = SOCK_LOAN_CHUNK;

	eva = round_page((vaddr_t) iov->iov_base + space);
	sva = trunc_page((vaddr_t) iov->iov_base);
	len = eva - sva;
	npgs = len >> PAGE_SHIFT;

	KASSERT(npgs <= M_EXT_MAXPAGES);

	lva = sokvaalloc(sva, len, so);
	if (lva == 0)
		return 0;

	error = uvm_loan(&uio->uio_vmspace->vm_map, sva, len,
	    m->m_ext.ext_pgs, UVM_LOAN_TOPAGE);
	if (error) {
		sokvafree(lva, len);
		return (0);
	}

	for (i = 0, va = lva; i < npgs; i++, va += PAGE_SIZE)
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(m->m_ext.ext_pgs[i]),
		    VM_PROT_READ, 0);
	pmap_update(pmap_kernel());

	lva += (vaddr_t) iov->iov_base & PAGE_MASK;

	MEXTADD(m, (void *) lva, space, M_MBUF, soloanfree, so);
	m->m_flags |= M_EXT_PAGES | M_EXT_ROMAP;

	uio->uio_resid -= space;
	/* uio_offset not updated, not set/used for write(2) */
	uio->uio_iov->iov_base = (char *)uio->uio_iov->iov_base + space;
	uio->uio_iov->iov_len -= space;
	if (uio->uio_iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}

	return (space);
}

struct mbuf *
getsombuf(struct socket *so, int type)
{
	struct mbuf *m;

	m = m_get(M_WAIT, type);
	MCLAIM(m, so->so_mowner);
	return m;
}

static int
socket_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result;
	enum kauth_network_req req;

	result = KAUTH_RESULT_DEFER;
	req = (enum kauth_network_req)arg0;

	if ((action != KAUTH_NETWORK_SOCKET) &&
	    (action != KAUTH_NETWORK_BIND))
		return result;

	switch (req) {
	case KAUTH_REQ_NETWORK_BIND_PORT:
		result = KAUTH_RESULT_ALLOW;
		break;

	case KAUTH_REQ_NETWORK_SOCKET_DROP: {
		/* Normal users can only drop their own connections. */
		struct socket *so = (struct socket *)arg1;

		if (so->so_cred && proc_uidmatch(cred, so->so_cred) == 0)
			result = KAUTH_RESULT_ALLOW;

		break;
		}

	case KAUTH_REQ_NETWORK_SOCKET_OPEN:
		/* We allow "raw" routing/bluetooth sockets to anyone. */
		if ((u_long)arg1 == PF_ROUTE || (u_long)arg1 == PF_OROUTE
		    || (u_long)arg1 == PF_BLUETOOTH) {
			result = KAUTH_RESULT_ALLOW;
		} else {
			/* Privileged, let secmodel handle this. */
			if ((u_long)arg2 == SOCK_RAW)
				break;
		}

		result = KAUTH_RESULT_ALLOW;

		break;

	case KAUTH_REQ_NETWORK_SOCKET_CANSEE:
		result = KAUTH_RESULT_ALLOW;

		break;

	default:
		break;
	}

	return result;
}

void
soinit(void)
{

	sysctl_kern_socket_setup();

	mutex_init(&so_pendfree_lock, MUTEX_DEFAULT, IPL_VM);
	softnet_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
	cv_init(&socurkva_cv, "sokva");
	cv_init(&pendfree_thread_cv, "sopendfr");
	soinit2();

	/* Set the initial adjusted socket buffer size. */
	if (sb_max_set(sb_max))
		panic("bad initial sb_max value: %lu", sb_max);

	socket_listener = kauth_listen_scope(KAUTH_SCOPE_NETWORK,
	    socket_listener_cb, NULL);
}

void
soinit1(void)
{
	int error = kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL,
	    sopendfree_thread, NULL, &sopendfree_lwp, "sopendfree");
	if (error)
		panic("soinit1 %d", error);
}

/*
 * socreate: create a new socket of the specified type and the protocol.
 *
 * => Caller may specify another socket for lock sharing (must not be held).
 * => Returns the new socket without lock held.
 */
int
socreate(int dom, struct socket **aso, int type, int proto, struct lwp *l,
	 struct socket *lockso)
{
	const struct protosw	*prp;
	struct socket	*so;
	uid_t		uid;
	int		error;
	kmutex_t	*lock;

	error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_SOCKET,
	    KAUTH_REQ_NETWORK_SOCKET_OPEN, KAUTH_ARG(dom), KAUTH_ARG(type),
	    KAUTH_ARG(proto));
	if (error != 0)
		return error;

	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);
	if (prp == NULL) {
		/* no support for domain */
		if (pffinddomain(dom) == 0)
			return EAFNOSUPPORT;
		/* no support for socket type */
		if (proto == 0 && type != 0)
			return EPROTOTYPE;
		return EPROTONOSUPPORT;
	}
	if (prp->pr_usrreqs == NULL)
		return EPROTONOSUPPORT;
	if (prp->pr_type != type)
		return EPROTOTYPE;

	so = soget(true);
	so->so_type = type;
	so->so_proto = prp;
	so->so_send = sosend;
	so->so_receive = soreceive;
#ifdef MBUFTRACE
	so->so_rcv.sb_mowner = &prp->pr_domain->dom_mowner;
	so->so_snd.sb_mowner = &prp->pr_domain->dom_mowner;
	so->so_mowner = &prp->pr_domain->dom_mowner;
#endif
	uid = kauth_cred_geteuid(l->l_cred);
	so->so_uidinfo = uid_find(uid);
	so->so_cpid = l->l_proc->p_pid;

	/*
	 * Lock assigned and taken during PCB attach, unless we share
	 * the lock with another socket, e.g. socketpair(2) case.
	 */
	if (lockso) {
		lock = lockso->so_lock;
		so->so_lock = lock;
		mutex_obj_hold(lock);
		mutex_enter(lock);
	}

	/* Attach the PCB (returns with the socket lock held). */
	error = (*prp->pr_usrreqs->pr_attach)(so, proto);
	KASSERT(solocked(so));

	if (error) {
		KASSERT(so->so_pcb == NULL);
		so->so_state |= SS_NOFDREF;
		sofree(so);
		return error;
	}
	so->so_cred = kauth_cred_dup(l->l_cred);
	sounlock(so);

	*aso = so;
	return 0;
}

/*
 * fsocreate: create a socket and a file descriptor associated with it.
 *
 * => On success, write file descriptor to fdout and return zero.
 * => On failure, return non-zero; *fdout will be undefined.
 */
int
fsocreate(int domain, struct socket **sop, int type, int proto, int *fdout)
{
	lwp_t *l = curlwp;
	int error, fd, flags;
	struct socket *so;
	struct file *fp;

	if ((error = fd_allocfile(&fp, &fd)) != 0) {
		return error;
	}
	flags = type & SOCK_FLAGS_MASK;
	fd_set_exclose(l, fd, (flags & SOCK_CLOEXEC) != 0);
	fp->f_flag = FREAD|FWRITE|((flags & SOCK_NONBLOCK) ? FNONBLOCK : 0)|
	    ((flags & SOCK_NOSIGPIPE) ? FNOSIGPIPE : 0);
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;

	type &= ~SOCK_FLAGS_MASK;
	error = socreate(domain, &so, type, proto, l, NULL);
	if (error) {
		fd_abort(curproc, fp, fd);
		return error;
	}
	if (flags & SOCK_NONBLOCK) {
		so->so_state |= SS_NBIO;
	}
	fp->f_socket = so;
	fd_affix(curproc, fp, fd);

	if (sop != NULL) {
		*sop = so;
	}
	*fdout = fd;
	return error;
}

int
sofamily(const struct socket *so)
{
	const struct protosw *pr;
	const struct domain *dom;

	if ((pr = so->so_proto) == NULL)
		return AF_UNSPEC;
	if ((dom = pr->pr_domain) == NULL)
		return AF_UNSPEC;
	return dom->dom_family;
}

int
sobind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	int	error;

	solock(so);
	if (nam->sa_family != so->so_proto->pr_domain->dom_family) {
		sounlock(so);
		return EAFNOSUPPORT;
	}
	error = (*so->so_proto->pr_usrreqs->pr_bind)(so, nam, l);
	sounlock(so);
	return error;
}

int
solisten(struct socket *so, int backlog, struct lwp *l)
{
	int	error;

	solock(so);
	if ((so->so_state & (SS_ISCONNECTED | SS_ISCONNECTING | 
	    SS_ISDISCONNECTING)) != 0) {
		sounlock(so);
		return EINVAL;
	}
	error = (*so->so_proto->pr_usrreqs->pr_listen)(so, l);
	if (error != 0) {
		sounlock(so);
		return error;
	}
	if (TAILQ_EMPTY(&so->so_q))
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0)
		backlog = 0;
	so->so_qlimit = min(backlog, somaxconn);
	sounlock(so);
	return 0;
}

void
sofree(struct socket *so)
{
	u_int refs;

	KASSERT(solocked(so));

	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0) {
		sounlock(so);
		return;
	}
	if (so->so_head) {
		/*
		 * We must not decommission a socket that's on the accept(2)
		 * queue.  If we do, then accept(2) may hang after select(2)
		 * indicated that the listening socket was ready.
		 */
		if (!soqremque(so, 0)) {
			sounlock(so);
			return;
		}
	}
	if (so->so_rcv.sb_hiwat)
		(void)chgsbsize(so->so_uidinfo, &so->so_rcv.sb_hiwat, 0,
		    RLIM_INFINITY);
	if (so->so_snd.sb_hiwat)
		(void)chgsbsize(so->so_uidinfo, &so->so_snd.sb_hiwat, 0,
		    RLIM_INFINITY);
	sbrelease(&so->so_snd, so);
	KASSERT(!cv_has_waiters(&so->so_cv));
	KASSERT(!cv_has_waiters(&so->so_rcv.sb_cv));
	KASSERT(!cv_has_waiters(&so->so_snd.sb_cv));
	sorflush(so);
	refs = so->so_aborting;	/* XXX */
	/* Remove acccept filter if one is present. */
	if (so->so_accf != NULL)
		(void)accept_filt_clear(so);
	sounlock(so);
	if (refs == 0)		/* XXX */
		soput(so);
}

/*
 * soclose: close a socket on last file table reference removal.
 * Initiate disconnect if connected.  Free socket when disconnect complete.
 */
int
soclose(struct socket *so)
{
	struct socket *so2;
	int error = 0;

	solock(so);
	if (so->so_options & SO_ACCEPTCONN) {
		for (;;) {
			if ((so2 = TAILQ_FIRST(&so->so_q0)) != 0) {
				KASSERT(solocked2(so, so2));
				(void) soqremque(so2, 0);
				/* soabort drops the lock. */
				(void) soabort(so2);
				solock(so);
				continue;
			}
			if ((so2 = TAILQ_FIRST(&so->so_q)) != 0) {
				KASSERT(solocked2(so, so2));
				(void) soqremque(so2, 1);
				/* soabort drops the lock. */
				(void) soabort(so2);
				solock(so);
				continue;
			}
			break;
		}
	}
	if (so->so_pcb == NULL)
		goto discard;
	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			if (error)
				goto drop;
		}
		if (so->so_options & SO_LINGER) {
			if ((so->so_state & (SS_ISDISCONNECTING|SS_NBIO)) ==
			    (SS_ISDISCONNECTING|SS_NBIO))
				goto drop;
			while (so->so_state & SS_ISCONNECTED) {
				error = sowait(so, true, so->so_linger * hz);
				if (error)
					break;
			}
		}
	}
 drop:
	if (so->so_pcb) {
		KASSERT(solocked(so));
		(*so->so_proto->pr_usrreqs->pr_detach)(so);
	}
 discard:
	KASSERT((so->so_state & SS_NOFDREF) == 0);
	kauth_cred_free(so->so_cred);
	so->so_state |= SS_NOFDREF;
	sofree(so);
	return error;
}

/*
 * Must be called with the socket locked..  Will return with it unlocked.
 */
int
soabort(struct socket *so)
{
	u_int refs;
	int error;
	
	KASSERT(solocked(so));
	KASSERT(so->so_head == NULL);

	so->so_aborting++;		/* XXX */
	error = (*so->so_proto->pr_usrreqs->pr_abort)(so);
	refs = --so->so_aborting;	/* XXX */
	if (error || (refs == 0)) {
		sofree(so);
	} else {
		sounlock(so);
	}
	return error;
}

int
soaccept(struct socket *so, struct sockaddr *nam)
{
	int error;

	KASSERT(solocked(so));
	KASSERT((so->so_state & SS_NOFDREF) != 0);

	so->so_state &= ~SS_NOFDREF;
	if ((so->so_state & SS_ISDISCONNECTED) == 0 ||
	    (so->so_proto->pr_flags & PR_ABRTACPTDIS) == 0)
		error = (*so->so_proto->pr_usrreqs->pr_accept)(so, nam);
	else
		error = ECONNABORTED;

	return error;
}

int
soconnect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	int error;

	KASSERT(solocked(so));

	if (so->so_options & SO_ACCEPTCONN)
		return EOPNOTSUPP;
	/*
	 * If protocol is connection-based, can only connect once.
	 * Otherwise, if connected, try to disconnect first.
	 * This allows user to disconnect by connecting to, e.g.,
	 * a null address.
	 */
	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
	    (error = sodisconnect(so)))) {
		error = EISCONN;
	} else {
		if (nam->sa_family != so->so_proto->pr_domain->dom_family) {
			return EAFNOSUPPORT;
		}
		error = (*so->so_proto->pr_usrreqs->pr_connect)(so, nam, l);
	}

	return error;
}

int
soconnect2(struct socket *so1, struct socket *so2)
{
	KASSERT(solocked2(so1, so2));

	return (*so1->so_proto->pr_usrreqs->pr_connect2)(so1, so2);
}

int
sodisconnect(struct socket *so)
{
	int	error;

	KASSERT(solocked(so));

	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
	} else if (so->so_state & SS_ISDISCONNECTING) {
		error = EALREADY;
	} else {
		error = (*so->so_proto->pr_usrreqs->pr_disconnect)(so);
	}
	return (error);
}

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)
/*
 * Send on a socket.
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * Lock against other senders.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 * The data to be sent is described by "uio" if nonzero,
 * otherwise by the mbuf chain "top" (which must be null
 * if uio is not).  Data provided in mbuf chain must be small
 * enough to send all at once.
 *
 * Returns nonzero on error, timeout or signal; callers
 * must check for short counts if EINTR/ERESTART are returned.
 * Data and control buffers are freed on return.
 */
int
sosend(struct socket *so, struct sockaddr *addr, struct uio *uio,
	struct mbuf *top, struct mbuf *control, int flags, struct lwp *l)
{
	struct mbuf	**mp, *m;
	long		space, len, resid, clen, mlen;
	int		error, s, dontroute, atomic;
	short		wakeup_state = 0;

	clen = 0;

	/*
	 * solock() provides atomicity of access.  splsoftnet() prevents
	 * protocol processing soft interrupts from interrupting us and
	 * blocking (expensive).
	 */
	s = splsoftnet();
	solock(so);
	atomic = sosendallatonce(so) || top;
	if (uio)
		resid = uio->uio_resid;
	else
		resid = top->m_pkthdr.len;
	/*
	 * In theory resid should be unsigned.
	 * However, space must be signed, as it might be less than 0
	 * if we over-committed, and we must use a signed comparison
	 * of space and resid.  On the other hand, a negative resid
	 * causes us to loop sending 0-length segments to the protocol.
	 */
	if (resid < 0) {
		error = EINVAL;
		goto out;
	}
	dontroute =
	    (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 &&
	    (so->so_proto->pr_flags & PR_ATOMIC);
	l->l_ru.ru_msgsnd++;
	if (control)
		clen = control->m_len;
 restart:
	if ((error = sblock(&so->so_snd, SBLOCKWAIT(flags))) != 0)
		goto out;
	do {
		if (so->so_state & SS_CANTSENDMORE) {
			error = EPIPE;
			goto release;
		}
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			goto release;
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
				if (resid || clen == 0) {
					error = ENOTCONN;
					goto release;
				}
			} else if (addr == NULL) {
				error = EDESTADDRREQ;
				goto release;
			}
		}
		space = sbspace(&so->so_snd);
		if (flags & MSG_OOB)
			space += 1024;
		if ((atomic && resid > so->so_snd.sb_hiwat) ||
		    clen > so->so_snd.sb_hiwat) {
			error = EMSGSIZE;
			goto release;
		}
		if (space < resid + clen &&
		    (atomic || space < so->so_snd.sb_lowat || space < clen)) {
			if ((so->so_state & SS_NBIO) || (flags & MSG_NBIO)) {
				error = EWOULDBLOCK;
				goto release;
			}
			sbunlock(&so->so_snd);
			if (wakeup_state & SS_RESTARTSYS) {
				error = ERESTART;
				goto out;
			}
			error = sbwait(&so->so_snd);
			if (error)
				goto out;
			wakeup_state = so->so_state;
			goto restart;
		}
		wakeup_state = 0;
		mp = &top;
		space -= clen;
		do {
			if (uio == NULL) {
				/*
				 * Data is prepackaged in "top".
				 */
				resid = 0;
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
			} else do {
				sounlock(so);
				splx(s);
				if (top == NULL) {
					m = m_gethdr(M_WAIT, MT_DATA);
					mlen = MHLEN;
					m->m_pkthdr.len = 0;
					m->m_pkthdr.rcvif = NULL;
				} else {
					m = m_get(M_WAIT, MT_DATA);
					mlen = MLEN;
				}
				MCLAIM(m, so->so_snd.sb_mowner);
				if (sock_loan_thresh >= 0 &&
				    uio->uio_iov->iov_len >= sock_loan_thresh &&
				    space >= sock_loan_thresh &&
				    (len = sosend_loan(so, uio, m,
						       space)) != 0) {
					SOSEND_COUNTER_INCR(&sosend_loan_big);
					space -= len;
					goto have_data;
				}
				if (resid >= MINCLSIZE && space >= MCLBYTES) {
					SOSEND_COUNTER_INCR(&sosend_copy_big);
					m_clget(m, M_DONTWAIT);
					if ((m->m_flags & M_EXT) == 0)
						goto nopages;
					mlen = MCLBYTES;
					if (atomic && top == 0) {
						len = lmin(MCLBYTES - max_hdr,
						    resid);
						m->m_data += max_hdr;
					} else
						len = lmin(MCLBYTES, resid);
					space -= len;
				} else {
 nopages:
					SOSEND_COUNTER_INCR(&sosend_copy_small);
					len = lmin(lmin(mlen, resid), space);
					space -= len;
					/*
					 * For datagram protocols, leave room
					 * for protocol headers in first mbuf.
					 */
					if (atomic && top == 0 && len < mlen)
						MH_ALIGN(m, len);
				}
				error = uiomove(mtod(m, void *), (int)len, uio);
 have_data:
				resid = uio->uio_resid;
				m->m_len = len;
				*mp = m;
				top->m_pkthdr.len += len;
				s = splsoftnet();
				solock(so);
				if (error != 0)
					goto release;
				mp = &m->m_next;
				if (resid <= 0) {
					if (flags & MSG_EOR)
						top->m_flags |= M_EOR;
					break;
				}
			} while (space > 0 && atomic);

			if (so->so_state & SS_CANTSENDMORE) {
				error = EPIPE;
				goto release;
			}
			if (dontroute)
				so->so_options |= SO_DONTROUTE;
			if (resid > 0)
				so->so_state |= SS_MORETOCOME;
			if (flags & MSG_OOB) {
				error = (*so->so_proto->pr_usrreqs->pr_sendoob)(so,
				    top, control);
			} else {
				error = (*so->so_proto->pr_usrreqs->pr_send)(so,
				    top, addr, control, l);
			}
			if (dontroute)
				so->so_options &= ~SO_DONTROUTE;
			if (resid > 0)
				so->so_state &= ~SS_MORETOCOME;
			clen = 0;
			control = NULL;
			top = NULL;
			mp = &top;
			if (error != 0)
				goto release;
		} while (resid && space > 0);
	} while (resid);

 release:
	sbunlock(&so->so_snd);
 out:
	sounlock(so);
	splx(s);
	if (top)
		m_freem(top);
	if (control)
		m_freem(control);
	return (error);
}

/*
 * Following replacement or removal of the first mbuf on the first
 * mbuf chain of a socket buffer, push necessary state changes back
 * into the socket buffer so that other consumers see the values
 * consistently.  'nextrecord' is the callers locally stored value of
 * the original value of sb->sb_mb->m_nextpkt which must be restored
 * when the lead mbuf changes.  NOTE: 'nextrecord' may be NULL.
 */
static void
sbsync(struct sockbuf *sb, struct mbuf *nextrecord)
{

	KASSERT(solocked(sb->sb_so));

	/*
	 * First, update for the new value of nextrecord.  If necessary,
	 * make it the first record.
	 */
	if (sb->sb_mb != NULL)
		sb->sb_mb->m_nextpkt = nextrecord;
	else
		sb->sb_mb = nextrecord;

        /*
         * Now update any dependent socket buffer fields to reflect
         * the new state.  This is an inline of SB_EMPTY_FIXUP, with
         * the addition of a second clause that takes care of the
         * case where sb_mb has been updated, but remains the last
         * record.
         */
        if (sb->sb_mb == NULL) {
                sb->sb_mbtail = NULL;
                sb->sb_lastrecord = NULL;
        } else if (sb->sb_mb->m_nextpkt == NULL)
                sb->sb_lastrecord = sb->sb_mb;
}

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * The caller may receive the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  The uio is then used
 * only for the count in uio_resid.
 */
int
soreceive(struct socket *so, struct mbuf **paddr, struct uio *uio,
	struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct lwp *l = curlwp;
	struct mbuf	*m, **mp, *mt;
	size_t len, offset, moff, orig_resid;
	int atomic, flags, error, s, type;
	const struct protosw	*pr;
	struct mbuf	*nextrecord;
	int		mbuf_removed = 0;
	const struct domain *dom;
	short		wakeup_state = 0;

	pr = so->so_proto;
	atomic = pr->pr_flags & PR_ATOMIC;
	dom = pr->pr_domain;
	mp = mp0;
	type = 0;
	orig_resid = uio->uio_resid;

	if (paddr != NULL)
		*paddr = NULL;
	if (controlp != NULL)
		*controlp = NULL;
	if (flagsp != NULL)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;

	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		solock(so);
		error = (*pr->pr_usrreqs->pr_recvoob)(so, m, flags & MSG_PEEK);
		sounlock(so);
		if (error)
			goto bad;
		do {
			error = uiomove(mtod(m, void *),
			    MIN(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid > 0 && error == 0 && m);
 bad:
		if (m != NULL)
			m_freem(m);
		return error;
	}
	if (mp != NULL)
		*mp = NULL;

	/*
	 * solock() provides atomicity of access.  splsoftnet() prevents
	 * protocol processing soft interrupts from interrupting us and
	 * blocking (expensive).
	 */
	s = splsoftnet();
	solock(so);
 restart:
	if ((error = sblock(&so->so_rcv, SBLOCKWAIT(flags))) != 0) {
		sounlock(so);
		splx(s);
		return error;
	}

	m = so->so_rcv.sb_mb;
	/*
	 * If we have less data than requested, block awaiting more
	 * (subject to any timeout) if:
	 *   1. the current count is less than the low water mark,
	 *   2. MSG_WAITALL is set, and it is possible to do the entire
	 *	receive operation at once if we block (resid <= hiwat), or
	 *   3. MSG_DONTWAIT is not set.
	 * If MSG_WAITALL is set but resid is larger than the receive buffer,
	 * we have to do the receive in sections, and thus risk returning
	 * a short count if a timeout or signal occurs after we start.
	 */
	if (m == NULL ||
	    ((flags & MSG_DONTWAIT) == 0 &&
	     so->so_rcv.sb_cc < uio->uio_resid &&
	     (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	      ((flags & MSG_WAITALL) &&
	       uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	     m->m_nextpkt == NULL && !atomic)) {
#ifdef DIAGNOSTIC
		if (m == NULL && so->so_rcv.sb_cc)
			panic("receive 1");
#endif
		if (so->so_error) {
			if (m != NULL)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			if (m != NULL)
				goto dontblock;
			else
				goto release;
		}
		for (; m != NULL; m = m->m_next)
			if (m->m_type == MT_OOBDATA  || (m->m_flags & M_EOR)) {
				m = so->so_rcv.sb_mb;
				goto dontblock;
			}
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) ||
		    (flags & (MSG_DONTWAIT|MSG_NBIO))) {
			error = EWOULDBLOCK;
			goto release;
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 1");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 1");
		sbunlock(&so->so_rcv);
		if (wakeup_state & SS_RESTARTSYS)
			error = ERESTART;
		else
			error = sbwait(&so->so_rcv);
		if (error != 0) {
			sounlock(so);
			splx(s);
			return error;
		}
		wakeup_state = so->so_state;
		goto restart;
	}
 dontblock:
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * From this point onward, we maintain 'nextrecord' as a cache of the
	 * pointer to the next record in the socket buffer.  We must keep the
	 * various socket buffer pointers and local stack versions of the
	 * pointers in sync, pushing out modifications before dropping the
	 * socket lock, and re-reading them when picking it up.
	 *
	 * Otherwise, we will race with the network stack appending new data
	 * or records onto the socket buffer by using inconsistent/stale
	 * versions of the field, possibly resulting in socket buffer
	 * corruption.
	 *
	 * By holding the high-level sblock(), we prevent simultaneous
	 * readers from pulling off the front of the socket buffer.
	 */
	if (l != NULL)
		l->l_ru.ru_msgrcv++;
	KASSERT(m == so->so_rcv.sb_mb);
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 1");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 1");
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
#endif
		orig_resid = 0;
		if (flags & MSG_PEEK) {
			if (paddr)
				*paddr = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			mbuf_removed = 1;
			if (paddr != NULL) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = NULL;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
			sbsync(&so->so_rcv, nextrecord);
		}
	}

	/*
	 * Process one or more MT_CONTROL mbufs present before any data mbufs
	 * in the first mbuf chain on the socket buffer.  If MSG_PEEK, we
	 * just copy the data; if !MSG_PEEK, we call into the protocol to
	 * perform externalization (or freeing if controlp == NULL).
	 */
	if (__predict_false(m != NULL && m->m_type == MT_CONTROL)) {
		struct mbuf *cm = NULL, *cmn;
		struct mbuf **cme = &cm;

		do {
			if (flags & MSG_PEEK) {
				if (controlp != NULL) {
					*controlp = m_copy(m, 0, m->m_len);
					controlp = &(*controlp)->m_next;
				}
				m = m->m_next;
			} else {
				sbfree(&so->so_rcv, m);
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = NULL;
				*cme = m;
				cme = &(*cme)->m_next;
				m = so->so_rcv.sb_mb;
			}
		} while (m != NULL && m->m_type == MT_CONTROL);
		if ((flags & MSG_PEEK) == 0)
			sbsync(&so->so_rcv, nextrecord);
		for (; cm != NULL; cm = cmn) {
			cmn = cm->m_next;
			cm->m_next = NULL;
			type = mtod(cm, struct cmsghdr *)->cmsg_type;
			if (controlp != NULL) {
				if (dom->dom_externalize != NULL &&
				    type == SCM_RIGHTS) {
					sounlock(so);
					splx(s);
					error = (*dom->dom_externalize)(cm, l,
					    (flags & MSG_CMSG_CLOEXEC) ?
					    O_CLOEXEC : 0);
					s = splsoftnet();
					solock(so);
				}
				*controlp = cm;
				while (*controlp != NULL)
					controlp = &(*controlp)->m_next;
			} else {
				/*
				 * Dispose of any SCM_RIGHTS message that went
				 * through the read path rather than recv.
				 */
				if (dom->dom_dispose != NULL &&
				    type == SCM_RIGHTS) {
				    	sounlock(so);
					(*dom->dom_dispose)(cm);
					solock(so);
				}
				m_freem(cm);
			}
		}
		if (m != NULL)
			nextrecord = so->so_rcv.sb_mb->m_nextpkt;
		else
			nextrecord = so->so_rcv.sb_mb;
		orig_resid = 0;
	}

	/* If m is non-NULL, we have some data to read. */
	if (__predict_true(m != NULL)) {
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 2");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 2");

	moff = 0;
	offset = 0;
	while (m != NULL && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
#ifdef DIAGNOSTIC
		else if (m->m_type != MT_DATA && m->m_type != MT_HEADER)
			panic("receive 3");
#endif
		so->so_state &= ~SS_RCVATMARK;
		wakeup_state = 0;
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		/*
		 * If mp is set, just pass back the mbufs.
		 * Otherwise copy them out via the uio, then free.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (mp == NULL) {
			SBLASTRECORDCHK(&so->so_rcv, "soreceive uiomove");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive uiomove");
			sounlock(so);
			splx(s);
			error = uiomove(mtod(m, char *) + moff, len, uio);
			s = splsoftnet();
			solock(so);
			if (error != 0) {
				/*
				 * If any part of the record has been removed
				 * (such as the MT_SONAME mbuf, which will
				 * happen when PR_ADDR, and thus also
				 * PR_ATOMIC, is set), then drop the entire
				 * record to maintain the atomicity of the
				 * receive operation.
				 *
				 * This avoids a later panic("receive 1a")
				 * when compiled with DIAGNOSTIC.
				 */
				if (m && mbuf_removed && atomic)
					(void) sbdroprecord(&so->so_rcv);

				goto release;
			}
		} else
			uio->uio_resid -= len;
		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				sbfree(&so->so_rcv, m);
				if (mp) {
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = NULL;
				} else {
					MFREE(m, so->so_rcv.sb_mb);
					m = so->so_rcv.sb_mb;
				}
				/*
				 * If m != NULL, we also know that
				 * so->so_rcv.sb_mb != NULL.
				 */
				KASSERT(so->so_rcv.sb_mb == m);
				if (m) {
					m->m_nextpkt = nextrecord;
					if (nextrecord == NULL)
						so->so_rcv.sb_lastrecord = m;
				} else {
					so->so_rcv.sb_mb = nextrecord;
					SB_EMPTY_FIXUP(&so->so_rcv);
				}
				SBLASTRECORDCHK(&so->so_rcv, "soreceive 3");
				SBLASTMBUFCHK(&so->so_rcv, "soreceive 3");
			}
		} else if (flags & MSG_PEEK)
			moff += len;
		else {
			if (mp != NULL) {
				mt = m_copym(m, 0, len, M_NOWAIT);
				if (__predict_false(mt == NULL)) {
					sounlock(so);
					mt = m_copym(m, 0, len, M_WAIT);
					solock(so);
				}
				*mp = mt;
			}
			m->m_data += len;
			m->m_len -= len;
			so->so_rcv.sb_cc -= len;
		}
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else {
				offset += len;
				if (offset == so->so_oobmark)
					break;
			}
		}
		if (flags & MSG_EOR)
			break;
		/*
		 * If the MSG_WAITALL flag is set (for non-atomic socket),
		 * we must not quit until "uio->uio_resid == 0" or an error
		 * termination.  If a signal/timeout occurs, return
		 * with a short count but without error.
		 * Keep sockbuf locked against other readers.
		 */
		while (flags & MSG_WAITALL && m == NULL && uio->uio_resid > 0 &&
		    !sosendallatonce(so) && !nextrecord) {
			if (so->so_error || so->so_state & SS_CANTRCVMORE)
				break;
			/*
			 * If we are peeking and the socket receive buffer is
			 * full, stop since we can't get more data to peek at.
			 */
			if ((flags & MSG_PEEK) && sbspace(&so->so_rcv) <= 0)
				break;
			/*
			 * If we've drained the socket buffer, tell the
			 * protocol in case it needs to do something to
			 * get it filled again.
			 */
			if ((pr->pr_flags & PR_WANTRCVD) && so->so_pcb)
				(*pr->pr_usrreqs->pr_rcvd)(so, flags, l);
			SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 2");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 2");
			if (wakeup_state & SS_RESTARTSYS)
				error = ERESTART;
			else
				error = sbwait(&so->so_rcv);
			if (error != 0) {
				sbunlock(&so->so_rcv);
				sounlock(so);
				splx(s);
				return 0;
			}
			if ((m = so->so_rcv.sb_mb) != NULL)
				nextrecord = m->m_nextpkt;
			wakeup_state = so->so_state;
		}
	}

	if (m && atomic) {
		flags |= MSG_TRUNC;
		if ((flags & MSG_PEEK) == 0)
			(void) sbdroprecord(&so->so_rcv);
	}
	if ((flags & MSG_PEEK) == 0) {
		if (m == NULL) {
			/*
			 * First part is an inline SB_EMPTY_FIXUP().  Second
			 * part makes sure sb_lastrecord is up-to-date if
			 * there is still data in the socket buffer.
			 */
			so->so_rcv.sb_mb = nextrecord;
			if (so->so_rcv.sb_mb == NULL) {
				so->so_rcv.sb_mbtail = NULL;
				so->so_rcv.sb_lastrecord = NULL;
			} else if (nextrecord->m_nextpkt == NULL)
				so->so_rcv.sb_lastrecord = nextrecord;
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive 4");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive 4");
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreqs->pr_rcvd)(so, flags, l);
	}
	if (orig_resid == uio->uio_resid && orig_resid &&
	    (flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(&so->so_rcv);
		goto restart;
	}

	if (flagsp != NULL)
		*flagsp |= flags;
 release:
	sbunlock(&so->so_rcv);
	sounlock(so);
	splx(s);
	return error;
}

int
soshutdown(struct socket *so, int how)
{
	const struct protosw	*pr;
	int	error;

	KASSERT(solocked(so));

	pr = so->so_proto;
	if (!(how == SHUT_RD || how == SHUT_WR || how == SHUT_RDWR))
		return (EINVAL);

	if (how == SHUT_RD || how == SHUT_RDWR) {
		sorflush(so);
		error = 0;
	}
	if (how == SHUT_WR || how == SHUT_RDWR)
		error = (*pr->pr_usrreqs->pr_shutdown)(so);

	return error;
}

void
sorestart(struct socket *so)
{
	/*
	 * An application has called close() on an fd on which another
	 * of its threads has called a socket system call.
	 * Mark this and wake everyone up, and code that would block again
	 * instead returns ERESTART.
	 * On system call re-entry the fd is validated and EBADF returned.
	 * Any other fd will block again on the 2nd syscall.
	 */
	solock(so);
	so->so_state |= SS_RESTARTSYS;
	cv_broadcast(&so->so_cv);
	cv_broadcast(&so->so_snd.sb_cv);
	cv_broadcast(&so->so_rcv.sb_cv);
	sounlock(so);
}

void
sorflush(struct socket *so)
{
	struct sockbuf	*sb, asb;
	const struct protosw	*pr;

	KASSERT(solocked(so));

	sb = &so->so_rcv;
	pr = so->so_proto;
	socantrcvmore(so);
	sb->sb_flags |= SB_NOINTR;
	(void )sblock(sb, M_WAITOK);
	sbunlock(sb);
	asb = *sb;
	/*
	 * Clear most of the sockbuf structure, but leave some of the
	 * fields valid.
	 */
	memset(&sb->sb_startzero, 0,
	    sizeof(*sb) - offsetof(struct sockbuf, sb_startzero));
	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose) {
		sounlock(so);
		(*pr->pr_domain->dom_dispose)(asb.sb_mb);
		solock(so);
	}
	sbrelease(&asb, so);
}

/*
 * internal set SOL_SOCKET options
 */
static int
sosetopt1(struct socket *so, const struct sockopt *sopt)
{
	int error = EINVAL, opt;
	int optval = 0; /* XXX: gcc */
	struct linger l;
	struct timeval tv;

	switch ((opt = sopt->sopt_name)) {

	case SO_ACCEPTFILTER:
		error = accept_filt_setopt(so, sopt);
		KASSERT(solocked(so));
		break;

  	case SO_LINGER:
 		error = sockopt_get(sopt, &l, sizeof(l));
		solock(so);
 		if (error)
 			break;
 		if (l.l_linger < 0 || l.l_linger > USHRT_MAX ||
 		    l.l_linger > (INT_MAX / hz)) {
			error = EDOM;
			break;
		}
 		so->so_linger = l.l_linger;
 		if (l.l_onoff)
 			so->so_options |= SO_LINGER;
 		else
 			so->so_options &= ~SO_LINGER;
   		break;

	case SO_DEBUG:
	case SO_KEEPALIVE:
	case SO_DONTROUTE:
	case SO_USELOOPBACK:
	case SO_BROADCAST:
	case SO_REUSEADDR:
	case SO_REUSEPORT:
	case SO_OOBINLINE:
	case SO_TIMESTAMP:
	case SO_NOSIGPIPE:
#ifdef SO_OTIMESTAMP
	case SO_OTIMESTAMP:
#endif
		error = sockopt_getint(sopt, &optval);
		solock(so);
		if (error)
			break;
		if (optval)
			so->so_options |= opt;
		else
			so->so_options &= ~opt;
		break;

	case SO_SNDBUF:
	case SO_RCVBUF:
	case SO_SNDLOWAT:
	case SO_RCVLOWAT:
		error = sockopt_getint(sopt, &optval);
		solock(so);
		if (error)
			break;

		/*
		 * Values < 1 make no sense for any of these
		 * options, so disallow them.
		 */
		if (optval < 1) {
			error = EINVAL;
			break;
		}

		switch (opt) {
		case SO_SNDBUF:
			if (sbreserve(&so->so_snd, (u_long)optval, so) == 0) {
				error = ENOBUFS;
				break;
			}
			so->so_snd.sb_flags &= ~SB_AUTOSIZE;
			break;

		case SO_RCVBUF:
			if (sbreserve(&so->so_rcv, (u_long)optval, so) == 0) {
				error = ENOBUFS;
				break;
			}
			so->so_rcv.sb_flags &= ~SB_AUTOSIZE;
			break;

		/*
		 * Make sure the low-water is never greater than
		 * the high-water.
		 */
		case SO_SNDLOWAT:
			if (optval > so->so_snd.sb_hiwat)
				optval = so->so_snd.sb_hiwat;

			so->so_snd.sb_lowat = optval;
			break;

		case SO_RCVLOWAT:
			if (optval > so->so_rcv.sb_hiwat)
				optval = so->so_rcv.sb_hiwat;

			so->so_rcv.sb_lowat = optval;
			break;
		}
		break;

#ifdef COMPAT_50
	case SO_OSNDTIMEO:
	case SO_ORCVTIMEO: {
		struct timeval50 otv;
		error = sockopt_get(sopt, &otv, sizeof(otv));
		if (error) {
			solock(so);
			break;
		}
		timeval50_to_timeval(&otv, &tv);
		opt = opt == SO_OSNDTIMEO ? SO_SNDTIMEO : SO_RCVTIMEO;
		error = 0;
		/*FALLTHROUGH*/
	}
#endif /* COMPAT_50 */

	case SO_SNDTIMEO:
	case SO_RCVTIMEO:
		if (error)
			error = sockopt_get(sopt, &tv, sizeof(tv));
		solock(so);
		if (error)
			break;

		if (tv.tv_sec > (INT_MAX - tv.tv_usec / tick) / hz) {
			error = EDOM;
			break;
		}

		optval = tv.tv_sec * hz + tv.tv_usec / tick;
		if (optval == 0 && tv.tv_usec != 0)
			optval = 1;

		switch (opt) {
		case SO_SNDTIMEO:
			so->so_snd.sb_timeo = optval;
			break;
		case SO_RCVTIMEO:
			so->so_rcv.sb_timeo = optval;
			break;
		}
		break;

	default:
		solock(so);
		error = ENOPROTOOPT;
		break;
	}
	KASSERT(solocked(so));
	return error;
}

int
sosetopt(struct socket *so, struct sockopt *sopt)
{
	int error, prerr;

	if (sopt->sopt_level == SOL_SOCKET) {
		error = sosetopt1(so, sopt);
		KASSERT(solocked(so));
	} else {
		error = ENOPROTOOPT;
		solock(so);
	}

	if ((error == 0 || error == ENOPROTOOPT) &&
	    so->so_proto != NULL && so->so_proto->pr_ctloutput != NULL) {
		/* give the protocol stack a shot */
		prerr = (*so->so_proto->pr_ctloutput)(PRCO_SETOPT, so, sopt);
		if (prerr == 0)
			error = 0;
		else if (prerr != ENOPROTOOPT)
			error = prerr;
	}
	sounlock(so);
	return error;
}

/*
 * so_setsockopt() is a wrapper providing a sockopt structure for sosetopt()
 */
int
so_setsockopt(struct lwp *l, struct socket *so, int level, int name,
    const void *val, size_t valsize)
{
	struct sockopt sopt;
	int error;

	KASSERT(valsize == 0 || val != NULL);

	sockopt_init(&sopt, level, name, valsize);
	sockopt_set(&sopt, val, valsize);

	error = sosetopt(so, &sopt);

	sockopt_destroy(&sopt);

	return error;
}
 
/*
 * internal get SOL_SOCKET options
 */
static int
sogetopt1(struct socket *so, struct sockopt *sopt)
{
	int error, optval, opt;
	struct linger l;
	struct timeval tv;

	switch ((opt = sopt->sopt_name)) {

	case SO_ACCEPTFILTER:
		error = accept_filt_getopt(so, sopt);
		break;

	case SO_LINGER:
		l.l_onoff = (so->so_options & SO_LINGER) ? 1 : 0;
		l.l_linger = so->so_linger;

		error = sockopt_set(sopt, &l, sizeof(l));
		break;

	case SO_USELOOPBACK:
	case SO_DONTROUTE:
	case SO_DEBUG:
	case SO_KEEPALIVE:
	case SO_REUSEADDR:
	case SO_REUSEPORT:
	case SO_BROADCAST:
	case SO_OOBINLINE:
	case SO_TIMESTAMP:
	case SO_NOSIGPIPE:
#ifdef SO_OTIMESTAMP
	case SO_OTIMESTAMP:
#endif
	case SO_ACCEPTCONN:
		error = sockopt_setint(sopt, (so->so_options & opt) ? 1 : 0);
		break;

	case SO_TYPE:
		error = sockopt_setint(sopt, so->so_type);
		break;

	case SO_ERROR:
		error = sockopt_setint(sopt, so->so_error);
		so->so_error = 0;
		break;

	case SO_SNDBUF:
		error = sockopt_setint(sopt, so->so_snd.sb_hiwat);
		break;

	case SO_RCVBUF:
		error = sockopt_setint(sopt, so->so_rcv.sb_hiwat);
		break;

	case SO_SNDLOWAT:
		error = sockopt_setint(sopt, so->so_snd.sb_lowat);
		break;

	case SO_RCVLOWAT:
		error = sockopt_setint(sopt, so->so_rcv.sb_lowat);
		break;

#ifdef COMPAT_50
	case SO_OSNDTIMEO:
	case SO_ORCVTIMEO: {
		struct timeval50 otv;

		optval = (opt == SO_OSNDTIMEO ?
		     so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

		otv.tv_sec = optval / hz;
		otv.tv_usec = (optval % hz) * tick;

		error = sockopt_set(sopt, &otv, sizeof(otv));
		break;
	}
#endif /* COMPAT_50 */

	case SO_SNDTIMEO:
	case SO_RCVTIMEO:
		optval = (opt == SO_SNDTIMEO ?
		     so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

		tv.tv_sec = optval / hz;
		tv.tv_usec = (optval % hz) * tick;

		error = sockopt_set(sopt, &tv, sizeof(tv));
		break;

	case SO_OVERFLOWED:
		error = sockopt_setint(sopt, so->so_rcv.sb_overflowed);
		break;

	default:
		error = ENOPROTOOPT;
		break;
	}

	return (error);
}

int
sogetopt(struct socket *so, struct sockopt *sopt)
{
	int		error;

	solock(so);
	if (sopt->sopt_level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			error = ((*so->so_proto->pr_ctloutput)
			    (PRCO_GETOPT, so, sopt));
		} else
			error = (ENOPROTOOPT);
	} else {
		error = sogetopt1(so, sopt);
	}
	sounlock(so);
	return (error);
}

/*
 * alloc sockopt data buffer buffer
 *	- will be released at destroy
 */
static int
sockopt_alloc(struct sockopt *sopt, size_t len, km_flag_t kmflag)
{

	KASSERT(sopt->sopt_size == 0);

	if (len > sizeof(sopt->sopt_buf)) {
		sopt->sopt_data = kmem_zalloc(len, kmflag);
		if (sopt->sopt_data == NULL)
			return ENOMEM;
	} else
		sopt->sopt_data = sopt->sopt_buf;

	sopt->sopt_size = len;
	return 0;
}

/*
 * initialise sockopt storage
 *	- MAY sleep during allocation
 */
void
sockopt_init(struct sockopt *sopt, int level, int name, size_t size)
{

	memset(sopt, 0, sizeof(*sopt));

	sopt->sopt_level = level;
	sopt->sopt_name = name;
	(void)sockopt_alloc(sopt, size, KM_SLEEP);
}

/*
 * destroy sockopt storage
 *	- will release any held memory references
 */
void
sockopt_destroy(struct sockopt *sopt)
{

	if (sopt->sopt_data != sopt->sopt_buf)
		kmem_free(sopt->sopt_data, sopt->sopt_size);

	memset(sopt, 0, sizeof(*sopt));
}

/*
 * set sockopt value
 *	- value is copied into sockopt
 * 	- memory is allocated when necessary, will not sleep
 */
int
sockopt_set(struct sockopt *sopt, const void *buf, size_t len)
{
	int error;

	if (sopt->sopt_size == 0) {
		error = sockopt_alloc(sopt, len, KM_NOSLEEP);
		if (error)
			return error;
	}

	KASSERT(sopt->sopt_size == len);
	memcpy(sopt->sopt_data, buf, len);
	return 0;
}

/*
 * common case of set sockopt integer value
 */
int
sockopt_setint(struct sockopt *sopt, int val)
{

	return sockopt_set(sopt, &val, sizeof(int));
}

/*
 * get sockopt value
 *	- correct size must be given
 */
int
sockopt_get(const struct sockopt *sopt, void *buf, size_t len)
{

	if (sopt->sopt_size != len)
		return EINVAL;

	memcpy(buf, sopt->sopt_data, len);
	return 0;
}

/*
 * common case of get sockopt integer value
 */
int
sockopt_getint(const struct sockopt *sopt, int *valp)
{

	return sockopt_get(sopt, valp, sizeof(int));
}

/*
 * set sockopt value from mbuf
 *	- ONLY for legacy code
 *	- mbuf is released by sockopt
 *	- will not sleep
 */
int
sockopt_setmbuf(struct sockopt *sopt, struct mbuf *m)
{
	size_t len;
	int error;

	len = m_length(m);

	if (sopt->sopt_size == 0) {
		error = sockopt_alloc(sopt, len, KM_NOSLEEP);
		if (error)
			return error;
	}

	KASSERT(sopt->sopt_size == len);
	m_copydata(m, 0, len, sopt->sopt_data);
	m_freem(m);

	return 0;
}

/*
 * get sockopt value into mbuf
 *	- ONLY for legacy code
 *	- mbuf to be released by the caller
 *	- will not sleep
 */
struct mbuf *
sockopt_getmbuf(const struct sockopt *sopt)
{
	struct mbuf *m;

	if (sopt->sopt_size > MCLBYTES)
		return NULL;

	m = m_get(M_DONTWAIT, MT_SOOPTS);
	if (m == NULL)
		return NULL;

	if (sopt->sopt_size > MLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return NULL;
		}
	}

	memcpy(mtod(m, void *), sopt->sopt_data, sopt->sopt_size);
	m->m_len = sopt->sopt_size;

	return m;
}

void
sohasoutofband(struct socket *so)
{

	fownsignal(so->so_pgid, SIGURG, POLL_PRI, POLLPRI|POLLRDBAND, so);
	selnotify(&so->so_rcv.sb_sel, POLLPRI | POLLRDBAND, NOTE_SUBMIT);
}

static void
filt_sordetach(struct knote *kn)
{
	struct socket	*so;

	so = ((file_t *)kn->kn_obj)->f_socket;
	solock(so);
	SLIST_REMOVE(&so->so_rcv.sb_sel.sel_klist, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_rcv.sb_sel.sel_klist))
		so->so_rcv.sb_flags &= ~SB_KNOTE;
	sounlock(so);
}

/*ARGSUSED*/
static int
filt_soread(struct knote *kn, long hint)
{
	struct socket	*so;
	int rv;

	so = ((file_t *)kn->kn_obj)->f_socket;
	if (hint != NOTE_SUBMIT)
		solock(so);
	kn->kn_data = so->so_rcv.sb_cc;
	if (so->so_state & SS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		rv = 1;
	} else if (so->so_error)	/* temporary udp error */
		rv = 1;
	else if (kn->kn_sfflags & NOTE_LOWAT)
		rv = (kn->kn_data >= kn->kn_sdata);
	else 
		rv = (kn->kn_data >= so->so_rcv.sb_lowat);
	if (hint != NOTE_SUBMIT)
		sounlock(so);
	return rv;
}

static void
filt_sowdetach(struct knote *kn)
{
	struct socket	*so;

	so = ((file_t *)kn->kn_obj)->f_socket;
	solock(so);
	SLIST_REMOVE(&so->so_snd.sb_sel.sel_klist, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_snd.sb_sel.sel_klist))
		so->so_snd.sb_flags &= ~SB_KNOTE;
	sounlock(so);
}

/*ARGSUSED*/
static int
filt_sowrite(struct knote *kn, long hint)
{
	struct socket	*so;
	int rv;

	so = ((file_t *)kn->kn_obj)->f_socket;
	if (hint != NOTE_SUBMIT)
		solock(so);
	kn->kn_data = sbspace(&so->so_snd);
	if (so->so_state & SS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		rv = 1;
	} else if (so->so_error)	/* temporary udp error */
		rv = 1;
	else if (((so->so_state & SS_ISCONNECTED) == 0) &&
	    (so->so_proto->pr_flags & PR_CONNREQUIRED))
		rv = 0;
	else if (kn->kn_sfflags & NOTE_LOWAT)
		rv = (kn->kn_data >= kn->kn_sdata);
	else
		rv = (kn->kn_data >= so->so_snd.sb_lowat);
	if (hint != NOTE_SUBMIT)
		sounlock(so);
	return rv;
}

/*ARGSUSED*/
static int
filt_solisten(struct knote *kn, long hint)
{
	struct socket	*so;
	int rv;

	so = ((file_t *)kn->kn_obj)->f_socket;

	/*
	 * Set kn_data to number of incoming connections, not
	 * counting partial (incomplete) connections.
	 */
	if (hint != NOTE_SUBMIT)
		solock(so);
	kn->kn_data = so->so_qlen;
	rv = (kn->kn_data > 0);
	if (hint != NOTE_SUBMIT)
		sounlock(so);
	return rv;
}

static const struct filterops solisten_filtops =
	{ 1, NULL, filt_sordetach, filt_solisten };
static const struct filterops soread_filtops =
	{ 1, NULL, filt_sordetach, filt_soread };
static const struct filterops sowrite_filtops =
	{ 1, NULL, filt_sowdetach, filt_sowrite };

int
soo_kqfilter(struct file *fp, struct knote *kn)
{
	struct socket	*so;
	struct sockbuf	*sb;

	so = ((file_t *)kn->kn_obj)->f_socket;
	solock(so);
	switch (kn->kn_filter) {
	case EVFILT_READ:
		if (so->so_options & SO_ACCEPTCONN)
			kn->kn_fop = &solisten_filtops;
		else
			kn->kn_fop = &soread_filtops;
		sb = &so->so_rcv;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &sowrite_filtops;
		sb = &so->so_snd;
		break;
	default:
		sounlock(so);
		return (EINVAL);
	}
	SLIST_INSERT_HEAD(&sb->sb_sel.sel_klist, kn, kn_selnext);
	sb->sb_flags |= SB_KNOTE;
	sounlock(so);
	return (0);
}

static int
sodopoll(struct socket *so, int events)
{
	int revents;

	revents = 0;

	if (events & (POLLIN | POLLRDNORM))
		if (soreadable(so))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (sowritable(so))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & (POLLPRI | POLLRDBAND))
		if (so->so_oobmark || (so->so_state & SS_RCVATMARK))
			revents |= events & (POLLPRI | POLLRDBAND);

	return revents;
}

int
sopoll(struct socket *so, int events)
{
	int revents = 0;

#ifndef DIAGNOSTIC
	/*
	 * Do a quick, unlocked check in expectation that the socket
	 * will be ready for I/O.  Don't do this check if DIAGNOSTIC,
	 * as the solocked() assertions will fail.
	 */
	if ((revents = sodopoll(so, events)) != 0)
		return revents;
#endif

	solock(so);
	if ((revents = sodopoll(so, events)) == 0) {
		if (events & (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND)) {
			selrecord(curlwp, &so->so_rcv.sb_sel);
			so->so_rcv.sb_flags |= SB_NOTIFY;
		}

		if (events & (POLLOUT | POLLWRNORM)) {
			selrecord(curlwp, &so->so_snd.sb_sel);
			so->so_snd.sb_flags |= SB_NOTIFY;
		}
	}
	sounlock(so);

	return revents;
}


#include <sys/sysctl.h>

static int sysctl_kern_somaxkva(SYSCTLFN_PROTO);
static int sysctl_kern_sbmax(SYSCTLFN_PROTO);

/*
 * sysctl helper routine for kern.somaxkva.  ensures that the given
 * value is not too small.
 * (XXX should we maybe make sure it's not too large as well?)
 */
static int
sysctl_kern_somaxkva(SYSCTLFN_ARGS)
{
	int error, new_somaxkva;
	struct sysctlnode node;

	new_somaxkva = somaxkva;
	node = *rnode;
	node.sysctl_data = &new_somaxkva;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (new_somaxkva < (16 * 1024 * 1024)) /* sanity */
		return (EINVAL);

	mutex_enter(&so_pendfree_lock);
	somaxkva = new_somaxkva;
	cv_broadcast(&socurkva_cv);
	mutex_exit(&so_pendfree_lock);

	return (error);
}

/*
 * sysctl helper routine for kern.sbmax. Basically just ensures that
 * any new value is not too small.
 */
static int
sysctl_kern_sbmax(SYSCTLFN_ARGS)
{
	int error, new_sbmax;
	struct sysctlnode node;

	new_sbmax = sb_max;
	node = *rnode;
	node.sysctl_data = &new_sbmax;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	KERNEL_LOCK(1, NULL);
	error = sb_max_set(new_sbmax);
	KERNEL_UNLOCK_ONE(NULL);

	return (error);
}

static void
sysctl_kern_socket_setup(void)
{

	KASSERT(socket_sysctllog == NULL);

	sysctl_createv(&socket_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "somaxkva",
		       SYSCTL_DESCR("Maximum amount of kernel memory to be "
				    "used for socket buffers"),
		       sysctl_kern_somaxkva, 0, NULL, 0,
		       CTL_KERN, KERN_SOMAXKVA, CTL_EOL);

	sysctl_createv(&socket_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sbmax",
		       SYSCTL_DESCR("Maximum socket buffer size"),
		       sysctl_kern_sbmax, 0, NULL, 0,
		       CTL_KERN, KERN_SBMAX, CTL_EOL);
}
