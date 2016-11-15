/*	$NetBSD: procfs_cmdline.c,v 1.28 2011/03/04 22:25:32 joerg Exp $	*/

/*
 * Copyright (c) 1999 Jaromir Dolecek <dolecek@ics.muni.cz>
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
__KERNEL_RCSID(0, "$NetBSD: procfs_cmdline.c,v 1.28 2011/03/04 22:25:32 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslimits.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <miscfs/procfs/procfs.h>

#include <uvm/uvm_extern.h>

static int
procfs_docmdline_helper(void *cookie, const void *src, size_t off, size_t len)
{
	struct uio *uio = cookie;
	char *buf = __UNCONST(src);

	buf += uio->uio_offset - off;
	if (off + len <= uio->uio_offset)
		return 0;
	return uiomove(buf, off + len - uio->uio_offset, cookie);
}

/*
 * code for returning process's command line arguments
 */
int
procfs_docmdline(
    struct lwp *curl,
    struct proc *p,
    struct pfsnode *pfs,
    struct uio *uio
)
{
	size_t len, start;
	int error;

	/* Don't allow writing. */
	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	/*
	 * Zombies don't have a stack, so we can't read their psstrings.
	 * System processes also don't have a user stack.  This is what
	 * ps(1) would display.
	 */
	if (P_ZOMBIE(p) || (p->p_flag & PK_SYSTEM) != 0) {
		static char msg[] = "()";
		error = 0;
		if (0 == uio->uio_offset) {
			error = uiomove(msg, 1, uio);
			if (error)
				return (error);
		}
		len = strlen(p->p_comm);
		if (len >= uio->uio_offset) {
			start = uio->uio_offset - 1;
			error = uiomove(p->p_comm + start, len - start, uio);
			if (error)
				return (error);
		}
		if (len + 2 >= uio->uio_offset) {
			start = uio->uio_offset - 1 - len;
			error = uiomove(msg + 1 + start, 2 - start, uio);
		}
		return (error);
	}

	len = uio->uio_offset + uio->uio_resid;

	error = copy_procargs(p, KERN_PROC_ARGV, &len,
	    procfs_docmdline_helper, uio);
	return error;
}
