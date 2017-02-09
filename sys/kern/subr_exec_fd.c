/*	$NetBSD: subr_exec_fd.c,v 1.7 2014/09/05 09:20:59 matt Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: subr_exec_fd.c,v 1.7 2014/09/05 09:20:59 matt Exp $");

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/syslog.h>
#include <sys/vnode.h>
#include <sys/ktrace.h>

void
fd_ktrexecfd(void)
{
	proc_t *p;
	filedesc_t *fdp;
	fdfile_t *ff;
	lwp_t *l;
	fdtab_t *dt;
	int fd;

	l = curlwp;
	p = l->l_proc;
	fdp = p->p_fd;
	dt = fdp->fd_dt;

	for (fd = 0; fd <= fdp->fd_lastfile; fd++) {
		if ((ff = dt->dt_ff[fd]) == NULL) {
			KASSERT(fd >= NDFDFILE);
			continue;
		}
		KASSERT(fd >= NDFDFILE ||
		    ff == (fdfile_t *)fdp->fd_dfdfile[fd]);
		if (ff->ff_file == NULL)
			continue;
		ktr_execfd(fd, ff->ff_file->f_type);
	}
}

/*
 * It is unsafe for set[ug]id processes to be started with file
 * descriptors 0..2 closed, as these descriptors are given implicit
 * significance in the Standard C library.  fdcheckstd() will create a
 * descriptor referencing /dev/null for each of stdin, stdout, and
 * stderr that is not already open.
 */
#define CHECK_UPTO 3
int
fd_checkstd(void)
{
	struct proc *p;
	struct pathbuf *pb;
	struct nameidata nd;
	filedesc_t *fdp;
	file_t *fp;
	fdtab_t *dt;
	struct proc *pp;
	int fd, i, error, flags = FREAD|FWRITE;
	char closed[CHECK_UPTO * 3 + 1], which[3 + 1];

	p = curproc;
	closed[0] = '\0';
	if ((fdp = p->p_fd) == NULL)
		return (0);
	dt = fdp->fd_dt;
	for (i = 0; i < CHECK_UPTO; i++) {
		KASSERT(i >= NDFDFILE ||
		    dt->dt_ff[i] == (fdfile_t *)fdp->fd_dfdfile[i]);
		if (dt->dt_ff[i]->ff_file != NULL)
			continue;
		snprintf(which, sizeof(which), ",%d", i);
		strlcat(closed, which, sizeof(closed));
		if ((error = fd_allocfile(&fp, &fd)) != 0)
			return (error);
		KASSERT(fd < CHECK_UPTO);
		pb = pathbuf_create("/dev/null");
		if (pb == NULL) {
			return ENOMEM;
		}
		NDINIT(&nd, LOOKUP, FOLLOW, pb);
		if ((error = vn_open(&nd, flags, 0)) != 0) {
			pathbuf_destroy(pb);
			fd_abort(p, fp, fd);
			return (error);
		}
		fp->f_type = DTYPE_VNODE;
		fp->f_vnode = nd.ni_vp;
		fp->f_flag = flags;
		fp->f_ops = &vnops;
		VOP_UNLOCK(nd.ni_vp);
		fd_affix(p, fp, fd);
		pathbuf_destroy(pb);
	}
	if (closed[0] != '\0') {
		mutex_enter(proc_lock);
		pp = p->p_pptr;
		mutex_enter(pp->p_lock);
		log(LOG_WARNING, "set{u,g}id pid %d (%s) "
		    "was invoked by uid %d ppid %d (%s) "
		    "with fd %s closed\n",
		    p->p_pid, p->p_comm, kauth_cred_geteuid(pp->p_cred),
		    pp->p_pid, pp->p_comm, &closed[1]);
		mutex_exit(pp->p_lock);
		mutex_exit(proc_lock);
	}
	return (0);
}
#undef CHECK_UPTO
