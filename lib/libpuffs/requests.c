/*	$NetBSD: requests.c,v 1.24 2013/01/23 20:22:34 riastradh Exp $	*/

/*
 * Copyright (c) 2007 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Research Foundation of Helsinki University of Technology
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

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: requests.c,v 1.24 2013/01/23 20:22:34 riastradh Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>

#if !defined(__minix)
#include <dev/putter/putter.h>
#endif /* !defined(__minix) */

#include <assert.h>
#include <errno.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "puffs_priv.h"

/*
 * Read a frame from the upstream provider.  First read the frame
 * length and after this read the actual contents.  Yes, optimize
 * me some day.
 */
/*ARGSUSED*/
int
puffs__fsframe_read(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	int fd, int *done)
{
	struct putter_hdr phdr;
	void *win;
	size_t howmuch, winlen, curoff;
	ssize_t n;
	int lenstate;

	/* How much to read? */
 the_next_level:
	curoff = puffs_framebuf_telloff(pb);
	if (curoff < sizeof(struct putter_hdr)) {
		howmuch = sizeof(struct putter_hdr) - curoff;
		lenstate = 1;
	} else {
		puffs_framebuf_getdata_atoff(pb, 0, &phdr, sizeof(phdr));
		/*LINTED*/
		howmuch = phdr.pth_framelen - curoff;
		lenstate = 0;
	}

	if (puffs_framebuf_reserve_space(pb, howmuch) == -1)
		return errno;

	/* Read contents */
	while (howmuch) {
		winlen = howmuch;
		curoff = puffs_framebuf_telloff(pb);
		if (puffs_framebuf_getwindow(pb, curoff, &win, &winlen) == -1)
			return errno;
		n = read(fd, win, winlen);
		switch (n) {
		case 0:
			return ECONNRESET;
		case -1:
			if (errno == EAGAIN)
				return 0;
			return errno;
		default:
			howmuch -= n;
			puffs_framebuf_seekset(pb, curoff + n);
			break;
		}
	}

	if (lenstate)
		goto the_next_level;

	puffs_framebuf_seekset(pb, 0);
	*done = 1;
	return 0;
}

/*
 * Write a frame upstream
 */
/*ARGSUSED*/
int
puffs__fsframe_write(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	int fd, int *done)
{
	void *win;
	uint64_t flen;
	size_t winlen, howmuch, curoff;
	ssize_t n;
	int rv;

	/*
	 * Finalize it if we haven't written anything yet (or we're still
	 * attempting to write the first byte)
	 *
	 * XXX: this shouldn't be here
	 */
	if (puffs_framebuf_telloff(pb) == 0) {
		struct puffs_req *preq;

		winlen = sizeof(struct puffs_req);
		rv = puffs_framebuf_getwindow(pb, 0, (void *)&preq, &winlen);
		if (rv == -1)
			return errno;
		preq->preq_pth.pth_framelen = flen = preq->preq_buflen;
	} else {
		struct putter_hdr phdr;

		puffs_framebuf_getdata_atoff(pb, 0, &phdr, sizeof(phdr));
		flen = phdr.pth_framelen;
	}

	/*
	 * Then write it.  Chances are if we are talking to the kernel it'll
	 * just shlosh in all at once, but if we're e.g. talking to the
	 * network it might take a few tries.
	 */
	/*LINTED*/
	howmuch = flen - puffs_framebuf_telloff(pb);

	while (howmuch) {
		winlen = howmuch;
		curoff = puffs_framebuf_telloff(pb);
		if (puffs_framebuf_getwindow(pb, curoff, &win, &winlen) == -1)
			return errno;

		/*
		 * XXX: we know from the framebuf implementation that we
		 * will always managed to map the entire window.  But if
		 * that changes, this will catch it.  Then we can do stuff
		 * iov stuff instead.
		 */
		assert(winlen == howmuch);

		/* XXX: want NOSIGNAL if writing to a pipe */
#if 0
		n = send(fd, win, winlen, MSG_NOSIGNAL);
#else
		n = write(fd, win, winlen);
#endif
		switch (n) {
		case 0:
			return ECONNRESET;
		case -1:
			if (errno == EAGAIN)
				return 0;
			return errno;
		default:
			howmuch -= n;
			puffs_framebuf_seekset(pb, curoff + n);
			break;
		}
	}

	*done = 1;
	return 0;
}

/*
 * Compare if "pb1" is a response to a previously sent frame pb2.
 * More often than not "pb1" is not a response to anything but
 * rather a fresh request from the kernel.
 */
/*ARGSUSED*/
int
puffs__fsframe_cmp(struct puffs_usermount *pu,
	struct puffs_framebuf *pb1, struct puffs_framebuf *pb2, int *notresp)
{
	struct puffs_req *preq1, *preq2;
	size_t winlen;
	int rv;

	/* map incoming preq */
	winlen = sizeof(struct puffs_req);
	rv = puffs_framebuf_getwindow(pb1, 0, (void *)&preq1, &winlen);
	assert(rv == 0); /* frames are always at least puffs_req in size */
	assert(winlen == sizeof(struct puffs_req));

	/*
	 * Check if this is not a response in this slot.  That's the
	 * likely case.
	 */
	if ((preq1->preq_opclass & PUFFSOPFLAG_ISRESPONSE) == 0) {
		*notresp = 1;
		return 0;
	}

	/* map second preq */
	winlen = sizeof(struct puffs_req);
	rv = puffs_framebuf_getwindow(pb2, 0, (void *)&preq2, &winlen);
	assert(rv == 0); /* frames are always at least puffs_req in size */
	assert(winlen == sizeof(struct puffs_req));

	/* then compare: resid equal? */
	return preq1->preq_id != preq2->preq_id;
}

void
puffs__fsframe_gotframe(struct puffs_usermount *pu, struct puffs_framebuf *pb)
{

	puffs_framebuf_seekset(pb, 0);
	puffs__ml_dispatch(pu, pb);
}
