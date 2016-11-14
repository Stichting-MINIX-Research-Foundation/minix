/*	$NetBSD: kern_resource.c,v 1.174 2014/10/18 08:33:29 snj Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)kern_resource.c	8.8 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_resource.c,v 1.174 2014/10/18 08:33:29 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/resourcevar.h>
#include <sys/kmem.h>
#include <sys/namei.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/timevar.h>
#include <sys/kauth.h>
#include <sys/atomic.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/atomic.h>

#include <uvm/uvm_extern.h>

/*
 * Maximum process data and stack limits.
 * They are variables so they are patchable.
 */
rlim_t			maxdmap = MAXDSIZ;
rlim_t			maxsmap = MAXSSIZ;

static pool_cache_t	plimit_cache	__read_mostly;
static pool_cache_t	pstats_cache	__read_mostly;

static kauth_listener_t	resource_listener;
static struct sysctllog	*proc_sysctllog;

static int	donice(struct lwp *, struct proc *, int);
static void	sysctl_proc_setup(void);

static int
resource_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	struct proc *p;
	int result;

	result = KAUTH_RESULT_DEFER;
	p = arg0;

	switch (action) {
	case KAUTH_PROCESS_NICE:
		if (kauth_cred_geteuid(cred) != kauth_cred_geteuid(p->p_cred) &&
		    kauth_cred_getuid(cred) != kauth_cred_geteuid(p->p_cred)) {
			break;
		}

		if ((u_long)arg1 >= p->p_nice)
			result = KAUTH_RESULT_ALLOW;

		break;

	case KAUTH_PROCESS_RLIMIT: {
		enum kauth_process_req req;

		req = (enum kauth_process_req)(unsigned long)arg1;

		switch (req) {
		case KAUTH_REQ_PROCESS_RLIMIT_GET:
			result = KAUTH_RESULT_ALLOW;
			break;

		case KAUTH_REQ_PROCESS_RLIMIT_SET: {
			struct rlimit *new_rlimit;
			u_long which;

			if ((p != curlwp->l_proc) &&
			    (proc_uidmatch(cred, p->p_cred) != 0))
				break;

			new_rlimit = arg2;
			which = (u_long)arg3;

			if (new_rlimit->rlim_max <= p->p_rlimit[which].rlim_max)
				result = KAUTH_RESULT_ALLOW;

			break;
			}

		default:
			break;
		}

		break;
	}

	default:
		break;
	}

	return result;
}

void
resource_init(void)
{

	plimit_cache = pool_cache_init(sizeof(struct plimit), 0, 0, 0,
	    "plimitpl", NULL, IPL_NONE, NULL, NULL, NULL);
	pstats_cache = pool_cache_init(sizeof(struct pstats), 0, 0, 0,
	    "pstatspl", NULL, IPL_NONE, NULL, NULL, NULL);

	resource_listener = kauth_listen_scope(KAUTH_SCOPE_PROCESS,
	    resource_listener_cb, NULL);

	sysctl_proc_setup();
}

/*
 * Resource controls and accounting.
 */

int
sys_getpriority(struct lwp *l, const struct sys_getpriority_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) which;
		syscallarg(id_t) who;
	} */
	struct proc *curp = l->l_proc, *p;
	id_t who = SCARG(uap, who);
	int low = NZERO + PRIO_MAX + 1;

	mutex_enter(proc_lock);
	switch (SCARG(uap, which)) {
	case PRIO_PROCESS:
		p = who ? proc_find(who) : curp;
		if (p != NULL)
			low = p->p_nice;
		break;

	case PRIO_PGRP: {
		struct pgrp *pg;

		if (who == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgrp_find(who)) == NULL)
			break;
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			if (p->p_nice < low)
				low = p->p_nice;
		}
		break;
	}

	case PRIO_USER:
		if (who == 0)
			who = (int)kauth_cred_geteuid(l->l_cred);
		PROCLIST_FOREACH(p, &allproc) {
			mutex_enter(p->p_lock);
			if (kauth_cred_geteuid(p->p_cred) ==
			    (uid_t)who && p->p_nice < low)
				low = p->p_nice;
			mutex_exit(p->p_lock);
		}
		break;

	default:
		mutex_exit(proc_lock);
		return EINVAL;
	}
	mutex_exit(proc_lock);

	if (low == NZERO + PRIO_MAX + 1) {
		return ESRCH;
	}
	*retval = low - NZERO;
	return 0;
}

int
sys_setpriority(struct lwp *l, const struct sys_setpriority_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) which;
		syscallarg(id_t) who;
		syscallarg(int) prio;
	} */
	struct proc *curp = l->l_proc, *p;
	id_t who = SCARG(uap, who);
	int found = 0, error = 0;

	mutex_enter(proc_lock);
	switch (SCARG(uap, which)) {
	case PRIO_PROCESS:
		p = who ? proc_find(who) : curp;
		if (p != NULL) {
			mutex_enter(p->p_lock);
			found++;
			error = donice(l, p, SCARG(uap, prio));
			mutex_exit(p->p_lock);
		}
		break;

	case PRIO_PGRP: {
		struct pgrp *pg;

		if (who == 0)
			pg = curp->p_pgrp;
		else if ((pg = pgrp_find(who)) == NULL)
			break;
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			mutex_enter(p->p_lock);
			found++;
			error = donice(l, p, SCARG(uap, prio));
			mutex_exit(p->p_lock);
			if (error)
				break;
		}
		break;
	}

	case PRIO_USER:
		if (who == 0)
			who = (int)kauth_cred_geteuid(l->l_cred);
		PROCLIST_FOREACH(p, &allproc) {
			mutex_enter(p->p_lock);
			if (kauth_cred_geteuid(p->p_cred) ==
			    (uid_t)SCARG(uap, who)) {
				found++;
				error = donice(l, p, SCARG(uap, prio));
			}
			mutex_exit(p->p_lock);
			if (error)
				break;
		}
		break;

	default:
		mutex_exit(proc_lock);
		return EINVAL;
	}
	mutex_exit(proc_lock);

	return (found == 0) ? ESRCH : error;
}

/*
 * Renice a process.
 *
 * Call with the target process' credentials locked.
 */
static int
donice(struct lwp *l, struct proc *chgp, int n)
{
	kauth_cred_t cred = l->l_cred;

	KASSERT(mutex_owned(chgp->p_lock));

	if (kauth_cred_geteuid(cred) && kauth_cred_getuid(cred) &&
	    kauth_cred_geteuid(cred) != kauth_cred_geteuid(chgp->p_cred) &&
	    kauth_cred_getuid(cred) != kauth_cred_geteuid(chgp->p_cred))
		return EPERM;

	if (n > PRIO_MAX) {
		n = PRIO_MAX;
	}
	if (n < PRIO_MIN) {
		n = PRIO_MIN;
	}
	n += NZERO;

	if (kauth_authorize_process(cred, KAUTH_PROCESS_NICE, chgp,
	    KAUTH_ARG(n), NULL, NULL)) {
		return EACCES;
	}

	sched_nice(chgp, n);
	return 0;
}

int
sys_setrlimit(struct lwp *l, const struct sys_setrlimit_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) which;
		syscallarg(const struct rlimit *) rlp;
	} */
	int error, which = SCARG(uap, which);
	struct rlimit alim;

	error = copyin(SCARG(uap, rlp), &alim, sizeof(struct rlimit));
	if (error) {
		return error;
	}
	return dosetrlimit(l, l->l_proc, which, &alim);
}

int
dosetrlimit(struct lwp *l, struct proc *p, int which, struct rlimit *limp)
{
	struct rlimit *alimp;
	int error;

	if ((u_int)which >= RLIM_NLIMITS)
		return EINVAL;

	if (limp->rlim_cur > limp->rlim_max) {
		/*
		 * This is programming error. According to SUSv2, we should
		 * return error in this case.
		 */
		return EINVAL;
	}

	alimp = &p->p_rlimit[which];
	/* if we don't change the value, no need to limcopy() */
	if (limp->rlim_cur == alimp->rlim_cur &&
	    limp->rlim_max == alimp->rlim_max)
		return 0;

	error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_RLIMIT,
	    p, KAUTH_ARG(KAUTH_REQ_PROCESS_RLIMIT_SET), limp, KAUTH_ARG(which));
	if (error)
		return error;

	lim_privatise(p);
	/* p->p_limit is now unchangeable */
	alimp = &p->p_rlimit[which];

	switch (which) {

	case RLIMIT_DATA:
		if (limp->rlim_cur > maxdmap)
			limp->rlim_cur = maxdmap;
		if (limp->rlim_max > maxdmap)
			limp->rlim_max = maxdmap;
		break;

	case RLIMIT_STACK:
		if (limp->rlim_cur > maxsmap)
			limp->rlim_cur = maxsmap;
		if (limp->rlim_max > maxsmap)
			limp->rlim_max = maxsmap;

		/*
		 * Return EINVAL if the new stack size limit is lower than
		 * current usage. Otherwise, the process would get SIGSEGV the
		 * moment it would try to access anything on its current stack.
		 * This conforms to SUSv2.
		 */
		if (limp->rlim_cur < p->p_vmspace->vm_ssize * PAGE_SIZE ||
		    limp->rlim_max < p->p_vmspace->vm_ssize * PAGE_SIZE) {
			return EINVAL;
		}

		/*
		 * Stack is allocated to the max at exec time with
		 * only "rlim_cur" bytes accessible (In other words,
		 * allocates stack dividing two contiguous regions at
		 * "rlim_cur" bytes boundary).
		 *
		 * Since allocation is done in terms of page, roundup
		 * "rlim_cur" (otherwise, contiguous regions
		 * overlap).  If stack limit is going up make more
		 * accessible, if going down make inaccessible.
		 */
		limp->rlim_cur = round_page(limp->rlim_cur);
		if (limp->rlim_cur != alimp->rlim_cur) {
			vaddr_t addr;
			vsize_t size;
			vm_prot_t prot;
			char *base, *tmp;

			base = p->p_vmspace->vm_minsaddr;
			if (limp->rlim_cur > alimp->rlim_cur) {
				prot = VM_PROT_READ | VM_PROT_WRITE;
				size = limp->rlim_cur - alimp->rlim_cur;
				tmp = STACK_GROW(base, alimp->rlim_cur);
			} else {
				prot = VM_PROT_NONE;
				size = alimp->rlim_cur - limp->rlim_cur;
				tmp = STACK_GROW(base, limp->rlim_cur);
			}
			addr = (vaddr_t)STACK_ALLOC(tmp, size);
			(void) uvm_map_protect(&p->p_vmspace->vm_map,
			    addr, addr + size, prot, false);
		}
		break;

	case RLIMIT_NOFILE:
		if (limp->rlim_cur > maxfiles)
			limp->rlim_cur = maxfiles;
		if (limp->rlim_max > maxfiles)
			limp->rlim_max = maxfiles;
		break;

	case RLIMIT_NPROC:
		if (limp->rlim_cur > maxproc)
			limp->rlim_cur = maxproc;
		if (limp->rlim_max > maxproc)
			limp->rlim_max = maxproc;
		break;

	case RLIMIT_NTHR:
		if (limp->rlim_cur > maxlwp)
			limp->rlim_cur = maxlwp;
		if (limp->rlim_max > maxlwp)
			limp->rlim_max = maxlwp;
		break;
	}

	mutex_enter(&p->p_limit->pl_lock);
	*alimp = *limp;
	mutex_exit(&p->p_limit->pl_lock);
	return 0;
}

int
sys_getrlimit(struct lwp *l, const struct sys_getrlimit_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) which;
		syscallarg(struct rlimit *) rlp;
	} */
	struct proc *p = l->l_proc;
	int which = SCARG(uap, which);
	struct rlimit rl;

	if ((u_int)which >= RLIM_NLIMITS)
		return EINVAL;

	mutex_enter(p->p_lock);
	memcpy(&rl, &p->p_rlimit[which], sizeof(rl));
	mutex_exit(p->p_lock);

	return copyout(&rl, SCARG(uap, rlp), sizeof(rl));
}

/*
 * Transform the running time and tick information in proc p into user,
 * system, and interrupt time usage.
 *
 * Should be called with p->p_lock held unless called from exit1().
 */
void
calcru(struct proc *p, struct timeval *up, struct timeval *sp,
    struct timeval *ip, struct timeval *rp)
{
	uint64_t u, st, ut, it, tot;
	struct lwp *l;
	struct bintime tm;
	struct timeval tv;

	KASSERT(p->p_stat == SDEAD || mutex_owned(p->p_lock));

	mutex_spin_enter(&p->p_stmutex);
	st = p->p_sticks;
	ut = p->p_uticks;
	it = p->p_iticks;
	mutex_spin_exit(&p->p_stmutex);

	tm = p->p_rtime;

	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		lwp_lock(l);
		bintime_add(&tm, &l->l_rtime);
		if ((l->l_pflag & LP_RUNNING) != 0) {
			struct bintime diff;
			/*
			 * Adjust for the current time slice.  This is
			 * actually fairly important since the error
			 * here is on the order of a time quantum,
			 * which is much greater than the sampling
			 * error.
			 */
			binuptime(&diff);
			bintime_sub(&diff, &l->l_stime);
			bintime_add(&tm, &diff);
		}
		lwp_unlock(l);
	}

	tot = st + ut + it;
	bintime2timeval(&tm, &tv);
	u = (uint64_t)tv.tv_sec * 1000000ul + tv.tv_usec;

	if (tot == 0) {
		/* No ticks, so can't use to share time out, split 50-50 */
		st = ut = u / 2;
	} else {
		st = (u * st) / tot;
		ut = (u * ut) / tot;
	}
	if (sp != NULL) {
		sp->tv_sec = st / 1000000;
		sp->tv_usec = st % 1000000;
	}
	if (up != NULL) {
		up->tv_sec = ut / 1000000;
		up->tv_usec = ut % 1000000;
	}
	if (ip != NULL) {
		if (it != 0)
			it = (u * it) / tot;
		ip->tv_sec = it / 1000000;
		ip->tv_usec = it % 1000000;
	}
	if (rp != NULL) {
		*rp = tv;
	}
}

int
sys___getrusage50(struct lwp *l, const struct sys___getrusage50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) who;
		syscallarg(struct rusage *) rusage;
	} */
	int error;
	struct rusage ru;
	struct proc *p = l->l_proc;

	error = getrusage1(p, SCARG(uap, who), &ru);
	if (error != 0)
		return error;

	return copyout(&ru, SCARG(uap, rusage), sizeof(ru));
}

int
getrusage1(struct proc *p, int who, struct rusage *ru) {

	switch (who) {
	case RUSAGE_SELF:
		mutex_enter(p->p_lock);
		memcpy(ru, &p->p_stats->p_ru, sizeof(*ru));
		calcru(p, &ru->ru_utime, &ru->ru_stime, NULL, NULL);
		rulwps(p, ru);
		mutex_exit(p->p_lock);
		break;
	case RUSAGE_CHILDREN:
		mutex_enter(p->p_lock);
		memcpy(ru, &p->p_stats->p_cru, sizeof(*ru));
		mutex_exit(p->p_lock);
		break;
	default:
		return EINVAL;
	}

	return 0;
}

void
ruadd(struct rusage *ru, struct rusage *ru2)
{
	long *ip, *ip2;
	int i;

	timeradd(&ru->ru_utime, &ru2->ru_utime, &ru->ru_utime);
	timeradd(&ru->ru_stime, &ru2->ru_stime, &ru->ru_stime);
	if (ru->ru_maxrss < ru2->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	ip = &ru->ru_first; ip2 = &ru2->ru_first;
	for (i = &ru->ru_last - &ru->ru_first; i >= 0; i--)
		*ip++ += *ip2++;
}

void
rulwps(proc_t *p, struct rusage *ru)
{
	lwp_t *l;

	KASSERT(mutex_owned(p->p_lock));

	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		ruadd(ru, &l->l_ru);
		ru->ru_nvcsw += (l->l_ncsw - l->l_nivcsw);
		ru->ru_nivcsw += l->l_nivcsw;
	}
}

/*
 * lim_copy: make a copy of the plimit structure.
 *
 * We use copy-on-write after fork, and copy when a limit is changed.
 */
struct plimit *
lim_copy(struct plimit *lim)
{
	struct plimit *newlim;
	char *corename;
	size_t alen, len;

	newlim = pool_cache_get(plimit_cache, PR_WAITOK);
	mutex_init(&newlim->pl_lock, MUTEX_DEFAULT, IPL_NONE);
	newlim->pl_writeable = false;
	newlim->pl_refcnt = 1;
	newlim->pl_sv_limit = NULL;

	mutex_enter(&lim->pl_lock);
	memcpy(newlim->pl_rlimit, lim->pl_rlimit,
	    sizeof(struct rlimit) * RLIM_NLIMITS);

	/*
	 * Note: the common case is a use of default core name.
	 */
	alen = 0;
	corename = NULL;
	for (;;) {
		if (lim->pl_corename == defcorename) {
			newlim->pl_corename = defcorename;
			newlim->pl_cnlen = 0;
			break;
		}
		len = lim->pl_cnlen;
		if (len == alen) {
			newlim->pl_corename = corename;
			newlim->pl_cnlen = len;
			memcpy(corename, lim->pl_corename, len);
			corename = NULL;
			break;
		}
		mutex_exit(&lim->pl_lock);
		if (corename) {
			kmem_free(corename, alen);
		}
		alen = len;
		corename = kmem_alloc(alen, KM_SLEEP);
		mutex_enter(&lim->pl_lock);
	}
	mutex_exit(&lim->pl_lock);

	if (corename) {
		kmem_free(corename, alen);
	}
	return newlim;
}

void
lim_addref(struct plimit *lim)
{
	atomic_inc_uint(&lim->pl_refcnt);
}

/*
 * lim_privatise: give a process its own private plimit structure.
 */
void
lim_privatise(proc_t *p)
{
	struct plimit *lim = p->p_limit, *newlim;

	if (lim->pl_writeable) {
		return;
	}

	newlim = lim_copy(lim);

	mutex_enter(p->p_lock);
	if (p->p_limit->pl_writeable) {
		/* Other thread won the race. */
		mutex_exit(p->p_lock);
		lim_free(newlim);
		return;
	}

	/*
	 * Since p->p_limit can be accessed without locked held,
	 * old limit structure must not be deleted yet.
	 */
	newlim->pl_sv_limit = p->p_limit;
	newlim->pl_writeable = true;
	p->p_limit = newlim;
	mutex_exit(p->p_lock);
}

void
lim_setcorename(proc_t *p, char *name, size_t len)
{
	struct plimit *lim;
	char *oname;
	size_t olen;

	lim_privatise(p);
	lim = p->p_limit;

	mutex_enter(&lim->pl_lock);
	oname = lim->pl_corename;
	olen = lim->pl_cnlen;
	lim->pl_corename = name;
	lim->pl_cnlen = len;
	mutex_exit(&lim->pl_lock);

	if (oname != defcorename) {
		kmem_free(oname, olen);
	}
}

void
lim_free(struct plimit *lim)
{
	struct plimit *sv_lim;

	do {
		if (atomic_dec_uint_nv(&lim->pl_refcnt) > 0) {
			return;
		}
		if (lim->pl_corename != defcorename) {
			kmem_free(lim->pl_corename, lim->pl_cnlen);
		}
		sv_lim = lim->pl_sv_limit;
		mutex_destroy(&lim->pl_lock);
		pool_cache_put(plimit_cache, lim);
	} while ((lim = sv_lim) != NULL);
}

struct pstats *
pstatscopy(struct pstats *ps)
{
	struct pstats *nps;
	size_t len;

	nps = pool_cache_get(pstats_cache, PR_WAITOK);

	len = (char *)&nps->pstat_endzero - (char *)&nps->pstat_startzero;
	memset(&nps->pstat_startzero, 0, len);

	len = (char *)&nps->pstat_endcopy - (char *)&nps->pstat_startcopy;
	memcpy(&nps->pstat_startcopy, &ps->pstat_startcopy, len);

	return nps;
}

void
pstatsfree(struct pstats *ps)
{

	pool_cache_put(pstats_cache, ps);
}

/*
 * sysctl_proc_findproc: a routine for sysctl proc subtree helpers that
 * need to pick a valid process by PID.
 *
 * => Hold a reference on the process, on success.
 */
static int
sysctl_proc_findproc(lwp_t *l, pid_t pid, proc_t **p2)
{
	proc_t *p;
	int error;

	if (pid == PROC_CURPROC) {
		p = l->l_proc;
	} else {
		mutex_enter(proc_lock);
		p = proc_find(pid);
		if (p == NULL) {
			mutex_exit(proc_lock);
			return ESRCH;
		}
	}
	error = rw_tryenter(&p->p_reflock, RW_READER) ? 0 : EBUSY;
	if (pid != PROC_CURPROC) {
		mutex_exit(proc_lock);
	}
	*p2 = p;
	return error;
}

/*
 * sysctl_proc_corename: helper routine to get or set the core file name
 * for a process specified by PID.
 */
static int
sysctl_proc_corename(SYSCTLFN_ARGS)
{
	struct proc *p;
	struct plimit *lim;
	char *cnbuf, *cname;
	struct sysctlnode node;
	size_t len;
	int error;

	/* First, validate the request. */
	if (namelen != 0 || name[-1] != PROC_PID_CORENAME)
		return EINVAL;

	/* Find the process.  Hold a reference (p_reflock), if found. */
	error = sysctl_proc_findproc(l, (pid_t)name[-2], &p);
	if (error)
		return error;

	/* XXX-elad */
	error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_CANSEE, p,
	    KAUTH_ARG(KAUTH_REQ_PROCESS_CANSEE_ENTRY), NULL, NULL);
	if (error) {
		rw_exit(&p->p_reflock);
		return error;
	}

	cnbuf = PNBUF_GET();

	if (oldp) {
		/* Get case: copy the core name into the buffer. */
		error = kauth_authorize_process(l->l_cred,
		    KAUTH_PROCESS_CORENAME, p,
		    KAUTH_ARG(KAUTH_REQ_PROCESS_CORENAME_GET), NULL, NULL);
		if (error) {
			goto done;
		}
		lim = p->p_limit;
		mutex_enter(&lim->pl_lock);
		strlcpy(cnbuf, lim->pl_corename, MAXPATHLEN);
		mutex_exit(&lim->pl_lock);
	}

	node = *rnode;
	node.sysctl_data = cnbuf;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	/* Return if error, or if caller is only getting the core name. */
	if (error || newp == NULL) {
		goto done;
	}

	/*
	 * Set case.  Check permission and then validate new core name.
	 * It must be either "core", "/core", or end in ".core".
	 */
	error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_CORENAME,
	    p, KAUTH_ARG(KAUTH_REQ_PROCESS_CORENAME_SET), cnbuf, NULL);
	if (error) {
		goto done;
	}
	len = strlen(cnbuf);
	if ((len < 4 || strcmp(cnbuf + len - 4, "core") != 0) ||
	    (len > 4 && cnbuf[len - 5] != '/' && cnbuf[len - 5] != '.')) {
		error = EINVAL;
		goto done;
	}

	/* Allocate, copy and set the new core name for plimit structure. */
	cname = kmem_alloc(++len, KM_NOSLEEP);
	if (cname == NULL) {
		error = ENOMEM;
		goto done;
	}
	memcpy(cname, cnbuf, len);
	lim_setcorename(p, cname, len);
done:
	rw_exit(&p->p_reflock);
	PNBUF_PUT(cnbuf);
	return error;
}

/*
 * sysctl_proc_stop: helper routine for checking/setting the stop flags.
 */
static int
sysctl_proc_stop(SYSCTLFN_ARGS)
{
	struct proc *p;
	int isset, flag, error = 0;
	struct sysctlnode node;

	if (namelen != 0)
		return EINVAL;

	/* Find the process.  Hold a reference (p_reflock), if found. */
	error = sysctl_proc_findproc(l, (pid_t)name[-2], &p);
	if (error)
		return error;

	/* XXX-elad */
	error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_CANSEE, p,
	    KAUTH_ARG(KAUTH_REQ_PROCESS_CANSEE_ENTRY), NULL, NULL);
	if (error) {
		goto out;
	}

	/* Determine the flag. */
	switch (rnode->sysctl_num) {
	case PROC_PID_STOPFORK:
		flag = PS_STOPFORK;
		break;
	case PROC_PID_STOPEXEC:
		flag = PS_STOPEXEC;
		break;
	case PROC_PID_STOPEXIT:
		flag = PS_STOPEXIT;
		break;
	default:
		error = EINVAL;
		goto out;
	}
	isset = (p->p_flag & flag) ? 1 : 0;
	node = *rnode;
	node.sysctl_data = &isset;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	/* Return if error, or if callers is only getting the flag. */
	if (error || newp == NULL) {
		goto out;
	}

	/* Check if caller can set the flags. */
	error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_STOPFLAG,
	    p, KAUTH_ARG(flag), NULL, NULL);
	if (error) {
		goto out;
	}
	mutex_enter(p->p_lock);
	if (isset) {
		p->p_sflag |= flag;
	} else {
		p->p_sflag &= ~flag;
	}
	mutex_exit(p->p_lock);
out:
	rw_exit(&p->p_reflock);
	return error;
}

/*
 * sysctl_proc_plimit: helper routine to get/set rlimits of a process.
 */
static int
sysctl_proc_plimit(SYSCTLFN_ARGS)
{
	struct proc *p;
	u_int limitno;
	int which, error = 0;
        struct rlimit alim;
	struct sysctlnode node;

	if (namelen != 0)
		return EINVAL;

	which = name[-1];
	if (which != PROC_PID_LIMIT_TYPE_SOFT &&
	    which != PROC_PID_LIMIT_TYPE_HARD)
		return EINVAL;

	limitno = name[-2] - 1;
	if (limitno >= RLIM_NLIMITS)
		return EINVAL;

	if (name[-3] != PROC_PID_LIMIT)
		return EINVAL;

	/* Find the process.  Hold a reference (p_reflock), if found. */
	error = sysctl_proc_findproc(l, (pid_t)name[-4], &p);
	if (error)
		return error;

	/* XXX-elad */
	error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_CANSEE, p,
	    KAUTH_ARG(KAUTH_REQ_PROCESS_CANSEE_ENTRY), NULL, NULL);
	if (error)
		goto out;

	/* Check if caller can retrieve the limits. */
	if (newp == NULL) {
		error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_RLIMIT,
		    p, KAUTH_ARG(KAUTH_REQ_PROCESS_RLIMIT_GET), &alim,
		    KAUTH_ARG(which));
		if (error)
			goto out;
	}

	/* Retrieve the limits. */
	node = *rnode;
	memcpy(&alim, &p->p_rlimit[limitno], sizeof(alim));
	if (which == PROC_PID_LIMIT_TYPE_HARD) {
		node.sysctl_data = &alim.rlim_max;
	} else {
		node.sysctl_data = &alim.rlim_cur;
	}
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	/* Return if error, or if we are only retrieving the limits. */
	if (error || newp == NULL) {
		goto out;
	}
	error = dosetrlimit(l, p, limitno, &alim);
out:
	rw_exit(&p->p_reflock);
	return error;
}

/*
 * Setup sysctl nodes.
 */
static void
sysctl_proc_setup(void)
{

	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_ANYNUMBER,
		       CTLTYPE_NODE, "curproc",
		       SYSCTL_DESCR("Per-process settings"),
		       NULL, 0, NULL, 0,
		       CTL_PROC, PROC_CURPROC, CTL_EOL);

	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_ANYWRITE,
		       CTLTYPE_STRING, "corename",
		       SYSCTL_DESCR("Core file name"),
		       sysctl_proc_corename, 0, NULL, MAXPATHLEN,
		       CTL_PROC, PROC_CURPROC, PROC_PID_CORENAME, CTL_EOL);
	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "rlimit",
		       SYSCTL_DESCR("Process limits"),
		       NULL, 0, NULL, 0,
		       CTL_PROC, PROC_CURPROC, PROC_PID_LIMIT, CTL_EOL);

#define create_proc_plimit(s, n) do {					\
	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,			\
		       CTLFLAG_PERMANENT,				\
		       CTLTYPE_NODE, s,					\
		       SYSCTL_DESCR("Process " s " limits"),		\
		       NULL, 0, NULL, 0,				\
		       CTL_PROC, PROC_CURPROC, PROC_PID_LIMIT, n,	\
		       CTL_EOL);					\
	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,			\
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_ANYWRITE, \
		       CTLTYPE_QUAD, "soft",				\
		       SYSCTL_DESCR("Process soft " s " limit"),	\
		       sysctl_proc_plimit, 0, NULL, 0,			\
		       CTL_PROC, PROC_CURPROC, PROC_PID_LIMIT, n,	\
		       PROC_PID_LIMIT_TYPE_SOFT, CTL_EOL);		\
	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,			\
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_ANYWRITE, \
		       CTLTYPE_QUAD, "hard",				\
		       SYSCTL_DESCR("Process hard " s " limit"),	\
		       sysctl_proc_plimit, 0, NULL, 0,			\
		       CTL_PROC, PROC_CURPROC, PROC_PID_LIMIT, n,	\
		       PROC_PID_LIMIT_TYPE_HARD, CTL_EOL);		\
	} while (0/*CONSTCOND*/)

	create_proc_plimit("cputime",		PROC_PID_LIMIT_CPU);
	create_proc_plimit("filesize",		PROC_PID_LIMIT_FSIZE);
	create_proc_plimit("datasize",		PROC_PID_LIMIT_DATA);
	create_proc_plimit("stacksize",		PROC_PID_LIMIT_STACK);
	create_proc_plimit("coredumpsize",	PROC_PID_LIMIT_CORE);
	create_proc_plimit("memoryuse",		PROC_PID_LIMIT_RSS);
	create_proc_plimit("memorylocked",	PROC_PID_LIMIT_MEMLOCK);
	create_proc_plimit("maxproc",		PROC_PID_LIMIT_NPROC);
	create_proc_plimit("descriptors",	PROC_PID_LIMIT_NOFILE);
	create_proc_plimit("sbsize",		PROC_PID_LIMIT_SBSIZE);
	create_proc_plimit("vmemoryuse",	PROC_PID_LIMIT_AS);
	create_proc_plimit("maxlwp",		PROC_PID_LIMIT_NTHR);

#undef create_proc_plimit

	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_ANYWRITE,
		       CTLTYPE_INT, "stopfork",
		       SYSCTL_DESCR("Stop process at fork(2)"),
		       sysctl_proc_stop, 0, NULL, 0,
		       CTL_PROC, PROC_CURPROC, PROC_PID_STOPFORK, CTL_EOL);
	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_ANYWRITE,
		       CTLTYPE_INT, "stopexec",
		       SYSCTL_DESCR("Stop process at execve(2)"),
		       sysctl_proc_stop, 0, NULL, 0,
		       CTL_PROC, PROC_CURPROC, PROC_PID_STOPEXEC, CTL_EOL);
	sysctl_createv(&proc_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_ANYWRITE,
		       CTLTYPE_INT, "stopexit",
		       SYSCTL_DESCR("Stop process before completing exit"),
		       sysctl_proc_stop, 0, NULL, 0,
		       CTL_PROC, PROC_CURPROC, PROC_PID_STOPEXIT, CTL_EOL);
}
