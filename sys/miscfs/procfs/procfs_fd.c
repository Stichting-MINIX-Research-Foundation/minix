/*	$NetBSD: procfs_fd.c,v 1.14 2008/04/28 20:24:08 martin Exp $	*/

/*-
 * Copyright (c) 2003, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__KERNEL_RCSID(0, "$NetBSD: procfs_fd.c,v 1.14 2008/04/28 20:24:08 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/filedesc.h>

#include <miscfs/procfs/procfs.h>

int
procfs_dofd(lwp_t *curl, proc_t *p, struct pfsnode *pfs, struct uio *uio)
{
	int error;
	file_t *fp;
	off_t offs;

	if ((fp = fd_getfile2(p, pfs->pfs_fd)) == NULL)
		return EBADF;

	offs = fp->f_offset;

	switch (uio->uio_rw) {
	case UIO_READ:
		error = (*fp->f_ops->fo_read)(fp, &offs, uio, curl->l_cred, 0);
		break;
	case UIO_WRITE:
		error = (*fp->f_ops->fo_write)(fp, &offs, uio, curl->l_cred, 0);
		break;
	default:
		panic("bad uio op");
	}

	closef(fp);

	return (error);
}
