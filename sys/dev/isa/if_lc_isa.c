/*	$NetBSD: if_lc_isa.c,v 1.34 2012/10/27 17:18:24 chs Exp $ */

/*-
 * Copyright (c) 1994, 1995, 1997 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 * DEC EtherWORKS 3 Ethernet Controllers
 *
 * Written by Matt Thomas
 *
 *   This driver supports the LEMAC (DE203, DE204, and DE205) cards.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_lc_isa.c,v 1.34 2012/10/27 17:18:24 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/lemacreg.h>
#include <dev/ic/lemacvar.h>

#include <dev/isa/isavar.h>

extern struct cfdriver lc_cd;

static int lemac_isa_find(lemac_softc_t *, const char *, struct isa_attach_args *, int);
static int lemac_isa_probe(device_t, cfdata_t, void *);
static void lemac_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(lc_isa, sizeof(lemac_softc_t),
    lemac_isa_probe, lemac_isa_attach, NULL, NULL);

static int
lemac_isa_find(lemac_softc_t *sc, const char *xname, struct isa_attach_args *ia, int attach)
{
	bus_addr_t maddr;
	bus_size_t msiz;
	int rv = 0, irq;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/*
	 * Disallow wildcarded i/o addresses.
	 */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	/*
	 * Make sure this is a valid LEMAC address.
	 */
	if (ia->ia_io[0].ir_addr & (LEMAC_IOSIZE - 1))
		return 0;

	sc->sc_iot = ia->ia_iot;

	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, LEMAC_IOSIZE, 0,
	    &sc->sc_ioh)) {
		if (attach)
			printf(": can't map i/o space\n");
		return 0;
	}

	/*
	 * Read the Ethernet address from the EEPROM.
	 * It must start with one of the DEC OUIs and pass the
	 * DEC ethernet checksum test.
	 */
	if (lemac_port_check(sc->sc_iot, sc->sc_ioh) == 0)
		goto outio;

	/*
	 * Get information about memory space and attempt to map it.
	 */
	lemac_info_get(sc->sc_iot, sc->sc_ioh, &maddr, &msiz, &irq);

	if (ia->ia_iomem[0].ir_addr != ISA_UNKNOWN_IOMEM &&
	    ia->ia_iomem[0].ir_addr != maddr)
		goto outio;

	if (attach) {
		if (msiz == 0) {
			printf(": memory configuration is invalid\n");
			goto outio;
		}

		sc->sc_memt = ia->ia_memt;
		if (bus_space_map(ia->ia_memt, maddr, msiz, 0, &sc->sc_memh)) {
			printf(": can't map mem space\n");
			goto outio;
		}
	}

	/*
	 * Double-check IRQ configuration.
	 */
	if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
	    ia->ia_irq[0].ir_irq != irq)
		printf("%s: overriding IRQ %d to %d\n", xname,
		       ia->ia_irq[0].ir_irq, irq);

	if (attach) {
		sc->sc_ats = shutdownhook_establish(lemac_shutdown, sc);
		if (sc->sc_ats == NULL) {
			aprint_normal("\n");
			aprint_error("%s: warning: can't establish shutdown hook\n", xname);
		}

		lemac_ifattach(sc);

		sc->sc_ih = isa_intr_establish(ia->ia_ic, irq, IST_EDGE,
		    IPL_NET, lemac_intr, sc);
	}

	/*
	 * I guess we've found one.
	 */
	rv = 1;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = LEMAC_IOSIZE;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_addr = maddr;
	ia->ia_iomem[0].ir_size = msiz;

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = irq;

	ia->ia_ndrq = 0;

outio:
	if (rv == 0 || !attach)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, LEMAC_IOSIZE);
	return rv;
}

static int
lemac_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	cfdata_t cf = match;
	lemac_softc_t sc;
	char xname[16];

	snprintf(xname, sizeof(xname), "%s%d", lc_cd.cd_name, cf->cf_unit);

	return lemac_isa_find(&sc, xname, ia, 0);
}

static void
lemac_isa_attach(device_t parent, device_t self, void *aux)
{
	lemac_softc_t *sc = device_private(self);
	struct isa_attach_args *ia = aux;

	sc->sc_dev = self;
	(void) lemac_isa_find(sc, device_xname(self), ia, 1);
}
