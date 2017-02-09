/* $NetBSD: if_an_pcmcia.c,v 1.41 2012/10/27 17:18:36 chs Exp $ */

/*-
 * Copyright (c) 2000, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe
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
__KERNEL_RCSID(0, "$NetBSD: if_an_pcmcia.c,v 1.41 2012/10/27 17:18:36 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
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

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/anreg.h>
#include <dev/ic/anvar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

static int an_pcmcia_match(device_t, cfdata_t, void *);
static int an_pcmcia_validate_config(struct pcmcia_config_entry *);
static void an_pcmcia_attach(device_t, device_t, void *);
static int an_pcmcia_detach(device_t, int);
static int an_pcmcia_enable(struct an_softc *);
static void an_pcmcia_disable(struct an_softc *);

struct an_pcmcia_softc {
	struct an_softc sc_an;			/* real "an" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int sc_io_window;			/* our i/o window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handle */

	int sc_state;
#define	AN_PCMCIA_ATTACHED	3
};

CFATTACH_DECL_NEW(an_pcmcia, sizeof(struct an_pcmcia_softc),
    an_pcmcia_match, an_pcmcia_attach, an_pcmcia_detach, an_activate);

static const struct pcmcia_product an_pcmcia_products[] = {
	{ PCMCIA_VENDOR_AIRONET,	PCMCIA_PRODUCT_AIRONET_PC4800,
	  PCMCIA_CIS_AIRONET_PC4800 },
	{ PCMCIA_VENDOR_AIRONET,	PCMCIA_PRODUCT_AIRONET_PC4500,
	  PCMCIA_CIS_AIRONET_PC4500 },
	{ PCMCIA_VENDOR_AIRONET,	PCMCIA_PRODUCT_AIRONET_350,
	  PCMCIA_CIS_AIRONET_350 },
};
static const size_t an_pcmcia_nproducts =
    sizeof(an_pcmcia_products) / sizeof(an_pcmcia_products[0]);

static int
an_pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, an_pcmcia_products, an_pcmcia_nproducts,
	    sizeof(an_pcmcia_products[0]), NULL))
		return (1);
	return (0);
}

static int
an_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1)
		return (EINVAL);
	return (0);
}

static void
an_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct an_pcmcia_softc *psc = device_private(self);
	struct an_softc *sc = &psc->sc_an;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int error;

	sc->sc_dev = self;
	psc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, an_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n",
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_iot = cfe->iospace[0].handle.iot;
	sc->sc_ioh = cfe->iospace[0].handle.ioh;

	error = an_pcmcia_enable(sc);
	if (error)
		goto fail;

	sc->sc_enabled = 1;
	sc->sc_enable = an_pcmcia_enable;
	sc->sc_disable = an_pcmcia_disable;

	error = an_attach(sc);
	if (error) {
		aprint_error_dev(self, "failed to attach controller\n");
		goto fail2;
	}

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, &sc->sc_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	an_pcmcia_disable(sc);
	sc->sc_enabled = 0;
	psc->sc_state = AN_PCMCIA_ATTACHED;
	return;

fail2:
	an_pcmcia_disable(sc);
	sc->sc_enabled = 0;
fail:
	pcmcia_function_unconfigure(pa->pf);
}


static int
an_pcmcia_detach(device_t self, int flags)
{
	struct an_pcmcia_softc *psc = device_private(self);
	int error;

	if (psc->sc_state != AN_PCMCIA_ATTACHED)
		return (0);

	pmf_device_deregister(self);

	error = an_detach(&psc->sc_an);
	if (error)
		return (error);

	pcmcia_function_unconfigure(psc->sc_pf);

	return (0);
}

static int
an_pcmcia_enable(struct an_softc *sc)
{
	struct an_pcmcia_softc *psc = (void *)sc;
	int error;

	/* establish the interrupt. */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, an_intr, sc);
	if (!psc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(psc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
		psc->sc_ih = 0;
	}

	return (error);
}

static void
an_pcmcia_disable(struct an_softc *sc)
{
	struct an_pcmcia_softc *psc = (void *)sc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
	psc->sc_ih = 0;
}
