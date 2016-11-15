/*	$NetBSD: puffs_rumpglue.c,v 1.15 2015/05/10 14:05:22 christos Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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
__KERNEL_RCSID(0, "$NetBSD: puffs_rumpglue.c,v 1.15 2015/05/10 14:05:22 christos Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kthread.h>
#include <sys/mount.h>

#include <dev/putter/putter.h>
#include <dev/putter/putter_sys.h>

#include <rump/rump.h>
#include <rump/rumpuser.h>

#include "rump_vfs_private.h"

void putterattach(void); /* XXX: from autoconf */
dev_type_open(puttercdopen);

struct ptargs {
	int comfd;
	int fpfd;
	struct filedesc *fdp;
};

#define BUFSIZE (64*1024)
extern int hz;

/*
 * Read requests from /dev/puffs and forward them to comfd
 *
 * XXX: the init detection is really sucky, but let's not
 * waste too much energy for a better one here
 */
static void
readthread(void *arg)
{
	struct ptargs *pap = arg;
	struct file *fp;
	register_t rv;
	char *buf;
	off_t off;
	int error, inited;

	buf = kmem_alloc(BUFSIZE, KM_SLEEP);
	inited = 0;

 retry:
	kpause(NULL, 0, hz/4, NULL);

	for (;;) {
		size_t n;

		off = 0;
		fp = fd_getfile(pap->fpfd);
		if (fp == NULL)
			error = EINVAL;
		else
			error = dofileread(pap->fpfd, fp, buf, BUFSIZE,
			    &off, 0, &rv);
		if (error) {
			if (error == ENOENT && inited == 0)
				goto retry;
			if (error == ENXIO)
				break;
			panic("fileread failed: %d", error);
		}
		inited = 1;

		while (rv) {
			struct rumpuser_iovec iov;

			iov.iov_base = buf;
			iov.iov_len = rv;

			error = rumpuser_iovwrite(pap->comfd, &iov, 1,
			    RUMPUSER_IOV_NOSEEK, &n);
			if (error)
				panic("fileread failed: %d", error);
			if (n == 0)
				panic("fileread failed: closed");
			rv -= n;
		}
	}

	kthread_exit(0);
}

/* Read requests from comfd and proxy them to /dev/puffs */
static void
writethread(void *arg)
{
	struct ptargs *pap = arg;
	struct file *fp;
	struct putter_hdr *phdr;
	register_t rv;
	char *buf;
	off_t off;
	size_t toread;
	int error;

	buf = kmem_alloc(BUFSIZE, KM_SLEEP);
	phdr = (struct putter_hdr *)buf;

	for (;;) {
		size_t n;

		/*
		 * Need to write everything to the "kernel" in one chunk,
		 * so make sure we have it here.
		 */
		off = 0;
		toread = sizeof(struct putter_hdr);
		do {
			struct rumpuser_iovec iov;

			iov.iov_base = buf+off;
			iov.iov_len = toread;
			error = rumpuser_iovread(pap->comfd, &iov, 1,
			    RUMPUSER_IOV_NOSEEK, &n);
			if (error)
				panic("rumpuser_read %zd %d", n, error);
			if (n == 0)
				goto out;
			off += n;
			if (off >= sizeof(struct putter_hdr))
				toread = phdr->pth_framelen - off;
			else
				toread = off - sizeof(struct putter_hdr);
		} while (toread);

		off = 0;
		rv = 0;
		fp = fd_getfile(pap->fpfd);
		if (fp == NULL)
			error = EINVAL;
		else
			error = dofilewrite(pap->fpfd, fp, buf,
			    phdr->pth_framelen, &off, 0, &rv);
		if (error == ENXIO)
			goto out;
		KASSERT(rv == phdr->pth_framelen);
	}
 out:

	kthread_exit(0);
}

int
rump_syspuffs_glueinit(int fd, int *newfd)
{
	struct ptargs *pap;
	int rv;

	if ((rv = rump_init()) != 0)
		return rv;

	putterattach();
	rv = puttercdopen(makedev(178, 0), 0, 0, curlwp);
	if (rv && rv != EMOVEFD)
		return rv;

	pap = kmem_alloc(sizeof(struct ptargs), KM_SLEEP);
	pap->comfd = fd;
	pap->fpfd = curlwp->l_dupfd;
	pap->fdp = curlwp->l_proc->p_fd;

	rv = kthread_create(PRI_NONE, 0, NULL, readthread, pap, NULL,
	    "rputter");
	if (rv)
		return rv;

	rv = kthread_create(PRI_NONE, 0, NULL, writethread, pap, NULL,
	    "wputter");
	if (rv)
		return rv;

	*newfd = curlwp->l_dupfd;
	return 0;
}
