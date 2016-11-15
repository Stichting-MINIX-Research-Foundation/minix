/*	$NetBSD: spc_pcmcia.c,v 1.21 2009/11/12 20:14:04 dyoung Exp $	*/

/*-
 * Copyright (c) 2000, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
__KERNEL_RCSID(0, "$NetBSD: spc_pcmcia.c,v 1.21 2009/11/12 20:14:04 dyoung Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/mb89352var.h>

struct spc_pcmcia_softc {
	struct spc_softc	sc_spc;		/* glue to MI code */

	/* PCMCIA-specific goo. */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handler */

	int sc_state;
#define	SPC_PCMCIA_ATTACHED	3
};

static int	spc_pcmcia_match(device_t, cfdata_t, void *);
static int	spc_pcmcia_validate_config(struct pcmcia_config_entry *);
static void	spc_pcmcia_attach(device_t, device_t, void *);
static int	spc_pcmcia_detach(device_t, int);
static int	spc_pcmcia_enable(device_t, int);

CFATTACH_DECL2_NEW(spc_pcmcia, sizeof(struct spc_pcmcia_softc),
    spc_pcmcia_match, spc_pcmcia_attach, spc_pcmcia_detach, NULL, NULL,
    spc_childdet);

static const struct pcmcia_product spc_pcmcia_products[] = {
	{ PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_SCSI600,
	  PCMCIA_CIS_FUJITSU_SCSI600 },
};
static const size_t spc_pcmcia_nproducts = __arraycount(spc_pcmcia_products);

int
spc_pcmcia_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, spc_pcmcia_products, spc_pcmcia_nproducts,
	    sizeof(spc_pcmcia_products[0]), NULL))
		return 1;
	return 0;
}

int
spc_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{

	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_memspace != 0 ||
	    cfe->num_iospace != 1)
		return EINVAL;
	return 0;
}

void
spc_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct spc_pcmcia_softc *sc = device_private(self);
	struct spc_softc *spc = &sc->sc_spc;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_function *pf = pa->pf;
	int error;

	spc->sc_dev = self;
	sc->sc_pf = pf;

	error = pcmcia_function_configure(pf, spc_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n", error);
		return;
	}

	cfe = pf->cfe;
	spc->sc_iot = cfe->iospace[0].handle.iot;
	spc->sc_ioh = cfe->iospace[0].handle.ioh;

	error = spc_pcmcia_enable(self, 1);
	if (error)
		goto fail;

	spc->sc_initiator = 7; /* XXX */
	spc->sc_adapter.adapt_enable = spc_pcmcia_enable;
	spc->sc_adapter.adapt_refcnt = 1;

	/*
	 *  Initialize nca board itself.
	 */
	spc_attach(spc);
	scsipi_adapter_delref(&spc->sc_adapter);
	sc->sc_state = SPC_PCMCIA_ATTACHED;
	return;

 fail:
	pcmcia_function_unconfigure(pf);
}

int
spc_pcmcia_detach(device_t self, int flags)
{
	struct spc_pcmcia_softc *sc = device_private(self);
	int error;

	if (sc->sc_state != SPC_PCMCIA_ATTACHED)
		return 0;

	error = spc_detach(self, flags);
	if (error)
		return error;

	pcmcia_function_unconfigure(sc->sc_pf);

	return 0;
}

int
spc_pcmcia_enable(device_t self, int onoff)
{
	struct spc_pcmcia_softc *sc = device_private(self);
	int error;

	if (onoff) {
		/* Establish the interrupt handler. */
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_BIO,
		    spc_intr, &sc->sc_spc);
		if (!sc->sc_ih)
			return EIO;

		error = pcmcia_function_enable(sc->sc_pf);
		if (error) {
			pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
			sc->sc_ih = 0;
			return error;
		}

		/* Initialize only chip.  */
		spc_init(&sc->sc_spc, 0);
	} else {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return 0;
}
