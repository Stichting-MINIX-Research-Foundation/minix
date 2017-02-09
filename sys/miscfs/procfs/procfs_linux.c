/*      $NetBSD: procfs_linux.c,v 1.71 2015/07/24 13:02:52 maxv Exp $      */

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_linux.c,v 1.71 2015/07/24 13:02:52 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/tty.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/filedesc.h>

#include <miscfs/procfs/procfs.h>

#include <compat/linux/common/linux_exec.h>
#include <compat/linux32/common/linux32_sysctl.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>

extern struct devsw_conv *devsw_conv;
extern int max_devsw_convs;

#define PGTOB(p)	((unsigned long)(p) << PAGE_SHIFT)
#define PGTOKB(p)	((unsigned long)(p) << (PAGE_SHIFT - 10))

#define LBFSZ (8 * 1024)

static void
get_proc_size_info(struct lwp *l, unsigned long *stext, unsigned long *etext, unsigned long *sstack)
{
	struct proc *p = l->l_proc;
	struct vmspace *vm;
	struct vm_map *map;
	struct vm_map_entry *entry;

	*stext = 0;
	*etext = 0;
	*sstack = 0;

	proc_vmspace_getref(p, &vm);
	map = &vm->vm_map;
	vm_map_lock_read(map);

	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {
		if (UVM_ET_ISSUBMAP(entry))
			continue;
		/* assume text is the first entry */
		if (*stext == *etext) {
			*stext = entry->start;
			*etext = entry->end;
			break;
		}
	}
#if defined(LINUX_USRSTACK32) && defined(USRSTACK32)
	if (strcmp(p->p_emul->e_name, "linux32") == 0 &&
	    LINUX_USRSTACK32 < USRSTACK32)
		*sstack = (unsigned long)LINUX_USRSTACK32;
	else
#endif
#ifdef LINUX_USRSTACK
	if (strcmp(p->p_emul->e_name, "linux") == 0 &&
	    LINUX_USRSTACK < USRSTACK)
		*sstack = (unsigned long)LINUX_USRSTACK;
	else
#endif
#ifdef	USRSTACK32
	if (strstr(p->p_emul->e_name, "32") != NULL)
		*sstack = (unsigned long)USRSTACK32;
	else
#endif
		*sstack = (unsigned long)USRSTACK;

	/*
	 * jdk 1.6 compares low <= addr && addr < high
	 * if we put addr == high, then the test fails
	 * so eat one page.
	 */
	*sstack -= PAGE_SIZE;

	vm_map_unlock_read(map);
	uvmspace_free(vm);
}

/*
 * Linux compatible /proc/meminfo. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_domeminfo(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	char *bf;
	int len;
	int error = 0;

	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	len = snprintf(bf, LBFSZ,
		"        total:    used:    free:  shared: buffers: cached:\n"
		"Mem:  %8lu %8lu %8lu %8lu %8lu %8lu\n"
		"Swap: %8lu %8lu %8lu\n"
		"MemTotal:  %8lu kB\n"
		"MemFree:   %8lu kB\n"
		"MemShared: %8lu kB\n"
		"Buffers:   %8lu kB\n"
		"Cached:    %8lu kB\n"
		"SwapTotal: %8lu kB\n"
		"SwapFree:  %8lu kB\n",
		PGTOB(uvmexp.npages),
		PGTOB(uvmexp.npages - uvmexp.free),
		PGTOB(uvmexp.free),
		0L,
		PGTOB(uvmexp.filepages),
		PGTOB(uvmexp.anonpages + uvmexp.filepages + uvmexp.execpages),
		PGTOB(uvmexp.swpages),
		PGTOB(uvmexp.swpginuse),
		PGTOB(uvmexp.swpages - uvmexp.swpginuse),
		PGTOKB(uvmexp.npages),
		PGTOKB(uvmexp.free),
		0L,
		PGTOKB(uvmexp.filepages),
		PGTOKB(uvmexp.anonpages + uvmexp.filepages + uvmexp.execpages),
		PGTOKB(uvmexp.swpages),
		PGTOKB(uvmexp.swpages - uvmexp.swpginuse));

	if (len == 0)
		goto out;

	error = uiomove_frombuf(bf, len, uio);
out:
	free(bf, M_TEMP);
	return error;
}

/*
 * Linux compatible /proc/devices. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_dodevices(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	char *bf;
	int offset = 0;
	int i, error = ENAMETOOLONG;

	/* XXX elad - may need filtering. */

	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	offset += snprintf(&bf[offset], LBFSZ - offset, "Character devices:\n");
	if (offset >= LBFSZ)
		goto out;

	mutex_enter(&device_lock);
	for (i = 0; i < max_devsw_convs; i++) {
		if ((devsw_conv[i].d_name == NULL) || 
		    (devsw_conv[i].d_cmajor == -1))
			continue;

		offset += snprintf(&bf[offset], LBFSZ - offset, 
		    "%3d %s\n", devsw_conv[i].d_cmajor, devsw_conv[i].d_name);
		if (offset >= LBFSZ) {
			mutex_exit(&device_lock);
			goto out;
		}
	}

	offset += snprintf(&bf[offset], LBFSZ - offset, "\nBlock devices:\n");
	if (offset >= LBFSZ) {
		mutex_exit(&device_lock);
		goto out;
	}

	for (i = 0; i < max_devsw_convs; i++) {
		if ((devsw_conv[i].d_name == NULL) || 
		    (devsw_conv[i].d_bmajor == -1))
			continue;

		offset += snprintf(&bf[offset], LBFSZ - offset, 
		    "%3d %s\n", devsw_conv[i].d_bmajor, devsw_conv[i].d_name);
		if (offset >= LBFSZ) {
			mutex_exit(&device_lock);
			goto out;
		}
	}
	mutex_exit(&device_lock);

	error = uiomove_frombuf(bf, offset, uio);
out:
	free(bf, M_TEMP);
	return error;
}

/*
 * Linux compatible /proc/stat. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_docpustat(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	char		*bf;
	int	 	 error;
	int	 	 len;
#if defined(MULTIPROCESSOR)
        struct cpu_info *ci;
        CPU_INFO_ITERATOR cii;
#endif
	int	 	 i;
	uint64_t	nintr;
	uint64_t	nswtch;

	error = ENAMETOOLONG;
	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	len = snprintf(bf, LBFSZ,
		"cpu %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
		curcpu()->ci_schedstate.spc_cp_time[CP_USER],
		curcpu()->ci_schedstate.spc_cp_time[CP_NICE],
		curcpu()->ci_schedstate.spc_cp_time[CP_SYS] /*+ [CP_INTR]*/,
		curcpu()->ci_schedstate.spc_cp_time[CP_IDLE]);
	if (len == 0)
		goto out;

#if defined(MULTIPROCESSOR)
#define ALLCPUS	CPU_INFO_FOREACH(cii, ci)
#define CPUNAME	ci
#else
#define ALLCPUS	; i < 1 ;
#define CPUNAME	curcpu()
#endif

	i = 0;
	nintr = 0;
	nswtch = 0;
	for (ALLCPUS) {
		len += snprintf(&bf[len], LBFSZ - len, 
			"cpu%d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64
			"\n", i,
			CPUNAME->ci_schedstate.spc_cp_time[CP_USER],
			CPUNAME->ci_schedstate.spc_cp_time[CP_NICE],
			CPUNAME->ci_schedstate.spc_cp_time[CP_SYS],
			CPUNAME->ci_schedstate.spc_cp_time[CP_IDLE]);
		if (len >= LBFSZ)
			goto out;
		i += 1;
		nintr += CPUNAME->ci_data.cpu_nintr;
		nswtch += CPUNAME->ci_data.cpu_nswtch;
	}

	len += snprintf(&bf[len], LBFSZ - len,
			"disk 0 0 0 0\n"
			"page %u %u\n"
			"swap %u %u\n"
			"intr %"PRIu64"\n"
			"ctxt %"PRIu64"\n"
			"btime %"PRId64"\n",
			uvmexp.pageins, uvmexp.pdpageouts,
			uvmexp.pgswapin, uvmexp.pgswapout,
			nintr,
			nswtch,
			boottime.tv_sec);
	if (len >= LBFSZ)
		goto out;

	error = uiomove_frombuf(bf, len, uio);
out:
	free(bf, M_TEMP);
	return error;
}

/*
 * Linux compatible /proc/loadavg. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_doloadavg(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	char	*bf;
	int 	 error;
	int 	 len;

	error = ENAMETOOLONG;
	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	averunnable.fscale = FSCALE;
	len = snprintf(bf, LBFSZ,
	        "%d.%02d %d.%02d %d.%02d %d/%d %d\n",
		(int)(averunnable.ldavg[0] / averunnable.fscale),
		(int)(averunnable.ldavg[0] * 100 / averunnable.fscale % 100),
		(int)(averunnable.ldavg[1] / averunnable.fscale),
		(int)(averunnable.ldavg[1] * 100 / averunnable.fscale % 100),
		(int)(averunnable.ldavg[2] / averunnable.fscale),
		(int)(averunnable.ldavg[2] * 100 / averunnable.fscale % 100),
		1,		/* number of ONPROC processes */
		nprocs,
		30000);		/* last pid */
	if (len == 0)
		goto out;

	error = uiomove_frombuf(bf, len, uio);
out:
	free(bf, M_TEMP);
	return error;
}

/*
 * Linux compatible /proc/<pid>/statm. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_do_pid_statm(struct lwp *curl, struct lwp *l,
    struct pfsnode *pfs, struct uio *uio)
{
	struct vmspace	*vm;
	struct proc	*p = l->l_proc;
	struct rusage	*ru = &p->p_stats->p_ru;
	char		*bf;
	int	 	 error;
	int	 	 len;

	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	/* XXX - we use values from vmspace, since dsl says that ru figures
	   are always 0 except for zombies. See kvm_proc.c::kvm_getproc2() */
	if ((error = proc_vmspace_getref(p, &vm)) != 0) {
		goto out;
	}

	len = snprintf(bf, LBFSZ,
	        "%lu %lu %lu %lu %lu %lu %lu\n",
		(unsigned long)(vm->vm_tsize + vm->vm_dsize + vm->vm_ssize), /* size */
		(unsigned long)(vm->vm_rssize),	/* resident */
		(unsigned long)(ru->ru_ixrss),	/* shared */
		(unsigned long)(vm->vm_tsize),	/* text size in pages */
		(unsigned long)(vm->vm_dsize),	/* data size in pages */
		(unsigned long)(vm->vm_ssize),	/* stack size in pages */
		(unsigned long) 0);

	uvmspace_free(vm);

	if (len == 0)
		goto out;

	error = uiomove_frombuf(bf, len, uio);
out:
	free(bf, M_TEMP);
	return error;
}

#define UTIME2TICKS(s,u)	(((uint64_t)(s) * 1000000 + (u)) / 10000)

/*
 * Linux compatible /proc/<pid>/stat. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_do_pid_stat(struct lwp *curl, struct lwp *l,
    struct pfsnode *pfs, struct uio *uio)
{
	char *bf;
	struct proc *p = l->l_proc;
	int len;
	struct rusage *cru = &p->p_stats->p_cru;
	unsigned long stext = 0, etext = 0, sstack = 0;
	struct timeval rt;
	struct vmspace	*vm;
	struct kinfo_proc2 ki;
	int error;

	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	if ((error = proc_vmspace_getref(p, &vm)) != 0) {
		goto out;
	}

	get_proc_size_info(l, &stext, &etext, &sstack);

	mutex_enter(proc_lock);
	mutex_enter(p->p_lock);

	fill_kproc2(p, &ki, false);
	calcru(p, NULL, NULL, NULL, &rt);

	len = snprintf(bf, LBFSZ,
	    "%d (%s) %c %d %d %d %u %d "
	    "%u "
	    "%"PRIu64" %lu %"PRIu64" %lu %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" "
	    "%d %d %"PRIu64" "
	    "%lld %"PRIu64" %"PRId64" %lu %"PRIu64" "
	    "%lu %lu %lu "
	    "%u %u "
	    "%u %u %u %u "
	    "%"PRIu64" %"PRIu64" %"PRIu64" %d %"PRIu64"\n",

	    ki.p_pid,						/* 1 pid */
	    ki.p_comm,						/* 2 tcomm */
	    "0RRSTZXR8"[(ki.p_stat > 8) ? 0 : (int)ki.p_stat],	/* 3 state */
	    ki.p_ppid,						/* 4 ppid */
	    ki.p__pgid,						/* 5 pgrp */
	    ki.p_sid,						/* 6 sid */
	    (ki.p_tdev != (uint32_t)NODEV) ? ki.p_tdev : 0,	/* 7 tty_nr */
	    ki.p_tpgid,						/* 8 tty_pgrp */

	    ki.p_flag,						/* 9 flags */

	    ki.p_uru_minflt,					/* 10 min_flt */
	    cru->ru_minflt,
	    ki.p_uru_majflt,					/* 12 maj_flt */
	    cru->ru_majflt,
	    UTIME2TICKS(ki.p_uutime_sec, ki.p_uutime_usec),	/* 14 utime */
	    UTIME2TICKS(ki.p_ustime_sec, ki.p_ustime_usec),	/* 15 stime */
	    UTIME2TICKS(cru->ru_utime.tv_sec, cru->ru_utime.tv_usec), /* 16 cutime */
	    UTIME2TICKS(cru->ru_stime.tv_sec, cru->ru_stime.tv_usec), /* 17 cstime */

	    ki.p_priority,				/* XXX: 18 priority */
	    ki.p_nice - NZERO,				/* 19 nice */
	    ki.p_nlwps,					/* 20 num_threads */

	    (long long)rt.tv_sec,
	    UTIME2TICKS(ki.p_ustart_sec, ki.p_ustart_usec), /* 22 start_time */
	    ki.p_vm_msize,				/* 23 vsize */
	    PGTOKB(ki.p_vm_rssize),			/* 24 rss */
	    p->p_rlimit[RLIMIT_RSS].rlim_cur,		/* 25 rsslim */

	    stext,					/* 26 start_code */
	    etext,					/* 27 end_code */
	    sstack,					/* 28 start_stack */

	    0,						/* XXX: 29 esp */
	    0,						/* XXX: 30 eip */

	    ki.p_siglist.__bits[0],			/* XXX: 31 pending */
	    0,						/* XXX: 32 blocked */
	    ki.p_sigignore.__bits[0],		/* 33 sigign */
	    ki.p_sigcatch.__bits[0],		/* 34 sigcatch */

	    ki.p_wchan,					/* 35 wchan */
	    ki.p_uru_nvcsw,
	    ki.p_uru_nivcsw,
	    ki.p_exitsig,				/* 38 exit_signal */
	    ki.p_cpuid);				/* 39 task_cpu */

	mutex_exit(p->p_lock);
	mutex_exit(proc_lock);

	uvmspace_free(vm);

	if (len == 0)
		goto out;

	error = uiomove_frombuf(bf, len, uio);
out:
	free(bf, M_TEMP);
	return error;
}

int
procfs_docpuinfo(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	size_t len = LBFSZ;
	char *bf = NULL;
	int error;

	do {
		if (bf)
			free(bf, M_TEMP);
		bf = malloc(len, M_TEMP, M_WAITOK);
	} while (procfs_getcpuinfstr(bf, &len) < 0);

	if (len == 0) {
		error = 0;
		goto done;
	}

	error = uiomove_frombuf(bf, len, uio);
done:
	free(bf, M_TEMP);
	return error;
}

int
procfs_douptime(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	char *bf;
	int len;
	struct timeval runtime;
	u_int64_t idle;
	int error = 0;

	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	microuptime(&runtime);
	idle = curcpu()->ci_schedstate.spc_cp_time[CP_IDLE];
	len = snprintf(bf, LBFSZ,
	    "%lld.%02lu %" PRIu64 ".%02" PRIu64 "\n",
	    (long long)runtime.tv_sec, (long)runtime.tv_usec / 10000,
	    idle / hz, (((idle % hz) * 100) / hz) % 100);

	if (len == 0)
		goto out;

	error = uiomove_frombuf(bf, len, uio);
out:
	free(bf, M_TEMP);
	return error;
}

static int
procfs_format_sfs(char **mtab, size_t *mlen, char *buf, size_t blen,
    const struct statvfs *sfs, struct lwp *curl, int suser)
{
	const char *fsname;

	/* Linux uses different names for some filesystems */
	fsname = sfs->f_fstypename;
	if (strcmp(fsname, "procfs") == 0)
		fsname = "proc";
	else if (strcmp(fsname, "ext2fs") == 0)
		fsname = "ext2";

	blen = snprintf(buf, blen, "%s %s %s %s%s%s%s%s%s 0 0\n",
	    sfs->f_mntfromname, sfs->f_mntonname, fsname,
	    (sfs->f_flag & ST_RDONLY) ? "ro" : "rw",
	    (sfs->f_flag & ST_NOSUID) ? ",nosuid" : "",
	    (sfs->f_flag & ST_NOEXEC) ? ",noexec" : "",
	    (sfs->f_flag & ST_NODEV) ? ",nodev" : "",
	    (sfs->f_flag & ST_SYNCHRONOUS) ? ",sync" : "",
	    (sfs->f_flag & ST_NOATIME) ? ",noatime" : "");

	*mtab = realloc(*mtab, *mlen + blen, M_TEMP, M_WAITOK);
	memcpy(*mtab + *mlen, buf, blen);
	*mlen += blen;
	return sfs->f_mntonname[0] == '/' && sfs->f_mntonname[1] == '\0';
}

int
procfs_domounts(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	char *bf, *mtab = NULL;
	size_t mtabsz = 0;
	struct mount *mp, *nmp;
	int error = 0, root = 0;
	struct cwdinfo *cwdi = curl->l_proc->p_cwdi;

	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	mutex_enter(&mountlist_lock);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		struct statvfs sfs;

		if (vfs_busy(mp, &nmp))
			continue;

		if ((error = dostatvfs(mp, &sfs, curl, MNT_WAIT, 0)) == 0)
			root |= procfs_format_sfs(&mtab, &mtabsz, bf, LBFSZ,
			    &sfs, curl, 0);

		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);

	/*
	 * If we are inside a chroot that is not itself a mount point,
	 * fake a root entry.
	 */
	if (!root && cwdi->cwdi_rdir)
		(void)procfs_format_sfs(&mtab, &mtabsz, bf, LBFSZ,
		    &cwdi->cwdi_rdir->v_mount->mnt_stat, curl, 1);

	free(bf, M_TEMP);

	if (mtabsz > 0) {
		error = uiomove_frombuf(mtab, mtabsz, uio);
		free(mtab, M_TEMP);
	}

	return error;
}

/*
 * Linux compatible /proc/version. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_doversion(struct lwp *curl, struct proc *p,
    struct pfsnode *pfs, struct uio *uio)
{
	char *bf;
	char lostype[20], losrelease[20], lversion[80];
	const char *postype, *posrelease, *pversion;
	const char *emulname = curlwp->l_proc->p_emul->e_name;
	int len;
	int error = 0;
	int nm[4];
	size_t buflen;

	CTASSERT(EMUL_LINUX_KERN_OSTYPE == EMUL_LINUX32_KERN_OSTYPE);
	CTASSERT(EMUL_LINUX_KERN_OSRELEASE == EMUL_LINUX32_KERN_OSRELEASE);
	CTASSERT(EMUL_LINUX_KERN_VERSION == EMUL_LINUX32_KERN_VERSION);

	bf = malloc(LBFSZ, M_TEMP, M_WAITOK);

	sysctl_lock(false);

	if (strncmp(emulname, "linux", 5) == 0) {
		/*
		 * Lookup the emulation ostype, osrelease, and version.
		 * Since compat_linux and compat_linux32 can be built as
		 * modules, we use sysctl to obtain the values instead of
		 * using the symbols directly.
		 */

		if (strcmp(emulname, "linux32") == 0) {
			nm[0] = CTL_EMUL;
			nm[1] = EMUL_LINUX32;
			nm[2] = EMUL_LINUX32_KERN;
		} else {
			nm[0] = CTL_EMUL;
			nm[1] = EMUL_LINUX;
			nm[2] = EMUL_LINUX_KERN;
		}

		nm[3] = EMUL_LINUX_KERN_OSTYPE;
		buflen = sizeof(lostype);
		error = sysctl_dispatch(nm, __arraycount(nm),
		    lostype, &buflen,
		    NULL, 0, NULL, NULL, NULL);
		if (error)
			goto out;

		nm[3] = EMUL_LINUX_KERN_OSRELEASE;
		buflen = sizeof(losrelease);
		error = sysctl_dispatch(nm, __arraycount(nm),
		    losrelease, &buflen,
		    NULL, 0, NULL, NULL, NULL);
		if (error)
			goto out;

		nm[3] = EMUL_LINUX_KERN_VERSION;
		buflen = sizeof(lversion);
		error = sysctl_dispatch(nm, __arraycount(nm),
		    lversion, &buflen,
		    NULL, 0, NULL, NULL, NULL);
		if (error)
			goto out;

		postype = lostype;
		posrelease = losrelease;
		pversion = lversion;
	} else {
		postype = ostype;
		posrelease = osrelease;
		strlcpy(lversion, version, sizeof(lversion));
		if (strchr(lversion, '\n'))
			*strchr(lversion, '\n') = '\0';
		pversion = lversion;
	}

	len = snprintf(bf, LBFSZ,
		"%s version %s (%s@localhost) (gcc version %s) %s\n",
		postype, posrelease, emulname,
#ifdef __VERSION__
		__VERSION__,
#else
		"unknown",
#endif
		pversion);

	if (len == 0)
		goto out;

	error = uiomove_frombuf(bf, len, uio);
out:
	free(bf, M_TEMP);
	sysctl_unlock();
	return error;
}
