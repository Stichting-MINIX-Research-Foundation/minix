/*	$NetBSD: hcsc.c,v 1.21 2012/10/27 17:18:37 chs Exp $	*/

/*
 * Copyright (c) 2001 Ben Harris
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mark Brinicombe of Causality Limited.
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
 * Copyright (c) 1996, 1997 Matthias Pfaller.
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
 * HCCS 8-bit SCSI driver using the generic NCR5380 driver
 *
 * Andy Armstrong gives some details of the HCCS SCSI cards at
 * <URL:http://www.armlinux.org/~webmail/linux-arm/1997-08/msg00042.html>.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hcsc.c,v 1.21 2012/10/27 17:18:37 chs Exp $");

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#include <machine/bootconfig.h>

#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>
#include <dev/podulebus/powerromreg.h>

#include <dev/podulebus/hcscreg.h>

int  hcsc_match(device_t, cfdata_t, void *);
void hcsc_attach(device_t, device_t, void *);

static int hcsc_pdma_in(struct ncr5380_softc *, int, int, uint8_t *);
static int hcsc_pdma_out(struct ncr5380_softc *, int, int, uint8_t *);


/*
 * HCCS 8-bit SCSI softc structure.
 *
 * Contains the generic ncr5380 device node, podule information and
 * global information required by the driver.
 */

struct hcsc_softc {
	struct ncr5380_softc	sc_ncr5380;
	bus_space_tag_t		sc_pdmat;
	bus_space_handle_t	sc_pdmah;
	void		*sc_ih;
	struct evcnt	sc_intrcnt;
};

CFATTACH_DECL_NEW(hcsc, sizeof(struct hcsc_softc),
    hcsc_match, hcsc_attach, NULL, NULL);

/*
 * Card probe function
 *
 * Just match the manufacturer and podule ID's
 */

int
hcsc_match(device_t parent, cfdata_t cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	/* Normal ROM */
	if (pa->pa_product == PODULE_HCCS_IDESCSI &&
	    strncmp(pa->pa_descr, "SCSI", 4) == 0)
		return 1;
	/* PowerROM */
	if (pa->pa_product == PODULE_ALSYSTEMS_SCSI &&
	    podulebus_initloader(pa) == 0 &&
	    podloader_callloader(pa, 0, 0) == PRID_HCCS_SCSI1)
		return 1;
	return 0;
}

/*
 * Card attach function
 *
 */

void
hcsc_attach(device_t parent, device_t self, void *aux)
{
	struct hcsc_softc *sc = device_private(self);
	struct ncr5380_softc *ncr_sc = &sc->sc_ncr5380;
	struct podulebus_attach_args *pa = aux;
#ifndef NCR5380_USE_BUS_SPACE
	uint8_t *iobase;
#endif
	char hi_option[sizeof(device_xname(self)) + 8];

	ncr_sc->sc_dev = self;
	ncr_sc->sc_min_dma_len = 0;
	ncr_sc->sc_no_disconnect = 0;
	ncr_sc->sc_parity_disable = 0;

	ncr_sc->sc_dma_alloc = NULL;
	ncr_sc->sc_dma_free = NULL;
	ncr_sc->sc_dma_poll = NULL;
	ncr_sc->sc_dma_setup = NULL;
	ncr_sc->sc_dma_start = NULL;
	ncr_sc->sc_dma_eop = NULL;
	ncr_sc->sc_dma_stop = NULL;
	ncr_sc->sc_intr_on = NULL;
	ncr_sc->sc_intr_off = NULL;

#ifdef NCR5380_USE_BUS_SPACE
	ncr_sc->sc_regt = pa->pa_fast_t;
	bus_space_map(ncr_sc->sc_regt,
	    pa->pa_fast_base + HCSC_DP8490_OFFSET, 8, 0,
	    &ncr_sc->sc_regh);
	ncr_sc->sci_r0 = 0;
	ncr_sc->sci_r1 = 1;
	ncr_sc->sci_r2 = 2;
	ncr_sc->sci_r3 = 3;
	ncr_sc->sci_r4 = 4;
	ncr_sc->sci_r5 = 5;
	ncr_sc->sci_r6 = 6;
	ncr_sc->sci_r7 = 7;
#else
	iobase = (u_char *)pa->pa_fast_base + HCSC_DP8490_OFFSET;
	ncr_sc->sci_r0 = iobase + 0;
	ncr_sc->sci_r1 = iobase + 4;
	ncr_sc->sci_r2 = iobase + 8;
	ncr_sc->sci_r3 = iobase + 12;
	ncr_sc->sci_r4 = iobase + 16;
	ncr_sc->sci_r5 = iobase + 20;
	ncr_sc->sci_r6 = iobase + 24;
	ncr_sc->sci_r7 = iobase + 28;
#endif
	sc->sc_pdmat = pa->pa_mod_t;
	bus_space_map(sc->sc_pdmat, pa->pa_mod_base + HCSC_PDMA_OFFSET, 1, 0,
	    &sc->sc_pdmah);

	ncr_sc->sc_rev = NCR_VARIANT_DP8490;

	ncr_sc->sc_pio_in = hcsc_pdma_in;
	ncr_sc->sc_pio_out = hcsc_pdma_out;

	/* Provide an override for the host id */
	ncr_sc->sc_channel.chan_id = 7;
	snprintf(hi_option, sizeof(hi_option), "%s.hostid",
	    device_xname(self));
	(void)get_bootconf_option(boot_args, hi_option,
	    BOOTOPT_TYPE_INT, &ncr_sc->sc_channel.chan_id);
	ncr_sc->sc_adapter.adapt_minphys = minphys;

	aprint_normal(": host ID %d\n", ncr_sc->sc_channel.chan_id);

	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    device_xname(self), "intr");
	sc->sc_ih = podulebus_irq_establish(pa->pa_ih, IPL_BIO, ncr5380_intr,
	    sc, &sc->sc_intrcnt);

	ncr5380_attach(ncr_sc);
}

#ifndef HCSC_TSIZE_OUT
#define HCSC_TSIZE_OUT	512
#endif

#ifndef HCSC_TSIZE_IN
#define HCSC_TSIZE_IN	512
#endif

#define TIMEOUT 1000000

static inline int
hcsc_ready(struct ncr5380_softc *sc)
{
	int i;

	for (i = TIMEOUT; i > 0; i--) {
		if ((NCR5380_READ(sc,sci_csr) &
		    (SCI_CSR_DREQ | SCI_CSR_PHASE_MATCH)) ==
		    (SCI_CSR_DREQ | SCI_CSR_PHASE_MATCH))
		    	return 1;

		if ((NCR5380_READ(sc, sci_csr) & SCI_CSR_PHASE_MATCH) == 0 ||
		    SCI_BUSY(sc) == 0)
			return 0;
	}
	printf("%s: ready timeout\n", device_xname(sc->sc_dev));
	return 0;
}



/* Return zero on success. */
static inline void hcsc_wait_not_req(struct ncr5380_softc *sc)
{
	int timo;

	for (timo = TIMEOUT; timo; timo--) {
		if ((NCR5380_READ(sc, sci_bus_csr) & SCI_BUS_REQ) == 0 ||
		    (NCR5380_READ(sc, sci_csr) & SCI_CSR_PHASE_MATCH) == 0 ||
		    SCI_BUSY(sc) == 0) {
			return;
		}
	}
	printf("%s: pdma not_req timeout\n", device_xname(sc->sc_dev));
}

static int
hcsc_pdma_in(struct ncr5380_softc *ncr_sc, int phase, int datalen,
    uint8_t *data)
{
	struct hcsc_softc *sc = (struct hcsc_softc *)ncr_sc;
	bus_space_tag_t pdmat = sc->sc_pdmat;
	bus_space_handle_t pdmah = sc->sc_pdmah;
	int s, resid, len;

	s = splbio();

	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) | SCI_MODE_DMA);
	NCR5380_WRITE(ncr_sc, sci_irecv, 0);

	resid = datalen;
	while (resid > 0) {
		len = min(resid, HCSC_TSIZE_IN);
		if (hcsc_ready(ncr_sc) == 0)
			goto interrupt;
		bus_space_read_multi_1(pdmat, pdmah, 0, data, len);
		data += len;
		resid -= len;
	}

	hcsc_wait_not_req(ncr_sc);

interrupt:
	SCI_CLR_INTR(ncr_sc);
	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) & ~SCI_MODE_DMA);
	splx(s);
	return datalen - resid;
}

static int
hcsc_pdma_out(struct ncr5380_softc *ncr_sc, int phase, int datalen,
    uint8_t *data)
{
	struct hcsc_softc *sc = (struct hcsc_softc *)ncr_sc;
	bus_space_tag_t pdmat = sc->sc_pdmat;
	bus_space_handle_t pdmah = sc->sc_pdmah;
	int i, s, icmd, resid;

	s = splbio();
	icmd = NCR5380_READ(ncr_sc, sci_icmd) & SCI_ICMD_RMASK;
	NCR5380_WRITE(ncr_sc, sci_icmd, icmd | SCI_ICMD_DATA);
	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) | SCI_MODE_DMA);
	NCR5380_WRITE(ncr_sc, sci_dma_send, 0);

	resid = datalen;
	if (hcsc_ready(ncr_sc) == 0)
		goto interrupt;

	if (resid > HCSC_TSIZE_OUT) {
		/*
		 * Because of the chips DMA prefetch, phase changes
		 * etc, won't be detected until we have written at
		 * least one byte more. We pre-write 4 bytes so
		 * subsequent transfers will be aligned to a 4 byte
		 * boundary. Assuming disconects will only occur on
		 * block boundaries, we then correct for the pre-write
		 * when and if we get a phase change. If the chip had
		 * DMA byte counting hardware, the assumption would not
		 * be necessary.
		 */
		bus_space_write_multi_1(pdmat, pdmah, 0, data, 4);
		data += 4;
		resid -= 4;

		for (; resid >= HCSC_TSIZE_OUT; resid -= HCSC_TSIZE_OUT) {
			if (hcsc_ready(ncr_sc) == 0) {
				resid += 4; /* Overshot */
				goto interrupt;
			}
			bus_space_write_multi_1(pdmat, pdmah, 0, data,
			    HCSC_TSIZE_OUT);
			data += HCSC_TSIZE_OUT;
		}
		if (hcsc_ready(ncr_sc) == 0) {
			resid += 4; /* Overshot */
			goto interrupt;
		}
	}

	if (resid) {
		bus_space_write_multi_1(pdmat, pdmah, 0, data, resid);
		resid = 0;
	}
	for (i = TIMEOUT; i > 0; i--) {
		if ((NCR5380_READ(ncr_sc, sci_csr)
		    & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH))
		    != SCI_CSR_DREQ)
			break;
	}
	if (i != 0)
		bus_space_write_1(pdmat, pdmah, 0, 0);
	else
		printf("%s: timeout waiting for final SCI_DSR_DREQ.\n",
		    device_xname(ncr_sc->sc_dev));

	hcsc_wait_not_req(ncr_sc);
interrupt:
	SCI_CLR_INTR(ncr_sc);
	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) & ~SCI_MODE_DMA);
	NCR5380_WRITE(ncr_sc, sci_icmd, icmd);
	splx(s);
	return datalen - resid;
}
