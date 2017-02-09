/*	$NetBSD: uvm_pdpolicy_clock.c,v 1.17 2012/01/30 17:21:52 para Exp $	*/
/*	NetBSD: uvm_pdaemon.c,v 1.72 2006/01/05 10:47:33 yamt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)vm_pageout.c        8.5 (Berkeley) 2/14/94
 * from: Id: uvm_pdaemon.c,v 1.1.2.32 1998/02/06 05:26:30 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#if defined(PDSIM)

#include "pdsim.h"

#else /* defined(PDSIM) */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_pdpolicy_clock.c,v 1.17 2012/01/30 17:21:52 para Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pdpolicy.h>
#include <uvm/uvm_pdpolicy_impl.h>

#endif /* defined(PDSIM) */

#define PQ_INACTIVE	PQ_PRIVATE1	/* page is in inactive list */
#define PQ_ACTIVE	PQ_PRIVATE2	/* page is in active list */

#if !defined(CLOCK_INACTIVEPCT)
#define	CLOCK_INACTIVEPCT	33
#endif /* !defined(CLOCK_INACTIVEPCT) */

struct uvmpdpol_globalstate {
	struct pglist s_activeq;	/* allocated pages, in use */
	struct pglist s_inactiveq;	/* pages between the clock hands */
	int s_active;
	int s_inactive;
	int s_inactarg;
	struct uvm_pctparam s_anonmin;
	struct uvm_pctparam s_filemin;
	struct uvm_pctparam s_execmin;
	struct uvm_pctparam s_anonmax;
	struct uvm_pctparam s_filemax;
	struct uvm_pctparam s_execmax;
	struct uvm_pctparam s_inactivepct;
};

struct uvmpdpol_scanstate {
	bool ss_first;
	bool ss_anonreact, ss_filereact, ss_execreact;
	struct vm_page *ss_nextpg;
};

static struct uvmpdpol_globalstate pdpol_state;
static struct uvmpdpol_scanstate pdpol_scanstate;

PDPOL_EVCNT_DEFINE(reactexec)
PDPOL_EVCNT_DEFINE(reactfile)
PDPOL_EVCNT_DEFINE(reactanon)

static void
clock_tune(void)
{
	struct uvmpdpol_globalstate *s = &pdpol_state;

	s->s_inactarg = UVM_PCTPARAM_APPLY(&s->s_inactivepct,
	    s->s_active + s->s_inactive);
	if (s->s_inactarg <= uvmexp.freetarg) {
		s->s_inactarg = uvmexp.freetarg + 1;
	}
}

void
uvmpdpol_scaninit(void)
{
	struct uvmpdpol_globalstate *s = &pdpol_state;
	struct uvmpdpol_scanstate *ss = &pdpol_scanstate;
	int t;
	bool anonunder, fileunder, execunder;
	bool anonover, fileover, execover;
	bool anonreact, filereact, execreact;

	/*
	 * decide which types of pages we want to reactivate instead of freeing
	 * to keep usage within the minimum and maximum usage limits.
	 */

	t = s->s_active + s->s_inactive + uvmexp.free;
	anonunder = uvmexp.anonpages <= UVM_PCTPARAM_APPLY(&s->s_anonmin, t);
	fileunder = uvmexp.filepages <= UVM_PCTPARAM_APPLY(&s->s_filemin, t);
	execunder = uvmexp.execpages <= UVM_PCTPARAM_APPLY(&s->s_execmin, t);
	anonover = uvmexp.anonpages > UVM_PCTPARAM_APPLY(&s->s_anonmax, t);
	fileover = uvmexp.filepages > UVM_PCTPARAM_APPLY(&s->s_filemax, t);
	execover = uvmexp.execpages > UVM_PCTPARAM_APPLY(&s->s_execmax, t);
	anonreact = anonunder || (!anonover && (fileover || execover));
	filereact = fileunder || (!fileover && (anonover || execover));
	execreact = execunder || (!execover && (anonover || fileover));
	if (filereact && execreact && (anonreact || uvm_swapisfull())) {
		anonreact = filereact = execreact = false;
	}
	ss->ss_anonreact = anonreact;
	ss->ss_filereact = filereact;
	ss->ss_execreact = execreact;

	ss->ss_first = true;
}

struct vm_page *
uvmpdpol_selectvictim(void)
{
	struct uvmpdpol_scanstate *ss = &pdpol_scanstate;
	struct vm_page *pg;
	kmutex_t *lock;

	KASSERT(mutex_owned(&uvm_pageqlock));

	while (/* CONSTCOND */ 1) {
		struct vm_anon *anon;
		struct uvm_object *uobj;

		if (ss->ss_first) {
			pg = TAILQ_FIRST(&pdpol_state.s_inactiveq);
			ss->ss_first = false;
		} else {
			pg = ss->ss_nextpg;
			if (pg != NULL && (pg->pqflags & PQ_INACTIVE) == 0) {
				pg = TAILQ_FIRST(&pdpol_state.s_inactiveq);
			}
		}
		if (pg == NULL) {
			break;
		}
		ss->ss_nextpg = TAILQ_NEXT(pg, pageq.queue);

		uvmexp.pdscans++;

		/*
		 * move referenced pages back to active queue and
		 * skip to next page.
		 */

		lock = uvmpd_trylockowner(pg);
		if (lock != NULL) {
			if (pmap_is_referenced(pg)) {
				uvmpdpol_pageactivate(pg);
				uvmexp.pdreact++;
				mutex_exit(lock);
				continue;
			}
			mutex_exit(lock);
		}

		anon = pg->uanon;
		uobj = pg->uobject;

		/*
		 * enforce the minimum thresholds on different
		 * types of memory usage.  if reusing the current
		 * page would reduce that type of usage below its
		 * minimum, reactivate the page instead and move
		 * on to the next page.
		 */

		if (uobj && UVM_OBJ_IS_VTEXT(uobj) && ss->ss_execreact) {
			uvmpdpol_pageactivate(pg);
			PDPOL_EVCNT_INCR(reactexec);
			continue;
		}
		if (uobj && UVM_OBJ_IS_VNODE(uobj) &&
		    !UVM_OBJ_IS_VTEXT(uobj) && ss->ss_filereact) {
			uvmpdpol_pageactivate(pg);
			PDPOL_EVCNT_INCR(reactfile);
			continue;
		}
		if ((anon || UVM_OBJ_IS_AOBJ(uobj)) && ss->ss_anonreact) {
			uvmpdpol_pageactivate(pg);
			PDPOL_EVCNT_INCR(reactanon);
			continue;
		}

		break;
	}

	return pg;
}

void
uvmpdpol_balancequeue(int swap_shortage)
{
	int inactive_shortage;
	struct vm_page *p, *nextpg;
	kmutex_t *lock;

	/*
	 * we have done the scan to get free pages.   now we work on meeting
	 * our inactive target.
	 */

	inactive_shortage = pdpol_state.s_inactarg - pdpol_state.s_inactive;
	for (p = TAILQ_FIRST(&pdpol_state.s_activeq);
	     p != NULL && (inactive_shortage > 0 || swap_shortage > 0);
	     p = nextpg) {
		nextpg = TAILQ_NEXT(p, pageq.queue);

		/*
		 * if there's a shortage of swap slots, try to free it.
		 */

		if (swap_shortage > 0 && (p->pqflags & PQ_SWAPBACKED) != 0) {
			if (uvmpd_trydropswap(p)) {
				swap_shortage--;
			}
		}

		/*
		 * if there's a shortage of inactive pages, deactivate.
		 */

		if (inactive_shortage <= 0) {
			continue;
		}

		/* no need to check wire_count as pg is "active" */
		lock = uvmpd_trylockowner(p);
		if (lock != NULL) {
			uvmpdpol_pagedeactivate(p);
			uvmexp.pddeact++;
			inactive_shortage--;
			mutex_exit(lock);
		}
	}
}

void
uvmpdpol_pagedeactivate(struct vm_page *pg)
{

	KASSERT(uvm_page_locked_p(pg));
	KASSERT(mutex_owned(&uvm_pageqlock));

	if (pg->pqflags & PQ_ACTIVE) {
		TAILQ_REMOVE(&pdpol_state.s_activeq, pg, pageq.queue);
		pg->pqflags &= ~PQ_ACTIVE;
		KASSERT(pdpol_state.s_active > 0);
		pdpol_state.s_active--;
	}
	if ((pg->pqflags & PQ_INACTIVE) == 0) {
		KASSERT(pg->wire_count == 0);
		pmap_clear_reference(pg);
		TAILQ_INSERT_TAIL(&pdpol_state.s_inactiveq, pg, pageq.queue);
		pg->pqflags |= PQ_INACTIVE;
		pdpol_state.s_inactive++;
	}
}

void
uvmpdpol_pageactivate(struct vm_page *pg)
{

	uvmpdpol_pagedequeue(pg);
	TAILQ_INSERT_TAIL(&pdpol_state.s_activeq, pg, pageq.queue);
	pg->pqflags |= PQ_ACTIVE;
	pdpol_state.s_active++;
}

void
uvmpdpol_pagedequeue(struct vm_page *pg)
{

	if (pg->pqflags & PQ_ACTIVE) {
		KASSERT(mutex_owned(&uvm_pageqlock));
		TAILQ_REMOVE(&pdpol_state.s_activeq, pg, pageq.queue);
		pg->pqflags &= ~PQ_ACTIVE;
		KASSERT(pdpol_state.s_active > 0);
		pdpol_state.s_active--;
	} else if (pg->pqflags & PQ_INACTIVE) {
		KASSERT(mutex_owned(&uvm_pageqlock));
		TAILQ_REMOVE(&pdpol_state.s_inactiveq, pg, pageq.queue);
		pg->pqflags &= ~PQ_INACTIVE;
		KASSERT(pdpol_state.s_inactive > 0);
		pdpol_state.s_inactive--;
	}
}

void
uvmpdpol_pageenqueue(struct vm_page *pg)
{

	uvmpdpol_pageactivate(pg);
}

void
uvmpdpol_anfree(struct vm_anon *an)
{
}

bool
uvmpdpol_pageisqueued_p(struct vm_page *pg)
{

	return (pg->pqflags & (PQ_ACTIVE | PQ_INACTIVE)) != 0;
}

void
uvmpdpol_estimatepageable(int *active, int *inactive)
{

	if (active) {
		*active = pdpol_state.s_active;
	}
	if (inactive) {
		*inactive = pdpol_state.s_inactive;
	}
}

#if !defined(PDSIM)
static int
min_check(struct uvm_pctparam *pct, int t)
{
	struct uvmpdpol_globalstate *s = &pdpol_state;
	int total = t;

	if (pct != &s->s_anonmin) {
		total += uvm_pctparam_get(&s->s_anonmin);
	}
	if (pct != &s->s_filemin) {
		total += uvm_pctparam_get(&s->s_filemin);
	}
	if (pct != &s->s_execmin) {
		total += uvm_pctparam_get(&s->s_execmin);
	}
	if (total > 95) {
		return EINVAL;
	}
	return 0;
}
#endif /* !defined(PDSIM) */

void
uvmpdpol_init(void)
{
	struct uvmpdpol_globalstate *s = &pdpol_state;

	TAILQ_INIT(&s->s_activeq);
	TAILQ_INIT(&s->s_inactiveq);
	uvm_pctparam_init(&s->s_inactivepct, CLOCK_INACTIVEPCT, NULL);
	uvm_pctparam_init(&s->s_anonmin, 10, min_check);
	uvm_pctparam_init(&s->s_filemin, 10, min_check);
	uvm_pctparam_init(&s->s_execmin,  5, min_check);
	uvm_pctparam_init(&s->s_anonmax, 80, NULL);
	uvm_pctparam_init(&s->s_filemax, 50, NULL);
	uvm_pctparam_init(&s->s_execmax, 30, NULL);
}

void
uvmpdpol_reinit(void)
{
}

bool
uvmpdpol_needsscan_p(void)
{

	return pdpol_state.s_inactive < pdpol_state.s_inactarg;
}

void
uvmpdpol_tune(void)
{

	clock_tune();
}

#if !defined(PDSIM)

#include <sys/sysctl.h>	/* XXX SYSCTL_DESCR */

void
uvmpdpol_sysctlsetup(void)
{
	struct uvmpdpol_globalstate *s = &pdpol_state;

	uvm_pctparam_createsysctlnode(&s->s_anonmin, "anonmin",
	    SYSCTL_DESCR("Percentage of physical memory reserved "
	    "for anonymous application data"));
	uvm_pctparam_createsysctlnode(&s->s_filemin, "filemin",
	    SYSCTL_DESCR("Percentage of physical memory reserved "
	    "for cached file data"));
	uvm_pctparam_createsysctlnode(&s->s_execmin, "execmin",
	    SYSCTL_DESCR("Percentage of physical memory reserved "
	    "for cached executable data"));

	uvm_pctparam_createsysctlnode(&s->s_anonmax, "anonmax",
	    SYSCTL_DESCR("Percentage of physical memory which will "
	    "be reclaimed from other usage for "
	    "anonymous application data"));
	uvm_pctparam_createsysctlnode(&s->s_filemax, "filemax",
	    SYSCTL_DESCR("Percentage of physical memory which will "
	    "be reclaimed from other usage for cached "
	    "file data"));
	uvm_pctparam_createsysctlnode(&s->s_execmax, "execmax",
	    SYSCTL_DESCR("Percentage of physical memory which will "
	    "be reclaimed from other usage for cached "
	    "executable data"));

	uvm_pctparam_createsysctlnode(&s->s_inactivepct, "inactivepct",
	    SYSCTL_DESCR("Percentage of inactive queue of "
	    "the entire (active + inactive) queue"));
}

#endif /* !defined(PDSIM) */

#if defined(PDSIM)
void
pdsim_dump(const char *id)
{
#if defined(DEBUG)
	/* XXX */
#endif /* defined(DEBUG) */
}
#endif /* defined(PDSIM) */
