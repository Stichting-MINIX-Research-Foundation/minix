/*	$NetBSD: nca_pcmcia.c,v 1.24 2008/04/28 20:23:56 martin Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: nca_pcmcia.c,v 1.24 2008/04/28 20:23:56 martin Exp $");

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

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>
#include <dev/ic/ncr53c400reg.h>

struct nca_pcmcia_softc {
	struct ncr5380_softc	sc_ncr5380;	/* glue to MI code */

	/* PCMCIA-specific goo. */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handler */

	int sc_state;
#define NCA_PCMCIA_ATTACHED	3
};

int	nca_pcmcia_match(device_t, cfdata_t, void *);
int	nca_pcmcia_validate_config(struct pcmcia_config_entry *);
void	nca_pcmcia_attach(device_t, device_t, void *);
int	nca_pcmcia_detach(device_t, int);
int	nca_pcmcia_enable(device_t, int);

CFATTACH_DECL_NEW(nca_pcmcia, sizeof(struct nca_pcmcia_softc),
    nca_pcmcia_match, nca_pcmcia_attach, nca_pcmcia_detach, NULL);

#define MIN_DMA_LEN 128

/* Options for disconnect/reselect, DMA, and interrupts. */
#define NCA_NO_DISCONNECT	0x00ff
#define NCA_NO_PARITY_CHK	0xff00

const struct pcmcia_product nca_pcmcia_products[] = {
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_INVALID },
};
const size_t nca_pcmcia_nproducts =
    __arraycount(nca_pcmcia_products);

int
nca_pcmcia_match(device_t parent,  cfdata_t cf, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, nca_pcmcia_products, nca_pcmcia_nproducts,
	    sizeof(nca_pcmcia_products[0]), NULL))
		return 1;
	return 0;
}

int
nca_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{

	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_memspace != 0 ||
	    cfe->num_iospace != 1)
		return EINVAL;
	return 0;
}

void
nca_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct nca_pcmcia_softc *esc = device_private(self);
	struct ncr5380_softc *sc = &esc->sc_ncr5380;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_function *pf = pa->pf;
	int flags;
	int error;

	sc->sc_dev = self;
	esc->sc_pf = pf;

	error = pcmcia_function_configure(pf, nca_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n", error);
		return;
	}

	cfe = pf->cfe;
	sc->sc_regt = cfe->iospace[0].handle.iot;
	sc->sc_regh = cfe->iospace[0].handle.ioh;

	/* Initialize 5380 compatible register offsets. */
	sc->sci_r0 = C400_5380_REG_OFFSET + 0;
	sc->sci_r1 = C400_5380_REG_OFFSET + 1;
	sc->sci_r2 = C400_5380_REG_OFFSET + 2;
	sc->sci_r3 = C400_5380_REG_OFFSET + 3;
	sc->sci_r4 = C400_5380_REG_OFFSET + 4;
	sc->sci_r5 = C400_5380_REG_OFFSET + 5;
	sc->sci_r6 = C400_5380_REG_OFFSET + 6;
	sc->sci_r7 = C400_5380_REG_OFFSET + 7;

	sc->sc_rev = NCR_VARIANT_NCR53C400;

	/*
	 * MD function pointers used by the MI code.
	 */
	sc->sc_pio_out = ncr5380_pio_out;
	sc->sc_pio_in =  ncr5380_pio_in;
	sc->sc_dma_alloc = NULL;
	sc->sc_dma_free  = NULL;
	sc->sc_dma_setup = NULL;
	sc->sc_dma_start = NULL;
	sc->sc_dma_poll  = NULL;
	sc->sc_dma_eop   = NULL;
	sc->sc_dma_stop  = NULL;
	sc->sc_intr_on   = NULL;
	sc->sc_intr_off  = NULL;

	/*
	 * Support the "options" (config file flags).
	 * Disconnect/reselect is a per-target mask.
	 * Interrupts and DMA are per-controller.
	 */
#if 0
	flags = 0x0000;	/* no options */
#else
	flags = 0xffff;	/* all options except force poll */
#endif

	sc->sc_no_disconnect = (flags & NCA_NO_DISCONNECT);
	sc->sc_parity_disable = (flags & NCA_NO_PARITY_CHK) >> 8;
	sc->sc_min_dma_len = MIN_DMA_LEN;

	error = nca_pcmcia_enable(self, 1);
	if (error)
		goto fail;

	sc->sc_adapter.adapt_enable = nca_pcmcia_enable;
	sc->sc_adapter.adapt_refcnt = 1;

	ncr5380_attach(sc);
	scsipi_adapter_delref(&sc->sc_adapter);
	esc->sc_state = NCA_PCMCIA_ATTACHED;
	return;

fail:
	pcmcia_function_unconfigure(pf);
}

int
nca_pcmcia_detach(device_t self, int flags)
{
	struct nca_pcmcia_softc *sc = device_private(self);
	int error;

	if (sc->sc_state != NCA_PCMCIA_ATTACHED)
		return 0;

	error = ncr5380_detach(&sc->sc_ncr5380, flags);
	if (error)
		return error;

	pcmcia_function_unconfigure(sc->sc_pf);

	return 0;
}

int
nca_pcmcia_enable(device_t self, int onoff)
{
	struct nca_pcmcia_softc *sc = device_private(self);
	int error;

	if (onoff) {
		/* Establish the interrupt handler. */
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_BIO,
		    ncr5380_intr, &sc->sc_ncr5380);
		if (sc->sc_ih == NULL)
			return EIO;

		error = pcmcia_function_enable(sc->sc_pf);
		if (error) {
			pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
			sc->sc_ih = 0;
			return error;
		}

		/* Initialize only chip.  */
		ncr5380_init(&sc->sc_ncr5380);
	} else {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return 0;
}
