/*	$NetBSD: i82365_isapnp.c,v 1.32 2012/10/27 17:18:26 chs Exp $	*/

/*
 * Copyright (c) 1998 Bill Sommerfeld.  All rights reserved.
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
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
__KERNEL_RCSID(0, "$NetBSD: i82365_isapnp.c,v 1.32 2012/10/27 17:18:26 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <dev/ic/i82365reg.h>
#include <dev/ic/i82365var.h>
#include <dev/isa/i82365_isavar.h>

#undef DPRINTF
#ifdef PCICISADEBUG
int	pcicisapnp_debug = 0 /* XXX */ ;
#define	DPRINTF(arg) if (pcicisapnp_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

int pcic_isapnp_match(device_t, cfdata_t, void *);
void pcic_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(pcic_isapnp, sizeof(struct pcic_isa_softc),
    pcic_isapnp_match, pcic_isapnp_attach, NULL, NULL);

static const struct pcmcia_chip_functions pcic_isa_functions = {
	pcic_chip_mem_alloc,
	pcic_chip_mem_free,
	pcic_chip_mem_map,
	pcic_chip_mem_unmap,

	pcic_chip_io_alloc,
	pcic_chip_io_free,
	pcic_chip_io_map,
	pcic_chip_io_unmap,

	pcic_isa_chip_intr_establish,
	pcic_isa_chip_intr_disestablish,

	pcic_chip_socket_enable,
	pcic_chip_socket_disable,
	pcic_chip_socket_settype,
	NULL,
};

int
pcic_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_pcic_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return pri;
}

void
pcic_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct pcic_isa_softc *isc = device_private(self);
	struct pcic_softc *sc = &isc->sc_pcic;
	struct isapnp_attach_args *ipa = aux;
	isa_chipset_tag_t ic = ipa->ipa_ic;
	bus_space_tag_t iot = ipa->ipa_iot;
	bus_space_tag_t memt = ipa->ipa_memt;
	bus_space_handle_t ioh;
	bus_space_handle_t memh;
	bus_addr_t maddr;
	int msize;
	int tmp1;

	sc->dev = self;

	printf("\n");

	if (isapnp_config(iot, memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	printf("%s: %s %s", device_xname(self), ipa->ipa_devident,
	    ipa->ipa_devclass);

	/* sanity check that we get at least one hunk of IO space.. */
	if (ipa->ipa_nio < 1) {
		aprint_error_dev(self,
		    "failed to get one chunk of i/o space\n");
		return;
	}

	/* Find i/o space. */
	ioh = ipa->ipa_io[0].h;

	/* sanity check to make sure we have a real PCIC there.. */
	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C0SA + PCIC_IDENT);
	tmp1 = bus_space_read_1(iot, ioh, PCIC_REG_DATA);
	printf("(ident 0x%x", tmp1);
	if (pcic_ident_ok(tmp1)) {
	  	printf(" OK)");
	} else {
	        printf(" Not OK)\n");
		return;
	}

	msize =  0x4000;
	if (isa_mem_alloc(memt, msize, msize, 0, 0, &maddr, &memh)) {
		printf(": can't alloc mem space\n");
		return;
	}
	printf(": using iomem %#" PRIxPADDR " iosiz %#x", maddr, msize);
	sc->membase = maddr;
	sc->subregionmask = (1 << (msize / PCIC_MEM_PAGESIZE)) - 1;

	isc->sc_ic = ic;
	sc->pct = &pcic_isa_functions;

	sc->iot = iot;
	sc->ioh = ioh;
	sc->memt = memt;
	sc->memh = memh;

	/*
	 * allocate an irq.  it will be used by both controllers.  I could
	 * use two different interrupts, but interrupts are relatively
	 * scarce, shareable, and for PCIC controllers, very infrequent.
	 */

	if (ipa->ipa_nirq > 0)
		sc->irq = ipa->ipa_irq[0].num;
	else
		sc->irq = -1;

	printf("\n");

	pcic_attach(sc);
	pcic_isa_bus_width_probe(sc, iot, ioh, ipa->ipa_io[0].base,
	    ipa->ipa_io[0].length);
	pcic_attach_sockets(sc);
	config_interrupts(self, pcic_isa_config_interrupts);
}
