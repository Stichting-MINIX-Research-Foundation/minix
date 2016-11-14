/*	$NetBSD: longrun.c,v 1.4 2011/10/23 13:02:32 jmcneill Exp $	*/

/*-
 * Copyright (c) 2001 Tamotsu Hattori.
 * Copyright (c) 2001 Mitsuru IWASAKI.
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
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: longrun.c,v 1.4 2011/10/23 13:02:32 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <machine/specialreg.h>
#include <machine/cpu.h>

/*
 * Transmeta Crusoe LongRun Support by Tamotsu Hattori.
 * Port from FreeBSD-current(August, 2001) to NetBSD by tshiozak.
 */

#define	MSR_TMx86_LONGRUN		0x80868010
#define	MSR_TMx86_LONGRUN_FLAGS		0x80868011

#define	LONGRUN_MODE_MASK(x)		((x) & 0x0000007f)
#define	LONGRUN_MODE_RESERVED(x)	((x) & 0xffffff80)
#define	LONGRUN_MODE_WRITE(x, y)	(LONGRUN_MODE_RESERVED(x) | \
					    LONGRUN_MODE_MASK(y))

#define	LONGRUN_MODE_MINFREQUENCY	0x00
#define	LONGRUN_MODE_ECONOMY		0x01
#define	LONGRUN_MODE_PERFORMANCE	0x02
#define	LONGRUN_MODE_MAXFREQUENCY	0x03
#define	LONGRUN_MODE_UNKNOWN		0x04
#define	LONGRUN_MODE_MAX		0x04

union msrinfo {
	uint64_t	msr;
	uint32_t	regs[2];
};

static u_int crusoe_longrun = 0;
static u_int crusoe_frequency = 0;
static u_int crusoe_voltage = 0;
static u_int crusoe_percentage = 0;

static const uint32_t longrun_modes[LONGRUN_MODE_MAX][3] = {
	/*  MSR low, MSR high, flags bit0 */
	{	  0,	  0,		0},	/* LONGRUN_MODE_MINFREQUENCY */
	{	  0,	100,		0},	/* LONGRUN_MODE_ECONOMY */
	{	  0,	100,		1},	/* LONGRUN_MODE_PERFORMANCE */
	{	100,	100,		1},	/* LONGRUN_MODE_MAXFREQUENCY */
};

static u_int tmx86_set_longrun_mode(u_int);
static void tmx86_get_longrun_status_all(void);
static int sysctl_machdep_tm_longrun(SYSCTLFN_PROTO);

void
tmx86_init_longrun(void)
{
	const struct sysctlnode *mnode;
	uint64_t msr;

	/* PR #32894, make sure the longrun MSR is present */
	if (rdmsr_safe(MSR_TMx86_LONGRUN, &msr) == EFAULT)
		return;

	/* create the sysctl machdep.tm_longrun_* nodes */
        sysctl_createv(NULL, 0, NULL, &mnode, CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "machdep", NULL,
		       NULL, 0, NULL, 0,
		       CTL_MACHDEP, CTL_EOL);

        sysctl_createv(NULL, 0, NULL, NULL, CTLFLAG_READWRITE,
		       CTLTYPE_INT, "tm_longrun_mode", NULL,
		       sysctl_machdep_tm_longrun, 0, NULL, 0,
		       CTL_MACHDEP, CPU_TMLR_MODE, CTL_EOL);

	sysctl_createv(NULL, 0, NULL, NULL, 0,
		       CTLTYPE_INT, "tm_longrun_frequency", NULL,
		       sysctl_machdep_tm_longrun, 0, NULL, 0,
		       CTL_MACHDEP, CPU_TMLR_FREQUENCY, CTL_EOL);

	sysctl_createv(NULL, 0, NULL, NULL, 0,
		       CTLTYPE_INT, "tm_longrun_voltage", NULL,
		       sysctl_machdep_tm_longrun, 0, NULL, 0,
		       CTL_MACHDEP, CPU_TMLR_VOLTAGE, CTL_EOL);

	sysctl_createv(NULL, 0, NULL, NULL, 0,
		       CTLTYPE_INT, "tm_longrun_percentage", NULL,
		       sysctl_machdep_tm_longrun, 0, NULL, 0,
		       CTL_MACHDEP, CPU_TMLR_PERCENTAGE, CTL_EOL);
}

u_int
tmx86_get_longrun_mode(void)
{
	u_long		eflags;
	union msrinfo	msrinfo;
	u_int		low, high, flags, mode;

	eflags = x86_read_psl();
	x86_disable_intr();

	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN);
	low = LONGRUN_MODE_MASK(msrinfo.regs[0]);
	high = LONGRUN_MODE_MASK(msrinfo.regs[1]);
	flags = rdmsr(MSR_TMx86_LONGRUN_FLAGS) & 0x01;

	for (mode = 0; mode < LONGRUN_MODE_MAX; mode++) {
		if (low   == longrun_modes[mode][0] &&
		    high  == longrun_modes[mode][1] &&
		    flags == longrun_modes[mode][2]) {
			goto out;
		}
	}
	mode = LONGRUN_MODE_UNKNOWN;
out:
	x86_write_psl(eflags);
	return mode;
}

void
tmx86_get_longrun_status(u_int *frequency, u_int *voltage, u_int *percentage)
{
	u_long eflags;
	u_int descs[4];

	eflags = x86_read_psl();
	x86_disable_intr();

	x86_cpuid(0x80860007, descs);
	*frequency = descs[0];
	*voltage = descs[1];
	*percentage = descs[2];

	x86_write_psl(eflags);
}

static u_int
tmx86_set_longrun_mode(u_int mode)
{
	u_long		eflags;
	union msrinfo	msrinfo;

	if (mode >= LONGRUN_MODE_UNKNOWN)
		return 0;

	eflags = x86_read_psl();
	x86_disable_intr();

	/* Write LongRun mode values to Model Specific Register. */
	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN);
	msrinfo.regs[0] = LONGRUN_MODE_WRITE(msrinfo.regs[0],
	    longrun_modes[mode][0]);
	msrinfo.regs[1] = LONGRUN_MODE_WRITE(msrinfo.regs[1],
	    longrun_modes[mode][1]);
	wrmsr(MSR_TMx86_LONGRUN, msrinfo.msr);

	/* Write LongRun mode flags to Model Specific Register. */
	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN_FLAGS);
	msrinfo.regs[0] = (msrinfo.regs[0] & ~0x01) | longrun_modes[mode][2];
	wrmsr(MSR_TMx86_LONGRUN_FLAGS, msrinfo.msr);

	x86_write_psl(eflags);
	return 1;
}

static void
tmx86_get_longrun_status_all(void)
{

	tmx86_get_longrun_status(&crusoe_frequency,
	    &crusoe_voltage, &crusoe_percentage);
}

/*
 * sysctl helper routine for machdep.tm* nodes.
 */
static int
sysctl_machdep_tm_longrun(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int io, error;

	node = *rnode;
	node.sysctl_data = &io;

	switch (rnode->sysctl_num) {
	case CPU_TMLR_MODE:
		io = (int)(crusoe_longrun = tmx86_get_longrun_mode());
		break;
	case CPU_TMLR_FREQUENCY:
		tmx86_get_longrun_status_all();
		io = crusoe_frequency;
		break;
	case CPU_TMLR_VOLTAGE:
		tmx86_get_longrun_status_all();
		io = crusoe_voltage;
		break;
	case CPU_TMLR_PERCENTAGE:
		tmx86_get_longrun_status_all();
		io = crusoe_percentage;
		break;
	default:
		return EOPNOTSUPP;
	}

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (rnode->sysctl_num == CPU_TMLR_MODE) {
		if (tmx86_set_longrun_mode(io))
			crusoe_longrun = (u_int)io;
		else
			return EINVAL;
	}

	return 0;
}
