/*	$NetBSD: clock_pcctwo.c,v 1.17 2012/10/27 17:18:27 chs Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 * Glue for the Peripheral Channel Controller Two (PCCChip2) timers,
 * the Memory Controller ASIC (MCchip, and the Mostek clock chip found
 * on the MVME-1[67]7, MVME-1[67]2 and MVME-187 series of boards.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clock_pcctwo.c,v 1.17 2012/10/27 17:18:27 chs Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timetc.h>

#include <machine/psl.h>
#include <sys/bus.h>

#include <dev/mvme/clockvar.h>
#include <dev/mvme/pcctwovar.h>
#include <dev/mvme/pcctworeg.h>


int clock_pcctwo_match(device_t, cfdata_t, void *);
void clock_pcctwo_attach(device_t, device_t, void *);

struct clock_pcctwo_softc {
	struct clock_attach_args sc_clock_args;
	u_char sc_clock_lvl;
	struct timecounter sc_tc;
};

CFATTACH_DECL_NEW(clock_pcctwo, sizeof(struct clock_pcctwo_softc),
    clock_pcctwo_match, clock_pcctwo_attach, NULL, NULL);

extern struct cfdriver clock_cd;

static int clock_pcctwo_profintr(void *);
static int clock_pcctwo_statintr(void *);
static void clock_pcctwo_initclocks(void *, int, int);
static u_int clock_pcctwo_getcount(struct timecounter *);
static void clock_pcctwo_shutdown(void *);

static struct clock_pcctwo_softc *clock_pcctwo_sc;
static uint32_t clock_pcctwo_count;

/* ARGSUSED */
int
clock_pcctwo_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pcctwo_attach_args *pa = aux;

	/* Only one clock, please. */
	if (clock_pcctwo_sc)
		return (0);

	if (strcmp(pa->pa_name, clock_cd.cd_name))
		return (0);

	pa->pa_ipl = cf->pcctwocf_ipl;

	return (1);
}

/* ARGSUSED */
void
clock_pcctwo_attach(device_t parent, device_t self, void *aux)
{
	struct clock_pcctwo_softc *sc;
	struct pcctwo_attach_args *pa;

	sc = clock_pcctwo_sc = device_private(self);
	pa = aux;

	if (pa->pa_ipl != CLOCK_LEVEL)
		panic("clock_pcctwo_attach: wrong interrupt level");

	sc->sc_clock_args.ca_arg = sc;
	sc->sc_clock_args.ca_initfunc = clock_pcctwo_initclocks;

	/* Do common portions of clock config. */
	clock_config(self, &sc->sc_clock_args, pcctwointr_evcnt(pa->pa_ipl));

	/* Ensure our interrupts get disabled at shutdown time. */
	(void) shutdownhook_establish(clock_pcctwo_shutdown, NULL);

	sc->sc_clock_lvl = (pa->pa_ipl & PCCTWO_ICR_LEVEL_MASK) |
	    PCCTWO_ICR_ICLR | PCCTWO_ICR_IEN;

	/* Attach the interrupt handlers. */
	pcctwointr_establish(PCCTWOV_TIMER1, clock_pcctwo_profintr,
	    pa->pa_ipl, NULL, &clock_profcnt);
	pcctwointr_establish(PCCTWOV_TIMER2, clock_pcctwo_statintr,
	    pa->pa_ipl, NULL, &clock_statcnt);
}

void
clock_pcctwo_initclocks(void *arg, int prof_us, int stat_us)
{
	struct clock_pcctwo_softc *sc;

	sc = arg;

	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER1_CONTROL, PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write32(sys_pcctwo, PCC2REG_TIMER1_COUNTER, 0);
	pcc2_reg_write32(sys_pcctwo, PCC2REG_TIMER1_COMPARE,
	    PCCTWO_US2LIM(prof_us));
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER1_CONTROL,
	    PCCTWO_TT_CTRL_CEN | PCCTWO_TT_CTRL_COC | PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER1_ICSR, sc->sc_clock_lvl);

	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_CONTROL, PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write32(sys_pcctwo, PCC2REG_TIMER2_COUNTER, 0);
	pcc2_reg_write32(sys_pcctwo, PCC2REG_TIMER2_COMPARE,
	    PCCTWO_US2LIM(stat_us));
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_CONTROL,
	    PCCTWO_TT_CTRL_CEN | PCCTWO_TT_CTRL_COC | PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_ICSR, sc->sc_clock_lvl);

	sc->sc_tc.tc_get_timecount = clock_pcctwo_getcount;
	sc->sc_tc.tc_name = "pcctwo_count";
	sc->sc_tc.tc_frequency = PCCTWO_TIMERFREQ;
	sc->sc_tc.tc_quality = 100;
	sc->sc_tc.tc_counter_mask = ~0;
	tc_init(&sc->sc_tc);
}

/* ARGSUSED */
u_int
clock_pcctwo_getcount(struct timecounter *tc)
{
	u_int cnt;
	uint32_t tc1, tc2;
	uint8_t cr;
	int s;

	s = splhigh();

	/*
	 * There's no way to latch the counter and overflow registers
	 * without pausing the clock, so compensate for the possible
	 * race by checking for counter wrap-around and re-reading the
	 * overflow counter if necessary.
	 *
	 * Note: This only works because we're at splhigh().
	 */
	tc1 = pcc2_reg_read32(sys_pcctwo, PCC2REG_TIMER1_COUNTER);
	cr = pcc2_reg_read(sys_pcctwo, PCC2REG_TIMER1_CONTROL);
	tc2 = pcc2_reg_read32(sys_pcctwo, PCC2REG_TIMER1_COUNTER);
	if (tc1 > tc2) {
		cr = pcc2_reg_read(sys_pcctwo, PCC2REG_TIMER1_CONTROL);
		tc1 = tc2;
	}
	cnt = clock_pcctwo_count;
	splx(s);
	/* XXX assume HZ == 100 */
	cnt += tc1 + (PCCTWO_TIMERFREQ / 100) * PCCTWO_TT_CTRL_OVF(cr);

	return cnt;
}

int
clock_pcctwo_profintr(void *frame)
{
	u_int8_t cr;
	u_int32_t tc;
	int s;

	s = splhigh();
	tc = pcc2_reg_read32(sys_pcctwo, PCC2REG_TIMER1_COUNTER);
	cr = pcc2_reg_read(sys_pcctwo, PCC2REG_TIMER1_CONTROL);
	if (tc > pcc2_reg_read32(sys_pcctwo, PCC2REG_TIMER1_COUNTER))
		cr = pcc2_reg_read(sys_pcctwo, PCC2REG_TIMER1_CONTROL);
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER1_CONTROL,
	    PCCTWO_TT_CTRL_CEN | PCCTWO_TT_CTRL_COC | PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER1_ICSR,
	    clock_pcctwo_sc->sc_clock_lvl);
	splx(s);

	for (cr = PCCTWO_TT_CTRL_OVF(cr); cr; cr--) {
		/* XXX assume HZ == 100 */
		clock_pcctwo_count += PCCTWO_TIMERFREQ / 100;
		hardclock(frame);
	}

	return (1);
}

int
clock_pcctwo_statintr(void *frame)
{

	/* Disable the timer interrupt while we handle it. */
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_ICSR, 0);

	statclock((struct clockframe *) frame);

	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_CONTROL, PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write32(sys_pcctwo, PCC2REG_TIMER2_COUNTER, 0);
	pcc2_reg_write32(sys_pcctwo, PCC2REG_TIMER2_COMPARE,
	    PCCTWO_US2LIM(CLOCK_NEWINT(clock_statvar, clock_statmin)));
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_CONTROL,
	    PCCTWO_TT_CTRL_CEN | PCCTWO_TT_CTRL_COC | PCCTWO_TT_CTRL_COVF);

	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_ICSR,
	    clock_pcctwo_sc->sc_clock_lvl);

	return (1);
}

/* ARGSUSED */
void
clock_pcctwo_shutdown(void *arg)
{

	/* Make sure the timer interrupts are turned off. */
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER1_CONTROL, PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER1_ICSR, 0);
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_CONTROL, PCCTWO_TT_CTRL_COVF);
	pcc2_reg_write(sys_pcctwo, PCC2REG_TIMER2_ICSR, 0);
}
