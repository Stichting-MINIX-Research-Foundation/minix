/*	$NetBSD: uvm_stat.c,v 1.37 2011/05/17 04:18:07 mrg Exp $	 */

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: Id: uvm_stat.c,v 1.1.2.3 1997/12/19 15:01:00 mrg Exp
 */

/*
 * uvm_stat.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_stat.c,v 1.37 2011/05/17 04:18:07 mrg Exp $");

#include "opt_readahead.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpu.h>

#include <uvm/uvm.h>
#include <uvm/uvm_ddb.h>

#ifdef DDB

/*
 * uvmexp_print: ddb hook to print interesting uvm counters
 */
void
uvmexp_print(void (*pr)(const char *, ...)
    __attribute__((__format__(__printf__,1,2))))
{
	int active, inactive;
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	uvm_estimatepageable(&active, &inactive);

	(*pr)("Current UVM status:\n");
	(*pr)("  pagesize=%d (0x%x), pagemask=0x%x, pageshift=%d\n, ncolors=%d",
	    uvmexp.pagesize, uvmexp.pagesize, uvmexp.pagemask,
	    uvmexp.pageshift, uvmexp.ncolors);
	(*pr)("  %d VM pages: %d active, %d inactive, %d wired, %d free\n",
	    uvmexp.npages, active, inactive, uvmexp.wired,
	    uvmexp.free);
	(*pr)("  pages  %d anon, %d file, %d exec\n",
	    uvmexp.anonpages, uvmexp.filepages, uvmexp.execpages);
	(*pr)("  freemin=%d, free-target=%d, wired-max=%d\n",
	    uvmexp.freemin, uvmexp.freetarg, uvmexp.wiredmax);

	for (CPU_INFO_FOREACH(cii, ci)) {
		(*pr)("  cpu%u:\n", cpu_index(ci));
		(*pr)("    faults=%" PRIu64 ", traps=%" PRIu64 ", "
		    "intrs=%" PRIu64 ", ctxswitch=%" PRIu64 "\n",
		    ci->ci_data.cpu_nfault, ci->ci_data.cpu_ntrap,
		    ci->ci_data.cpu_nintr, ci->ci_data.cpu_nswtch);
		(*pr)("    softint=%" PRIu64 ", syscalls=%" PRIu64 "\n",
		    ci->ci_data.cpu_nsoft, ci->ci_data.cpu_nsyscall);
	}

	(*pr)("  fault counts:\n");
	(*pr)("    noram=%d, noanon=%d, pgwait=%d, pgrele=%d\n",
	    uvmexp.fltnoram, uvmexp.fltnoanon, uvmexp.fltpgwait,
	    uvmexp.fltpgrele);
	(*pr)("    ok relocks(total)=%d(%d), anget(retrys)=%d(%d), "
	    "amapcopy=%d\n", uvmexp.fltrelckok, uvmexp.fltrelck,
	    uvmexp.fltanget, uvmexp.fltanretry, uvmexp.fltamcopy);
	(*pr)("    neighbor anon/obj pg=%d/%d, gets(lock/unlock)=%d/%d\n",
	    uvmexp.fltnamap, uvmexp.fltnomap, uvmexp.fltlget, uvmexp.fltget);
	(*pr)("    cases: anon=%d, anoncow=%d, obj=%d, prcopy=%d, przero=%d\n",
	    uvmexp.flt_anon, uvmexp.flt_acow, uvmexp.flt_obj, uvmexp.flt_prcopy,
	    uvmexp.flt_przero);

	(*pr)("  daemon and swap counts:\n");
	(*pr)("    woke=%d, revs=%d, scans=%d, obscans=%d, anscans=%d\n",
	    uvmexp.pdwoke, uvmexp.pdrevs, uvmexp.pdscans, uvmexp.pdobscan,
	    uvmexp.pdanscan);
	(*pr)("    busy=%d, freed=%d, reactivate=%d, deactivate=%d\n",
	    uvmexp.pdbusy, uvmexp.pdfreed, uvmexp.pdreact, uvmexp.pddeact);
	(*pr)("    pageouts=%d, pending=%d, nswget=%d\n", uvmexp.pdpageouts,
	    uvmexp.pdpending, uvmexp.nswget);
	(*pr)("    nswapdev=%d, swpgavail=%d\n",
	    uvmexp.nswapdev, uvmexp.swpgavail);
	(*pr)("    swpages=%d, swpginuse=%d, swpgonly=%d, paging=%d\n",
	    uvmexp.swpages, uvmexp.swpginuse, uvmexp.swpgonly, uvmexp.paging);
}
#endif

#if defined(READAHEAD_STATS)

#define	UVM_RA_EVCNT_DEFINE(name) \
struct evcnt uvm_ra_##name = \
EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "readahead", #name); \
EVCNT_ATTACH_STATIC(uvm_ra_##name);

UVM_RA_EVCNT_DEFINE(total);
UVM_RA_EVCNT_DEFINE(hit);
UVM_RA_EVCNT_DEFINE(miss);

#endif /* defined(READAHEAD_STATS) */
