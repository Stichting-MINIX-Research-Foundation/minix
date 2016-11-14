/*	$NetBSD: kern_cpu.c,v 1.71 2015/08/29 12:24:00 maxv Exp $	*/

/*-
 * Copyright (c) 2007, 2008, 2009, 2010, 2012 The NetBSD Foundation, Inc.
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

/*-
 * Copyright (c)2007 YAMAMOTO Takashi,
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_cpu.c,v 1.71 2015/08/29 12:24:00 maxv Exp $");

#include "opt_cpu_ucode.h"
#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/idle.h>
#include <sys/sched.h>
#include <sys/intr.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/cpuio.h>
#include <sys/proc.h>
#include <sys/percpu.h>
#include <sys/kernel.h>
#include <sys/kauth.h>
#include <sys/xcall.h>
#include <sys/pool.h>
#include <sys/kmem.h>
#include <sys/select.h>
#include <sys/namei.h>
#include <sys/callout.h>
#include <sys/pcu.h>

#include <uvm/uvm_extern.h>

#include "ioconf.h"

/*
 * If the port has stated that cpu_data is the first thing in cpu_info,
 * verify that the claim is true. This will prevent them from getting out
 * of sync.
 */
#ifdef __HAVE_CPU_DATA_FIRST
CTASSERT(offsetof(struct cpu_info, ci_data) == 0);
#else
CTASSERT(offsetof(struct cpu_info, ci_data) != 0);
#endif

static void	cpu_xc_online(struct cpu_info *);
static void	cpu_xc_offline(struct cpu_info *);

dev_type_ioctl(cpuctl_ioctl);

const struct cdevsw cpuctl_cdevsw = {
	.d_open = nullopen,
	.d_close = nullclose,
	.d_read = nullread,
	.d_write = nullwrite,
	.d_ioctl = cpuctl_ioctl,
	.d_stop = nullstop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

kmutex_t	cpu_lock		__cacheline_aligned;
int		ncpu			__read_mostly;
int		ncpuonline		__read_mostly;
bool		mp_online		__read_mostly;

/* An array of CPUs.  There are ncpu entries. */
struct cpu_info **cpu_infos		__read_mostly;

/* Note: set on mi_cpu_attach() and idle_loop(). */
kcpuset_t *	kcpuset_attached	__read_mostly	= NULL;
kcpuset_t *	kcpuset_running		__read_mostly	= NULL;


static char cpu_model[128];

/*
 * mi_cpu_init: early initialisation of MI CPU related structures.
 *
 * Note: may not block and memory allocator is not yet available.
 */
void
mi_cpu_init(void)
{

	mutex_init(&cpu_lock, MUTEX_DEFAULT, IPL_NONE);

	kcpuset_create(&kcpuset_attached, true);
	kcpuset_create(&kcpuset_running, true);
	kcpuset_set(kcpuset_running, 0);
}

int
mi_cpu_attach(struct cpu_info *ci)
{
	int error;

	KASSERT(maxcpus > 0);

	ci->ci_index = ncpu;
	kcpuset_set(kcpuset_attached, cpu_index(ci));

	/*
	 * Create a convenience cpuset of just ourselves.
	 */
	kcpuset_create(&ci->ci_data.cpu_kcpuset, true);
	kcpuset_set(ci->ci_data.cpu_kcpuset, cpu_index(ci));

	TAILQ_INIT(&ci->ci_data.cpu_ld_locks);
	__cpu_simple_lock_init(&ci->ci_data.cpu_ld_lock);

	/* This is useful for eg, per-cpu evcnt */
	snprintf(ci->ci_data.cpu_name, sizeof(ci->ci_data.cpu_name), "cpu%d",
	    cpu_index(ci));

	if (__predict_false(cpu_infos == NULL)) {
		size_t ci_bufsize = (maxcpus + 1) * sizeof(struct cpu_info *);
		cpu_infos = kmem_zalloc(ci_bufsize, KM_SLEEP);
	}
	cpu_infos[cpu_index(ci)] = ci;

	sched_cpuattach(ci);

	error = create_idle_lwp(ci);
	if (error != 0) {
		/* XXX revert sched_cpuattach */
		return error;
	}

	if (ci == curcpu())
		ci->ci_data.cpu_onproc = curlwp;
	else
		ci->ci_data.cpu_onproc = ci->ci_data.cpu_idlelwp;

	percpu_init_cpu(ci);
	softint_init(ci);
	callout_init_cpu(ci);
	xc_init_cpu(ci);
	pool_cache_cpu_init(ci);
	selsysinit(ci);
	cache_cpu_init(ci);
	TAILQ_INIT(&ci->ci_data.cpu_biodone);
	ncpu++;
	ncpuonline++;

	return 0;
}

void
cpuctlattach(int dummy __unused)
{

	KASSERT(cpu_infos != NULL);
}

int
cpuctl_ioctl(dev_t dev, u_long cmd, void *data, int flag, lwp_t *l)
{
	CPU_INFO_ITERATOR cii;
	cpustate_t *cs;
	struct cpu_info *ci;
	int error, i;
	u_int id;

	error = 0;

	mutex_enter(&cpu_lock);
	switch (cmd) {
	case IOC_CPU_SETSTATE:
		cs = data;
		error = kauth_authorize_system(l->l_cred,
		    KAUTH_SYSTEM_CPU, KAUTH_REQ_SYSTEM_CPU_SETSTATE, cs, NULL,
		    NULL);
		if (error != 0)
			break;
		if (cs->cs_id >= maxcpus ||
		    (ci = cpu_lookup(cs->cs_id)) == NULL) {
			error = ESRCH;
			break;
		}
		cpu_setintr(ci, cs->cs_intr);
		error = cpu_setstate(ci, cs->cs_online);
		break;

	case IOC_CPU_GETSTATE:
		cs = data;
		id = cs->cs_id;
		memset(cs, 0, sizeof(*cs));
		cs->cs_id = id;
		if (cs->cs_id >= maxcpus ||
		    (ci = cpu_lookup(id)) == NULL) {
			error = ESRCH;
			break;
		}
		if ((ci->ci_schedstate.spc_flags & SPCF_OFFLINE) != 0)
			cs->cs_online = false;
		else
			cs->cs_online = true;
		if ((ci->ci_schedstate.spc_flags & SPCF_NOINTR) != 0)
			cs->cs_intr = false;
		else
			cs->cs_intr = true;
		cs->cs_lastmod = (int32_t)ci->ci_schedstate.spc_lastmod;
		cs->cs_lastmodhi = (int32_t)
		    (ci->ci_schedstate.spc_lastmod >> 32);
		cs->cs_intrcnt = cpu_intr_count(ci) + 1;
		cs->cs_hwid = ci->ci_cpuid;
		break;

	case IOC_CPU_MAPID:
		i = 0;
		for (CPU_INFO_FOREACH(cii, ci)) {
			if (i++ == *(int *)data)
				break;
		}
		if (ci == NULL)
			error = ESRCH;
		else
			*(int *)data = cpu_index(ci);
		break;

	case IOC_CPU_GETCOUNT:
		*(int *)data = ncpu;
		break;

#ifdef CPU_UCODE
	case IOC_CPU_UCODE_GET_VERSION:
		error = cpu_ucode_get_version((struct cpu_ucode_version *)data);
		break;

#ifdef COMPAT_60
	case OIOC_CPU_UCODE_GET_VERSION:
		error = compat6_cpu_ucode_get_version((struct compat6_cpu_ucode *)data);
		break;
#endif

	case IOC_CPU_UCODE_APPLY:
		error = kauth_authorize_machdep(l->l_cred,
		    KAUTH_MACHDEP_CPU_UCODE_APPLY,
		    NULL, NULL, NULL, NULL);
		if (error != 0)
			break;
		error = cpu_ucode_apply((const struct cpu_ucode *)data);
		break;

#ifdef COMPAT_60
	case OIOC_CPU_UCODE_APPLY:
		error = kauth_authorize_machdep(l->l_cred,
		    KAUTH_MACHDEP_CPU_UCODE_APPLY,
		    NULL, NULL, NULL, NULL);
		if (error != 0)
			break;
		error = compat6_cpu_ucode_apply((const struct compat6_cpu_ucode *)data);
		break;
#endif
#endif

	default:
		error = ENOTTY;
		break;
	}
	mutex_exit(&cpu_lock);

	return error;
}

struct cpu_info *
cpu_lookup(u_int idx)
{
	struct cpu_info *ci;

	KASSERT(idx < maxcpus);

	if (__predict_false(cpu_infos == NULL)) {
		KASSERT(idx == 0);
		return curcpu();
	}

	ci = cpu_infos[idx];
	KASSERT(ci == NULL || cpu_index(ci) == idx);

	return ci;
}

static void
cpu_xc_offline(struct cpu_info *ci)
{
	struct schedstate_percpu *spc, *mspc = NULL;
	struct cpu_info *target_ci;
	struct lwp *l;
	CPU_INFO_ITERATOR cii;
	int s;

	/*
	 * Thread that made the cross call (separate context) holds
	 * cpu_lock on our behalf.
	 */
	spc = &ci->ci_schedstate;
	s = splsched();
	spc->spc_flags |= SPCF_OFFLINE;
	splx(s);

	/* Take the first available CPU for the migration. */
	for (CPU_INFO_FOREACH(cii, target_ci)) {
		mspc = &target_ci->ci_schedstate;
		if ((mspc->spc_flags & SPCF_OFFLINE) == 0)
			break;
	}
	KASSERT(target_ci != NULL);

	/*
	 * Migrate all non-bound threads to the other CPU.  Note that this
	 * runs from the xcall thread, thus handling of LSONPROC is not needed.
	 */
	mutex_enter(proc_lock);
	LIST_FOREACH(l, &alllwp, l_list) {
		struct cpu_info *mci;

		lwp_lock(l);
		if (l->l_cpu != ci || (l->l_pflag & (LP_BOUND | LP_INTR))) {
			lwp_unlock(l);
			continue;
		}
		/* Regular case - no affinity. */
		if (l->l_affinity == NULL) {
			lwp_migrate(l, target_ci);
			continue;
		}
		/* Affinity is set, find an online CPU in the set. */
		for (CPU_INFO_FOREACH(cii, mci)) {
			mspc = &mci->ci_schedstate;
			if ((mspc->spc_flags & SPCF_OFFLINE) == 0 &&
			    kcpuset_isset(l->l_affinity, cpu_index(mci)))
				break;
		}
		if (mci == NULL) {
			lwp_unlock(l);
			mutex_exit(proc_lock);
			goto fail;
		}
		lwp_migrate(l, mci);
	}
	mutex_exit(proc_lock);

#if PCU_UNIT_COUNT > 0
	pcu_save_all_on_cpu();
#endif

#ifdef __HAVE_MD_CPU_OFFLINE
	cpu_offline_md();
#endif
	return;
fail:
	/* Just unset the SPCF_OFFLINE flag, caller will check */
	s = splsched();
	spc->spc_flags &= ~SPCF_OFFLINE;
	splx(s);
}

static void
cpu_xc_online(struct cpu_info *ci)
{
	struct schedstate_percpu *spc;
	int s;

	spc = &ci->ci_schedstate;
	s = splsched();
	spc->spc_flags &= ~SPCF_OFFLINE;
	splx(s);
}

int
cpu_setstate(struct cpu_info *ci, bool online)
{
	struct schedstate_percpu *spc;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci2;
	uint64_t where;
	xcfunc_t func;
	int nonline;

	spc = &ci->ci_schedstate;

	KASSERT(mutex_owned(&cpu_lock));

	if (online) {
		if ((spc->spc_flags & SPCF_OFFLINE) == 0)
			return 0;
		func = (xcfunc_t)cpu_xc_online;
	} else {
		if ((spc->spc_flags & SPCF_OFFLINE) != 0)
			return 0;
		nonline = 0;
		/*
		 * Ensure that at least one CPU within the processor set
		 * stays online.  Revisit this later.
		 */
		for (CPU_INFO_FOREACH(cii, ci2)) {
			if ((ci2->ci_schedstate.spc_flags & SPCF_OFFLINE) != 0)
				continue;
			if (ci2->ci_schedstate.spc_psid != spc->spc_psid)
				continue;
			nonline++;
		}
		if (nonline == 1)
			return EBUSY;
		func = (xcfunc_t)cpu_xc_offline;
	}

	where = xc_unicast(0, func, ci, NULL, ci);
	xc_wait(where);
	if (online) {
		KASSERT((spc->spc_flags & SPCF_OFFLINE) == 0);
		ncpuonline++;
	} else {
		if ((spc->spc_flags & SPCF_OFFLINE) == 0) {
			/* If was not set offline, then it is busy */
			return EBUSY;
		}
		ncpuonline--;
	}

	spc->spc_lastmod = time_second;
	return 0;
}

int
cpu_setmodel(const char *fmt, ...)
{
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(cpu_model, sizeof(cpu_model), fmt, ap);
	va_end(ap);
	return len;
}

const char *
cpu_getmodel(void)
{
	return cpu_model;
}

#ifdef __HAVE_INTR_CONTROL
static void
cpu_xc_intr(struct cpu_info *ci)
{
	struct schedstate_percpu *spc;
	int s;

	spc = &ci->ci_schedstate;
	s = splsched();
	spc->spc_flags &= ~SPCF_NOINTR;
	splx(s);
}

static void
cpu_xc_nointr(struct cpu_info *ci)
{
	struct schedstate_percpu *spc;
	int s;

	spc = &ci->ci_schedstate;
	s = splsched();
	spc->spc_flags |= SPCF_NOINTR;
	splx(s);
}

int
cpu_setintr(struct cpu_info *ci, bool intr)
{
	struct schedstate_percpu *spc;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci2;
	uint64_t where;
	xcfunc_t func;
	int nintr;

	spc = &ci->ci_schedstate;

	KASSERT(mutex_owned(&cpu_lock));

	if (intr) {
		if ((spc->spc_flags & SPCF_NOINTR) == 0)
			return 0;
		func = (xcfunc_t)cpu_xc_intr;
	} else {
		if ((spc->spc_flags & SPCF_NOINTR) != 0)
			return 0;
		/*
		 * Ensure that at least one CPU within the system
		 * is handing device interrupts.
		 */
		nintr = 0;
		for (CPU_INFO_FOREACH(cii, ci2)) {
			if ((ci2->ci_schedstate.spc_flags & SPCF_NOINTR) != 0)
				continue;
			if (ci2 == ci)
				continue;
			nintr++;
		}
		if (nintr == 0)
			return EBUSY;
		func = (xcfunc_t)cpu_xc_nointr;
	}

	where = xc_unicast(0, func, ci, NULL, ci);
	xc_wait(where);
	if (intr) {
		KASSERT((spc->spc_flags & SPCF_NOINTR) == 0);
	} else if ((spc->spc_flags & SPCF_NOINTR) == 0) {
		/* If was not set offline, then it is busy */
		return EBUSY;
	}

	/* Direct interrupts away from the CPU and record the change. */
	cpu_intr_redistribute();
	spc->spc_lastmod = time_second;
	return 0;
}
#else	/* __HAVE_INTR_CONTROL */
int
cpu_setintr(struct cpu_info *ci, bool intr)
{

	return EOPNOTSUPP;
}

u_int
cpu_intr_count(struct cpu_info *ci)
{

	return 0;	/* 0 == "don't know" */
}
#endif	/* __HAVE_INTR_CONTROL */

bool
cpu_softintr_p(void)
{

	return (curlwp->l_pflag & LP_INTR) != 0;
}

#ifdef CPU_UCODE
int
cpu_ucode_load(struct cpu_ucode_softc *sc, const char *fwname)
{
	firmware_handle_t fwh;
	int error;

	if (sc->sc_blob != NULL) {
		firmware_free(sc->sc_blob, sc->sc_blobsize);
		sc->sc_blob = NULL;
		sc->sc_blobsize = 0;
	}

	error = cpu_ucode_md_open(&fwh, sc->loader_version, fwname);
	if (error != 0) {
		aprint_error("ucode: firmware_open failed: %i\n", error);
		goto err0;
	}

	sc->sc_blobsize = firmware_get_size(fwh);
	sc->sc_blob = firmware_malloc(sc->sc_blobsize);
	if (sc->sc_blob == NULL) {
		error = ENOMEM;
		firmware_close(fwh);
		goto err0;
	}

	error = firmware_read(fwh, 0, sc->sc_blob, sc->sc_blobsize);
	firmware_close(fwh);
	if (error != 0)
		goto err1;

	return 0;

err1:
	firmware_free(sc->sc_blob, sc->sc_blobsize);
	sc->sc_blob = NULL;
	sc->sc_blobsize = 0;
err0:
	return error;
}
#endif
