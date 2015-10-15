/*	$NetBSD: systm.h,v 1.268 2015/08/28 07:18:40 knakahara Exp $	*/

/*-
 * Copyright (c) 1982, 1988, 1991, 1993
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
 *	@(#)systm.h	8.7 (Berkeley) 3/29/95
 */

#ifndef _SYS_SYSTM_H_
#define _SYS_SYSTM_H_

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#include "opt_multiprocessor.h"
#endif
#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <stdbool.h>
#endif

#include <machine/endian.h>

#include <sys/types.h>
#include <sys/stdarg.h>

#include <sys/device_if.h>

struct clockframe;
struct lwp;
struct proc;
struct sysent;
struct timeval;
struct tty;
struct uio;
struct vnode;
struct vmspace;

extern const char *panicstr;	/* panic message */
extern int doing_shutdown;	/* shutting down */

extern const char copyright[];	/* system copyright */
extern char machine[];		/* machine type */
extern char machine_arch[];	/* machine architecture */
extern const char osrelease[];	/* short system version */
extern const char ostype[];	/* system type */
extern const char kernel_ident[];/* kernel configuration ID */
extern const char version[];	/* system version */
extern const char buildinfo[];	/* information from build environment */

extern int autonicetime;        /* time (in seconds) before autoniceval */
extern int autoniceval;         /* proc priority after autonicetime */

extern int selwait;		/* select timeout address */

extern int maxmem;		/* max memory per process */
extern int physmem;		/* physical memory */

extern dev_t dumpdev;		/* dump device */
extern dev_t dumpcdev;		/* dump device (character equivalent) */
extern long dumplo;		/* offset into dumpdev */
extern int dumpsize;		/* size of dump in pages */
extern const char *dumpspec;	/* how dump device was specified */

extern dev_t rootdev;		/* root device */
extern struct vnode *rootvp;	/* vnode equivalent to above */
extern device_t root_device; /* device equivalent to above */
extern const char *rootspec;	/* how root device was specified */

extern int ncpu;		/* number of CPUs configured */
extern int ncpuonline;		/* number of CPUs online */
#if defined(_KERNEL)
extern bool mp_online;		/* secondary processors are started */
#endif /* defined(_KERNEL) */

extern const char hexdigits[];	/* "0123456789abcdef" in subr_prf.c */
extern const char HEXDIGITS[];	/* "0123456789ABCDEF" in subr_prf.c */

/*
 * These represent the swap pseudo-device (`sw').  This device
 * is used by the swap pager to indirect through the routines
 * in sys/vm/vm_swap.c.
 */
extern const dev_t swapdev;	/* swapping device */
extern struct vnode *swapdev_vp;/* vnode equivalent to above */

extern const dev_t zerodev;	/* /dev/zero */

typedef int	sy_call_t(struct lwp *, const void *, register_t *);

extern struct sysent {		/* system call table */
	short	sy_narg;	/* number of args */
	short	sy_argsize;	/* total size of arguments */
	int	sy_flags;	/* flags. see below */
	sy_call_t *sy_call;     /* implementing function */
	uint32_t sy_entry;	/* DTrace entry ID for systrace. */
	uint32_t sy_return;	/* DTrace return ID for systrace. */
} sysent[];
extern int nsysent;
#if	BYTE_ORDER == BIG_ENDIAN
#define	SCARG(p,k)	((p)->k.be.datum)	/* get arg from args pointer */
#elif	BYTE_ORDER == LITTLE_ENDIAN
#define	SCARG(p,k)	((p)->k.le.datum)	/* get arg from args pointer */
#else
#error	"what byte order is this machine?"
#endif

#define	SYCALL_INDIRECT	0x0000002 /* indirect (ie syscall() or __syscall()) */
#define	SYCALL_NARGS64_MASK	0x000f000 /* count of 64bit args */
#define SYCALL_RET_64	0x0010000 /* retval is a 64bit integer value */
#define SYCALL_ARG0_64  0x0020000
#define SYCALL_ARG1_64  0x0040000
#define SYCALL_ARG2_64  0x0080000
#define SYCALL_ARG3_64  0x0100000
#define SYCALL_ARG4_64  0x0200000
#define SYCALL_ARG5_64  0x0400000
#define SYCALL_ARG6_64  0x0800000
#define SYCALL_ARG7_64  0x1000000
#define SYCALL_NOSYS    0x2000000 /* permanent nosys in sysent[] */
#define	SYCALL_ARG_PTR	0x4000000 /* at least one argument is a pointer */
#define SYCALL_RET_64_P(sy)	((sy)->sy_flags & SYCALL_RET_64)
#define SYCALL_ARG_64_P(sy, n)	((sy)->sy_flags & (SYCALL_ARG0_64 << (n)))
#define	SYCALL_ARG_64_MASK(sy)	(((sy)->sy_flags >> 17) & 0xff)
#define	SYCALL_ARG_PTR_P(sy)	((sy)->sy_flags & SYCALL_ARG_PTR)
#define	SYCALL_NARGS64(sy)	(((sy)->sy_flags >> 12) & 0x0f)
#define	SYCALL_NARGS64_VAL(n)	((n) << 12)

extern int boothowto;		/* reboot flags, from console subsystem */
#define	bootverbose	(boothowto & AB_VERBOSE)
#define	bootquiet	(boothowto & AB_QUIET)

extern void (*v_putc)(int); /* Virtual console putc routine */

/*
 * General function declarations.
 */
void	voidop(void);
int	nullop(void *);
void*	nullret(void);
int	enodev(void);
int	enosys(void);
int	enoioctl(void);
int	enxio(void);
int	eopnotsupp(void);

enum hashtype {
	HASH_LIST,
	HASH_SLIST,
	HASH_TAILQ
};

#ifdef _KERNEL
void	*hashinit(u_int, enum hashtype, bool, u_long *);
void	hashdone(void *, enum hashtype, u_long);
int	seltrue(dev_t, int, struct lwp *);
int	sys_nosys(struct lwp *, const void *, register_t *);
int	sys_nomodule(struct lwp *, const void *, register_t *);

void	aprint_normal(const char *, ...) __printflike(1, 2);
void	aprint_error(const char *, ...) __printflike(1, 2);
void	aprint_naive(const char *, ...) __printflike(1, 2);
void	aprint_verbose(const char *, ...) __printflike(1, 2);
void	aprint_debug(const char *, ...) __printflike(1, 2);

void	device_printf(device_t, const char *fmt, ...) __printflike(2, 3);

void	aprint_normal_dev(device_t, const char *, ...) __printflike(2, 3);
void	aprint_error_dev(device_t, const char *, ...) __printflike(2, 3);
void	aprint_naive_dev(device_t, const char *, ...) __printflike(2, 3);
void	aprint_verbose_dev(device_t, const char *, ...) __printflike(2, 3);
void	aprint_debug_dev(device_t, const char *, ...) __printflike(2, 3);

struct ifnet;

void	aprint_normal_ifnet(struct ifnet *, const char *, ...)
    __printflike(2, 3);
void	aprint_error_ifnet(struct ifnet *, const char *, ...)
    __printflike(2, 3);
void	aprint_naive_ifnet(struct ifnet *, const char *, ...)
    __printflike(2, 3);
void	aprint_verbose_ifnet(struct ifnet *, const char *, ...)
    __printflike(2, 3);
void	aprint_debug_ifnet(struct ifnet *, const char *, ...)
    __printflike(2, 3);

int	aprint_get_error_count(void);

void	printf_tolog(const char *, ...) __printflike(1, 2);

void	printf_nolog(const char *, ...) __printflike(1, 2);

void	printf(const char *, ...) __printflike(1, 2);

int	snprintf(char *, size_t, const char *, ...) __printflike(3, 4);

void	vprintf(const char *, va_list) __printflike(1, 0);

int	vsnprintf(char *, size_t, const char *, va_list) __printflike(3, 0);

int	humanize_number(char *, size_t, uint64_t, const char *, int);

void	twiddle(void);
void	banner(void);
#endif /* _KERNEL */

void	panic(const char *, ...) __dead __printflike(1, 2);
void	vpanic(const char *, va_list) __dead __printflike(1, 0);
void	uprintf(const char *, ...) __printflike(1, 2);
void	uprintf_locked(const char *, ...) __printflike(1, 2);
void	ttyprintf(struct tty *, const char *, ...) __printflike(2, 3);

int	format_bytes(char *, size_t, uint64_t);

void	tablefull(const char *, const char *);

int	kcopy(const void *, void *, size_t);

#ifdef _KERNEL
#define bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define bzero(src, len)		memset((src), 0, (len))
#define bcmp(a, b, len)		memcmp((a), (b), (len))
#endif /* KERNEL */

int	copystr(const void *, void *, size_t, size_t *);
int	copyinstr(const void *, void *, size_t, size_t *);
int	copyoutstr(const void *, void *, size_t, size_t *);
int	copyin(const void *, void *, size_t);
int	copyout(const void *, void *, size_t);

#ifdef _KERNEL
typedef	int	(*copyin_t)(const void *, void *, size_t);
typedef int	(*copyout_t)(const void *, void *, size_t);
#endif

int	copyin_proc(struct proc *, const void *, void *, size_t);
int	copyout_proc(struct proc *, const void *, void *, size_t);
int	copyin_vmspace(struct vmspace *, const void *, void *, size_t);
int	copyout_vmspace(struct vmspace *, const void *, void *, size_t);

int	ioctl_copyin(int ioctlflags, const void *src, void *dst, size_t len);
int	ioctl_copyout(int ioctlflags, const void *src, void *dst, size_t len);

int	ucas_ptr(volatile void *, void *, void *, void *);
int	ucas_int(volatile int *, int, int, int *);

int	subyte(void *, int);
int	suibyte(void *, int);
int	susword(void *, short);
int	suisword(void *, short);
int	suswintr(void *, short);
int	suword(void *, long);
int	suiword(void *, long);

int	fubyte(const void *);
int	fuibyte(const void *);
int	fusword(const void *);
int	fuisword(const void *);
int	fuswintr(const void *);
long	fuword(const void *);
long	fuiword(const void *);

void	hardclock(struct clockframe *);
void	softclock(void *);
void	statclock(struct clockframe *);

#ifdef NTP
void	ntp_init(void);
#ifdef PPS_SYNC
struct timespec;
void	hardpps(struct timespec *, long);
#endif /* PPS_SYNC */
#else
void	ntp_init(void);	/* also provides adjtime() functionality */
#endif /* NTP */

void	ssp_init(void);

void	initclocks(void);
void	inittodr(time_t);
void	resettodr(void);
void	cpu_initclocks(void);
void	setrootfstime(time_t);

void	startprofclock(struct proc *);
void	stopprofclock(struct proc *);
void	proftick(struct clockframe *);
void	setstatclockrate(int);

/*
 * Critical polling hooks.  Functions to be run while the kernel stays
 * elevated IPL for a "long" time.  (watchdogs).
 */
void	*critpollhook_establish(void (*)(void *), void *);
void	critpollhook_disestablish(void *);
void	docritpollhooks(void);

/*
 * Shutdown hooks.  Functions to be run with all interrupts disabled
 * immediately before the system is halted or rebooted.
 */
void	*shutdownhook_establish(void (*)(void *), void *);
void	shutdownhook_disestablish(void *);
void	doshutdownhooks(void);

/*
 * Power management hooks.
 */
void	*powerhook_establish(const char *, void (*)(int, void *), void *);
void	powerhook_disestablish(void *);
void	dopowerhooks(int);
#define PWR_RESUME	0
#define PWR_SUSPEND	1
#define PWR_STANDBY	2
#define PWR_SOFTRESUME	3
#define PWR_SOFTSUSPEND	4
#define PWR_SOFTSTANDBY	5
#define PWR_NAMES \
	"resume",	/* 0 */ \
	"suspend",	/* 1 */ \
	"standby",	/* 2 */ \
	"softresume",	/* 3 */ \
	"softsuspend",	/* 4 */ \
	"softstandby"	/* 5 */

/*
 * Mountroot hooks (and mountroot declaration).  Device drivers establish
 * these to be executed just before (*mountroot)() if the passed device is
 * selected as the root device.
 */

#define	ROOT_FSTYPE_ANY	"?"

extern const char *rootfstype;
void	*mountroothook_establish(void (*)(device_t), device_t);
void	mountroothook_disestablish(void *);
void	mountroothook_destroy(void);
void	domountroothook(device_t);

/*
 * Exec hooks. Subsystems may want to do cleanup when a process
 * execs.
 */
void	*exechook_establish(void (*)(struct proc *, void *), void *);
void	exechook_disestablish(void *);
void	doexechooks(struct proc *);

/*
 * Exit hooks. Subsystems may want to do cleanup when a process exits.
 */
void	*exithook_establish(void (*)(struct proc *, void *), void *);
void	exithook_disestablish(void *);
void	doexithooks(struct proc *);

/*
 * Fork hooks.  Subsystems may want to do special processing when a process
 * forks.
 */
void	*forkhook_establish(void (*)(struct proc *, struct proc *));
void	forkhook_disestablish(void *);
void	doforkhooks(struct proc *, struct proc *);

/*
 * kernel syscall tracing/debugging hooks.
 */
#ifdef _KERNEL
bool	trace_is_enabled(struct proc *);
int	trace_enter(register_t, const struct sysent *, const void *);
void	trace_exit(register_t, const struct sysent *, const void *,
    register_t [], int);
#endif

int	uiomove(void *, size_t, struct uio *);
int	uiomove_frombuf(void *, size_t, struct uio *);

#ifdef _KERNEL
int	setjmp(label_t *) __returns_twice;
void	longjmp(label_t *) __dead;
#endif

void	consinit(void);

void	cpu_startup(void);
void	cpu_configure(void);
void	cpu_bootconf(void);
void	cpu_rootconf(void);
void	cpu_dumpconf(void);

#ifdef GPROF
void	kmstartup(void);
#endif

void	machdep_init(void);

#ifdef _KERNEL
#include <lib/libkern/libkern.h>

/*
 * Stuff to handle debugger magic key sequences.
 */
#define CNS_LEN			128
#define CNS_MAGIC_VAL(x)	((x)&0x1ff)
#define CNS_MAGIC_NEXT(x)	(((x)>>9)&0x7f)
#define CNS_TERM		0x7f	/* End of sequence */

typedef struct cnm_state {
	int	cnm_state;
	u_short	*cnm_magic;
} cnm_state_t;

/* Override db_console() in MD headers */
#ifndef cn_trap
#define cn_trap()	console_debugger()
#endif
#ifndef cn_isconsole
#define cn_isconsole(d)	(cn_tab != NULL && (d) == cn_tab->cn_dev)
#endif

void cn_init_magic(cnm_state_t *);
void cn_destroy_magic(cnm_state_t *);
int cn_set_magic(const char *);
int cn_get_magic(char *, size_t);
/* This should be called for each byte read */
#ifndef cn_check_magic
#define cn_check_magic(d, k, s)						\
	do {								\
		if (cn_isconsole(d)) {					\
			int _v = (s).cnm_magic[(s).cnm_state];		\
			if ((k) == CNS_MAGIC_VAL(_v)) {			\
				(s).cnm_state = CNS_MAGIC_NEXT(_v);	\
				if ((s).cnm_state == CNS_TERM) {	\
					cn_trap();			\
					(s).cnm_state = 0;		\
				}					\
			} else {					\
				(s).cnm_state = 0;			\
			}						\
		}							\
	} while (/* CONSTCOND */ 0)
#endif

/* Encode out-of-band events this way when passing to cn_check_magic() */
#define	CNC_BREAK		0x100

#if defined(DDB) || defined(sun3) || defined(sun2)
/* note that cpu_Debugger() is always available on sun[23] */
void	cpu_Debugger(void);
#define Debugger	cpu_Debugger
#endif

#ifdef DDB
/*
 * Enter debugger(s) from console attention if enabled
 */
extern int db_fromconsole; /* XXX ddb/ddbvar.h */
#define console_debugger() if (db_fromconsole) Debugger()
#elif defined(Debugger)
#define console_debugger() Debugger()
#else
#define console_debugger() do {} while (/* CONSTCOND */ 0) /* NOP */
#endif
#endif /* _KERNEL */

/* For SYSCALL_DEBUG */
void scdebug_call(register_t, const register_t[]);
void scdebug_ret(register_t, int, const register_t[]);

void	kernel_lock_init(void);
void	_kernel_lock(int);
void	_kernel_unlock(int, int *);
bool	_kernel_locked_p(void);

#ifdef _KERNEL
void	kernconfig_lock_init(void);
void	kernconfig_lock(void);
void	kernconfig_unlock(void);
bool	kernconfig_is_held(void);
#endif

#if defined(MULTIPROCESSOR) || defined(_MODULE)
#define	KERNEL_LOCK(count, lwp)			\
do {						\
	if ((count) != 0)			\
		_kernel_lock((count));	\
} while (/* CONSTCOND */ 0)
#define	KERNEL_UNLOCK(all, lwp, p)	_kernel_unlock((all), (p))
#define	KERNEL_LOCKED_P()		_kernel_locked_p()
#else
#define	KERNEL_LOCK(count, lwp)		do {(void)(count); (void)(lwp);} while (/* CONSTCOND */ 0) /*NOP*/
#define	KERNEL_UNLOCK(all, lwp, ptr)	do {(void)(all); (void)(lwp); (void)(ptr);} while (/* CONSTCOND */ 0) /*NOP*/
#define	KERNEL_LOCKED_P()		(true)
#endif

#define	KERNEL_UNLOCK_LAST(l)		KERNEL_UNLOCK(-1, (l), NULL)
#define	KERNEL_UNLOCK_ALL(l, p)		KERNEL_UNLOCK(0, (l), (p))
#define	KERNEL_UNLOCK_ONE(l)		KERNEL_UNLOCK(1, (l), NULL)

/* Preemption control. */
#ifdef _KERNEL
void	kpreempt_disable(void);
void	kpreempt_enable(void);
bool	kpreempt_disabled(void);
#endif

void assert_sleepable(void);
#if defined(DEBUG)
#define	ASSERT_SLEEPABLE()	assert_sleepable()
#else /* defined(DEBUG) */
#define	ASSERT_SLEEPABLE()	do {} while (0)
#endif /* defined(DEBUG) */

vaddr_t calc_cache_size(vsize_t , int, int);

#endif	/* !_SYS_SYSTM_H_ */
