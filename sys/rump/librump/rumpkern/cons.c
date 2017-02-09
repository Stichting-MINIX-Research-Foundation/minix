/*	$NetBSD: cons.c,v 1.5 2015/05/26 15:29:39 pooka Exp $	*/

/*
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
 * rumpcons, a (very) simple console-type device which relays output
 * to the rumpuser_putchar() hypercall.  This is most useful in
 * environments where there is no Unix-like host (e.g. Xen DomU).
 * It's currently a truly half duplex console since there is support
 * only for writing to the console (there is no hypercall for reading
 * the host console).  The driver attempts to look like a tty just
 * enough to fool isatty().  Let's see how far that gets us.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cons.c,v 1.5 2015/05/26 15:29:39 pooka Exp $");

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/termios.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

static int rumpcons_write(struct file *, off_t *, struct uio *,
			  kauth_cred_t, int);
static int rumpcons_ioctl(struct file *, u_long, void *);
static int rumpcons_stat(struct file *, struct stat *);
static int rumpcons_poll(struct file *, int events);

static const struct fileops rumpcons_fileops = {
	.fo_read = (void *)nullop,
	.fo_write = rumpcons_write,
	.fo_ioctl = rumpcons_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = rumpcons_poll,
	.fo_stat = rumpcons_stat,
	.fo_close = (void *)nullop,
	.fo_kqfilter = fnullop_kqfilter,
	.fo_restart = fnullop_restart,
};

void
rump_consdev_init(void)
{
	struct file *fp;
	int fd, error;

	KASSERT(fd_getfile(0) == NULL);
	KASSERT(fd_getfile(1) == NULL);
	KASSERT(fd_getfile(2) == NULL);
	
	/* then, map a file descriptor to the device */
	if ((error = fd_allocfile(&fp, &fd)) != 0)
		panic("cons fd_allocfile failed: %d", error);

	fp->f_flag = FWRITE;
	fp->f_type = DTYPE_MISC;
	fp->f_ops = &rumpcons_fileops;
	fp->f_data = NULL;
	fd_affix(curproc, fp, fd);

	KASSERT(fd == 0);
	error += fd_dup2(fp, 1, 0);
	error += fd_dup2(fp, 2, 0);

	if (error)
		panic("failed to dup fd 0/1/2");
}

static int
rumpcons_write(struct file *fp, off_t *off, struct uio *uio,
	kauth_cred_t cred, int flags)
{
	char *buf;
	size_t len, n;
	int error = 0;

	buf = kmem_alloc(PAGE_SIZE, KM_SLEEP);
	while (uio->uio_resid > 0) {
		len = min(PAGE_SIZE, uio->uio_resid);
		error = uiomove(buf, len, uio);
		if (error)
			break;

		for (n = 0; n < len; n++) {
			rumpuser_putchar(*(buf+n));
		}
	}
	kmem_free(buf, PAGE_SIZE);

	return error;
}

static int
rumpcons_ioctl(struct file *fp, u_long cmd, void *data)
{

	if (cmd == TIOCGETA)
		return 0;

	return ENOTTY; /* considering how we are cheating, lol */
}

static int
rumpcons_stat(struct file *fp, struct stat *sb)
{

	memset(sb, 0, sizeof(*sb));
	sb->st_mode = 0600 | _S_IFCHR;
	sb->st_atimespec = sb->st_mtimespec = sb->st_ctimespec = boottime;
	sb->st_birthtimespec = boottime;

	return 0;
}

static int
rumpcons_poll(struct file *fp, int events)
{
	int revents = 0;

	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);

	return revents;
}
