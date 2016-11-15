/*	$NetBSD: pcctwo.c,v 1.11 2012/10/27 17:18:27 chs Exp $	*/

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
 * PCCchip2 and MCchip Driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcctwo.c,v 1.11 2012/10/27 17:18:27 chs Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/mvme/pcctworeg.h>
#include <dev/mvme/pcctwovar.h>

/*
 * Global Pointer to the PCCChip2/MCchip soft state, and chip ID
 */
struct pcctwo_softc *sys_pcctwo;

int pcctwoprint(void *, const char *);


/* ARGSUSED */
void
pcctwo_init(struct pcctwo_softc *sc, const struct pcctwo_device *pd, int devoff)
{
	struct pcctwo_attach_args npa;
	u_int8_t cid;

	/*
	 * Fix up the vector base for PCCChip2 Interrupts
	 */
	pcc2_reg_write(sc, PCC2REG_VECTOR_BASE, sc->sc_vecbase);

	/*
	 * Enable PCCChip2 Interrupts
	 */
	pcc2_reg_write(sc, PCC2REG_GENERAL_CONTROL,
	    pcc2_reg_read(sc, PCC2REG_GENERAL_CONTROL) | PCCTWO_GEN_CTRL_MIEN);

	/* What are we? */
	cid = pcc2_reg_read(sc, PCC2REG_CHIP_ID);

	/*
	 * Announce ourselves to the world in general
	 */
	if (cid == PCCTWO_CHIP_ID_PCC2)
		printf(": Peripheral Channel Controller (PCCchip2), Rev %d\n",
		    pcc2_reg_read(sc, PCC2REG_CHIP_REVISION));
	else
	if (cid == PCCTWO_CHIP_ID_MCCHIP)
		printf(": Memory Controller ASIC (MCchip), Rev %d\n",
		    pcc2_reg_read(sc, PCC2REG_CHIP_REVISION));

	/*
	 * Attach configured children.
	 */
	npa._pa_base = devoff;
	while (pd->pcc_name != NULL) {
		/*
		 * Note that IPL is filled in by match function.
		 */
		npa.pa_name = pd->pcc_name;
		npa.pa_ipl = -1;
		npa.pa_dmat = sc->sc_dmat;
		npa.pa_bust = sc->sc_bust;
		npa.pa_offset = pd->pcc_offset + devoff;
		pd++;

		/* Attach the device if configured. */
		(void) config_found(sc->sc_dev, &npa, pcctwoprint);
	}
}

int
pcctwoprint(void *aux, const char *cp)
{
	struct pcctwo_attach_args *pa;

	pa = aux;

	if (cp)
		aprint_normal("%s at %s", pa->pa_name, cp);

	aprint_normal(" offset 0x%lx", pa->pa_offset - pa->_pa_base);
	if (pa->pa_ipl != -1)
		aprint_normal(" ipl %d", pa->pa_ipl);

	return (UNCONF);
}

/*
 * pcctwointr_establish: Establish PCCChip2 Interrupt
 */
void
pcctwointr_establish(
	int vec,
	int (*hand)(void *),
	int lvl,
	void *arg,
	struct evcnt *evcnt)
{
	int vec2icsr;

#ifdef DEBUG
	if (vec < 0 || vec >= PCCTWOV_MAX) {
		printf("pcctwo: illegal vector offset: 0x%x\n", vec);
		panic("pcctwointr_establish");
	}
	if (lvl < 1 || lvl > 7) {
		printf("pcctwo: illegal interrupt level: %d\n", lvl);
		panic("pcctwointr_establish");
	}
	if (sys_pcctwo->sc_vec2icsr[vec] == -1) {
		printf("pcctwo: unsupported vector: %d\n", vec);
		panic("pcctwointr_establish");
	}
#endif

	vec2icsr = sys_pcctwo->sc_vec2icsr[vec];
	pcc2_reg_write(sys_pcctwo, VEC2ICSR_REG(vec2icsr), 0);

	/* Hook the interrupt */
	(*sys_pcctwo->sc_isrlink)(sys_pcctwo->sc_isrcookie, hand, arg,
	    lvl, vec + sys_pcctwo->sc_vecbase, evcnt);

	/* Enable it in hardware */
	pcc2_reg_write(sys_pcctwo, VEC2ICSR_REG(vec2icsr),
	    VEC2ICSR_INIT(vec2icsr) | lvl);
}

void
pcctwointr_disestablish(int vec)
{

#ifdef DEBUG
	if (vec < 0 || vec >= PCCTWOV_MAX) {
		printf("pcctwo: illegal vector offset: 0x%x\n", vec);
		panic("pcctwointr_disestablish");
	}
	if (sys_pcctwo->sc_vec2icsr[vec] == -1) {
		printf("pcctwo: unsupported vector: %d\n", vec);
		panic("pcctwointr_establish");
	}
#endif

	/* Disable it in hardware */
	pcc2_reg_write(sys_pcctwo, sys_pcctwo->sc_vec2icsr[vec], 0);

	(*sys_pcctwo->sc_isrunlink)(sys_pcctwo->sc_isrcookie,
	    vec + sys_pcctwo->sc_vecbase);
}

struct evcnt *
pcctwointr_evcnt(int lev)
{

	return ((*sys_pcctwo->sc_isrevcnt)(sys_pcctwo->sc_isrcookie, lev));
}
