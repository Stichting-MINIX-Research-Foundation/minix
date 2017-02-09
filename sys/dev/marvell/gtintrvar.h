/*	$NetBSD: gtintrvar.h,v 1.1 2010/04/28 13:51:56 kiyohara Exp $	*/
/*
 * Copyright (c) 2009 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _MARVELL_GTINTRVAR_H_
#define _MARVELL_GTINTRVAR_H_

#include <dev/marvell/gtreg.h>

/*
 * Main Interrupt related functions
 */

static __inline uint32_t
discovery_enable_intr(struct gt_softc *sc, int irq)
{
	bus_size_t reg; 
	uint32_t cim;

	reg = (irq < 32) ? ICR_CIM_LO : ICR_CIM_HI;
	cim = bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg);
	cim |= 1 << (irq & 31);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg, cim);
	return cim;
}

static __inline uint32_t
discovery_disable_intr(struct gt_softc *sc, int irq)
{
	bus_size_t reg; 
	uint32_t cim;

	reg = (irq < 32) ? ICR_CIM_LO : ICR_CIM_HI;
	cim = bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg);
	cim &= ~(1 << (irq & 31));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg, cim);
	return cim;
}

static __inline int
discovery_mic_low(struct gt_softc *sc)
{

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, ICR_MIC_LO);
}

static __inline int
discovery_mic_high(struct gt_softc *sc)
{

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, ICR_MIC_HI);
}


/*
 * GPP Interrupt related functions
 */

static __inline uint32_t
discovery_gpp_enable_intr(struct gt_softc *sc, int pin)
{
	uint32_t gppim;

	gppim = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Mask);
	gppim |= 1 << pin;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Mask, gppim);
	return gppim;
}

static __inline uint32_t
discovery_gpp_disable_intr(struct gt_softc *sc, int pin)
{
	uint32_t gppim;

	gppim = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Mask);
	gppim &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Mask, gppim);
	return gppim;
}

static __inline void
discovery_gpp_clear_cause(struct gt_softc *sc, int pin)
{
	uint32_t gppic;

	gppic =
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Cause);
	gppic &= ~(1 << pin);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Cause,
	    gppic);
}

static __inline int
discovery_gpp_cause(struct gt_softc *sc)
{

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Cause);
}

static __inline int
discovery_gpp_mask(struct gt_softc *sc)
{

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, GT_GPP_Interrupt_Mask);
}
#endif	/* _MARVELL_GTINTRVAR_H_ */
