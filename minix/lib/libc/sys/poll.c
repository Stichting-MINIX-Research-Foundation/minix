/*	$NetBSD: poll.c,v 1.3 2008/04/29 05:46:08 martin Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles Blundell.
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

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/poll.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>

/*
 * Build the select(2) sets from the entries that are still in play -- that
 * is, every entry with a nonnegative descriptor that has not already been
 * answered with POLLNVAL.  Returns the highest descriptor involved, or -1
 * if there is none.
 */
static int
poll_to_sets(struct pollfd *p, nfds_t nfds, fd_set *rd, fd_set *wr,
	fd_set *except)
{
	nfds_t i;
	int highfd;

	FD_ZERO(rd);
	FD_ZERO(wr);
	FD_ZERO(except);

	highfd = -1;
	for (i = 0; i < nfds; i++) {
		if (p[i].fd < 0 || p[i].revents == POLLNVAL)
			continue;
		if (p[i].fd > highfd)
			highfd = p[i].fd;

		if (p[i].events & (POLLIN|POLLRDNORM))
			FD_SET(p[i].fd, rd);
		if (p[i].events & (POLLOUT|POLLWRNORM|POLLWRBAND))
			FD_SET(p[i].fd, wr);
		if (p[i].events & (POLLRDBAND|POLLPRI))
			FD_SET(p[i].fd, except);
	}

	return highfd;
}

/*
 * select(2) fails the whole call when one descriptor in its sets is not
 * open; poll(2) answers that descriptor with POLLNVAL and carries on with
 * the others.  Find the offenders and mark them.  Returns how many were
 * newly found, zero if none -- in which case the EBADF came from something
 * we cannot attribute, and the caller has to pass it on.
 */
static int
poll_mark_bad(struct pollfd *p, nfds_t nfds)
{
	nfds_t i;
	int nbad;

	nbad = 0;
	for (i = 0; i < nfds; i++) {
		if (p[i].fd < 0 || p[i].revents == POLLNVAL)
			continue;
		if (fcntl(p[i].fd, F_GETFD) != -1)
			continue;
		p[i].revents = POLLNVAL;
		nbad++;
	}

	return nbad;
}

int poll(struct pollfd *p, nfds_t nfds, int timout)
{
	fd_set rd, wr, except;
	struct timeval tv, *tvp;
	nfds_t i;
	int highfd, rval, nbad, n;

	/*
	 * select(2) can tell us nothing about POLLHUP, and answers
	 * POLL*BAND and POLLPRI only as far as its exception set goes.
	 * POLLNVAL it cannot express at all -- it fails the whole call for
	 * one bad descriptor -- so that part is done here.
	 */
	nbad = 0;
	for (i = 0; i < nfds; i++) {
		p[i].revents = 0;
		/* No descriptor this wide can be open: OPEN_MAX is below
		 * FD_SETSIZE, and select(2) could not carry it anyway. */
		if (p[i].fd >= FD_SETSIZE) {
			p[i].revents = POLLNVAL;
			nbad++;
		}
	}

	tv.tv_sec = timout / 1000;
	tv.tv_usec = (timout % 1000) * 1000;
	tvp = (timout == -1) ? NULL : &tv;

	for (;;) {
		highfd = poll_to_sets(p, nfds, &rd, &wr, &except);

		/* Everything we were given is invalid: report that, rather
		 * than turning the call into a sleep. */
		if (highfd == -1 && nbad > 0)
			return nbad;

		/* An entry that is already answered is an event, and
		 * poll(2) returns as soon as it has one: look at what is
		 * left, but do not wait for it. */
		if (nbad > 0) {
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			tvp = &tv;
		}

		rval = select(highfd + 1, &rd, &wr, &except, tvp);
		if (rval != -1 || errno != EBADF)
			break;

		if ((n = poll_mark_bad(p, nfds)) == 0)
			return -1;	/* an EBADF that is not ours */
		nbad += n;
	}

	if (rval == -1)
		return -1;
	if (rval == 0)
		return nbad;

	rval = 0;
	for (i = 0; i < nfds; i++) {
		if (p[i].fd < 0 || p[i].revents == POLLNVAL)
			continue;
		if (FD_ISSET(p[i].fd, &rd))
			p[i].revents |= p[i].events & (POLLIN|POLLRDNORM);
		if (FD_ISSET(p[i].fd, &wr))
			p[i].revents |=
			    p[i].events & (POLLOUT|POLLWRNORM|POLLWRBAND);
		if (FD_ISSET(p[i].fd, &except))
			p[i].revents |= p[i].events & (POLLRDBAND|POLLPRI);
		/* XXX: POLLERR/POLLHUP? */
		if (p[i].revents != 0)
			rval++;
	}
	return rval + nbad;
}
