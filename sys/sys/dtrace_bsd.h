/*	$NetBSD: dtrace_bsd.h,v 1.7 2012/12/02 01:05:16 chs Exp $	*/

/*-
 * Copyright (c) 2007-2008 John Birrell (jb@freebsd.org)
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
 *
 * $FreeBSD: src/sys/sys/dtrace_bsd.h,v 1.3.2.1 2009/08/03 08:13:06 kensmith Exp $
 *
 * This file contains BSD shims for Sun's DTrace code.
 */

#ifndef _SYS_DTRACE_BSD_H
#define	_SYS_DTRACE_BSD_H

#if defined(_KERNEL_OPT)
#include "opt_dtrace.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/proc.h>

/* Forward definitions: */
struct mbuf;
struct trapframe;
struct lwp;
struct vattr;
struct vnode;
struct ucred;

/*
 * Cyclic clock function type definition used to hook the cyclic
 * subsystem into the appropriate timer interrupt.
 */
typedef	void (*cyclic_clock_func_t)(struct clockframe *);
extern cyclic_clock_func_t	cyclic_clock_func[];

/*
 * The dtrace module handles traps that occur during a DTrace probe.
 * This type definition is used in the trap handler to provide a
 * hook for the dtrace module to register it's handler with.
 */
typedef int (*dtrace_trap_func_t)(struct trapframe *, u_int);

int	dtrace_trap(struct trapframe *, u_int);

extern dtrace_trap_func_t	dtrace_trap_func;

/* Used by the machine dependent trap() code. */
typedef	int (*dtrace_invop_func_t)(uintptr_t, uintptr_t *, uintptr_t);
typedef void (*dtrace_doubletrap_func_t)(void);

/* Global variables in trap.c */
extern	dtrace_invop_func_t	dtrace_invop_func;
extern	dtrace_doubletrap_func_t	dtrace_doubletrap_func;

/* Virtual time hook function type. */
typedef	void (*dtrace_vtime_switch_func_t)(struct lwp *);

extern int			dtrace_vtime_active;
extern dtrace_vtime_switch_func_t	dtrace_vtime_switch_func;

/* The fasttrap module hooks into the fork, exit and exit. */
typedef void (*dtrace_fork_func_t)(struct proc *, struct proc *);
typedef void (*dtrace_execexit_func_t)(struct proc *);

/* Global variable in kern_fork.c */
extern dtrace_fork_func_t	dtrace_fasttrap_fork;

/* Global variable in kern_exec.c */
extern dtrace_execexit_func_t	dtrace_fasttrap_exec;

/* Global variable in kern_exit.c */
extern dtrace_execexit_func_t	dtrace_fasttrap_exit;

/* The dtmalloc provider hooks into malloc. */
typedef	void (*dtrace_malloc_probe_func_t)(u_int32_t, uintptr_t arg0,
    uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

extern dtrace_malloc_probe_func_t   dtrace_malloc_probe;

/* dtnfsclient NFSv3 access cache provider hooks. */
typedef void (*dtrace_nfsclient_accesscache_flush_probe_func_t)(uint32_t,
    struct vnode *);
extern dtrace_nfsclient_accesscache_flush_probe_func_t
    dtrace_nfsclient_accesscache_flush_done_probe;

typedef void (*dtrace_nfsclient_accesscache_get_probe_func_t)(uint32_t,
    struct vnode *, uid_t, uint32_t);
extern dtrace_nfsclient_accesscache_get_probe_func_t
    dtrace_nfsclient_accesscache_get_hit_probe,
    dtrace_nfsclient_accesscache_get_miss_probe;

typedef void (*dtrace_nfsclient_accesscache_load_probe_func_t)(uint32_t,
    struct vnode *, uid_t, uint32_t, int);
extern dtrace_nfsclient_accesscache_load_probe_func_t
    dtrace_nfsclient_accesscache_load_done_probe;

/* dtnfsclient NFSv[23] attribute cache provider hooks. */
typedef void (*dtrace_nfsclient_attrcache_flush_probe_func_t)(uint32_t,
    struct vnode *);
extern dtrace_nfsclient_attrcache_flush_probe_func_t
    dtrace_nfsclient_attrcache_flush_done_probe;

typedef void (*dtrace_nfsclient_attrcache_get_hit_probe_func_t)(uint32_t,
    struct vnode *, struct vattr *);
extern dtrace_nfsclient_attrcache_get_hit_probe_func_t
    dtrace_nfsclient_attrcache_get_hit_probe;

typedef void (*dtrace_nfsclient_attrcache_get_miss_probe_func_t)(uint32_t,
    struct vnode *);
extern dtrace_nfsclient_attrcache_get_miss_probe_func_t
    dtrace_nfsclient_attrcache_get_miss_probe;

typedef void (*dtrace_nfsclient_attrcache_load_probe_func_t)(uint32_t,
    struct vnode *, struct vattr *, int);
extern dtrace_nfsclient_attrcache_load_probe_func_t
    dtrace_nfsclient_attrcache_load_done_probe;

/* dtnfsclient NFSv[23] RPC provider hooks. */
typedef void (*dtrace_nfsclient_nfs23_start_probe_func_t)(uint32_t,
    struct vnode *, struct mbuf *, struct ucred *, int);
extern dtrace_nfsclient_nfs23_start_probe_func_t
    dtrace_nfsclient_nfs23_start_probe;

typedef void (*dtrace_nfsclient_nfs23_done_probe_func_t)(uint32_t,
    struct vnode *, struct mbuf *, struct ucred *, int, int);
extern dtrace_nfsclient_nfs23_done_probe_func_t
    dtrace_nfsclient_nfs23_done_probe;

/*
 * OpenSolaris compatible time functions returning nanoseconds.
 * On OpenSolaris these return hrtime_t which we define as uint64_t.
 */
uint64_t	dtrace_gethrtime(void);
uint64_t	dtrace_gethrestime(void);

/* sizes based on DTrace structure requirements */
#define KDTRACE_PROC_SIZE	64
#define KDTRACE_PROC_ZERO	8
#define	KDTRACE_THREAD_SIZE	256
#define	KDTRACE_THREAD_ZERO	64

/*
 * Functions for managing the opaque DTrace memory areas for 
 * processes and lwps.
 */

static inline size_t	kdtrace_proc_size(void);
static inline void kdtrace_proc_ctor(void *, struct proc *);
static inline void kdtrace_proc_dtor(void *, struct proc *);
static inline size_t	kdtrace_thread_size(void);
static inline void kdtrace_thread_ctor(void *, struct lwp *);
static inline void kdtrace_thread_dtor(void *, struct lwp *);


/* Return the DTrace process data size compiled in the kernel hooks. */
static inline size_t
kdtrace_proc_size(void)
{

	return KDTRACE_PROC_SIZE;
}

/* Return the DTrace thread data size compiled in the kernel hooks. */
static inline size_t
kdtrace_thread_size(void)
{

	return KDTRACE_THREAD_SIZE;
}

static inline void
kdtrace_proc_ctor(void *arg, struct proc *p)
{

#ifdef KDTRACE_HOOKS
	p->p_dtrace = kmem_zalloc(KDTRACE_PROC_SIZE, KM_SLEEP);
#endif
}

static inline void
kdtrace_proc_dtor(void *arg, struct proc *p)
{

#ifdef KDTRACE_HOOKS
	if (p->p_dtrace != NULL) {
		kmem_free(p->p_dtrace, KDTRACE_PROC_SIZE);
		p->p_dtrace = NULL;
	}
#endif
}

static inline void
kdtrace_thread_ctor(void *arg, struct lwp *l)
{

#ifdef KDTRACE_HOOKS
	l->l_dtrace = kmem_zalloc(KDTRACE_THREAD_SIZE, KM_SLEEP);
#endif
}

static inline void
kdtrace_thread_dtor(void *arg, struct lwp *l)
{

#ifdef KDTRACE_HOOKS
	if (l->l_dtrace != NULL) {
		kmem_free(l->l_dtrace, KDTRACE_THREAD_SIZE);
		l->l_dtrace = NULL;
	}
#endif
}

#endif /* _SYS_DTRACE_BSD_H */
