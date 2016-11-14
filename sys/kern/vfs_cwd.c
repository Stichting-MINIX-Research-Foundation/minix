/*	$NetBSD: vfs_cwd.c,v 1.4 2011/02/15 15:54:28 pooka Exp $	*/

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

/*
 * Current working directory.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_cwd.c,v 1.4 2011/02/15 15:54:28 pooka Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/vnode.h>

static int	cwdi_ctor(void *, void *, int);
static void	cwdi_dtor(void *, void *);

static pool_cache_t cwdi_cache;

void
cwd_sys_init(void)
{

	cwdi_cache = pool_cache_init(sizeof(struct cwdinfo), coherency_unit,
	    0, 0, "cwdi", NULL, IPL_NONE, cwdi_ctor, cwdi_dtor, NULL);
	KASSERT(cwdi_cache != NULL);
}

/*
 * Create an initial cwdinfo structure, using the same current and root
 * directories as curproc.
 */
struct cwdinfo *
cwdinit(void)
{
	struct cwdinfo *cwdi;
	struct cwdinfo *copy;

	cwdi = pool_cache_get(cwdi_cache, PR_WAITOK);
	copy = curproc->p_cwdi;

	rw_enter(&copy->cwdi_lock, RW_READER);
	cwdi->cwdi_cdir = copy->cwdi_cdir;
	if (cwdi->cwdi_cdir)
		vref(cwdi->cwdi_cdir);
	cwdi->cwdi_rdir = copy->cwdi_rdir;
	if (cwdi->cwdi_rdir)
		vref(cwdi->cwdi_rdir);
	cwdi->cwdi_edir = copy->cwdi_edir;
	if (cwdi->cwdi_edir)
		vref(cwdi->cwdi_edir);
	cwdi->cwdi_cmask = copy->cwdi_cmask;
	cwdi->cwdi_refcnt = 1;
	rw_exit(&copy->cwdi_lock);

	return (cwdi);
}

static int
cwdi_ctor(void *arg, void *obj, int flags)
{
	struct cwdinfo *cwdi = obj;

	rw_init(&cwdi->cwdi_lock);

	return 0;
}

static void
cwdi_dtor(void *arg, void *obj)
{
	struct cwdinfo *cwdi = obj;

	rw_destroy(&cwdi->cwdi_lock);
}

/*
 * Make p2 share p1's cwdinfo.
 */
void
cwdshare(struct proc *p2)
{
	struct cwdinfo *cwdi;

	cwdi = curproc->p_cwdi;

	atomic_inc_uint(&cwdi->cwdi_refcnt);
	p2->p_cwdi = cwdi;
}

/*
 * Make sure proc has only one reference to its cwdi, creating
 * a new one if necessary.
 */
void
cwdunshare(struct proc *p)
{
	struct cwdinfo *cwdi = p->p_cwdi;

	if (cwdi->cwdi_refcnt > 1) {
		cwdi = cwdinit();
		cwdfree(p->p_cwdi);
		p->p_cwdi = cwdi;
	}
}

/*
 * Release a cwdinfo structure.
 */
void
cwdfree(struct cwdinfo *cwdi)
{

	if (atomic_dec_uint_nv(&cwdi->cwdi_refcnt) > 0)
		return;

	vrele(cwdi->cwdi_cdir);
	if (cwdi->cwdi_rdir)
		vrele(cwdi->cwdi_rdir);
	if (cwdi->cwdi_edir)
		vrele(cwdi->cwdi_edir);
	pool_cache_put(cwdi_cache, cwdi);
}

void
cwdexec(struct proc *p)
{

	cwdunshare(p);

	if (p->p_cwdi->cwdi_edir) {
		vrele(p->p_cwdi->cwdi_edir);
	}
}
