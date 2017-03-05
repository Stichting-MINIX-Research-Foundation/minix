/*	$NetBSD: daemon-bozo.c,v 1.16 2014/01/02 08:21:38 mrg Exp $	*/

/*	$eterna: daemon-bozo.c,v 1.24 2011/11/18 09:21:15 mrg Exp $	*/

/*
 * Copyright (c) 1997-2014 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer and
 *    dedication in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* this code implements daemon mode for bozohttpd */

#ifndef NO_DAEMON_MODE

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bozohttpd.h"

static	void	sigchild(int);	/* SIGCHLD handler */

#ifndef POLLRDNORM
#define POLLRDNORM 0
#endif
#ifndef POLLRDBAND
#define POLLRDBAND 0
#endif
#ifndef INFTIM
#define INFTIM -1
#endif

static const char* pidfile_path = NULL;
static pid_t pidfile_pid = 0;

/* ARGSUSED */
static void
sigchild(int signo)
{
	while (waitpid(-1, NULL, WNOHANG) > 0) {
	}
}

/* Signal handler to exit in a controlled manner.  This ensures that
 * any atexit(3) handlers are properly executed. */
/* ARGSUSED */
BOZO_DEAD static void
controlled_exit(int signo)
{

	exit(EXIT_SUCCESS);
}

static void
remove_pidfile(void)
{

	if (pidfile_path != NULL && pidfile_pid == getpid()) {
		(void)unlink(pidfile_path);
		pidfile_path = NULL;
	}
}

static void
create_pidfile(bozohttpd_t *httpd)
{
	FILE *file;

	assert(pidfile_path == NULL);

	if (httpd->pidfile == NULL)
		return;

	if (atexit(remove_pidfile) == -1)
		bozo_err(httpd, 1, "Failed to install pidfile handler");

	if ((file = fopen(httpd->pidfile, "w")) == NULL)
		bozo_err(httpd, 1, "Failed to create pidfile '%s'",
		    httpd->pidfile);
	(void)fprintf(file, "%d\n", getpid());
	(void)fclose(file);

	pidfile_path = httpd->pidfile;
	pidfile_pid = getpid();

	debug((httpd, DEBUG_FAT, "Created pid file '%s' for pid %d",
	    pidfile_path, pidfile_pid));
}

void
bozo_daemon_init(bozohttpd_t *httpd)
{
	struct addrinfo h, *r, *r0;
	const char	*portnum;
	int e, i, on = 1;

	if (!httpd->background)
		return;

	portnum = (httpd->bindport) ? httpd->bindport : "http";
	
	memset(&h, 0, sizeof(h));
	h.ai_family = PF_UNSPEC;
	h.ai_socktype = SOCK_STREAM;
	h.ai_flags = AI_PASSIVE;
	e = getaddrinfo(httpd->bindaddress, portnum, &h, &r0);
	if (e)
		bozo_err(httpd, 1, "getaddrinfo([%s]:%s): %s",
		    httpd->bindaddress ? httpd->bindaddress : "*",
		    portnum, gai_strerror(e));
	for (r = r0; r != NULL; r = r->ai_next)
		httpd->nsock++;
	httpd->sock = bozomalloc(httpd, httpd->nsock * sizeof(*httpd->sock));
	httpd->fds = bozomalloc(httpd, httpd->nsock * sizeof(*httpd->fds));
	for (i = 0, r = r0; r != NULL; r = r->ai_next) {
		httpd->sock[i] = socket(r->ai_family, SOCK_STREAM, 0);
		if (httpd->sock[i] == -1)
			continue;
		if (setsockopt(httpd->sock[i], SOL_SOCKET, SO_REUSEADDR, &on,
		    sizeof(on)) == -1)
			bozo_warn(httpd, "setsockopt SO_REUSEADDR: %s",
			    strerror(errno));
		if (bind(httpd->sock[i], r->ai_addr, r->ai_addrlen) == -1)
			continue;
		if (listen(httpd->sock[i], SOMAXCONN) == -1)
			continue;
		httpd->fds[i].events = POLLIN | POLLPRI | POLLRDNORM |
				POLLRDBAND | POLLERR;
		httpd->fds[i].fd = httpd->sock[i];
		i++;
	}
	if (i == 0)
		bozo_err(httpd, 1, "could not find any addresses to bind");
	httpd->nsock = i;
	freeaddrinfo(r0);

	if (httpd->foreground == 0)
		daemon(1, 0);

	create_pidfile(httpd);

	bozo_warn(httpd, "started in daemon mode as `%s' port `%s' root `%s'",
	    httpd->virthostname, portnum, httpd->slashdir);

	signal(SIGHUP, controlled_exit);
	signal(SIGINT, controlled_exit);
	signal(SIGTERM, controlled_exit);

	signal(SIGCHLD, sigchild);
}

void
bozo_daemon_closefds(bozohttpd_t *httpd)
{
	int i;

	for (i = 0; i < httpd->nsock; i++)
		close(httpd->sock[i]);
}

static void
daemon_runchild(bozohttpd_t *httpd, int fd)
{
	httpd->request_times++;

	/* setup stdin/stdout/stderr */
	dup2(fd, 0);
	dup2(fd, 1);
	/*dup2(fd, 2);*/
	close(fd);
}

static int
daemon_poll_err(bozohttpd_t *httpd, int fd, int idx)
{
	if ((httpd->fds[idx].revents & (POLLNVAL|POLLERR|POLLHUP)) == 0)
		return 0;

	bozo_warn(httpd, "poll on fd %d pid %d revents %d: %s",
	    httpd->fds[idx].fd, getpid(), httpd->fds[idx].revents,
	    strerror(errno));
	bozo_warn(httpd, "nsock = %d", httpd->nsock);
	close(httpd->sock[idx]);
	httpd->nsock--;
	bozo_warn(httpd, "nsock now = %d", httpd->nsock);
	/* no sockets left */
	if (httpd->nsock == 0)
		exit(0);
	/* last socket closed is the easy case */
	if (httpd->nsock != idx) {
		memmove(&httpd->fds[idx], &httpd->fds[idx+1],
			(httpd->nsock - idx) * sizeof(*httpd->fds));
		memmove(&httpd->sock[idx], &httpd->sock[idx+1],
			(httpd->nsock - idx) * sizeof(*httpd->sock));
	}

	return 1;
}

/*
 * the parent never returns from this function, only children that
 * are ready to run... XXXMRG - still true in fork-lesser bozo?
 */
int
bozo_daemon_fork(bozohttpd_t *httpd)
{
	int i;

	debug((httpd, DEBUG_FAT, "%s: pid %u request_times %d",
		__func__, getpid(),
		httpd->request_times));
	/* if we've handled 5 files, exit and let someone else work */
	if (httpd->request_times > 5 ||
	    (httpd->background == 2 && httpd->request_times > 0))
		_exit(0);

#if 1
	if (httpd->request_times > 0)
		_exit(0);
#endif

	while (httpd->background) {
		struct	sockaddr_storage ss;
		socklen_t slen;
		int fd;

		if (httpd->nsock == 0)
			exit(0);

		/*
		 * wait for a connection, then fork() and return NULL in
		 * the parent, who will come back here waiting for another
		 * connection.  read the request in in the child, and return
		 * it, for processing.
		 */
again:
		if (poll(httpd->fds, (unsigned)httpd->nsock, INFTIM) == -1) {
			/* fail on programmer errors */
			if (errno == EFAULT ||
			    errno == EINVAL)
				bozo_err(httpd, 1, "poll: %s",
					strerror(errno));

			/* sleep on some temporary kernel failures */
			if (errno == ENOMEM ||
			    errno == EAGAIN)
				sleep(1);

			goto again;
		}

		for (i = 0; i < httpd->nsock; i++) {
			if (daemon_poll_err(httpd, fd, i))
				break;
			if (httpd->fds[i].revents == 0)
				continue;

			slen = sizeof(ss);
			fd = accept(httpd->fds[i].fd,
					(struct sockaddr *)(void *)&ss, &slen);
			if (fd == -1) {
				if (errno == EFAULT ||
				    errno == EINVAL)
					bozo_err(httpd, 1, "accept: %s",
						strerror(errno));

				if (errno == ENOMEM ||
				    errno == EAGAIN)
					sleep(1);

				continue;
			}

#if 0
			/*
			 * This code doesn't work.  It interacts very poorly
			 * with ~user translation and needs to be fixed.
			 */
			if (httpd->request_times > 0) {
				daemon_runchild(httpd, fd);
				return 0;
			}
#endif

			switch (fork()) {
			case -1: /* eep, failure */
				bozo_warn(httpd, "fork() failed, sleeping for "
					"10 seconds: %s", strerror(errno));
				close(fd);
				sleep(10);
				break;

			case 0: /* child */
				daemon_runchild(httpd, fd);
				return 0;

			default: /* parent */
				close(fd);
				break;
			}
		}
	}
	return 0;
}

#endif /* NO_DAEMON_MODE */
