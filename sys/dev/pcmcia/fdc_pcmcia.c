/*	$NetBSD: fdc_pcmcia.c,v 1.20 2008/04/28 20:23:56 martin Exp $	*/

/*-
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__KERNEL_RCSID(0, "$NetBSD: fdc_pcmcia.c,v 1.20 2008/04/28 20:23:56 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/buf.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/isa/isavar.h>

#include <dev/isa/fdreg.h>
#include <dev/isa/fdcvar.h>

struct fdc_pcmcia_softc {
	struct fdc_softc sc_fdc;		/* real "fdc" softc */

	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
};

int fdc_pcmcia_match(device_t, cfdata_t, void *);
int fdc_pcmcia_validate_config(struct pcmcia_config_entry *);
void fdc_pcmcia_attach(device_t, device_t, void *);
static void fdc_conf(struct fdc_softc *);

CFATTACH_DECL_NEW(fdc_pcmcia, sizeof(struct fdc_pcmcia_softc),
    fdc_pcmcia_match, fdc_pcmcia_attach, NULL, NULL);

const struct pcmcia_product fdc_pcmcia_products[] = {
	{ PCMCIA_VENDOR_YEDATA, PCMCIA_PRODUCT_YEDATA_EXTERNAL_FDD,
	  PCMCIA_CIS_YEDATA_EXTERNAL_FDD },
};
const size_t fdc_pcmcia_nproducts =
    sizeof(fdc_pcmcia_products) / sizeof(fdc_pcmcia_products[0]);

static void
fdc_conf(struct fdc_softc *fdc)
{
	bus_space_tag_t iot = fdc->sc_iot;
	bus_space_handle_t ioh = fdc->sc_ioh;
	int n;

	/* Figure out what we have */
	if (out_fdc_cmd(iot, ioh, FDC_CMD_VERSION) == -1 ||
	    (n = fdcresult(fdc, 1)) != 1)
		return;

	/* Nec765 or equivalent */
	if (FDC_ST0(fdc->sc_status[0]) == FDC_ST0_INVL)
		return;

#if 0
	/* ns8477 check */
	if (out_fdc_cmd(iot, ioh, FDC_CMD_NSC) == -1 ||
	    (n = fdcresult(fdc, 1)) != 1) {
		printf("NSC command failed\n");
		return;
	}
	else
		printf("Version %x\n", fdc->sc_status[0]);
#endif

	if (out_fdc_cmd(iot, ioh, FDC_CMD_DUMPREG) == -1 ||
	    (n = fdcresult(fdc, -1)) == -1)
		return;

	/*
         * Expect 10 bytes of status; one means that it did not
	 * understand the command
	 */
	if (n == 1)
		return;

	/*
	 * Configure controller to use FIFO and 8 bytes of FIFO threshold
	 */
	(void)out_fdc_cmd(iot, ioh, FDC_CMD_CONFIGURE);
	(void)out_fdc(iot, ioh, 0x00);	/* doc says 0 */
	(void)out_fdc(iot, ioh, 8);	/* FIFO is active low. */
	(void)out_fdc(iot, ioh, fdc->sc_status[9]); /* same comp */
	/* No result phase */

	/* Lock this configuration */
	if (out_fdc_cmd(iot, ioh, FDC_CMD_LOCK(FDC_CMD_FLAGS_LOCK)) == -1 ||
	    fdcresult(fdc, 1) != 1)
		return;
}

int
fdc_pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, fdc_pcmcia_products, fdc_pcmcia_nproducts,
	    sizeof(fdc_pcmcia_products[0]), NULL))
		return (1);
	return (0);
}

void
fdc_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct fdc_pcmcia_softc *psc = device_private(self);
	struct fdc_softc *fdc = &psc->sc_fdc;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_function *pf = pa->pf;
	struct fdc_attach_args fa;
	int error;

	fdc->sc_dev = self;
	psc->sc_pf = pf;

	error = pcmcia_function_configure(pf, fdc_pcmcia_validate_config);
        if (error) {
                aprint_error_dev(self, "configure failed, error=%d\n", error);
                return;
        }

	cfe = pf->cfe;
	fdc->sc_iot = cfe->iospace[0].handle.iot;
	fdc->sc_iot = cfe->iospace[0].handle.ioh;

	if (pcmcia_function_enable(pf))
		goto fail;

	fdc->sc_flags = FDC_HEADSETTLE;
	fdc->sc_state = DEVIDLE;
	TAILQ_INIT(&fdc->sc_drives);

	if (!fdcfind(fdc->sc_iot, fdc->sc_ioh, 1))
		aprint_error_dev(self, "coundn't find fdc\n");

	fdc_conf(fdc);

	/* Establish the interrupt handler. */
	fdc->sc_ih = pcmcia_intr_establish(pa->pf, IPL_BIO, fdchwintr, fdc);
	if (!fdc->sc_ih)
		goto fail;

	/* physical limit: four drives per controller. */
	for (fa.fa_drive = 0; fa.fa_drive < 4; fa.fa_drive++) {
		if (fa.fa_drive < 2)
			fa.fa_deftype = &fd_types[0];
		else
			fa.fa_deftype = NULL;		/* unknown */
		(void)config_found(self, (void *)&fa, fdprint);
	}

	return;

fail:
	pcmcia_function_unconfigure(pf);
}
