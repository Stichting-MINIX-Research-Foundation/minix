/*	$NetBSD: uba.c,v 1.79 2010/11/13 13:52:10 uebayasi Exp $	   */
/*
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)uba.c	7.10 (Berkeley) 12/16/90
 *	@(#)autoconf.c	7.20 (Berkeley) 5/9/91
 */

/*
 * Copyright (c) 1996 Jonathan Stone.
 * Copyright (c) 1994, 1996 Ludd, University of Lule}, Sweden.
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
 *	@(#)uba.c	7.10 (Berkeley) 12/16/90
 *	@(#)autoconf.c	7.20 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uba.c,v 1.79 2010/11/13 13:52:10 uebayasi Exp $");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/scb.h>
#include <sys/cpu.h>

#include <dev/qbus/ubareg.h>
#include <dev/qbus/ubavar.h>

#include "ioconf.h"
#include "locators.h"

static int ubasearch (device_t, cfdata_t,
		      const int *, void *);
static int ubaprint (void *, const char *);

/*
 * If we failed to allocate uba resources, put us on a queue to wait
 * until there is available resources. Resources to compete about
 * are map registers and BDPs. This is normally only a problem on
 * Unibus systems, Qbus systems have more map registers than usable.
 */
void
uba_enqueue(struct uba_unit *uu)
{
	struct uba_softc *uh;
	int s;

	uh = device_private(device_parent(uu->uu_dev));

	s = spluba();
	SIMPLEQ_INSERT_TAIL(&uh->uh_resq, uu, uu_resq);
	splx(s);
}

/*
 * When a routine that uses resources is finished, the next device
 * in queue for map registers etc is called. If it succeeds to get
 * resources, call next, and next, and next...
 * This routine must be called at spluba.
 */
void
uba_done(struct uba_softc *uh)
{
	struct uba_unit *uu;

	while ((uu = SIMPLEQ_FIRST(&uh->uh_resq))) {
		SIMPLEQ_REMOVE_HEAD(&uh->uh_resq, uu_resq);
		if ((*uu->uu_ready)(uu) == 0) {
			SIMPLEQ_INSERT_HEAD(&uh->uh_resq, uu, uu_resq);
			break;
		}
	}
}

/*
 * Each device that needs some handling if an ubareset occurs must
 * register for reset first through this routine.
 */
void
uba_reset_establish(void (*reset)(device_t), device_t dev)
{
	struct uba_softc *uh = device_private(device_parent(dev));
	struct uba_reset *ur;

	ur = malloc(sizeof(struct uba_reset), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (ur == NULL)
		panic("uba_reset_establish");
	ur->ur_dev = dev;
	ur->ur_reset = reset;

	SIMPLEQ_INSERT_TAIL(&uh->uh_resetq, ur, ur_resetq);
}

/*
 * Allocate a bunch of map registers and map them to the given address.
 */
int
uballoc(struct uba_softc *uh, struct ubinfo *ui, int flags)
{
	int waitok = (flags & UBA_CANTWAIT) == 0;
	int error;

	if ((error = bus_dmamap_create(uh->uh_dmat, ui->ui_size, 1,
	    ui->ui_size, 0, (waitok ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT),
	    &ui->ui_dmam)))
		return error;

	if ((error = bus_dmamap_load(uh->uh_dmat, ui->ui_dmam, ui->ui_vaddr,
	    ui->ui_size, NULL, (waitok ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT)))) {
		bus_dmamap_destroy(uh->uh_dmat, ui->ui_dmam);
		return error;
	}
	ui->ui_baddr = ui->ui_dmam->dm_segs[0].ds_addr;
	return 0;
}

/*
 * Allocate DMA-able memory and map it on the unibus.
 */
int
ubmemalloc(struct uba_softc *uh, struct ubinfo *ui, int flags)
{
	int waitok = (flags & UBA_CANTWAIT ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	int error;

	if ((error = bus_dmamem_alloc(uh->uh_dmat, ui->ui_size, PAGE_SIZE, 0,
	    &ui->ui_seg, 1, &ui->ui_rseg, waitok)))
		return error;
	if ((error = bus_dmamem_map(uh->uh_dmat, &ui->ui_seg, ui->ui_rseg,
	    ui->ui_size, &ui->ui_vaddr, waitok|BUS_DMA_COHERENT))) {
		bus_dmamem_free(uh->uh_dmat, &ui->ui_seg, ui->ui_rseg);
		return error;
	}
	if ((error = uballoc(uh, ui, flags))) {
		bus_dmamem_unmap(uh->uh_dmat, ui->ui_vaddr, ui->ui_size);
		bus_dmamem_free(uh->uh_dmat, &ui->ui_seg, ui->ui_rseg);
	}
	return error;
}

void
ubfree(struct uba_softc *uh, struct ubinfo *ui)
{
	bus_dmamap_unload(uh->uh_dmat, ui->ui_dmam);
	bus_dmamap_destroy(uh->uh_dmat, ui->ui_dmam);
}

void
ubmemfree(struct uba_softc *uh, struct ubinfo *ui)
{
	bus_dmamem_unmap(uh->uh_dmat, ui->ui_vaddr, ui->ui_size);
	bus_dmamem_free(uh->uh_dmat, &ui->ui_seg, ui->ui_rseg);
	ubfree(uh, ui);
}

/*
 * Generate a reset on uba number uban.	 Then
 * call each device that asked to be called during attach,
 * giving it a chance to clean up so as to be able to continue.
 */
void
ubareset(struct uba_softc *uh)
{
	struct uba_reset *ur;
	int s;

	s = spluba();
	SIMPLEQ_INIT(&uh->uh_resq);
	printf("%s: reset", device_xname(uh->uh_dev));
	(*uh->uh_ubainit)(uh);

	ur = SIMPLEQ_FIRST(&uh->uh_resetq);
	if (ur) do {
		printf(" %s", device_xname(ur->ur_dev));
		(*ur->ur_reset)(ur->ur_dev);
	} while ((ur = SIMPLEQ_NEXT(ur, ur_resetq)));

	printf("\n");
	splx(s);
}

/*
 * The common attach routine:
 *   Calls the scan routine to search for uba devices.
 */
void
uba_attach(struct uba_softc *sc, paddr_t iopagephys)
{

	/*
	 * Set last free interrupt vector for devices with
	 * programmable interrupt vectors.  Use is to decrement
	 * this number and use result as interrupt vector.
	 */
	sc->uh_lastiv = 0x200;
	SIMPLEQ_INIT(&sc->uh_resq);
	SIMPLEQ_INIT(&sc->uh_resetq);

	/*
	 * Allocate place for unibus I/O space in virtual space.
	 */
	if (bus_space_map(sc->uh_iot, iopagephys, UBAIOSIZE, 0, &sc->uh_ioh))
		return;

	/*
	 * Keep track of which addressed devices are found.
	 */
	sc->uh_used = malloc(UBAIOSIZE, M_TEMP, M_WAITOK);
	memset(sc->uh_used, 0, UBAIOSIZE);

	if (sc->uh_beforescan)
		(*sc->uh_beforescan)(sc);
	/*
	 * Now start searching for devices.
	 */
	config_search_ia(ubasearch, sc->uh_dev, "uba", NULL);

	if (sc->uh_afterscan)
		(*sc->uh_afterscan)(sc);

	free(sc->uh_used, M_TEMP);
}

int
ubasearch(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct	uba_softc *sc = device_private(parent);
	struct	uba_attach_args ua;
	int	i, csr, vec, br;

	csr = cf->cf_loc[UBACF_CSR];
	if (sc->uh_used[ubdevreg(csr)])
		return 0; /* something are already at this address */

	ua.ua_ioh = ubdevreg(csr) + sc->uh_ioh;
	ua.ua_iot = sc->uh_iot;
	ua.ua_dmat = sc->uh_dmat;

	if (badaddr((void *)ua.ua_ioh, 2) ||
	    (sc->uh_errchk ? (*sc->uh_errchk)(sc):0))
		goto forgetit;

	scb_vecref(0, 0); /* Clear vector ref */
	i = config_match(parent, cf, &ua);

	if (sc->uh_errchk)
		if ((*sc->uh_errchk)(sc))
			goto forgetit;
	if (i == 0)
		goto forgetit;

	i = scb_vecref(&vec, &br);
	if (i == 0)
		goto fail;
	if (vec == 0)
		goto fail;

	ua.ua_br = br;
	ua.ua_cvec = vec;
	ua.ua_iaddr = csr;
	ua.ua_evcnt = NULL;

	sc->uh_used[ubdevreg(csr)] = 1;

	config_attach(parent, cf, &ua, ubaprint);
	return 0;

fail:
	printf("%s%d at %s csr %o %s\n",
	    cf->cf_name, cf->cf_unit, device_xname(parent),
	    csr, (i ? "zero vector" : "didn't interrupt"));

forgetit:
	return 0;
}

/*
 * Print out some interesting info common to all unibus devices.
 */
int
ubaprint(void *aux, const char *uba)
{
	struct uba_attach_args *ua = aux;

	aprint_normal(" csr %o vec %o ipl %x", ua->ua_iaddr,
	    ua->ua_cvec & 511, ua->ua_br);
	return UNCONF;
}

/*
 * Move to machdep eventually
 */
void
uba_intr_establish(void *icookie, int vec, void (*ifunc)(void *iarg),
	void *iarg, struct evcnt *ev)
{
	scb_vecalloc(vec, ifunc, iarg, SCB_ISTACK, ev);
}
