/*	$OpenBSD: buffer.c,v 1.9 2005/08/11 16:26:29 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/uio.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"

void	buf_enqueue(struct msgbuf *, struct buf *);
void	buf_dequeue(struct msgbuf *, struct buf *);

struct buf *
buf_open(size_t len)
{
	struct buf	*buf;

	if ((buf = calloc(1, sizeof(struct buf))) == NULL)
		return (NULL);
	if ((buf->buf = malloc(len)) == NULL) {
		free(buf);
		return (NULL);
	}
	buf->size = len;

	return (buf);
}

int
buf_add(struct buf *buf, void *data, size_t len)
{
	if (buf->wpos + len > buf->size)
		return (-1);

	memcpy(buf->buf + buf->wpos, data, len);
	buf->wpos += len;
	return (0);
}

int
buf_close(struct msgbuf *msgbuf, struct buf *buf)
{
	buf_enqueue(msgbuf, buf);
	return (1);
}

void
buf_free(struct buf *buf)
{
	free(buf->buf);
	free(buf);
}

void
msgbuf_init(struct msgbuf *msgbuf)
{
	msgbuf->queued = 0;
	msgbuf->fd = -1;
	TAILQ_INIT(&msgbuf->bufs);
}

void
msgbuf_clear(struct msgbuf *msgbuf)
{
	struct buf	*buf;

	while ((buf = TAILQ_FIRST(&msgbuf->bufs)) != NULL)
		buf_dequeue(msgbuf, buf);
}

int
msgbuf_write(struct msgbuf *msgbuf)
{
	struct iovec	 iov[IOV_MAX];
	struct buf	*buf, *next;
	int		 i = 0;
	ssize_t		 n;

	bzero(&iov, sizeof(iov));
	TAILQ_FOREACH(buf, &msgbuf->bufs, entry) {
		if (i >= IOV_MAX)
			break;
		iov[i].iov_base = buf->buf + buf->rpos;
		iov[i].iov_len = buf->size - buf->rpos;
		i++;
	}

	if ((n = writev(msgbuf->fd, iov, i)) == -1) {
		if (errno == EAGAIN || errno == EINTR ||
		    errno == ENOBUFS)	/* try again later */
			return (0);
		else
			return (-1);
	}

	if (n == 0) {			/* connection closed */
		errno = 0;
		return (-2);
	}

	for (buf = TAILQ_FIRST(&msgbuf->bufs); buf != NULL && n > 0;
	    buf = next) {
		next = TAILQ_NEXT(buf, entry);
		if (buf->rpos + n >= buf->size) {
			n -= buf->size - buf->rpos;
			buf_dequeue(msgbuf, buf);
		} else {
			buf->rpos += n;
			n = 0;
		}
	}

	return (0);
}

void
buf_enqueue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_INSERT_TAIL(&msgbuf->bufs, buf, entry);
	msgbuf->queued++;
}

void
buf_dequeue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_REMOVE(&msgbuf->bufs, buf, entry);
	msgbuf->queued--;
	buf_free(buf);
}
