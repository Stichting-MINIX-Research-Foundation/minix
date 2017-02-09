/*	$NetBSD: init_main.c,v 1.470 2015/09/14 01:40:03 uebayasi Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1982, 1986, 1989, 1991, 1992, 1993
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
 *	@(#)init_main.c	8.16 (Berkeley) 5/14/95
 */

/*
 * Copyright (c) 1995 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)init_main.c	8.16 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: init_main.c,v 1.470 2015/09/14 01:40:03 uebayasi Exp $");

#include "opt_ddb.h"
#include "opt_ipsec.h"
#include "opt_modular.h"
#include "opt_ntp.h"
#include "opt_pipe.h"
#include "opt_syscall_debug.h"
#include "opt_sysv.h"
#include "opt_fileassoc.h"
#include "opt_ktrace.h"
#include "opt_pax.h"
#include "opt_compat_netbsd.h"
#include "opt_wapbl.h"
#include "opt_ptrace.h"
#include "opt_rnd_printf.h"
#include "opt_splash.h"

#if defined(SPLASHSCREEN) && defined(makeoptions_SPLASHSCREEN_IMAGE)
extern void *_binary_splash_image_start;
extern void *_binary_splash_image_end;
#endif

#include "drvctl.h"
#include "ether.h"
#include "ksyms.h"

#include "veriexec.h"

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/cpu.h>
#include <sys/cpufreq.h>
#include <sys/spldebug.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/fstrans.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/exec.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/percpu.h>
#include <sys/pserialize.h>
#include <sys/pset.h>
#include <sys/sysctl.h>
#include <sys/reboot.h>
#include <sys/event.h>
#include <sys/mbuf.h>
#include <sys/sched.h>
#include <sys/sleepq.h>
#include <sys/ipi.h>
#include <sys/iostat.h>
#include <sys/vmem.h>
#include <sys/uuid.h>
#include <sys/extent.h>
#include <sys/disk.h>
#include <sys/msgbuf.h>
#include <sys/module.h>
#include <sys/event.h>
#include <sys/lockf.h>
#include <sys/once.h>
#include <sys/kcpuset.h>
#include <sys/ksyms.h>
#include <sys/uidinfo.h>
#include <sys/kprintf.h>
#ifdef IPSEC
#include <netipsec/ipsec.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#include <sys/domain.h>
#include <sys/namei.h>
#include <sys/rnd.h>
#include <sys/pipe.h>
#if NVERIEXEC > 0
#include <sys/verified_exec.h>
#endif /* NVERIEXEC > 0 */
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/kauth.h>
#include <net80211/ieee80211_netbsd.h>
#ifdef PTRACE
#include <sys/ptrace.h>
#endif /* PTRACE */
#include <sys/cprng.h>

#include <sys/syscall.h>
#include <sys/syscallargs.h>

#if defined(PAX_MPROTECT) || defined(PAX_SEGVGUARD) || defined(PAX_ASLR)
#include <sys/pax.h>
#endif /* PAX_MPROTECT || PAX_SEGVGUARD || PAX_ASLR */

#include <secmodel/secmodel.h>

#include <ufs/ufs/quota.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <sys/cpu.h>

#include <uvm/uvm.h>	/* extern struct uvm uvm */

#include <dev/cons.h>
#include <dev/splash/splash.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/raw_cb.h>
#include <net/if_llatbl.h>

#include <prop/proplib.h>

#include <sys/userconf.h>

extern struct lwp lwp0;
extern time_t rootfstime;

#ifndef curlwp
struct	lwp *curlwp = &lwp0;
#endif
struct	proc *initproc;

struct	vnode *rootvp, *swapdev_vp;
int	boothowto;
int	cold = 1;			/* still working on startup */
struct timespec boottime;	        /* time at system startup - will only follow settime deltas */

int	start_init_exec;		/* semaphore for start_init() */

cprng_strong_t	*kern_cprng;

static void check_console(struct lwp *l);
static void start_init(void *);
static void configure(void);
static void configure2(void);
static void configure3(void);
void main(void);

/*
 * System startup; initialize the world, create process 0, mount root
 * filesystem, and fork to create init and pagedaemon.  Most of the
 * hard work is done in the lower-level initialization routines including
 * startup(), which does memory initialization and autoconfiguration.
 */
void
main(void)
{
	struct timespec time;
	struct lwp *l;
	struct proc *p;
	int s, error;
#ifdef NVNODE_IMPLICIT
	int usevnodes;
#endif
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	l = &lwp0;
#ifndef LWP0_CPU_INFO
	l->l_cpu = curcpu();
#endif
	l->l_pflag |= LP_RUNNING;

	/*
	 * Attempt to find console and initialize
	 * in case of early panic or other messages.
	 */
	consinit();

	kernel_lock_init();
	once_init();

	mi_cpu_init();
	kernconfig_lock_init();
	kthread_sysinit();

	/* Initialize the device switch tables. */
	devsw_init();

	/* Initialize event counters. */
	evcnt_init();

	uvm_init();
	ubchist_init();
	kcpuset_sysinit();

	prop_kern_init();

#if ((NKSYMS > 0) || (NDDB > 0) || (NMODULAR > 0))
	ksyms_init();
#endif
	kprintf_init();

	percpu_init();

	/* Initialize lock caches. */
	mutex_obj_init();
	rw_obj_init();

	/* Passive serialization. */
	pserialize_init();

	/* Initialize the extent manager. */
	extent_init();

	/* Do machine-dependent initialization. */
	cpu_startup();

	/* Initialize the sysctl subsystem. */
	sysctl_init();

	/* Initialize callouts, part 1. */
	callout_startup();

	/* Initialize the kernel authorization subsystem. */
	kauth_init();

	secmodel_init();

	spec_init();

	/*
	 * Set BPF op vector.  Can't do this in bpf attach, since
	 * network drivers attach before bpf.
	 */
	bpf_setops();

	/* Start module system. */
	module_init();

	/*
	 * Initialize the kernel authorization subsystem and start the
	 * default security model, if any. We need to do this early
	 * enough so that subsystems relying on any of the aforementioned
	 * can work properly. Since the security model may dictate the
	 * credential inheritance policy, it is needed at least before
	 * any process is created, specifically proc0.
	 */
	module_init_class(MODULE_CLASS_SECMODEL);

	/* Initialize the buffer cache */
	bufinit();


#if defined(SPLASHSCREEN) && defined(SPLASHSCREEN_IMAGE)
	size_t splash_size = (&_binary_splash_image_end -
	    &_binary_splash_image_start) * sizeof(void *);
	splash_setimage(&_binary_splash_image_start, splash_size);
#endif

	/* Initialize sockets. */
	soinit();

	/*
	 * The following things must be done before autoconfiguration.
	 */
	rnd_init();		/* initialize entropy pool */

	cprng_init();		/* initialize cryptographic PRNG */

	/* Initialize process and pgrp structures. */
	procinit();
	lwpinit();

	/* Initialize signal-related data structures. */
	signal_init();

	/* Initialize resource management. */
	resource_init();

	/* Create process 0. */
	proc0_init();
	lwp0_init();

	/* Disable preemption during boot. */
	kpreempt_disable();

	/* Initialize the UID hash table. */
	uid_init();

	/* Charge root for one process. */
	(void)chgproccnt(0, 1);

	/* Initialize timekeeping. */
	time_init();

	/* Initialize the run queues, turnstiles and sleep queues. */
	sched_rqinit();
	turnstile_init();
	sleeptab_init(&sleeptab);

	sched_init();

	/* Initialize processor-sets */
	psets_init();

	/* Initialize cpufreq(9) */
	cpufreq_init();

	/* MI initialization of the boot cpu */
	error = mi_cpu_attach(curcpu());
	KASSERT(error == 0);

	/* Initialize timekeeping, part 2. */
	time_init2();

	/*
	 * Initialize mbuf's.  Do this now because we might attempt to
	 * allocate mbufs or mbuf clusters during autoconfiguration.
	 */
	mbinit();

	/* Initialize I/O statistics. */
	iostat_init();

	/* Initialize the log device. */
	loginit();

	/* Second part of module system initialization. */
	module_start_unload_thread();

	/* Initialize the file systems. */
#ifdef NVNODE_IMPLICIT
	/*
	 * If maximum number of vnodes in namei vnode cache is not explicitly
	 * defined in kernel config, adjust the number such as we use roughly
	 * 10% of memory for vnodes and associated data structures in the
	 * assumed worst case.  Do not provide fewer than NVNODE vnodes.
	 */
	usevnodes = calc_cache_size(vmem_size(kmem_arena, VMEM_FREE|VMEM_ALLOC),
	    10, VNODE_KMEM_MAXPCT) / VNODE_COST;
	if (usevnodes > desiredvnodes)
		desiredvnodes = usevnodes;
#endif
	vfsinit();
	lf_init();

	/* Initialize fstrans. */
	fstrans_init();

	/* Initialize the file descriptor system. */
	fd_sys_init();

	/* Initialize cwd structures */
	cwd_sys_init();

	/* Initialize kqueue. */
	kqueue_init();

	inittimecounter();
	ntp_init();

	/* Initialize tty subsystem. */
	tty_init();
	ttyldisc_init();

	/* Initialize the buffer cache, part 2. */
	bufinit2();

	/* Initialize the disk wedge subsystem. */
	dkwedge_init();

	/* Initialize the kernel strong PRNG. */
	kern_cprng = cprng_strong_create("kernel", IPL_VM,
					 CPRNG_INIT_ANY|CPRNG_REKEY_ANY);

	/* Initialize interfaces. */
	ifinit1();

	spldebug_start();

	/* Initialize sockets thread(s) */
	soinit1();

	/* Configure the system hardware.  This will enable interrupts. */
	configure();

	/* Once all CPUs are detected, initialize the per-CPU cprng_fast.  */
	cprng_fast_init();

	ssp_init();

	ubc_init();		/* must be after autoconfig */

	mm_init();

	configure2();

	ipi_sysinit();

	/* Now timer is working.  Enable preemption. */
	kpreempt_enable();

	/* Get the threads going and into any sleeps before continuing. */
	yield();

	/* Enable deferred processing of RNG samples */
	rnd_init_softint();

#ifdef RND_PRINTF
	/* Enable periodic injection of console output into entropy pool */
	kprintf_init_callout();
#endif

#ifdef SYSVSHM
	/* Initialize System V style shared memory. */
	shminit();
#endif

	vmem_rehash_start();	/* must be before exec_init */

	/* Initialize exec structures */
	exec_init(1);		/* seminit calls exithook_establish() */

#ifdef SYSVSEM
	/* Initialize System V style semaphores. */
	seminit();
#endif

#ifdef SYSVMSG
	/* Initialize System V style message queues. */
	msginit();
#endif

#if NVERIEXEC > 0
	/*
	 * Initialise the Veriexec subsystem.
	 */
	veriexec_init();
#endif /* NVERIEXEC > 0 */

#if defined(PAX_MPROTECT) || defined(PAX_SEGVGUARD) || defined(PAX_ASLR)
	pax_init();
#endif /* PAX_MPROTECT || PAX_SEGVGUARD || PAX_ASLR */

#ifdef	IPSEC
	/* Attach network crypto subsystem */
	ipsec_attach();
#endif

	/*
	 * Initialize protocols.  Block reception of incoming packets
	 * until everything is ready.
	 */
	s = splnet();
	ifinit();
#if NETHER > 0
	lltableinit();
#endif
	domaininit(true);
	if_attachdomain();
	splx(s);

#ifdef GPROF
	/* Initialize kernel profiling. */
	kmstartup();
#endif

	/* Initialize system accounting. */
	acct_init();

#ifndef PIPE_SOCKETPAIR
	/* Initialize pipes. */
	pipe_init();
#endif

#ifdef KTRACE
	/* Initialize ktrace. */
	ktrinit();
#endif

#ifdef PTRACE
	/* Initialize ptrace. */
	ptrace_init();
#endif /* PTRACE */

	machdep_init();

	procinit_sysctl();

	/*
	 * Create process 1 (init(8)).  We do this now, as Unix has
	 * historically had init be process 1, and changing this would
	 * probably upset a lot of people.
	 *
	 * Note that process 1 won't immediately exec init(8), but will
	 * wait for us to inform it that the root file system has been
	 * mounted.
	 */
	if (fork1(l, 0, SIGCHLD, NULL, 0, start_init, NULL, NULL, &initproc))
		panic("fork init");

	/*
	 * Load any remaining builtin modules, and hand back temporary
	 * storage to the VM system.  Then require force when loading any
	 * remaining un-init'ed built-in modules to avoid later surprises.
	 */
	module_init_class(MODULE_CLASS_ANY);
	module_builtin_require_force();

	/*
	 * Finalize configuration now that all real devices have been
	 * found.  This needs to be done before the root device is
	 * selected, since finalization may create the root device.
	 */
	config_finalize();

	sysctl_finalize();

	/*
	 * Now that autoconfiguration has completed, we can determine
	 * the root and dump devices.
	 */
	cpu_rootconf();
	cpu_dumpconf();

	/* Mount the root file system. */
	do {
		domountroothook(root_device);
		if ((error = vfs_mountroot())) {
			printf("cannot mount root, error = %d\n", error);
			boothowto |= RB_ASKNAME;
			setroot(root_device,
			    (rootdev != NODEV) ? DISKPART(rootdev) : 0);
		}
	} while (error != 0);
	mountroothook_destroy();

	configure3();

	/*
	 * Initialise the time-of-day clock, passing the time recorded
	 * in the root filesystem (if any) for use by systems that
	 * don't have a non-volatile time-of-day device.
	 */
	inittodr(rootfstime);

	/*
	 * Now can look at time, having had a chance to verify the time
	 * from the file system.  Reset l->l_rtime as it may have been
	 * munched in mi_switch() after the time got set.
	 */
	getnanotime(&time);
	boottime = time;

	mutex_enter(proc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		KASSERT((p->p_flag & PK_MARKER) == 0);
		mutex_enter(p->p_lock);
		TIMESPEC_TO_TIMEVAL(&p->p_stats->p_start, &time);
		LIST_FOREACH(l, &p->p_lwps, l_sibling) {
			lwp_lock(l);
			memset(&l->l_rtime, 0, sizeof(l->l_rtime));
			lwp_unlock(l);
		}
		mutex_exit(p->p_lock);
	}
	mutex_exit(proc_lock);
	binuptime(&curlwp->l_stime);

	for (CPU_INFO_FOREACH(cii, ci)) {
		ci->ci_schedstate.spc_lastmod = time_second;
	}

	/* Create the pageout daemon kernel thread. */
	uvm_swap_init();
	if (kthread_create(PRI_PGDAEMON, KTHREAD_MPSAFE, NULL, uvm_pageout,
	    NULL, NULL, "pgdaemon"))
		panic("fork pagedaemon");

	/* Create the filesystem syncer kernel thread. */
	if (kthread_create(PRI_IOFLUSH, KTHREAD_MPSAFE, NULL, sched_sync,
	    NULL, NULL, "ioflush"))
		panic("fork syncer");

	/* Create the aiodone daemon kernel thread. */
	if (workqueue_create(&uvm.aiodone_queue, "aiodoned",
	    uvm_aiodone_worker, NULL, PRI_VM, IPL_NONE, WQ_MPSAFE))
		panic("fork aiodoned");

	/* Wait for final configure threads to complete. */
	config_finalize_mountroot();

	/*
	 * Okay, now we can let init(8) exec!  It's off to userland!
	 */
	mutex_enter(proc_lock);
	start_init_exec = 1;
	cv_broadcast(&lbolt);
	mutex_exit(proc_lock);

	/* The scheduler is an infinite loop. */
	uvm_scheduler();
	/* NOTREACHED */
}

/*
 * Configure the system's hardware.
 */
static void
configure(void)
{

	/* Initialize autoconf data structures. */
	config_init_mi();
	/*
	 * XXX
	 * callout_setfunc() requires mutex(9) so it can't be in config_init()
	 * on amiga and atari which use config_init() and autoconf(9) fucntions
	 * to initialize console devices.
	 */
	config_twiddle_init();

	pmf_init();
#if NDRVCTL > 0
	drvctl_init();
#endif

	userconf_init();
	if (boothowto & RB_USERCONF)
		userconf_prompt();

	if ((boothowto & (AB_SILENT|AB_VERBOSE)) == AB_SILENT) {
		printf_nolog("Detecting hardware...");
	}

	/*
	 * Do the machine-dependent portion of autoconfiguration.  This
	 * sets the configuration machinery here in motion by "finding"
	 * the root bus.  When this function returns, we expect interrupts
	 * to be enabled.
	 */
	cpu_configure();
}

static void
configure2(void)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	int s;

	/*
	 * Now that we've found all the hardware, start the real time
	 * and statistics clocks.
	 */
	initclocks();

	cold = 0;	/* clocks are running, we're warm now! */
	s = splsched();
	curcpu()->ci_schedstate.spc_flags |= SPCF_RUNNING;
	splx(s);

	/* Boot the secondary processors. */
	for (CPU_INFO_FOREACH(cii, ci)) {
		uvm_cpu_attach(ci);
	}
	mp_online = true;
#if defined(MULTIPROCESSOR)
	cpu_boot_secondary_processors();
#endif

	/* Setup the runqueues and scheduler. */
	runq_init();
	synch_init();

	/*
	 * Bus scans can make it appear as if the system has paused, so
	 * twiddle constantly while config_interrupts() jobs are running.
	 */
	config_twiddle_fn(NULL);

	/*
	 * Create threads to call back and finish configuration for
	 * devices that want interrupts enabled.
	 */
	config_create_interruptthreads();
}

static void
configure3(void)
{

	/*
	 * Create threads to call back and finish configuration for
	 * devices that want the mounted root file system.
	 */
	config_create_mountrootthreads();

	/* Get the threads going and into any sleeps before continuing. */
	yield();
}

static void
rootconf_handle_wedges(void)
{
	struct partinfo dpart;
	struct partition *p;
	struct vnode *vp;
	daddr_t startblk;
	uint64_t nblks;
	device_t dev; 
	int error;

	if (booted_nblks) {
		/*
		 * bootloader passed geometry
		 */
		dev      = booted_device;
		startblk = booted_startblk;
		nblks    = booted_nblks;

		/*
		 * keep booted_device and booted_partition
		 * in case the kernel doesn't identify a wedge
		 */
	} else {
		/*
		 * bootloader passed partition number
		 *
		 * We cannot ask the partition device directly when it is
		 * covered by a wedge. Instead we look up the geometry in
		 * the disklabel.
		 */
		vp = opendisk(booted_device);

		if (vp == NULL)
			return;

		error = VOP_IOCTL(vp, DIOCGPART, &dpart, FREAD, NOCRED);
		VOP_CLOSE(vp, FREAD, NOCRED);
		vput(vp);
		if (error)
			return;

		KASSERT(booted_partition >= 0
			&& booted_partition < MAXPARTITIONS);

		p = &dpart.disklab->d_partitions[booted_partition];

		dev      = booted_device;
		startblk = p->p_offset;
		nblks    = p->p_size;
	}

	dev = dkwedge_find_partition(dev, startblk, nblks);
	if (dev != NULL) {
		booted_device = dev;
		booted_partition = 0;
	}
}

void
rootconf(void)
{
	if (booted_device != NULL)
		rootconf_handle_wedges();

	setroot(booted_device, booted_partition);
}

static void
check_console(struct lwp *l)
{
	struct vnode *vp;
	int error;

	error = namei_simple_kernel("/dev/console",
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error == 0)
		vrele(vp);
	else if (error == ENOENT)
		printf("warning: no /dev/console\n");
	else
		printf("warning: lookup /dev/console: error %d\n", error);
}

/*
 * List of paths to try when searching for "init".
 */
static const char * const initpaths[] = {
	"/sbin/init",
	"/sbin/oinit",
	"/sbin/init.bak",
	"/rescue/init",
	NULL,
};

/*
 * Start the initial user process; try exec'ing each pathname in "initpaths".
 * The program is invoked with one argument containing the boot flags.
 */
static void
start_init(void *arg)
{
	struct lwp *l = arg;
	struct proc *p = l->l_proc;
	vaddr_t addr;
	struct sys_execve_args /* {
		syscallarg(const char *) path;
		syscallarg(char * const *) argp;
		syscallarg(char * const *) envp;
	} */ args;
	int options, i, error;
	register_t retval[2];
	char flags[4], *flagsp;
	const char *path, *slash;
	char *ucp, **uap, *arg0, *arg1 = NULL;
	char ipath[129];
	int ipx, len;

	/*
	 * Now in process 1.
	 */
	strncpy(p->p_comm, "init", MAXCOMLEN);

	/*
	 * Wait for main() to tell us that it's safe to exec.
	 */
	mutex_enter(proc_lock);
	while (start_init_exec == 0)
		cv_wait(&lbolt, proc_lock);
	mutex_exit(proc_lock);

	/*
	 * This is not the right way to do this.  We really should
	 * hand-craft a descriptor onto /dev/console to hand to init,
	 * but that's a _lot_ more work, and the benefit from this easy
	 * hack makes up for the "good is the enemy of the best" effect.
	 */
	check_console(l);

	/*
	 * Need just enough stack to hold the faked-up "execve()" arguments.
	 */
	addr = (vaddr_t)STACK_ALLOC(USRSTACK, PAGE_SIZE);
	if (uvm_map(&p->p_vmspace->vm_map, &addr, PAGE_SIZE,
                    NULL, UVM_UNKNOWN_OFFSET, 0,
                    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_COPY,
		    UVM_ADV_NORMAL,
                    UVM_FLAG_FIXED|UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW)) != 0)
		panic("init: couldn't allocate argument space");
	p->p_vmspace->vm_maxsaddr = (void *)STACK_MAX(addr, PAGE_SIZE);

	ipx = 0;
	while (1) {
		if (boothowto & RB_ASKNAME) {
			printf("init path");
			if (initpaths[ipx])
				printf(" (default %s)", initpaths[ipx]);
			printf(": ");
			len = cngetsn(ipath, sizeof(ipath)-1);
			if (len == 4 && strcmp(ipath, "halt") == 0) {
				cpu_reboot(RB_HALT, NULL);
			} else if (len == 6 && strcmp(ipath, "reboot") == 0) {
				cpu_reboot(0, NULL);
#if defined(DDB)
			} else if (len == 3 && strcmp(ipath, "ddb") == 0) {
				console_debugger();
				continue;
#endif
			} else if (len > 0 && ipath[0] == '/') {
				ipath[len] = '\0';
				path = ipath;
			} else if (len == 0 && initpaths[ipx] != NULL) {
				path = initpaths[ipx++];
			} else {
				printf("use absolute path, ");
#if defined(DDB)
				printf("\"ddb\", ");
#endif
				printf("\"halt\", or \"reboot\"\n");
				continue;
			}
		} else {
			if ((path = initpaths[ipx++]) == NULL) {
				ipx = 0;
				boothowto |= RB_ASKNAME;
				continue;
			}
		}

		ucp = (char *)USRSTACK;

		/*
		 * Construct the boot flag argument.
		 */
		flagsp = flags;
		*flagsp++ = '-';
		options = 0;

		if (boothowto & RB_SINGLE) {
			*flagsp++ = 's';
			options = 1;
		}
#ifdef notyet
		if (boothowto & RB_FASTBOOT) {
			*flagsp++ = 'f';
			options = 1;
		}
#endif

		/*
		 * Move out the flags (arg 1), if necessary.
		 */
		if (options != 0) {
			*flagsp++ = '\0';
			i = flagsp - flags;
#ifdef DEBUG
			aprint_normal("init: copying out flags `%s' %d\n", flags, i);
#endif
			arg1 = STACK_ALLOC(ucp, i);
			ucp = STACK_MAX(arg1, i);
			(void)copyout((void *)flags, arg1, i);
		}

		/*
		 * Move out the file name (also arg 0).
		 */
		i = strlen(path) + 1;
#ifdef DEBUG
		aprint_normal("init: copying out path `%s' %d\n", path, i);
#else
		if (boothowto & RB_ASKNAME || path != initpaths[0])
			printf("init: trying %s\n", path);
#endif
		arg0 = STACK_ALLOC(ucp, i);
		ucp = STACK_MAX(arg0, i);
		(void)copyout(path, arg0, i);

		/*
		 * Move out the arg pointers.
		 */
		ucp = (void *)STACK_ALIGN(ucp, STACK_ALIGNBYTES);
		uap = (char **)STACK_ALLOC(ucp, sizeof(char *) * 3);
		SCARG(&args, path) = arg0;
		SCARG(&args, argp) = uap;
		SCARG(&args, envp) = NULL;
		slash = strrchr(path, '/');
		if (slash)
			(void)suword((void *)uap++,
			    (long)arg0 + (slash + 1 - path));
		else
			(void)suword((void *)uap++, (long)arg0);
		if (options != 0)
			(void)suword((void *)uap++, (long)arg1);
		(void)suword((void *)uap++, 0);	/* terminator */

		/*
		 * Now try to exec the program.  If can't for any reason
		 * other than it doesn't exist, complain.
		 */
		error = sys_execve(l, &args, retval);
		if (error == 0 || error == EJUSTRETURN) {
			KERNEL_UNLOCK_LAST(l);
			return;
		}
		printf("exec %s: error %d\n", path, error);
	}
	printf("init: not found\n");
	panic("no init");
}

/*
 * calculate cache size (in bytes) from physmem and vsize.
 */
vaddr_t
calc_cache_size(vsize_t vsize, int pct, int va_pct)
{
	paddr_t t;

	/* XXX should consider competing cache if any */
	/* XXX should consider submaps */
	t = (uintmax_t)physmem * pct / 100 * PAGE_SIZE;
	if (vsize != 0) {
		vsize = (uintmax_t)vsize * va_pct / 100;
		if (t > vsize) {
			t = vsize;
		}
	}
	return t;
}

/*
 * Print the system start up banner.
 *
 * - Print a limited banner if AB_SILENT.
 * - Always send normal banner to the log.
 */
#define MEM_PBUFSIZE	sizeof("99999 MB")

void
banner(void)
{
	static char notice[] = " Notice: this software is "
	    "protected by copyright";
	char pbuf[81];
	void (*pr)(const char *, ...) __printflike(1, 2);
	int i;

	if ((boothowto & AB_SILENT) != 0) {
		snprintf(pbuf, sizeof(pbuf), "%s %s (%s)",
		    ostype, osrelease, kernel_ident);
		printf_nolog("%s", pbuf);
		for (i = 80 - strlen(pbuf) - sizeof(notice); i > 0; i--)
			printf(" ");
		printf_nolog("%s\n", notice);
		pr = aprint_normal;
	} else {
		pr = printf;
	}

	memset(pbuf, 0, sizeof(pbuf));
	(*pr)("%s%s", copyright, version);
	format_bytes(pbuf, MEM_PBUFSIZE, ctob((uint64_t)physmem));
	(*pr)("total memory = %s\n", pbuf);
	format_bytes(pbuf, MEM_PBUFSIZE, ctob((uint64_t)uvmexp.free));
	(*pr)("avail memory = %s\n", pbuf);
}
