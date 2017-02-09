/*	$NetBSD: clmpcc_pcctwo.c,v 1.20 2014/03/25 15:52:16 christos Exp $	*/

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
 * Cirrus Logic CD2401 4-channel serial chip. PCCchip2 Front-end.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clmpcc_pcctwo.c,v 1.20 2014/03/25 15:52:16 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/cons.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/clmpccvar.h>

#include <dev/mvme/pcctwovar.h>
#include <dev/mvme/pcctworeg.h>

/* XXXXSCW: Fixme */
#ifdef MVME68K
#include <mvme68k/dev/mainbus.h>
#else
#error Need consiack hook
#endif


/* Definition of the driver for autoconfig. */
int clmpcc_pcctwo_match(device_t, cfdata_t, void *);
void clmpcc_pcctwo_attach(device_t, device_t, void *);
void clmpcc_pcctwo_iackhook(struct clmpcc_softc *, int);
void clmpcc_pcctwo_consiackhook(struct clmpcc_softc *, int);

CFATTACH_DECL_NEW(clmpcc_pcctwo, sizeof(struct clmpcc_softc),
    clmpcc_pcctwo_match, clmpcc_pcctwo_attach, NULL, NULL);

extern struct cfdriver clmpcc_cd;

extern const struct cdevsw clmpcc_cdevsw;

/*
 * For clmpcccn*()
 */
cons_decl(clmpcc);


/*
 * Is the CD2401 chip present?
 */
int
clmpcc_pcctwo_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pcctwo_attach_args *pa;

	pa = aux;

	if (strcmp(pa->pa_name, clmpcc_cd.cd_name))
		return (0);

	pa->pa_ipl = cf->pcctwocf_ipl;

	return (1);
}

/*
 * Attach a found CD2401.
 */
void
clmpcc_pcctwo_attach(device_t parent, device_t self, void *aux)
{
	struct clmpcc_softc *sc;
	struct pcctwo_attach_args *pa = aux;
	int level = pa->pa_ipl;

	sc = device_private(self);
	sc->sc_dev = self;
	level = pa->pa_ipl;
	sc->sc_iot = pa->pa_bust;
	bus_space_map(pa->pa_bust, pa->pa_offset, 0x100, 0, &sc->sc_ioh);

	sc->sc_clk = 20000000;
	sc->sc_byteswap = CLMPCC_BYTESWAP_LOW;
	sc->sc_swaprtsdtr = 1;
	sc->sc_iackhook = clmpcc_pcctwo_iackhook;
	sc->sc_vector_base = PCCTWO_SCC_VECBASE;
	sc->sc_rpilr = 0x03;
	sc->sc_tpilr = 0x02;
	sc->sc_mpilr = 0x01;
	sc->sc_evcnt = pcctwointr_evcnt(level);

	/* Do common parts of CD2401 configuration. */
	clmpcc_attach(sc);

	/* Hook the interrupts */
	pcctwointr_establish(PCCTWOV_SCC_RX, clmpcc_rxintr, level, sc, NULL);
	pcctwointr_establish(PCCTWOV_SCC_RX_EXCEP, clmpcc_rxintr, level, sc,
	    NULL);
	pcctwointr_establish(PCCTWOV_SCC_TX, clmpcc_txintr, level, sc, NULL);
	pcctwointr_establish(PCCTWOV_SCC_MODEM, clmpcc_mdintr, level, sc, NULL);
}

void
clmpcc_pcctwo_iackhook(struct clmpcc_softc *sc, int which)
{
	bus_size_t offset;
	volatile u_char foo;

	switch (which) {
	case CLMPCC_IACK_MODEM:
		offset = PCC2REG_SCC_MODEM_PIACK;
		break;

	case CLMPCC_IACK_RX:
		offset = PCC2REG_SCC_RX_PIACK;
		break;

	case CLMPCC_IACK_TX:
		offset = PCC2REG_SCC_TX_PIACK;
		break;
	default:
#ifdef DEBUG
		printf("%s: Invalid IACK number '%d'\n",
		    device_xname(sc->sc_dev), which);
#endif
		panic("clmpcc_pcctwo_iackhook %d", which);
	}

	foo = pcc2_reg_read(sys_pcctwo, offset);
	__USE(foo);
}

/*
 * This routine is only used prior to clmpcc_attach() being called
 */
void
clmpcc_pcctwo_consiackhook(struct clmpcc_softc *sc, int which)
{
	bus_space_handle_t bush;
	bus_size_t offset;
	volatile u_char foo;

	switch (which) {
	case CLMPCC_IACK_MODEM:
		offset = PCC2REG_SCC_MODEM_PIACK;
		break;

	case CLMPCC_IACK_RX:
		offset = PCC2REG_SCC_RX_PIACK;
		break;

	case CLMPCC_IACK_TX:
		offset = PCC2REG_SCC_TX_PIACK;
		break;
	default:
#ifdef DEBUG
		printf("%s: Invalid IACK number '%d'\n",
		    device_xname(sc->sc_dev), which);
		panic("clmpcc_pcctwo_consiackhook");
#endif
		panic("clmpcc_pcctwo_iackhook %d", which);
	}

#ifdef MVME68K
	/*
	 * We need to fake the tag and handle since 'sys_pcctwo' will
	 * be NULL during early system startup...
	 */
	bush = (bus_space_handle_t) & (intiobase[MAINBUS_PCCTWO_OFFSET +
		PCCTWO_REG_OFF]);

	foo = bus_space_read_1(&_mainbus_space_tag, bush, offset);
	__USE(foo);
#else
#error Need consiack hook
#endif
}


/****************************************************************
 * Console support functions (MVME PCCchip2 specific!)
 ****************************************************************/

/*
 * Check for CD2401 console.
 */
void
clmpcccnprobe(struct consdev *cp)
{
	int maj;

#if defined(MVME68K)
	if (machineid != MVME_167 && machineid != MVME_177)
#elif defined(MVME88K)
	if (machineid != MVME_187)
#endif
	{
		cp->cn_pri = CN_DEAD;
		return;
	}

	/*
	 * Locate the major number
	 */
	maj = cdevsw_lookup_major(&clmpcc_cdevsw);

	/* Initialize required fields. */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_NORMAL;
}

void
clmpcccninit(struct consdev *cp)
{
	static struct clmpcc_softc cons_sc;

	cons_sc.sc_iot = &_mainbus_space_tag;
	bus_space_map(&_mainbus_space_tag,
	    intiobase_phys + MAINBUS_PCCTWO_OFFSET + PCCTWO_SCC_OFF,
	    PCC2REG_SIZE, 0, &cons_sc.sc_ioh);
	cons_sc.sc_clk = 20000000;
	cons_sc.sc_byteswap = CLMPCC_BYTESWAP_LOW;
	cons_sc.sc_swaprtsdtr = 1;
	cons_sc.sc_iackhook = clmpcc_pcctwo_consiackhook;
	cons_sc.sc_vector_base = PCCTWO_SCC_VECBASE;
	cons_sc.sc_rpilr = 0x03;
	cons_sc.sc_tpilr = 0x02;
	cons_sc.sc_mpilr = 0x01;

	clmpcc_cnattach(&cons_sc, 0, 9600);
}
