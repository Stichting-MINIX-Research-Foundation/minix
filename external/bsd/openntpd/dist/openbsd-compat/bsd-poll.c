/* $Id: bsd-poll.c,v 1.5 2005/07/03 14:29:43 dtucker Exp $ */

/*
 * Copyright (c) 2004, 2005 Darren Tucker (dtucker at zip com au).
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"
#if !defined(HAVE_POLL) && defined(HAVE_SELECT)

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include <errno.h>
#include "bsd-poll.h"

int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	nfds_t i;
	int ret, fd, maxfd = 0;
	fd_set readfds, writefds, exceptfds;
	struct timeval tv, *tvp = NULL;

	if (nfds > FD_SETSIZE) {
		errno = EINVAL;
		return -1;
	}

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);

	/* poll timeout is msec, select is timeval (sec + usec) */
	if (timeout >= 0) {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tvp = &tv;
	}

	/* populate event bit vectors for the events we're interested in */
	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		if (fd == -1)
			continue;
		maxfd = MAX(maxfd, fd);
		if (fds[i].events & POLLIN) {
			FD_SET(fd, &readfds);
			FD_SET(fd, &exceptfds);
		}
		if (fds[i].events & POLLOUT) {
			FD_SET(fd, &writefds);
			FD_SET(fd, &exceptfds);
		}
	}

	ret = select(maxfd + 1, &readfds, &writefds, &exceptfds, tvp);

	/* scan through select results and set poll() flags */
	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		fds[i].revents = 0;
		if (fd == -1)
			continue;
		if (FD_ISSET(fd, &readfds)) {
			fds[i].revents |= POLLIN;
		}
		if (FD_ISSET(fd, &writefds)) {
			fds[i].revents |= POLLOUT;
		}
		if (FD_ISSET(fd, &exceptfds)) {
			fds[i].revents |= POLLERR;
		}
	}
	return ret;
}
#endif
