/*      $NetBSD: rumpclient.c,v 1.65 2015/01/17 19:34:50 justin Exp $	*/

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
 * Client side routines for rump syscall proxy.
 */

#include <rump/rumpuser_port.h>

/*
 * We use kqueue on the BSDs, poll elsewhere.  We
 * want to use kqueue because it will give us the ability to get signal
 * notifications but defer their handling to a stage where we do not
 * hold the communication lock.  Taking a signal while holding on to
 * that lock may cause a deadlock.  Therefore, block signals throughout
 * the RPC when using poll.  On Linux, we use signalfd in the same role
 * as kqueue on NetBSD to be able to take signals while waiting for a
 * response from the server.
 */

#if defined(__NetBSD__) || defined(__FreeBSD__) || \
    defined(__DragonFly__) || defined(__OpenBSD__)
#define USE_KQUEUE
#endif
#if defined(__linux__)
#define USE_SIGNALFD
#endif

__RCSID("$NetBSD: rumpclient.c,v 1.65 2015/01/17 19:34:50 justin Exp $");

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>

#ifdef USE_KQUEUE
#include <sys/event.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#if defined(__minix)
#define _MTHREADIFY_PTHREADS
#include <minix/mthread.h>
#else
#include <pthread.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rumpclient.h>

#define HOSTOPS
int	(*host_socket)(int, int, int);
int	(*host_close)(int);
int	(*host_connect)(int, const struct sockaddr *, socklen_t);
int	(*host_fcntl)(int, int, ...);
int	(*host_poll)(struct pollfd *, nfds_t, int);
ssize_t	(*host_read)(int, void *, size_t);
ssize_t (*host_sendmsg)(int, const struct msghdr *, int);
int	(*host_setsockopt)(int, int, int, const void *, socklen_t);
int	(*host_dup)(int);

#ifdef USE_KQUEUE
int	(*host_kqueue)(void);
#ifdef __NetBSD__
int	(*host_kevent)(int, const struct kevent *, size_t,
		       struct kevent *, size_t, const struct timespec *);
#else
int	(*host_kevent)(int, const struct kevent *, int,
		       struct kevent *, int, const struct timespec *);
#endif
#endif

#ifdef USE_SIGNALFD
#include <sys/signalfd.h>

int	(*host_signalfd)(int, const sigset_t *, int);
#endif

int	(*host_execve)(const char *, char *const[], char *const[]);

#include "sp_common.c"
#include "rumpuser_sigtrans.c"

static struct spclient clispc = {
	.spc_fd = -1,
};

static int holyfd = -1;
static sigset_t fullset;

static int doconnect(void);
static int handshake_req(struct spclient *, int, void *, int, bool);

/*
 * Default: don't retry.  Most clients can't handle it
 * (consider e.g. fds suddenly going missing).
 */
static time_t retrytimo = 0;

/* always defined to nothingness for now */
#define ERRLOG(a)

static int
send_with_recon(struct spclient *spc, struct iovec *iov, size_t iovlen)
{
	struct timeval starttime, curtime;
	time_t prevreconmsg;
	unsigned reconretries;
	int rv;

	for (prevreconmsg = 0, reconretries = 0;;) {
		rv = dosend(spc, iov, iovlen);
		if (__predict_false(rv == ENOTCONN || rv == EBADF)) {
			/* no persistent connections */
			if (retrytimo == 0) {
				rv = ENOTCONN;
				break;
			}
			if (retrytimo == RUMPCLIENT_RETRYCONN_DIE)
				_exit(1);

			if (!prevreconmsg) {
				prevreconmsg = time(NULL);
				gettimeofday(&starttime, NULL);
			}
			if (reconretries == 1) {
				if (retrytimo == RUMPCLIENT_RETRYCONN_ONCE) {
					rv = ENOTCONN;
					break;
				}
				fprintf(stderr, "rump_sp: connection to "
				    "kernel lost, trying to reconnect ...\n");
			} else if (time(NULL) - prevreconmsg > 120) {
				fprintf(stderr, "rump_sp: still trying to "
				    "reconnect ...\n");
				prevreconmsg = time(NULL);
			}

			/* check that we aren't over the limit */
			if (retrytimo > 0) {
				time_t tdiff;

				gettimeofday(&curtime, NULL);
				tdiff = curtime.tv_sec - starttime.tv_sec;
				if (starttime.tv_usec > curtime.tv_usec)
					tdiff--;
				if (tdiff >= retrytimo) {
					fprintf(stderr, "rump_sp: reconnect "
					    "failed, %lld second timeout\n",
					    (long long)retrytimo);
					return ENOTCONN;
				}
			}

			/* adhoc backoff timer */
			if (reconretries < 10) {
				usleep(100000 * reconretries);
			} else {
				sleep(MIN(10, reconretries-9));
			}
			reconretries++;

			if ((rv = doconnect()) != 0)
				continue;
			if ((rv = handshake_req(&clispc, HANDSHAKE_GUEST,
			    NULL, 0, true)) != 0)
				continue;

			/*
			 * ok, reconnect succesful.  we need to return to
			 * the upper layer to get the entire PDU resent.
			 */
			if (reconretries != 1)
				fprintf(stderr, "rump_sp: reconnected!\n");
			rv = EAGAIN;
			break;
		} else {
			_DIAGASSERT(errno != EAGAIN);
			break;
		}
	}

	return rv;
}

static int
cliwaitresp(struct spclient *spc, struct respwait *rw, sigset_t *mask,
	bool keeplock)
{
	uint64_t mygen;
	bool imalive = true;

	pthread_mutex_lock(&spc->spc_mtx);
	if (!keeplock)
		sendunlockl(spc);
	mygen = spc->spc_generation;

	rw->rw_error = 0;
	while (!rw->rw_done && rw->rw_error == 0) {
		if (__predict_false(spc->spc_generation != mygen || !imalive))
			break;

		/* are we free to receive? */
		if (spc->spc_istatus == SPCSTATUS_FREE) {
			int gotresp, dosig, rv;

			spc->spc_istatus = SPCSTATUS_BUSY;
			pthread_mutex_unlock(&spc->spc_mtx);

			dosig = 0;
			for (gotresp = 0; !gotresp; ) {
#ifdef USE_KQUEUE
				struct kevent kev[8];
				int i;

				/*
				 * typically we don't have a frame waiting
				 * when we come in here, so call kevent now
				 */
				rv = host_kevent(holyfd, NULL, 0,
				    kev, __arraycount(kev), NULL);

				if (__predict_false(rv == -1)) {
					goto activity;
				}

				/*
				 * XXX: don't know how this can happen
				 * (timeout cannot expire since there
				 * isn't one), but it does happen.
				 * treat it as an expectional condition
				 * and go through tryread to determine
				 * alive status.
				 */
				if (__predict_false(rv == 0))
					goto activity;

				for (i = 0; i < rv; i++) {
					if (kev[i].filter == EVFILT_SIGNAL)
						dosig++;
				}
				if (dosig)
					goto cleanup;

				/*
				 * ok, activity.  try to read a frame to
				 * determine what happens next.
				 */
 activity:
#else /* !USE_KQUEUE */
				struct pollfd pfd[2];

				pfd[0].fd = clispc.spc_fd;
				pfd[0].events = POLLIN;
				pfd[1].fd = holyfd;
				pfd[1].events = POLLIN;

				rv = host_poll(pfd, 2, -1);
				if (rv >= 1 && pfd[1].revents & POLLIN) {
					dosig = 1;
					goto cleanup;
				}
#endif /* !USE_KQUEUE */

				switch (readframe(spc)) {
				case 0:
					continue;
				case -1:
					imalive = false;
					goto cleanup;
				default:
					/* case 1 */
					break;
				}

				switch (spc->spc_hdr.rsp_class) {
				case RUMPSP_RESP:
				case RUMPSP_ERROR:
					kickwaiter(spc);
					gotresp = spc->spc_hdr.rsp_reqno ==
					    rw->rw_reqno;
					break;
				case RUMPSP_REQ:
					handlereq(spc);
					break;
				default:
					/* panic */
					break;
				}
			}

 cleanup:
			pthread_mutex_lock(&spc->spc_mtx);
			if (spc->spc_istatus == SPCSTATUS_WANTED)
				kickall(spc);
			spc->spc_istatus = SPCSTATUS_FREE;

			/* take one for the team */
			if (dosig) {
				pthread_mutex_unlock(&spc->spc_mtx);
				pthread_sigmask(SIG_SETMASK, mask, NULL);
				pthread_sigmask(SIG_SETMASK, &fullset, NULL);
				pthread_mutex_lock(&spc->spc_mtx);
			}
		} else {
			spc->spc_istatus = SPCSTATUS_WANTED;
			pthread_cond_wait(&rw->rw_cv, &spc->spc_mtx);
		}
	}
	TAILQ_REMOVE(&spc->spc_respwait, rw, rw_entries);
	pthread_mutex_unlock(&spc->spc_mtx);
	pthread_cond_destroy(&rw->rw_cv);

	if (spc->spc_generation != mygen || !imalive) {
		return ENOTCONN;
	}
	return rw->rw_error;
}

static int
syscall_req(struct spclient *spc, sigset_t *omask, int sysnum,
	const void *data, size_t dlen, void **resp)
{
	struct rsp_hdr rhdr;
	struct respwait rw;
	struct iovec iov[2];
	int rv;

	rhdr.rsp_len = sizeof(rhdr) + dlen;
	rhdr.rsp_class = RUMPSP_REQ;
	rhdr.rsp_type = RUMPSP_SYSCALL;
	rhdr.rsp_sysnum = sysnum;

	IOVPUT(iov[0], rhdr);
	IOVPUT_WITHSIZE(iov[1], __UNCONST(data), dlen);

	do {
		putwait(spc, &rw, &rhdr);
		if ((rv = send_with_recon(spc, iov, __arraycount(iov))) != 0) {
			unputwait(spc, &rw);
			continue;
		}

		rv = cliwaitresp(spc, &rw, omask, false);
		if (rv == ENOTCONN)
			rv = EAGAIN;
	} while (rv == EAGAIN);

	*resp = rw.rw_data;
	return rv;
}

static int
handshake_req(struct spclient *spc, int type, void *data,
	int cancel, bool haslock)
{
	struct handshake_fork rf;
	const char *myprogname = NULL; /* XXXgcc */
	struct rsp_hdr rhdr;
	struct respwait rw;
	sigset_t omask;
	size_t bonus;
	struct iovec iov[2];
	int rv;

	if (type == HANDSHAKE_FORK) {
		bonus = sizeof(rf);
	} else {
#ifdef __NetBSD__
		/* would procfs work on NetBSD too? */
		myprogname = getprogname();
#else
		int fd = open("/proc/self/comm", O_RDONLY);
		if (fd == -1) {
			myprogname = "???";
		} else {
			static char commname[128];

			memset(commname, 0, sizeof(commname));
			if (read(fd, commname, sizeof(commname)) > 0) {
				char *n;

				n = strrchr(commname, '\n');
				if (n)
					*n = '\0';
				myprogname = commname;
			} else {
				myprogname = "???";
			}
			close(fd);
		}
#endif
		bonus = strlen(myprogname)+1;
	}

	/* performs server handshake */
	rhdr.rsp_len = sizeof(rhdr) + bonus;
	rhdr.rsp_class = RUMPSP_REQ;
	rhdr.rsp_type = RUMPSP_HANDSHAKE;
	rhdr.rsp_handshake = type;

	IOVPUT(iov[0], rhdr);

	pthread_sigmask(SIG_SETMASK, &fullset, &omask);
	if (haslock)
		putwait_locked(spc, &rw, &rhdr);
	else
		putwait(spc, &rw, &rhdr);
	if (type == HANDSHAKE_FORK) {
		memcpy(rf.rf_auth, data, sizeof(rf.rf_auth)); /* uh, why? */
		rf.rf_cancel = cancel;
		IOVPUT(iov[1], rf);
	} else {
		IOVPUT_WITHSIZE(iov[1], __UNCONST(myprogname), bonus);
	}
	rv = send_with_recon(spc, iov, __arraycount(iov));
	if (rv || cancel) {
		if (haslock)
			unputwait_locked(spc, &rw);
		else
			unputwait(spc, &rw);
		if (cancel) {
			goto out;
		}
	} else {
		rv = cliwaitresp(spc, &rw, &omask, haslock);
	}
	if (rv)
		goto out;

	rv = *(int *)rw.rw_data;
	free(rw.rw_data);

 out:
	pthread_sigmask(SIG_SETMASK, &omask, NULL);
	return rv;
}

static int
prefork_req(struct spclient *spc, sigset_t *omask, void **resp)
{
	struct rsp_hdr rhdr;
	struct respwait rw;
	struct iovec iov[1];
	int rv;

	rhdr.rsp_len = sizeof(rhdr);
	rhdr.rsp_class = RUMPSP_REQ;
	rhdr.rsp_type = RUMPSP_PREFORK;
	rhdr.rsp_error = 0;

	IOVPUT(iov[0], rhdr);

	do {
		putwait(spc, &rw, &rhdr);
		rv = send_with_recon(spc, iov, __arraycount(iov));
		if (rv != 0) {
			unputwait(spc, &rw);
			continue;
		}

		rv = cliwaitresp(spc, &rw, omask, false);
		if (rv == ENOTCONN)
			rv = EAGAIN;
	} while (rv == EAGAIN);

	*resp = rw.rw_data;
	return rv;
}

/*
 * prevent response code from deadlocking with reconnect code
 */
static int
resp_sendlock(struct spclient *spc)
{
	int rv = 0;

	pthread_mutex_lock(&spc->spc_mtx);
	while (spc->spc_ostatus != SPCSTATUS_FREE) {
		if (__predict_false(spc->spc_reconnecting)) {
			rv = EBUSY;
			goto out;
		}
		spc->spc_ostatus = SPCSTATUS_WANTED;
		pthread_cond_wait(&spc->spc_cv, &spc->spc_mtx);
	}
	spc->spc_ostatus = SPCSTATUS_BUSY;

 out:
	pthread_mutex_unlock(&spc->spc_mtx);
	return rv;
}

static void
send_copyin_resp(struct spclient *spc, uint64_t reqno, void *data, size_t dlen,
	int wantstr)
{
	struct rsp_hdr rhdr;
	struct iovec iov[2];

	if (wantstr)
		dlen = MIN(dlen, strlen(data)+1);

	rhdr.rsp_len = sizeof(rhdr) + dlen;
	rhdr.rsp_reqno = reqno;
	rhdr.rsp_class = RUMPSP_RESP;
	rhdr.rsp_type = RUMPSP_COPYIN;
	rhdr.rsp_sysnum = 0;

	IOVPUT(iov[0], rhdr);
	IOVPUT_WITHSIZE(iov[1], data, dlen);

	if (resp_sendlock(spc) != 0)
		return;
	(void)SENDIOV(spc, iov);
	sendunlock(spc);
}

static void
send_anonmmap_resp(struct spclient *spc, uint64_t reqno, void *addr)
{
	struct rsp_hdr rhdr;
	struct iovec iov[2];

	rhdr.rsp_len = sizeof(rhdr) + sizeof(addr);
	rhdr.rsp_reqno = reqno;
	rhdr.rsp_class = RUMPSP_RESP;
	rhdr.rsp_type = RUMPSP_ANONMMAP;
	rhdr.rsp_sysnum = 0;

	IOVPUT(iov[0], rhdr);
	IOVPUT(iov[1], addr);

	if (resp_sendlock(spc) != 0)
		return;
	(void)SENDIOV(spc, iov);
	sendunlock(spc);
}

int
rumpclient_syscall(int sysnum, const void *data, size_t dlen,
	register_t *retval)
{
	struct rsp_sysresp *resp;
	sigset_t omask;
	void *rdata;
	int rv;

	pthread_sigmask(SIG_SETMASK, &fullset, &omask);

	DPRINTF(("rumpsp syscall_req: syscall %d with %p/%zu\n",
	    sysnum, data, dlen));

	rv = syscall_req(&clispc, &omask, sysnum, data, dlen, &rdata);
	if (rv)
		goto out;

	resp = rdata;
	DPRINTF(("rumpsp syscall_resp: syscall %d error %d, rv: %d/%d\n",
	    sysnum, rv, resp->rsys_retval[0], resp->rsys_retval[1]));

	memcpy(retval, &resp->rsys_retval, sizeof(resp->rsys_retval));
	rv = resp->rsys_error;
	free(rdata);

 out:
	pthread_sigmask(SIG_SETMASK, &omask, NULL);
	return rv;
}

static void
handlereq(struct spclient *spc)
{
	struct rsp_copydata *copydata;
	struct rsp_hdr *rhdr = &spc->spc_hdr;
	void *mapaddr;
	size_t maplen;
	int reqtype = spc->spc_hdr.rsp_type;
	int sig;

	switch (reqtype) {
	case RUMPSP_COPYIN:
	case RUMPSP_COPYINSTR:
		/*LINTED*/
		copydata = (struct rsp_copydata *)spc->spc_buf;
		DPRINTF(("rump_sp handlereq: copyin request: %p/%zu\n",
		    copydata->rcp_addr, copydata->rcp_len));
		send_copyin_resp(spc, spc->spc_hdr.rsp_reqno,
		    copydata->rcp_addr, copydata->rcp_len,
		    reqtype == RUMPSP_COPYINSTR);
		break;
	case RUMPSP_COPYOUT:
	case RUMPSP_COPYOUTSTR:
		/*LINTED*/
		copydata = (struct rsp_copydata *)spc->spc_buf;
		DPRINTF(("rump_sp handlereq: copyout request: %p/%zu\n",
		    copydata->rcp_addr, copydata->rcp_len));
		/*LINTED*/
		memcpy(copydata->rcp_addr, copydata->rcp_data,
		    copydata->rcp_len);
		break;
	case RUMPSP_ANONMMAP:
		/*LINTED*/
		maplen = *(size_t *)spc->spc_buf;
		mapaddr = mmap(NULL, maplen, PROT_READ|PROT_WRITE,
		    MAP_ANON|MAP_PRIVATE, -1, 0);
		if (mapaddr == MAP_FAILED)
			mapaddr = NULL;
		DPRINTF(("rump_sp handlereq: anonmmap: %p\n", mapaddr));
		send_anonmmap_resp(spc, spc->spc_hdr.rsp_reqno, mapaddr);
		break;
	case RUMPSP_RAISE:
		sig = rumpuser__sig_rump2host(rhdr->rsp_signo);
		DPRINTF(("rump_sp handlereq: raise sig %d\n", sig));
		raise(sig);
		/*
		 * We most likely have signals blocked, but the signal
		 * will be handled soon enough when we return.
		 */
		break;
	default:
		printf("PANIC: INVALID TYPE %d\n", reqtype);
		abort();
		break;
	}

	spcfreebuf(spc);
}

static unsigned ptab_idx;
static struct sockaddr *serv_sa;

/* dup until we get a "good" fd which does not collide with stdio */
static int
dupgood(int myfd, int mustchange)
{
	int ofds[4];
	int sverrno;
	unsigned int i;

	for (i = 0; (myfd <= 2 || mustchange) && myfd != -1; i++) {
		assert(i < __arraycount(ofds));
		ofds[i] = myfd;
		myfd = host_dup(myfd);
		if (mustchange) {
			i--; /* prevent closing old fd */
			mustchange = 0;
		}
	}

	sverrno = 0;
	if (myfd == -1 && i > 0)
		sverrno = errno;

	while (i-- > 0) {
		host_close(ofds[i]);
	}

	if (sverrno)
		errno = sverrno;

	return myfd;
}

#if defined(USE_KQUEUE)

static int
makeholyfd(void)
{
	struct kevent kev[NSIG+1];
	int i, fd;

	/* setup kqueue, we want all signals and the fd */
	if ((fd = dupgood(host_kqueue(), 0)) == -1) {
		ERRLOG(("rump_sp: cannot setup kqueue"));
		return -1;
	}

	for (i = 0; i < NSIG; i++) {
		EV_SET(&kev[i], i+1, EVFILT_SIGNAL, EV_ADD|EV_ENABLE, 0, 0, 0);
	}
	EV_SET(&kev[NSIG], clispc.spc_fd,
	    EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, 0);
	if (host_kevent(fd, kev, NSIG+1, NULL, 0, NULL) == -1) {
		ERRLOG(("rump_sp: kevent() failed"));
		host_close(fd);
		return -1;
	}

	return fd;
}

#elif defined(USE_SIGNALFD) /* !USE_KQUEUE */

static int
makeholyfd(void)
{

	return host_signalfd(-1, &fullset, 0);
}

#else /* !USE_KQUEUE && !USE_SIGNALFD */

static int
makeholyfd(void)
{

	return -1;
}

#endif

static int
doconnect(void)
{
	struct respwait rw;
	struct rsp_hdr rhdr;
	char banner[MAXBANNER];
	int s, error, flags;
	ssize_t n;

	if (holyfd != -1)
		host_close(holyfd);
	holyfd = -1;
	s = -1;

	if (clispc.spc_fd != -1)
		host_close(clispc.spc_fd);
	clispc.spc_fd = -1;

	/*
	 * for reconnect, gate everyone out of the receiver code
	 */
	putwait_locked(&clispc, &rw, &rhdr);

	pthread_mutex_lock(&clispc.spc_mtx);
	clispc.spc_reconnecting = 1;
	pthread_cond_broadcast(&clispc.spc_cv);
	clispc.spc_generation++;
	while (clispc.spc_istatus != SPCSTATUS_FREE) {
		clispc.spc_istatus = SPCSTATUS_WANTED;
		pthread_cond_wait(&rw.rw_cv, &clispc.spc_mtx);
	}
	kickall(&clispc);

	/*
	 * we can release it already since we hold the
	 * send lock during reconnect
	 * XXX: assert it
	 */
	clispc.spc_istatus = SPCSTATUS_FREE;
	pthread_mutex_unlock(&clispc.spc_mtx);
	unputwait_locked(&clispc, &rw);

	free(clispc.spc_buf);
	clispc.spc_off = 0;

	s = dupgood(host_socket(parsetab[ptab_idx].domain, SOCK_STREAM, 0), 0);
	if (s == -1)
		return -1;

	while (host_connect(s, serv_sa, parsetab[ptab_idx].slen) == -1) {
		if (errno == EINTR)
			continue;
		ERRLOG(("rump_sp: client connect failed: %s\n",
		    strerror(errno)));
		return -1;
	}

	if ((error = parsetab[ptab_idx].connhook(s)) != 0) {
		ERRLOG(("rump_sp: connect hook failed\n"));
		return -1;
	}

	if ((n = host_read(s, banner, sizeof(banner)-1)) <= 0) {
		ERRLOG(("rump_sp: failed to read banner\n"));
		return -1;
	}

	if (banner[n-1] != '\n') {
		ERRLOG(("rump_sp: invalid banner\n"));
		return -1;
	}
	banner[n] = '\0';
	/* XXX parse the banner some day */

	flags = host_fcntl(s, F_GETFL, 0);
	if (host_fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
		ERRLOG(("rump_sp: socket fd NONBLOCK: %s\n", strerror(errno)));
		return -1;
	}
	clispc.spc_fd = s;
	clispc.spc_state = SPCSTATE_RUNNING;
	clispc.spc_reconnecting = 0;
	holyfd = makeholyfd();

	return 0;
}

static int
doinit(void)
{

	TAILQ_INIT(&clispc.spc_respwait);
	pthread_mutex_init(&clispc.spc_mtx, NULL);
	pthread_cond_init(&clispc.spc_cv, NULL);

	return 0;
}

#ifdef RTLD_NEXT
void *rumpclient__dlsym(void *, const char *);
void *
rumpclient__dlsym(void *handle, const char *symbol)
{

	return dlsym(handle, symbol);
}
void *rumphijack_dlsym(void *, const char *)
    __attribute__((__weak__, alias("rumpclient__dlsym")));
#endif

static pid_t init_done = 0;

int
rumpclient_init(void)
{
	char *p;
	int error;
	int rv = -1;
	int hstype;
	pid_t mypid;

	/*
	 * Make sure we're not riding the context of a previous
	 * host fork.  Note: it's *possible* that after n>1 forks
	 * we have the same pid as one of our exited parents, but
	 * I'm pretty sure there are 0 practical implications, since
	 * it means generations would have to skip rumpclient init.
	 */
	if (init_done == (mypid = getpid()))
		return 0;

#ifdef USE_KQUEUE
	/* kq does not traverse fork() */
	holyfd = -1;
#endif
	init_done = mypid;

	sigfillset(&fullset);

	/*
	 * sag mir, wo die symbols sind.  zogen fort, der krieg beginnt.
	 * wann wird man je verstehen?  wann wird man je verstehen?
	 */
#ifdef RTLD_NEXT
#define FINDSYM2(_name_,_syscall_)					\
	if ((host_##_name_ = rumphijack_dlsym(RTLD_NEXT,		\
	    #_syscall_)) == NULL) {					\
		if (rumphijack_dlsym == rumpclient__dlsym)		\
			host_##_name_ = _name_; /* static fallback */	\
		if (host_##_name_ == NULL) {				\
			fprintf(stderr,"cannot find %s: %s", #_syscall_,\
			    dlerror());					\
			exit(1);					\
		}							\
	}
#else
#define FINDSYM2(_name_,_syscall)					\
	host_##_name_ = _name_;
#endif
#define FINDSYM(_name_) FINDSYM2(_name_,_name_)
#ifdef __NetBSD__
	FINDSYM2(socket,__socket30)
#else
	FINDSYM(socket)
#endif

	FINDSYM(close)
	FINDSYM(connect)
	FINDSYM(fcntl)
	FINDSYM(poll)
	FINDSYM(read)
	FINDSYM(sendmsg)
	FINDSYM(setsockopt)
	FINDSYM(dup)
	FINDSYM(execve)

#ifdef USE_KQUEUE
	FINDSYM(kqueue)
#ifdef __NetBSD__
#if !__NetBSD_Prereq__(5,99,7)
	FINDSYM(kevent)
#else
	FINDSYM2(kevent,_sys___kevent50)
#endif
#else
	FINDSYM(kevent)
#endif
#endif /* USE_KQUEUE */

#ifdef USE_SIGNALFD
	FINDSYM(signalfd)
#endif

#undef	FINDSYM
#undef	FINDSY2

	if ((p = getenv("RUMP__PARSEDSERVER")) == NULL) {
		if ((p = getenv("RUMP_SERVER")) == NULL) {
			fprintf(stderr, "error: RUMP_SERVER not set\n");
			errno = ENOENT;
			goto out;
		}
	}

	if ((error = parseurl(p, &serv_sa, &ptab_idx, 0)) != 0) {
		errno = error;
		goto out;
	}

	if (doinit() == -1)
		goto out;

	if ((p = getenv("RUMPCLIENT__EXECFD")) != NULL) {
		sscanf(p, "%d,%d", &clispc.spc_fd, &holyfd);
		unsetenv("RUMPCLIENT__EXECFD");
		hstype = HANDSHAKE_EXEC;
	} else {
		if (doconnect() == -1)
			goto out;
		hstype = HANDSHAKE_GUEST;
	}

	error = handshake_req(&clispc, hstype, NULL, 0, false);
	if (error) {
		pthread_mutex_destroy(&clispc.spc_mtx);
		pthread_cond_destroy(&clispc.spc_cv);
		if (clispc.spc_fd != -1)
			host_close(clispc.spc_fd);
		errno = error;
		goto out;
	}
	rv = 0;

 out:
	if (rv == -1)
		init_done = 0;
	return rv;
}

struct rumpclient_fork {
	uint32_t fork_auth[AUTHLEN];
	struct spclient fork_spc;
	int fork_holyfd;
};

struct rumpclient_fork *
rumpclient_prefork(void)
{
	struct rumpclient_fork *rpf;
	sigset_t omask;
	void *resp;
	int rv;

	pthread_sigmask(SIG_SETMASK, &fullset, &omask);
	rpf = malloc(sizeof(*rpf));
	if (rpf == NULL)
		goto out;

	if ((rv = prefork_req(&clispc, &omask, &resp)) != 0) {
		free(rpf);
		errno = rv;
		rpf = NULL;
		goto out;
	}

	memcpy(rpf->fork_auth, resp, sizeof(rpf->fork_auth));
	free(resp);

	rpf->fork_spc = clispc;
	rpf->fork_holyfd = holyfd;

 out:
	pthread_sigmask(SIG_SETMASK, &omask, NULL);
	return rpf;
}

int
rumpclient_fork_init(struct rumpclient_fork *rpf)
{
	int error;
	int osock;

	osock = clispc.spc_fd;
	memset(&clispc, 0, sizeof(clispc));
	clispc.spc_fd = osock;

#ifdef USE_KQUEUE
	holyfd = -1; /* kqueue descriptor is not copied over fork() */
#else
	if (holyfd != -1) {
		host_close(holyfd);
		holyfd = -1;
	}
#endif

	if (doinit() == -1)
		return -1;
	if (doconnect() == -1)
		return -1;

	error = handshake_req(&clispc, HANDSHAKE_FORK, rpf->fork_auth,
	    0, false);
	if (error) {
		pthread_mutex_destroy(&clispc.spc_mtx);
		pthread_cond_destroy(&clispc.spc_cv);
		errno = error;
		return -1;
	}

	return 0;
}

/*ARGSUSED*/
void
rumpclient_fork_cancel(struct rumpclient_fork *rpf)
{

	/* EUNIMPL */
}

void
rumpclient_fork_vparent(struct rumpclient_fork *rpf)
{

	clispc = rpf->fork_spc;
	holyfd = rpf->fork_holyfd;
}

void
rumpclient_setconnretry(time_t timeout)
{

	if (timeout < RUMPCLIENT_RETRYCONN_DIE)
		return; /* gigo */

	retrytimo = timeout;
}

int
rumpclient__closenotify(int *fdp, enum rumpclient_closevariant variant)
{
	int fd = *fdp;
	int untilfd, rv;
	int newfd;

	switch (variant) {
	case RUMPCLIENT_CLOSE_FCLOSEM:
		untilfd = MAX(clispc.spc_fd, holyfd);
		for (; fd <= untilfd; fd++) {
			if (fd == clispc.spc_fd || fd == holyfd)
				continue;
			rv = host_close(fd);
			if (rv == -1)
				return -1;
		}
		*fdp = fd;
		break;

	case RUMPCLIENT_CLOSE_CLOSE:
	case RUMPCLIENT_CLOSE_DUP2:
		if (fd == clispc.spc_fd) {
			newfd = dupgood(clispc.spc_fd, 1);
			if (newfd == -1)
				return -1;

#ifdef USE_KQUEUE
			{
			struct kevent kev[2];

			/*
			 * now, we have a new socket number, so change
			 * the file descriptor that kqueue is
			 * monitoring.  remove old and add new.
			 */
			EV_SET(&kev[0], clispc.spc_fd,
			    EVFILT_READ, EV_DELETE, 0, 0, 0);
			EV_SET(&kev[1], newfd,
			    EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, 0);
			if (host_kevent(holyfd, kev, 2, NULL, 0, NULL) == -1) {
				int sverrno = errno;
				host_close(newfd);
				errno = sverrno;
				return -1;
			}}
#endif /* !USE_KQUEUE */
			clispc.spc_fd = newfd;
		}
		if (holyfd != -1 && fd == holyfd) {
			newfd = dupgood(holyfd, 1);
			if (newfd == -1)
				return -1;
			holyfd = newfd;
		}
		break;
	}

	return 0;
}

pid_t
rumpclient_fork(void)
{

	return rumpclient__dofork(fork);
}

/*
 * Process is about to exec.  Save info about our existing connection
 * in the env.  rumpclient will check for this info in init().
 * This is mostly for the benefit of rumphijack, but regular applications
 * may use it as well.
 */
int
rumpclient_exec(const char *path, char *const argv[], char *const envp[])
{
	char buf[4096];
	char **newenv;
	char *envstr, *envstr2;
	size_t nelem;
	int rv, sverrno;

	snprintf(buf, sizeof(buf), "RUMPCLIENT__EXECFD=%d,%d",
	    clispc.spc_fd, holyfd);
	envstr = malloc(strlen(buf)+1);
	if (envstr == NULL) {
		return ENOMEM;
	}
	strcpy(envstr, buf);

	/* do we have a fully parsed url we want to forward in the env? */
	if (*parsedurl != '\0') {
		snprintf(buf, sizeof(buf),
		    "RUMP__PARSEDSERVER=%s", parsedurl);
		envstr2 = malloc(strlen(buf)+1);
		if (envstr2 == NULL) {
			free(envstr);
			return ENOMEM;
		}
		strcpy(envstr2, buf);
	} else {
		envstr2 = NULL;
	}

	for (nelem = 0; envp && envp[nelem]; nelem++)
		continue;

	newenv = malloc(sizeof(*newenv) * (nelem+3));
	if (newenv == NULL) {
		free(envstr2);
		free(envstr);
		return ENOMEM;
	}
	memcpy(&newenv[0], envp, nelem*sizeof(*envp));

	newenv[nelem] = envstr;
	newenv[nelem+1] = envstr2;
	newenv[nelem+2] = NULL;

	rv = host_execve(path, argv, newenv);

	_DIAGASSERT(rv != 0);
	sverrno = errno;
	free(envstr2);
	free(envstr);
	free(newenv);
	errno = sverrno;
	return rv;
}

/*
 * daemon() is handwritten for the benefit of platforms which
 * do not support daemon().
 */
int
rumpclient_daemon(int nochdir, int noclose)
{
	struct rumpclient_fork *rf;
	int sverrno;

	if ((rf = rumpclient_prefork()) == NULL)
		return -1;

	switch (fork()) {
	case 0:
		break;
	case -1:
		goto daemonerr;
	default:
		_exit(0);
	}

	if (setsid() == -1)
		goto daemonerr;
	if (!nochdir && chdir("/") == -1)
		goto daemonerr;
	if (!noclose) {
		int fd = open("/dev/null", O_RDWR);
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd);
	}

	/* note: fork is either completed or cancelled by the call */
	if (rumpclient_fork_init(rf) == -1)
		return -1;

	return 0;

 daemonerr:
	sverrno = errno;
	rumpclient_fork_cancel(rf);
	errno = sverrno;
	return -1;
}
