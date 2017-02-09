/*	$NetBSD: pmc.c,v 1.21 2013/11/21 22:04:40 riz Exp $	*/

/*-
 * Copyright (c) 2000 Zembu Labs, Inc.
 * All rights reserved.
 *
 * Author: Jason R. Thorpe <thorpej@zembu.com>
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
 *	This product includes software developed by Zembu Labs, Inc.
 * 4. Neither the name of Zembu Labs nor the names of its employees may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ZEMBU LABS, INC. ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAR-
 * RANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DIS-
 * CLAIMED.  IN NO EVENT SHALL ZEMBU LABS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Interface to x86 CPU Performance Counters.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pmc.c,v 1.21 2013/11/21 22:04:40 riz Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/pmc.h>
#include <machine/cpu_counter.h>
#include <machine/cputypes.h>

static int pmc_initialized;
static int pmc_ncounters;
static int pmc_type;
static int pmc_flags;

static struct pmc_state {
	uint64_t pmcs_val;
	uint64_t pmcs_tsc;
	uint64_t pmcs_control;
	uint32_t pmcs_ctrmsr;
} pmc_state[PMC_NCOUNTERS];

static int pmc_running;

static void
pmc_init(void)
{
	const char *cpu_vendorstr;
	struct cpu_info *ci;

	if (pmc_initialized)
		return;

	pmc_type = PMC_TYPE_NONE;

#ifdef MULTIPROCESSOR
	/* XXX */
	if (ncpu > 1)
		goto done;
#endif

	ci = curcpu();
	cpu_vendorstr = (char *)ci->ci_vendor;

	switch (cpu_class) {
	case CPUCLASS_586:
		if (strncmp(cpu_vendorstr, "GenuineIntel", 12) == 0) {
			pmc_type = PMC_TYPE_I586;
			pmc_ncounters = 2;
			pmc_state[0].pmcs_ctrmsr = MSR_CTR0;
			pmc_state[1].pmcs_ctrmsr = MSR_CTR1;
			break;
		}

	case CPUCLASS_686:
		if (strncmp(cpu_vendorstr, "GenuineIntel", 12) == 0) {

			/*
			 * Figure out what we support; right now
			 * we're missing Pentium 4 support.
			 */
			if (cpuid_level == -1 ||
			    CPUID_TO_FAMILY(ci->ci_signature) == CPU_FAMILY_P4)
				break;

			pmc_type = PMC_TYPE_I686;
			pmc_ncounters = 2;
			pmc_state[0].pmcs_ctrmsr = MSR_PERFCTR0;
			pmc_state[1].pmcs_ctrmsr = MSR_PERFCTR1;
		} else if (strncmp(cpu_vendorstr, "AuthenticAMD",
			   12) == 0) {
			pmc_type = PMC_TYPE_K7;
			pmc_ncounters = 4;
			pmc_state[0].pmcs_ctrmsr = MSR_K7_PERFCTR0;
			pmc_state[1].pmcs_ctrmsr = MSR_K7_PERFCTR1;
			pmc_state[2].pmcs_ctrmsr = MSR_K7_PERFCTR2;
			pmc_state[3].pmcs_ctrmsr = MSR_K7_PERFCTR3;
		}
		break;
	}

	if (pmc_type != PMC_TYPE_NONE && cpu_hascounter())
		pmc_flags |= PMC_INFO_HASTSC;

#ifdef MULTIPROCESSOR
done:
#endif
	pmc_initialized = 1;
}

int
pmc_info(struct lwp *l, struct x86_pmc_info_args *uargs,
    register_t *retval)
{
	struct x86_pmc_info_args rv;

	memset(&rv, 0, sizeof(rv));

	if (pmc_initialized == 0)
		pmc_init();

	rv.type = pmc_type;
	rv.flags = pmc_flags;

	return (copyout(&rv, uargs, sizeof(rv)));
}

int
pmc_startstop(struct lwp *l, struct x86_pmc_startstop_args *uargs,
    register_t *retval)
{
	struct x86_pmc_startstop_args args;
	int error, mask, start;

	if (pmc_initialized == 0)
		pmc_init();
	if (pmc_type == PMC_TYPE_NONE)
		return (ENODEV);

	error = copyin(uargs, &args, sizeof(args));
	if (error)
		return (error);

	if (args.counter < 0 || args.counter >= pmc_ncounters)
		return (EINVAL);

	mask = 1 << args.counter;
	start = (args.flags & (PMC_SETUP_KERNEL|PMC_SETUP_USER)) != 0;

	if ((pmc_running & mask) != 0 && start != 0)
		return (EBUSY);
	else if ((pmc_running & mask) == 0 && start == 0)
		return (0);

	x86_disable_intr();
	if (start) {
		pmc_running |= mask;
		pmc_state[args.counter].pmcs_val = args.val;
		switch (pmc_type) {
		case PMC_TYPE_I586:
			pmc_state[args.counter].pmcs_control = args.event |
			    ((args.flags & PMC_SETUP_KERNEL) ?
			      PMC5_CESR_OS : 0) |
			    ((args.flags & PMC_SETUP_USER) ?
			      PMC5_CESR_USR : 0) |
			    ((args.flags & PMC_SETUP_EDGE) ?
			      PMC5_CESR_E : 0);
			break;

		case PMC_TYPE_I686:
			pmc_state[args.counter].pmcs_control = args.event |
			    (args.unit << PMC6_EVTSEL_UNIT_SHIFT) |
			    ((args.flags & PMC_SETUP_KERNEL) ?
			      PMC6_EVTSEL_OS : 0) |
			    ((args.flags & PMC_SETUP_USER) ?
			      PMC6_EVTSEL_USR : 0) |
			    ((args.flags & PMC_SETUP_EDGE) ?
			      PMC6_EVTSEL_E : 0) |
			    ((args.flags & PMC_SETUP_INV) ?
			      PMC6_EVTSEL_INV : 0) |
			    (args.compare << PMC6_EVTSEL_COUNTER_MASK_SHIFT);
			break;

		case PMC_TYPE_K7:
			pmc_state[args.counter].pmcs_control = args.event |
			    (args.unit << K7_EVTSEL_UNIT_SHIFT) |
			    ((args.flags & PMC_SETUP_KERNEL) ?
			      K7_EVTSEL_OS : 0) |
			    ((args.flags & PMC_SETUP_USER) ?
			      K7_EVTSEL_USR : 0) |
			    ((args.flags & PMC_SETUP_EDGE) ?
			      K7_EVTSEL_E : 0) |
			    ((args.flags & PMC_SETUP_INV) ?
			      K7_EVTSEL_INV : 0) |
			    (args.compare << PMC6_EVTSEL_COUNTER_MASK_SHIFT);
			break;
		}
		wrmsr(pmc_state[args.counter].pmcs_ctrmsr,
		    pmc_state[args.counter].pmcs_val);
	} else {
		pmc_running &= ~mask;
		pmc_state[args.counter].pmcs_control = 0;
	}

	switch (pmc_type) {
	case PMC_TYPE_I586:
		wrmsr(MSR_CESR, pmc_state[0].pmcs_control |
		    (pmc_state[1].pmcs_control << 16));
		break;

	case PMC_TYPE_I686:
		if (args.counter == 1)
			wrmsr(MSR_EVNTSEL1, pmc_state[1].pmcs_control);
		wrmsr(MSR_EVNTSEL0, pmc_state[0].pmcs_control |
		    (pmc_running ? PMC6_EVTSEL_EN : 0));
		break;

	case PMC_TYPE_K7:
		if (args.counter == 1)
			wrmsr(MSR_K7_EVNTSEL1, pmc_state[1].pmcs_control);
		wrmsr(MSR_K7_EVNTSEL0, pmc_state[0].pmcs_control |
		    (pmc_running ? K7_EVTSEL_EN : 0));
		break;
	}
	x86_enable_intr();

	return (0);
}

int
pmc_read(struct lwp *l, struct x86_pmc_read_args *uargs,
    register_t *retval)
{
	struct x86_pmc_read_args args;
	int error;

	if (pmc_initialized == 0)
		pmc_init();
	if (pmc_type == PMC_TYPE_NONE)
		return (ENODEV);

	error = copyin(uargs, &args, sizeof(args));
	if (error)
		return (error);

	if (args.counter < 0 || args.counter >= pmc_ncounters)
		return (EINVAL);

	if (pmc_running & (1 << args.counter)) {
		pmc_state[args.counter].pmcs_val =
		    rdmsr(pmc_state[args.counter].pmcs_ctrmsr) &
		    0xffffffffffULL;
		if (pmc_flags & PMC_INFO_HASTSC)
			pmc_state[args.counter].pmcs_tsc = cpu_counter();
	}

	args.val = pmc_state[args.counter].pmcs_val;
	args.time = pmc_state[args.counter].pmcs_tsc;

	return (copyout(&args, uargs, sizeof(args)));
}
