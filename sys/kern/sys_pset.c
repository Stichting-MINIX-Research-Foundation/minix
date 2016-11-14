/*	$NetBSD: sys_pset.c,v 1.19 2015/07/30 08:11:44 maxv Exp $	*/

/*
 * Copyright (c) 2008, Mindaugas Rasiukevicius <rmind at NetBSD org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Implementation of the Processor Sets.
 * 
 * Locking
 *  The array of the processor-set structures and its members are protected
 *  by the global cpu_lock.  Note that in scheduler, the very l_psid value
 *  might be used without lock held.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_pset.c,v 1.19 2015/07/30 08:11:44 maxv Exp $");

#include <sys/param.h>

#include <sys/cpu.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/lwp.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pset.h>
#include <sys/sched.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/types.h>

static pset_info_t **	psets;
static u_int		psets_max;
static u_int		psets_count;
static kauth_listener_t	psets_listener;

static int	psets_realloc(int);
static int	psid_validate(psetid_t, bool);
static int	kern_pset_create(psetid_t *);
static int	kern_pset_destroy(psetid_t);

static int
psets_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	psetid_t id;
	enum kauth_system_req req;
	int result;

	result = KAUTH_RESULT_DEFER;
	req = (enum kauth_system_req)arg0;
	id = (psetid_t)(unsigned long)arg1;

	if (action != KAUTH_SYSTEM_PSET)
		return result;

	if ((req == KAUTH_REQ_SYSTEM_PSET_ASSIGN) ||
	    (req == KAUTH_REQ_SYSTEM_PSET_BIND)) {
		if (id == PS_QUERY)
			result = KAUTH_RESULT_ALLOW;
	}

	return result;
}

/*
 * Initialization of the processor-sets.
 */
void
psets_init(void)
{

	psets_max = max(maxcpus, 32);
	psets = kmem_zalloc(psets_max * sizeof(void *), KM_SLEEP);
	psets_count = 0;

	psets_listener = kauth_listen_scope(KAUTH_SCOPE_SYSTEM,
	    psets_listener_cb, NULL);
}

/*
 * Reallocate the array of the processor-set structures.
 */
static int
psets_realloc(int new_psets_max)
{
	pset_info_t **new_psets, **old_psets;
	const u_int newsize = new_psets_max * sizeof(void *);
	u_int i, oldsize;

	if (new_psets_max < 1)
		return EINVAL;

	new_psets = kmem_zalloc(newsize, KM_SLEEP);
	mutex_enter(&cpu_lock);
	old_psets = psets;
	oldsize = psets_max * sizeof(void *);

	/* Check if we can lower the size of the array */
	if (new_psets_max < psets_max) {
		for (i = new_psets_max; i < psets_max; i++) {
			if (psets[i] == NULL)
				continue;
			mutex_exit(&cpu_lock);
			kmem_free(new_psets, newsize);
			return EBUSY;
		}
	}

	/* Copy all pointers to the new array */
	memcpy(new_psets, psets, newsize);
	psets_max = new_psets_max;
	psets = new_psets;
	mutex_exit(&cpu_lock);

	kmem_free(old_psets, oldsize);
	return 0;
}

/*
 * Validate processor-set ID.
 */
static int
psid_validate(psetid_t psid, bool chkps)
{

	KASSERT(mutex_owned(&cpu_lock));

	if (chkps && (psid == PS_NONE || psid == PS_QUERY || psid == PS_MYID))
		return 0;
	if (psid <= 0 || psid > psets_max)
		return EINVAL;
	if (psets[psid - 1] == NULL)
		return EINVAL;
	if (psets[psid - 1]->ps_flags & PSET_BUSY)
		return EBUSY;

	return 0;
}

/*
 * Create a processor-set.
 */
static int
kern_pset_create(psetid_t *psid)
{
	pset_info_t *pi;
	u_int i;

	if (psets_count == psets_max)
		return ENOMEM;

	pi = kmem_zalloc(sizeof(pset_info_t), KM_SLEEP);

	mutex_enter(&cpu_lock);
	if (psets_count == psets_max) {
		mutex_exit(&cpu_lock);
		kmem_free(pi, sizeof(pset_info_t));
		return ENOMEM;
	}

	/* Find a free entry in the array */
	for (i = 0; i < psets_max; i++)
		if (psets[i] == NULL)
			break;
	KASSERT(i != psets_max);

	psets[i] = pi;
	psets_count++;
	mutex_exit(&cpu_lock);

	*psid = i + 1;
	return 0;
}

/*
 * Destroy a processor-set.
 */
static int
kern_pset_destroy(psetid_t psid)
{
	struct cpu_info *ci;
	pset_info_t *pi;
	struct lwp *l;
	CPU_INFO_ITERATOR cii;
	int error;

	mutex_enter(&cpu_lock);
	if (psid == PS_MYID) {
		/* Use caller's processor-set ID */
		psid = curlwp->l_psid;
	}
	error = psid_validate(psid, false);
	if (error) {
		mutex_exit(&cpu_lock);
		return error;
	}

	/* Release the processor-set from all CPUs */
	for (CPU_INFO_FOREACH(cii, ci)) {
		struct schedstate_percpu *spc;

		spc = &ci->ci_schedstate;
		if (spc->spc_psid != psid)
			continue;
		spc->spc_psid = PS_NONE;
	}
	/* Mark that processor-set is going to be destroyed */
	pi = psets[psid - 1];
	pi->ps_flags |= PSET_BUSY;
	mutex_exit(&cpu_lock);

	/* Unmark the processor-set ID from each thread */
	mutex_enter(proc_lock);
	LIST_FOREACH(l, &alllwp, l_list) {
		/* Safe to check and set without lock held */
		if (l->l_psid != psid)
			continue;
		l->l_psid = PS_NONE;
	}
	mutex_exit(proc_lock);

	/* Destroy the processor-set */
	mutex_enter(&cpu_lock);
	psets[psid - 1] = NULL;
	psets_count--;
	mutex_exit(&cpu_lock);

	kmem_free(pi, sizeof(pset_info_t));
	return 0;
}

/*
 * General system calls for the processor-sets.
 */

int
sys_pset_create(struct lwp *l, const struct sys_pset_create_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(psetid_t) *psid;
	} */
	psetid_t psid;
	int error;

	/* Available only for super-user */
	if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_PSET,
	    KAUTH_REQ_SYSTEM_PSET_CREATE, NULL, NULL, NULL))
		return EPERM;

	error = kern_pset_create(&psid);
	if (error)
		return error;

	error = copyout(&psid, SCARG(uap, psid), sizeof(psetid_t));
	if (error)
		(void)kern_pset_destroy(psid);

	return error;
}

int
sys_pset_destroy(struct lwp *l, const struct sys_pset_destroy_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(psetid_t) psid;
	} */

	/* Available only for super-user */
	if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_PSET,
	    KAUTH_REQ_SYSTEM_PSET_DESTROY,
	    KAUTH_ARG(SCARG(uap, psid)), NULL, NULL))
		return EPERM;

	return kern_pset_destroy(SCARG(uap, psid));
}

int
sys_pset_assign(struct lwp *l, const struct sys_pset_assign_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(psetid_t) psid;
		syscallarg(cpuid_t) cpuid;
		syscallarg(psetid_t) *opsid;
	} */
	struct cpu_info *ici, *ci = NULL;
	struct schedstate_percpu *spc = NULL;
	struct lwp *t;
	psetid_t psid = SCARG(uap, psid), opsid = 0;
	CPU_INFO_ITERATOR cii;
	int error = 0, nnone = 0;

	/* Available only for super-user, except the case of PS_QUERY */
	if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_PSET,
	    KAUTH_REQ_SYSTEM_PSET_ASSIGN, KAUTH_ARG(SCARG(uap, psid)), NULL,
	    NULL))
		return EPERM;

	/* Find the target CPU */
	mutex_enter(&cpu_lock);
	for (CPU_INFO_FOREACH(cii, ici)) {
		struct schedstate_percpu *ispc;
		ispc = &ici->ci_schedstate;
		if (cpu_index(ici) == SCARG(uap, cpuid)) {
			ci = ici;
			spc = ispc;
		}
		nnone += (ispc->spc_psid == PS_NONE);
	}
	if (ci == NULL) {
		mutex_exit(&cpu_lock);
		return EINVAL;
	}
	error = psid_validate(psid, true);
	if (error) {
		mutex_exit(&cpu_lock);
		return error;
	}
	opsid = spc->spc_psid;
	switch (psid) {
	case PS_QUERY:
		break;
	case PS_MYID:
		psid = curlwp->l_psid;
		/* FALLTHROUGH */
	default:
		/*
		 * Ensure at least one CPU stays in the default set,
		 * and that specified CPU is not offline.
		 */
		if (psid != PS_NONE && ((spc->spc_flags & SPCF_OFFLINE) ||
		    (nnone == 1 && spc->spc_psid == PS_NONE))) {
			mutex_exit(&cpu_lock);
			return EBUSY;
		}
		mutex_enter(proc_lock);
		/*
		 * Ensure that none of the threads are using affinity mask
		 * with this target CPU in it.
		 */
		LIST_FOREACH(t, &alllwp, l_list) {
			if (t->l_affinity == NULL) {
				continue;
			}
			lwp_lock(t);
			if (t->l_affinity == NULL) {
				lwp_unlock(t);
				continue;
			}
			if (kcpuset_isset(t->l_affinity, cpu_index(ci))) {
				lwp_unlock(t);
				mutex_exit(proc_lock);
				mutex_exit(&cpu_lock);
				return EPERM;
			}
			lwp_unlock(t);
		}
		/*
		 * Set the processor-set ID.
		 * Migrate out any threads running on this CPU.
		 */
		spc->spc_psid = psid;

		LIST_FOREACH(t, &alllwp, l_list) {
			struct cpu_info *tci;
			if (t->l_cpu != ci)
				continue;
			if (t->l_pflag & (LP_BOUND | LP_INTR))
				continue;
			lwp_lock(t);
			tci = sched_takecpu(t);
			KASSERT(tci != ci);
			lwp_migrate(t, tci);
		}
		mutex_exit(proc_lock);
		break;
	}
	mutex_exit(&cpu_lock);

	if (SCARG(uap, opsid) != NULL)
		error = copyout(&opsid, SCARG(uap, opsid), sizeof(psetid_t));

	return error;
}

int
sys__pset_bind(struct lwp *l, const struct sys__pset_bind_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(idtype_t) idtype;
		syscallarg(id_t) first_id;
		syscallarg(id_t) second_id;
		syscallarg(psetid_t) psid;
		syscallarg(psetid_t) *opsid;
	} */
	struct cpu_info *ci;
	struct proc *p;
	struct lwp *t;
	id_t id1, id2;
	pid_t pid = 0;
	lwpid_t lid = 0;
	psetid_t psid, opsid;
	int error = 0, lcnt;

	psid = SCARG(uap, psid);

	/* Available only for super-user, except the case of PS_QUERY */
	if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_PSET,
	    KAUTH_REQ_SYSTEM_PSET_BIND, KAUTH_ARG(SCARG(uap, psid)), NULL,
	    NULL))
		return EPERM;

	mutex_enter(&cpu_lock);
	error = psid_validate(psid, true);
	if (error) {
		mutex_exit(&cpu_lock);
		return error;
	}
	if (psid == PS_MYID)
		psid = curlwp->l_psid;
	if (psid != PS_QUERY && psid != PS_NONE)
		psets[psid - 1]->ps_flags |= PSET_BUSY;
	mutex_exit(&cpu_lock);

	/*
	 * Get PID and LID from the ID.
	 */
	p = l->l_proc;
	id1 = SCARG(uap, first_id);
	id2 = SCARG(uap, second_id);

	switch (SCARG(uap, idtype)) {
	case P_PID:
		/*
		 * Process:
		 *  First ID	- PID;
		 *  Second ID	- ignored;
		 */
		pid = (id1 == P_MYID) ? p->p_pid : id1;
		lid = 0;
		break;
	case P_LWPID:
		/*
		 * Thread (LWP):
		 *  First ID	- LID;
		 *  Second ID	- PID;
		 */
		if (id1 == P_MYID) {
			pid = p->p_pid;
			lid = l->l_lid;
			break;
		}
		lid = id1;
		pid = (id2 == P_MYID) ? p->p_pid : id2;
		break;
	default:
		error = EINVAL;
		goto error;
	}

	/* Find the process */
	mutex_enter(proc_lock);
	p = proc_find(pid);
	if (p == NULL) {
		mutex_exit(proc_lock);
		error = ESRCH;
		goto error;
	}
	mutex_enter(p->p_lock);
	mutex_exit(proc_lock);

	/* Disallow modification of the system processes */
	if (p->p_flag & PK_SYSTEM) {
		mutex_exit(p->p_lock);
		error = EPERM;
		goto error;
	}

	/* Find the LWP(s) */
	lcnt = 0;
	ci = NULL;
	LIST_FOREACH(t, &p->p_lwps, l_sibling) {
		if (lid && lid != t->l_lid)
			continue;
		/*
		 * Bind the thread to the processor-set,
		 * take some CPU and migrate.
		 */
		lwp_lock(t);
		opsid = t->l_psid;
		t->l_psid = psid;
		ci = sched_takecpu(t);
		/* Unlocks LWP */
		lwp_migrate(t, ci);
		lcnt++;
	}
	mutex_exit(p->p_lock);
	if (lcnt == 0) {
		error = ESRCH;
		goto error;
	}
	if (SCARG(uap, opsid))
		error = copyout(&opsid, SCARG(uap, opsid), sizeof(psetid_t));
error:
	if (psid != PS_QUERY && psid != PS_NONE) {
		mutex_enter(&cpu_lock);
		psets[psid - 1]->ps_flags &= ~PSET_BUSY;
		mutex_exit(&cpu_lock);
	}
	return error;
}

/*
 * Sysctl nodes and initialization.
 */

static int
sysctl_psets_max(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int error, newsize;

	node = *rnode;
	node.sysctl_data = &newsize;

	newsize = psets_max;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (newsize <= 0)
		return EINVAL;

	sysctl_unlock();
	error = psets_realloc(newsize);
	sysctl_relock();
	return error;
}

static int
sysctl_psets_list(SYSCTLFN_ARGS)
{
	const size_t bufsz = 1024;
	char *buf, tbuf[16];
	int i, error;
	size_t len;

	sysctl_unlock();
	buf = kmem_alloc(bufsz, KM_SLEEP);
	snprintf(buf, bufsz, "%d:1", PS_NONE);	/* XXX */

	mutex_enter(&cpu_lock);
	for (i = 0; i < psets_max; i++) {
		if (psets[i] == NULL)
			continue;
		snprintf(tbuf, sizeof(tbuf), ",%d:2", i + 1);	/* XXX */
		strlcat(buf, tbuf, bufsz);
	}
	mutex_exit(&cpu_lock);
	len = strlen(buf) + 1;
	error = 0;
	if (oldp != NULL)
		error = copyout(buf, oldp, min(len, *oldlenp));
	*oldlenp = len;
	kmem_free(buf, bufsz);
	sysctl_relock();
	return error;
}

SYSCTL_SETUP(sysctl_pset_setup, "sysctl kern.pset subtree setup")
{
	const struct sysctlnode *node = NULL;

	sysctl_createv(clog, 0, NULL, &node,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "pset",
		SYSCTL_DESCR("Processor-set options"),
		NULL, 0, NULL, 0,
		CTL_KERN, CTL_CREATE, CTL_EOL);

	if (node == NULL)
		return;

	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "psets_max",
		SYSCTL_DESCR("Maximal count of the processor-sets"),
		sysctl_psets_max, 0, &psets_max, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_STRING, "list",
		SYSCTL_DESCR("List of active sets"),
		sysctl_psets_list, 0, NULL, 0,
		CTL_CREATE, CTL_EOL);
}
