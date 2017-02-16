#include <sys/cdefs.h>
 __RCSID("$NetBSD: control.c,v 1.10 2015/08/21 10:39:00 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcpcd.h"
#include "control.h"
#include "eloop.h"
#include "if.h"

#ifndef SUN_LEN
#define SUN_LEN(su) \
            (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

static void
control_queue_purge(struct dhcpcd_ctx *ctx, char *data)
{
	int found;
	struct fd_list *fp;
	struct fd_data *fpd;

	/* If no other fd queue has the same data, free it */
	found = 0;
	TAILQ_FOREACH(fp, &ctx->control_fds, next) {
		TAILQ_FOREACH(fpd, &fp->queue, next) {
			if (fpd->data == data) {
				found = 1;
				break;
			}
		}
	}
	if (!found)
		free(data);
}

static void
control_queue_free(struct fd_list *fd)
{
	struct fd_data *fdp;

	while ((fdp = TAILQ_FIRST(&fd->queue))) {
		TAILQ_REMOVE(&fd->queue, fdp, next);
		if (fdp->freeit)
			control_queue_purge(fd->ctx, fdp->data);
		free(fdp);
	}
	while ((fdp = TAILQ_FIRST(&fd->free_queue))) {
		TAILQ_REMOVE(&fd->free_queue, fdp, next);
		free(fdp);
	}
}

static void
control_delete(struct fd_list *fd)
{

	TAILQ_REMOVE(&fd->ctx->control_fds, fd, next);
	eloop_event_delete(fd->ctx->eloop, fd->fd);
	close(fd->fd);
	control_queue_free(fd);
	free(fd);
}

static void
control_handle_data(void *arg)
{
	struct fd_list *fd = arg;
	char buffer[1024], *e, *p, *argvp[255], **ap, *a;
	ssize_t bytes;
	size_t len;
	int argc;

	bytes = read(fd->fd, buffer, sizeof(buffer) - 1);
	if (bytes == -1 || bytes == 0) {
		/* Control was closed or there was an error.
		 * Remove it from our list. */
		control_delete(fd);
		return;
	}
	buffer[bytes] = '\0';
	p = buffer;
	e = buffer + bytes;

	/* Each command is \n terminated
	 * Each argument is NULL separated */
	while (p < e) {
		argc = 0;
		ap = argvp;
		while (p < e) {
			argc++;
			if ((size_t)argc >= sizeof(argvp) / sizeof(argvp[0])) {
				errno = ENOBUFS;
				return;
			}
			a = *ap++ = p;
			len = strlen(p);
			p += len + 1;
			if (len && a[len - 1] == '\n') {
				a[len - 1] = '\0';
				break;
			}
		}
		*ap = NULL;
		if (dhcpcd_handleargs(fd->ctx, fd, argc, argvp) == -1) {
			logger(fd->ctx, LOG_ERR,
			    "%s: dhcpcd_handleargs: %m", __func__);
			if (errno != EINTR && errno != EAGAIN) {
				control_delete(fd);
				return;
			}
		}
	}
}

static void
control_handle1(struct dhcpcd_ctx *ctx, int lfd, unsigned int fd_flags)
{
	struct sockaddr_un run;
	socklen_t len;
	struct fd_list *l;
	int fd, flags;

	len = sizeof(run);
	if ((fd = accept(lfd, (struct sockaddr *)&run, &len)) == -1)
		return;
	if ((flags = fcntl(fd, F_GETFD, 0)) == -1 ||
	    fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
	{
		close(fd);
	        return;
	}
	if ((flags = fcntl(fd, F_GETFL, 0)) == -1 ||
	    fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		close(fd);
	        return;
	}
	l = malloc(sizeof(*l));
	if (l) {
		l->ctx = ctx;
		l->fd = fd;
		l->flags = fd_flags;
		TAILQ_INIT(&l->queue);
		TAILQ_INIT(&l->free_queue);
		TAILQ_INSERT_TAIL(&ctx->control_fds, l, next);
		eloop_event_add(ctx->eloop, l->fd,
		    control_handle_data, l, NULL, NULL);
	} else
		close(fd);
}

static void
control_handle(void *arg)
{
	struct dhcpcd_ctx *ctx = arg;

	control_handle1(ctx, ctx->control_fd, 0);
}

static void
control_handle_unpriv(void *arg)
{
	struct dhcpcd_ctx *ctx = arg;

	control_handle1(ctx, ctx->control_unpriv_fd, FD_UNPRIV);
}

static int
make_sock(struct sockaddr_un *sa, const char *ifname, int unpriv)
{
	int fd;

	if ((fd = xsocket(AF_UNIX, SOCK_STREAM, 0, O_NONBLOCK|O_CLOEXEC)) == -1)
		return -1;
	memset(sa, 0, sizeof(*sa));
	sa->sun_family = AF_UNIX;
	if (unpriv)
		strlcpy(sa->sun_path, UNPRIVSOCKET, sizeof(sa->sun_path));
	else {
		snprintf(sa->sun_path, sizeof(sa->sun_path), CONTROLSOCKET,
		    ifname ? "-" : "", ifname ? ifname : "");
	}
	return fd;
}

#define S_PRIV (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define S_UNPRIV (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

static int
control_start1(struct dhcpcd_ctx *ctx, const char *ifname, mode_t fmode)
{
	struct sockaddr_un sa;
	int fd;
	socklen_t len;

	if ((fd = make_sock(&sa, ifname, (fmode & S_UNPRIV) == S_UNPRIV)) == -1)
		return -1;
	len = (socklen_t)SUN_LEN(&sa);
	unlink(sa.sun_path);
	if (bind(fd, (struct sockaddr *)&sa, len) == -1 ||
	    chmod(sa.sun_path, fmode) == -1 ||
	    (ctx->control_group &&
	    chown(sa.sun_path, geteuid(), ctx->control_group) == -1) ||
	    listen(fd, sizeof(ctx->control_fds)) == -1)
	{
		close(fd);
		unlink(sa.sun_path);
		return -1;
	}
	
	if ((fmode & S_UNPRIV) != S_UNPRIV)
		strlcpy(ctx->control_sock, sa.sun_path,
		    sizeof(ctx->control_sock));
	return fd;
}

int
control_start(struct dhcpcd_ctx *ctx, const char *ifname)
{
	int fd;

	if ((fd = control_start1(ctx, ifname, S_PRIV)) == -1)
		return -1;

	ctx->control_fd = fd;
	eloop_event_add(ctx->eloop, fd, control_handle, ctx, NULL, NULL);

	if (ifname == NULL && (fd = control_start1(ctx, NULL, S_UNPRIV)) != -1){
		/* We must be in master mode, so create an unpriviledged socket
		 * to allow normal users to learn the status of dhcpcd. */
		ctx->control_unpriv_fd = fd;
		eloop_event_add(ctx->eloop, fd, control_handle_unpriv,
		    ctx, NULL, NULL);
	}
	return ctx->control_fd;
}

int
control_stop(struct dhcpcd_ctx *ctx)
{
	int retval = 0;
	struct fd_list *l;

	if (ctx->options & DHCPCD_FORKED)
		goto freeit;

	if (ctx->control_fd == -1)
		return 0;
	eloop_event_delete(ctx->eloop, ctx->control_fd);
	close(ctx->control_fd);
	ctx->control_fd = -1;
	if (unlink(ctx->control_sock) == -1)
		retval = -1;

	if (ctx->control_unpriv_fd != -1) {
		eloop_event_delete(ctx->eloop, ctx->control_unpriv_fd);
		close(ctx->control_unpriv_fd);
		ctx->control_unpriv_fd = -1;
		if (unlink(UNPRIVSOCKET) == -1)
			retval = -1;
	}

freeit:
	while ((l = TAILQ_FIRST(&ctx->control_fds))) {
		TAILQ_REMOVE(&ctx->control_fds, l, next);
		eloop_event_delete(ctx->eloop, l->fd);
		close(l->fd);
		control_queue_free(l);
		free(l);
	}

	return retval;
}

int
control_open(struct dhcpcd_ctx *ctx, const char *ifname)
{
	struct sockaddr_un sa;
	socklen_t len;

	if ((ctx->control_fd = make_sock(&sa, ifname, 0)) == -1)
		return -1;
	len = (socklen_t)SUN_LEN(&sa);
	if (connect(ctx->control_fd, (struct sockaddr *)&sa, len) == -1) {
		close(ctx->control_fd);
		ctx->control_fd = -1;
		return -1;
	}
	return 0;
}

ssize_t
control_send(struct dhcpcd_ctx *ctx, int argc, char * const *argv)
{
	char buffer[1024];
	int i;
	size_t len, l;

	if (argc > 255) {
		errno = ENOBUFS;
		return -1;
	}
	len = 0;
	for (i = 0; i < argc; i++) {
		l = strlen(argv[i]) + 1;
		if (len + l > sizeof(buffer)) {
			errno = ENOBUFS;
			return -1;
		}
		memcpy(buffer + len, argv[i], l);
		len += l;
	}
	return write(ctx->control_fd, buffer, len);
}

static void
control_writeone(void *arg)
{
	struct fd_list *fd;
	struct iovec iov[2];
	struct fd_data *data;

	fd = arg;
	data = TAILQ_FIRST(&fd->queue);
	iov[0].iov_base = &data->data_len;
	iov[0].iov_len = sizeof(size_t);
	iov[1].iov_base = data->data;
	iov[1].iov_len = data->data_len;
	if (writev(fd->fd, iov, 2) == -1) {
		logger(fd->ctx, LOG_ERR,
		    "%s: writev fd %d: %m", __func__, fd->fd);
		if (errno != EINTR && errno != EAGAIN)
			control_delete(fd);
		return;
	}

	TAILQ_REMOVE(&fd->queue, data, next);
	if (data->freeit)
		control_queue_purge(fd->ctx, data->data);
	data->data = NULL; /* safety */
	data->data_len = 0;
	TAILQ_INSERT_TAIL(&fd->free_queue, data, next);

	if (TAILQ_FIRST(&fd->queue) == NULL)
		eloop_event_remove_writecb(fd->ctx->eloop, fd->fd);
}

int
control_queue(struct fd_list *fd, char *data, size_t data_len, uint8_t fit)
{
	struct fd_data *d;
	size_t n;

	d = TAILQ_FIRST(&fd->free_queue);
	if (d) {
		TAILQ_REMOVE(&fd->free_queue, d, next);
	} else {
		n = 0;
		TAILQ_FOREACH(d, &fd->queue, next) {
			if (++n == CONTROL_QUEUE_MAX) {
				errno = ENOBUFS;
				return -1;
			}
		}
		d = malloc(sizeof(*d));
		if (d == NULL)
			return -1;
	}
	d->data = data;
	d->data_len = data_len;
	d->freeit = fit;
	TAILQ_INSERT_TAIL(&fd->queue, d, next);
	eloop_event_add(fd->ctx->eloop, fd->fd,
	    NULL, NULL, control_writeone, fd);
	return 0;
}

void
control_close(struct dhcpcd_ctx *ctx)
{

	if (ctx->control_fd != -1) {
		close(ctx->control_fd);
		ctx->control_fd = -1;
	}
}
