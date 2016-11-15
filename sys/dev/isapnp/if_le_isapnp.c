/*	$NetBSD: if_le_isapnp.c,v 1.35 2010/11/13 13:52:03 uebayasi Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1997 Jonathan Stone <jonathan@NetBSD.org> and
 * Matthias Drochner. All rights reserved.
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
 *      This product includes software developed by Jonathan Stone.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_le_isapnp.c,v 1.35 2010/11/13 13:52:03 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#define	LE_ISAPNP_MEMSIZE	16384

#define	PCNET_SAPROM	0x00
#define	PCNET_RDP	0x10
#define	PCNET_RAP	0x12

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ethercom.ec_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct le_isapnp_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	void	*sc_ih;
	bus_space_tag_t sc_iot;		/* space cookie */
	bus_space_handle_t sc_ioh;	/* bus space handle */
	bus_dma_tag_t	sc_dmat;	/* bus dma tag */
	bus_dmamap_t	sc_dmam;	/* bus dma map */
	int	sc_rap, sc_rdp;		/* offsets to LANCE registers */
};

int le_isapnp_match(device_t, cfdata_t, void *);
void le_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(le_isapnp, sizeof(struct le_isapnp_softc),
    le_isapnp_match, le_isapnp_attach, NULL, NULL);

int	le_isapnp_intredge(void *);
static void le_isapnp_wrcsr(struct lance_softc *, uint16_t, uint16_t);
static uint16_t le_isapnp_rdcsr(struct lance_softc *, uint16_t);

static void
le_isapnp_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_isapnp_softc *lesc = (struct le_isapnp_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	bus_space_write_2(iot, ioh, lesc->sc_rdp, val);
}

static uint16_t
le_isapnp_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct le_isapnp_softc *lesc = (struct le_isapnp_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;
	uint16_t val;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	val = bus_space_read_2(iot, ioh, lesc->sc_rdp);
	return (val);
}

int
le_isapnp_match(device_t parent, cfdata_t cf, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_le_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return (pri);
}

void
le_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct le_isapnp_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	struct isapnp_attach_args *ipa = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t dmat;
	bus_dma_segment_t seg;
	int i, rseg, error;

	sc->sc_dev = self;

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error(": error in region allocation\n");
		return;
	}

	lesc->sc_iot = iot = ipa->ipa_iot;
	lesc->sc_ioh = ioh = ipa->ipa_io[0].h;
	lesc->sc_dmat = dmat = ipa->ipa_dmat;

	lesc->sc_rap = PCNET_RAP;
	lesc->sc_rdp = PCNET_RDP;

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_enaddr); i++)
		sc->sc_enaddr[i] = bus_space_read_1(iot, ioh, PCNET_SAPROM + i);

	/*
	 * Allocate a DMA area for the card.
	 */
	if (bus_dmamem_alloc(dmat, LE_ISAPNP_MEMSIZE, PAGE_SIZE, 0, &seg, 1,
	    &rseg, BUS_DMA_NOWAIT)) {
		aprint_error(": couldn't allocate memory for card\n");
		return;
	}
	if (bus_dmamem_map(dmat, &seg, rseg, LE_ISAPNP_MEMSIZE,
	    (void **)&sc->sc_mem, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		aprint_error(": couldn't map memory for card\n");
		return;
	}

	/*
	 * Create and load the DMA map for the DMA area.
	 */
	if (bus_dmamap_create(dmat, LE_ISAPNP_MEMSIZE, 1,
	    LE_ISAPNP_MEMSIZE, 0, BUS_DMA_NOWAIT, &lesc->sc_dmam)) {
		aprint_error(": couldn't create DMA map\n");
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_load(dmat, lesc->sc_dmam,
	    sc->sc_mem, LE_ISAPNP_MEMSIZE, NULL, BUS_DMA_NOWAIT)) {
		aprint_error(": coundn't load DMA map\n");
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}

	sc->sc_conf3 = 0;
	sc->sc_addr = lesc->sc_dmam->dm_segs[0].ds_addr;
	sc->sc_memsize = LE_ISAPNP_MEMSIZE;

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = le_isapnp_rdcsr;
	sc->sc_wrcsr = le_isapnp_wrcsr;
	sc->sc_hwinit = NULL;

	if (ipa->ipa_ndrq > 0) {
		if ((error = isa_dmacascade(ipa->ipa_ic,
		    ipa->ipa_drq[0].num)) != 0) {
			aprint_error(": unable to cascade DRQ, error = %d\n",
			    error);
			return;
		}
	}

	lesc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_NET, le_isapnp_intredge, sc);

	aprint_normal(": %s %s\n", ipa->ipa_devident, ipa->ipa_devclass);

	aprint_normal("%s", device_xname(self));
	am7990_config(&lesc->sc_am7990);
}


/*
 * Controller interrupt.
 */
int
le_isapnp_intredge(void *arg)
{

	if (am7990_intr(arg) == 0)
		return (0);
	for (;;)
		if (am7990_intr(arg) == 0)
			return (1);
}
