/*	$NetBSD: uvm_meter.c,v 1.60 2012/06/02 21:36:48 dsl Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1982, 1986, 1989, 1993
 *      The Regents of the University of California.
 *
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
 *      @(#)vm_meter.c  8.4 (Berkeley) 1/4/94
 * from: Id: uvm_meter.c,v 1.1.2.1 1997/08/14 19:10:35 chuck Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_meter.c,v 1.60 2012/06/02 21:36:48 dsl Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pdpolicy.h>

/*
 * maxslp: ???? XXXCDC
 */

int maxslp = MAXSLP;	/* patchable ... */
struct loadavg averunnable;

static void uvm_total(struct vmtotal *);

/*
 * sysctl helper routine for the vm.vmmeter node.
 */
static int
sysctl_vm_meter(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct vmtotal vmtotals;

	node = *rnode;
	node.sysctl_data = &vmtotals;
	uvm_total(&vmtotals);

	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

/*
 * sysctl helper routine for the vm.uvmexp node.
 */
static int
sysctl_vm_uvmexp(SYSCTLFN_ARGS)
{
	struct sysctlnode node;

	node = *rnode;
	if (oldlenp)
		node.sysctl_size = min(*oldlenp, node.sysctl_size);

	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

static int
sysctl_vm_uvmexp2(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct uvmexp_sysctl u;
	int active, inactive;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	uvm_estimatepageable(&active, &inactive);

	memset(&u, 0, sizeof(u));

	/* Entries here are in order of uvmexp_sysctl, not uvmexp */
	u.pagesize = uvmexp.pagesize;
	u.pagemask = uvmexp.pagemask;
	u.pageshift = uvmexp.pageshift;
	u.npages = uvmexp.npages;
	u.free = uvmexp.free;
	u.active = active;
	u.inactive = inactive;
	u.paging = uvmexp.paging;
	u.wired = uvmexp.wired;
	u.zeropages = uvmexp.zeropages;
	u.reserve_pagedaemon = uvmexp.reserve_pagedaemon;
	u.reserve_kernel = uvmexp.reserve_kernel;
	u.freemin = uvmexp.freemin;
	u.freetarg = uvmexp.freetarg;
	u.inactarg = 0; /* unused */
	u.wiredmax = uvmexp.wiredmax;
	u.nswapdev = uvmexp.nswapdev;
	u.swpages = uvmexp.swpages;
	u.swpginuse = uvmexp.swpginuse;
	u.swpgonly = uvmexp.swpgonly;
	u.nswget = uvmexp.nswget;
	for (CPU_INFO_FOREACH(cii, ci)) {
		u.faults += ci->ci_data.cpu_nfault;
		u.traps += ci->ci_data.cpu_ntrap;
		u.intrs += ci->ci_data.cpu_nintr;
		u.swtch += ci->ci_data.cpu_nswtch;
		u.softs += ci->ci_data.cpu_nsoft;
		u.syscalls += ci->ci_data.cpu_nsyscall;
	}
	u.pageins = uvmexp.pageins;
	u.pgswapin = uvmexp.pgswapin;
	u.pgswapout = uvmexp.pgswapout;
	u.forks = uvmexp.forks;
	u.forks_ppwait = uvmexp.forks_ppwait;
	u.forks_sharevm = uvmexp.forks_sharevm;
	u.pga_zerohit = uvmexp.pga_zerohit;
	u.pga_zeromiss = uvmexp.pga_zeromiss;
	u.zeroaborts = uvmexp.zeroaborts;
	u.fltnoram = uvmexp.fltnoram;
	u.fltnoanon = uvmexp.fltnoanon;
	u.fltpgwait = uvmexp.fltpgwait;
	u.fltpgrele = uvmexp.fltpgrele;
	u.fltrelck = uvmexp.fltrelck;
	u.fltrelckok = uvmexp.fltrelckok;
	u.fltanget = uvmexp.fltanget;
	u.fltanretry = uvmexp.fltanretry;
	u.fltamcopy = uvmexp.fltamcopy;
	u.fltnamap = uvmexp.fltnamap;
	u.fltnomap = uvmexp.fltnomap;
	u.fltlget = uvmexp.fltlget;
	u.fltget = uvmexp.fltget;
	u.flt_anon = uvmexp.flt_anon;
	u.flt_acow = uvmexp.flt_acow;
	u.flt_obj = uvmexp.flt_obj;
	u.flt_prcopy = uvmexp.flt_prcopy;
	u.flt_przero = uvmexp.flt_przero;
	u.pdwoke = uvmexp.pdwoke;
	u.pdrevs = uvmexp.pdrevs;
	u.pdfreed = uvmexp.pdfreed;
	u.pdscans = uvmexp.pdscans;
	u.pdanscan = uvmexp.pdanscan;
	u.pdobscan = uvmexp.pdobscan;
	u.pdreact = uvmexp.pdreact;
	u.pdbusy = uvmexp.pdbusy;
	u.pdpageouts = uvmexp.pdpageouts;
	u.pdpending = uvmexp.pdpending;
	u.pddeact = uvmexp.pddeact;
	u.anonpages = uvmexp.anonpages;
	u.filepages = uvmexp.filepages;
	u.execpages = uvmexp.execpages;
	u.colorhit = uvmexp.colorhit;
	u.colormiss = uvmexp.colormiss;
	u.cpuhit = uvmexp.cpuhit;
	u.cpumiss = uvmexp.cpumiss;

	node = *rnode;
	node.sysctl_data = &u;
	node.sysctl_size = sizeof(u);
	if (oldlenp)
		node.sysctl_size = min(*oldlenp, node.sysctl_size);
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

/*
 * sysctl helper routine for uvm_pctparam.
 */
static int
uvm_sysctlpctparam(SYSCTLFN_ARGS)
{
	int t, error;
	struct sysctlnode node;
	struct uvm_pctparam *pct;

	pct = rnode->sysctl_data;
	t = pct->pct_pct;

	node = *rnode;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < 0 || t > 100)
		return EINVAL;

	error = uvm_pctparam_check(pct, t);
	if (error) {
		return error;
	}
	uvm_pctparam_set(pct, t);

	return (0);
}

/*
 * uvm_sysctl: sysctl hook into UVM system.
 */
SYSCTL_SETUP(sysctl_vm_setup, "sysctl vm subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vm", NULL,
		       NULL, 0, NULL, 0,
		       CTL_VM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "vmmeter",
		       SYSCTL_DESCR("Simple system-wide virtual memory "
				    "statistics"),
		       sysctl_vm_meter, 0, NULL, sizeof(struct vmtotal),
		       CTL_VM, VM_METER, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "loadavg",
		       SYSCTL_DESCR("System load average history"),
		       NULL, 0, &averunnable, sizeof(averunnable),
		       CTL_VM, VM_LOADAVG, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "uvmexp",
		       SYSCTL_DESCR("Detailed system-wide virtual memory "
				    "statistics"),
		       sysctl_vm_uvmexp, 0, &uvmexp, sizeof(uvmexp),
		       CTL_VM, VM_UVMEXP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "uvmexp2",
		       SYSCTL_DESCR("Detailed system-wide virtual memory "
				    "statistics (MI)"),
		       sysctl_vm_uvmexp2, 0, NULL, 0,
		       CTL_VM, VM_UVMEXP2, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT, CTLTYPE_INT, "maxslp",
		       SYSCTL_DESCR("Maximum process sleep time before being "
				    "swapped"),
		       NULL, 0, &maxslp, 0,
		       CTL_VM, VM_MAXSLP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "uspace",
		       SYSCTL_DESCR("Number of bytes allocated for a kernel "
				    "stack"),
		       NULL, USPACE, NULL, 0,
		       CTL_VM, VM_USPACE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_BOOL, "idlezero",
		       SYSCTL_DESCR("Whether try to zero pages in idle loop"),
		       NULL, 0, &vm_page_zero_enable, 0,
		       CTL_VM, CTL_CREATE, CTL_EOL);

	uvmpdpol_sysctlsetup();
}

/*
 * uvm_total: calculate the current state of the system.
 */
static void
uvm_total(struct vmtotal *totalp)
{
	struct lwp *l;
#if 0
	struct vm_map_entry *	entry;
	struct vm_map *map;
	int paging;
#endif
	int active;

	memset(totalp, 0, sizeof *totalp);

	/*
	 * calculate process statistics
	 */
	mutex_enter(proc_lock);
	LIST_FOREACH(l, &alllwp, l_list) {
		if (l->l_proc->p_flag & PK_SYSTEM)
			continue;
		switch (l->l_stat) {
		case 0:
			continue;

		case LSSLEEP:
		case LSSTOP:
			if ((l->l_flag & LW_SINTR) == 0) {
				totalp->t_dw++;
			} else if (l->l_slptime < maxslp) {
				totalp->t_sl++;
			}
			if (l->l_slptime >= maxslp)
				continue;
			break;

		case LSRUN:
		case LSONPROC:
		case LSIDL:
			totalp->t_rq++;
			if (l->l_stat == LSIDL)
				continue;
			break;
		}
		/*
		 * note active objects
		 */
#if 0
		/*
		 * XXXCDC: BOGUS!  rethink this.  in the mean time
		 * don't do it.
		 */
		paging = 0;
		vm_map_lock(map);
		for (map = &p->p_vmspace->vm_map, entry = map->header.next;
		    entry != &map->header; entry = entry->next) {
			if (entry->is_a_map || entry->is_sub_map ||
			    entry->object.uvm_obj == NULL)
				continue;
			/* XXX how to do this with uvm */
		}
		vm_map_unlock(map);
		if (paging)
			totalp->t_pw++;
#endif
	}
	mutex_exit(proc_lock);

	/*
	 * Calculate object memory usage statistics.
	 */
	uvm_estimatepageable(&active, NULL);
	totalp->t_free = uvmexp.free;
	totalp->t_vm = uvmexp.npages - uvmexp.free + uvmexp.swpginuse;
	totalp->t_avm = active + uvmexp.swpginuse;	/* XXX */
	totalp->t_rm = uvmexp.npages - uvmexp.free;
	totalp->t_arm = active;
	totalp->t_vmshr = 0;		/* XXX */
	totalp->t_avmshr = 0;		/* XXX */
	totalp->t_rmshr = 0;		/* XXX */
	totalp->t_armshr = 0;		/* XXX */
}

void
uvm_pctparam_set(struct uvm_pctparam *pct, int val)
{

	pct->pct_pct = val;
	pct->pct_scaled = val * UVM_PCTPARAM_SCALE / 100;
}

int
uvm_pctparam_get(struct uvm_pctparam *pct)
{

	return pct->pct_pct;
}

int
uvm_pctparam_check(struct uvm_pctparam *pct, int val)
{

	if (pct->pct_check == NULL) {
		return 0;
	}
	return (*pct->pct_check)(pct, val);
}

void
uvm_pctparam_init(struct uvm_pctparam *pct, int val,
    int (*fn)(struct uvm_pctparam *, int))
{

	pct->pct_check = fn;
	uvm_pctparam_set(pct, val);
}

int
uvm_pctparam_createsysctlnode(struct uvm_pctparam *pct, const char *name,
    const char *desc)
{

	return sysctl_createv(NULL, 0, NULL, NULL,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
	    CTLTYPE_INT, name, SYSCTL_DESCR(desc),
	    uvm_sysctlpctparam, 0, (void *)pct, 0, CTL_VM, CTL_CREATE, CTL_EOL);
}
