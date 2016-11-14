/*	$NetBSD: kern_core.c,v 1.23 2014/04/22 19:01:47 maxv Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_sig.c	8.14 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_core.c,v 1.23 2014/04/22 19:01:47 maxv Exp $");

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/acct.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/module.h>

MODULE(MODULE_CLASS_MISC, coredump, NULL);

struct coredump_iostate {
	struct lwp *io_lwp;
	struct vnode *io_vp;
	kauth_cred_t io_cred;
	off_t io_offset;
};

static int	coredump(struct lwp *, const char *);
static int	coredump_buildname(struct proc *, char *, const char *, size_t);

static int
coredump_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		coredump_vec = coredump;
		return 0;
	case MODULE_CMD_FINI:
		/*
		 * In theory we don't need to patch this, as the various
		 * exec formats depend on this module.  If this module has
		 * no references, and so can be unloaded, no user programs
		 * can be running and so nothing can call *coredump_vec.
		 */
		coredump_vec = (int (*)(struct lwp *, const char *))enosys;
		return 0;
	default:
		return ENOTTY;
	}
}

/*
 * Dump core, into a file named "progname.core" or "core" (depending on the
 * value of shortcorename), unless the process was setuid/setgid.
 */
static int
coredump(struct lwp *l, const char *pattern)
{
	struct vnode		*vp;
	struct proc		*p;
	struct vmspace		*vm;
	kauth_cred_t		cred;
	struct pathbuf		*pb;
	struct nameidata	nd;
	struct vattr		vattr;
	struct coredump_iostate	io;
	struct plimit		*lim;
	int			error, error1;
	char			*name, *lastslash;

	name = PNBUF_GET();

	p = l->l_proc;
	vm = p->p_vmspace;

	mutex_enter(proc_lock);		/* p_session */
	mutex_enter(p->p_lock);

	/*
	 * Refuse to core if the data + stack + user size is larger than
	 * the core dump limit.  XXX THIS IS WRONG, because of mapped
	 * data.
	 */
	if (USPACE + ctob(vm->vm_dsize + vm->vm_ssize) >=
	    p->p_rlimit[RLIMIT_CORE].rlim_cur) {
		error = EFBIG;		/* better error code? */
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);
		goto done;
	}

	/*
	 * It may well not be curproc, so grab a reference to its current
	 * credentials.
	 */
	kauth_cred_hold(p->p_cred);
	cred = p->p_cred;

	/*
	 * Make sure the process has not set-id, to prevent data leaks,
	 * unless it was specifically requested to allow set-id coredumps.
	 */
	if (p->p_flag & PK_SUGID) {
		if (!security_setidcore_dump) {
			error = EPERM;
			mutex_exit(p->p_lock);
			mutex_exit(proc_lock);
			goto done;
		}
		pattern = security_setidcore_path;
	}

	/* Lock, as p_limit and pl_corename might change. */
	lim = p->p_limit;
	mutex_enter(&lim->pl_lock);
	if (pattern == NULL) {
		pattern = lim->pl_corename;
	}
	error = coredump_buildname(p, name, pattern, MAXPATHLEN);
	mutex_exit(&lim->pl_lock);

	if (error) {
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);
		goto done;
	}

	/*
	 * On a simple filename, see if the filesystem allow us to write
	 * core dumps there.
	 */
	lastslash = strrchr(name, '/');
	if (!lastslash) {
		vp = p->p_cwdi->cwdi_cdir;
		if (vp->v_mount == NULL ||
		    (vp->v_mount->mnt_flag & MNT_NOCOREDUMP) != 0)
			error = EPERM;
	}

	mutex_exit(p->p_lock);
	mutex_exit(proc_lock);
	if (error)
		goto done;

	/*
	 * On a complex filename, see if the filesystem allow us to write
	 * core dumps there.
	 *
	 * XXX: We should have an API that avoids double lookups
	 */
	if (lastslash) {
		char c[2];

		if (lastslash - name >= MAXPATHLEN - 2) {
			error = EPERM;
			goto done;
		}

		c[0] = lastslash[1];
		c[1] = lastslash[2];
		lastslash[1] = '.';
		lastslash[2] = '\0';
		error = namei_simple_kernel(name, NSM_FOLLOW_NOEMULROOT, &vp);
		if (error)
			goto done;
		if (vp->v_mount == NULL ||
		    (vp->v_mount->mnt_flag & MNT_NOCOREDUMP) != 0)
			error = EPERM;
		vrele(vp);
		if (error)
			goto done;
		lastslash[1] = c[0];
		lastslash[2] = c[1];
	}

	pb = pathbuf_create(name);
	if (pb == NULL) {
		error = ENOMEM;
		goto done;
	}
	NDINIT(&nd, LOOKUP, NOFOLLOW, pb);
	if ((error = vn_open(&nd, O_CREAT | O_NOFOLLOW | FWRITE,
	    S_IRUSR | S_IWUSR)) != 0) {
		pathbuf_destroy(pb);
		goto done;
	}
	vp = nd.ni_vp;
	pathbuf_destroy(pb);

	/*
	 * Don't dump to:
	 * 	- non-regular files
	 * 	- files with links
	 * 	- files we don't own
	 */
	if (vp->v_type != VREG ||
	    VOP_GETATTR(vp, &vattr, cred) || vattr.va_nlink != 1 ||
	    vattr.va_uid != kauth_cred_geteuid(cred)) {
		error = EACCES;
		goto out;
	}
	vattr_null(&vattr);
	vattr.va_size = 0;

	if ((p->p_flag & PK_SUGID) && security_setidcore_dump) {
		vattr.va_uid = security_setidcore_owner;
		vattr.va_gid = security_setidcore_group;
		vattr.va_mode = security_setidcore_mode;
	}

	VOP_SETATTR(vp, &vattr, cred);
	p->p_acflag |= ACORE;

	io.io_lwp = l;
	io.io_vp = vp;
	io.io_cred = cred;
	io.io_offset = 0;

	/* Now dump the actual core file. */
	error = (*p->p_execsw->es_coredump)(l, &io);
 out:
	VOP_UNLOCK(vp);
	error1 = vn_close(vp, FWRITE, cred);
	if (error == 0)
		error = error1;
done:
	if (name != NULL)
		PNBUF_PUT(name);
	return error;
}

static int
coredump_buildname(struct proc *p, char *dst, const char *src, size_t len)
{
	const char	*s;
	char		*d, *end;
	int		i;

	KASSERT(mutex_owned(proc_lock));

	for (s = src, d = dst, end = d + len; *s != '\0'; s++) {
		if (*s == '%') {
			switch (*(s + 1)) {
			case 'n':
				i = snprintf(d, end - d, "%s", p->p_comm);
				break;
			case 'p':
				i = snprintf(d, end - d, "%d", p->p_pid);
				break;
			case 'u':
				i = snprintf(d, end - d, "%.*s",
				    (int)sizeof p->p_pgrp->pg_session->s_login,
				    p->p_pgrp->pg_session->s_login);
				break;
			case 't':
				i = snprintf(d, end - d, "%lld",
				    (long long)p->p_stats->p_start.tv_sec);
				break;
			default:
				goto copy;
			}
			d += i;
			s++;
		} else {
 copy:			*d = *s;
			d++;
		}
		if (d >= end)
			return (ENAMETOOLONG);
	}
	*d = '\0';
	return 0;
}

int
coredump_write(struct coredump_iostate *io, enum uio_seg segflg,
    const void *data, size_t len)
{
	int error;

	error = vn_rdwr(UIO_WRITE, io->io_vp, __UNCONST(data), len,
	    io->io_offset, segflg,
	    IO_NODELOCKED|IO_UNIT, io->io_cred, NULL,
	    segflg == UIO_USERSPACE ? io->io_lwp : NULL);
	if (error) {
		printf("pid %d (%s): %s write of %zu@%p at %lld failed: %d\n",
		    io->io_lwp->l_proc->p_pid, io->io_lwp->l_proc->p_comm,
		    segflg == UIO_USERSPACE ? "user" : "system",
		    len, data, (long long) io->io_offset, error);
		return (error);
	}

	io->io_offset += len;
	return (0);
}

off_t
coredump_offset(struct coredump_iostate *io)
{
	return io->io_offset; 
}
