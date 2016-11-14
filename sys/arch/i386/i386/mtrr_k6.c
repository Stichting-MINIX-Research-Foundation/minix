/*	$NetBSD: mtrr_k6.c,v 1.13 2008/04/04 22:07:22 cegger Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
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

/*
 * AMD K6 MTRR support.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mtrr_k6.c,v 1.13 2008/04/04 22:07:22 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h> 
#include <sys/malloc.h>

#include <machine/specialreg.h>
#include <machine/cpufunc.h>
#include <machine/mtrr.h>

static void	k6_mtrr_init_cpu(struct cpu_info *);
static void	k6_mtrr_reload_cpu(struct cpu_info *);
static void	k6_mtrr_clean(struct proc *);
static int	k6_mtrr_set(struct mtrr *, int *, struct proc *, int);
static int	k6_mtrr_get(struct mtrr *, int *, struct proc *, int);
static void	k6_mtrr_commit(void);
static void	k6_mtrr_dump(const char *);

static int	k6_mtrr_validate(struct mtrr *, struct proc *);
static void	k6_raw2soft(void);
static void	k6_soft2raw(void);

static struct mtrr_state
mtrr_var_raw[] = {
	{ 0, 0 },
	{ 1, 0 },
};

static struct mtrr *mtrr_var;

struct mtrr_funcs k6_mtrr_funcs = {
	k6_mtrr_init_cpu,
	k6_mtrr_reload_cpu,
	k6_mtrr_clean,
	k6_mtrr_set,
	k6_mtrr_get,
	k6_mtrr_commit,
	k6_mtrr_dump
};

static void
k6_mtrr_dump(const char *tag)
{
	uint64_t uwccr;
	int i;

	uwccr = rdmsr(MSR_K6_UWCCR);

	for (i = 0; i < MTRR_K6_NVAR; i++)
		printf("%s: %x: 0x%08llx\n", tag, mtrr_var_raw[i].msraddr,
		    (uwccr >> (32 * mtrr_var_raw[i].msraddr)) & 0xffffffff);
}

/*
 * There are no multiprocessor K6 systems, so we don't have to deal with
 * any multiprocessor stuff here.
 */
static void
k6_mtrr_reload(void)
{
	uint64_t uwccr;
	uint32_t origcr0, cr0;
	int i;

	x86_disable_intr();

	origcr0 = cr0 = rcr0();
	cr0 |= CR0_CD;
	lcr0(cr0);

	wbinvd();

	for (i = 0, uwccr = 0; i < MTRR_K6_NVAR; i++) {
		uwccr |= mtrr_var_raw[i].msrval <<
		    (32 * mtrr_var_raw[i].msraddr);
	}

	wrmsr(MSR_K6_UWCCR, uwccr);

	lcr0(origcr0);

	x86_enable_intr();
}

static void
k6_mtrr_reload_cpu(struct cpu_info *ci)
{

	k6_mtrr_reload();
}

void
k6_mtrr_init_first(void)
{
	uint64_t uwccr;
	int i;

	uwccr = rdmsr(MSR_K6_UWCCR);

	for (i = 0; i < MTRR_K6_NVAR; i++) {
		mtrr_var_raw[i].msrval =
		    (uwccr >> (32 * mtrr_var_raw[i].msraddr)) & 0xffffffff;
	}
#if 0
	mtrr_dump("init mtrr");
#endif

	mtrr_var = (struct mtrr *)
	    malloc(MTRR_K6_NVAR * sizeof(struct mtrr), M_TEMP, M_NOWAIT);
	if (mtrr_var == NULL)
		panic("can't allocate variable MTRR array");
	mtrr_funcs = &k6_mtrr_funcs;

	k6_raw2soft();
}

static void
k6_raw2soft(void)
{
	struct mtrr *mtrrp;
	uint32_t base, mask;
	int i;

	for (i = 0; i < MTRR_K6_NVAR; i++) {
		mtrrp = &mtrr_var[i];
		memset(mtrrp, 0, sizeof(*mtrrp));
		base = mtrr_var_raw[i].msrval & MTRR_K6_ADDR;
		mask = (mtrr_var_raw[i].msrval & MTRR_K6_MASK) >>
		    MTRR_K6_MASK_SHIFT;
		if (mask == 0)
			continue;
		mtrrp->base = base;
		mtrrp->len = ffs(mask) << MTRR_K6_ADDR_SHIFT;
		/* XXXJRT can both UC and WC be set? */
		if (mtrr_var_raw[i].msrval & MTRR_K6_UC)
			mtrrp->type = MTRR_TYPE_UC;
		else if (mtrr_var_raw[i].msrval & MTRR_K6_WC)
			mtrrp->type = MTRR_TYPE_WC;
		else	/* XXXJRT Correct default? */
			mtrrp->type = MTRR_TYPE_WT;
		mtrrp->flags |= MTRR_VALID;
	}
}

static void
k6_soft2raw(void)
{
	struct mtrr *mtrrp;
	uint32_t mask;
	int i, bit;

	for (i = 0; i < MTRR_K6_NVAR; i++) {
		mtrrp = &mtrr_var[i];
		if ((mtrrp->flags & MTRR_VALID) == 0) {
			mtrr_var_raw[i].msrval = 0;
			continue;
		}
		mtrr_var_raw[i].msrval = mtrrp->base;
		for (bit = ffs(mtrrp->len >> MTRR_K6_ADDR_SHIFT) - 1, mask = 0;
		     bit < 15; bit++)
			mask |= 1U << bit;
		mtrr_var_raw[i].msrval |= mask << MTRR_K6_MASK_SHIFT;
		if (mtrrp->type == MTRR_TYPE_UC)
			mtrr_var_raw[i].msrval |= MTRR_K6_UC;
		else if (mtrrp->type == MTRR_TYPE_WC)
			mtrr_var_raw[i].msrval |= MTRR_K6_WC;
	}
}

static void
k6_mtrr_init_cpu(struct cpu_info *ci)
{

	k6_mtrr_reload();
#if 0
	mtrr_dump(device_xname(ci->ci_dev));
#endif
}

static int
k6_mtrr_validate(struct mtrr *mtrrp, struct proc *p)
{

	/*
	 * Must be at least 128K aligned.
	 */
	if (mtrrp->base & ~MTRR_K6_ADDR)
		return (EINVAL);

	/*
	 * Must be at least 128K long, and must be a power of 2.
	 */
	if (mtrrp->len < (128 * 1024) || powerof2(mtrrp->len) == 0)
		return (EINVAL);

	/*
	 * Filter out bad types.
	 */
	switch (mtrrp->type) {
	case MTRR_TYPE_UC:
	case MTRR_TYPE_WC:
	case MTRR_TYPE_WT:
		/* These are fine. */
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

/*
 * Try to find a non-conflicting match on physical MTRRs for the
 * requested range.
 */
static int
k6_mtrr_setone(struct mtrr *mtrrp, struct proc *p)
{
	struct mtrr *freep;
	uint32_t low, high, curlow, curhigh;
	int i;

	/*
	 * Try one of the variable range registers.
	 * XXX could be more sophisticated here by merging ranges.
	 */
	low = mtrrp->base;
	high = low + mtrrp->len;
	freep = NULL;
	for (i = 0; i < MTRR_K6_NVAR; i++) {
		if ((mtrr_var[i].flags & MTRR_VALID) == 0) {
			freep = &mtrr_var[i];
			continue;
		}
		curlow = mtrr_var[i].base;
		curhigh = curlow + mtrr_var[i].len;
		if (low == curlow && high == curhigh &&
		    (!(mtrr_var[i].flags & MTRR_PRIVATE) ||
		     mtrr_var[i].owner == p->p_pid)) {
			freep = &mtrr_var[i];
			break;
		}
		if (((high >= curlow && high < curhigh) ||
		    (low >= curlow && low < curhigh)) &&
		    ((mtrr_var[i].type != mtrrp->type) ||
		     ((mtrr_var[i].flags & MTRR_PRIVATE) &&
		      mtrr_var[i].owner != p->p_pid))) {
			return (EBUSY);
		}
	}
	if (freep == NULL)
		return (EBUSY);
	mtrrp->flags &= ~MTRR_CANTSET;
	*freep = *mtrrp;
	freep->owner = mtrrp->flags & MTRR_PRIVATE ? p->p_pid : 0;

	return (0);
}

static void
k6_mtrr_clean(struct proc *p)
{
	int i;

	for (i = 0; i < MTRR_K6_NVAR; i++) {
		if ((mtrr_var[i].flags & MTRR_PRIVATE) &&
		    (mtrr_var[i].owner == p->p_pid))
			mtrr_var[i].flags &= ~(MTRR_PRIVATE | MTRR_VALID);
	}

	k6_mtrr_commit();
}

static int
k6_mtrr_set(struct mtrr *mtrrp, int *n, struct proc *p, int flags)
{
	struct mtrr mtrr;
	int i, error;

	if (*n > MTRR_K6_NVAR) {
		*n = 0;
		return EINVAL;
	}

	error = 0;
	for (i = 0; i < *n; i++) {
		if (flags & MTRR_GETSET_USER) {
			error = copyin(&mtrrp[i], &mtrr, sizeof(mtrr));
			if (error != 0)
				break;
		} else
			mtrr = mtrrp[i];
		error = k6_mtrr_validate(&mtrr, p);
		if (error != 0)
			break;
		error = k6_mtrr_setone(&mtrr, p);
		if (error != 0)
			break;
		if (mtrr.flags & MTRR_PRIVATE)
			p->p_md.md_flags |= MDP_USEDMTRR;
	}
	*n = i;
	return (error);
}

static int
k6_mtrr_get(struct mtrr *mtrrp, int *n, struct proc *p, int flags)
{
	int i, error;

	if (mtrrp == NULL) {
		*n = MTRR_K6_NVAR;
		return (0);
	}

	error = 0;

	for (i = 0; i < MTRR_K6_NVAR && i < *n; i++) {
		if (flags & MTRR_GETSET_USER) {
			error = copyout(&mtrr_var[i], &mtrrp[i],
			    sizeof(*mtrrp));
			if (error != 0)
				break;
		} else
			memcpy(&mtrrp[i], &mtrr_var[i], sizeof(*mtrrp));
	}
	*n = i;
	return (error);
}

static void
k6_mtrr_commit(void)
{

	k6_soft2raw();
	k6_mtrr_reload();
}
