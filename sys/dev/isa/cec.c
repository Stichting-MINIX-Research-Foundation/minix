/*	$NetBSD: cec.c,v 1.13 2012/10/27 17:18:24 chs Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry.
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
__KERNEL_RCSID(0, "$NetBSD: cec.c,v 1.13 2012/10/27 17:18:24 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/gpib/gpibvar.h>

#include <dev/ic/nec7210reg.h>

#ifndef DEBUG
#define DEBUG
#endif

#ifdef DEBUG
int cecdebug = 0x1f;
#define DPRINTF(flag, str)	if (cecdebug & (flag)) printf str
#define DBG_FOLLOW	0x01
#define DBG_CONFIG	0x02
#define DBG_INTR	0x04
#define DBG_REPORTTIME	0x08
#define DBG_FAIL	0x10
#define DBG_WAIT	0x20
#else
#define DPRINTF(flag, str)	/* nothing */
#endif

#define CEC_IOSIZE	8

struct cec_softc {
	device_t sc_dev;		/* generic device glue */

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	isa_chipset_tag_t sc_ic;
	int sc_drq;
	void *sc_ih;

	int sc_myaddr;			/* my address */
	struct gpib_softc *sc_gpib;

	volatile int sc_flags;
#define	CECF_IO		0x1
#define	CECF_PPOLL	0x4
#define	CECF_READ	0x8
#define	CECF_TIMO	0x10
#define CECF_USEDMA	0x20
	int sc_ppoll_slave;		/* XXX stash our ppoll address */
	callout_t sc_timeout_ch;
};

int	cecprobe(device_t, cfdata_t, void *);
void	cecattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cec, sizeof(struct cec_softc),
	cecprobe, cecattach, NULL, NULL);

void	cecreset(void *);
int	cecpptest(void *, int);
void	cecppwatch(void *, int);
void	cecppclear(void *);
void	cecxfer(void *, int, int, void *, int, int, int);
void	cecgo(void *v);
int	cecintr(void *);
int	cecsendcmds(void *, void *, int);
int	cecsenddata(void *, void *, int);
int	cecrecvdata(void *, void *, int);
int	cecgts(void *);
int	cectc(void *, int);
void	cecifc(void *);

static int	cecwait(struct cec_softc *, int, int);
static void	cectimeout(void *v);
static int	nec7210_setaddress(struct cec_softc *, int, int);
static void	nec7210_init(struct cec_softc *);
static void	nec7210_ifc(struct cec_softc *);

/*
 * Our chipset structure.
 */
struct gpib_chipset_tag cec_ic = {
	cecreset,
	NULL,
	NULL,
	cecpptest,
	cecppwatch,
	cecppclear,
	cecxfer,
	cectc,
	cecgts,
	cecifc,
	cecsendcmds,
	cecsenddata,
	cecrecvdata,
	NULL,
	NULL
};

int cecwtimeout = 0x10000;
int cecdmathresh = 3;

int
cecprobe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	DPRINTF(DBG_CONFIG, ("cecprobe: called\n"));

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	if (ia->ia_ndrq > 0 && ia->ia_drq[0].ir_drq == ISA_UNKNOWN_DRQ)
		ia->ia_ndrq = 0;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, CEC_IOSIZE, 0, &ioh))
		return (0);

	/* XXX insert probe here */

	ia->ia_io[0].ir_size = CEC_IOSIZE;
	ia->ia_niomem = 0;

	bus_space_unmap(iot, ioh, CEC_IOSIZE);

	return (1);
}

void
cecattach(device_t parent, device_t self, void *aux)
{
	struct cec_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	struct gpibdev_attach_args ga;
	bus_size_t maxsize;

	printf("\n");

	DPRINTF(DBG_CONFIG, ("cecattach: called\n"));

	sc->sc_dev = self;
	sc->sc_iot = ia->ia_iot;
	sc->sc_ic = ia->ia_ic;

	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, CEC_IOSIZE,
	    0, &sc->sc_ioh) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map I/O space\n");
		return;
	}

	if (ia->ia_ndrq > 0) {
		sc->sc_flags |= CECF_USEDMA;
		sc->sc_drq = ia->ia_drq[0].ir_drq;

		(void) isa_drq_alloc(sc->sc_ic, sc->sc_drq);
		maxsize = isa_dmamaxsize(sc->sc_ic, sc->sc_drq);
		if (isa_dmamap_create(sc->sc_ic, sc->sc_drq,
		    maxsize, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW)) {
			aprint_error_dev(sc->sc_dev, "unable to create map for drq %d\n",
			    sc->sc_drq);
			sc->sc_flags &= ~CECF_USEDMA;
		}
	}

	sc->sc_myaddr = 15;		/* XXX */

	cecreset(sc);
	(void) nec7210_setaddress(sc, sc->sc_myaddr, -1);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_BIO, cecintr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
		return;
	}

	callout_init(&sc->sc_timeout_ch, 0);

	/* attach MI GPIB bus */
	cec_ic.cookie = (void *)sc;
	ga.ga_ic = &cec_ic;
	ga.ga_address = sc->sc_myaddr;
	sc->sc_gpib =
	    (struct gpib_softc *)config_found(self, &ga, gpibdevprint);
}

int
cecintr(void *v)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t stat1, stat2;

	stat1 = bus_space_read_1(iot, ioh, NEC7210_ISR1);
	stat2 = bus_space_read_1(iot, ioh, NEC7210_ISR2);

	DPRINTF(DBG_INTR, ("cecintr: sc=%p stat1=0x%x stat2=0x%x\n",
	    sc, stat1, stat2));

	if (sc->sc_flags & CECF_IO) {

		if (sc->sc_flags & CECF_TIMO)
			callout_stop(&sc->sc_timeout_ch);

		bus_space_write_1(iot, ioh, NEC7210_IMR1, 0);
		bus_space_write_1(iot, ioh, NEC7210_IMR2, 0);
		bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_TCA);
		sc->sc_flags &= ~(CECF_IO | CECF_READ | CECF_TIMO);
		if (sc->sc_flags & CECF_USEDMA)
			isa_dmadone(sc->sc_ic, sc->sc_drq);
		gpibintr(sc->sc_gpib);

	} else if (sc->sc_flags & CECF_PPOLL) {

		if (cecpptest(sc, sc->sc_ppoll_slave)) {
			sc->sc_flags &= ~CECF_PPOLL;
			bus_space_write_1(iot, ioh, NEC7210_IMR2, 0);
			gpibintr(sc->sc_gpib);
		}

	}
	return (1);
}

void
cecreset(void *v)
{
	struct cec_softc *sc = v;
	u_int8_t cmd;

	DPRINTF(DBG_FOLLOW, ("cecreset: sc=%p\n", sc));

	nec7210_init(sc);
	nec7210_ifc(sc);
	/* we're now the system controller */

	/* XXX should be pushed higher */

	/* universal device clear */
	cmd = GPIBCMD_DCL;
	(void) cecsendcmds(sc, &cmd, 1);
	/* delay for devices to clear */
	DELAY(100000);
}

int
cecsendcmds(void *v, void *ptr, int origcnt)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int cnt = origcnt;
	u_int8_t *addr = ptr;

	DPRINTF(DBG_FOLLOW, ("cecsendcmds: sc=%p, ptr=%p cnt=%d\n",
	    sc, ptr, origcnt));

	while (--cnt >= 0) {
		bus_space_write_1(iot, ioh, NEC7210_CDOR, *addr++);
		if (cecwait(sc, 0, ISR2_CO))
			return (origcnt - cnt - 1);
	}
	return (origcnt);
}


int
cecrecvdata(void *v, void *ptr, int origcnt)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int cnt = origcnt;
	u_int8_t *addr = ptr;

	DPRINTF(DBG_FOLLOW, ("cecrecvdata: sc=%p, ptr=%p cnt=%d\n",
	    sc, ptr, origcnt));

	/* XXX holdoff on end */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NEC7210_AUXMR, AUXCMD_RHDF);

	if (cnt) {
		while (--cnt >= 0) {
			if (cecwait(sc, ISR1_DI, 0))
				return (origcnt - cnt - 1);
			*addr++ = bus_space_read_1(iot, ioh, NEC7210_DIR);
		}
	}
	return (origcnt);
}

int
cecsenddata(void *v, void *ptr, int origcnt)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int cnt = origcnt;
	u_int8_t *addr = ptr;

	DPRINTF(DBG_FOLLOW, ("cecdsenddata: sc=%p, ptr=%p cnt=%d\n",
	    sc, ptr, origcnt));

	if (cnt) {
		while (--cnt > 0) {
			bus_space_write_1(iot, ioh, NEC7210_CDOR, *addr++);
			if (cecwait(sc, ISR1_DO, 0))
				return (origcnt - cnt - 1);
		}
		bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_SEOI);
		bus_space_write_1(iot, ioh, NEC7210_CDOR, *addr);
		(void) cecwait(sc, ISR1_DO, 0);
	}
	return (origcnt);
}

int
cectc(void *v, int sync)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t adsr;
	int timo = cecwtimeout;

	DPRINTF(DBG_FOLLOW, ("cectc: sc=%p, sync=%d\n", sc, sync));

	adsr = bus_space_read_1(iot, ioh, NEC7210_ADSR);
#if 0
	if ((adsr & (ADSR_CIC | ADSR_NATN)) == ADSR_CIC) {
		DPRINTF(0xff, ("cectc: already CIC\n"));
		return (0);
	}
#endif

	if (sync) {
		bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_RHDF);
		bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_TCS);
	} else {
		bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_TCA);
	}

	/* wait until ATN is asserted */
	for (;;) {
		adsr = bus_space_read_1(iot, ioh, NEC7210_ADSR);
		if (--timo == 0) {
			DPRINTF(DBG_REPORTTIME, ("cectc: timeout\n"));
			return (1);
		}
		if ((adsr & ADSR_NATN) == 0)
			break;
		DELAY(1);
	}

	return (0);
}

int
cecgts(void *v)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t adsr;
	int timo = cecwtimeout;

	DPRINTF(DBG_FOLLOW, ("cecgts: sc=%p\n", sc));

	adsr = bus_space_read_1(iot, ioh, NEC7210_ADSR);
#if 0
	if ((adsr & (ADSR_CIC | ADSR_NATN)) == ADSR_NATN) {
		DPRINTF(0xff, ("cecgts: already standby\n"));
		return (0);
	}
#endif

	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_GTS);

	/* wait unit ATN is released */
	for (;;) {
		adsr = bus_space_read_1(iot, ioh, NEC7210_ADSR);
		if (--timo == 0) {
			DPRINTF(DBG_REPORTTIME, ("cecgts: timeout\n"));
			return (1);
		}
		if ((adsr & ADSR_NATN) == ADSR_NATN)
			break;
		DELAY(1);
	}

	return (0);
}

int
cecpptest(void *v, int slave)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int ppoll;

	DPRINTF(DBG_FOLLOW, ("cecpptest: sc=%p slave=%d\n", sc, slave));

	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_EPP);
	DELAY(25);
	ppoll = bus_space_read_1(iot, ioh, NEC7210_CPTR);
	DPRINTF(0xff, ("cecpptest: ppoll=%x\n", ppoll));
	return ((ppoll & (0x80 >> slave)) != 0);
}

void
cecppwatch(void *v, int slave)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	DPRINTF(DBG_FOLLOW, ("cecppwatch: sc=%p\n", sc));

	sc->sc_flags |= CECF_PPOLL;
	sc->sc_ppoll_slave = slave;
	bus_space_write_1(iot, ioh, NEC7210_IMR2, IMR2_CO);
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_EPP);
}

void
cecppclear(void *v)
{
	struct cec_softc *sc = v;

	DPRINTF(DBG_FOLLOW, ("cecppclear: sc=%p\n", sc));

	sc->sc_flags &= ~CECF_PPOLL;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, NEC7210_IMR2, 0);
}

void
cecxfer(void *v, int slave, int sec, void *buf, int count, int dir, int timo)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	DPRINTF(DBG_FOLLOW,
	    ("cecxfer: slave=%d sec=%d buf=%p count=%d dir=%x timo=%d\n",
	    slave, sec, buf, count, dir, timo));

	sc->sc_flags |= CECF_IO;
	if (dir == GPIB_READ)
		sc->sc_flags |= CECF_READ;
	if (timo) {
		sc->sc_flags |= CECF_TIMO;
		callout_reset(&sc->sc_timeout_ch, 5*hz, cectimeout, sc);
	}

	if (sc->sc_flags & CECF_READ) {
		DPRINTF(DBG_FOLLOW, ("cecxfer: DMA read request\n"));
		if ((sc->sc_flags & CECF_USEDMA) != 0) {
			isa_dmastart(sc->sc_ic, sc->sc_drq, buf, count, NULL,
			    DMAMODE_READ | DMAMODE_DEMAND, BUS_DMA_NOWAIT);
			bus_space_write_1(iot, ioh, NEC7210_IMR2, IMR2_DMAI);
			bus_space_write_1(iot, ioh, NEC7210_IMR1, IMR1_END);
			// XXX (void) cecrecv(sc, slave, sec, NULL, 0);
			(void) gpibrecv(&cec_ic, slave, sec, NULL, 0);
		} else {
			/* XXX this doesn't work */
			DPRINTF(DBG_FOLLOW, ("cecxfer: polling instead\n"));
			bus_space_write_1(iot, ioh, NEC7210_IMR1, IMR1_END);
			// XXX (void) cecrecv(sc, slave, sec, buf, count);
			(void) gpibrecv(&cec_ic, slave, sec, buf, count);
			bus_space_write_1(iot, ioh, NEC7210_IMR2, IMR2_CO);
		}
	} else {
		DPRINTF(DBG_FOLLOW, ("cecxfer: DMA write request\n"));
		bus_space_write_1(iot, ioh, NEC7210_IMR2, 0);
		if (count < cecdmathresh ||
		    (sc->sc_flags & CECF_USEDMA) == 0) {
			DPRINTF(DBG_FOLLOW, ("cecxfer: polling instead\n"));
			// XXX (void) cecsend(sc, slave, sec, buf, count);
			(void) gpibsend(&cec_ic, slave, sec, buf, count);
			bus_space_write_1(iot, ioh, NEC7210_IMR2, IMR2_CO);
			return;
		}
		/* we send the last byte with EOI set */
		isa_dmastart(sc->sc_ic, sc->sc_drq, buf, count-1, NULL,
		    DMAMODE_WRITE | DMAMODE_DEMAND, BUS_DMA_NOWAIT);
		bus_space_write_1(iot, ioh, NEC7210_IMR2, IMR2_DMAO);
		// XXX (void) cecsend(sc, slave, sec, NULL, 0);
		(void) gpibsend(&cec_ic, slave, sec, NULL, 0);
		while (!isa_dmafinished(sc->sc_ic, sc->sc_drq))
			DELAY(1);
		(void) cecwait(sc, ISR1_DO, 0);
		bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_SEOI);
		bus_space_write_1(iot, ioh, NEC7210_CDOR, *(char *)buf+count);
		/* generate interrupt */
		bus_space_write_1(iot, ioh, NEC7210_IMR1, IMR1_DO);
	}
}

void
cecifc(void *v)
{
	struct cec_softc *sc = v;

	nec7210_ifc(sc);
}

static int
nec7210_setaddress(struct cec_softc *sc, int pri, int sec)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t admr;

	/* assign our primary address */
	bus_space_write_1(iot, ioh, NEC7210_ADDR, (pri & ADDR_MASK));

	admr = ADMR_TRM0 | ADMR_TRM1;

	/* assign our secondary address */
	if (sec != -1) {
		bus_space_write_1(iot, ioh, NEC7210_ADDR,
		    (ADDR_ARS | (sec & ADDR_MASK)));
		admr |= ADMR_ADM1;
	} else {
		/* disable secondary address */
		bus_space_write_1(iot, ioh, NEC7210_ADDR,
		    (ADDR_ARS | ADDR_DT | ADDR_DL));
		admr |= ADMR_ADM0;
	}
	bus_space_write_1(iot, ioh, NEC7210_ADMR, admr);

	return (0);
}

static void
nec7210_init(struct cec_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* reset chip */
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_CRST);

	/* clear interrupts */
	bus_space_read_1(iot, ioh, NEC7210_CPTR);
	bus_space_read_1(iot, ioh, NEC7210_ISR1);
	bus_space_read_1(iot, ioh, NEC7210_ISR2);

	/* initialise interrupts */
	bus_space_write_1(iot, ioh, NEC7210_IMR1, 0);
	bus_space_write_1(iot, ioh, NEC7210_IMR2, 0);
	bus_space_write_1(iot, ioh, NEC7210_SPMR, 0);
	bus_space_write_1(iot, ioh, NEC7210_EOSR, 0);

	/* set internal clock to 8MHz */
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, (AUXMR_ICR | 0x8));
	/* parallel poll unconfigure */
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, (AUXMR_PPOLL | PPOLL_PPU));

	/* assign our address */
	bus_space_write_1(iot, ioh, NEC7210_ADDR, 0);
	/* disable secondary address */
	bus_space_write_1(iot, ioh, NEC7210_ADDR,
	    (ADDR_ARS | ADDR_DT | ADDR_DL));

	/* setup transceivers */
	bus_space_write_1(iot, ioh, NEC7210_ADMR,
	    (ADMR_ADM0 | ADMR_TRM0 | ADMR_TRM1));
	bus_space_write_1(iot, ioh, NEC7210_AUXMR,
	    (AUXMR_REGA | AUX_A_HSNORM));

	/* set INT pin to active high */
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXMR_REGB);
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXMR_REGE);

	/* holdoff on end condition */
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, (AUXMR_REGA | AUX_A_HLDE));

	/* reconnect to bus */
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, (AUXMR_CMD | AUXCMD_IEPON));
}

/*
 * Place all devices on the bus into quiescient state ready for
 * remote programming.
 * Obviously, we're the system controller upon exit.
 */
void
nec7210_ifc(struct cec_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

/*XXX*/	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_TCA);
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_CREN);
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_SIFC);
	/* wait for devices to enter quiescient state */
	DELAY(100);
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_CIFC);
	bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_SREN);
}

static int
cecwait(struct cec_softc *sc, int x1, int x2)
{
	int timo = cecwtimeout;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t stat1, stat2;

	DPRINTF(DBG_WAIT, ("cecwait: sc=%p, x1=0x%x x2=0x%x\n", sc, x1, x2));

	for (;;) {
		stat1 = bus_space_read_1(iot, ioh, NEC7210_ISR1);
		stat2 = bus_space_read_1(iot, ioh, NEC7210_ISR2);
#if 0
		if ((stat1 & ISR1_ERR)) {
			DPRINTF(DBG_WAIT, ("cecwait: got ERR\n"));
			return (1);
		}
#endif
		if (--timo == 0) {
			DPRINTF(DBG_REPORTTIME,
			    ("cecwait: timeout x1=0x%x x2=0x%x\n", x1, x2));
			return (1);
		}
		if ((stat1 & x1) || (stat2 & x2))
			break;
		DELAY(1);
	}
	return (0);
}

static void
cectimeout(void *v)
{
	struct cec_softc *sc = v;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	DPRINTF(DBG_FOLLOW, ("cectimeout: sc=%p\n", sc));

	s = splbio();
	if (sc->sc_flags & CECF_IO) {
		bus_space_write_1(iot, ioh, NEC7210_IMR1, 0);
		bus_space_write_2(iot, ioh, NEC7210_IMR2, 0);
		bus_space_write_1(iot, ioh, NEC7210_AUXMR, AUXCMD_TCA);
		sc->sc_flags &= ~(CECF_IO | CECF_READ | CECF_TIMO);
		isa_dmaabort(sc->sc_ic, sc->sc_drq);
		aprint_error_dev(sc->sc_dev, "%s timeout\n",
		    sc->sc_flags & CECF_READ ? "read" : "write");
		gpibintr(sc->sc_gpib);
	}
	splx(s);
}
