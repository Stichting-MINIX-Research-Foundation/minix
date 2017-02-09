/*	$NetBSD: init_sysctl.c,v 1.209 2015/08/25 14:52:31 pooka Exp $ */

/*-
 * Copyright (c) 2003, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown, and by Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: init_sysctl.c,v 1.209 2015/08/25 14:52:31 pooka Exp $");

#include "opt_sysv.h"
#include "opt_compat_netbsd.h"
#include "opt_modular.h"
#include "pty.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/cpu.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/unistd.h>
#include <sys/disklabel.h>
#include <sys/cprng.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <dev/cons.h>
#include <sys/socketvar.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/kmem.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/exec.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/stat.h>
#include <sys/kauth.h>
#include <sys/ktrace.h>
#include <sys/ksem.h>

#include <sys/cpu.h>

int security_setidcore_dump;
char security_setidcore_path[MAXPATHLEN] = "/var/crash/%n.core";
uid_t security_setidcore_owner = 0;
gid_t security_setidcore_group = 0;
mode_t security_setidcore_mode = (S_IRUSR|S_IWUSR);

/*
 * Current status of SysV IPC capability.  Initially, these are
 * 0 if the capability is not built-in to the kernel, but can
 * be updated if the appropriate kernel module is (auto)loaded.
 */

int kern_has_sysvmsg = 0;
int kern_has_sysvshm = 0;
int kern_has_sysvsem = 0;

static const u_int sysctl_lwpprflagmap[] = {
	LPR_DETACHED, L_DETACHED,
	0
};

/*
 * try over estimating by 5 procs/lwps
 */
#define KERN_LWPSLOP	(5 * sizeof(struct kinfo_lwp))

static int dcopyout(struct lwp *, const void *, void *, size_t);

static int
dcopyout(struct lwp *l, const void *kaddr, void *uaddr, size_t len)
{
	int error;

	error = copyout(kaddr, uaddr, len);
	ktrmibio(-1, UIO_READ, uaddr, len, error);

	return error;
}

#ifdef DIAGNOSTIC
static int sysctl_kern_trigger_panic(SYSCTLFN_PROTO);
#endif
static int sysctl_kern_maxvnodes(SYSCTLFN_PROTO);
static int sysctl_kern_rtc_offset(SYSCTLFN_PROTO);
static int sysctl_kern_maxproc(SYSCTLFN_PROTO);
static int sysctl_kern_hostid(SYSCTLFN_PROTO);
static int sysctl_kern_defcorename(SYSCTLFN_PROTO);
static int sysctl_kern_cptime(SYSCTLFN_PROTO);
#if NPTY > 0
static int sysctl_kern_maxptys(SYSCTLFN_PROTO);
#endif /* NPTY > 0 */
static int sysctl_kern_lwp(SYSCTLFN_PROTO);
static int sysctl_kern_forkfsleep(SYSCTLFN_PROTO);
static int sysctl_kern_root_partition(SYSCTLFN_PROTO);
static int sysctl_kern_drivers(SYSCTLFN_PROTO);
static int sysctl_security_setidcore(SYSCTLFN_PROTO);
static int sysctl_security_setidcorename(SYSCTLFN_PROTO);
static int sysctl_kern_cpid(SYSCTLFN_PROTO);
static int sysctl_hw_usermem(SYSCTLFN_PROTO);
static int sysctl_hw_cnmagic(SYSCTLFN_PROTO);

static void fill_lwp(struct lwp *l, struct kinfo_lwp *kl);

/*
 * ********************************************************************
 * section 1: setup routines
 * ********************************************************************
 * These functions are stuffed into a link set for sysctl setup
 * functions. They're never called or referenced from anywhere else.
 * ********************************************************************
 */

/*
 * this setup routine is a replacement for kern_sysctl()
 */
SYSCTL_SETUP(sysctl_kern_setup, "sysctl kern subtree setup")
{
	extern int kern_logsigexit;	/* defined in kern/kern_sig.c */
	extern fixpt_t ccpu;		/* defined in kern/kern_synch.c */
	extern int dumponpanic;		/* defined in kern/subr_prf.c */
	const struct sysctlnode *rnode;

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxvnodes",
		       SYSCTL_DESCR("Maximum number of vnodes"),
		       sysctl_kern_maxvnodes, 0, NULL, 0,
		       CTL_KERN, KERN_MAXVNODES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxproc",
		       SYSCTL_DESCR("Maximum number of simultaneous processes"),
		       sysctl_kern_maxproc, 0, NULL, 0,
		       CTL_KERN, KERN_MAXPROC, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxfiles",
		       SYSCTL_DESCR("Maximum number of open files"),
		       NULL, 0, &maxfiles, 0,
		       CTL_KERN, KERN_MAXFILES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "argmax",
		       SYSCTL_DESCR("Maximum number of bytes of arguments to "
				    "execve(2)"),
		       NULL, ARG_MAX, NULL, 0,
		       CTL_KERN, KERN_ARGMAX, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_HEX,
		       CTLTYPE_INT, "hostid",
		       SYSCTL_DESCR("System host ID number"),
		       sysctl_kern_hostid, 0, NULL, 0,
		       CTL_KERN, KERN_HOSTID, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "vnode",
		       SYSCTL_DESCR("System vnode table"),
		       sysctl_kern_vnode, 0, NULL, 0,
		       CTL_KERN, KERN_VNODE, CTL_EOL);
#ifndef GPROF
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "profiling",
		       SYSCTL_DESCR("Profiling information (not available)"),
		       sysctl_notavail, 0, NULL, 0,
		       CTL_KERN, KERN_PROF, CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "posix1version",
		       SYSCTL_DESCR("Version of ISO/IEC 9945 (POSIX 1003.1) "
				    "with which the operating system attempts "
				    "to comply"),
		       NULL, _POSIX_VERSION, NULL, 0,
		       CTL_KERN, KERN_POSIX1, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "ngroups",
		       SYSCTL_DESCR("Maximum number of supplemental groups"),
		       NULL, NGROUPS_MAX, NULL, 0,
		       CTL_KERN, KERN_NGROUPS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "job_control",
		       SYSCTL_DESCR("Whether job control is available"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, KERN_JOB_CONTROL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "saved_ids",
		       SYSCTL_DESCR("Whether POSIX saved set-group/user ID is "
				    "available"), NULL,
#ifdef _POSIX_SAVED_IDS
		       1,
#else /* _POSIX_SAVED_IDS */
		       0,
#endif /* _POSIX_SAVED_IDS */
		       NULL, 0, CTL_KERN, KERN_SAVED_IDS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_HEX,
		       CTLTYPE_INT, "boothowto",
		       SYSCTL_DESCR("Flags from boot loader"),
		       NULL, 0, &boothowto, sizeof(boothowto),
		       CTL_KERN, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "boottime",
		       SYSCTL_DESCR("System boot time"),
		       NULL, 0, &boottime, sizeof(boottime),
		       CTL_KERN, KERN_BOOTTIME, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "maxpartitions",
		       SYSCTL_DESCR("Maximum number of partitions allowed per "
				    "disk"),
		       NULL, MAXPARTITIONS, NULL, 0,
		       CTL_KERN, KERN_MAXPARTITIONS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "timex", NULL,
		       sysctl_notavail, 0, NULL, 0,
		       CTL_KERN, KERN_TIMEX, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rtc_offset",
		       SYSCTL_DESCR("Offset of real time clock from UTC in "
				    "minutes"),
		       sysctl_kern_rtc_offset, 0, &rtc_offset, 0,
		       CTL_KERN, KERN_RTC_OFFSET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "root_device",
		       SYSCTL_DESCR("Name of the root device"),
		       sysctl_root_device, 0, NULL, 0,
		       CTL_KERN, KERN_ROOT_DEVICE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "fsync",
		       SYSCTL_DESCR("Whether the POSIX 1003.1b File "
				    "Synchronization Option is available on "
				    "this system"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, KERN_FSYNC, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ipc",
		       SYSCTL_DESCR("SysV IPC options"),
		       NULL, 0, NULL, 0,
		       CTL_KERN, KERN_SYSVIPC, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
		       CTLTYPE_INT, "sysvmsg",
		       SYSCTL_DESCR("System V style message support available"),
		       NULL, 0, &kern_has_sysvmsg, sizeof(int),
		       CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_MSG, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
		       CTLTYPE_INT, "sysvsem",
		       SYSCTL_DESCR("System V style semaphore support "
				    "available"),
		       NULL, 0, &kern_has_sysvsem, sizeof(int),
		       CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SEM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
		       CTLTYPE_INT, "sysvshm",
		       SYSCTL_DESCR("System V style shared memory support "
				    "available"),
		       NULL, 0, &kern_has_sysvshm, sizeof(int),
		       CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SHM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "synchronized_io",
		       SYSCTL_DESCR("Whether the POSIX 1003.1b Synchronized "
				    "I/O Option is available on this system"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, KERN_SYNCHRONIZED_IO, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "iov_max",
		       SYSCTL_DESCR("Maximum number of iovec structures per "
				    "process"),
		       NULL, IOV_MAX, NULL, 0,
		       CTL_KERN, KERN_IOV_MAX, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "mapped_files",
		       SYSCTL_DESCR("Whether the POSIX 1003.1b Memory Mapped "
				    "Files Option is available on this system"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, KERN_MAPPED_FILES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "memlock",
		       SYSCTL_DESCR("Whether the POSIX 1003.1b Process Memory "
				    "Locking Option is available on this "
				    "system"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, KERN_MEMLOCK, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "memlock_range",
		       SYSCTL_DESCR("Whether the POSIX 1003.1b Range Memory "
				    "Locking Option is available on this "
				    "system"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, KERN_MEMLOCK_RANGE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "memory_protection",
		       SYSCTL_DESCR("Whether the POSIX 1003.1b Memory "
				    "Protection Option is available on this "
				    "system"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, KERN_MEMORY_PROTECTION, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "login_name_max",
		       SYSCTL_DESCR("Maximum login name length"),
		       NULL, LOGIN_NAME_MAX, NULL, 0,
		       CTL_KERN, KERN_LOGIN_NAME_MAX, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRING, "defcorename",
		       SYSCTL_DESCR("Default core file name"),
		       sysctl_kern_defcorename, 0, defcorename, MAXPATHLEN,
		       CTL_KERN, KERN_DEFCORENAME, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "logsigexit",
		       SYSCTL_DESCR("Log process exit when caused by signals"),
		       NULL, 0, &kern_logsigexit, 0,
		       CTL_KERN, KERN_LOGSIGEXIT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "fscale",
		       SYSCTL_DESCR("Kernel fixed-point scale factor"),
		       NULL, FSCALE, NULL, 0,
		       CTL_KERN, KERN_FSCALE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "ccpu",
		       SYSCTL_DESCR("Scheduler exponential decay value"),
		       NULL, 0, &ccpu, 0,
		       CTL_KERN, KERN_CCPU, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "cp_time",
		       SYSCTL_DESCR("Clock ticks spent in different CPU states"),
		       sysctl_kern_cptime, 0, NULL, 0,
		       CTL_KERN, KERN_CP_TIME, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "consdev",
		       SYSCTL_DESCR("Console device"),
		       sysctl_consdev, 0, NULL, sizeof(dev_t),
		       CTL_KERN, KERN_CONSDEV, CTL_EOL);
#if NPTY > 0
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "maxptys",
		       SYSCTL_DESCR("Maximum number of pseudo-ttys"),
		       sysctl_kern_maxptys, 0, NULL, 0,
		       CTL_KERN, KERN_MAXPTYS, CTL_EOL);
#endif /* NPTY > 0 */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "maxphys",
		       SYSCTL_DESCR("Maximum raw I/O transfer size"),
		       NULL, MAXPHYS, NULL, 0,
		       CTL_KERN, KERN_MAXPHYS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "monotonic_clock",
		       SYSCTL_DESCR("Implementation version of the POSIX "
				    "1003.1b Monotonic Clock Option"),
		       /* XXX _POSIX_VERSION */
		       NULL, _POSIX_MONOTONIC_CLOCK, NULL, 0,
		       CTL_KERN, KERN_MONOTONIC_CLOCK, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "labelsector",
		       SYSCTL_DESCR("Sector number containing the disklabel"),
		       NULL, LABELSECTOR, NULL, 0,
		       CTL_KERN, KERN_LABELSECTOR, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "labeloffset",
		       SYSCTL_DESCR("Offset of the disklabel within the "
				    "sector"),
		       NULL, LABELOFFSET, NULL, 0,
		       CTL_KERN, KERN_LABELOFFSET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "labelusesmbr",
		       SYSCTL_DESCR("disklabel is inside MBR partition"),
		       NULL, LABELUSESMBR, NULL, 0,
		       CTL_KERN, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "lwp",
		       SYSCTL_DESCR("System-wide LWP information"),
		       sysctl_kern_lwp, 0, NULL, 0,
		       CTL_KERN, KERN_LWP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "forkfsleep",
		       SYSCTL_DESCR("Milliseconds to sleep on fork failure due "
				    "to process limits"),
		       sysctl_kern_forkfsleep, 0, NULL, 0,
		       CTL_KERN, KERN_FORKFSLEEP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "posix_threads",
		       SYSCTL_DESCR("Version of IEEE Std 1003.1 and its "
				    "Threads option to which the system "
				    "attempts to conform"),
		       /* XXX _POSIX_VERSION */
		       NULL, _POSIX_THREADS, NULL, 0,
		       CTL_KERN, KERN_POSIX_THREADS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "posix_semaphores",
		       SYSCTL_DESCR("Version of IEEE Std 1003.1 and its "
				    "Semaphores option to which the system "
				    "attempts to conform"), NULL,
		       200112, NULL, 0,
		       CTL_KERN, KERN_POSIX_SEMAPHORES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "posix_barriers",
		       SYSCTL_DESCR("Version of IEEE Std 1003.1 and its "
				    "Barriers option to which the system "
				    "attempts to conform"),
		       /* XXX _POSIX_VERSION */
		       NULL, _POSIX_BARRIERS, NULL, 0,
		       CTL_KERN, KERN_POSIX_BARRIERS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "posix_timers",
		       SYSCTL_DESCR("Version of IEEE Std 1003.1 and its "
				    "Timers option to which the system "
				    "attempts to conform"),
		       /* XXX _POSIX_VERSION */
		       NULL, _POSIX_TIMERS, NULL, 0,
		       CTL_KERN, KERN_POSIX_TIMERS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "posix_spin_locks",
		       SYSCTL_DESCR("Version of IEEE Std 1003.1 and its Spin "
				    "Locks option to which the system attempts "
				    "to conform"),
		       /* XXX _POSIX_VERSION */
		       NULL, _POSIX_SPIN_LOCKS, NULL, 0,
		       CTL_KERN, KERN_POSIX_SPIN_LOCKS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "posix_reader_writer_locks",
		       SYSCTL_DESCR("Version of IEEE Std 1003.1 and its "
				    "Read-Write Locks option to which the "
				    "system attempts to conform"),
		       /* XXX _POSIX_VERSION */
		       NULL, _POSIX_READER_WRITER_LOCKS, NULL, 0,
		       CTL_KERN, KERN_POSIX_READER_WRITER_LOCKS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "dump_on_panic",
		       SYSCTL_DESCR("Perform a crash dump on system panic"),
		       NULL, 0, &dumponpanic, 0,
		       CTL_KERN, KERN_DUMP_ON_PANIC, CTL_EOL);
#ifdef DIAGNOSTIC
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "panic_now",
		       SYSCTL_DESCR("Trigger a panic"),
		       sysctl_kern_trigger_panic, 0, NULL, 0,
		       CTL_KERN, CTL_CREATE, CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "root_partition",
		       SYSCTL_DESCR("Root partition on the root device"),
		       sysctl_kern_root_partition, 0, NULL, 0,
		       CTL_KERN, KERN_ROOT_PARTITION, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "drivers",
		       SYSCTL_DESCR("List of all drivers with block and "
				    "character device numbers"),
		       sysctl_kern_drivers, 0, NULL, 0,
		       CTL_KERN, KERN_DRIVERS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "cp_id",
		       SYSCTL_DESCR("Mapping of CPU number to CPU id"),
		       sysctl_kern_cpid, 0, NULL, 0,
		       CTL_KERN, KERN_CP_ID, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "coredump",
		       SYSCTL_DESCR("Coredump settings."),
		       NULL, 0, NULL, 0,
		       CTL_KERN, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "setid",
		       SYSCTL_DESCR("Set-id processes' coredump settings."),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "dump",
		       SYSCTL_DESCR("Allow set-id processes to dump core."),
		       sysctl_security_setidcore, 0, &security_setidcore_dump,
		       sizeof(security_setidcore_dump),
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRING, "path",
		       SYSCTL_DESCR("Path pattern for set-id coredumps."),
		       sysctl_security_setidcorename, 0,
		       security_setidcore_path,
		       sizeof(security_setidcore_path),
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "owner",
		       SYSCTL_DESCR("Owner id for set-id processes' cores."),
		       sysctl_security_setidcore, 0, &security_setidcore_owner,
		       0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "group",
		       SYSCTL_DESCR("Group id for set-id processes' cores."),
		       sysctl_security_setidcore, 0, &security_setidcore_group,
		       0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mode",
		       SYSCTL_DESCR("Mode for set-id processes' cores."),
		       sysctl_security_setidcore, 0, &security_setidcore_mode,
		       0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_IMMEDIATE|CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "no_sa_support",
		       SYSCTL_DESCR("0 if the kernel supports SA, otherwise "
		       "it doesn't"),
		       NULL, 1, NULL, 0,
		       CTL_KERN, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_STRING, "configname",
			SYSCTL_DESCR("Name of config file"),
			NULL, 0, __UNCONST(kernel_ident), 0,
			CTL_KERN, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_STRING, "buildinfo",
			SYSCTL_DESCR("Information from build environment"),
			NULL, 0, __UNCONST(buildinfo), 0,
			CTL_KERN, CTL_CREATE, CTL_EOL);

	/* kern.posix. */
	sysctl_createv(clog, 0, NULL, &rnode,
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "posix",
			SYSCTL_DESCR("POSIX options"),
			NULL, 0, NULL, 0,
			CTL_KERN, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
			CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
			CTLTYPE_INT, "semmax",
			SYSCTL_DESCR("Maximal number of semaphores"),
			NULL, 0, &ksem_max, 0,
			CTL_CREATE, CTL_EOL);
}

SYSCTL_SETUP(sysctl_hw_misc_setup, "sysctl hw subtree misc setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "usermem",
		       SYSCTL_DESCR("Bytes of non-kernel memory"),
		       sysctl_hw_usermem, 0, NULL, 0,
		       CTL_HW, HW_USERMEM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE|CTLFLAG_HEX,
		       CTLTYPE_STRING, "cnmagic",
		       SYSCTL_DESCR("Console magic key sequence"),
		       sysctl_hw_cnmagic, 0, NULL, CNS_LEN,
		       CTL_HW, HW_CNMAGIC, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_QUAD, "usermem64",
		       SYSCTL_DESCR("Bytes of non-kernel memory"),
		       sysctl_hw_usermem, 0, NULL, 0,
		       CTL_HW, HW_USERMEM64, CTL_EOL);
}

#ifdef DEBUG
/*
 * Debugging related system variables.
 */
struct ctldebug /* debug0, */ /* debug1, */ debug2, debug3, debug4;
struct ctldebug debug5, debug6, debug7, debug8, debug9;
struct ctldebug debug10, debug11, debug12, debug13, debug14;
struct ctldebug debug15, debug16, debug17, debug18, debug19;
static struct ctldebug *debugvars[CTL_DEBUG_MAXID] = {
	&debug0, &debug1, &debug2, &debug3, &debug4,
	&debug5, &debug6, &debug7, &debug8, &debug9,
	&debug10, &debug11, &debug12, &debug13, &debug14,
	&debug15, &debug16, &debug17, &debug18, &debug19,
};

/*
 * this setup routine is a replacement for debug_sysctl()
 *
 * note that it creates several nodes per defined debug variable
 */
SYSCTL_SETUP(sysctl_debug_setup, "sysctl debug subtree setup")
{
	struct ctldebug *cdp;
	char nodename[20];
	int i;

	/*
	 * two ways here:
	 *
	 * the "old" way (debug.name -> value) which was emulated by
	 * the sysctl(8) binary
	 *
	 * the new way, which the sysctl(8) binary was actually using

	 node	debug
	 node	debug.0
	 string debug.0.name
	 int	debug.0.value
	 int	debug.name

	 */

	for (i = 0; i < CTL_DEBUG_MAXID; i++) {
		cdp = debugvars[i];
		if (cdp->debugname == NULL || cdp->debugvar == NULL)
			continue;

		snprintf(nodename, sizeof(nodename), "debug%d", i);
		sysctl_createv(clog, 0, NULL, NULL,
			       CTLFLAG_PERMANENT|CTLFLAG_HIDDEN,
			       CTLTYPE_NODE, nodename, NULL,
			       NULL, 0, NULL, 0,
			       CTL_DEBUG, i, CTL_EOL);
		sysctl_createv(clog, 0, NULL, NULL,
			       CTLFLAG_PERMANENT|CTLFLAG_HIDDEN,
			       CTLTYPE_STRING, "name", NULL,
			       /*XXXUNCONST*/
			       NULL, 0, __UNCONST(cdp->debugname), 0,
			       CTL_DEBUG, i, CTL_DEBUG_NAME, CTL_EOL);
		sysctl_createv(clog, 0, NULL, NULL,
			       CTLFLAG_PERMANENT|CTLFLAG_HIDDEN,
			       CTLTYPE_INT, "value", NULL,
			       NULL, 0, cdp->debugvar, 0,
			       CTL_DEBUG, i, CTL_DEBUG_VALUE, CTL_EOL);
		sysctl_createv(clog, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_INT, cdp->debugname, NULL,
			       NULL, 0, cdp->debugvar, 0,
			       CTL_DEBUG, CTL_CREATE, CTL_EOL);
	}
}
#endif /* DEBUG */

/*
 * ********************************************************************
 * section 2: private node-specific helper routines.
 * ********************************************************************
 */

#ifdef DIAGNOSTIC
static int
sysctl_kern_trigger_panic(SYSCTLFN_ARGS)
{
	int newtrig, error;
	struct sysctlnode node;

	newtrig = 0;
	node = *rnode;
	node.sysctl_data = &newtrig;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (newtrig != 0)
		panic("Panic triggered");

	return (error);
}
#endif

/*
 * sysctl helper routine for kern.maxvnodes.  Drain vnodes if
 * new value is lower than desiredvnodes and then calls reinit
 * routines that needs to adjust to the new value.
 */
static int
sysctl_kern_maxvnodes(SYSCTLFN_ARGS)
{
	int error, new_vnodes, old_vnodes, new_max;
	struct sysctlnode node;

	new_vnodes = desiredvnodes;
	node = *rnode;
	node.sysctl_data = &new_vnodes;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	/*
	 * sysctl passes down unsigned values, require them
	 * to be positive
	 */
	if (new_vnodes <= 0)
		return (EINVAL);

	/* Limits: 75% of kmem and physical memory. */
	new_max = calc_cache_size(vmem_size(kmem_arena, VMEM_FREE|VMEM_ALLOC),
	    75, 75) / VNODE_COST;
	if (new_vnodes > new_max)
		new_vnodes = new_max;

	old_vnodes = desiredvnodes;
	desiredvnodes = new_vnodes;
	error = vfs_drainvnodes(new_vnodes);
	if (error) {
		desiredvnodes = old_vnodes;
		return (error);
	}
	vfs_reinit();
	nchreinit();

	return (0);
}

/*
 * sysctl helper routine for rtc_offset - set time after changes
 */
static int
sysctl_kern_rtc_offset(SYSCTLFN_ARGS)
{
	struct timespec ts, delta;
	int error, new_rtc_offset;
	struct sysctlnode node;

	new_rtc_offset = rtc_offset;
	node = *rnode;
	node.sysctl_data = &new_rtc_offset;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_TIME,
	    KAUTH_REQ_SYSTEM_TIME_RTCOFFSET,
	    KAUTH_ARG(new_rtc_offset), NULL, NULL))
		return (EPERM);
	if (rtc_offset == new_rtc_offset)
		return (0);

	/* if we change the offset, adjust the time */
	nanotime(&ts);
	delta.tv_sec = 60 * (new_rtc_offset - rtc_offset);
	delta.tv_nsec = 0;
	timespecadd(&ts, &delta, &ts);
	rtc_offset = new_rtc_offset;
	return (settime(l->l_proc, &ts));
}

/*
 * sysctl helper routine for kern.maxproc. Ensures that the new
 * values are not too low or too high.
 */
static int
sysctl_kern_maxproc(SYSCTLFN_ARGS)
{
	int error, nmaxproc;
	struct sysctlnode node;

	nmaxproc = maxproc;
	node = *rnode;
	node.sysctl_data = &nmaxproc;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (nmaxproc < 0 || nmaxproc >= PID_MAX)
		return (EINVAL);
#ifdef __HAVE_CPU_MAXPROC
	if (nmaxproc > cpu_maxproc())
		return (EINVAL);
#endif
	maxproc = nmaxproc;

	return (0);
}

/*
 * sysctl helper function for kern.hostid. The hostid is a long, but
 * we export it as an int, so we need to give it a little help.
 */
static int
sysctl_kern_hostid(SYSCTLFN_ARGS)
{
	int error, inthostid;
	struct sysctlnode node;

	inthostid = hostid;  /* XXX assumes sizeof int <= sizeof long */
	node = *rnode;
	node.sysctl_data = &inthostid;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	hostid = (unsigned)inthostid;

	return (0);
}

/*
 * sysctl helper routine for kern.defcorename. In the case of a new
 * string being assigned, check that it's not a zero-length string.
 * (XXX the check in -current doesn't work, but do we really care?)
 */
static int
sysctl_kern_defcorename(SYSCTLFN_ARGS)
{
	int error;
	char *newcorename;
	struct sysctlnode node;

	newcorename = PNBUF_GET();
	node = *rnode;
	node.sysctl_data = &newcorename[0];
	memcpy(node.sysctl_data, rnode->sysctl_data, MAXPATHLEN);
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL) {
		goto done;
	}

	/*
	 * when sysctl_lookup() deals with a string, it's guaranteed
	 * to come back nul terminated. So there.  :)
	 */
	if (strlen(newcorename) == 0) {
		error = EINVAL;
	} else {
		memcpy(rnode->sysctl_data, node.sysctl_data, MAXPATHLEN);
		error = 0;
	}
done:
	PNBUF_PUT(newcorename);
	return error;
}

/*
 * sysctl helper routine for kern.cp_time node. Adds up cpu time
 * across all cpus.
 */
static int
sysctl_kern_cptime(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	uint64_t *cp_time = NULL;
	int error, n = ncpu, i;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	/*
	 * if you specifically pass a buffer that is the size of the
	 * sum, or if you are probing for the size, you get the "sum"
	 * of cp_time (and the size thereof) across all processors.
	 *
	 * alternately, you can pass an additional mib number and get
	 * cp_time for that particular processor.
	 */
	switch (namelen) {
	case 0:
		if (*oldlenp == sizeof(uint64_t) * CPUSTATES || oldp == NULL) {
			node.sysctl_size = sizeof(uint64_t) * CPUSTATES;
			n = -1; /* SUM */
		}
		else {
			node.sysctl_size = n * sizeof(uint64_t) * CPUSTATES;
			n = -2; /* ALL */
		}
		break;
	case 1:
		if (name[0] < 0 || name[0] >= n)
			return (ENOENT); /* ENOSUCHPROCESSOR */
		node.sysctl_size = sizeof(uint64_t) * CPUSTATES;
		n = name[0];
		/*
		 * adjust these so that sysctl_lookup() will be happy
		 */
		name++;
		namelen--;
		break;
	default:
		return (EINVAL);
	}

	cp_time = kmem_alloc(node.sysctl_size, KM_SLEEP);
	if (cp_time == NULL)
		return (ENOMEM);
	node.sysctl_data = cp_time;
	memset(cp_time, 0, node.sysctl_size);

	for (CPU_INFO_FOREACH(cii, ci)) {
		if (n <= 0) {
			for (i = 0; i < CPUSTATES; i++) {
				cp_time[i] += ci->ci_schedstate.spc_cp_time[i];
			}
		}
		/*
		 * if a specific processor was requested and we just
		 * did it, we're done here
		 */
		if (n == 0)
			break;
		/*
		 * if doing "all", skip to next cp_time set for next processor
		 */
		if (n == -2)
			cp_time += CPUSTATES;
		/*
		 * if we're doing a specific processor, we're one
		 * processor closer
		 */
		if (n > 0)
			n--;
	}

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	kmem_free(node.sysctl_data, node.sysctl_size);
	return (error);
}

#if NPTY > 0
/*
 * sysctl helper routine for kern.maxptys. Ensures that any new value
 * is acceptable to the pty subsystem.
 */
static int
sysctl_kern_maxptys(SYSCTLFN_ARGS)
{
	int pty_maxptys(int, int);		/* defined in kern/tty_pty.c */
	int error, xmax;
	struct sysctlnode node;

	/* get current value of maxptys */
	xmax = pty_maxptys(0, 0);

	node = *rnode;
	node.sysctl_data = &xmax;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (xmax != pty_maxptys(xmax, 1))
		return (EINVAL);

	return (0);
}
#endif /* NPTY > 0 */

/*
 * sysctl helper routine to do kern.lwp.* work.
 */
static int
sysctl_kern_lwp(SYSCTLFN_ARGS)
{
	struct kinfo_lwp klwp;
	struct proc *p;
	struct lwp *l2, *l3;
	char *where, *dp;
	int pid, elem_size, elem_count;
	int buflen, needed, error;
	bool gotit;

	if (namelen == 1 && name[0] == CTL_QUERY)
		return (sysctl_query(SYSCTLFN_CALL(rnode)));

	dp = where = oldp;
	buflen = where != NULL ? *oldlenp : 0;
	error = needed = 0;

	if (newp != NULL || namelen != 3)
		return (EINVAL);
	pid = name[0];
	elem_size = name[1];
	elem_count = name[2];

	sysctl_unlock();
	if (pid == -1) {
		mutex_enter(proc_lock);
		PROCLIST_FOREACH(p, &allproc) {
			/* Grab a hold on the process. */
			if (!rw_tryenter(&p->p_reflock, RW_READER)) {
				continue;
			}
			mutex_exit(proc_lock);

			mutex_enter(p->p_lock);
			LIST_FOREACH(l2, &p->p_lwps, l_sibling) {
				if (buflen >= elem_size && elem_count > 0) {
					lwp_lock(l2);
					fill_lwp(l2, &klwp);
					lwp_unlock(l2);
					mutex_exit(p->p_lock);

					/*
					 * Copy out elem_size, but not
					 * larger than the size of a
					 * struct kinfo_proc2.
					 */
					error = dcopyout(l, &klwp, dp,
					    min(sizeof(klwp), elem_size));
					if (error) {
						rw_exit(&p->p_reflock);
						goto cleanup;
					}
					mutex_enter(p->p_lock);
					LIST_FOREACH(l3, &p->p_lwps,
					    l_sibling) {
						if (l2 == l3)
							break;
					}
					if (l3 == NULL) {
						mutex_exit(p->p_lock);
						rw_exit(&p->p_reflock);
						error = EAGAIN;
						goto cleanup;
					}
					dp += elem_size;
					buflen -= elem_size;
					elem_count--;
				}
				needed += elem_size;
			}
			mutex_exit(p->p_lock);

			/* Drop reference to process. */
			mutex_enter(proc_lock);
			rw_exit(&p->p_reflock);
		}
		mutex_exit(proc_lock);
	} else {
		mutex_enter(proc_lock);
		p = proc_find(pid);
		if (p == NULL) {
			error = ESRCH;
			mutex_exit(proc_lock);
			goto cleanup;
		}
		/* Grab a hold on the process. */
		gotit = rw_tryenter(&p->p_reflock, RW_READER);
		mutex_exit(proc_lock);
		if (!gotit) {
			error = ESRCH;
			goto cleanup;
		}

		mutex_enter(p->p_lock);
		LIST_FOREACH(l2, &p->p_lwps, l_sibling) {
			if (buflen >= elem_size && elem_count > 0) {
				lwp_lock(l2);
				fill_lwp(l2, &klwp);
				lwp_unlock(l2);
				mutex_exit(p->p_lock);
				/*
				 * Copy out elem_size, but not larger than
				 * the size of a struct kinfo_proc2.
				 */
				error = dcopyout(l, &klwp, dp,
				    min(sizeof(klwp), elem_size));
				if (error) {
					rw_exit(&p->p_reflock);
					goto cleanup;
				}
				mutex_enter(p->p_lock);
				LIST_FOREACH(l3, &p->p_lwps, l_sibling) {
					if (l2 == l3)
						break;
				}
				if (l3 == NULL) {
					mutex_exit(p->p_lock);
					rw_exit(&p->p_reflock);
					error = EAGAIN;
					goto cleanup;
				}
				dp += elem_size;
				buflen -= elem_size;
				elem_count--;
			}
			needed += elem_size;
		}
		mutex_exit(p->p_lock);

		/* Drop reference to process. */
		rw_exit(&p->p_reflock);
	}

	if (where != NULL) {
		*oldlenp = dp - where;
		if (needed > *oldlenp) {
			sysctl_relock();
			return (ENOMEM);
		}
	} else {
		needed += KERN_LWPSLOP;
		*oldlenp = needed;
	}
	error = 0;
 cleanup:
	sysctl_relock();
	return (error);
}

/*
 * sysctl helper routine for kern.forkfsleep node. Ensures that the
 * given value is not too large or two small, and is at least one
 * timer tick if not zero.
 */
static int
sysctl_kern_forkfsleep(SYSCTLFN_ARGS)
{
	/* userland sees value in ms, internally is in ticks */
	extern int forkfsleep;		/* defined in kern/kern_fork.c */
	int error, timo, lsleep;
	struct sysctlnode node;

	lsleep = forkfsleep * 1000 / hz;
	node = *rnode;
	node.sysctl_data = &lsleep;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	/* refuse negative values, and overly 'long time' */
	if (lsleep < 0 || lsleep > MAXSLP * 1000)
		return (EINVAL);

	timo = mstohz(lsleep);

	/* if the interval is >0 ms && <1 tick, use 1 tick */
	if (lsleep != 0 && timo == 0)
		forkfsleep = 1;
	else
		forkfsleep = timo;

	return (0);
}

/*
 * sysctl helper routine for kern.root_partition
 */
static int
sysctl_kern_root_partition(SYSCTLFN_ARGS)
{
	int rootpart = DISKPART(rootdev);
	struct sysctlnode node = *rnode;

	node.sysctl_data = &rootpart;
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

/*
 * sysctl helper function for kern.drivers
 */
static int
sysctl_kern_drivers(SYSCTLFN_ARGS)
{
	int error;
	size_t buflen;
	struct kinfo_drivers kd;
	char *start, *where;
	const char *dname;
	int i;
	extern struct devsw_conv *devsw_conv;
	extern int max_devsw_convs;

	start = where = oldp;
	buflen = *oldlenp;
	if (where == NULL) {
		*oldlenp = max_devsw_convs * sizeof kd;
		return 0;
	}

	/*
	 * An array of kinfo_drivers structures
	 */
	error = 0;
	sysctl_unlock();
	mutex_enter(&device_lock);
	for (i = 0; i < max_devsw_convs; i++) {
		dname = devsw_conv[i].d_name;
		if (dname == NULL)
			continue;
		if (buflen < sizeof kd) {
			error = ENOMEM;
			break;
		}
		memset(&kd, 0, sizeof(kd));
		kd.d_bmajor = devsw_conv[i].d_bmajor;
		kd.d_cmajor = devsw_conv[i].d_cmajor;
		strlcpy(kd.d_name, dname, sizeof kd.d_name);
		mutex_exit(&device_lock);
		error = dcopyout(l, &kd, where, sizeof kd);
		mutex_enter(&device_lock);
		if (error != 0)
			break;
		buflen -= sizeof kd;
		where += sizeof kd;
	}
	mutex_exit(&device_lock);
	sysctl_relock();
	*oldlenp = where - start;
	return error;
}

static int
sysctl_security_setidcore(SYSCTLFN_ARGS)
{
	int newsize, error;
	struct sysctlnode node;

	node = *rnode;
	node.sysctl_data = &newsize;
	newsize = *(int *)rnode->sysctl_data;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_SETIDCORE,
	    0, NULL, NULL, NULL))
		return (EPERM);

	*(int *)rnode->sysctl_data = newsize;

	return 0;
}

static int
sysctl_security_setidcorename(SYSCTLFN_ARGS)
{
	int error;
	char *newsetidcorename;
	struct sysctlnode node;

	newsetidcorename = PNBUF_GET();
	node = *rnode;
	node.sysctl_data = newsetidcorename;
	memcpy(node.sysctl_data, rnode->sysctl_data, MAXPATHLEN);
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL) {
		goto out;
	}
	if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_SETIDCORE,
	    0, NULL, NULL, NULL)) {
		error = EPERM;
		goto out;
	}
	if (strlen(newsetidcorename) == 0) {
		error = EINVAL;
		goto out;
	}
	memcpy(rnode->sysctl_data, node.sysctl_data, MAXPATHLEN);
out:
	PNBUF_PUT(newsetidcorename);
	return error;
}

/*
 * sysctl helper routine for kern.cp_id node. Maps cpus to their
 * cpuids.
 */
static int
sysctl_kern_cpid(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	uint64_t *cp_id = NULL;
	int error, n = ncpu;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	/*
	 * Here you may either retrieve a single cpu id or the whole
	 * set. The size you get back when probing depends on what
	 * you ask for.
	 */
	switch (namelen) {
	case 0:
		node.sysctl_size = n * sizeof(uint64_t);
		n = -2; /* ALL */
		break;
	case 1:
		if (name[0] < 0 || name[0] >= n)
			return (ENOENT); /* ENOSUCHPROCESSOR */
		node.sysctl_size = sizeof(uint64_t);
		n = name[0];
		/*
		 * adjust these so that sysctl_lookup() will be happy
		 */
		name++;
		namelen--;
		break;
	default:
		return (EINVAL);
	}

	cp_id = kmem_alloc(node.sysctl_size, KM_SLEEP);
	if (cp_id == NULL)
		return (ENOMEM);
	node.sysctl_data = cp_id;
	memset(cp_id, 0, node.sysctl_size);

	for (CPU_INFO_FOREACH(cii, ci)) {
		if (n <= 0)
			cp_id[0] = cpu_index(ci);
		/*
		 * if a specific processor was requested and we just
		 * did it, we're done here
		 */
		if (n == 0)
			break;
		/*
		 * if doing "all", skip to next cp_id slot for next processor
		 */
		if (n == -2)
			cp_id++;
		/*
		 * if we're doing a specific processor, we're one
		 * processor closer
		 */
		if (n > 0)
			n--;
	}

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	kmem_free(node.sysctl_data, node.sysctl_size);
	return (error);
}

/*
 * sysctl helper routine for hw.usermem and hw.usermem64. Values are
 * calculate on the fly taking into account integer overflow and the
 * current wired count.
 */
static int
sysctl_hw_usermem(SYSCTLFN_ARGS)
{
	u_int ui;
	u_quad_t uq;
	struct sysctlnode node;

	node = *rnode;
	switch (rnode->sysctl_num) {
	case HW_USERMEM:
		if ((ui = physmem - uvmexp.wired) > (UINT_MAX / PAGE_SIZE))
			ui = UINT_MAX;
		else
			ui *= PAGE_SIZE;
		node.sysctl_data = &ui;
		break;
	case HW_USERMEM64:
		uq = (u_quad_t)(physmem - uvmexp.wired) * PAGE_SIZE;
		node.sysctl_data = &uq;
		break;
	default:
		return (EINVAL);
	}

	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

/*
 * sysctl helper routine for kern.cnmagic node. Pulls the old value
 * out, encoded, and stuffs the new value in for decoding.
 */
static int
sysctl_hw_cnmagic(SYSCTLFN_ARGS)
{
	char magic[CNS_LEN];
	int error;
	struct sysctlnode node;

	if (oldp)
		cn_get_magic(magic, CNS_LEN);
	node = *rnode;
	node.sysctl_data = &magic[0];
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	return (cn_set_magic(magic));
}

/*
 * ********************************************************************
 * section 3: public helper routines that are used for more than one
 * node
 * ********************************************************************
 */

/*
 * sysctl helper routine for the kern.root_device node and some ports'
 * machdep.root_device nodes.
 */
int
sysctl_root_device(SYSCTLFN_ARGS)
{
	struct sysctlnode node;

	node = *rnode;
	node.sysctl_data = __UNCONST(device_xname(root_device));
	node.sysctl_size = strlen(device_xname(root_device)) + 1;
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

/*
 * sysctl helper routine for kern.consdev, dependent on the current
 * state of the console. Also used for machdep.console_device on some
 * ports.
 */
int
sysctl_consdev(SYSCTLFN_ARGS)
{
	dev_t consdev;
	uint32_t oconsdev;
	struct sysctlnode node;

	if (cn_tab != NULL)
		consdev = cn_tab->cn_dev;
	else
		consdev = NODEV;
	node = *rnode;
	switch (*oldlenp) {
	case sizeof(consdev):
		node.sysctl_data = &consdev;
		node.sysctl_size = sizeof(consdev);
		break;
	case sizeof(oconsdev):
		oconsdev = (uint32_t)consdev;
		node.sysctl_data = &oconsdev;
		node.sysctl_size = sizeof(oconsdev);
		break;
	default:
		return EINVAL;
	}
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

/*
 * ********************************************************************
 * section 4: support for some helpers
 * ********************************************************************
 */


/*
 * Fill in a kinfo_lwp structure for the specified lwp.
 */
static void
fill_lwp(struct lwp *l, struct kinfo_lwp *kl)
{
	struct proc *p = l->l_proc;
	struct timeval tv;

	KASSERT(lwp_locked(l, NULL));

	memset(kl, 0, sizeof(*kl));

	kl->l_forw = 0;
	kl->l_back = 0;
	kl->l_laddr = PTRTOUINT64(l);
	kl->l_addr = PTRTOUINT64(l->l_addr);
	kl->l_stat = l->l_stat;
	kl->l_lid = l->l_lid;
	kl->l_flag = L_INMEM;
	kl->l_flag |= sysctl_map_flags(sysctl_lwpprflagmap, l->l_prflag);
	kl->l_flag |= sysctl_map_flags(sysctl_lwpflagmap, l->l_flag);

	kl->l_swtime = l->l_swtime;
	kl->l_slptime = l->l_slptime;
	if (l->l_stat == LSONPROC)
		kl->l_schedflags = l->l_cpu->ci_schedstate.spc_flags;
	else
		kl->l_schedflags = 0;
	kl->l_priority = lwp_eprio(l);
	kl->l_usrpri = l->l_priority;
	if (l->l_wchan)
		strncpy(kl->l_wmesg, l->l_wmesg, sizeof(kl->l_wmesg));
	kl->l_wchan = PTRTOUINT64(l->l_wchan);
	kl->l_cpuid = cpu_index(l->l_cpu);
	bintime2timeval(&l->l_rtime, &tv);
	kl->l_rtime_sec = tv.tv_sec;
	kl->l_rtime_usec = tv.tv_usec;
	kl->l_cpticks = l->l_cpticks;
	kl->l_pctcpu = l->l_pctcpu;
	kl->l_pid = p->p_pid;
	if (l->l_name == NULL)
		kl->l_name[0] = '\0';
	else
		strlcpy(kl->l_name, l->l_name, sizeof(kl->l_name));
}
