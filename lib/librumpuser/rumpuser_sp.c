/*      $NetBSD: rumpuser_sp.c,v 1.70 2015/08/16 11:37:39 pooka Exp $	*/

/*
 * Copyright (c) 2010, 2011 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sysproxy routines.  This provides system RPC support over host sockets.
 * The most notable limitation is that the client and server must share
 * the same ABI.  This does not mean that they have to be the same
 * machine or that they need to run the same version of the host OS,
 * just that they must agree on the data structures.  This even *might*
 * work correctly from one hardware architecture to another.
 */

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpuser_sp.c,v 1.70 2015/08/16 11:37:39 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rump.h> /* XXX: for rfork flags */
#include <rump/rumpuser.h>

#include "rumpuser_int.h"

#include "sp_common.c"

#ifndef MAXCLI
#define MAXCLI 256
#endif
#ifndef MAXWORKER
#define MAXWORKER 128
#endif
#ifndef IDLEWORKER
#define IDLEWORKER 16
#endif
int rumpsp_maxworker = MAXWORKER;
int rumpsp_idleworker = IDLEWORKER;

static struct pollfd pfdlist[MAXCLI];
static struct spclient spclist[MAXCLI];
static unsigned int disco;
static volatile int spfini;

static char banner[MAXBANNER];

#define PROTOMAJOR 0
#define PROTOMINOR 4


/* either no atomic ops, or we haven't figured out how to use them */
#if defined(__linux__) || defined(__APPLE__) || defined(__CYGWIN__) || defined(__OpenBSD__) || defined(__GNU__) || defined(__GLIBC__)
static pthread_mutex_t discomtx = PTHREAD_MUTEX_INITIALIZER;

static void
signaldisco(void)
{

	pthread_mutex_lock(&discomtx);
	disco++;
	pthread_mutex_unlock(&discomtx);
}

static unsigned int
getdisco(void)
{
	unsigned int discocnt;

	pthread_mutex_lock(&discomtx);
	discocnt = disco;
	disco = 0;
	pthread_mutex_unlock(&discomtx);

	return discocnt;
}

#elif defined(__FreeBSD__) || defined(__DragonFly__)

#include <machine/atomic.h>
#define signaldisco()	atomic_add_int(&disco, 1)
#define getdisco()	atomic_readandclear_int(&disco)

#else /* NetBSD */

#include <sys/atomic.h>
#define signaldisco() atomic_inc_uint(&disco)
#define getdisco() atomic_swap_uint(&disco, 0)

#endif


struct prefork {
	uint32_t pf_auth[AUTHLEN];
	struct lwp *pf_lwp;

	LIST_ENTRY(prefork) pf_entries;		/* global list */
	LIST_ENTRY(prefork) pf_spcentries;	/* linked from forking spc */
};
static LIST_HEAD(, prefork) preforks = LIST_HEAD_INITIALIZER(preforks);
static pthread_mutex_t pfmtx;

/*
 * This version is for the server.  It's optimized for multiple threads
 * and is *NOT* reentrant wrt to signals.
 */
static int
waitresp(struct spclient *spc, struct respwait *rw)
{
	int spcstate;
	int rv = 0;

	pthread_mutex_lock(&spc->spc_mtx);
	sendunlockl(spc);
	while (!rw->rw_done && spc->spc_state != SPCSTATE_DYING) {
		pthread_cond_wait(&rw->rw_cv, &spc->spc_mtx);
	}
	TAILQ_REMOVE(&spc->spc_respwait, rw, rw_entries);
	spcstate = spc->spc_state;
	pthread_mutex_unlock(&spc->spc_mtx);

	pthread_cond_destroy(&rw->rw_cv);

	if (rv)
		return rv;
	if (spcstate == SPCSTATE_DYING)
		return ENOTCONN;
	return rw->rw_error;
}

/*
 * Manual wrappers, since librump does not have access to the
 * user namespace wrapped interfaces.
 */

static void
lwproc_switch(struct lwp *l)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_switch(l);
	rumpuser__hyp.hyp_unschedule();
}

static void
lwproc_release(void)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwproc_release();
	rumpuser__hyp.hyp_unschedule();
}

static int
lwproc_rfork(struct spclient *spc, int flags, const char *comm)
{
	int rv;

	rumpuser__hyp.hyp_schedule();
	rv = rumpuser__hyp.hyp_lwproc_rfork(spc, flags, comm);
	rumpuser__hyp.hyp_unschedule();

	return rv;
}

static int
lwproc_newlwp(pid_t pid)
{
	int rv;

	rumpuser__hyp.hyp_schedule();
	rv = rumpuser__hyp.hyp_lwproc_newlwp(pid);
	rumpuser__hyp.hyp_unschedule();

	return rv;
}

static struct lwp *
lwproc_curlwp(void)
{
	struct lwp *l;

	rumpuser__hyp.hyp_schedule();
	l = rumpuser__hyp.hyp_lwproc_curlwp();
	rumpuser__hyp.hyp_unschedule();

	return l;
}

static pid_t
lwproc_getpid(void)
{
	pid_t p;

	rumpuser__hyp.hyp_schedule();
	p = rumpuser__hyp.hyp_getpid();
	rumpuser__hyp.hyp_unschedule();

	return p;
}

static void
lwproc_execnotify(const char *comm)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_execnotify(comm);
	rumpuser__hyp.hyp_unschedule();
}

static void
lwproc_lwpexit(void)
{

	rumpuser__hyp.hyp_schedule();
	rumpuser__hyp.hyp_lwpexit();
	rumpuser__hyp.hyp_unschedule();
}

static int
rumpsyscall(int sysnum, void *data, register_t *regrv)
{
	long retval[2] = {0, 0};
	int rv;

	rumpuser__hyp.hyp_schedule();
	rv = rumpuser__hyp.hyp_syscall(sysnum, data, retval);
	rumpuser__hyp.hyp_unschedule();

	regrv[0] = retval[0];
	regrv[1] = retval[1];
	return rv;
}

static uint64_t
nextreq(struct spclient *spc)
{
	uint64_t nw;

	pthread_mutex_lock(&spc->spc_mtx);
	nw = spc->spc_nextreq++;
	pthread_mutex_unlock(&spc->spc_mtx);

	return nw;
}

/*
 * XXX: we send responses with "blocking" I/O.  This is not
 * ok for the main thread.  XXXFIXME
 */

static void
send_error_resp(struct spclient *spc, uint64_t reqno, enum rumpsp_err error)
{
	struct rsp_hdr rhdr;
	struct iovec iov[1];

	rhdr.rsp_len = sizeof(rhdr);
	rhdr.rsp_reqno = reqno;
	rhdr.rsp_class = RUMPSP_ERROR;
	rhdr.rsp_type = 0;
	rhdr.rsp_error = error;

	IOVPUT(iov[0], rhdr);

	sendlock(spc);
	(void)SENDIOV(spc, iov);
	sendunlock(spc);
}

static int
send_handshake_resp(struct spclient *spc, uint64_t reqno, int error)
{
	struct rsp_hdr rhdr;
	struct iovec iov[2];
	int rv;

	rhdr.rsp_len = sizeof(rhdr) + sizeof(error);
	rhdr.rsp_reqno = reqno;
	rhdr.rsp_class = RUMPSP_RESP;
	rhdr.rsp_type = RUMPSP_HANDSHAKE;
	rhdr.rsp_error = 0;

	IOVPUT(iov[0], rhdr);
	IOVPUT(iov[1], error);

	sendlock(spc);
	rv = SENDIOV(spc, iov);
	sendunlock(spc);

	return rv;
}

static int
send_syscall_resp(struct spclient *spc, uint64_t reqno, int error,
	register_t *retval)
{
	struct rsp_hdr rhdr;
	struct rsp_sysresp sysresp;
	struct iovec iov[2];
	int rv;

	rhdr.rsp_len = sizeof(rhdr) + sizeof(sysresp);
	rhdr.rsp_reqno = reqno;
	rhdr.rsp_class = RUMPSP_RESP;
	rhdr.rsp_type = RUMPSP_SYSCALL;
	rhdr.rsp_sysnum = 0;

	sysresp.rsys_error = error;
	memcpy(sysresp.rsys_retval, retval, sizeof(sysresp.rsys_retval));

	IOVPUT(iov[0], rhdr);
	IOVPUT(iov[1], sysresp);

	sendlock(spc);
	rv = SENDIOV(spc, iov);
	sendunlock(spc);

	return rv;
}

static int
send_prefork_resp(struct spclient *spc, uint64_t reqno, uint32_t *auth)
{
	struct rsp_hdr rhdr;
	struct iovec iov[2];
	int rv;

	rhdr.rsp_len = sizeof(rhdr) + AUTHLEN*sizeof(*auth);
	rhdr.rsp_reqno = reqno;
	rhdr.rsp_class = RUMPSP_RESP;
	rhdr.rsp_type = RUMPSP_PREFORK;
	rhdr.rsp_sysnum = 0;

	IOVPUT(iov[0], rhdr);
	IOVPUT_WITHSIZE(iov[1], auth, AUTHLEN*sizeof(*auth));

	sendlock(spc);
	rv = SENDIOV(spc, iov);
	sendunlock(spc);

	return rv;
}

static int
copyin_req(struct spclient *spc, const void *remaddr, size_t *dlen,
	int wantstr, void **resp)
{
	struct rsp_hdr rhdr;
	struct rsp_copydata copydata;
	struct respwait rw;
	struct iovec iov[2];
	int rv;

	DPRINTF(("copyin_req: %zu bytes from %p\n", *dlen, remaddr));

	rhdr.rsp_len = sizeof(rhdr) + sizeof(copydata);
	rhdr.rsp_class = RUMPSP_REQ;
	if (wantstr)
		rhdr.rsp_type = RUMPSP_COPYINSTR;
	else
		rhdr.rsp_type = RUMPSP_COPYIN;
	rhdr.rsp_sysnum = 0;

	copydata.rcp_addr = __UNCONST(remaddr);
	copydata.rcp_len = *dlen;

	IOVPUT(iov[0], rhdr);
	IOVPUT(iov[1], copydata);

	putwait(spc, &rw, &rhdr);
	rv = SENDIOV(spc, iov);
	if (rv) {
		unputwait(spc, &rw);
		return rv;
	}

	rv = waitresp(spc, &rw);

	DPRINTF(("copyin: response %d\n", rv));

	*resp = rw.rw_data;
	if (wantstr)
		*dlen = rw.rw_dlen;

	return rv;

}

static int
send_copyout_req(struct spclient *spc, const void *remaddr,
	const void *data, size_t dlen)
{
	struct rsp_hdr rhdr;
	struct rsp_copydata copydata;
	struct iovec iov[3];
	int rv;

	DPRINTF(("copyout_req (async): %zu bytes to %p\n", dlen, remaddr));

	rhdr.rsp_len = sizeof(rhdr) + sizeof(copydata) + dlen;
	rhdr.rsp_reqno = nextreq(spc);
	rhdr.rsp_class = RUMPSP_REQ;
	rhdr.rsp_type = RUMPSP_COPYOUT;
	rhdr.rsp_sysnum = 0;

	copydata.rcp_addr = __UNCONST(remaddr);
	copydata.rcp_len = dlen;

	IOVPUT(iov[0], rhdr);
	IOVPUT(iov[1], copydata);
	IOVPUT_WITHSIZE(iov[2], __UNCONST(data), dlen);

	sendlock(spc);
	rv = SENDIOV(spc, iov);
	sendunlock(spc);

	return rv;
}

static int
anonmmap_req(struct spclient *spc, size_t howmuch, void **resp)
{
	struct rsp_hdr rhdr;
	struct respwait rw;
	struct iovec iov[2];
	int rv;

	DPRINTF(("anonmmap_req: %zu bytes\n", howmuch));

	rhdr.rsp_len = sizeof(rhdr) + sizeof(howmuch);
	rhdr.rsp_class = RUMPSP_REQ;
	rhdr.rsp_type = RUMPSP_ANONMMAP;
	rhdr.rsp_sysnum = 0;

	IOVPUT(iov[0], rhdr);
	IOVPUT(iov[1], howmuch);

	putwait(spc, &rw, &rhdr);
	rv = SENDIOV(spc, iov);
	if (rv) {
		unputwait(spc, &rw);
		return rv;
	}

	rv = waitresp(spc, &rw);

	*resp = rw.rw_data;

	DPRINTF(("anonmmap: mapped at %p\n", **(void ***)resp));

	return rv;
}

static int
send_raise_req(struct spclient *spc, int signo)
{
	struct rsp_hdr rhdr;
	struct iovec iov[1];
	int rv;

	rhdr.rsp_len = sizeof(rhdr);
	rhdr.rsp_class = RUMPSP_REQ;
	rhdr.rsp_type = RUMPSP_RAISE;
	rhdr.rsp_signo = signo;

	IOVPUT(iov[0], rhdr);

	sendlock(spc);
	rv = SENDIOV(spc, iov);
	sendunlock(spc);

	return rv;
}

static void
spcref(struct spclient *spc)
{

	pthread_mutex_lock(&spc->spc_mtx);
	spc->spc_refcnt++;
	pthread_mutex_unlock(&spc->spc_mtx);
}

static void
spcrelease(struct spclient *spc)
{
	int ref;

	pthread_mutex_lock(&spc->spc_mtx);
	ref = --spc->spc_refcnt;
	if (__predict_false(spc->spc_inexec && ref <= 2))
		pthread_cond_broadcast(&spc->spc_cv);
	pthread_mutex_unlock(&spc->spc_mtx);

	if (ref > 0)
		return;

	DPRINTF(("rump_sp: spcrelease: spc %p fd %d\n", spc, spc->spc_fd));

	_DIAGASSERT(TAILQ_EMPTY(&spc->spc_respwait));
	_DIAGASSERT(spc->spc_buf == NULL);

	if (spc->spc_mainlwp) {
		lwproc_switch(spc->spc_mainlwp);
		lwproc_release();
	}
	spc->spc_mainlwp = NULL;

	close(spc->spc_fd);
	spc->spc_fd = -1;
	spc->spc_state = SPCSTATE_NEW;

	signaldisco();
}

static void
serv_handledisco(unsigned int idx)
{
	struct spclient *spc = &spclist[idx];
	int dolwpexit;

	DPRINTF(("rump_sp: disconnecting [%u]\n", idx));

	pfdlist[idx].fd = -1;
	pfdlist[idx].revents = 0;
	pthread_mutex_lock(&spc->spc_mtx);
	spc->spc_state = SPCSTATE_DYING;
	kickall(spc);
	sendunlockl(spc);
	/* exec uses mainlwp in another thread, but also nuked all lwps */
	dolwpexit = !spc->spc_inexec;
	pthread_mutex_unlock(&spc->spc_mtx);

	if (dolwpexit && spc->spc_mainlwp) {
		lwproc_switch(spc->spc_mainlwp);
		lwproc_lwpexit();
		lwproc_switch(NULL);
	}

	/*
	 * Nobody's going to attempt to send/receive anymore,
	 * so reinit info relevant to that.
	 */
	/*LINTED:pointer casts may be ok*/
	memset((char *)spc + SPC_ZEROFF, 0, sizeof(*spc) - SPC_ZEROFF);

	spcrelease(spc);
}

static void
serv_shutdown(void)
{
	struct spclient *spc;
	unsigned int i;

	for (i = 1; i < MAXCLI; i++) {
		spc = &spclist[i];
		if (spc->spc_fd == -1)
			continue;

		shutdown(spc->spc_fd, SHUT_RDWR);
		serv_handledisco(i);

		spcrelease(spc);
	}
}

static unsigned
serv_handleconn(int fd, connecthook_fn connhook, int busy)
{
	struct sockaddr_storage ss;
	socklen_t sl = sizeof(ss);
	int newfd, flags;
	unsigned i;

	/*LINTED: cast ok */
	newfd = accept(fd, (struct sockaddr *)&ss, &sl);
	if (newfd == -1)
		return 0;

	if (busy) {
		close(newfd); /* EBUSY */
		return 0;
	}

	flags = fcntl(newfd, F_GETFL, 0);
	if (fcntl(newfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		close(newfd);
		return 0;
	}

	if (connhook(newfd) != 0) {
		close(newfd);
		return 0;
	}

	/* write out a banner for the client */
	if (send(newfd, banner, strlen(banner), MSG_NOSIGNAL)
	    != (ssize_t)strlen(banner)) {
		close(newfd);
		return 0;
	}

	/* find empty slot the simple way */
	for (i = 0; i < MAXCLI; i++) {
		if (pfdlist[i].fd == -1 && spclist[i].spc_state == SPCSTATE_NEW)
			break;
	}

	/*
	 * Although not finding a slot is impossible (cf. how this routine
	 * is called), the compiler can still think that i == MAXCLI
	 * if this code is either compiled with NDEBUG or the platform
	 * does not use __dead for assert().  Therefore, add an explicit
	 * check to avoid an array-bounds error.
	 */
	/* assert(i < MAXCLI); */
	if (i == MAXCLI)
		abort();

	pfdlist[i].fd = newfd;
	spclist[i].spc_fd = newfd;
	spclist[i].spc_istatus = SPCSTATUS_BUSY; /* dedicated receiver */
	spclist[i].spc_refcnt = 1;

	TAILQ_INIT(&spclist[i].spc_respwait);

	DPRINTF(("rump_sp: added new connection fd %d at idx %u\n", newfd, i));

	return i;
}

static void
serv_handlesyscall(struct spclient *spc, struct rsp_hdr *rhdr, uint8_t *data)
{
	register_t retval[2] = {0, 0};
	int rv, sysnum;

	sysnum = (int)rhdr->rsp_sysnum;
	DPRINTF(("rump_sp: handling syscall %d from client %d\n",
	    sysnum, spc->spc_pid));

	if (__predict_false((rv = lwproc_newlwp(spc->spc_pid)) != 0)) {
		retval[0] = -1;
		send_syscall_resp(spc, rhdr->rsp_reqno, rv, retval);
		return;
	}
	spc->spc_syscallreq = rhdr->rsp_reqno;
	rv = rumpsyscall(sysnum, data, retval);
	spc->spc_syscallreq = 0;
	lwproc_release();

	DPRINTF(("rump_sp: got return value %d & %d/%d\n",
	    rv, retval[0], retval[1]));

	send_syscall_resp(spc, rhdr->rsp_reqno, rv, retval);
}

static void
serv_handleexec(struct spclient *spc, struct rsp_hdr *rhdr, char *comm)
{
	size_t commlen = rhdr->rsp_len - HDRSZ;

	pthread_mutex_lock(&spc->spc_mtx);
	/* one for the connection and one for us */
	while (spc->spc_refcnt > 2)
		pthread_cond_wait(&spc->spc_cv, &spc->spc_mtx);
	pthread_mutex_unlock(&spc->spc_mtx);

	/*
	 * ok, all the threads are dead (or one is still alive and
	 * the connection is dead, in which case this doesn't matter
	 * very much).  proceed with exec.
	 */

	/* ensure comm is 0-terminated */
	/* TODO: make sure it contains sensible chars? */
	comm[commlen] = '\0';

	lwproc_switch(spc->spc_mainlwp);
	lwproc_execnotify(comm);
	lwproc_switch(NULL);

	pthread_mutex_lock(&spc->spc_mtx);
	spc->spc_inexec = 0;
	pthread_mutex_unlock(&spc->spc_mtx);
	send_handshake_resp(spc, rhdr->rsp_reqno, 0);
}

enum sbatype { SBA_SYSCALL, SBA_EXEC };

struct servbouncearg {
	struct spclient *sba_spc;
	struct rsp_hdr sba_hdr;
	enum sbatype sba_type;
	uint8_t *sba_data;

	TAILQ_ENTRY(servbouncearg) sba_entries;
};
static pthread_mutex_t sbamtx;
static pthread_cond_t sbacv;
static int nworker, idleworker, nwork;
static TAILQ_HEAD(, servbouncearg) wrklist = TAILQ_HEAD_INITIALIZER(wrklist);

/*ARGSUSED*/
static void *
serv_workbouncer(void *arg)
{
	struct servbouncearg *sba;

	for (;;) {
		pthread_mutex_lock(&sbamtx);
		if (__predict_false(idleworker - nwork >= rumpsp_idleworker)) {
			nworker--;
			pthread_mutex_unlock(&sbamtx);
			break;
		}
		idleworker++;
		while (TAILQ_EMPTY(&wrklist)) {
			_DIAGASSERT(nwork == 0);
			pthread_cond_wait(&sbacv, &sbamtx);
		}
		idleworker--;

		sba = TAILQ_FIRST(&wrklist);
		TAILQ_REMOVE(&wrklist, sba, sba_entries);
		nwork--;
		pthread_mutex_unlock(&sbamtx);

		if (__predict_true(sba->sba_type == SBA_SYSCALL)) {
			serv_handlesyscall(sba->sba_spc,
			    &sba->sba_hdr, sba->sba_data);
		} else {
			_DIAGASSERT(sba->sba_type == SBA_EXEC);
			serv_handleexec(sba->sba_spc, &sba->sba_hdr,
			    (char *)sba->sba_data);
		}
		spcrelease(sba->sba_spc);
		free(sba->sba_data);
		free(sba);
	}

	return NULL;
}

static int
sp_copyin(void *arg, const void *raddr, void *laddr, size_t *len, int wantstr)
{
	struct spclient *spc = arg;
	void *rdata = NULL; /* XXXuninit */
	int rv, nlocks;

	rumpkern_unsched(&nlocks, NULL);

	rv = copyin_req(spc, raddr, len, wantstr, &rdata);
	if (rv)
		goto out;

	memcpy(laddr, rdata, *len);
	free(rdata);

 out:
	rumpkern_sched(nlocks, NULL);
	if (rv)
		rv = EFAULT;
	ET(rv);
}

int
rumpuser_sp_copyin(void *arg, const void *raddr, void *laddr, size_t len)
{
	int rv;

	rv = sp_copyin(arg, raddr, laddr, &len, 0);
	ET(rv);
}

int
rumpuser_sp_copyinstr(void *arg, const void *raddr, void *laddr, size_t *len)
{
	int rv;

	rv = sp_copyin(arg, raddr, laddr, len, 1);
	ET(rv);
}

static int
sp_copyout(void *arg, const void *laddr, void *raddr, size_t dlen)
{
	struct spclient *spc = arg;
	int nlocks, rv;

	rumpkern_unsched(&nlocks, NULL);
	rv = send_copyout_req(spc, raddr, laddr, dlen);
	rumpkern_sched(nlocks, NULL);

	if (rv)
		rv = EFAULT;
	ET(rv);
}

int
rumpuser_sp_copyout(void *arg, const void *laddr, void *raddr, size_t dlen)
{
	int rv;

	rv = sp_copyout(arg, laddr, raddr, dlen);
	ET(rv);
}

int
rumpuser_sp_copyoutstr(void *arg, const void *laddr, void *raddr, size_t *dlen)
{
	int rv;

	rv = sp_copyout(arg, laddr, raddr, *dlen);
	ET(rv);
}

int
rumpuser_sp_anonmmap(void *arg, size_t howmuch, void **addr)
{
	struct spclient *spc = arg;
	void *resp, *rdata = NULL; /* XXXuninit */
	int nlocks, rv;

	rumpkern_unsched(&nlocks, NULL);

	rv = anonmmap_req(spc, howmuch, &rdata);
	if (rv) {
		rv = EFAULT;
		goto out;
	}

	resp = *(void **)rdata;
	free(rdata);

	if (resp == NULL) {
		rv = ENOMEM;
	}

	*addr = resp;

 out:
	rumpkern_sched(nlocks, NULL);
	ET(rv);
}

int
rumpuser_sp_raise(void *arg, int signo)
{
	struct spclient *spc = arg;
	int rv, nlocks;

	rumpkern_unsched(&nlocks, NULL);
	rv = send_raise_req(spc, signo);
	rumpkern_sched(nlocks, NULL);

	return rv;
}

static pthread_attr_t pattr_detached;
static void
schedulework(struct spclient *spc, enum sbatype sba_type)
{
	struct servbouncearg *sba;
	pthread_t pt;
	uint64_t reqno;
	int retries = 0;

	reqno = spc->spc_hdr.rsp_reqno;
	while ((sba = malloc(sizeof(*sba))) == NULL) {
		if (nworker == 0 || retries > 10) {
			send_error_resp(spc, reqno, RUMPSP_ERR_TRYAGAIN);
			spcfreebuf(spc);
			return;
		}
		/* slim chance of more memory? */
		usleep(10000);
	}

	sba->sba_spc = spc;
	sba->sba_type = sba_type;
	sba->sba_hdr = spc->spc_hdr;
	sba->sba_data = spc->spc_buf;
	spcresetbuf(spc);

	spcref(spc);

	pthread_mutex_lock(&sbamtx);
	TAILQ_INSERT_TAIL(&wrklist, sba, sba_entries);
	nwork++;
	if (nwork <= idleworker) {
		/* do we have a daemon's tool (i.e. idle threads)? */
		pthread_cond_signal(&sbacv);
	} else if (nworker < rumpsp_maxworker) {
		/*
		 * Else, need to create one
		 * (if we can, otherwise just expect another
		 * worker to pick up the syscall)
		 */
		if (pthread_create(&pt, &pattr_detached,
		    serv_workbouncer, NULL) == 0) {
			nworker++;
		}
	}
	pthread_mutex_unlock(&sbamtx);
}

/*
 *
 * Startup routines and mainloop for server.
 *
 */

struct spservarg {
	int sps_sock;
	connecthook_fn sps_connhook;
};

static void
handlereq(struct spclient *spc)
{
	uint64_t reqno;
	int error;

	reqno = spc->spc_hdr.rsp_reqno;
	if (__predict_false(spc->spc_state == SPCSTATE_NEW)) {
		if (spc->spc_hdr.rsp_type != RUMPSP_HANDSHAKE) {
			send_error_resp(spc, reqno, RUMPSP_ERR_AUTH);
			shutdown(spc->spc_fd, SHUT_RDWR);
			spcfreebuf(spc);
			return;
		}

		if (spc->spc_hdr.rsp_handshake == HANDSHAKE_GUEST) {
			char *comm = (char *)spc->spc_buf;
			size_t commlen = spc->spc_hdr.rsp_len - HDRSZ;

			/* ensure it's 0-terminated */
			/* XXX make sure it contains sensible chars? */
			comm[commlen] = '\0';

			/* make sure we fork off of proc1 */
			_DIAGASSERT(lwproc_curlwp() == NULL);

			if ((error = lwproc_rfork(spc,
			    RUMP_RFFD_CLEAR, comm)) != 0) {
				shutdown(spc->spc_fd, SHUT_RDWR);
			}

			spcfreebuf(spc);
			if (error)
				return;

			spc->spc_mainlwp = lwproc_curlwp();

			send_handshake_resp(spc, reqno, 0);
		} else if (spc->spc_hdr.rsp_handshake == HANDSHAKE_FORK) {
			struct lwp *tmpmain;
			struct prefork *pf;
			struct handshake_fork *rfp;
			int cancel;

			if (spc->spc_off-HDRSZ != sizeof(*rfp)) {
				send_error_resp(spc, reqno,
				    RUMPSP_ERR_MALFORMED_REQUEST);
				shutdown(spc->spc_fd, SHUT_RDWR);
				spcfreebuf(spc);
				return;
			}

			/*LINTED*/
			rfp = (void *)spc->spc_buf;
			cancel = rfp->rf_cancel;

			pthread_mutex_lock(&pfmtx);
			LIST_FOREACH(pf, &preforks, pf_entries) {
				if (memcmp(rfp->rf_auth, pf->pf_auth,
				    sizeof(rfp->rf_auth)) == 0) {
					LIST_REMOVE(pf, pf_entries);
					LIST_REMOVE(pf, pf_spcentries);
					break;
				}
			}
			pthread_mutex_unlock(&pfmtx);
			spcfreebuf(spc);

			if (!pf) {
				send_error_resp(spc, reqno,
				    RUMPSP_ERR_INVALID_PREFORK);
				shutdown(spc->spc_fd, SHUT_RDWR);
				return;
			}

			tmpmain = pf->pf_lwp;
			free(pf);
			lwproc_switch(tmpmain);
			if (cancel) {
				lwproc_release();
				shutdown(spc->spc_fd, SHUT_RDWR);
				return;
			}

			/*
			 * So, we forked already during "prefork" to save
			 * the file descriptors from a parent exit
			 * race condition.  But now we need to fork
			 * a second time since the initial fork has
			 * the wrong spc pointer.  (yea, optimize
			 * interfaces some day if anyone cares)
			 */
			if ((error = lwproc_rfork(spc,
			    RUMP_RFFD_SHARE, NULL)) != 0) {
				send_error_resp(spc, reqno,
				    RUMPSP_ERR_RFORK_FAILED);
				shutdown(spc->spc_fd, SHUT_RDWR);
				lwproc_release();
				return;
			}
			spc->spc_mainlwp = lwproc_curlwp();
			lwproc_switch(tmpmain);
			lwproc_release();
			lwproc_switch(spc->spc_mainlwp);

			send_handshake_resp(spc, reqno, 0);
		} else {
			send_error_resp(spc, reqno, RUMPSP_ERR_AUTH);
			shutdown(spc->spc_fd, SHUT_RDWR);
			spcfreebuf(spc);
			return;
		}

		spc->spc_pid = lwproc_getpid();

		DPRINTF(("rump_sp: handshake for client %p complete, pid %d\n",
		    spc, spc->spc_pid));
		    
		lwproc_switch(NULL);
		spc->spc_state = SPCSTATE_RUNNING;
		return;
	}

	if (__predict_false(spc->spc_hdr.rsp_type == RUMPSP_PREFORK)) {
		struct prefork *pf;
		uint32_t auth[AUTHLEN];
		size_t randlen;
		int inexec;

		DPRINTF(("rump_sp: prefork handler executing for %p\n", spc));
		spcfreebuf(spc);

		pthread_mutex_lock(&spc->spc_mtx);
		inexec = spc->spc_inexec;
		pthread_mutex_unlock(&spc->spc_mtx);
		if (inexec) {
			send_error_resp(spc, reqno, RUMPSP_ERR_INEXEC);
			shutdown(spc->spc_fd, SHUT_RDWR);
			return;
		}

		pf = malloc(sizeof(*pf));
		if (pf == NULL) {
			send_error_resp(spc, reqno, RUMPSP_ERR_NOMEM);
			return;
		}

		/*
		 * Use client main lwp to fork.  this is never used by
		 * worker threads (except in exec, but we checked for that
		 * above) so we can safely use it here.
		 */
		lwproc_switch(spc->spc_mainlwp);
		if ((error = lwproc_rfork(spc, RUMP_RFFD_COPY, NULL)) != 0) {
			DPRINTF(("rump_sp: fork failed: %d (%p)\n",error, spc));
			send_error_resp(spc, reqno, RUMPSP_ERR_RFORK_FAILED);
			lwproc_switch(NULL);
			free(pf);
			return;
		}

		/* Ok, we have a new process context and a new curlwp */
		rumpuser_getrandom(auth, sizeof(auth), 0, &randlen);
		memcpy(pf->pf_auth, auth, sizeof(pf->pf_auth));
		pf->pf_lwp = lwproc_curlwp();
		lwproc_switch(NULL);

		pthread_mutex_lock(&pfmtx);
		LIST_INSERT_HEAD(&preforks, pf, pf_entries);
		LIST_INSERT_HEAD(&spc->spc_pflist, pf, pf_spcentries);
		pthread_mutex_unlock(&pfmtx);

		DPRINTF(("rump_sp: prefork handler success %p\n", spc));

		send_prefork_resp(spc, reqno, auth);
		return;
	}

	if (__predict_false(spc->spc_hdr.rsp_type == RUMPSP_HANDSHAKE)) {
		int inexec;

		if (spc->spc_hdr.rsp_handshake != HANDSHAKE_EXEC) {
			send_error_resp(spc, reqno,
			    RUMPSP_ERR_MALFORMED_REQUEST);
			shutdown(spc->spc_fd, SHUT_RDWR);
			spcfreebuf(spc);
			return;
		}

		pthread_mutex_lock(&spc->spc_mtx);
		inexec = spc->spc_inexec;
		pthread_mutex_unlock(&spc->spc_mtx);
		if (inexec) {
			send_error_resp(spc, reqno, RUMPSP_ERR_INEXEC);
			shutdown(spc->spc_fd, SHUT_RDWR);
			spcfreebuf(spc);
			return;
		}

		pthread_mutex_lock(&spc->spc_mtx);
		spc->spc_inexec = 1;
		pthread_mutex_unlock(&spc->spc_mtx);

		/*
		 * start to drain lwps.  we will wait for it to finish
		 * in another thread
		 */
		lwproc_switch(spc->spc_mainlwp);
		lwproc_lwpexit();
		lwproc_switch(NULL);

		/*
		 * exec has to wait for lwps to drain, so finish it off
		 * in another thread
		 */
		schedulework(spc, SBA_EXEC);
		return;
	}

	if (__predict_false(spc->spc_hdr.rsp_type != RUMPSP_SYSCALL)) {
		send_error_resp(spc, reqno, RUMPSP_ERR_MALFORMED_REQUEST);
		spcfreebuf(spc);
		return;
	}

	schedulework(spc, SBA_SYSCALL);
}

static void *
spserver(void *arg)
{
	struct spservarg *sarg = arg;
	struct spclient *spc;
	unsigned idx;
	int seen;
	int rv;
	unsigned int nfds, maxidx;

	for (idx = 0; idx < MAXCLI; idx++) {
		pfdlist[idx].fd = -1;
		pfdlist[idx].events = POLLIN;

		spc = &spclist[idx];
		pthread_mutex_init(&spc->spc_mtx, NULL);
		pthread_cond_init(&spc->spc_cv, NULL);
		spc->spc_fd = -1;
	}
	pfdlist[0].fd = spclist[0].spc_fd = sarg->sps_sock;
	pfdlist[0].events = POLLIN;
	nfds = 1;
	maxidx = 0;

	pthread_attr_init(&pattr_detached);
	pthread_attr_setdetachstate(&pattr_detached, PTHREAD_CREATE_DETACHED);
#if NOTYET
	pthread_attr_setstacksize(&pattr_detached, 32*1024);
#endif

	pthread_mutex_init(&sbamtx, NULL);
	pthread_cond_init(&sbacv, NULL);

	DPRINTF(("rump_sp: server mainloop\n"));

	for (;;) {
		int discoed;

		/* g/c hangarounds (eventually) */
		discoed = getdisco();
		while (discoed--) {
			nfds--;
			idx = maxidx;
			while (idx) {
				if (pfdlist[idx].fd != -1) {
					maxidx = idx;
					break;
				}
				idx--;
			}
			DPRINTF(("rump_sp: set maxidx to [%u]\n",
			    maxidx));
		}

		DPRINTF(("rump_sp: loop nfd %d\n", maxidx+1));
		seen = 0;
		rv = poll(pfdlist, maxidx+1, INFTIM);
		assert(maxidx+1 <= MAXCLI);
		assert(rv != 0);
		if (rv == -1) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "rump_spserver: poll returned %d\n",
			    errno);
			break;
		}

		for (idx = 0; seen < rv && idx < MAXCLI; idx++) {
			if ((pfdlist[idx].revents & POLLIN) == 0)
				continue;

			seen++;
			DPRINTF(("rump_sp: activity at [%u] %d/%d\n",
			    idx, seen, rv));
			if (idx > 0) {
				spc = &spclist[idx];
				DPRINTF(("rump_sp: mainloop read [%u]\n", idx));
				switch (readframe(spc)) {
				case 0:
					break;
				case -1:
					serv_handledisco(idx);
					break;
				default:
					switch (spc->spc_hdr.rsp_class) {
					case RUMPSP_RESP:
						kickwaiter(spc);
						break;
					case RUMPSP_REQ:
						handlereq(spc);
						break;
					default:
						send_error_resp(spc,
						  spc->spc_hdr.rsp_reqno,
						  RUMPSP_ERR_MALFORMED_REQUEST);
						spcfreebuf(spc);
						break;
					}
					break;
				}

			} else {
				DPRINTF(("rump_sp: mainloop new connection\n"));

				if (__predict_false(spfini)) {
					close(spclist[0].spc_fd);
					serv_shutdown();
					goto out;
				}

				idx = serv_handleconn(pfdlist[0].fd,
				    sarg->sps_connhook, nfds == MAXCLI);
				if (idx)
					nfds++;
				if (idx > maxidx)
					maxidx = idx;
				DPRINTF(("rump_sp: maxid now %d\n", maxidx));
			}
		}
	}

 out:
	return NULL;
}

static unsigned cleanupidx;
static struct sockaddr *cleanupsa;
int
rumpuser_sp_init(const char *url,
	const char *ostype, const char *osrelease, const char *machine)
{
	pthread_t pt;
	struct spservarg *sarg;
	struct sockaddr *sap;
	char *p;
	unsigned idx = 0; /* XXXgcc */
	int error, s;

	p = strdup(url);
	if (p == NULL) {
		error = ENOMEM;
		goto out;
	}
	error = parseurl(p, &sap, &idx, 1);
	free(p);
	if (error)
		goto out;

	snprintf(banner, sizeof(banner), "RUMPSP-%d.%d-%s-%s/%s\n",
	    PROTOMAJOR, PROTOMINOR, ostype, osrelease, machine);

	s = socket(parsetab[idx].domain, SOCK_STREAM, 0);
	if (s == -1) {
		error = errno;
		goto out;
	}

	sarg = malloc(sizeof(*sarg));
	if (sarg == NULL) {
		close(s);
		error = ENOMEM;
		goto out;
	}

	sarg->sps_sock = s;
	sarg->sps_connhook = parsetab[idx].connhook;

	cleanupidx = idx;
	cleanupsa = sap;

	/* sloppy error recovery */

	/*LINTED*/
	if (bind(s, sap, parsetab[idx].slen) == -1) {
		error = errno;
		fprintf(stderr, "rump_sp: failed to bind to URL %s\n", url);
		goto out;
	}
	if (listen(s, MAXCLI) == -1) {
		error = errno;
		fprintf(stderr, "rump_sp: server listen failed\n");
		goto out;
	}

	if ((error = pthread_create(&pt, NULL, spserver, sarg)) != 0) {
		fprintf(stderr, "rump_sp: cannot create wrkr thread\n");
		goto out;
	}
	pthread_detach(pt);

 out:
	ET(error);
}

void
rumpuser_sp_fini(void *arg)
{
	struct spclient *spc = arg;
	register_t retval[2] = {0, 0};

	if (spclist[0].spc_fd) {
		parsetab[cleanupidx].cleanup(cleanupsa);
	}

	/*
	 * stuff response into the socket, since the rump kernel container
	 * is just about to exit
	 */
	if (spc && spc->spc_syscallreq)
		send_syscall_resp(spc, spc->spc_syscallreq, 0, retval);

	if (spclist[0].spc_fd) {
		shutdown(spclist[0].spc_fd, SHUT_RDWR);
		spfini = 1;
	}

	/*
	 * could release thread, but don't bother, since the container
	 * will be stone dead in a moment.
	 */
}
