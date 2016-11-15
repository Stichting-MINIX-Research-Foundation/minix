/*	$NetBSD: procfs_subr.c,v 1.106 2014/11/10 18:46:34 maxv Exp $	*/

/*-
 * Copyright (c) 2006, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_subr.c	8.6 (Berkeley) 5/14/95
 */

/*
 * Copyright (c) 1994 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)procfs_subr.c	8.6 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_subr.c,v 1.106 2014/11/10 18:46:34 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>

#include <miscfs/procfs/procfs.h>

/*
 * Allocate a pfsnode/vnode pair.  The vnode is referenced.
 * The pid, type, and file descriptor uniquely identify a pfsnode.
 */
int
procfs_allocvp(struct mount *mp, struct vnode **vpp, pid_t pid,
    pfstype type, int fd)
{
	struct pfskey key;

	memset(&key, 0, sizeof(key));
	key.pk_type = type;
	key.pk_pid = pid;
	key.pk_fd = fd;

	return vcache_get(mp, &key, sizeof(key), vpp);
}

int
procfs_rw(void *v)
{
	struct vop_read_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct lwp *curl;
	struct lwp *l;
	struct pfsnode *pfs = VTOPFS(vp);
	struct proc *p;
	int error;

	if (uio->uio_offset < 0)
		return EINVAL;

	if ((error = procfs_proc_lock(pfs->pfs_pid, &p, ESRCH)) != 0)
		return error;

	curl = curlwp;

	/*
	 * Do not allow init to be modified while in secure mode; it
	 * could be duped into changing the security level.
	 */
#define	M2K(m)	((m) == UIO_READ ? KAUTH_REQ_PROCESS_PROCFS_READ : \
		 KAUTH_REQ_PROCESS_PROCFS_WRITE)
	mutex_enter(p->p_lock);
	error = kauth_authorize_process(curl->l_cred, KAUTH_PROCESS_PROCFS,
	    p, pfs, KAUTH_ARG(M2K(uio->uio_rw)), NULL);
	mutex_exit(p->p_lock);
	if (error) {
		procfs_proc_unlock(p);
		return (error);
	}
#undef	M2K

	mutex_enter(p->p_lock);
	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		if (l->l_stat != LSZOMB)
			break;
	}
	/* Process is exiting if no-LWPS or all LWPs are LSZOMB */
	if (l == NULL) {
		mutex_exit(p->p_lock);
		procfs_proc_unlock(p);
		return ESRCH;
	}

	lwp_addref(l);
	mutex_exit(p->p_lock);

	switch (pfs->pfs_type) {
	case PFSnote:
	case PFSnotepg:
		error = procfs_donote(curl, p, pfs, uio);
		break;

	case PFSregs:
		error = procfs_doregs(curl, l, pfs, uio);
		break;

	case PFSfpregs:
		error = procfs_dofpregs(curl, l, pfs, uio);
		break;

	case PFSctl:
		error = procfs_doctl(curl, l, pfs, uio);
		break;

	case PFSstatus:
		error = procfs_dostatus(curl, l, pfs, uio);
		break;

	case PFSstat:
		error = procfs_do_pid_stat(curl, l, pfs, uio);
		break;

	case PFSmap:
		error = procfs_domap(curl, p, pfs, uio, 0);
		break;

	case PFSmaps:
		error = procfs_domap(curl, p, pfs, uio, 1);
		break;

	case PFSmem:
		error = procfs_domem(curl, l, pfs, uio);
		break;

	case PFScmdline:
		error = procfs_docmdline(curl, p, pfs, uio);
		break;

	case PFSmeminfo:
		error = procfs_domeminfo(curl, p, pfs, uio);
		break;

	case PFSdevices:
		error = procfs_dodevices(curl, p, pfs, uio);
		break;

	case PFScpuinfo:
		error = procfs_docpuinfo(curl, p, pfs, uio);
		break;

	case PFScpustat:
		error = procfs_docpustat(curl, p, pfs, uio);
		break;

	case PFSloadavg:
		error = procfs_doloadavg(curl, p, pfs, uio);
		break;

	case PFSstatm:
		error = procfs_do_pid_statm(curl, l, pfs, uio);
		break;

	case PFSfd:
		error = procfs_dofd(curl, p, pfs, uio);
		break;

	case PFSuptime:
		error = procfs_douptime(curl, p, pfs, uio);
		break;

	case PFSmounts:
		error = procfs_domounts(curl, p, pfs, uio);
		break;

	case PFSemul:
		error = procfs_doemul(curl, p, pfs, uio);
		break;

	case PFSversion:
		error = procfs_doversion(curl, p, pfs, uio);
		break;

#ifdef __HAVE_PROCFS_MACHDEP
	PROCFS_MACHDEP_NODETYPE_CASES
		error = procfs_machdep_rw(curl, l, pfs, uio);
		break;
#endif

	default:
		error = EOPNOTSUPP;
		break;
	}

	/*
	 * Release the references that we acquired earlier.
	 */
	lwp_delref(l);
	procfs_proc_unlock(p);

	return (error);
}

/*
 * Get a string from userland into (bf).  Strip a trailing
 * nl character (to allow easy access from the shell).
 * The buffer should be *buflenp + 1 chars long.  vfs_getuserstr
 * will automatically add a nul char at the end.
 *
 * Returns 0 on success or the following errors
 *
 * EINVAL:    file offset is non-zero.
 * EMSGSIZE:  message is longer than kernel buffer
 * EFAULT:    user i/o buffer is not addressable
 */
int
vfs_getuserstr(struct uio *uio, char *bf, int *buflenp)
{
	int xlen;
	int error;

	if (uio->uio_offset != 0)
		return (EINVAL);

	xlen = *buflenp;

	/* must be able to read the whole string in one go */
	if (xlen < uio->uio_resid)
		return (EMSGSIZE);
	xlen = uio->uio_resid;

	if ((error = uiomove(bf, xlen, uio)) != 0)
		return (error);

	/* allow multiple writes without seeks */
	uio->uio_offset = 0;

	/* cleanup string and remove trailing newline */
	bf[xlen] = '\0';
	xlen = strlen(bf);
	if (xlen > 0 && bf[xlen-1] == '\n')
		bf[--xlen] = '\0';
	*buflenp = xlen;

	return (0);
}

const vfs_namemap_t *
vfs_findname(const vfs_namemap_t *nm, const char *bf, int buflen)
{

	for (; nm->nm_name; nm++)
		if (memcmp(bf, nm->nm_name, buflen+1) == 0)
			return (nm);

	return (0);
}

static bool
procfs_revoke_selector(void *arg, struct vnode *vp)
{
	struct proc *p = arg;
	struct pfsnode *pfs = VTOPFS(vp);

	return (pfs != NULL && pfs->pfs_pid == p->p_pid);
}

void
procfs_revoke_vnodes(struct proc *p, void *arg)
{
	struct vnode *vp;
	struct vnode_iterator *marker;
	struct mount *mp = (struct mount *)arg;

	if (!(p->p_flag & PK_SUGID))
		return;

	vfs_vnode_iterator_init(mp, &marker);

	while ((vp = vfs_vnode_iterator_next(marker,
	    procfs_revoke_selector, p)) != NULL) {
		VOP_REVOKE(vp, REVOKEALL);
		vrele(vp);
	}

	vfs_vnode_iterator_destroy(marker);
}

int
procfs_proc_lock(int pid, struct proc **bunghole, int notfound)
{
	struct proc *tp;
	int error = 0;

	mutex_enter(proc_lock);

	if (pid == 0)
		tp = &proc0;
	else if ((tp = proc_find(pid)) == NULL)
		error = notfound;
	if (tp != NULL && !rw_tryenter(&tp->p_reflock, RW_READER))
		error = EBUSY;

	mutex_exit(proc_lock);

	*bunghole = tp;
	return error;
}

void
procfs_proc_unlock(struct proc *p)
{

	rw_exit(&p->p_reflock);
}

int
procfs_doemul(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	const char *ename = p->p_emul->e_name;
	return uiomove_frombuf(__UNCONST(ename), strlen(ename), uio);
}
