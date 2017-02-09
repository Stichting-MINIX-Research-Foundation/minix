/*	$NetBSD: lpt_pcctwo.c,v 1.12 2009/03/14 15:36:19 dsl Exp $	*/

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
 * Device Driver back-end for the PCCChip2's parallel printer port
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lpt_pcctwo.c,v 1.12 2009/03/14 15:36:19 dsl Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/syslog.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/mvme/lptvar.h>
#include <dev/mvme/pcctworeg.h>
#include <dev/mvme/pcctwovar.h>

/*
 * Autoconfig stuff
 */
int lpt_pcctwo_match(device_t, cfdata_t , void *);
void lpt_pcctwo_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(lpt_pcctwo, sizeof(struct lpt_softc),
    lpt_pcctwo_match, lpt_pcctwo_attach, NULL, NULL);

extern struct cfdriver lpt_cd;


int lpt_pcctwo_intr(void *);
void lpt_pcctwo_open(struct lpt_softc *, int);
void lpt_pcctwo_close(struct lpt_softc *);
void lpt_pcctwo_iprime(struct lpt_softc *);
void lpt_pcctwo_speed(struct lpt_softc *, int);
int lpt_pcctwo_notrdy(struct lpt_softc *, int);
void lpt_pcctwo_wr_data(struct lpt_softc *, u_char);

struct lpt_funcs lpt_pcctwo_funcs = {
	lpt_pcctwo_open,
	lpt_pcctwo_close,
	lpt_pcctwo_iprime,
	lpt_pcctwo_speed,
	lpt_pcctwo_notrdy,
	lpt_pcctwo_wr_data
};

/* ARGSUSED */
int
lpt_pcctwo_match(device_t parent, cfdata_t cf, void *args)
{
	struct pcctwo_attach_args *pa;

	pa = args;

	if (strcmp(pa->pa_name, lpt_cd.cd_name))
		return (0);

#ifdef MVME68K
	if (machineid != MVME_167 && machineid != MVME_177)
		return (0);
#endif

#ifdef MVME88K
	if (machineid != MVME_187)
		return (0);
#endif

	pa->pa_ipl = cf->pcctwocf_ipl;

	return (1);
}

/* ARGSUSED */
void
lpt_pcctwo_attach(device_t parent, device_t self, void *args)
{
	struct pcctwo_attach_args *pa;
	struct lpt_softc *sc;

	pa = (struct pcctwo_attach_args *) args;
	sc = device_private(self);
	sc->sc_dev = self;

	/* The printer registers are part of the PCCChip2's own registers. */
	sc->sc_bust = pa->pa_bust;
	bus_space_map(pa->pa_bust, pa->pa_offset, PCC2REG_SIZE, 0,
	    &sc->sc_bush);

	sc->sc_ipl = pa->pa_ipl & PCCTWO_ICR_LEVEL_MASK;
	sc->sc_laststatus = 0;
	sc->sc_funcs = &lpt_pcctwo_funcs;

	aprint_normal(": PCCchip2 Parallel Printer\n");

	/*
	 * Disable interrupts until device is opened
	 */
	pcc2_reg_write(sc, PCC2REG_PRT_ACK_ICSR, 0);
	pcc2_reg_write(sc, PCC2REG_PRT_FAULT_ICSR, 0);
	pcc2_reg_write(sc, PCC2REG_PRT_SEL_ICSR, 0);
	pcc2_reg_write(sc, PCC2REG_PRT_PE_ICSR, 0);
	pcc2_reg_write(sc, PCC2REG_PRT_BUSY_ICSR, 0);
	pcc2_reg_write(sc, PCC2REG_PRT_CONTROL, 0);

	/*
	 * Main attachment code
	 */
	lpt_attach_subr(sc);

	/* Register the event counter */
	evcnt_attach_dynamic(&sc->sc_evcnt, EVCNT_TYPE_INTR,
	    pcctwointr_evcnt(sc->sc_ipl), "printer", device_xname(sc->sc_dev));

	/*
	 * Hook into the printer interrupt
	 */
	pcctwointr_establish(PCCTWOV_PRT_ACK, lpt_pcctwo_intr, sc->sc_ipl, sc,
	    &sc->sc_evcnt);
}

/*
 * Handle printer interrupts
 */
int
lpt_pcctwo_intr(void *arg)
{
	struct lpt_softc *sc;
	int i;

	sc = (struct lpt_softc *) arg;

	/* is printer online and ready for output */
	if (lpt_pcctwo_notrdy(sc, 0) || lpt_pcctwo_notrdy(sc, 1))
		return (0);

	i = lpt_intr(sc);

	if (pcc2_reg_read(sc, PCC2REG_PRT_INPUT_STATUS) & PCCTWO_PRT_IN_SR_PINT)
		pcc2_reg_write(sc, PCC2REG_PRT_ACK_ICSR,
		    sc->sc_icr | PCCTWO_ICR_ICLR);

	return (i);
}

void
lpt_pcctwo_open(struct lpt_softc *sc, int int_ena)
{
	int sps;

	pcc2_reg_write(sc, PCC2REG_PRT_ACK_ICSR,
	    PCCTWO_ICR_ICLR | PCCTWO_ICR_EDGE);

	pcc2_reg_write(sc, PCC2REG_PRT_CONTROL,
	    pcc2_reg_read(sc, PCC2REG_PRT_CONTROL) | PCCTWO_PRT_CTRL_DOEN);

	if (int_ena == 0) {
		sps = splhigh();
		sc->sc_icr = sc->sc_ipl | PCCTWO_ICR_EDGE;
		pcc2_reg_write(sc, PCC2REG_PRT_ACK_ICSR, sc->sc_icr);
		splx(sps);
	}
}

void
lpt_pcctwo_close(struct lpt_softc *sc)
{

	pcc2_reg_write(sc, PCC2REG_PRT_ACK_ICSR,
	    PCCTWO_ICR_ICLR | PCCTWO_ICR_EDGE);
	pcc2_reg_write(sc, PCC2REG_PRT_CONTROL, 0);
}

void
lpt_pcctwo_iprime(struct lpt_softc *sc)
{

	pcc2_reg_write(sc, PCC2REG_PRT_CONTROL,
	    pcc2_reg_read(sc, PCC2REG_PRT_CONTROL) | PCCTWO_PRT_CTRL_INP);

	delay(100);

	pcc2_reg_write(sc, PCC2REG_PRT_CONTROL,
	    pcc2_reg_read(sc, PCC2REG_PRT_CONTROL) & ~PCCTWO_PRT_CTRL_INP);

	delay(100);
}

void
lpt_pcctwo_speed(struct lpt_softc *sc, int speed)
{
	u_int8_t reg;

	reg = pcc2_reg_read(sc, PCC2REG_PRT_CONTROL);

	if (speed == LPT_STROBE_FAST)
		reg |= PCCTWO_PRT_CTRL_FAST;
	else
		reg &= ~PCCTWO_PRT_CTRL_FAST;

	pcc2_reg_write(sc, PCC2REG_PRT_CONTROL, reg);
}

int
lpt_pcctwo_notrdy(struct lpt_softc *sc, int err)
{
	u_int8_t status;
	u_int8_t new;

#define	LPS_INVERT	(PCCTWO_PRT_IN_SR_SEL)
#define	LPS_MASK	(PCCTWO_PRT_IN_SR_SEL | PCCTWO_PRT_IN_SR_FLT | \
			 PCCTWO_PRT_IN_SR_BSY | PCCTWO_PRT_IN_SR_PE)

	status = pcc2_reg_read(sc, PCC2REG_PRT_INPUT_STATUS) ^ LPS_INVERT;
	status &= LPS_MASK;

	if (err) {
		new = status & ~sc->sc_laststatus;
		sc->sc_laststatus = status;

		if (new & PCCTWO_PRT_IN_SR_SEL)
			log(LOG_NOTICE, "%s: offline\n",
			    device_xname(sc->sc_dev));
		else if (new & PCCTWO_PRT_IN_SR_PE)
			log(LOG_NOTICE, "%s: out of paper\n",
			    device_xname(sc->sc_dev));
		else if (new & PCCTWO_PRT_IN_SR_FLT)
			log(LOG_NOTICE, "%s: output error\n",
			    device_xname(sc->sc_dev));
	}

	return (status);
}

void
lpt_pcctwo_wr_data(struct lpt_softc *sc, u_char data)
{

	pcc2_reg_write16(sc, PCC2REG_PRT_DATA, (u_int16_t) data);
}
