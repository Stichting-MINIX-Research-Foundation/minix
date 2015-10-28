/*	$NetBSD: kvm_file.c,v 1.29 2014/02/19 20:21:22 dsl Exp $	*/

/*-
 * Copyright (c) 1989, 1992, 1993
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_file.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: kvm_file.c,v 1.29 2014/02/19 20:21:22 dsl Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * File list interface for kvm.  pstat, fstat and netstat are
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#define _KERNEL
#include <sys/types.h>
#undef _KERNEL
#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/exec.h>
#define _KERNEL
#include <sys/file.h>
#undef _KERNEL
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <ndbm.h>
#include <paths.h>
#include <string.h>

#include "kvm_private.h"

static int
kvm_deadfiles(kvm_t *, int, int, long, int);

/*
 * Get file structures.
 */
/*ARGSUSED*/
static int
kvm_deadfiles(kvm_t *kd, int op, int arg, long ofhead, int numfiles)
{
	size_t buflen = kd->argspc_len, n = 0;
	struct file *fp;
	struct filelist fhead;
	char *where = kd->argspc;

	/*
	 * first copyout filehead
	 */
	if (buflen < sizeof(fhead) ||
	    KREAD(kd, (u_long)ofhead, &fhead)) {
		_kvm_err(kd, kd->program, "can't read filehead");
		return (0);
	}
	buflen -= sizeof(fhead);
	where += sizeof(fhead);
	(void)memcpy(kd->argspc, &fhead, sizeof(fhead));

	/*
	 * followed by an array of file structures
	 */
	for (fp = fhead.lh_first; fp != 0; fp = fp->f_list.le_next) {
		if (buflen > sizeof(struct file)) {
			if (KREAD(kd, (u_long)fp,
			    ((struct file *)(void *)where))) {
				_kvm_err(kd, kd->program, "can't read kfp");
				return (0);
			}
			buflen -= sizeof(struct file);
			fp = (struct file *)(void *)where;
			where += sizeof(struct file);
			n++;
		}
	}
	if (n != numfiles) {
		_kvm_err(kd, kd->program, "inconsistent nfiles");
		return (0);
	}
	return (numfiles);
}

char *
kvm_getfiles(kvm_t *kd, int op, int arg, int *cnt)
{
	size_t size;
	int mib[2], st;
	int numfiles;
	struct file *fp, *fplim;
	struct filelist fhead;

	if (ISSYSCTL(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_FILE;
		st = sysctl(mib, 2, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		KVM_ALLOC(kd, argspc, size);
		st = sysctl(mib, 2, kd->argspc, &size, NULL, 0);
		if (st == -1 || size < sizeof(fhead)) {
			_kvm_syserr(kd, kd->program, "kvm_getfiles");
			return (0);
		}
		(void)memcpy(&fhead, kd->argspc, sizeof(fhead));
		fp = (struct file *)(void *)(kd->argspc + sizeof(fhead));
		fplim = (struct file *)(void *)(kd->argspc + size);
		for (numfiles = 0; fhead.lh_first && (fp < fplim);
		    numfiles++, fp++)
			fhead.lh_first = fp->f_list.le_next;
	} else {
		struct nlist nl[3], *p;

		nl[0].n_name = "_nfiles";
		nl[1].n_name = "_filehead";
		nl[2].n_name = 0;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				;
			_kvm_err(kd, kd->program,
				 "%s: no such symbol", p->n_name);
			return (0);
		}
		if (KREAD(kd, nl[0].n_value, &numfiles)) {
			_kvm_err(kd, kd->program, "can't read numfiles");
			return (0);
		}
		size = sizeof(fhead) + (numfiles + 10) * sizeof(struct file);
		KVM_ALLOC(kd, argspc, size);
		numfiles = kvm_deadfiles(kd, op, arg, (long)nl[1].n_value,
		    numfiles);
		if (numfiles == 0)
			return (0);
	}
	*cnt = numfiles;
	return (kd->argspc);
}
