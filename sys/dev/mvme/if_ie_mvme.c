/*	$NetBSD: if_ie_mvme.c,v 1.20 2014/03/25 15:52:33 christos Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ie_mvme.c,v 1.20 2014/03/25 15:52:33 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <machine/autoconf.h>
#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/ic/i82586reg.h>
#include <dev/ic/i82586var.h>

#include <dev/mvme/if_iereg.h>
#include <dev/mvme/pcctwovar.h>
#include <dev/mvme/pcctworeg.h>


int ie_pcctwo_match(device_t, cfdata_t, void *);
void ie_pcctwo_attach(device_t, device_t, void *);

struct ie_pcctwo_softc {
	struct ie_softc ps_ie;
	bus_space_tag_t ps_bust;
	bus_space_handle_t ps_bush;
	struct evcnt ps_evcnt;
};

CFATTACH_DECL_NEW(ie_pcctwo, sizeof(struct ie_pcctwo_softc),
    ie_pcctwo_match, ie_pcctwo_attach, NULL, NULL);

extern struct cfdriver ie_cd;


/* Functions required by the i82586 MI driver */
static void ie_reset(struct ie_softc *, int);
static int ie_intrhook(struct ie_softc *, int);
static void ie_hwinit(struct ie_softc *);
static void ie_atten(struct ie_softc *, int);

static void ie_copyin(struct ie_softc *, void *, int, size_t);
static void ie_copyout(struct ie_softc *, const void *, int, size_t);

static u_int16_t ie_read_16(struct ie_softc *, int);
static void ie_write_16(struct ie_softc *, int, u_int16_t);
static void ie_write_24(struct ie_softc *, int, int);

/*
 * i82596 Support Routines for MVME1[67][27] and MVME187 Boards
 */
static void
ie_reset(struct ie_softc *sc, int why)
{
	struct ie_pcctwo_softc *ps;
	u_int32_t scp_addr;

	ps = (struct ie_pcctwo_softc *) sc;

	switch (why) {
	case CHIP_PROBE:
	case CARD_RESET:
		bus_space_write_2(ps->ps_bust, ps->ps_bush, IE_MPUREG_UPPER,
		    IE_PORT_RESET);
		bus_space_write_2(ps->ps_bust, ps->ps_bush, IE_MPUREG_LOWER, 0);
		delay(1000);

		/*
		 * Set the BUSY and BUS_USE bytes here, since the MI code
		 * incorrectly assumes it can use byte addressing to set it.
		 * (due to wrong-endianess of the chip)
		 */
		ie_write_16(sc, IE_ISCP_BUSY(sc->iscp), 1);
		ie_write_16(sc, IE_SCP_BUS_USE(sc->scp), IE_BUS_USE);

		scp_addr = sc->scp + (u_int) sc->sc_iobase;
		scp_addr |= IE_PORT_ALT_SCP;

		bus_space_write_2(ps->ps_bust, ps->ps_bush, IE_MPUREG_UPPER,
		    scp_addr & 0xffff);
		bus_space_write_2(ps->ps_bust, ps->ps_bush, IE_MPUREG_LOWER,
		    (scp_addr >> 16) & 0xffff);
		delay(1000);
		break;
	}
}

/* ARGSUSED */
static int
ie_intrhook(struct ie_softc *sc, int when)
{
	u_int8_t reg;

	if (when == INTR_EXIT) {
		reg = pcc2_reg_read(sys_pcctwo, PCC2REG_ETH_ICSR);
		reg |= PCCTWO_ICR_ICLR;
		pcc2_reg_write(sys_pcctwo, PCC2REG_ETH_ICSR, reg);
	}
	return (0);
}

/* ARGSUSED */
static void
ie_hwinit(struct ie_softc *sc)
{
	u_int8_t reg;

	reg = pcc2_reg_read(sys_pcctwo, PCC2REG_ETH_ICSR);
	reg |= PCCTWO_ICR_IEN | PCCTWO_ICR_ICLR;
	pcc2_reg_write(sys_pcctwo, PCC2REG_ETH_ICSR, reg);
}

/* ARGSUSED */
static void
ie_atten(struct ie_softc *sc, int reason)
{
	struct ie_pcctwo_softc *ps;

	ps = (struct ie_pcctwo_softc *) sc;
	bus_space_write_4(ps->ps_bust, ps->ps_bush, IE_MPUREG_CA, 0);
}

static void
ie_copyin(struct ie_softc *sc, void *dst, int offset, size_t size)
{
	if (size == 0)		/* This *can* happen! */
		return;

#if 0
	bus_space_read_region_1(sc->bt, sc->bh, offset, dst, size);
#else
	/* A minor optimisation ;-) */
	memcpy(dst, (void *) ((u_long) sc->bh + (u_long) offset), size);
#endif
}

static void
ie_copyout(struct ie_softc *sc, const void *src, int offset, size_t size)
{
	if (size == 0)		/* This *can* happen! */
		return;

#if 0
	bus_space_write_region_1(sc->bt, sc->bh, offset, src, size);
#else
	/* A minor optimisation ;-) */
	memcpy((void *) ((u_long) sc->bh + (u_long) offset), src, size);
#endif
}

static u_int16_t
ie_read_16(struct ie_softc *sc, int offset)
{

	return (bus_space_read_2(sc->bt, sc->bh, offset));
}

static void
ie_write_16(struct ie_softc *sc, int offset, u_int16_t value)
{

	bus_space_write_2(sc->bt, sc->bh, offset, value);
}

static void
ie_write_24(struct ie_softc *sc, int offset, int addr)
{

	addr += (int) sc->sc_iobase;

	bus_space_write_2(sc->bt, sc->bh, offset, addr & 0xffff);
	bus_space_write_2(sc->bt, sc->bh, offset + 2, (addr >> 16) & 0x00ff);
}

/* ARGSUSED */
int
ie_pcctwo_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pcctwo_attach_args *pa;

	pa = aux;

	if (strcmp(pa->pa_name, ie_cd.cd_name))
		return (0);

	pa->pa_ipl = cf->pcctwocf_ipl;

	return (1);
}

/* ARGSUSED */
void
ie_pcctwo_attach(device_t parent, device_t self, void *aux)
{
	struct pcctwo_attach_args *pa;
	struct ie_pcctwo_softc *ps;
	struct ie_softc *sc;
	bus_dma_segment_t seg;
	int rseg;

	pa = aux;
	ps = device_private(self);
	sc = &ps->ps_ie;
	sc->sc_dev = self;

	/* Map the MPU controller registers in PCCTWO space */
	ps->ps_bust = pa->pa_bust;
	bus_space_map(pa->pa_bust, pa->pa_offset, IE_MPUREG_SIZE,
	    0, &ps->ps_bush);

	/* Get contiguous DMA-able memory for the IE chip */
	if (bus_dmamem_alloc(pa->pa_dmat, ether_data_buff_size, PAGE_SIZE, 0,
		&seg, 1, &rseg,
		BUS_DMA_NOWAIT | BUS_DMA_ONBOARD_RAM | BUS_DMA_24BIT) != 0) {
		aprint_error_dev(self, "Failed to allocate ether buffer\n");
		return;
	}
	if (bus_dmamem_map(pa->pa_dmat, &seg, rseg, ether_data_buff_size,
	    (void **) & sc->sc_maddr, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) {
		aprint_error_dev(self, "Failed to map ether buffer\n");
		bus_dmamem_free(pa->pa_dmat, &seg, rseg);
		return;
	}
	sc->bt = pa->pa_bust;
	sc->bh = (bus_space_handle_t) sc->sc_maddr;	/* XXXSCW Better way? */
	sc->sc_iobase = (void *) seg.ds_addr;
	sc->sc_msize = ether_data_buff_size;
	memset(sc->sc_maddr, 0, ether_data_buff_size);

	sc->hwreset = ie_reset;
	sc->hwinit = ie_hwinit;
	sc->chan_attn = ie_atten;
	sc->intrhook = ie_intrhook;
	sc->memcopyin = ie_copyin;
	sc->memcopyout = ie_copyout;
	sc->ie_bus_barrier = NULL;
	sc->ie_bus_read16 = ie_read_16;
	sc->ie_bus_write16 = ie_write_16;
	sc->ie_bus_write24 = ie_write_24;
	sc->sc_mediachange = NULL;
	sc->sc_mediastatus = NULL;

	sc->scp = 0;
	sc->iscp = sc->scp + ((IE_SCP_SZ + 15) & ~15);
	sc->scb = sc->iscp + IE_ISCP_SZ;
	sc->buf_area = sc->scb + IE_SCB_SZ;
	sc->buf_area_sz = sc->sc_msize - (sc->buf_area - sc->scp);

	/*
	 * BUS_USE -> Interrupt Active High (edge-triggered),
	 *            Lock function enabled,
	 *            Internal bus throttle timer triggering,
	 *            82586 operating mode.
	 */
	ie_write_16(sc, IE_SCP_BUS_USE(sc->scp), IE_BUS_USE);
	ie_write_24(sc, IE_SCP_ISCP(sc->scp), sc->iscp);
	ie_write_16(sc, IE_ISCP_SCB(sc->iscp), sc->scb);
	ie_write_24(sc, IE_ISCP_BASE(sc->iscp), sc->scp);

	/* This has the side-effect of resetting the chip */
	i82586_proberam(sc);

	/* Attach the MI back-end */
	i82586_attach(sc, "onboard", mvme_ea, NULL, 0, 0);

	/* Register the event counter */
	evcnt_attach_dynamic(&ps->ps_evcnt, EVCNT_TYPE_INTR,
	    pcctwointr_evcnt(pa->pa_ipl), "ether", device_xname(self));

	/* Finally, hook the hardware interrupt */
	pcctwointr_establish(PCCTWOV_LANC_IRQ, i82586_intr, pa->pa_ipl, sc,
	    &ps->ps_evcnt);
}
