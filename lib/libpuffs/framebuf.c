/*	$NetBSD: framebuf.c,v 1.32 2012/06/25 22:32:47 abs Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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
 * The event portion of this code is a twisty maze of pointers,
 * flags, yields and continues.  Sincere aplogies.
 */

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: framebuf.c,v 1.32 2012/06/25 22:32:47 abs Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "puffs_priv.h"

struct puffs_framebuf {
	struct puffs_cc *pcc;	/* pcc to continue with */
	/* OR */
	puffs_framev_cb fcb;	/* non-blocking callback */
	void *fcb_arg;		/* argument for previous */

	uint8_t *buf;		/* buffer base */
	size_t len;		/* total length */

	size_t offset;		/* cursor, telloff() */
	size_t maxoff;		/* maximum offset for data, tellsize() */

	volatile int rv;	/* errno value */

	int	istat;

	TAILQ_ENTRY(puffs_framebuf) pfb_entries;
};
#define ISTAT_NODESTROY	0x01	/* indestructible by framebuf_destroy() */
#define ISTAT_INTERNAL	0x02	/* never leaves library			*/
#define ISTAT_NOREPLY	0x04	/* nuke after sending 			*/
#define ISTAT_DIRECT	0x08	/* receive directly, no moveinfo	*/

#define ISTAT_ONQUEUE	ISTAT_NODESTROY	/* alias */

#define PUFBUF_INCRALLOC 4096
#define PUFBUF_REMAIN(p) (p->len - p->offset)

/* for poll/kqueue */
struct puffs_fbevent {
	struct puffs_cc	*pcc;
	int what;
	volatile int rv;

	LIST_ENTRY(puffs_fbevent) pfe_entries;
};

static struct puffs_fctrl_io *
getfiobyfd(struct puffs_usermount *pu, int fd)
{
	struct puffs_fctrl_io *fio;

	LIST_FOREACH(fio, &pu->pu_ios, fio_entries)
		if (fio->io_fd == fd)
			return fio;
	return NULL;
}

struct puffs_framebuf *
puffs_framebuf_make(void)
{
	struct puffs_framebuf *pufbuf;

	pufbuf = malloc(sizeof(struct puffs_framebuf));
	if (pufbuf == NULL)
		return NULL;
	memset(pufbuf, 0, sizeof(struct puffs_framebuf));

	pufbuf->buf = malloc(PUFBUF_INCRALLOC);
	if (pufbuf->buf == NULL) {
		free(pufbuf);
		return NULL;
	}
	pufbuf->len = PUFBUF_INCRALLOC;

	puffs_framebuf_recycle(pufbuf);
	return pufbuf;
}

void
puffs_framebuf_destroy(struct puffs_framebuf *pufbuf)
{

	assert((pufbuf->istat & ISTAT_NODESTROY) == 0);

	free(pufbuf->buf);
	free(pufbuf);
}

void
puffs_framebuf_recycle(struct puffs_framebuf *pufbuf)
{

	assert((pufbuf->istat & ISTAT_NODESTROY) == 0);

	pufbuf->offset = 0;
	pufbuf->maxoff = 0;
	pufbuf->istat = 0;
}

static int
reservespace(struct puffs_framebuf *pufbuf, size_t off, size_t wantsize)
{
	size_t incr;
	void *nd;

	if (off <= pufbuf->len && pufbuf->len - off >= wantsize)
		return 0;

	for (incr = PUFBUF_INCRALLOC;
	    pufbuf->len + incr < off + wantsize;
	    incr += PUFBUF_INCRALLOC)
		continue;

	nd = realloc(pufbuf->buf, pufbuf->len + incr);
	if (nd == NULL)
		return -1;

	pufbuf->buf = nd;
	pufbuf->len += incr;

	return 0;
}

int
puffs_framebuf_dup(struct puffs_framebuf *pb, struct puffs_framebuf **pbp)
{
	struct puffs_framebuf *newpb;

	newpb = puffs_framebuf_make();
	if (newpb == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memcpy(newpb, pb, sizeof(struct puffs_framebuf));

	newpb->buf = NULL;
	newpb->len = 0;
	if (reservespace(newpb, 0, pb->maxoff) == -1) {
		puffs_framebuf_destroy(newpb);
		return -1;
	}

	memcpy(newpb->buf, pb->buf, pb->maxoff);
	newpb->istat = 0;
	*pbp = newpb;

	return 0;
}

int
puffs_framebuf_reserve_space(struct puffs_framebuf *pufbuf, size_t wantsize)
{

	return reservespace(pufbuf, pufbuf->offset, wantsize);
}

int
puffs_framebuf_putdata(struct puffs_framebuf *pufbuf,
	const void *data, size_t dlen)
{

	if (PUFBUF_REMAIN(pufbuf) < dlen)
		if (puffs_framebuf_reserve_space(pufbuf, dlen) == -1)
			return -1;

	memcpy(pufbuf->buf + pufbuf->offset, data, dlen);
	pufbuf->offset += dlen;

	if (pufbuf->offset > pufbuf->maxoff)
		pufbuf->maxoff = pufbuf->offset;

	return 0;
}

int
puffs_framebuf_putdata_atoff(struct puffs_framebuf *pufbuf, size_t offset,
	const void *data, size_t dlen)
{

	if (reservespace(pufbuf, offset, dlen) == -1)
		return -1;

	memcpy(pufbuf->buf + offset, data, dlen);

	if (offset + dlen > pufbuf->maxoff)
		pufbuf->maxoff = offset + dlen;

	return 0;
}

int
puffs_framebuf_getdata(struct puffs_framebuf *pufbuf, void *data, size_t dlen)
{

	if (pufbuf->maxoff < pufbuf->offset + dlen) {
		errno = ENOBUFS;
		return -1;
	}

	memcpy(data, pufbuf->buf + pufbuf->offset, dlen);
	pufbuf->offset += dlen;

	return 0;
}

int
puffs_framebuf_getdata_atoff(struct puffs_framebuf *pufbuf, size_t offset,
	void *data, size_t dlen)
{

	if (pufbuf->maxoff < offset + dlen) {
		errno = ENOBUFS;
		return -1;
	}

	memcpy(data, pufbuf->buf + offset, dlen);
	return 0;
}

size_t
puffs_framebuf_telloff(struct puffs_framebuf *pufbuf)
{

	return pufbuf->offset;
}

size_t
puffs_framebuf_tellsize(struct puffs_framebuf *pufbuf)
{

	return pufbuf->maxoff;
}

size_t
puffs_framebuf_remaining(struct puffs_framebuf *pufbuf)
{

	return puffs_framebuf_tellsize(pufbuf) - puffs_framebuf_telloff(pufbuf);
}

int
puffs_framebuf_seekset(struct puffs_framebuf *pufbuf, size_t newoff)
{

	if (reservespace(pufbuf, newoff, 0) == -1)
		return -1;

	pufbuf->offset = newoff;
	return 0;
}

int
puffs_framebuf_getwindow(struct puffs_framebuf *pufbuf, size_t winoff,
	void **data, size_t *dlen)
{
	size_t winlen;

#ifdef WINTESTING
	winlen = MIN(*dlen, 32);
#else
	winlen = *dlen;
#endif

	if (reservespace(pufbuf, winoff, winlen) == -1)
		return -1;

	*data = pufbuf->buf + winoff;
	if (pufbuf->maxoff < winoff + winlen)
		pufbuf->maxoff = winoff + winlen;

	return 0;
}

void *
puffs__framebuf_getdataptr(struct puffs_framebuf *pufbuf)
{

	return pufbuf->buf;
}

static void
errnotify(struct puffs_usermount *pu, struct puffs_framebuf *pufbuf, int error)
{

	pufbuf->rv = error;
	if (pufbuf->pcc) {
		puffs__goto(pufbuf->pcc);
	} else if (pufbuf->fcb) {
		pufbuf->istat &= ~ISTAT_NODESTROY;
		pufbuf->fcb(pu, pufbuf, pufbuf->fcb_arg, error);
	} else {
		pufbuf->istat &= ~ISTAT_NODESTROY;
		puffs_framebuf_destroy(pufbuf);
	}
}

#define GETFIO(fd)							\
do {									\
	fio = getfiobyfd(pu, fd);					\
	if (fio == NULL) {						\
		errno = EINVAL;						\
		return -1;						\
	}								\
	if (fio->stat & FIO_WRGONE) {					\
		errno = ESHUTDOWN;					\
		return -1;						\
	}								\
} while (/*CONSTCOND*/0)

int
puffs_framev_enqueue_cc(struct puffs_cc *pcc, int fd,
	struct puffs_framebuf *pufbuf, int flags)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	struct puffs_fctrl_io *fio;

	/*
	 * Technically we shouldn't allow this if RDGONE, but it's
	 * difficult to trap write close without allowing writes.
	 * And besides, there's probably a disconnect sequence in
	 * the protocol, so unexpectedly getting a closed fd is
	 * most likely an error condition.
	 */
	GETFIO(fd);

	pufbuf->pcc = pcc;
	pufbuf->fcb = NULL;
	pufbuf->fcb_arg = NULL;

	pufbuf->offset = 0;
	pufbuf->istat |= ISTAT_NODESTROY;

	if (flags & PUFFS_FBQUEUE_URGENT)
		TAILQ_INSERT_HEAD(&fio->snd_qing, pufbuf, pfb_entries);
	else
		TAILQ_INSERT_TAIL(&fio->snd_qing, pufbuf, pfb_entries);

	puffs_cc_yield(pcc);
	if (pufbuf->rv) {
		pufbuf->istat &= ~ISTAT_NODESTROY;
		errno = pufbuf->rv;
		return -1;
	}

	return 0;
}

int
puffs_framev_enqueue_cb(struct puffs_usermount *pu, int fd,
	struct puffs_framebuf *pufbuf, puffs_framev_cb fcb, void *arg,
	int flags)
{
	struct puffs_fctrl_io *fio;

	/* see enqueue_cc */
	GETFIO(fd);

	pufbuf->pcc = NULL;
	pufbuf->fcb = fcb;
	pufbuf->fcb_arg = arg;

	pufbuf->offset = 0;
	pufbuf->istat |= ISTAT_NODESTROY;

	if (flags & PUFFS_FBQUEUE_URGENT)
		TAILQ_INSERT_HEAD(&fio->snd_qing, pufbuf, pfb_entries);
	else
		TAILQ_INSERT_TAIL(&fio->snd_qing, pufbuf, pfb_entries);

	return 0;
}

int
puffs_framev_enqueue_justsend(struct puffs_usermount *pu, int fd,
	struct puffs_framebuf *pufbuf, int reply, int flags)
{
	struct puffs_fctrl_io *fio;

	assert((pufbuf->istat & ISTAT_INTERNAL) == 0);

	GETFIO(fd);

	pufbuf->pcc = NULL;
	pufbuf->fcb = NULL;
	pufbuf->fcb_arg = NULL;

	pufbuf->offset = 0;
	pufbuf->istat |= ISTAT_NODESTROY;
	if (!reply)
		pufbuf->istat |= ISTAT_NOREPLY;

	if (flags & PUFFS_FBQUEUE_URGENT)
		TAILQ_INSERT_HEAD(&fio->snd_qing, pufbuf, pfb_entries);
	else
		TAILQ_INSERT_TAIL(&fio->snd_qing, pufbuf, pfb_entries);

	return 0;
}

/* ARGSUSED */
int
puffs_framev_enqueue_directreceive(struct puffs_cc *pcc, int fd,
	struct puffs_framebuf *pufbuf, int flags /* used in the future */)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	struct puffs_fctrl_io *fio;

	assert((pufbuf->istat & ISTAT_INTERNAL) == 0);

	fio = getfiobyfd(pu, fd);
	if (fio == NULL) {
		errno = EINVAL;
		return -1;
	}

	/* XXX: should have cur_in queue */
	assert(fio->cur_in == NULL);
	fio->cur_in = pufbuf;

	pufbuf->pcc = pcc;
	pufbuf->fcb = NULL;
	pufbuf->fcb_arg = NULL;

	pufbuf->offset = 0;
	pufbuf->istat |= ISTAT_NODESTROY | ISTAT_DIRECT;

	puffs_cc_yield(pcc);
	pufbuf->istat &= ~ISTAT_NODESTROY; /* XXX: not the right place */
	if (pufbuf->rv) {
		errno = pufbuf->rv;
		return -1;
	}

	return 0;
}

int
puffs_framev_enqueue_directsend(struct puffs_cc *pcc, int fd,
	struct puffs_framebuf *pufbuf, int flags)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	struct puffs_fctrl_io *fio;

	assert((pufbuf->istat & ISTAT_INTERNAL) == 0);

	if (flags & PUFFS_FBQUEUE_URGENT)
		abort(); /* EOPNOTSUPP for now */

	GETFIO(fd);

	pufbuf->pcc = pcc;
	pufbuf->fcb = NULL;
	pufbuf->fcb_arg = NULL;

	pufbuf->offset = 0;
	pufbuf->istat |= ISTAT_NODESTROY | ISTAT_DIRECT;

	TAILQ_INSERT_TAIL(&fio->snd_qing, pufbuf, pfb_entries);

	puffs_cc_yield(pcc);
	if (pufbuf->rv) {
		pufbuf->istat &= ~ISTAT_NODESTROY;
		errno = pufbuf->rv;
		return -1;
	}

	return 0;
}

int
puffs_framev_framebuf_ccpromote(struct puffs_framebuf *pufbuf,
	struct puffs_cc *pcc)
{

	if ((pufbuf->istat & ISTAT_ONQUEUE) == 0) {
		errno = EBUSY;
		return -1;
	}

	pufbuf->pcc = pcc;
	pufbuf->fcb = NULL;
	pufbuf->fcb_arg = NULL;
	pufbuf->istat &= ~ISTAT_NOREPLY;

	puffs_cc_yield(pcc);

	return 0;
}

int
puffs_framev_enqueue_waitevent(struct puffs_cc *pcc, int fd, int *what)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	struct puffs_fctrl_io *fio;
	struct puffs_fbevent feb;
	struct kevent kev;
	int rv, svwhat;

	svwhat = *what;

	if (*what == 0) {
		errno = EINVAL;
		return -1;
	}

	fio = getfiobyfd(pu, fd);
	if (fio == NULL) {
		errno = EINVAL;
		return -1;
	}

	feb.pcc = pcc;
	feb.what = *what & (PUFFS_FBIO_READ|PUFFS_FBIO_WRITE|PUFFS_FBIO_ERROR);

	if (*what & PUFFS_FBIO_READ)
		if ((fio->stat & FIO_ENABLE_R) == 0)
			EV_SET(&kev, fd, EVFILT_READ, EV_ENABLE,
			    0, 0, (uintptr_t)fio);

	if (kevent(pu->pu_kq, &kev, 1, NULL, 0, NULL) == -1)
		return -1;

	if (*what & PUFFS_FBIO_READ)
		fio->rwait++;
	if (*what & PUFFS_FBIO_WRITE)
		fio->wwait++;

	LIST_INSERT_HEAD(&fio->ev_qing, &feb, pfe_entries);
	puffs_cc_yield(pcc);

	assert(svwhat == *what);

	if (*what & PUFFS_FBIO_READ) {
		fio->rwait--;
		if (fio->rwait == 0 && (fio->stat & FIO_ENABLE_R) == 0) {
			EV_SET(&kev, fd, EVFILT_READ, EV_DISABLE,
			    0, 0, (uintptr_t)fio);
			rv = kevent(pu->pu_kq, &kev, 1, NULL, 0, NULL);
#if 0
			if (rv != 0)
				/* XXXXX oh dear */;
#endif
		}
	}
	if (*what & PUFFS_FBIO_WRITE)
		fio->wwait--;

	if (feb.rv == 0) {
		*what = feb.what;
		rv = 0;
	} else {
		*what = PUFFS_FBIO_ERROR;
		errno = feb.rv;
		rv = -1;
	}

	return rv;
}

void
puffs__framev_notify(struct puffs_fctrl_io *fio, int what)
{
	struct puffs_fbevent *fbevp;

 restart:
	LIST_FOREACH(fbevp, &fio->ev_qing, pfe_entries) {
		if (fbevp->what & what) {
			fbevp->what = what;
			fbevp->rv = 0;
			LIST_REMOVE(fbevp, pfe_entries);
			puffs_cc_continue(fbevp->pcc);
			goto restart;
		}
	}
}

static struct puffs_framebuf *
findbuf(struct puffs_usermount *pu, struct puffs_framectrl *fctrl,
	struct puffs_fctrl_io *fio, struct puffs_framebuf *findme)
{
	struct puffs_framebuf *cand;
	int notresp = 0;

	TAILQ_FOREACH(cand, &fio->res_qing, pfb_entries)
		if (fctrl->cmpfb(pu, findme, cand, &notresp) == 0 || notresp)
			break;

	assert(!(notresp && cand == NULL));
	if (notresp || cand == NULL)
		return NULL;

	TAILQ_REMOVE(&fio->res_qing, cand, pfb_entries);
	return cand;
}

void
puffs__framebuf_moveinfo(struct puffs_framebuf *from, struct puffs_framebuf *to)
{

	assert(from->istat & ISTAT_INTERNAL);

	/* migrate buffer */
	free(to->buf);
	to->buf = from->buf;

	/* migrate buffer info */
	to->len = from->len;
	to->offset = from->offset;
	to->maxoff = from->maxoff;

	from->buf = NULL;
	from->len = 0;
}

void
puffs__framev_input(struct puffs_usermount *pu, struct puffs_framectrl *fctrl,
	struct puffs_fctrl_io *fio)
{
	struct puffs_framebuf *pufbuf, *appbuf;
	int rv, complete;

	while ((fio->stat & FIO_DEAD) == 0 && (fio->stat & FIO_ENABLE_R)) {
		if ((pufbuf = fio->cur_in) == NULL) {
			pufbuf = puffs_framebuf_make();
			if (pufbuf == NULL)
				return;
			pufbuf->istat |= ISTAT_INTERNAL;
			fio->cur_in = pufbuf;
		}

		complete = 0;
		rv = fctrl->rfb(pu, pufbuf, fio->io_fd, &complete);

		/* error */
		if (rv) {
			puffs__framev_readclose(pu, fio, rv);
			fio->cur_in = NULL;
			return;
		}

		/* partial read, come back to fight another day */
		if (complete == 0)
			break;

		/* else: full read, process */
		fio->cur_in = NULL;
		if ((pufbuf->istat & ISTAT_DIRECT) == 0) {
			appbuf = findbuf(pu, fctrl, fio, pufbuf);

			/*
			 * No request for this frame?  If fs implements
			 * gotfb, give frame to that.  Otherwise drop it.
			 */
			if (appbuf == NULL) {
				if (fctrl->gotfb) {
					pufbuf->istat &= ~ISTAT_INTERNAL;
					fctrl->gotfb(pu, pufbuf);
				} else {
					puffs_framebuf_destroy(pufbuf);
				}
				continue;
			}
			
			puffs__framebuf_moveinfo(pufbuf, appbuf);
			puffs_framebuf_destroy(pufbuf);
		} else {
			appbuf = pufbuf;
		}
		appbuf->istat &= ~ISTAT_NODESTROY;
	
		if (appbuf->pcc) {
			puffs__cc_cont(appbuf->pcc);
		} else if (appbuf->fcb) {
			appbuf->fcb(pu, appbuf, appbuf->fcb_arg, 0);
		} else {
			puffs_framebuf_destroy(appbuf);
		}

		/* hopeless romantics, here we go again */
	}
}

int
puffs__framev_output(struct puffs_usermount *pu, struct puffs_framectrl *fctrl,
	struct puffs_fctrl_io *fio)
{
	struct puffs_framebuf *pufbuf;
	int rv, complete, done;

	if (fio->stat & FIO_DEAD)
		return 0;

	for (pufbuf = TAILQ_FIRST(&fio->snd_qing), done = 0;
	    pufbuf && (fio->stat & FIO_DEAD) == 0 && fio->stat & FIO_ENABLE_W;
	    pufbuf = TAILQ_FIRST(&fio->snd_qing)) {
		complete = 0;
		rv = fctrl->wfb(pu, pufbuf, fio->io_fd, &complete);

		if (rv) {
			puffs__framev_writeclose(pu, fio, rv);
			done = 1;
			break;
		}

		/* partial write */
		if (complete == 0)
			return done;

		/* else, complete write */
		TAILQ_REMOVE(&fio->snd_qing, pufbuf, pfb_entries);

		/* can't wait for result if we can't read */
		if (fio->stat & FIO_RDGONE) {
			errnotify(pu, pufbuf, ENXIO);
			done = 1;
		} else if ((pufbuf->istat & ISTAT_DIRECT)) {
			pufbuf->istat &= ~ISTAT_NODESTROY;
			done = 1;
			puffs__cc_cont(pufbuf->pcc);
		} else if ((pufbuf->istat & ISTAT_NOREPLY) == 0) {
			TAILQ_INSERT_TAIL(&fio->res_qing, pufbuf,
			    pfb_entries);
		} else {
			pufbuf->istat &= ~ISTAT_NODESTROY;
			puffs_framebuf_destroy(pufbuf);
		}

		/* omstart! */
	}

	return done;
}

int
puffs__framev_addfd_ctrl(struct puffs_usermount *pu, int fd, int what,
	struct puffs_framectrl *pfctrl)
{
	struct puffs_fctrl_io *fio;
	struct kevent *newevs;
	struct kevent kev[2];
	size_t nevs;
	int rv, readenable;

	nevs = pu->pu_nevs+2;
	newevs = realloc(pu->pu_evs, nevs*sizeof(struct kevent));
	if (newevs == NULL)
		return -1;
	pu->pu_evs = newevs;

	fio = malloc(sizeof(struct puffs_fctrl_io));
	if (fio == NULL)
		return -1;
	memset(fio, 0, sizeof(struct puffs_fctrl_io));
	fio->io_fd = fd;
	fio->cur_in = NULL;
	fio->fctrl = pfctrl;
	TAILQ_INIT(&fio->snd_qing);
	TAILQ_INIT(&fio->res_qing);
	LIST_INIT(&fio->ev_qing);

	readenable = 0;
	if ((what & PUFFS_FBIO_READ) == 0)
		readenable = EV_DISABLE;

	if (pu->pu_state & PU_INLOOP) {
		EV_SET(&kev[0], fd, EVFILT_READ,
		    EV_ADD|readenable, 0, 0, (intptr_t)fio);
		EV_SET(&kev[1], fd, EVFILT_WRITE,
		    EV_ADD|EV_DISABLE, 0, 0, (intptr_t)fio);
		rv = kevent(pu->pu_kq, kev, 2, NULL, 0, NULL);
		if (rv == -1) {
			free(fio);
			return -1;
		}
	}
	if (what & PUFFS_FBIO_READ)
		fio->stat |= FIO_ENABLE_R;
	if (what & PUFFS_FBIO_WRITE)
		fio->stat |= FIO_ENABLE_W;

	LIST_INSERT_HEAD(&pu->pu_ios, fio, fio_entries);
	pu->pu_nevs = nevs;

	return 0;
}

int
puffs_framev_addfd(struct puffs_usermount *pu, int fd, int what)
{

	return puffs__framev_addfd_ctrl(pu, fd, what,
	    &pu->pu_framectrl[PU_FRAMECTRL_USER]);
}

/*
 * XXX: the following en/disable should be coalesced and executed
 * only during the actual kevent call.  So feel free to fix if
 * threatened by mindblowing boredom.
 */

int
puffs_framev_enablefd(struct puffs_usermount *pu, int fd, int what)
{
	struct kevent kev;
	struct puffs_fctrl_io *fio;
	int rv = 0;

	assert((what & (PUFFS_FBIO_READ | PUFFS_FBIO_WRITE)) != 0);

	fio = getfiobyfd(pu, fd);
	if (fio == NULL) {
		errno = ENXIO;
		return -1;
	}

	/* write is enabled in the event loop if there is output */
	if (what & PUFFS_FBIO_READ && fio->rwait == 0) {
		EV_SET(&kev, fd, EVFILT_READ, EV_ENABLE, 0, 0, (uintptr_t)fio);
		rv = kevent(pu->pu_kq, &kev, 1, NULL, 0, NULL);
	}

	if (rv == 0) {
		if (what & PUFFS_FBIO_READ)
			fio->stat |= FIO_ENABLE_R;
		if (what & PUFFS_FBIO_WRITE)
			fio->stat |= FIO_ENABLE_W;
	}

	return rv;
}

int
puffs_framev_disablefd(struct puffs_usermount *pu, int fd, int what)
{
	struct kevent kev[2];
	struct puffs_fctrl_io *fio;
	size_t i;
	int rv;

	assert((what & (PUFFS_FBIO_READ | PUFFS_FBIO_WRITE)) != 0);

	fio = getfiobyfd(pu, fd);
	if (fio == NULL) {
		errno = ENXIO;
		return -1;
	}

	i = 0;
	if (what & PUFFS_FBIO_READ && fio->rwait == 0) {
		EV_SET(&kev[0], fd,
		    EVFILT_READ, EV_DISABLE, 0, 0, (uintptr_t)fio);
		i++;
	}
	if (what & PUFFS_FBIO_WRITE && fio->stat & FIO_WR && fio->wwait == 0) {
		EV_SET(&kev[1], fd,
		    EVFILT_WRITE, EV_DISABLE, 0, 0, (uintptr_t)fio);
		i++;
	}
	if (i)
		rv = kevent(pu->pu_kq, kev, i, NULL, 0, NULL);
	else
		rv = 0;

	if (rv == 0) {
		if (what & PUFFS_FBIO_READ)
			fio->stat &= ~FIO_ENABLE_R;
		if (what & PUFFS_FBIO_WRITE)
			fio->stat &= ~FIO_ENABLE_W;
	}

	return rv;
}

void
puffs__framev_readclose(struct puffs_usermount *pu,
	struct puffs_fctrl_io *fio, int error)
{
	struct puffs_framebuf *pufbuf;
	struct kevent kev;
	int notflag;

	if (fio->stat & FIO_RDGONE || fio->stat & FIO_DEAD)
		return;
	fio->stat |= FIO_RDGONE;

	if (fio->cur_in) {
		if ((fio->cur_in->istat & ISTAT_DIRECT) == 0) {
			puffs_framebuf_destroy(fio->cur_in);
			fio->cur_in = NULL;
		} else {
			errnotify(pu, fio->cur_in, error);
		}
	}

	while ((pufbuf = TAILQ_FIRST(&fio->res_qing)) != NULL) {
		TAILQ_REMOVE(&fio->res_qing, pufbuf, pfb_entries);
		errnotify(pu, pufbuf, error);
	}

	EV_SET(&kev, fio->io_fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
	(void) kevent(pu->pu_kq, &kev, 1, NULL, 0, NULL);

	notflag = PUFFS_FBIO_READ;
	if (fio->stat & FIO_WRGONE)
		notflag |= PUFFS_FBIO_WRITE;

	if (fio->fctrl->fdnotfn)
		fio->fctrl->fdnotfn(pu, fio->io_fd, notflag);
}

void
puffs__framev_writeclose(struct puffs_usermount *pu,
	struct puffs_fctrl_io *fio, int error)
{
	struct puffs_framebuf *pufbuf;
	struct kevent kev;
	int notflag;

	if (fio->stat & FIO_WRGONE || fio->stat & FIO_DEAD)
		return;
	fio->stat |= FIO_WRGONE;

	while ((pufbuf = TAILQ_FIRST(&fio->snd_qing)) != NULL) {
		TAILQ_REMOVE(&fio->snd_qing, pufbuf, pfb_entries);
		errnotify(pu, pufbuf, error);
	}

	EV_SET(&kev, fio->io_fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
	(void) kevent(pu->pu_kq, &kev, 1, NULL, 0, NULL);

	notflag = PUFFS_FBIO_WRITE;
	if (fio->stat & FIO_RDGONE)
		notflag |= PUFFS_FBIO_READ;

	if (fio->fctrl->fdnotfn)
		fio->fctrl->fdnotfn(pu, fio->io_fd, notflag);
}

static int
removefio(struct puffs_usermount *pu, struct puffs_fctrl_io *fio, int error)
{
	struct puffs_fbevent *fbevp;

	LIST_REMOVE(fio, fio_entries);
	if (pu->pu_state & PU_INLOOP) {
		puffs__framev_readclose(pu, fio, error);
		puffs__framev_writeclose(pu, fio, error);
	}

	while ((fbevp = LIST_FIRST(&fio->ev_qing)) != NULL) {
		fbevp->rv = error;
		LIST_REMOVE(fbevp, pfe_entries);
		puffs__goto(fbevp->pcc);
	}

	/* don't bother with realloc */
	pu->pu_nevs -= 2;

	/* don't free us yet, might have some references in event arrays */
	fio->stat |= FIO_DEAD;
	LIST_INSERT_HEAD(&pu->pu_ios_rmlist, fio, fio_entries);

	return 0;

}

int
puffs_framev_removefd(struct puffs_usermount *pu, int fd, int error)
{
	struct puffs_fctrl_io *fio;

	fio = getfiobyfd(pu, fd);
	if (fio == NULL) {
		errno = ENXIO;
		return -1;
	}

	return removefio(pu, fio, error ? error : ECONNRESET);
}

void
puffs_framev_removeonclose(struct puffs_usermount *pu, int fd, int what)
{

	if (what == (PUFFS_FBIO_READ | PUFFS_FBIO_WRITE))
		(void) puffs_framev_removefd(pu, fd, ECONNRESET);
}

void
puffs_framev_unmountonclose(struct puffs_usermount *pu, int fd, int what)
{

	/* XXX & X: unmount is non-sensible */
	puffs_framev_removeonclose(pu, fd, what);
	if (what == (PUFFS_FBIO_READ | PUFFS_FBIO_WRITE))
		PU_SETSTATE(pu, PUFFS_STATE_UNMOUNTED);
}

void
puffs_framev_init(struct puffs_usermount *pu,
	puffs_framev_readframe_fn rfb, puffs_framev_writeframe_fn wfb,
	puffs_framev_cmpframe_fn cmpfb, puffs_framev_gotframe_fn gotfb,
	puffs_framev_fdnotify_fn fdnotfn)
{
	struct puffs_framectrl *pfctrl;

	pfctrl = &pu->pu_framectrl[PU_FRAMECTRL_USER];
	pfctrl->rfb = rfb;
	pfctrl->wfb = wfb;
	pfctrl->cmpfb = cmpfb;
	pfctrl->gotfb = gotfb;
	pfctrl->fdnotfn = fdnotfn;
}

void
puffs__framev_exit(struct puffs_usermount *pu)
{
	struct puffs_fctrl_io *fio;

	while ((fio = LIST_FIRST(&pu->pu_ios)) != NULL)
		removefio(pu, fio, ENXIO);
	free(pu->pu_evs);

	/* closing pu->pu_kq takes care of puffsfd */
}
