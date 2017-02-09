/*	$NetBSD: rump_private.h,v 1.92 2015/04/22 17:38:33 pooka Exp $	*/

/*
 * Copyright (c) 2007-2011 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYS_RUMP_PRIVATE_H_
#define _SYS_RUMP_PRIVATE_H_

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <uvm/uvm.h>
#include <uvm/uvm_object.h>
#include <uvm/uvm_page.h>

#include <rump/rump.h>
#include <rump/rumpuser.h>

#include "rumpkern_if_priv.h"

extern struct rumpuser_mtx *rump_giantlock;

extern int rump_threads;
extern struct device rump_rootdev;

extern struct sysent rump_sysent[];

enum rump_component_type {
	RUMP_COMPONENT_DEV,
		RUMP_COMPONENT_DEV_AFTERMAINBUS,
	RUMP_COMPONENT_NET,
		RUMP_COMPONENT_NET_ROUTE,
		RUMP_COMPONENT_NET_IF,
		RUMP_COMPONENT_NET_IFCFG,
	RUMP_COMPONENT_VFS,
	RUMP_COMPONENT_KERN,
		RUMP_COMPONENT_KERN_VFS,
	RUMP_COMPONENT_POSTINIT,
	RUMP_COMPONENT_SYSCALL,

	RUMP__FACTION_DEV,
	RUMP__FACTION_VFS,
	RUMP__FACTION_NET,

	RUMP_COMPONENT_MAX,
};
struct rump_component {
	enum rump_component_type rc_type;
	void (*rc_init)(void);
	LIST_ENTRY(rump_component) rc_entries;
};

/*
 * If RUMP_USE_CTOR is defined, we use __attribute__((constructor)) to
 * determine which components are present when rump_init() is called.
 * Otherwise, we use link sets and the __start/__stop symbols generated
 * by the toolchain.
 */

#ifdef RUMP_USE_CTOR
#define _RUMP_COMPONENT_REGISTER(type)					\
static void rumpcomp_ctor##type(void) __attribute__((constructor));	\
static void rumpcomp_ctor##type(void)					\
{									\
	rump_component_load(&rumpcomp##type);				\
}

#else /* RUMP_USE_CTOR */

#define _RUMP_COMPONENT_REGISTER(type)					\
__link_set_add_rodata(rump_components, rumpcomp##type);
#endif /* RUMP_USE_CTOR */

#define RUMP_COMPONENT(type)				\
static void rumpcompinit##type(void);			\
static struct rump_component rumpcomp##type = {	\
	.rc_type = type,				\
	.rc_init = rumpcompinit##type,			\
};							\
_RUMP_COMPONENT_REGISTER(type)				\
static void rumpcompinit##type(void)

#define FLAWLESSCALL(call)						\
do {									\
	int att_error;							\
	if ((att_error = call) != 0)					\
		panic("\"%s\" failed", #call);				\
} while (/*CONSTCOND*/0)

#define RUMPMEM_UNLIMITED ((unsigned long)-1)
extern unsigned long rump_physmemlimit;

extern struct vmspace *rump_vmspace_local;
extern struct pmap rump_pmap_local;
#define RUMP_LOCALPROC_P(p) \
    (p->p_vmspace == vmspace_kernel() || p->p_vmspace == rump_vmspace_local)

/* vm bundle for remote clients.  the last member is the hypercall cookie */
struct rump_spctl {
	struct vmspace spctl_vm;
	void *spctl;
};
#define RUMP_SPVM2CTL(vm) (((struct rump_spctl *)vm)->spctl)

void		rump_component_load(const struct rump_component *);
void		rump_component_init(enum rump_component_type);
int		rump_component_count(enum rump_component_type);

typedef void	(*rump_proc_vfs_init_fn)(struct proc *);
typedef void	(*rump_proc_vfs_release_fn)(struct proc *);
extern rump_proc_vfs_init_fn rump_proc_vfs_init;
extern rump_proc_vfs_release_fn rump_proc_vfs_release;

extern struct cpu_info *rump_cpu;

extern bool rump_ttycomponent;

struct lwp *	rump__lwproc_alloclwp(struct proc *);
void		rump__lwproc_lwphold(void);
void		rump__lwproc_lwprele(void);

void	rump_cpus_bootstrap(int *);
void	rump_biglock_init(void);
void	rump_scheduler_init(int);
void	rump_schedule(void);
void	rump_unschedule(void);
void 	rump_schedule_cpu(struct lwp *);
void 	rump_schedule_cpu_interlock(struct lwp *, void *);
void	rump_unschedule_cpu(struct lwp *);
void	rump_unschedule_cpu_interlock(struct lwp *, void *);
void	rump_unschedule_cpu1(struct lwp *, void *);
int	rump_syscall(int, void *, size_t, register_t *);

struct rump_onesyscall {
	int ros_num;
	sy_call_t *ros_handler;
};
void	rump_syscall_boot_establish(const struct rump_onesyscall *, size_t);

void	rump_schedlock_cv_wait(struct rumpuser_cv *);
int	rump_schedlock_cv_timedwait(struct rumpuser_cv *,
				    const struct timespec *);

void	rump_user_schedule(int, void *);
void	rump_user_unschedule(int, int *, void *);

void	rump_cpu_attach(struct cpu_info *);

struct clockframe *rump_cpu_makeclockframe(void);

void	rump_kernel_bigwrap(int *);
void	rump_kernel_bigunwrap(int);

void	rump_tsleep_init(void);

void	rump_intr_init(int);
void	rump_softint_run(struct cpu_info *);

void	*rump_hypermalloc(size_t, int, bool, const char *);
void	rump_hyperfree(void *, size_t);

void	rump_xc_highpri(struct cpu_info *);

void	rump_thread_init(void);
void	rump_thread_allow(struct lwp *);

void	rump_consdev_init(void);

void	rump_hyperentropy_init(void);

void	rump_lwproc_init(void);
void	rump_lwproc_curlwp_set(struct lwp *);
void	rump_lwproc_curlwp_clear(struct lwp *);
int	rump_lwproc_rfork_vmspace(struct vmspace *, int);

/*
 * sysproxy is an optional component.  The interfaces with "hyp"
 * in the name come into the rump kernel from the client or sysproxy
 * stub, the rest go out of the rump kernel.
 */
struct rump_sysproxy_ops {
	int (*rspo_copyin)(void *, const void *, void *, size_t);
	int (*rspo_copyinstr)(void *, const void *, void *, size_t *);
	int (*rspo_copyout)(void *, const void *, void *, size_t);
	int (*rspo_copyoutstr)(void *, const void *, void *, size_t *);
	int (*rspo_anonmmap)(void *, size_t, void **);
	int (*rspo_raise)(void *, int);
	void (*rspo_fini)(void *);

	pid_t (*rspo_hyp_getpid)(void);
	int (*rspo_hyp_syscall)(int, void *, long *);
	int (*rspo_hyp_rfork)(void *, int, const char *);
	void (*rspo_hyp_lwpexit)(void);
	void (*rspo_hyp_execnotify)(const char *);
};
extern struct rump_sysproxy_ops rump_sysproxy_ops;
#define rump_sysproxy_copyin(arg, raddr, laddr, len)			\
 	rump_sysproxy_ops.rspo_copyin(arg, raddr, laddr, len)
#define rump_sysproxy_copyinstr(arg, raddr, laddr, lenp)		\
 	rump_sysproxy_ops.rspo_copyinstr(arg, raddr, laddr, lenp)
#define rump_sysproxy_copyout(arg, laddr, raddr, len)			\
 	rump_sysproxy_ops.rspo_copyout(arg, laddr, raddr, len)
#define rump_sysproxy_copyoutstr(arg, laddr, raddr, lenp)		\
 	rump_sysproxy_ops.rspo_copyoutstr(arg, laddr, raddr, lenp)
#define rump_sysproxy_anonmmap(arg, howmuch, addr)			\
	rump_sysproxy_ops.rspo_anonmmap(arg, howmuch, addr)
#define rump_sysproxy_raise(arg, signo)					\
	rump_sysproxy_ops.rspo_raise(arg, signo)
#define rump_sysproxy_fini(arg)						\
	rump_sysproxy_ops.rspo_fini(arg)
#define rump_sysproxy_hyp_getpid()					\
	rump_sysproxy_ops.rspo_hyp_getpid()
#define rump_sysproxy_hyp_syscall(num, arg, retval)			\
	rump_sysproxy_ops.rspo_hyp_syscall(num, arg, retval)
#define rump_sysproxy_hyp_rfork(priv, flag, comm)			\
	rump_sysproxy_ops.rspo_hyp_rfork(priv, flag, comm)
#define rump_sysproxy_hyp_lwpexit()					\
	rump_sysproxy_ops.rspo_hyp_lwpexit()
#define rump_sysproxy_hyp_execnotify(comm)				\
	rump_sysproxy_ops.rspo_hyp_execnotify(comm)

#endif /* _SYS_RUMP_PRIVATE_H_ */
