/*	$NetBSD: dmover_io.c,v 1.44 2015/08/20 14:40:17 christos Exp $	*/

/*
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * dmover_io.c: Support for user-space access to dmover-api
 *
 * This interface is quite simple:
 *
 *	1.  The user opens /dev/dmover, which is a cloning device.  This
 *	    allocates internal state for the session.
 *
 *	2.  The user does a DMIO_SETFUNC to select the data movement
 *	    function.  This actually creates the dmover session.
 *
 *	3.  The user writes request messages to its dmover handle.
 *
 *	4.  The user reads request responses from its dmover handle.
 *
 *	5.  The user closes the file descriptor and the session is
 *	    torn down.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dmover_io.c,v 1.44 2015/08/20 14:40:17 christos Exp $");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/select.h>
#include <sys/systm.h>
#include <sys/workqueue.h>
#include <sys/once.h>
#include <sys/stat.h>
#include <sys/kauth.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#include <uvm/uvm_extern.h>

#include <dev/dmover/dmovervar.h>
#include <dev/dmover/dmover_io.h>

#include "ioconf.h"

struct dmio_usrreq_state {
	union {
		struct work u_work;
		TAILQ_ENTRY(dmio_usrreq_state) u_q;
	} dus_u;
#define	dus_q		dus_u.u_q
#define	dus_work	dus_u.u_work
	struct uio dus_uio_out;
	struct uio *dus_uio_in;
	struct dmover_request *dus_req;
	uint32_t dus_id;
	struct vmspace *dus_vmspace;
};

struct dmio_state {
	struct dmover_session *ds_session;
	TAILQ_HEAD(, dmio_usrreq_state) ds_pending;
	TAILQ_HEAD(, dmio_usrreq_state) ds_complete;
	struct selinfo ds_selq;
	volatile int ds_flags;
	u_int ds_nreqs;
	kmutex_t ds_lock;
	kcondvar_t ds_complete_cv;
	kcondvar_t ds_nreqs_cv;
	struct timespec ds_atime;
	struct timespec ds_mtime;
	struct timespec ds_btime;
};

static ONCE_DECL(dmio_cleaner_control);
static struct workqueue *dmio_cleaner;
static int dmio_cleaner_init(void);
static struct dmio_state *dmio_state_get(void);
static void dmio_state_put(struct dmio_state *);
static void dmio_usrreq_fini1(struct work *wk, void *);

#define	DMIO_STATE_SEL		0x0001
#define	DMIO_STATE_DEAD		0x0002
#define	DMIO_STATE_LARVAL	0x0004
#define	DMIO_STATE_READ_WAIT	0x0008
#define	DMIO_STATE_WRITE_WAIT	0x0010

#define	DMIO_NREQS_MAX		64	/* XXX pulled out of a hat */

struct pool dmio_state_pool;
struct pool dmio_usrreq_state_pool;

dev_type_open(dmoverioopen);

const struct cdevsw dmoverio_cdevsw = {
	.d_open = dmoverioopen,
	.d_close = noclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/*
 * dmoverioattach:
 *
 *	Pseudo-device attach routine.
 */
void
dmoverioattach(int count)
{

	pool_init(&dmio_state_pool, sizeof(struct dmio_state),
	    0, 0, 0, "dmiostate", NULL, IPL_SOFTCLOCK);
	pool_init(&dmio_usrreq_state_pool, sizeof(struct dmio_usrreq_state),
	    0, 0, 0, "dmiourstate", NULL, IPL_SOFTCLOCK);
}

/*
 * dmio_cleaner_init:
 *
 *	Create cleaner thread.
 */
static int
dmio_cleaner_init(void)
{

	return workqueue_create(&dmio_cleaner, "dmioclean", dmio_usrreq_fini1,
	    NULL, PWAIT, IPL_SOFTCLOCK, 0);
}

static struct dmio_state *
dmio_state_get(void)
{
	struct dmio_state *ds;

	ds = pool_get(&dmio_state_pool, PR_WAITOK);

	memset(ds, 0, sizeof(*ds));

	getnanotime(&ds->ds_btime);
	ds->ds_atime = ds->ds_mtime = ds->ds_btime;

	mutex_init(&ds->ds_lock, MUTEX_DEFAULT, IPL_SOFTCLOCK);
	cv_init(&ds->ds_complete_cv, "dmvrrd");
	cv_init(&ds->ds_nreqs_cv, "dmiowr");
	TAILQ_INIT(&ds->ds_pending);
	TAILQ_INIT(&ds->ds_complete);
	selinit(&ds->ds_selq);

	return ds;
}

static void
dmio_state_put(struct dmio_state *ds)
{

	seldestroy(&ds->ds_selq);
	cv_destroy(&ds->ds_nreqs_cv);
	cv_destroy(&ds->ds_complete_cv);
	mutex_destroy(&ds->ds_lock);

	pool_put(&dmio_state_pool, ds);
}

/*
 * dmio_usrreq_init:
 *
 *	Build a request structure.
 */
static int
dmio_usrreq_init(struct file *fp, struct dmio_usrreq_state *dus,
    struct dmio_usrreq *req, struct dmover_request *dreq)
{
	struct dmio_state *ds = (struct dmio_state *) fp->f_data;
	struct dmover_session *dses = ds->ds_session;
	struct uio *uio_out = &dus->dus_uio_out;
	struct uio *uio_in;
	dmio_buffer inbuf;
	size_t len;
	int i, error;
	u_int j;

	/* XXX How should malloc interact w/ FNONBLOCK? */

	error = RUN_ONCE(&dmio_cleaner_control, dmio_cleaner_init);
	if (error) {
		return error;
	}

	error = proc_vmspace_getref(curproc, &dus->dus_vmspace);
	if (error) {
		return error;
	}

	if (req->req_outbuf.dmbuf_iovcnt != 0) {
		if (req->req_outbuf.dmbuf_iovcnt > IOV_MAX)
			return (EINVAL);
		len = sizeof(struct iovec) * req->req_outbuf.dmbuf_iovcnt;
		uio_out->uio_iov = malloc(len, M_TEMP, M_WAITOK);
		error = copyin(req->req_outbuf.dmbuf_iov, uio_out->uio_iov,
		    len);
		if (error) {
			free(uio_out->uio_iov, M_TEMP);
			return (error);
		}

		for (j = 0, len = 0; j < req->req_outbuf.dmbuf_iovcnt; j++) {
			len += uio_out->uio_iov[j].iov_len;
			if (len > SSIZE_MAX) {
				free(uio_out->uio_iov, M_TEMP);
				return (EINVAL);
			}
		}

		uio_out->uio_iovcnt = req->req_outbuf.dmbuf_iovcnt;
		uio_out->uio_resid = len;
		uio_out->uio_rw = UIO_READ;
		uio_out->uio_vmspace = dus->dus_vmspace;

		dreq->dreq_outbuf_type = DMOVER_BUF_UIO;
		dreq->dreq_outbuf.dmbuf_uio = uio_out;
	} else {
		uio_out->uio_iov = NULL;
		uio_out = NULL;
		dreq->dreq_outbuf_type = DMOVER_BUF_NONE;
	}

	memcpy(dreq->dreq_immediate, req->req_immediate,
	    sizeof(dreq->dreq_immediate));

	if (dses->dses_ninputs == 0) {
		/* No inputs; all done. */
		return (0);
	}

	dreq->dreq_inbuf_type = DMOVER_BUF_UIO;

	dus->dus_uio_in = malloc(sizeof(struct uio) * dses->dses_ninputs,
	    M_TEMP, M_WAITOK);
	memset(dus->dus_uio_in, 0, sizeof(struct uio) * dses->dses_ninputs);

	for (i = 0; i < dses->dses_ninputs; i++) {
		uio_in = &dus->dus_uio_in[i];

		error = copyin(&req->req_inbuf[i], &inbuf, sizeof(inbuf));
		if (error)
			goto bad;

		if (inbuf.dmbuf_iovcnt > IOV_MAX) {
			error = EINVAL;
			goto bad;
		}
		len = sizeof(struct iovec) * inbuf.dmbuf_iovcnt;
		if (len == 0) {
			error = EINVAL;
			goto bad;
		}
		uio_in->uio_iov = malloc(len, M_TEMP, M_WAITOK);

		error = copyin(inbuf.dmbuf_iov, uio_in->uio_iov, len);
		if (error) {
			free(uio_in->uio_iov, M_TEMP);
			goto bad;
		}

		for (j = 0, len = 0; j < inbuf.dmbuf_iovcnt; j++) {
			len += uio_in->uio_iov[j].iov_len;
			if (len > SSIZE_MAX) {
				free(uio_in->uio_iov, M_TEMP);
				error = EINVAL;
				goto bad;
			}
		}

		if (uio_out != NULL && len != uio_out->uio_resid) {
			free(uio_in->uio_iov, M_TEMP);
			error = EINVAL;
			goto bad;
		}

		uio_in->uio_iovcnt = inbuf.dmbuf_iovcnt;
		uio_in->uio_resid = len;
		uio_in->uio_rw = UIO_WRITE;
		uio_in->uio_vmspace = dus->dus_vmspace;

		dreq->dreq_inbuf[i].dmbuf_uio = uio_in;
	}

	return (0);

 bad:
	if (i > 0) {
		for (--i; i >= 0; i--) {
			uio_in = &dus->dus_uio_in[i];
			free(uio_in->uio_iov, M_TEMP);
		}
	}
	free(dus->dus_uio_in, M_TEMP);
	if (uio_out != NULL)
		free(uio_out->uio_iov, M_TEMP);
	uvmspace_free(dus->dus_vmspace);
	return (error);
}

/*
 * dmio_usrreq_fini:
 *
 *	Tear down a request.  Must be called at splsoftclock().
 */
static void
dmio_usrreq_fini(struct dmio_state *ds, struct dmio_usrreq_state *dus)
{
	struct dmover_session *dses = ds->ds_session;
	struct uio *uio_out = &dus->dus_uio_out;
	struct uio *uio_in;
	int i;

	if (uio_out->uio_iov != NULL)
		free(uio_out->uio_iov, M_TEMP);

	if (dses->dses_ninputs) {
		for (i = 0; i < dses->dses_ninputs; i++) {
			uio_in = &dus->dus_uio_in[i];
			free(uio_in->uio_iov, M_TEMP);
		}
		free(dus->dus_uio_in, M_TEMP);
	}

	workqueue_enqueue(dmio_cleaner, &dus->dus_work, NULL);
}

static void
dmio_usrreq_fini1(struct work *wk, void *dummy)
{
	struct dmio_usrreq_state *dus = (void *)wk;

	KASSERT(wk == &dus->dus_work);

	uvmspace_free(dus->dus_vmspace);
	pool_put(&dmio_usrreq_state_pool, dus);
}

/*
 * dmio_read:
 *
 *	Read file op.
 */
static int
dmio_read(struct file *fp, off_t *offp, struct uio *uio,
    kauth_cred_t cred, int flags)
{
	struct dmio_state *ds = (struct dmio_state *) fp->f_data;
	struct dmio_usrreq_state *dus;
	struct dmover_request *dreq;
	struct dmio_usrresp resp;
	int error = 0, progress = 0;

	if ((uio->uio_resid % sizeof(resp)) != 0)
		return (EINVAL);

	if (ds->ds_session == NULL)
		return (ENXIO);

	getnanotime(&ds->ds_atime);
	mutex_enter(&ds->ds_lock);

	while (uio->uio_resid != 0) {

		for (;;) {
			dus = TAILQ_FIRST(&ds->ds_complete);
			if (dus == NULL) {
				if (fp->f_flag & FNONBLOCK) {
					error = progress ? 0 : EWOULDBLOCK;
					goto out;
				}
				ds->ds_flags |= DMIO_STATE_READ_WAIT;
				error = cv_wait_sig(&ds->ds_complete_cv, &ds->ds_lock);
				if (error)
					goto out;
				continue;
			}
			/* Have a completed request. */
			TAILQ_REMOVE(&ds->ds_complete, dus, dus_q);
			ds->ds_nreqs--;
			if (ds->ds_flags & DMIO_STATE_WRITE_WAIT) {
				ds->ds_flags &= ~DMIO_STATE_WRITE_WAIT;
				cv_broadcast(&ds->ds_nreqs_cv);
			}
			if (ds->ds_flags & DMIO_STATE_SEL) {
				ds->ds_flags &= ~DMIO_STATE_SEL;
				selnotify(&ds->ds_selq, POLLIN | POLLRDNORM, 0);
			}
			break;
		}

		dreq = dus->dus_req;
		resp.resp_id = dus->dus_id;
		if (dreq->dreq_flags & DMOVER_REQ_ERROR)
			resp.resp_error = dreq->dreq_error;
		else {
			resp.resp_error = 0;
			memcpy(resp.resp_immediate, dreq->dreq_immediate,
			    sizeof(resp.resp_immediate));
		}

		dmio_usrreq_fini(ds, dus);

		mutex_exit(&ds->ds_lock);

		progress = 1;

		dmover_request_free(dreq);

		error = uiomove(&resp, sizeof(resp), uio);
		if (error)
			return (error);

		mutex_enter(&ds->ds_lock);
	}

 out:
	mutex_exit(&ds->ds_lock);

	return (error);
}

/*
 * dmio_usrreq_done:
 *
 *	Dmover completion callback.
 */
static void
dmio_usrreq_done(struct dmover_request *dreq)
{
	struct dmio_usrreq_state *dus = dreq->dreq_cookie;
	struct dmio_state *ds = dreq->dreq_session->dses_cookie;

	/* We're already at splsoftclock(). */

	mutex_enter(&ds->ds_lock);
	TAILQ_REMOVE(&ds->ds_pending, dus, dus_q);
	if (ds->ds_flags & DMIO_STATE_DEAD) {
		int nreqs = --ds->ds_nreqs;
		mutex_exit(&ds->ds_lock);
		dmio_usrreq_fini(ds, dus);
		dmover_request_free(dreq);
		if (nreqs == 0) {
			dmio_state_put(ds);
		}
		return;
	}

	TAILQ_INSERT_TAIL(&ds->ds_complete, dus, dus_q);
	if (ds->ds_flags & DMIO_STATE_READ_WAIT) {
		ds->ds_flags &= ~DMIO_STATE_READ_WAIT;
		cv_broadcast(&ds->ds_complete_cv);
	}
	if (ds->ds_flags & DMIO_STATE_SEL) {
		ds->ds_flags &= ~DMIO_STATE_SEL;
		selnotify(&ds->ds_selq, POLLOUT | POLLWRNORM, 0);
	}
	mutex_exit(&ds->ds_lock);
}

/*
 * dmio_write:
 *
 *	Write file op.
 */
static int
dmio_write(struct file *fp, off_t *offp, struct uio *uio,
    kauth_cred_t cred, int flags)
{
	struct dmio_state *ds = (struct dmio_state *) fp->f_data;
	struct dmio_usrreq_state *dus;
	struct dmover_request *dreq;
	struct dmio_usrreq req;
	int error = 0, progress = 0;

	if ((uio->uio_resid % sizeof(req)) != 0)
		return (EINVAL);

	if (ds->ds_session == NULL)
		return (ENXIO);

	getnanotime(&ds->ds_mtime);
	mutex_enter(&ds->ds_lock);

	while (uio->uio_resid != 0) {

		if (ds->ds_nreqs == DMIO_NREQS_MAX) {
			if (fp->f_flag & FNONBLOCK) {
				error = progress ? 0 : EWOULDBLOCK;
				break;
			}
			ds->ds_flags |= DMIO_STATE_WRITE_WAIT;
			error = cv_wait_sig(&ds->ds_complete_cv, &ds->ds_lock);
			if (error)
				break;
			continue;
		}

		ds->ds_nreqs++;

		mutex_exit(&ds->ds_lock);

		progress = 1;

		error = uiomove(&req, sizeof(req), uio);
		if (error) {
			mutex_enter(&ds->ds_lock);
			ds->ds_nreqs--;
			break;
		}

		/* XXX How should this interact with FNONBLOCK? */
		dreq = dmover_request_alloc(ds->ds_session, NULL);
		if (dreq == NULL) {
			/* XXX */
			ds->ds_nreqs--;
			error = ENOMEM;
			return error;
		}
		dus = pool_get(&dmio_usrreq_state_pool, PR_WAITOK);

		error = dmio_usrreq_init(fp, dus, &req, dreq);
		if (error) {
			dmover_request_free(dreq);
			pool_put(&dmio_usrreq_state_pool, dus);
			return error;
		}

		dreq->dreq_callback = dmio_usrreq_done;
		dreq->dreq_cookie = dus;

		dus->dus_req = dreq;
		dus->dus_id = req.req_id;

		mutex_enter(&ds->ds_lock);

		TAILQ_INSERT_TAIL(&ds->ds_pending, dus, dus_q);

		mutex_exit(&ds->ds_lock);

		dmover_process(dreq);

		mutex_enter(&ds->ds_lock);
	}

	mutex_exit(&ds->ds_lock);

	return (error);
}

static int
dmio_stat(struct file *fp, struct stat *st)
{
	struct dmio_state *ds = fp->f_data;

	(void)memset(st, 0, sizeof(*st));
	KERNEL_LOCK(1, NULL);
	st->st_dev = makedev(cdevsw_lookup_major(&dmoverio_cdevsw), 0);
	st->st_atimespec = ds->ds_atime;
	st->st_mtimespec = ds->ds_mtime;
	st->st_ctimespec = st->st_birthtimespec = ds->ds_btime;
	st->st_uid = kauth_cred_geteuid(fp->f_cred);
	st->st_gid = kauth_cred_getegid(fp->f_cred);
	KERNEL_UNLOCK_ONE(NULL);
	return 0;
}

/*
 * dmio_ioctl:
 *
 *	Ioctl file op.
 */
static int
dmio_ioctl(struct file *fp, u_long cmd, void *data)
{
	struct dmio_state *ds = (struct dmio_state *) fp->f_data;
	int error;

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return (0);

	case DMIO_SETFUNC:
	    {
		struct dmio_setfunc *dsf = data;
		struct dmover_session *dses;

		mutex_enter(&ds->ds_lock);

		if (ds->ds_session != NULL ||
		    (ds->ds_flags & DMIO_STATE_LARVAL) != 0) {
			mutex_exit(&ds->ds_lock);
			return (EBUSY);
		}

		ds->ds_flags |= DMIO_STATE_LARVAL;

		mutex_exit(&ds->ds_lock);

		dsf->dsf_name[DMIO_MAX_FUNCNAME - 1] = '\0';
		error = dmover_session_create(dsf->dsf_name, &dses);

		mutex_enter(&ds->ds_lock);

		if (error == 0) {
			dses->dses_cookie = ds;
			ds->ds_session = dses;
		}
		ds->ds_flags &= ~DMIO_STATE_LARVAL;

		mutex_exit(&ds->ds_lock);
		break;
	    }

	default:
		error = ENOTTY;
	}

	return (error);
}

/*
 * dmio_poll:
 *
 *	Poll file op.
 */
static int
dmio_poll(struct file *fp, int events)
{
	struct dmio_state *ds = (struct dmio_state *) fp->f_data;
	int revents = 0;

	if ((events & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)) == 0)
		return (revents);

	mutex_enter(&ds->ds_lock);

	if (ds->ds_flags & DMIO_STATE_DEAD) {
		/* EOF */
		revents |= events & (POLLIN | POLLRDNORM |
		    POLLOUT | POLLWRNORM);
		goto out;
	}

	/* We can read if there are completed requests. */
	if (events & (POLLIN | POLLRDNORM))
		if (TAILQ_EMPTY(&ds->ds_complete) == 0)
			revents |= events & (POLLIN | POLLRDNORM);

	/*
	 * We can write if there is there are fewer then DMIO_NREQS_MAX
	 * are already in the queue.
	 */
	if (events & (POLLOUT | POLLWRNORM))
		if (ds->ds_nreqs < DMIO_NREQS_MAX)
			revents |= events & (POLLOUT | POLLWRNORM);

	if (revents == 0) {
		selrecord(curlwp, &ds->ds_selq);
		ds->ds_flags |= DMIO_STATE_SEL;
	}

 out:
	mutex_exit(&ds->ds_lock);

	return (revents);
}

/*
 * dmio_close:
 *
 *	Close file op.
 */
static int
dmio_close(struct file *fp)
{
	struct dmio_state *ds = (struct dmio_state *) fp->f_data;
	struct dmio_usrreq_state *dus;
	struct dmover_session *dses;

	mutex_enter(&ds->ds_lock);

	ds->ds_flags |= DMIO_STATE_DEAD;

	/* Garbage-collect all the responses on the queue. */
	while ((dus = TAILQ_FIRST(&ds->ds_complete)) != NULL) {
		TAILQ_REMOVE(&ds->ds_complete, dus, dus_q);
		ds->ds_nreqs--;
		mutex_exit(&ds->ds_lock);
		dmover_request_free(dus->dus_req);
		dmio_usrreq_fini(ds, dus);
		mutex_enter(&ds->ds_lock);
	}

	/*
	 * If there are any requests pending, we have to wait for
	 * them.  Don't free the dmio_state in this case.
	 */
	if (ds->ds_nreqs == 0) {
		dses = ds->ds_session;
		mutex_exit(&ds->ds_lock);
		dmio_state_put(ds);
	} else {
		dses = NULL;
		mutex_exit(&ds->ds_lock);
	}

	fp->f_data = NULL;

	if (dses != NULL)
		dmover_session_destroy(dses);

	return (0);
}

static const struct fileops dmio_fileops = {
	.fo_read = dmio_read,
	.fo_write = dmio_write,
	.fo_ioctl = dmio_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = dmio_poll,
	.fo_stat = dmio_stat,
	.fo_close = dmio_close,
	.fo_kqfilter = fnullop_kqfilter,
	.fo_restart = fnullop_restart,
};

/*
 * dmoverioopen:
 *
 *	Device switch open routine.
 */
int
dmoverioopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct dmio_state *ds;
	struct file *fp;
	int error, fd;

	if ((error = fd_allocfile(&fp, &fd)) != 0)
		return (error);

	ds = dmio_state_get();

	return fd_clone(fp, fd, flag, &dmio_fileops, ds);
}
