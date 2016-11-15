/*	$NetBSD: if_fxp_cardbus.c,v 1.51 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Johan Danielsson.
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
 * CardBus front-end for the Intel i82557 family of Ethernet chips.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_fxp_cardbus.c,v 1.51 2015/04/13 16:33:24 riastradh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <machine/endian.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif


#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/i82557reg.h>
#include <dev/ic/i82557var.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

static int fxp_cardbus_match(device_t, cfdata_t, void *);
static void fxp_cardbus_attach(device_t, device_t, void *);
static int fxp_cardbus_detach(device_t, int);
static void fxp_cardbus_setup(struct fxp_softc *);
static int fxp_cardbus_enable(struct fxp_softc *);
static void fxp_cardbus_disable(struct fxp_softc *);

struct fxp_cardbus_softc {
	struct fxp_softc sc;
	cardbus_devfunc_t ct;
	pcitag_t tag;
	pcireg_t base0_reg;
	pcireg_t base1_reg;
};

CFATTACH_DECL3_NEW(fxp_cardbus, sizeof(struct fxp_cardbus_softc),
    fxp_cardbus_match, fxp_cardbus_attach, fxp_cardbus_detach, fxp_activate,
    NULL, null_childdetached, DVF_DETACH_SHUTDOWN);

#ifdef CBB_DEBUG
#define DPRINTF(X) printf X
#else
#define DPRINTF(X)
#endif

static int
fxp_cardbus_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (PCI_VENDOR(ca->ca_id) == PCI_VENDOR_INTEL &&
	    PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_INTEL_8255X)
		return (1);

	return (0);
}

static void
fxp_cardbus_attach(device_t parent, device_t self,
    void *aux)
{
	struct fxp_cardbus_softc *csc = device_private(self);
	struct fxp_softc *sc = &csc->sc;
	struct cardbus_attach_args *ca = aux;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;

	bus_addr_t adr;

	sc->sc_dev = self;
	csc->ct = ca->ca_ct;
	csc->tag = ca->ca_tag;

	/*
         * Map control/status registers.
         */
	if (Cardbus_mapreg_map(csc->ct, PCI_BAR1,
	    PCI_MAPREG_TYPE_IO, 0, &iot, &ioh, &adr, &sc->sc_size) == 0) {
		csc->base1_reg = adr | 1;
		sc->sc_st = iot;
		sc->sc_sh = ioh;
	} else if (Cardbus_mapreg_map(csc->ct, PCI_BAR0,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
	    0, &memt, &memh, &adr, &sc->sc_size) == 0) {
		csc->base0_reg = adr;
		sc->sc_st = memt;
		sc->sc_sh = memh;
	} else
		panic("%s: failed to allocate mem and io space", __func__);

	if (ca->ca_cis.cis1_info[0] && ca->ca_cis.cis1_info[1])
		printf(": %s %s\n", ca->ca_cis.cis1_info[0],
		    ca->ca_cis.cis1_info[1]);
	else
		printf("\n");

	sc->sc_rev = PCI_REVISION(ca->ca_class);
	if (sc->sc_rev >= FXP_REV_82558_A4)
		sc->sc_flags |= FXPF_FC|FXPF_EXT_TXCB;
	if (sc->sc_rev >= FXP_REV_82559_A0)
		sc->sc_flags |= FXPF_82559_RXCSUM;
	if (sc->sc_rev >= FXP_REV_82550) {
		sc->sc_flags &= ~FXPF_82559_RXCSUM;
		sc->sc_flags |= FXPF_EXT_RFA;
	}

	sc->sc_dmat = ca->ca_dmat;
	sc->sc_enable = fxp_cardbus_enable;
	sc->sc_disable = fxp_cardbus_disable;
	sc->sc_enabled = 0;

	fxp_enable(sc);
	fxp_attach(sc);
	fxp_disable(sc);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, &sc->sc_ethercom.ec_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static void
fxp_cardbus_setup(struct fxp_softc * sc)
{
	struct fxp_cardbus_softc *csc = (struct fxp_cardbus_softc *)sc;
	pcireg_t command;

	pcitag_t tag = csc->tag;

	command = Cardbus_conf_read(csc->ct, tag, PCI_COMMAND_STATUS_REG);
	if (csc->base0_reg) {
		Cardbus_conf_write(csc->ct, tag,
		    PCI_BAR0, csc->base0_reg);
		command |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	} else if (csc->base1_reg) {
		Cardbus_conf_write(csc->ct, tag,
		    PCI_BAR1, csc->base1_reg);
		command |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	}

	/* enable the card */
	Cardbus_conf_write(csc->ct, tag, PCI_COMMAND_STATUS_REG, command);
}

static int
fxp_cardbus_enable(struct fxp_softc * sc)
{
	struct fxp_cardbus_softc *csc = (struct fxp_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->ct;

	Cardbus_function_enable(csc->ct);

	fxp_cardbus_setup(sc);

	/* Map and establish the interrupt. */

	sc->sc_ih = Cardbus_intr_establish(ct, IPL_NET, fxp_intr, sc);
	if (NULL == sc->sc_ih) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
		return 1;
	}

	return 0;
}

static void
fxp_cardbus_disable(struct fxp_softc * sc)
{
	struct fxp_cardbus_softc *csc = (struct fxp_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->ct;

	/* Remove interrupt handler. */
	Cardbus_intr_disestablish(ct, sc->sc_ih);

	Cardbus_function_disable(csc->ct);
}

static int
fxp_cardbus_detach(device_t self, int flags)
{
	struct fxp_cardbus_softc *csc = device_private(self);
	struct fxp_softc *sc = &csc->sc;
	struct cardbus_devfunc *ct = csc->ct;
	int rv, reg;

#ifdef DIAGNOSTIC
	if (ct == NULL)
		panic("%s: data structure lacks", device_xname(self));
#endif

	if ((rv = fxp_detach(sc, flags)) != 0)
		return rv;
	/*
	 * Unhook the interrupt handler.
	 */
	Cardbus_intr_disestablish(ct, sc->sc_ih);

	/*
	 * release bus space and close window
	 */
	if (csc->base0_reg)
		reg = PCI_BAR0;
	else
		reg = PCI_BAR1;
	Cardbus_mapreg_unmap(ct, reg, sc->sc_st, sc->sc_sh, sc->sc_size);
	return 0;
}
