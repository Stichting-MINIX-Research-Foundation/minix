/*	$NetBSD: kvm_proc.c,v 1.90 2014/02/19 20:21:22 dsl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
static char sccsid[] = "@(#)kvm_proc.c	8.3 (Berkeley) 9/23/93";
#else
__RCSID("$NetBSD: kvm_proc.c,v 1.90 2014/02/19 20:21:22 dsl Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * Proc traversal interface for kvm.  ps and w are (probably) the exclusive
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/resourcevar.h>
#include <sys/mutex.h>
#include <sys/specificdata.h>
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_param.h>
#include <uvm/uvm_amap.h>
#include <uvm/uvm_page.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <db.h>
#include <paths.h>

#include "kvm_private.h"

/*
 * Common info from kinfo_proc and kinfo_proc2 used by helper routines.
 */
struct miniproc {
	struct	vmspace *p_vmspace;
	char	p_stat;
	struct	proc *p_paddr;
	pid_t	p_pid;
};

/*
 * Convert from struct proc and kinfo_proc{,2} to miniproc.
 */
#define PTOMINI(kp, p) \
	do { \
		(p)->p_stat = (kp)->p_stat; \
		(p)->p_pid = (kp)->p_pid; \
		(p)->p_paddr = NULL; \
		(p)->p_vmspace = (kp)->p_vmspace; \
	} while (/*CONSTCOND*/0);

#define KPTOMINI(kp, p) \
	do { \
		(p)->p_stat = (kp)->kp_proc.p_stat; \
		(p)->p_pid = (kp)->kp_proc.p_pid; \
		(p)->p_paddr = (kp)->kp_eproc.e_paddr; \
		(p)->p_vmspace = (kp)->kp_proc.p_vmspace; \
	} while (/*CONSTCOND*/0);

#define KP2TOMINI(kp, p) \
	do { \
		(p)->p_stat = (kp)->p_stat; \
		(p)->p_pid = (kp)->p_pid; \
		(p)->p_paddr = (void *)(long)(kp)->p_paddr; \
		(p)->p_vmspace = (void *)(long)(kp)->p_vmspace; \
	} while (/*CONSTCOND*/0);

/*
 * NetBSD uses kauth(9) to manage credentials, which are stored in kauth_cred_t,
 * a kernel-only opaque type. This is an embedded version which is *INTERNAL* to
 * kvm(3) so dumps can be read properly.
 *
 * Whenever NetBSD starts exporting credentials to userland consistently (using
 * 'struct uucred', or something) this will have to be updated again.
 */
struct kvm_kauth_cred {
	u_int cr_refcnt;		/* reference count */
	uint8_t cr_pad[CACHE_LINE_SIZE - sizeof(u_int)];
	uid_t cr_uid;			/* user id */
	uid_t cr_euid;			/* effective user id */
	uid_t cr_svuid;			/* saved effective user id */
	gid_t cr_gid;			/* group id */
	gid_t cr_egid;			/* effective group id */
	gid_t cr_svgid;			/* saved effective group id */
	u_int cr_ngroups;		/* number of groups */
	gid_t cr_groups[NGROUPS];	/* group memberships */
	specificdata_reference cr_sd;	/* specific data */
};

/* XXX: What uses these two functions? */
char		*_kvm_uread(kvm_t *, const struct proc *, u_long, u_long *);
ssize_t		kvm_uread(kvm_t *, const struct proc *, u_long, char *,
		    size_t);

static char	*_kvm_ureadm(kvm_t *, const struct miniproc *, u_long,
		    u_long *);
static ssize_t	kvm_ureadm(kvm_t *, const struct miniproc *, u_long,
		    char *, size_t);

static char	**kvm_argv(kvm_t *, const struct miniproc *, u_long, int, int);
static int	kvm_deadprocs(kvm_t *, int, int, u_long, u_long, int);
static char	**kvm_doargv(kvm_t *, const struct miniproc *, int,
		    void (*)(struct ps_strings *, u_long *, int *));
static char	**kvm_doargv2(kvm_t *, pid_t, int, int);
static int	kvm_proclist(kvm_t *, int, int, struct proc *,
		    struct kinfo_proc *, int);
static int	proc_verify(kvm_t *, u_long, const struct miniproc *);
static void	ps_str_a(struct ps_strings *, u_long *, int *);
static void	ps_str_e(struct ps_strings *, u_long *, int *);


static char *
_kvm_ureadm(kvm_t *kd, const struct miniproc *p, u_long va, u_long *cnt)
{
	u_long addr, head;
	u_long offset;
	struct vm_map_entry vme;
	struct vm_amap amap;
	struct vm_anon *anonp, anon;
	struct vm_page pg;
	u_long slot;

	if (kd->swapspc == NULL) {
		kd->swapspc = _kvm_malloc(kd, (size_t)kd->nbpg);
		if (kd->swapspc == NULL)
			return (NULL);
	}

	/*
	 * Look through the address map for the memory object
	 * that corresponds to the given virtual address.
	 * The header just has the entire valid range.
	 */
	head = (u_long)&p->p_vmspace->vm_map.header;
	addr = head;
	for (;;) {
		if (KREAD(kd, addr, &vme))
			return (NULL);

		if (va >= vme.start && va < vme.end &&
		    vme.aref.ar_amap != NULL)
			break;

		addr = (u_long)vme.next;
		if (addr == head)
			return (NULL);
	}

	/*
	 * we found the map entry, now to find the object...
	 */
	if (vme.aref.ar_amap == NULL)
		return (NULL);

	addr = (u_long)vme.aref.ar_amap;
	if (KREAD(kd, addr, &amap))
		return (NULL);

	offset = va - vme.start;
	slot = offset / kd->nbpg + vme.aref.ar_pageoff;
	/* sanity-check slot number */
	if (slot > amap.am_nslot)
		return (NULL);

	addr = (u_long)amap.am_anon + (offset / kd->nbpg) * sizeof(anonp);
	if (KREAD(kd, addr, &anonp))
		return (NULL);

	addr = (u_long)anonp;
	if (KREAD(kd, addr, &anon))
		return (NULL);

	addr = (u_long)anon.an_page;
	if (addr) {
		if (KREAD(kd, addr, &pg))
			return (NULL);

		if (_kvm_pread(kd, kd->pmfd, kd->swapspc, (size_t)kd->nbpg,
		    (off_t)pg.phys_addr) != kd->nbpg)
			return (NULL);
	} else {
		if (kd->swfd < 0 ||
		    _kvm_pread(kd, kd->swfd, kd->swapspc, (size_t)kd->nbpg,
		    (off_t)(anon.an_swslot * kd->nbpg)) != kd->nbpg)
			return (NULL);
	}

	/* Found the page. */
	offset %= kd->nbpg;
	*cnt = kd->nbpg - offset;
	return (&kd->swapspc[(size_t)offset]);
}

char *
_kvm_uread(kvm_t *kd, const struct proc *p, u_long va, u_long *cnt)
{
	struct miniproc mp;

	PTOMINI(p, &mp);
	return (_kvm_ureadm(kd, &mp, va, cnt));
}

/*
 * Convert credentials located in kernel space address 'cred' and store
 * them in the appropriate members of 'eproc'.
 */
static int
_kvm_convertcred(kvm_t *kd, u_long cred, struct eproc *eproc)
{
	struct kvm_kauth_cred kauthcred;
	struct ki_pcred *pc = &eproc->e_pcred;
	struct ki_ucred *uc = &eproc->e_ucred;

	if (KREAD(kd, cred, &kauthcred) != 0)
		return (-1);

	/* inlined version of kauth_cred_to_pcred, see kauth(9). */
	pc->p_ruid = kauthcred.cr_uid;
	pc->p_svuid = kauthcred.cr_svuid;
	pc->p_rgid = kauthcred.cr_gid;
	pc->p_svgid = kauthcred.cr_svgid;
	pc->p_refcnt = kauthcred.cr_refcnt;
	pc->p_pad = NULL;

	/* inlined version of kauth_cred_to_ucred(), see kauth(9). */
	uc->cr_ref = kauthcred.cr_refcnt;
	uc->cr_uid = kauthcred.cr_euid;
	uc->cr_gid = kauthcred.cr_egid;
	uc->cr_ngroups = (uint32_t)MIN(kauthcred.cr_ngroups,
	    sizeof(uc->cr_groups) / sizeof(uc->cr_groups[0]));
	memcpy(uc->cr_groups, kauthcred.cr_groups,
	    uc->cr_ngroups * sizeof(uc->cr_groups[0]));

	return (0);
}

/*
 * Read proc's from memory file into buffer bp, which has space to hold
 * at most maxcnt procs.
 */
static int
kvm_proclist(kvm_t *kd, int what, int arg, struct proc *p,
	     struct kinfo_proc *bp, int maxcnt)
{
	int cnt = 0;
	int nlwps;
	struct kinfo_lwp *kl;
	struct eproc eproc;
	struct pgrp pgrp;
	struct session sess;
	struct tty tty;
	struct proc proc;

	for (; cnt < maxcnt && p != NULL; p = proc.p_list.le_next) {
		if (KREAD(kd, (u_long)p, &proc)) {
			_kvm_err(kd, kd->program, "can't read proc at %p", p);
			return (-1);
		}
		if (_kvm_convertcred(kd, (u_long)proc.p_cred, &eproc) != 0) {
			_kvm_err(kd, kd->program,
			    "can't read proc credentials at %p", p);
			return (-1);
		}

		switch (what) {

		case KERN_PROC_PID:
			if (proc.p_pid != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_UID:
			if (eproc.e_ucred.cr_uid != (uid_t)arg)
				continue;
			break;

		case KERN_PROC_RUID:
			if (eproc.e_pcred.p_ruid != (uid_t)arg)
				continue;
			break;
		}
		/*
		 * We're going to add another proc to the set.  If this
		 * will overflow the buffer, assume the reason is because
		 * nprocs (or the proc list) is corrupt and declare an error.
		 */
		if (cnt >= maxcnt) {
			_kvm_err(kd, kd->program, "nprocs corrupt");
			return (-1);
		}
		/*
		 * gather eproc
		 */
		eproc.e_paddr = p;
		if (KREAD(kd, (u_long)proc.p_pgrp, &pgrp)) {
			_kvm_err(kd, kd->program, "can't read pgrp at %p",
			    proc.p_pgrp);
			return (-1);
		}
		eproc.e_sess = pgrp.pg_session;
		eproc.e_pgid = pgrp.pg_id;
		eproc.e_jobc = pgrp.pg_jobc;
		if (KREAD(kd, (u_long)pgrp.pg_session, &sess)) {
			_kvm_err(kd, kd->program, "can't read session at %p",
			    pgrp.pg_session);
			return (-1);
		}
		if ((proc.p_lflag & PL_CONTROLT) && sess.s_ttyp != NULL) {
			if (KREAD(kd, (u_long)sess.s_ttyp, &tty)) {
				_kvm_err(kd, kd->program,
				    "can't read tty at %p", sess.s_ttyp);
				return (-1);
			}
			eproc.e_tdev = (uint32_t)tty.t_dev;
			eproc.e_tsess = tty.t_session;
			if (tty.t_pgrp != NULL) {
				if (KREAD(kd, (u_long)tty.t_pgrp, &pgrp)) {
					_kvm_err(kd, kd->program,
					    "can't read tpgrp at %p",
					    tty.t_pgrp);
					return (-1);
				}
				eproc.e_tpgid = pgrp.pg_id;
			} else
				eproc.e_tpgid = -1;
		} else
			eproc.e_tdev = (uint32_t)NODEV;
		eproc.e_flag = sess.s_ttyvp ? EPROC_CTTY : 0;
		eproc.e_sid = sess.s_sid;
		if (sess.s_leader == p)
			eproc.e_flag |= EPROC_SLEADER;
		/*
		 * Fill in the old-style proc.p_wmesg by copying the wmesg
		 * from the first available LWP.
		 */
		kl = kvm_getlwps(kd, proc.p_pid,
		    (u_long)PTRTOUINT64(eproc.e_paddr),
		    sizeof(struct kinfo_lwp), &nlwps);
		if (kl) {
			if (nlwps > 0) {
				strcpy(eproc.e_wmesg, kl[0].l_wmesg);
			}
		}
		(void)kvm_read(kd, (u_long)proc.p_vmspace, &eproc.e_vm,
		    sizeof(eproc.e_vm));

		eproc.e_xsize = eproc.e_xrssize = 0;
		eproc.e_xccount = eproc.e_xswrss = 0;

		switch (what) {

		case KERN_PROC_PGRP:
			if (eproc.e_pgid != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_TTY:
			if ((proc.p_lflag & PL_CONTROLT) == 0 ||
			    eproc.e_tdev != (dev_t)arg)
				continue;
			break;
		}
		memcpy(&bp->kp_proc, &proc, sizeof(proc));
		memcpy(&bp->kp_eproc, &eproc, sizeof(eproc));
		++bp;
		++cnt;
	}
	return (cnt);
}

/*
 * Build proc info array by reading in proc list from a crash dump.
 * Return number of procs read.  maxcnt is the max we will read.
 */
static int
kvm_deadprocs(kvm_t *kd, int what, int arg, u_long a_allproc,
	      u_long a_zombproc, int maxcnt)
{
	struct kinfo_proc *bp = kd->procbase;
	int acnt, zcnt;
	struct proc *p;

	if (KREAD(kd, a_allproc, &p)) {
		_kvm_err(kd, kd->program, "cannot read allproc");
		return (-1);
	}
	acnt = kvm_proclist(kd, what, arg, p, bp, maxcnt);
	if (acnt < 0)
		return (acnt);

	if (KREAD(kd, a_zombproc, &p)) {
		_kvm_err(kd, kd->program, "cannot read zombproc");
		return (-1);
	}
	zcnt = kvm_proclist(kd, what, arg, p, bp + acnt,
	    maxcnt - acnt);
	if (zcnt < 0)
		zcnt = 0;

	return (acnt + zcnt);
}

struct kinfo_proc2 *
kvm_getproc2(kvm_t *kd, int op, int arg, size_t esize, int *cnt)
{
	size_t size;
	int mib[6], st, nprocs;
	struct pstats pstats;

	if (ISSYSCTL(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC2;
		mib[2] = op;
		mib[3] = arg;
		mib[4] = (int)esize;
again:
		mib[5] = 0;
		st = sysctl(mib, 6, NULL, &size, NULL, (size_t)0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getproc2");
			return (NULL);
		}

		mib[5] = (int) (size / esize);
		KVM_ALLOC(kd, procbase2, size);
		st = sysctl(mib, 6, kd->procbase2, &size, NULL, (size_t)0);
		if (st == -1) {
			if (errno == ENOMEM) {
				goto again;
			}
			_kvm_syserr(kd, kd->program, "kvm_getproc2");
			return (NULL);
		}
		nprocs = (int) (size / esize);
	} else {
		char *kp2c;
		struct kinfo_proc *kp;
		struct kinfo_proc2 kp2, *kp2p;
		struct kinfo_lwp *kl;
		int i, nlwps;

		kp = kvm_getprocs(kd, op, arg, &nprocs);
		if (kp == NULL)
			return (NULL);

		size = nprocs * esize;
		KVM_ALLOC(kd, procbase2, size);
		kp2c = (char *)(void *)kd->procbase2;
		kp2p = &kp2;
		for (i = 0; i < nprocs; i++, kp++) {
			struct timeval tv;

			kl = kvm_getlwps(kd, kp->kp_proc.p_pid,
			    (u_long)PTRTOUINT64(kp->kp_eproc.e_paddr),
			    sizeof(struct kinfo_lwp), &nlwps);

			if (kl == NULL) {
				_kvm_syserr(kd, NULL,
					"kvm_getlwps() failed on process %u\n",
					kp->kp_proc.p_pid);
				if (nlwps == 0)
					return NULL;
				else
					continue;
			}

			/* We use kl[0] as the "representative" LWP */
			memset(kp2p, 0, sizeof(kp2));
			kp2p->p_forw = kl[0].l_forw;
			kp2p->p_back = kl[0].l_back;
			kp2p->p_paddr = PTRTOUINT64(kp->kp_eproc.e_paddr);
			kp2p->p_addr = kl[0].l_addr;
			kp2p->p_fd = PTRTOUINT64(kp->kp_proc.p_fd);
			kp2p->p_cwdi = PTRTOUINT64(kp->kp_proc.p_cwdi);
			kp2p->p_stats = PTRTOUINT64(kp->kp_proc.p_stats);
			kp2p->p_limit = PTRTOUINT64(kp->kp_proc.p_limit);
			kp2p->p_vmspace = PTRTOUINT64(kp->kp_proc.p_vmspace);
			kp2p->p_sigacts = PTRTOUINT64(kp->kp_proc.p_sigacts);
			kp2p->p_sess = PTRTOUINT64(kp->kp_eproc.e_sess);
			kp2p->p_tsess = 0;
#if 1 /* XXX: dsl - p_ru was only ever non-zero for zombies */
			kp2p->p_ru = 0;
#else
			kp2p->p_ru = PTRTOUINT64(pstats.p_ru);
#endif

			kp2p->p_eflag = 0;
			kp2p->p_exitsig = kp->kp_proc.p_exitsig;
			kp2p->p_flag = kp->kp_proc.p_flag;

			kp2p->p_pid = kp->kp_proc.p_pid;

			kp2p->p_ppid = kp->kp_eproc.e_ppid;
			kp2p->p_sid = kp->kp_eproc.e_sid;
			kp2p->p__pgid = kp->kp_eproc.e_pgid;

			kp2p->p_tpgid = -1 /* XXX NO_PGID! */;

			kp2p->p_uid = kp->kp_eproc.e_ucred.cr_uid;
			kp2p->p_ruid = kp->kp_eproc.e_pcred.p_ruid;
			kp2p->p_svuid = kp->kp_eproc.e_pcred.p_svuid;
			kp2p->p_gid = kp->kp_eproc.e_ucred.cr_gid;
			kp2p->p_rgid = kp->kp_eproc.e_pcred.p_rgid;
			kp2p->p_svgid = kp->kp_eproc.e_pcred.p_svgid;

			/*CONSTCOND*/
			memcpy(kp2p->p_groups, kp->kp_eproc.e_ucred.cr_groups,
			    MIN(sizeof(kp2p->p_groups),
			    sizeof(kp->kp_eproc.e_ucred.cr_groups)));
			kp2p->p_ngroups = kp->kp_eproc.e_ucred.cr_ngroups;

			kp2p->p_jobc = kp->kp_eproc.e_jobc;
			kp2p->p_tdev = kp->kp_eproc.e_tdev;
			kp2p->p_tpgid = kp->kp_eproc.e_tpgid;
			kp2p->p_tsess = PTRTOUINT64(kp->kp_eproc.e_tsess);

			kp2p->p_estcpu = 0;
			bintime2timeval(&kp->kp_proc.p_rtime, &tv);
			kp2p->p_rtime_sec = (uint32_t)tv.tv_sec;
			kp2p->p_rtime_usec = (uint32_t)tv.tv_usec;
			kp2p->p_cpticks = kl[0].l_cpticks;
			kp2p->p_pctcpu = kp->kp_proc.p_pctcpu;
			kp2p->p_swtime = kl[0].l_swtime;
			kp2p->p_slptime = kl[0].l_slptime;
#if 0 /* XXX thorpej */
			kp2p->p_schedflags = kp->kp_proc.p_schedflags;
#else
			kp2p->p_schedflags = 0;
#endif

			kp2p->p_uticks = kp->kp_proc.p_uticks;
			kp2p->p_sticks = kp->kp_proc.p_sticks;
			kp2p->p_iticks = kp->kp_proc.p_iticks;

			kp2p->p_tracep = PTRTOUINT64(kp->kp_proc.p_tracep);
			kp2p->p_traceflag = kp->kp_proc.p_traceflag;

			kp2p->p_holdcnt = kl[0].l_holdcnt;

			memcpy(&kp2p->p_siglist,
			    &kp->kp_proc.p_sigpend.sp_set,
			    sizeof(ki_sigset_t));
			memset(&kp2p->p_sigmask, 0,
			    sizeof(ki_sigset_t));
			memcpy(&kp2p->p_sigignore,
			    &kp->kp_proc.p_sigctx.ps_sigignore,
			    sizeof(ki_sigset_t));
			memcpy(&kp2p->p_sigcatch,
			    &kp->kp_proc.p_sigctx.ps_sigcatch,
			    sizeof(ki_sigset_t));

			kp2p->p_stat = kl[0].l_stat;
			kp2p->p_priority = kl[0].l_priority;
			kp2p->p_usrpri = kl[0].l_priority;
			kp2p->p_nice = kp->kp_proc.p_nice;

			kp2p->p_xstat = kp->kp_proc.p_xstat;
			kp2p->p_acflag = kp->kp_proc.p_acflag;

			/*CONSTCOND*/
			strncpy(kp2p->p_comm, kp->kp_proc.p_comm,
			    MIN(sizeof(kp2p->p_comm),
			    sizeof(kp->kp_proc.p_comm)));

			strncpy(kp2p->p_wmesg, kp->kp_eproc.e_wmesg,
			    sizeof(kp2p->p_wmesg));
			kp2p->p_wchan = kl[0].l_wchan;
			strncpy(kp2p->p_login, kp->kp_eproc.e_login,
			    sizeof(kp2p->p_login));

			kp2p->p_vm_rssize = kp->kp_eproc.e_xrssize;
			kp2p->p_vm_tsize = kp->kp_eproc.e_vm.vm_tsize;
			kp2p->p_vm_dsize = kp->kp_eproc.e_vm.vm_dsize;
			kp2p->p_vm_ssize = kp->kp_eproc.e_vm.vm_ssize;
			kp2p->p_vm_vsize = kp->kp_eproc.e_vm.vm_map.size
			    / kd->nbpg;
			/* Adjust mapped size */
			kp2p->p_vm_msize =
			    (kp->kp_eproc.e_vm.vm_map.size / kd->nbpg) -
			    kp->kp_eproc.e_vm.vm_issize +
			    kp->kp_eproc.e_vm.vm_ssize;

			kp2p->p_eflag = (int32_t)kp->kp_eproc.e_flag;

			kp2p->p_realflag = kp->kp_proc.p_flag;
			kp2p->p_nlwps = kp->kp_proc.p_nlwps;
			kp2p->p_nrlwps = kp->kp_proc.p_nrlwps;
			kp2p->p_realstat = kp->kp_proc.p_stat;

			if (P_ZOMBIE(&kp->kp_proc) ||
			    kp->kp_proc.p_stats == NULL ||
			    KREAD(kd, (u_long)kp->kp_proc.p_stats, &pstats)) {
				kp2p->p_uvalid = 0;
			} else {
				kp2p->p_uvalid = 1;

				kp2p->p_ustart_sec = (u_int32_t)
				    pstats.p_start.tv_sec;
				kp2p->p_ustart_usec = (u_int32_t)
				    pstats.p_start.tv_usec;

				kp2p->p_uutime_sec = (u_int32_t)
				    pstats.p_ru.ru_utime.tv_sec;
				kp2p->p_uutime_usec = (u_int32_t)
				    pstats.p_ru.ru_utime.tv_usec;
				kp2p->p_ustime_sec = (u_int32_t)
				    pstats.p_ru.ru_stime.tv_sec;
				kp2p->p_ustime_usec = (u_int32_t)
				    pstats.p_ru.ru_stime.tv_usec;

				kp2p->p_uru_maxrss = pstats.p_ru.ru_maxrss;
				kp2p->p_uru_ixrss = pstats.p_ru.ru_ixrss;
				kp2p->p_uru_idrss = pstats.p_ru.ru_idrss;
				kp2p->p_uru_isrss = pstats.p_ru.ru_isrss;
				kp2p->p_uru_minflt = pstats.p_ru.ru_minflt;
				kp2p->p_uru_majflt = pstats.p_ru.ru_majflt;
				kp2p->p_uru_nswap = pstats.p_ru.ru_nswap;
				kp2p->p_uru_inblock = pstats.p_ru.ru_inblock;
				kp2p->p_uru_oublock = pstats.p_ru.ru_oublock;
				kp2p->p_uru_msgsnd = pstats.p_ru.ru_msgsnd;
				kp2p->p_uru_msgrcv = pstats.p_ru.ru_msgrcv;
				kp2p->p_uru_nsignals = pstats.p_ru.ru_nsignals;
				kp2p->p_uru_nvcsw = pstats.p_ru.ru_nvcsw;
				kp2p->p_uru_nivcsw = pstats.p_ru.ru_nivcsw;

				kp2p->p_uctime_sec = (u_int32_t)
				    (pstats.p_cru.ru_utime.tv_sec +
				    pstats.p_cru.ru_stime.tv_sec);
				kp2p->p_uctime_usec = (u_int32_t)
				    (pstats.p_cru.ru_utime.tv_usec +
				    pstats.p_cru.ru_stime.tv_usec);
			}

			memcpy(kp2c, &kp2, esize);
			kp2c += esize;
		}
	}
	*cnt = nprocs;
	return (kd->procbase2);
}

struct kinfo_lwp *
kvm_getlwps(kvm_t *kd, int pid, u_long paddr, size_t esize, int *cnt)
{
	size_t size;
	int mib[5], nlwps;
	ssize_t st;
	struct kinfo_lwp *kl;

	if (ISSYSCTL(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_LWP;
		mib[2] = pid;
		mib[3] = (int)esize;
		mib[4] = 0;
again:
		st = sysctl(mib, 5, NULL, &size, NULL, (size_t)0);
		if (st == -1) {
			switch (errno) {
			case ESRCH: /* Treat this as a soft error; see kvm.c */
				_kvm_syserr(kd, NULL, "kvm_getlwps");
				return NULL;
			default:
				_kvm_syserr(kd, kd->program, "kvm_getlwps");
				return NULL;
			}
		}
		mib[4] = (int) (size / esize);
		KVM_ALLOC(kd, lwpbase, size);
		st = sysctl(mib, 5, kd->lwpbase, &size, NULL, (size_t)0);
		if (st == -1) {
			switch (errno) {
			case ESRCH: /* Treat this as a soft error; see kvm.c */
				_kvm_syserr(kd, NULL, "kvm_getlwps");
				return NULL;
			case ENOMEM:
				goto again;
			default:
				_kvm_syserr(kd, kd->program, "kvm_getlwps");
				return NULL;
			}
		}
		nlwps = (int) (size / esize);
	} else {
		/* grovel through the memory image */
		struct proc p;
		struct lwp l;
		u_long laddr;
		void *back;
		int i;

		st = kvm_read(kd, paddr, &p, sizeof(p));
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getlwps");
			return (NULL);
		}

		nlwps = p.p_nlwps;
		size = nlwps * sizeof(*kd->lwpbase);
		KVM_ALLOC(kd, lwpbase, size);
		laddr = (u_long)PTRTOUINT64(p.p_lwps.lh_first);
		for (i = 0; (i < nlwps) && (laddr != 0); i++) {
			st = kvm_read(kd, laddr, &l, sizeof(l));
			if (st == -1) {
				_kvm_syserr(kd, kd->program, "kvm_getlwps");
				return (NULL);
			}
			kl = &kd->lwpbase[i];
			kl->l_laddr = laddr;
			kl->l_forw = PTRTOUINT64(l.l_runq.tqe_next);
			laddr = (u_long)PTRTOUINT64(l.l_runq.tqe_prev);
			st = kvm_read(kd, laddr, &back, sizeof(back));
			if (st == -1) {
				_kvm_syserr(kd, kd->program, "kvm_getlwps");
				return (NULL);
			}
			kl->l_back = PTRTOUINT64(back);
			kl->l_addr = PTRTOUINT64(l.l_addr);
			kl->l_lid = l.l_lid;
			kl->l_flag = l.l_flag;
			kl->l_swtime = l.l_swtime;
			kl->l_slptime = l.l_slptime;
			kl->l_schedflags = 0; /* XXX */
			kl->l_holdcnt = 0;
			kl->l_priority = l.l_priority;
			kl->l_usrpri = l.l_priority;
			kl->l_stat = l.l_stat;
			kl->l_wchan = PTRTOUINT64(l.l_wchan);
			if (l.l_wmesg)
				(void)kvm_read(kd, (u_long)l.l_wmesg,
				    kl->l_wmesg, (size_t)WMESGLEN);
			kl->l_cpuid = KI_NOCPU;
			laddr = (u_long)PTRTOUINT64(l.l_sibling.le_next);
		}
	}

	*cnt = nlwps;
	return (kd->lwpbase);
}

struct kinfo_proc *
kvm_getprocs(kvm_t *kd, int op, int arg, int *cnt)
{
	size_t size;
	int mib[4], st, nprocs;

	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = op;
		mib[3] = arg;
		st = sysctl(mib, 4, NULL, &size, NULL, (size_t)0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (NULL);
		}
		KVM_ALLOC(kd, procbase, size);
		st = sysctl(mib, 4, kd->procbase, &size, NULL, (size_t)0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (NULL);
		}
		if (size % sizeof(struct kinfo_proc) != 0) {
			_kvm_err(kd, kd->program,
			    "proc size mismatch (%lu total, %lu chunks)",
			    (u_long)size, (u_long)sizeof(struct kinfo_proc));
			return (NULL);
		}
		nprocs = (int) (size / sizeof(struct kinfo_proc));
	} else {
		struct nlist nl[4], *p;

		(void)memset(nl, 0, sizeof(nl));
		nl[0].n_name = "_nprocs";
		nl[1].n_name = "_allproc";
		nl[2].n_name = "_zombproc";
		nl[3].n_name = NULL;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				continue;
			_kvm_err(kd, kd->program,
			    "%s: no such symbol", p->n_name);
			return (NULL);
		}
		if (KREAD(kd, nl[0].n_value, &nprocs)) {
			_kvm_err(kd, kd->program, "can't read nprocs");
			return (NULL);
		}
		size = nprocs * sizeof(*kd->procbase);
		KVM_ALLOC(kd, procbase, size);
		nprocs = kvm_deadprocs(kd, op, arg, nl[1].n_value,
		    nl[2].n_value, nprocs);
		if (nprocs < 0)
			return (NULL);
#ifdef notdef
		size = nprocs * sizeof(struct kinfo_proc);
		(void)realloc(kd->procbase, size);
#endif
	}
	*cnt = nprocs;
	return (kd->procbase);
}

void *
_kvm_realloc(kvm_t *kd, void *p, size_t n)
{
	void *np = realloc(p, n);

	if (np == NULL)
		_kvm_err(kd, kd->program, "out of memory");
	return (np);
}

/*
 * Read in an argument vector from the user address space of process p.
 * addr if the user-space base address of narg null-terminated contiguous
 * strings.  This is used to read in both the command arguments and
 * environment strings.  Read at most maxcnt characters of strings.
 */
static char **
kvm_argv(kvm_t *kd, const struct miniproc *p, u_long addr, int narg,
	 int maxcnt)
{
	char *np, *cp, *ep, *ap;
	u_long oaddr = (u_long)~0L;
	u_long len;
	size_t cc;
	char **argv;

	/*
	 * Check that there aren't an unreasonable number of arguments,
	 * and that the address is in user space.
	 */
	if (narg > ARG_MAX || addr < kd->min_uva || addr >= kd->max_uva)
		return (NULL);

	if (kd->argv == NULL) {
		/*
		 * Try to avoid reallocs.
		 */
		kd->argc = MAX(narg + 1, 32);
		kd->argv = _kvm_malloc(kd, kd->argc * sizeof(*kd->argv));
		if (kd->argv == NULL)
			return (NULL);
	} else if (narg + 1 > kd->argc) {
		kd->argc = MAX(2 * kd->argc, narg + 1);
		kd->argv = _kvm_realloc(kd, kd->argv, kd->argc *
		    sizeof(*kd->argv));
		if (kd->argv == NULL)
			return (NULL);
	}
	if (kd->argspc == NULL) {
		kd->argspc = _kvm_malloc(kd, (size_t)kd->nbpg);
		if (kd->argspc == NULL)
			return (NULL);
		kd->argspc_len = kd->nbpg;
	}
	if (kd->argbuf == NULL) {
		kd->argbuf = _kvm_malloc(kd, (size_t)kd->nbpg);
		if (kd->argbuf == NULL)
			return (NULL);
	}
	cc = sizeof(char *) * narg;
	if (kvm_ureadm(kd, p, addr, (void *)kd->argv, cc) != cc)
		return (NULL);
	ap = np = kd->argspc;
	argv = kd->argv;
	len = 0;
	/*
	 * Loop over pages, filling in the argument vector.
	 */
	while (argv < kd->argv + narg && *argv != NULL) {
		addr = (u_long)*argv & ~(kd->nbpg - 1);
		if (addr != oaddr) {
			if (kvm_ureadm(kd, p, addr, kd->argbuf,
			    (size_t)kd->nbpg) != kd->nbpg)
				return (NULL);
			oaddr = addr;
		}
		addr = (u_long)*argv & (kd->nbpg - 1);
		cp = kd->argbuf + (size_t)addr;
		cc = kd->nbpg - (size_t)addr;
		if (maxcnt > 0 && cc > (size_t)(maxcnt - len))
			cc = (size_t)(maxcnt - len);
		ep = memchr(cp, '\0', cc);
		if (ep != NULL)
			cc = ep - cp + 1;
		if (len + cc > kd->argspc_len) {
			ptrdiff_t off;
			char **pp;
			char *op = kd->argspc;

			kd->argspc_len *= 2;
			kd->argspc = _kvm_realloc(kd, kd->argspc,
			    kd->argspc_len);
			if (kd->argspc == NULL)
				return (NULL);
			/*
			 * Adjust argv pointers in case realloc moved
			 * the string space.
			 */
			off = kd->argspc - op;
			for (pp = kd->argv; pp < argv; pp++)
				*pp += off;
			ap += off;
			np += off;
		}
		memcpy(np, cp, cc);
		np += cc;
		len += cc;
		if (ep != NULL) {
			*argv++ = ap;
			ap = np;
		} else
			*argv += cc;
		if (maxcnt > 0 && len >= maxcnt) {
			/*
			 * We're stopping prematurely.  Terminate the
			 * current string.
			 */
			if (ep == NULL) {
				*np = '\0';
				*argv++ = ap;
			}
			break;
		}
	}
	/* Make sure argv is terminated. */
	*argv = NULL;
	return (kd->argv);
}

static void
ps_str_a(struct ps_strings *p, u_long *addr, int *n)
{

	*addr = (u_long)p->ps_argvstr;
	*n = p->ps_nargvstr;
}

static void
ps_str_e(struct ps_strings *p, u_long *addr, int *n)
{

	*addr = (u_long)p->ps_envstr;
	*n = p->ps_nenvstr;
}

/*
 * Determine if the proc indicated by p is still active.
 * This test is not 100% foolproof in theory, but chances of
 * being wrong are very low.
 */
static int
proc_verify(kvm_t *kd, u_long kernp, const struct miniproc *p)
{
	struct proc kernproc;

	/*
	 * Just read in the whole proc.  It's not that big relative
	 * to the cost of the read system call.
	 */
	if (kvm_read(kd, kernp, &kernproc, sizeof(kernproc)) !=
	    sizeof(kernproc))
		return (0);
	return (p->p_pid == kernproc.p_pid &&
	    (kernproc.p_stat != SZOMB || p->p_stat == SZOMB));
}

static char **
kvm_doargv(kvm_t *kd, const struct miniproc *p, int nchr,
	   void (*info)(struct ps_strings *, u_long *, int *))
{
	char **ap;
	u_long addr;
	int cnt;
	struct ps_strings arginfo;

	/*
	 * Pointers are stored at the top of the user stack.
	 */
	if (p->p_stat == SZOMB)
		return (NULL);
	cnt = (int)kvm_ureadm(kd, p, kd->usrstack - sizeof(arginfo),
	    (void *)&arginfo, sizeof(arginfo));
	if (cnt != sizeof(arginfo))
		return (NULL);

	(*info)(&arginfo, &addr, &cnt);
	if (cnt == 0)
		return (NULL);
	ap = kvm_argv(kd, p, addr, cnt, nchr);
	/*
	 * For live kernels, make sure this process didn't go away.
	 */
	if (ap != NULL && ISALIVE(kd) &&
	    !proc_verify(kd, (u_long)p->p_paddr, p))
		ap = NULL;
	return (ap);
}

/*
 * Get the command args.  This code is now machine independent.
 */
char **
kvm_getargv(kvm_t *kd, const struct kinfo_proc *kp, int nchr)
{
	struct miniproc p;

	KPTOMINI(kp, &p);
	return (kvm_doargv(kd, &p, nchr, ps_str_a));
}

char **
kvm_getenvv(kvm_t *kd, const struct kinfo_proc *kp, int nchr)
{
	struct miniproc p;

	KPTOMINI(kp, &p);
	return (kvm_doargv(kd, &p, nchr, ps_str_e));
}

static char **
kvm_doargv2(kvm_t *kd, pid_t pid, int type, int nchr)
{
	size_t bufs;
	int narg, mib[4];
	size_t newargspc_len;
	char **ap, *bp, *endp;

	/*
	 * Check that there aren't an unreasonable number of arguments.
	 */
	if (nchr > ARG_MAX)
		return (NULL);

	if (nchr == 0)
		nchr = ARG_MAX;

	/* Get number of strings in argv */
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = pid;
	mib[3] = type == KERN_PROC_ARGV ? KERN_PROC_NARGV : KERN_PROC_NENV;
	bufs = sizeof(narg);
	if (sysctl(mib, 4, &narg, &bufs, NULL, (size_t)0) == -1)
		return (NULL);

	if (kd->argv == NULL) {
		/*
		 * Try to avoid reallocs.
		 */
		kd->argc = MAX(narg + 1, 32);
		kd->argv = _kvm_malloc(kd, kd->argc * sizeof(*kd->argv));
		if (kd->argv == NULL)
			return (NULL);
	} else if (narg + 1 > kd->argc) {
		kd->argc = MAX(2 * kd->argc, narg + 1);
		kd->argv = _kvm_realloc(kd, kd->argv, kd->argc *
		    sizeof(*kd->argv));
		if (kd->argv == NULL)
			return (NULL);
	}

	newargspc_len = MIN(nchr, ARG_MAX);
	KVM_ALLOC(kd, argspc, newargspc_len);
	memset(kd->argspc, 0, (size_t)kd->argspc_len);	/* XXX necessary? */

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = pid;
	mib[3] = type;
	bufs = kd->argspc_len;
	if (sysctl(mib, 4, kd->argspc, &bufs, NULL, (size_t)0) == -1)
		return (NULL);

	bp = kd->argspc;
	bp[kd->argspc_len-1] = '\0';	/* make sure the string ends with nul */
	ap = kd->argv;
	endp = bp + MIN(nchr, bufs);

	while (bp < endp) {
		*ap++ = bp;
		/*
		 * XXX: don't need following anymore, or stick check
		 * for max argc in above while loop?
		 */
		if (ap >= kd->argv + kd->argc) {
			kd->argc *= 2;
			kd->argv = _kvm_realloc(kd, kd->argv,
			    kd->argc * sizeof(*kd->argv));
			ap = kd->argv;
		}
		bp += strlen(bp) + 1;
	}
	*ap = NULL;

	return (kd->argv);
}

char **
kvm_getargv2(kvm_t *kd, const struct kinfo_proc2 *kp, int nchr)
{

	return (kvm_doargv2(kd, kp->p_pid, KERN_PROC_ARGV, nchr));
}

char **
kvm_getenvv2(kvm_t *kd, const struct kinfo_proc2 *kp, int nchr)
{

	return (kvm_doargv2(kd, kp->p_pid, KERN_PROC_ENV, nchr));
}

/*
 * Read from user space.  The user context is given by p.
 */
static ssize_t
kvm_ureadm(kvm_t *kd, const struct miniproc *p, u_long uva,
	   char *buf, size_t len)
{
	char *cp;

	cp = buf;
	while (len > 0) {
		size_t cc;
		char *dp;
		u_long cnt;

		dp = _kvm_ureadm(kd, p, uva, &cnt);
		if (dp == NULL) {
			_kvm_err(kd, 0, "invalid address (%lx)", uva);
			return (0);
		}
		cc = (size_t)MIN(cnt, len);
		memcpy(cp, dp, cc);
		cp += cc;
		uva += cc;
		len -= cc;
	}
	return (ssize_t)(cp - buf);
}

ssize_t
kvm_uread(kvm_t *kd, const struct proc *p, u_long uva, char *buf, size_t len)
{
	struct miniproc mp;

	PTOMINI(p, &mp);
	return (kvm_ureadm(kd, &mp, uva, buf, len));
}
