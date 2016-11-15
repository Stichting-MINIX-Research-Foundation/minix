/* $NetBSD: dwc_tmr.c,v 1.1 2015/01/17 15:04:47 jmcneill Exp $ */

/*-
 * Copyright (c) 2015 Jared D. McNeill <jmcneill@invisible.ca>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dwc_tmr.c,v 1.1 2015/01/17 15:04:47 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/timetc.h>

#include <dev/ic/dwc_tmr_reg.h>
#include <dev/ic/dwc_tmr_var.h>

#define TIMER_READ(sc, reg) \
    bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))
#define TIMER_WRITE(sc, reg, val) \
    bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))

static u_int	dwc_tmr_get_timecount(struct timecounter *);

void
dwc_tmr_attach_subr(struct dwc_tmr_softc *sc, u_int64_t freq)
{

	TIMER_WRITE(sc, DWC_TMR_CONTROL_REG, 0);
	TIMER_WRITE(sc, DWC_TMR_LOAD_COUNT_REG, ~0);
	TIMER_WRITE(sc, DWC_TMR_CONTROL_REG, DWC_TMR_CONTROL_ENABLE);

	sc->sc_tc.tc_get_timecount = dwc_tmr_get_timecount;
	sc->sc_tc.tc_poll_pps = NULL;
	sc->sc_tc.tc_counter_mask = ~0;
	sc->sc_tc.tc_frequency = freq;
	sc->sc_tc.tc_name = device_xname(sc->sc_dev);
	sc->sc_tc.tc_priv = sc;
	sc->sc_tc.tc_quality = 900;

	tc_init(&sc->sc_tc);
}

static u_int
dwc_tmr_get_timecount(struct timecounter *tc)
{
	struct dwc_tmr_softc *sc = tc->tc_priv;

	return ~TIMER_READ(sc, DWC_TMR_CURRENT_VALUE_REG);
}
