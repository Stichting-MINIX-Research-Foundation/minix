/*	$NetBSD: kern_ktrace_vfs.c,v 1.2 2014/09/05 09:20:59 matt Exp $	*/

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
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_ktrace.c	8.5 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_ktrace_vfs.c,v 1.2 2014/09/05 09:20:59 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/ktrace.h>
#include <sys/kauth.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/*
 * ktrace system call, the part of the ktrace framework that
 * explicitly interacts with VFS
 */
/* ARGSUSED */
int
sys_ktrace(struct lwp *l, const struct sys_ktrace_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) fname;
		syscallarg(int) ops;
		syscallarg(int) facs;
		syscallarg(int) pid;
	} */
	struct vnode *vp = NULL;
	file_t *fp = NULL;
	struct pathbuf *pb;
	struct nameidata nd;
	int error = 0;
	int fd;

	if (ktrenter(l))
		return EAGAIN;

	if (KTROP(SCARG(uap, ops)) != KTROP_CLEAR) {
		/*
		 * an operation which requires a file argument.
		 */
		error = pathbuf_copyin(SCARG(uap, fname), &pb);
		if (error) {
			ktrexit(l);
			return (error);
		}
		NDINIT(&nd, LOOKUP, FOLLOW, pb);
		if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0) {
			pathbuf_destroy(pb);
			ktrexit(l);
			return (error);
		}
		vp = nd.ni_vp;
		pathbuf_destroy(pb);
		VOP_UNLOCK(vp);
		if (vp->v_type != VREG) {
			vn_close(vp, FREAD|FWRITE, l->l_cred);
			ktrexit(l);
			return (EACCES);
		}
		/*
		 * This uses up a file descriptor slot in the
		 * tracing process for the duration of this syscall.
		 * This is not expected to be a problem.
		 */
		if ((error = fd_allocfile(&fp, &fd)) != 0) {
			vn_close(vp, FWRITE, l->l_cred);
			ktrexit(l);
			return error;
		}
		fp->f_flag = FWRITE;
		fp->f_type = DTYPE_VNODE;
		fp->f_ops = &vnops;
		fp->f_vnode = vp;
		vp = NULL;
	}
	error = ktrace_common(l, SCARG(uap, ops), SCARG(uap, facs),
	    SCARG(uap, pid), &fp);
	if (KTROP(SCARG(uap, ops)) != KTROP_CLEAR)
		fd_abort(curproc, fp, fd);
	return (error);
}
