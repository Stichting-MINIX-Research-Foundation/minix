/*	$NetBSD: uba_bi.c,v 1.15 2009/11/23 02:13:45 rmind Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of
 *	Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * DWBUA BI-Unibus adapter
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uba_bi.c,v 1.15 2009/11/23 02:13:45 rmind Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/sid.h>
#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/trap.h>
#include <machine/scb.h>

#include <vax/bi/bireg.h>
#include <vax/bi/bivar.h>

#include <vax/uba/ubareg.h>
#include <vax/uba/ubavar.h>

#include "locators.h"

#define	BUA(uba)	((struct dwbua_regs *)(uba))

static	int uba_bi_match(device_t, cfdata_t, void *);
static	void uba_bi_attach(device_t, device_t, void *);
static	void bua_init(struct uba_softc *);
static	void bua_purge(struct uba_softc *, int);

/* bua_csr */
#define BUACSR_ERR      0x80000000      /* composite error */
#define BUACSR_BIF      0x10000000      /* BI failure */
#define BUACSR_SSYNTO   0x08000000      /* slave sync timeout */
#define BUACSR_UIE      0x04000000      /* unibus interlock error */
#define BUACSR_IVMR     0x02000000      /* invalid map register */
#define BUACSR_BADBDP   0x01000000      /* bad BDP select */
#define BUACSR_BUAEIE   0x00100000      /* bua error interrupt enable (?) */
#define BUACSR_UPI      0x00020000      /* unibus power init */
#define BUACSR_UREGDUMP 0x00010000      /* microdiag register dump */
#define BUACSR_IERRNO   0x000000ff      /* mask for internal errror number */

/* bua_offset */
#define BUAOFFSET_MASK  0x00003e00      /* hence max offset = 15872 */

/* bua_dpr */
#define BUADPR_DPSEL    0x00e00000      /* data path select (?) */
#define BUADPR_PURGE    0x00000001      /* purge bdp */

/* bua_map -- in particular, those bits that are not in DW780s & DW750s */
#define BUAMR_IOADR     0x40000000      /* I/O address space */
#define BUAMR_LAE       0x04000000      /* longword access enable */

static	int allocvec;

CFATTACH_DECL_NEW(uba_bi, sizeof(struct uba_softc),
    uba_bi_match, uba_bi_attach, NULL, NULL);

struct dwbua_regs {
	struct  biiregs bn_biic;   /* interface */
	int	pad1[396];
	int	bn_csr;
	int	bn_vor;		/* Vector offset from SCB */
	int	bn_fubar;	/* Failed Unibus address register */
	int	bn_bifar;	/* BI failed address register */
	int	bn_mdiag[5];	/* microdiag regs for BDP */
	int	pad2[3];
	int	bn_dpcsr[6];	/* Data path control and status register */
	int	pad3[38];
	struct	pte bn_map[UBAPAGES];	/* Unibus map registers */
	int	pad4[UBAIOPAGES];
};

/*
 * Poke at a supposed DWBUA to see if it is there.
 */
static int
uba_bi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct bi_attach_args *ba = aux;

	if ((ba->ba_node->biic.bi_dtype != BIDT_DWBUA) &&
	    (ba->ba_node->biic.bi_dtype != BIDT_KLESI))
		return 0;

	if (cf->cf_loc[BICF_NODE] != BICF_NODE_DEFAULT &&
	    cf->cf_loc[BICF_NODE] != ba->ba_nodenr)
		return 0;

	return 1;
}

void
uba_bi_attach(device_t parent, device_t self, void *aux)
{
	struct uba_softc *sc = device_private(self);
	struct bi_attach_args *ba = aux;
	volatile int timo;

	if (ba->ba_node->biic.bi_dtype == BIDT_DWBUA)
		printf(": DWBUA\n");
	else
		printf(": KLESI-B\n");

	/*
	 * Fill in bus specific data.
	 */
	sc->uh_dev = self;
	sc->uh_uba = (void *)ba->ba_node;
	sc->uh_nbdp = NBDPBUA;
/*	sc->uh_nr is 0; uninteresting here */
/*	sc->uh_afterscan; not used */
/*	sc->uh_errchk; not used */
/*	sc->uh_beforescan */
	sc->uh_ubapurge = bua_purge;
	sc->uh_ubainit = bua_init;
/*	sc->uh_type not used */
	sc->uh_memsize = UBAPAGES;
	sc->uh_mr = BUA(sc->uh_uba)->bn_map;

#ifdef notdef
	/* Can we get separate interrupts? */
	scb->scb_nexvec[1][ba->ba_nodenr] = &sc->sc_ivec;
#endif
	BUA(sc->uh_uba)->bn_biic.bi_csr |= BICSR_ARB_NONE;
	BUA(sc->uh_uba)->bn_biic.bi_csr |= BICSR_STS | BICSR_INIT;
	DELAY(1000);
	timo = 1000;
	while (BUA(sc->uh_uba)->bn_biic.bi_csr & BICSR_BROKE)
		if (timo == 0) {
			aprint_error_dev(self, "BROKE bit set\n");
			return;
		}

	BUA(sc->uh_uba)->bn_biic.bi_intrdes = ba->ba_intcpu;
	BUA(sc->uh_uba)->bn_biic.bi_csr =
	    (BUA(sc->uh_uba)->bn_biic.bi_csr&~BICSR_ARB_MASK) | BICSR_ARB_HIGH;
	BUA(sc->uh_uba)->bn_vor = VAX_NBPG + (VAX_NBPG * allocvec++);

	uba_attach(sc, BUA(sc->uh_uba)->bn_biic.bi_sadr + UBAPAGES * VAX_NBPG);
}


void
bua_init(struct uba_softc *sc)
{
	BUA(sc->uh_uba)->bn_csr |= BUACSR_UPI;
	DELAY(500000);
};

void
bua_purge(struct uba_softc *sc, int bdp)
{
	BUA(sc->uh_uba)->bn_dpcsr[bdp] |= BUADPR_PURGE;
}
