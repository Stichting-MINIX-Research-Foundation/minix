/*	$NetBSD: if_ral_cardbus.c,v 1.23 2012/02/18 13:38:35 drochner Exp $	*/
/*	$OpenBSD: if_ral_cardbus.c,v 1.6 2006/01/09 20:03:31 damien Exp $  */

/*-
 * Copyright (c) 2005, 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * CardBus front-end for the Ralink RT2560/RT2561/RT2561S/RT2661 driver.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ral_cardbus.c,v 1.23 2012/02/18 13:38:35 drochner Exp $");


#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/callout.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <netinet/in.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_rssadapt.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/rt2560var.h>
#include <dev/ic/rt2661var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

static struct ral_opns {
	int	(*attach)(void *, int);
	int	(*detach)(void *);
	int	(*intr)(void *);

}  ral_rt2560_opns = {
	rt2560_attach,
	rt2560_detach,
	rt2560_intr

}, ral_rt2661_opns = {
	rt2661_attach,
	rt2661_detach,
	rt2661_intr
};

struct ral_cardbus_softc {
	union {
		struct rt2560_softc	sc_rt2560;
		struct rt2661_softc	sc_rt2661;
	} u;
#define sc_sc	u.sc_rt2560

	/* cardbus specific goo */
	struct ral_opns		*sc_opns;
	cardbus_devfunc_t	sc_ct;
	pcitag_t		sc_tag;
	void			*sc_ih;
	bus_size_t		sc_mapsize;
	pcireg_t		sc_bar_val;
};

int	ral_cardbus_match(device_t, cfdata_t, void *);
void	ral_cardbus_attach(device_t, device_t, void *);
int	ral_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(ral_cardbus, sizeof (struct ral_cardbus_softc),
    ral_cardbus_match, ral_cardbus_attach, ral_cardbus_detach, NULL);

int	ral_cardbus_enable(struct rt2560_softc *);
void	ral_cardbus_disable(struct rt2560_softc *);
void	ral_cardbus_power(struct rt2560_softc *, int);
void	ral_cardbus_setup(struct ral_cardbus_softc *);

int
ral_cardbus_match(device_t parent,
    cfdata_t cfdata, void *aux)
{
        struct cardbus_attach_args *ca = aux;

        if (PCI_VENDOR(ca->ca_id) == PCI_VENDOR_RALINK) {
                switch (PCI_PRODUCT(ca->ca_id)) {
		case PCI_PRODUCT_RALINK_RT2560:
		case PCI_PRODUCT_RALINK_RT2561:
		case PCI_PRODUCT_RALINK_RT2561S:
		case PCI_PRODUCT_RALINK_RT2661:
			return 1;
		default:
			return 0;
                }
        }

        return 0;
}

void
ral_cardbus_attach(device_t parent, device_t self,
    void *aux)
{
	struct ral_cardbus_softc *csc = device_private(self);
	struct rt2560_softc *sc = &csc->sc_sc;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	char devinfo[256];
	bus_addr_t base;
	int error, revision;

	pci_devinfo(ca->ca_id, ca->ca_class, 0, devinfo, sizeof(devinfo));
	revision = PCI_REVISION(ca->ca_class);
	aprint_normal(": %s (rev. 0x%02x)\n", devinfo, revision);

	csc->sc_opns =
	    (PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_RALINK_RT2560) ?
	    &ral_rt2560_opns : &ral_rt2661_opns;

	sc->sc_dev = self;
	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	/* power management hooks */
	sc->sc_enable = ral_cardbus_enable;
	sc->sc_disable = ral_cardbus_disable;

	/* map control/status registers */
	error = Cardbus_mapreg_map(ct, PCI_BAR0,
	    PCI_MAPREG_TYPE_MEM, 0, &sc->sc_st, &sc->sc_sh, &base,
	    &csc->sc_mapsize);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}

	csc->sc_bar_val = base | PCI_MAPREG_TYPE_MEM;

	/* set up the PCI configuration registers */
	ral_cardbus_setup(csc);

	(*csc->sc_opns->attach)(sc, PCI_PRODUCT(ca->ca_id));

	Cardbus_function_disable(ct);
}

int
ral_cardbus_detach(device_t self, int flags)
{
	struct ral_cardbus_softc *csc = device_private(self);
	struct rt2560_softc *sc = &csc->sc_sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	int error;

	error = (*csc->sc_opns->detach)(sc);
	if (error != 0)
		return error;

	/* unhook the interrupt handler */
	if (csc->sc_ih != NULL) {
		Cardbus_intr_disestablish(ct, csc->sc_ih);
		csc->sc_ih = NULL;
	}

	/* release bus space and close window */
	Cardbus_mapreg_unmap(ct, PCI_BAR0, sc->sc_st, sc->sc_sh,
	    csc->sc_mapsize);

	return 0;
}

int
ral_cardbus_enable(struct rt2560_softc *sc)
{
	struct ral_cardbus_softc *csc = (struct ral_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;

	/* power on the socket */
	Cardbus_function_enable(ct);

	/* setup the PCI configuration registers */
	ral_cardbus_setup(csc);

	/* map and establish the interrupt handler */
	csc->sc_ih = Cardbus_intr_establish(ct, IPL_NET,
	    csc->sc_opns->intr, sc);
	if (csc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev,
				 "could not establish interrupt\n");
		Cardbus_function_disable(ct);
		return 1;
	}

	return 0;
}

void
ral_cardbus_disable(struct rt2560_softc *sc)
{
	struct ral_cardbus_softc *csc = (struct ral_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;

	/* unhook the interrupt handler */
	Cardbus_intr_disestablish(ct, csc->sc_ih);
	csc->sc_ih = NULL;

	/* power down the socket */
	Cardbus_function_disable(ct);
}

void
ral_cardbus_setup(struct ral_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	pcireg_t reg;

	/* program the BAR */
	Cardbus_conf_write(ct, csc->sc_tag, PCI_BAR0, csc->sc_bar_val);

	/* enable the appropriate bits in the PCI CSR */
	reg = Cardbus_conf_read(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE;
	Cardbus_conf_write(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG, reg);
}
