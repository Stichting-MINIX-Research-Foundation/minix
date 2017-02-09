/*	$NetBSD: tty_bsdpty.c,v 1.20 2014/04/04 18:11:58 christos Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: tty_bsdpty.c,v 1.20 2014/04/04 18:11:58 christos Exp $");

#include "opt_ptm.h"

#ifndef NO_DEV_PTM
#ifdef COMPAT_BSDPTY
/* bsd tty implementation for pty multiplexor driver /dev/ptm{,x} */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/lwp.h>
#include <sys/tty.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/signalvar.h>
#include <sys/filedesc.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/pty.h>
#include <sys/kauth.h>

/*
 * pts == /dev/tty[pqrs]?
 * ptc == /dev/pty[pqrs]?
 */

/*
 * All this hard-coding is really evil.
 */
#define TTY_GID		4
#define TTY_PERM	(S_IRUSR|S_IWUSR|S_IWGRP)
#define TTY_TEMPLATE	"/dev/XtyXX"
#define TTY_NAMESIZE	sizeof(TTY_TEMPLATE)
#define TTY_LETTERS	"pqrstuvwxyzPQRST"
#define TTY_OLD_SUFFIX  "0123456789abcdef"
#define TTY_NEW_SUFFIX  "ghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

static int pty_makename(struct mount *, struct lwp *, char *, size_t, dev_t,
    char);
static int pty_allocvp(struct mount *, struct lwp *, struct vnode **,
    dev_t, char);
static void pty_getvattr(struct mount *, struct lwp *, struct vattr *);
static int pty__getmp(struct lwp *, struct mount **);

struct ptm_pty ptm_bsdpty = {
	pty_allocvp,
	pty_makename,
	pty_getvattr,
	pty__getmp,
};

static int
/*ARGSUSED*/
pty_makename(struct mount *mp, struct lwp *l, char *bf,
    size_t bufsiz, dev_t dev, char c)
{
	size_t nt;
	dev_t minor = minor(dev);
	const char *suffix;

	if (bufsiz < TTY_NAMESIZE)
		return EINVAL;

	(void)memcpy(bf, TTY_TEMPLATE, TTY_NAMESIZE);

	if (minor < 256) {
		suffix = TTY_OLD_SUFFIX;
		nt = sizeof(TTY_OLD_SUFFIX) - 1;
	} else {
		minor -= 256;
		suffix = TTY_NEW_SUFFIX;
		nt = sizeof(TTY_NEW_SUFFIX) - 1;
	}

	bf[5] = c;
	bf[8] = TTY_LETTERS[minor / nt];
	bf[9] = suffix[minor % nt];
	return 0;
}


static int
/*ARGSUSED*/
pty_allocvp(struct mount *mp, struct lwp *l, struct vnode **vp, dev_t dev,
    char ms)
{
	int error;
	struct pathbuf *pb;
	struct nameidata nd;
	char name[TTY_NAMESIZE];

	error = pty_makename(NULL, l, name, sizeof(name), dev, ms);
	if (error)
		return error;

	pb = pathbuf_create(name);
	if (pb == NULL) {
		return ENOMEM;
	}

	NDINIT(&nd, LOOKUP, NOFOLLOW|LOCKLEAF, pb);
	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	*vp = nd.ni_vp;
	pathbuf_destroy(pb);
	return 0;
}


static void
/*ARGSUSED*/
pty_getvattr(struct mount *mp, struct lwp *l, struct vattr *vattr)
{
	vattr_null(vattr);
	/* get real uid */
	vattr->va_uid = kauth_cred_getuid(l->l_cred);
	vattr->va_gid = TTY_GID;
	vattr->va_mode = TTY_PERM;
}

static int
pty__getmp(struct lwp *l __unused, struct mount **mpp)
{
	*mpp = 0;
	return 0;
}

#endif /* COMPAT_BSDPTY */
#endif /* NO_DEV_PTM */
