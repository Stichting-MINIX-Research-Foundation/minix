/*      $NetBSD: shmif_user.c,v 1.1 2014/03/13 01:42:59 pooka Exp $	*/

/*-
 * Copyright (c) 2009, 2010 Antti Kantee.  All Rights Reserved.
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
#ifndef _KERNEL
#include <sys/types.h>
#include <sys/mman.h>

#include <errno.h>

#include <rump/rumpuser_component.h>

#include "shmif_user.h"

#define seterr(_v_) if ((_v_) == -1) *error = errno; else *error = 0;

/*
 * On BSD we use kqueue, on Linux we use inotify.  The underlying
 * interface requirements aren't quite the same, but we have a very
 * good chance of doing the fd->path mapping on Linux thanks to dcache,
 * so just keep the existing interfaces for now.
 */
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__) \
    || defined(__OpenBSD__)
#include <sys/event.h>

#include <stdlib.h>

int
rumpcomp_shmif_watchsetup(int *kqp, int fd)
{
	struct kevent kev;
	int rv, kq;

	kq = *kqp;
	if (kq == -1) {
		kq = kqueue();
		if (kq == -1) {
			rv = errno;
			goto out;
		}
	}

	EV_SET(&kev, fd, EVFILT_VNODE, EV_ADD|EV_ENABLE|EV_CLEAR,
	    NOTE_WRITE, 0, 0);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1) {
		rv = errno;
	} else {
		rv = 0;
		*kqp = kq;
	}

 out:
	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_shmif_watchwait(int kq)
{
	void *cookie;
	struct kevent kev;
	int rv;

	cookie = rumpuser_component_unschedule();
	do {
		rv = kevent(kq, NULL, 0, &kev, 1, NULL);
	} while (rv == -1 && errno == EINTR);
	if (rv == -1) {
		rv = errno;
	} else {
		rv = 0;
	}
	rumpuser_component_schedule(cookie);

	return rumpuser_component_errtrans(rv);
}

#elif defined(__linux__)
#include <sys/inotify.h>

#include <limits.h>
#include <stdio.h>
#include <unistd.h>

int
rumpcomp_shmif_watchsetup(int *inotifyp, int fd)
{
	char procbuf[PATH_MAX], linkbuf[PATH_MAX];
	ssize_t nn;
	int inotify, rv;

	inotify = *inotifyp;
	if (inotify == -1) {
		inotify = inotify_init();
		if (inotify == -1) {
			rv = errno;
			goto out;
		}
	}

	/* ok, need to map fd into path for inotify */
	snprintf(procbuf, sizeof(procbuf), "/proc/self/fd/%d", fd);
	nn = readlink(procbuf, linkbuf, sizeof(linkbuf)-1);
	if (nn >= (ssize_t)sizeof(linkbuf)-1) {
		nn = -1;
		errno = E2BIG; /* pick something */
	}
	if (nn == -1) {
		rv = errno;
		close(inotify);
		goto out;
	}

	linkbuf[nn] = '\0';
	if (inotify_add_watch(inotify, linkbuf, IN_MODIFY) == -1) {
		rv = errno;
		close(inotify);
		goto out;
	}
	rv = 0;
	*inotifyp = inotify;

 out:
	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_shmif_watchwait(int kq)
{
	struct inotify_event iev;
	void *cookie;
	ssize_t nn;
	int rv;

	cookie = rumpuser_component_unschedule();
	do {
		nn = read(kq, &iev, sizeof(iev));
	} while (nn == -1 && errno == EINTR);
	if (nn == -1) {
		rv = errno;
	} else {
		rv = 0;
	}
		
	rumpuser_component_schedule(cookie);

	return rumpuser_component_errtrans(rv);
}

#else
#include <stdio.h>
#include <unistd.h>

/* a polling default implementation */
int
rumpcomp_shmif_watchsetup(int *nono, int fd)
{
	static int warned = 0;

	if (!warned) {
		fprintf(stderr, "WARNING: rumpuser writewatchfile routines are "
		    "polling-only on this platform\n");
		warned = 1;
	}

	return 0;
}

int
rumpcomp_shmif_watchwait(int kq)
{
	void *cookie;

	cookie = rumpuser_component_unschedule();
	usleep(10000);
	rumpuser_component_schedule(cookie);

	return 0;
}
#endif

int
rumpcomp_shmif_mmap(int fd, size_t len, void **memp)
{
	void *mem;
	int rv;

	if (ftruncate(fd, len) == -1) {
		rv = errno;
		goto out;
	}

#if defined(__sun__) && !defined(MAP_FILE)
#define MAP_FILE 0
#endif
	
	mem = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED) {
		rv = errno;
	} else {
		rv = 0;
		*memp = mem;
	}

 out:
	return rumpuser_component_errtrans(rv);
}
#endif
