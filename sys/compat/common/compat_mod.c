/*	$NetBSD: compat_mod.c,v 1.22 2015/05/11 10:32:13 pgoyette Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software developed for The NetBSD Foundation
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

/*
 * Linkage for the compat module: spaghetti.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: compat_mod.c,v 1.22 2015/05/11 10:32:13 pgoyette Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_compat_43.h"
#include "opt_ntp.h"
#include "opt_sysv.h"
#include "opt_lfs.h"
#endif

#include <sys/systm.h>
#include <sys/module.h>
#include <sys/rwlock.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/syscallvar.h>
#include <sys/sysctl.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_object.h>

#include <compat/common/compat_util.h>
#include <compat/common/compat_mod.h>

#if defined(COMPAT_09) || defined(COMPAT_43) || defined(COMPAT_50)
static struct sysctllog *compat_clog = NULL;
#endif
 
MODULE(MODULE_CLASS_EXEC, compat, NULL);

int	ttcompat(struct tty *, u_long, void *, int, struct lwp *);

#ifdef _MODULE
#ifdef COMPAT_16
#if !defined(__amd64__) || defined(COMPAT_NETBSD32)
#define COMPAT_SIGCONTEXT
extern char sigcode[], esigcode[];
struct uvm_object *emul_netbsd_object;
#endif
#endif
#endif /* _MODULE */

extern krwlock_t exec_lock;
extern krwlock_t ttcompat_lock;

static const struct syscall_package compat_syscalls[] = {
#if defined(COMPAT_43)
	{ SYS_compat_43_fstat43, 0, (sy_call_t *)compat_43_sys_fstat },
	{ SYS_compat_43_lstat43, 0, (sy_call_t *)compat_43_sys_lstat },
	{ SYS_compat_43_oaccept, 0, (sy_call_t *)compat_43_sys_accept },
	{ SYS_compat_43_ocreat, 0, (sy_call_t *)compat_43_sys_creat },
	{ SYS_compat_43_oftruncate, 0, (sy_call_t *)compat_43_sys_ftruncate },
	{ SYS_compat_43_ogetdirentries, 0, (sy_call_t *)compat_43_sys_getdirentries },
	{ SYS_compat_43_ogetdtablesize, 0, (sy_call_t *)compat_43_sys_getdtablesize },
	{ SYS_compat_43_ogethostid, 0, (sy_call_t *)compat_43_sys_gethostid },
	{ SYS_compat_43_ogethostname, 0, (sy_call_t *)compat_43_sys_gethostname },
	{ SYS_compat_43_ogetkerninfo, 0, (sy_call_t *)compat_43_sys_getkerninfo },
	{ SYS_compat_43_ogetpagesize, 0, (sy_call_t *)compat_43_sys_getpagesize },
	{ SYS_compat_43_ogetpeername, 0, (sy_call_t *)compat_43_sys_getpeername },
	{ SYS_compat_43_ogetrlimit, 0, (sy_call_t *)compat_43_sys_getrlimit },
	{ SYS_compat_43_ogetsockname, 0, (sy_call_t *)compat_43_sys_getsockname },
	{ SYS_compat_43_okillpg, 0, (sy_call_t *)compat_43_sys_killpg },
	{ SYS_compat_43_olseek, 0, (sy_call_t *)compat_43_sys_lseek },
	{ SYS_compat_43_ommap, 0, (sy_call_t *)compat_43_sys_mmap },
	{ SYS_compat_43_oquota, 0, (sy_call_t *)compat_43_sys_quota },
	{ SYS_compat_43_orecv, 0, (sy_call_t *)compat_43_sys_recv },
	{ SYS_compat_43_orecvfrom, 0, (sy_call_t *)compat_43_sys_recvfrom },
	{ SYS_compat_43_orecvmsg, 0, (sy_call_t *)compat_43_sys_recvmsg },
	{ SYS_compat_43_osend, 0, (sy_call_t *)compat_43_sys_send },
	{ SYS_compat_43_osendmsg, 0, (sy_call_t *)compat_43_sys_sendmsg },
	{ SYS_compat_43_osethostid, 0, (sy_call_t *)compat_43_sys_sethostid },
	{ SYS_compat_43_osethostname, 0, (sy_call_t *)compat_43_sys_sethostname },
	{ SYS_compat_43_osetrlimit, 0, (sy_call_t *)compat_43_sys_setrlimit },
	{ SYS_compat_43_osigblock, 0, (sy_call_t *)compat_43_sys_sigblock },
	{ SYS_compat_43_osigsetmask, 0, (sy_call_t *)compat_43_sys_sigsetmask },
	{ SYS_compat_43_osigstack, 0, (sy_call_t *)compat_43_sys_sigstack },
	{ SYS_compat_43_osigvec, 0, (sy_call_t *)compat_43_sys_sigvec },
	{ SYS_compat_43_otruncate, 0, (sy_call_t *)compat_43_sys_truncate },
	{ SYS_compat_43_owait, 0, (sy_call_t *)compat_43_sys_wait },
	{ SYS_compat_43_stat43, 0, (sy_call_t *)compat_43_sys_stat },
#endif

#if defined(COMPAT_09)
	{ SYS_compat_09_ogetdomainname, 0, (sy_call_t *)compat_09_sys_getdomainname },
	{ SYS_compat_09_osetdomainname, 0, (sy_call_t *)compat_09_sys_setdomainname },
	{ SYS_compat_09_ouname, 0, (sy_call_t *)compat_09_sys_uname },
#endif

#if defined(COMPAT_10) && !defined(_LP64)
# if defined(SYSVMSG)
	{ SYS_compat_10_omsgsys, 0, (sy_call_t *)compat_10_sys_msgsys },
# endif
# if defined(SYSVSEM)
	{ SYS_compat_10_osemsys, 0, (sy_call_t *)compat_10_sys_semsys },
# endif
# if defined(SYSVSHM)
	{ SYS_compat_10_oshmsys, 0, (sy_call_t *)compat_10_sys_shmsys },
# endif
#endif	/* defined(COMPAT_10) && !defined(_LP64) */

#if defined(COMPAT_12)
	{ SYS_compat_12_fstat12, 0, (sy_call_t *)compat_12_sys_fstat },
	{ SYS_compat_12_getdirentries, 0, (sy_call_t *)compat_12_sys_getdirentries },
	{ SYS_compat_12_lstat12, 0, (sy_call_t *)compat_12_sys_lstat },
	{ SYS_compat_12_msync, 0, (sy_call_t *)compat_12_sys_msync },
	{ SYS_compat_12_oreboot, 0, (sy_call_t *)compat_12_sys_reboot },
	{ SYS_compat_12_oswapon, 0, (sy_call_t *)compat_12_sys_swapon },
	{ SYS_compat_12_stat12, 0, (sy_call_t *)compat_12_sys_stat },
#endif

#if defined(COMPAT_13)
	{ SYS_compat_13_sigaction13, 0, (sy_call_t *)compat_13_sys_sigaction },
	{ SYS_compat_13_sigaltstack13, 0, (sy_call_t *)compat_13_sys_sigaltstack },
	{ SYS_compat_13_sigpending13, 0, (sy_call_t *)compat_13_sys_sigpending },
	{ SYS_compat_13_sigprocmask13, 0, (sy_call_t *)compat_13_sys_sigprocmask },
	{ SYS_compat_13_sigreturn13, 0, (sy_call_t *)compat_13_sys_sigreturn },
	{ SYS_compat_13_sigsuspend13, 0, (sy_call_t *)compat_13_sys_sigsuspend },
#endif

#if defined(COMPAT_16)
#if defined(COMPAT_SIGCONTEXT)
	{ SYS_compat_16___sigaction14, 0, (sy_call_t *)compat_16_sys___sigaction14 },
	{ SYS_compat_16___sigreturn14, 0, (sy_call_t *)compat_16_sys___sigreturn14 },
#endif
#endif

#if defined(COMPAT_20)
	{ SYS_compat_20_fhstatfs, 0, (sy_call_t *)compat_20_sys_fhstatfs },
	{ SYS_compat_20_fstatfs, 0, (sy_call_t *)compat_20_sys_fstatfs },
	{ SYS_compat_20_getfsstat, 0, (sy_call_t *)compat_20_sys_getfsstat },
	{ SYS_compat_20_statfs, 0, (sy_call_t *)compat_20_sys_statfs },
#endif

#if defined(COMPAT_30)
	{ SYS_compat_30___fhstat30, 0, (sy_call_t *)compat_30_sys___fhstat30 },
	{ SYS_compat_30___fstat13, 0, (sy_call_t *)compat_30_sys___fstat13 },
	{ SYS_compat_30___lstat13, 0, (sy_call_t *)compat_30_sys___lstat13 },
	{ SYS_compat_30___stat13, 0, (sy_call_t *)compat_30_sys___stat13 },
	{ SYS_compat_30_fhopen, 0, (sy_call_t *)compat_30_sys_fhopen },
	{ SYS_compat_30_fhstat, 0, (sy_call_t *)compat_30_sys_fhstat },
	{ SYS_compat_30_fhstatvfs1, 0, (sy_call_t *)compat_30_sys_fhstatvfs1 },
	{ SYS_compat_30_getdents, 0, (sy_call_t *)compat_30_sys_getdents },
	{ SYS_compat_30_getfh, 0, (sy_call_t *)compat_30_sys_getfh },
	{ SYS_compat_30_socket, 0, (sy_call_t *)compat_30_sys_socket },
#endif

#if defined(COMPAT_40)
	{ SYS_compat_40_mount, 0, (sy_call_t *)compat_40_sys_mount },
#endif
#if defined(COMPAT_50)
	{ SYS_compat_50_wait4, 0, (sy_call_t *)compat_50_sys_wait4 },
	{ SYS_compat_50_mknod, 0, (sy_call_t *)compat_50_sys_mknod },
	{ SYS_compat_50_setitimer, 0, (sy_call_t *)compat_50_sys_setitimer },
	{ SYS_compat_50_getitimer, 0, (sy_call_t *)compat_50_sys_getitimer },
	{ SYS_compat_50_select, 0, (sy_call_t *)compat_50_sys_select },
	{ SYS_compat_50_gettimeofday, 0, (sy_call_t *)compat_50_sys_gettimeofday },
	{ SYS_compat_50_getrusage, 0, (sy_call_t *)compat_50_sys_getrusage },
	{ SYS_compat_50_settimeofday, 0, (sy_call_t *)compat_50_sys_settimeofday },
	{ SYS_compat_50_utimes, 0, (sy_call_t *)compat_50_sys_utimes },
	{ SYS_compat_50_adjtime, 0, (sy_call_t *)compat_50_sys_adjtime },
#ifdef LFS
	{ SYS_compat_50_lfs_segwait, 0, (sy_call_t *)compat_50_sys_lfs_segwait },
#endif
	{ SYS_compat_50_futimes, 0, (sy_call_t *)compat_50_sys_futimes },
	{ SYS_compat_50_clock_gettime, 0, (sy_call_t *)compat_50_sys_clock_gettime },
	{ SYS_compat_50_clock_settime, 0, (sy_call_t *)compat_50_sys_clock_settime },
	{ SYS_compat_50_clock_getres, 0, (sy_call_t *)compat_50_sys_clock_getres },
	{ SYS_compat_50_timer_settime, 0, (sy_call_t *)compat_50_sys_timer_settime },
	{ SYS_compat_50_timer_gettime, 0, (sy_call_t *)compat_50_sys_timer_gettime },
	{ SYS_compat_50_nanosleep, 0, (sy_call_t *)compat_50_sys_nanosleep },
	{ SYS_compat_50___sigtimedwait, 0, (sy_call_t *)compat_50_sys___sigtimedwait },
	{ SYS_compat_50_mq_timedsend, 0, (sy_call_t *)compat_50_sys_mq_timedsend },
	{ SYS_compat_50_mq_timedreceive, 0, (sy_call_t *)compat_50_sys_mq_timedreceive },
	{ SYS_compat_50_lutimes, 0, (sy_call_t *)compat_50_sys_lutimes },
	{ SYS_compat_50__lwp_park, 0, (sy_call_t *)compat_50_sys__lwp_park },
	{ SYS_compat_50_kevent, 0, (sy_call_t *)compat_50_sys_kevent },
	{ SYS_compat_50_pselect, 0, (sy_call_t *)compat_50_sys_pselect },
	{ SYS_compat_50_pollts, 0, (sy_call_t *)compat_50_sys_pollts },
	{ SYS_compat_50___stat30, 0, (sy_call_t *)compat_50_sys___stat30 },
	{ SYS_compat_50___fstat30, 0, (sy_call_t *)compat_50_sys___fstat30 },
	{ SYS_compat_50___lstat30, 0, (sy_call_t *)compat_50_sys___lstat30 },
# if defined(NTP)
	{ SYS_compat_50___ntp_gettime30, 0, (sy_call_t *)compat_50_sys___ntp_gettime30 },
# endif
	{ SYS_compat_50___fhstat40, 0, (sy_call_t *)compat_50_sys___fhstat40 },
	{ SYS_compat_50_aio_suspend, 0, (sy_call_t *)compat_50_sys_aio_suspend },
	{ SYS_compat_50_quotactl, 0, (sy_call_t *)compat_50_sys_quotactl },
#endif
#if defined(COMPAT_60)
	{ SYS_compat_60__lwp_park, 0, (sy_call_t *)compat_60_sys__lwp_park },
#endif
	{ 0, 0, NULL },
};

static int
compat_modcmd(modcmd_t cmd, void *arg)
{
#ifdef COMPAT_16
	proc_t *p;
#endif
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = syscall_establish(NULL, compat_syscalls);
		if (error != 0) {
			return error;
		}
#ifdef COMPAT_43
		KASSERT(ttcompatvec == NULL);
		ttcompatvec = ttcompat;
#endif
#ifdef COMPAT_16
#if defined(COMPAT_SIGCONTEXT)
		KASSERT(emul_netbsd.e_sigobject == NULL);
		rw_enter(&exec_lock, RW_WRITER);
		emul_netbsd.e_sigcode = sigcode;
		emul_netbsd.e_esigcode = esigcode;
		emul_netbsd.e_sigobject = &emul_netbsd_object;
		rw_exit(&exec_lock);
		KASSERT(sendsig_sigcontext_vec == NULL);
		sendsig_sigcontext_vec = sendsig_sigcontext;
#endif
#endif
		compat_sysctl_init();
		return 0;

	case MODULE_CMD_FINI:
#ifdef COMPAT_16
		/*
		 * Ensure sendsig_sigcontext() is not being used.
		 * module_lock prevents the flag being set on any
		 * further processes while we are here.  See
		 * sigaction1() for the opposing half.
		 */
		mutex_enter(proc_lock);
		PROCLIST_FOREACH(p, &allproc) {
			if ((p->p_lflag & PL_SIGCOMPAT) != 0) {
				break;
			}
		}
		mutex_exit(proc_lock);
		if (p != NULL) {
			return EBUSY;
		}
		sendsig_sigcontext_vec = NULL;
#endif
		/* Unlink the system calls. */
		error = syscall_disestablish(NULL, compat_syscalls);
		if (error != 0) {
			return error;
		}
#ifdef COMPAT_43
		/* Unlink ttcompatvec. */
		if (rw_tryenter(&ttcompat_lock, RW_WRITER)) {
			ttcompatvec = NULL;
			rw_exit(&ttcompat_lock);
		} else {
			error = syscall_establish(NULL, compat_syscalls);
			KASSERT(error == 0);
			return EBUSY;
		}
#endif
#ifdef COMPAT_16
#if defined(COMPAT_SIGCONTEXT)
		/*
		 * The sigobject may persist if still in use, but
		 * is reference counted so will die eventually.
		 */
		rw_enter(&exec_lock, RW_WRITER);
		if (emul_netbsd_object != NULL) {
			(*emul_netbsd_object->pgops->pgo_detach)
			    (emul_netbsd_object);
		}
		emul_netbsd_object = NULL;
		emul_netbsd.e_sigcode = NULL;
		emul_netbsd.e_esigcode = NULL;
		emul_netbsd.e_sigobject = NULL;
		rw_exit(&exec_lock);
#endif
#endif	/* COMPAT_16 */
		compat_sysctl_fini();
		return 0;

	default:
		return ENOTTY;
	}
}

void
compat_sysctl_init(void)
{

#if defined(COMPAT_09) || defined(COMPAT_43)
	compat_sysctl_vfs(&compat_clog);
#endif
#if defined(COMPAT_50)
	compat_sysctl_time(&compat_clog);
#endif
}

void
compat_sysctl_fini(void)
{
 
#if defined(COMPAT_09) || defined(COMPAT_43) || defined(COMPAT_50)
        sysctl_teardown(&compat_clog);
#endif
}
